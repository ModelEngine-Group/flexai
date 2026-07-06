#include "./include/nccl_util.h"

int main(int argc, char *argv[]) {
    int version;
    NCCLCHECK(ncclGetVersion(&version));
    printf("NCCL version: %d\n", version);

    // Initialize client socket
    const char* server_ip = (argc > 1) ? argv[1] : SERVER_IP;
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    // Receiving NCCL unique ID from server
    ncclUniqueId id;
    int nRanks;
    int baseRank;
    ssize_t bytesRead = read(sock, &id, sizeof(id));
    if (bytesRead < 0) {
        printf("\nError reading NCCL unique ID\n");
        return -1;
    }
    bytesRead = read(sock, &nRanks, sizeof(nRanks));
    if (bytesRead < 0) {
        printf("\nError reading nRanks\n");
        return -1;
    }
    bytesRead = read(sock, &baseRank, sizeof(baseRank));
    if (bytesRead < 0) {
        printf("\nError reading baseRank\n");
        return -1;
    }
    close(sock);
    printf("Received NCCL unique ID, nRanks=%d, baseRank=%d\n", nRanks, baseRank);

    // Initialize CUDA resources
    float **sendbuff = (float **)malloc(GPUS_NUM * sizeof(float *));
    float **recvbuff = (float **)malloc(GPUS_NUM * sizeof(float *));
    float **allgatherbuff = (float **)malloc(BUFFER_SIZE * nRanks * sizeof(float *));
    float **reducescatterbuff = (float **)malloc(BUFFER_SIZE / nRanks * sizeof(float *));
    float **reducebuff = (float **)malloc(GPUS_NUM * sizeof(float *));
    float **broadcastbuff = (float **)malloc(GPUS_NUM * sizeof(float *));
    float **sendrecvbuff = (float **)malloc(GPUS_NUM * sizeof(float *));
    cudaStream_t *s = (cudaStream_t *)malloc(sizeof(cudaStream_t) * GPUS_NUM);
    float* tmpbuff = (float*)malloc(BUFFER_SIZE * sizeof(float));
    for (int i = 0; i < BUFFER_SIZE; i++) tmpbuff[i] = 2;

    for (int devID = 0; devID < GPUS_NUM; ++devID) {
        CUDACHECK(cudaSetDevice(devID));
        CUDACHECK(cudaStreamCreate(s + devID));
        CUDACHECK(cudaMalloc(sendbuff + devID, BUFFER_SIZE * sizeof(float)));
        CUDACHECK(cudaMalloc(recvbuff + devID, BUFFER_SIZE * sizeof(float)));
        CUDACHECK(cudaMalloc(allgatherbuff + devID, BUFFER_SIZE * nRanks * sizeof(float)));
        CUDACHECK(cudaMalloc(reducescatterbuff + devID, BUFFER_SIZE / nRanks * sizeof(float)));
        CUDACHECK(cudaMalloc(reducebuff + devID, BUFFER_SIZE * sizeof(float)));
        CUDACHECK(cudaMalloc(broadcastbuff + devID, BUFFER_SIZE * sizeof(float)));
        CUDACHECK(cudaMalloc(sendrecvbuff + devID, BUFFER_SIZE * sizeof(float)));

        CUDACHECK(cudaMemcpy(sendbuff[devID], tmpbuff, BUFFER_SIZE * sizeof(float), cudaMemcpyHostToDevice));

        CUDACHECK(cudaMemset(recvbuff[devID], 0, BUFFER_SIZE * sizeof(float)));
        CUDACHECK(cudaMemset(allgatherbuff[devID], 0, BUFFER_SIZE * nRanks * sizeof(float)));
        CUDACHECK(cudaMemset(reducescatterbuff[devID], 0, BUFFER_SIZE / nRanks * sizeof(float)));
        CUDACHECK(cudaMemset(reducebuff[devID], 0, BUFFER_SIZE * sizeof(float)));
        CUDACHECK(cudaMemset(broadcastbuff[devID], 0, BUFFER_SIZE * sizeof(float)));
        CUDACHECK(cudaMemset(sendrecvbuff[devID], 0, BUFFER_SIZE * sizeof(float)));
    }
    printf("Init CUDA resources\n");
    printf("Before operations:\n");
    PrintBuffer((void**)sendbuff);

    // Initializing NCCL
    ncclComm_t comms[GPUS_NUM];
    ncclConfig_t config = NCCL_CONFIG_INITIALIZER;
    config.netName = "Socket";
    config.blocking = 1;
    NCCLCHECK(ncclGroupStart());
    for (int devID = 0; devID < GPUS_NUM; devID++) {
        CUDACHECK(cudaSetDevice(devID));
#ifdef USE_CONFIG
        NCCLCHECK(ncclCommInitRankConfig(comms + devID, nRanks, id, baseRank + devID, &config));
#else
        NCCLCHECK(ncclCommInitRank(comms + devID, nRanks, id, baseRank + devID));
#endif

    }
    NCCLCHECK(ncclGroupEnd());

    for (int devID = 0; devID < GPUS_NUM; devID++) {
        int count, device, userRank;
        NCCLCHECK(ncclCommCount(comms[devID], &count));
        NCCLCHECK(ncclCommCuDevice(comms[devID], &device));
        NCCLCHECK(ncclCommUserRank(comms[devID], &userRank));
        printf("comm: %p, CUDADevice: %d, userRank: %d, nRanks: %d\n", comms[devID], device, userRank, count);
    }

    // NCCL communication
    NCCLCHECK(ncclGroupStart());
    for (int devID = 0; devID < GPUS_NUM; devID++) {
        // AllReduce
        NCCLCHECK(ncclAllReduce((const void *)sendbuff[devID], (void *)recvbuff[devID],
                                BUFFER_SIZE, ncclFloat, ncclSum, comms[devID], s[devID]));
        
        // AllGather
        NCCLCHECK(ncclAllGather((const void *)sendbuff[devID], (void *)allgatherbuff[devID],
                                BUFFER_SIZE, ncclFloat, comms[devID], s[devID]));
        
        // ReduceScatter
        NCCLCHECK(ncclReduceScatter((const void *)sendbuff[devID], (void *)reducescatterbuff[devID],
                                    BUFFER_SIZE / nRanks, ncclFloat, ncclSum, comms[devID], s[devID]));
        
        // Reduce
        NCCLCHECK(ncclReduce((const void *)sendbuff[devID], (void *)reducebuff[devID],
                             BUFFER_SIZE, ncclFloat, ncclSum, 0, comms[devID], s[devID]));
        
        // Broadcast
        NCCLCHECK(ncclBroadcast((const void *)sendbuff[0], (void *)broadcastbuff[devID],
                                BUFFER_SIZE, ncclFloat, 0, comms[devID], s[devID]));
    }
    NCCLCHECK(ncclGroupEnd());

    // Send and Recv (must be grouped together)
    NCCLCHECK(ncclGroupStart());
    for (int devID = 0; devID < GPUS_NUM; devID++) {
        if (devID == 0) {
            // dev #0 (client, rank2) sends to dev rank0 (server)
            NCCLCHECK(ncclSend((const void *)sendbuff[devID], BUFFER_SIZE, ncclFloat, 0, comms[devID], s[devID]));
        } 
        else if (devID == 1) {
            // dev #1 (client, rank3) receives from dev rank1 (server)
            NCCLCHECK(ncclRecv((void *)sendrecvbuff[devID], BUFFER_SIZE, ncclFloat, 1, comms[devID], s[devID]));
        }
    }
    NCCLCHECK(ncclGroupEnd());

    // Synchronizing on CUDA stream
    for (int i = 0; i < GPUS_NUM; i++) {
        CUDACHECK(cudaSetDevice(i));
        CUDACHECK(cudaStreamSynchronize(s[i]));
        ncclResult_t ncclAsyncErr;
        NCCLCHECK(ncclCommGetAsyncError(comms[i], &ncclAsyncErr));
        if (ncclAsyncErr != ncclSuccess) {
            // An asynchronous error happened. Stop the operation and destroy
            // the communicator
            NCCLCHECK(ncclCommAbort(comms[i]));
        }
    }
    printf("After operations:\n");
    printf("AllReduce result:\n");
    PrintBuffer((void**)recvbuff);
    printf("AllGather result:\n");
    PrintBuffer((void**)allgatherbuff, BUFFER_SIZE * nRanks);
    printf("ReduceScatter result:\n");
    PrintBuffer((void**)reducescatterbuff, BUFFER_SIZE / nRanks);
    printf("Reduce result:\n");
    PrintBuffer((void**)reducebuff);
    printf("Broadcast result:\n");
    PrintBuffer((void**)broadcastbuff);
    printf("SendRecv result:\n");
    PrintBuffer((void**)sendrecvbuff);

    // Freeing device memory
    for (int i = 0; i < GPUS_NUM; i++) {
        CUDACHECK(cudaSetDevice(i));
        CUDACHECK(cudaFree(sendbuff[i]));
        CUDACHECK(cudaFree(recvbuff[i]));
        CUDACHECK(cudaFree(allgatherbuff[i]));
        CUDACHECK(cudaFree(reducescatterbuff[i]));
        CUDACHECK(cudaFree(reducebuff[i]));
        CUDACHECK(cudaFree(broadcastbuff[i]));
        CUDACHECK(cudaFree(sendrecvbuff[i]));
    }

    // Finalizing NCCL
    for (int devID = 0; devID < GPUS_NUM; devID++) {
        NCCLCHECK(ncclCommDestroy(comms[devID]));
    }

    printf("Client completed successfully\n");
    return 0;
}