#include "./include/nccl_util.h"

int main(int argc, char *argv[]) {
    int version;
    NCCLCHECK(ncclGetVersion(&version));
    printf("NCCL version: %d\n", version);

    const char* server_ip = (argc > 1) ? argv[1] : SERVER_IP;
    int nRanks = GPUS_NUM * (CLIENTS_NUM + 1); // each node has same number of GPUs

    // Initialize server socket
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, server_ip, &address.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %s:%d\n", server_ip, SERVER_PORT);

    // Generating NCCL unique ID
    ncclUniqueId id;
    ncclGetUniqueId(&id);

    // Accepting connections from clients
    for (int i = 0; i < CLIENTS_NUM; ++i) {
        int clientBaseRank = GPUS_NUM + i * GPUS_NUM; //start behind server
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                                 (socklen_t *)&addrlen)) < 0) {
            perror("accept");
            close(server_fd);
            exit(EXIT_FAILURE);
        }
        // Sending NCCL unique ID, nRanks, and its base rank to clients
        send(new_socket, &id, sizeof(id), 0);
        send(new_socket, &nRanks, sizeof(nRanks), 0);
        send(new_socket, &clientBaseRank, sizeof(clientBaseRank), 0);
        close(new_socket);
    }
    printf("NCCL unique ID, nRanks and base rank sent to clients\n");

    float **sendbuff = (float **)malloc(GPUS_NUM * sizeof(float *));
    float **recvbuff = (float **)malloc(GPUS_NUM * sizeof(float *));
    float **allgatherbuff = (float **)malloc(BUFFER_SIZE * nRanks * sizeof(float *));
    float **reducescatterbuff = (float **)malloc(BUFFER_SIZE / nRanks * sizeof(float *));
    float **reducebuff = (float **)malloc(GPUS_NUM * sizeof(float *));
    float **broadcastbuff = (float **)malloc(GPUS_NUM * sizeof(float *));
    float **sendrecvbuff = (float **)malloc(GPUS_NUM * sizeof(float *));
    cudaStream_t *s = (cudaStream_t *)malloc(sizeof(cudaStream_t) * GPUS_NUM);
    // Initialize CUDA resources
    float* tmpbuff = (float*)malloc(BUFFER_SIZE * sizeof(float));
    for (int i = 0; i < BUFFER_SIZE; i++) tmpbuff[i] = 1;

    for (int I = 0; I < GPUS_NUM; ++I) {
        CUDACHECK(cudaSetDevice(I));
        CUDACHECK(cudaStreamCreate(s + I));
        CUDACHECK(cudaMalloc(sendbuff + I, BUFFER_SIZE * sizeof(float)));
        CUDACHECK(cudaMalloc(recvbuff + I, BUFFER_SIZE * sizeof(float)));
        CUDACHECK(cudaMalloc(allgatherbuff + I, BUFFER_SIZE * nRanks * sizeof(float)));
        CUDACHECK(cudaMalloc(reducescatterbuff + I, BUFFER_SIZE / nRanks * sizeof(float)));
        CUDACHECK(cudaMalloc(reducebuff + I, BUFFER_SIZE * sizeof(float)));
        CUDACHECK(cudaMalloc(broadcastbuff + I, BUFFER_SIZE * sizeof(float)));
        CUDACHECK(cudaMalloc(sendrecvbuff + I, BUFFER_SIZE * sizeof(float)));

        CUDACHECK(cudaMemcpy(sendbuff[I], tmpbuff, BUFFER_SIZE * sizeof(float), cudaMemcpyHostToDevice));

        CUDACHECK(cudaMemset(recvbuff[I], 0, BUFFER_SIZE * sizeof(float)));
        CUDACHECK(cudaMemset(allgatherbuff[I], 0, BUFFER_SIZE * nRanks * sizeof(float)));
        CUDACHECK(cudaMemset(reducescatterbuff[I], 0, BUFFER_SIZE / nRanks * sizeof(float)));
        CUDACHECK(cudaMemset(reducebuff[I], 0, BUFFER_SIZE * sizeof(float)));
        CUDACHECK(cudaMemset(broadcastbuff[I], 0, BUFFER_SIZE * sizeof(float)));
        CUDACHECK(cudaMemset(sendrecvbuff[I], 0, BUFFER_SIZE * sizeof(float)));
    }
    printf("Init CUDA resources\n");
    printf("Before operations:\n");
    PrintBuffer((void**)sendbuff);

    // Initializing NCCL
    ncclConfig_t config = NCCL_CONFIG_INITIALIZER;
    config.netName = "Socket";
    ncclComm_t comms[GPUS_NUM];
    NCCLCHECK(ncclGroupStart());
    for (int devID = 0; devID < GPUS_NUM; devID++) {
        CUDACHECK(cudaSetDevice(devID));
#ifdef USE_CONFIG
        NCCLCHECK(ncclCommInitRankConfig(comms + devID, nRanks, id, devID, &config));
#else
        NCCLCHECK(ncclCommInitRank(comms + devID, nRanks, id, devID));
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

        // Send and Recv
        
    }
    NCCLCHECK(ncclGroupEnd());

    // Send and Recv
    NCCLCHECK(ncclGroupStart());
    for (int devID = 0; devID < GPUS_NUM; devID++) {
        if (devID == 1) {
            // dev #1 (server, rank1) sends to dev rank3 (client)
            NCCLCHECK(ncclSend((const void *)sendbuff[devID], BUFFER_SIZE, ncclFloat, 3, comms[devID], s[devID]));
        } 
        else if (devID == 0) {
            // dev #0 (server, rank0) receives from dev rank2 (client)
            NCCLCHECK(ncclRecv((void *)sendrecvbuff[devID], BUFFER_SIZE, ncclFloat, 2, comms[devID], s[devID]));
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
    for (int devID = 0; devID < GPUS_NUM; devID++) {
        CUDACHECK(cudaSetDevice(devID));
        CUDACHECK(cudaFree(sendbuff[devID]));
        CUDACHECK(cudaFree(recvbuff[devID]));
        CUDACHECK(cudaFree(allgatherbuff[devID]));
        CUDACHECK(cudaFree(reducescatterbuff[devID]));
        CUDACHECK(cudaFree(reducebuff[devID]));
        CUDACHECK(cudaFree(broadcastbuff[devID]));
    }

    // Finalizing NCCL
    for (int devID = 0; devID < GPUS_NUM; devID++) {
        ncclCommDestroy(comms[devID]);
    }

    printf("Server completed successfully\n");
    close(server_fd);
    return 0;
}