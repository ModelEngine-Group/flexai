#ifndef MY_DEFINE_H
#define MY_DEFINE_H

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <iomanip>
#include <bits/stdc++.h>
#include <stdint.h>
#include <getopt.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <execinfo.h>
#include <Python.h>
#include <frameobject.h>

#define LOG_INFO    1 << 0
#define LOG_COMM    1 << 1
#define LOG_ERROR   1 << 2
#define LOG_DEBUG   1 << 3
#define LOG_REGS    1 << 4

#define LOG_CURR    (LOG_INFO | LOG_ERROR)
// #define GV_Monitor
#define GV_GPUMAP
// #define GV_Scheduler
// #define GV_eScheduler
// #define GV_MSGHANDLER

// #define GV_MEMORY
// #define GV_MEMORY_PTX
// #define GV_HANDLE 
// #define GV_BACKUP

static const uint64_t MB_2_B = 1000 * 1000;
static const uint64_t MiB_2_B = uint64_t(1) << 20;
static const uint64_t KB_2_B = 1000;
static const uint64_t KiB_2_B = uint64_t(1) << 10;
static const uint64_t SEC_2_US = 1000 * 1000;

#define PAGE_SIZE 4096
#define ALIGN_UP(size) (((size) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define GET_BLOCK_ID(x)             (x >> BLOCK_SHIFT_BIT)               // address to block id
#define GET_BLOCK_INTER_OFFSET(x)   (x & ((1LL << BLOCK_SHIFT_BIT) - 1)) // address to block internal offset

#define GET_HANDLE_ID(x)            (x & HANDLE_MASK)                    // get the api handle id
#define CHECK_HANDLE_PREFIX(x)      (x & HANDLE_PREFIX)                  // check the api handle id (prefix)


#define CHKERR_ACTION(_cond, _msg, _action) \
    do { \
        if (_cond) { \
            fprintf(stderr, "Failed to %s\n", _msg); \
            _action; \
        } \
    } while (0)


#define CHKERR_JUMP(_cond, _msg, _label) \
    CHKERR_ACTION(_cond, _msg, goto _label)


#define CHKERR_JUMP_RETVAL(_cond, _msg, _label, _retval) \
    do { \
        if (_cond) { \
            fprintf(stderr, "Failed to %s, return value %d\n", _msg, _retval); \
            goto _label; \
        } \
    } while (0)

#define GENERATE_KEY(clientID, dataType) (((clientID) << 2) | (dataType))
#define GET_CLIENT_ID_FROM_KEY(key) ((key) >> 2)
#define GET_DATA_TYPE_FROM_KEY(key) ((key) & 0x3)


namespace tool {
    /**
     * @brief Get the Time Diff object
     * 
     * @param start_time start time
     * @param end_time end time
     * @return double the diff time (sec)
     */
    inline double GetTimeDiff(struct timeval start_time, struct timeval end_time) {
        double second;
        second = static_cast<double>(end_time.tv_sec - start_time.tv_sec) * SEC_2_US + 
            end_time.tv_usec - start_time.tv_usec;
        second = second / SEC_2_US;
        return second; 
    }
    
    /**
     * @brief compare the limits with the input
     * 
     * @param input the input number
     * @param lower the lower bound of the limitation
     * @param upper the upper bound of the limitation
     * @return uint32_t 
     */
    inline uint32_t CompareLimit(uint32_t input, uint32_t lower, uint32_t upper) {
        if (input <= lower) {
            return lower; 
        } else if (input >= upper) {
            return upper;
        } else {
            return input;
        }    
    }
    
    /**
     * @brief get the ceil of the division
     * 
     * @param a 
     * @param b 
     * @return uint32_t 
     */
    inline uint32_t DivCeil(uint32_t a, uint32_t b) {
        uint32_t tmp = a / b;
        if (a % b == 0) {
            return tmp;
        } else {
            return (tmp + 1);
        }
    }
    
    /**
     * @brief print the binary buffer
     * 
     * @param fp the pointer to the buffer
     * @param fp_size the size of the buffer
     */
    inline void PrintBinaryArray(const uint8_t* buffer, size_t buffer_size) {
        for (size_t i = 0; i < buffer_size; i++) {
            fprintf(stdout, "%02x", buffer[i]);
        }
        fprintf(stdout, "\n");
        return ;
    }

    inline void HexDump(const uint8_t* data, size_t size){
        size_t pos = 0;
        while (pos < size) {
            printf("%#05zx: ", pos);
            for (int i = 0; i < 16; i++) {
                if (pos + i < size) {
                    printf("%02x", data[pos + i]);
                } else {
                    printf("  ");
                }
                if (i % 4 == 3) {
                    printf(" ");
                }
            }
            printf(" | ");
            for (int i = 0; i < 16; i++) {
                if (pos + i < size) {
                    if (data[pos + i] >= 0x20 && data[pos + i] <= 0x7e) {
                        printf("%c", data[pos + i]);
                    } else {
                        printf(".");
                    }
                } else {
                    printf(" ");
                }
            }
            printf("\n");
            pos += 16;
        }
    }

    inline void PrintStackTrace(const std::string& filename, bool append = false) {
        const int maxFrames = 128; // max number of frames in the stack trace
        void* buffer[maxFrames];  // buffer pointer to store the stack trace

        int frameCount = backtrace(buffer, maxFrames);
        char** symbols = backtrace_symbols(buffer, frameCount);
        if (symbols == nullptr) {
            fprintf(stderr, "Failed to get the backtrace symbols\n");
            return ;
        }

        std::ofstream outFile;
        if (append) {
            outFile.open(filename, std::ios::app);
        }
        else {
            outFile.open(filename);
        }
        if (!outFile.is_open()) {
            std::cerr << "Failed to open file " << filename << std::endl;
            free(symbols);
            return;
        }

        outFile << "Call stack:" << std::endl;
        for (int i = 0; i < frameCount; ++i) {
            outFile << symbols[i] << std::endl;
            std::cout << symbols[i] << std::endl;
        }
        outFile << std::endl;
        // std::cout << std::endl;

        outFile.close();
        free(symbols);
    }

    inline bool CheckStackTrace(const std::string& target) {
        bool found = false;
        const int maxFrames = 128;
        void* buffer[maxFrames]; 

        int frameCount = backtrace(buffer, maxFrames);
        char** symbols = backtrace_symbols(buffer, frameCount);
        if (symbols == nullptr) {
            return found;
        }

        const char* target_cstr = target.c_str();
        for (int i = frameCount; i >= 0; i--) {
            if (strstr(symbols[i], target_cstr) != NULL) {
                found = true;
                break;
            }
        }
        free(symbols);
        return found;
    }

    inline void PrintPyStackTrace(const std::string& filename, bool append = false) {
        std::ofstream outFile;
        if (append) {
            outFile.open(filename, std::ios::app);
        }
        else {
            outFile.open(filename);
        }
        if (!outFile.is_open()) {
            std::cerr << "Failed to open file " << filename << std::endl;
            return;
        }

        if (!Py_IsInitialized()) {
            return;
        }

        PyGILState_STATE gstate = PyGILState_Ensure();

        PyThreadState *tstate = PyThreadState_Get();
        if (!tstate) {
            outFile << "Failed to get the thread state" << std::endl;
            // std::cerr << "Failed to get the thread state" << std::endl;
            PyGILState_Release(gstate);
            return;
        }

        PyFrameObject *frame = tstate->frame;
        if (!frame) {
            outFile << "Failed to get the frame" << std::endl;
            // std::cerr << "Failed to get the frame" << std::endl;
            PyGILState_Release(gstate);
            return;
        }

        outFile << "Python call stack:" << std::endl;
        while (frame) {
            PyCodeObject *code = (PyCodeObject *)frame->f_code;
            const char *filename_str = PyUnicode_AsUTF8(code->co_filename);
            const char *funcname = PyUnicode_AsUTF8(code->co_name);
            int lineno = PyFrame_GetLineNumber(frame);
            outFile << "  File \"" << filename_str << "\", line " << lineno << ", in " << funcname << std::endl;
            std::cout << "  File \"" << filename_str << "\", line " << lineno << ", in " << funcname << std::endl;
            frame = frame->f_back;
        }
        outFile << std::endl;

        outFile.close();
        PyGILState_Release(gstate);
    }

    inline bool CheckPyStackTrace(const std::string& target) {
        if (!Py_IsInitialized()) {
            return false;
        }

        PyGILState_STATE gstate = PyGILState_Ensure();
        PyThreadState *tstate = PyThreadState_Get();
        if (!tstate) {
            PyGILState_Release(gstate);
            return false;
        }

        PyFrameObject *frame = tstate->frame;
        if (!frame) {
            PyGILState_Release(gstate);
            return false;
        }

        const char* target_cstr = target.c_str();
        while (frame) {
            PyCodeObject *code = (PyCodeObject *)frame->f_code;
            const char *funcname = PyUnicode_AsUTF8(code->co_name);
            if (strstr(funcname, target_cstr) != NULL) {
                PyGILState_Release(gstate);
                return true;
            }

            frame = frame->f_back;
        }

        PyGILState_Release(gstate);
        return false;
    }
    
    /**
     * @brief a simple logger
     * 
     * @param logger the logger name
     * @param fmt the input message
     */
    inline void Logging(int loglevel, const char* logger, const char* fmt, ...) {
        if (LOG_CURR & loglevel) {
            using namespace std;
            char buf[BUFSIZ] = {'\0'};
            va_list ap;
            va_start(ap, fmt);
            vsnprintf(buf, BUFSIZ, fmt, ap);
            va_end(ap);
            time_t t = std::time(nullptr);
            stringstream output;
            output << std::put_time(std::localtime(&t), "%F %T ")
                << "<" << logger << ">: " << buf;
            cerr << output.str();
            return ;
        }
    }

    inline void Logging(const char* logger, const char* fmt, ...) {
        if (LOG_CURR & LOG_DEBUG) {
            using namespace std;
            char buf[BUFSIZ] = {'\0'};
            va_list ap;
            va_start(ap, fmt);
            vsnprintf(buf, BUFSIZ, fmt, ap);
            va_end(ap);
            time_t t = std::time(nullptr);
            stringstream output;
            output << std::put_time(std::localtime(&t), "%F %T ")
                << "<" << logger << ">: " << buf;
            cerr << output.str();
            return ;
        }
    }

    inline uint64_t ProcessMemUsage() {
        using std::ios_base;
        using std::ifstream;
        using std::string;

        uint64_t vm_usage     = 0;
        uint64_t resident_set = 0;

        // 'file' stat seems to give the most reliable results
        //
        ifstream stat_stream("/proc/self/stat",ios_base::in);

        // dummy vars for leading entries in stat that we don't care about
        //
        string pid, comm, state, ppid, pgrp, session, tty_nr;
        string tpgid, flags, minflt, cminflt, majflt, cmajflt;
        string utime, stime, cutime, cstime, priority, nice;
        string O, itrealvalue, starttime;

        // the two fields we want
        //
        unsigned long vsize;
        long rss;

        stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr
                    >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt
                    >> utime >> stime >> cutime >> cstime >> priority >> nice
                    >> O >> itrealvalue >> starttime >> vsize >> rss; // don't care about the rest

        stat_stream.close();

        long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024; // in case x86-64 is configured to use 2MB pages
        vm_usage     = vsize / 1024 ;
        resident_set = rss * page_size_kb;
        return resident_set; // only for PM
    }

    inline uint64_t GetMaxMemoryUsage() {
        struct rusage currentUsage;
        getrusage(RUSAGE_SELF, &currentUsage);
        return currentUsage.ru_maxrss;
    }

    inline std::string GenerateUUID() {
        static std::random_device rd;
        static std::mt19937_64 gen(rd());
        static std::uniform_int_distribution<uint64_t> dis;

        std::array<uint8_t, 16> data;
        std::array<char, 37> uuid;

        // 生成16个随机字节
        uint64_t* ptr = reinterpret_cast<uint64_t*>(data.data());
        ptr[0] = dis(gen);
        ptr[1] = dis(gen);

        // 设置版本 (4) 和变体位
        data[6] = (data[6] & 0x0F) | 0x40;
        data[8] = (data[8] & 0x3F) | 0x80;

        // 转换为十六进制字符串
        static const char* hex_chars = "0123456789abcdef";
        char* dst = uuid.data();
        for (int i = 0; i < 16; ++i) {
            if (i == 4 || i == 6 || i == 8 || i == 10) {
                *dst++ = '-';
            }
            *dst++ = hex_chars[data[i] >> 4];
            *dst++ = hex_chars[data[i] & 0x0F];
        }
        *dst = '\0';

        return std::string(uuid.data(), 36);
    }

    inline bool FileExist(std::string filePath) {
        return std::filesystem::is_regular_file(filePath);
    }

    inline uint64_t GetStrongSeed() {
        uint64_t a = clock();
        struct timeval currentTime;
        gettimeofday(&currentTime, NULL);
        uint64_t b = currentTime.tv_sec * SEC_2_US + currentTime.tv_usec;
        uint64_t c = getpid();

        // Robert Jenkins' 96 bit Mix Function
        a = a - b;  a = a - c;  a = a ^ (c >> 13);
        b = b - c;  b = b - a;  b = b ^ (a << 8);
        c = c - a;  c = c - b;  c = c ^ (b >> 13);
        a = a - b;  a = a - c;  a = a ^ (c >> 12);
        b = b - c;  b = b - a;  b = b ^ (a << 16);
        c = c - a;  c = c - b;  c = c ^ (b >> 5);
        a = a - b;  a = a - c;  a = a ^ (c >> 3);
        b = b - c;  b = b - a;  b = b ^ (a << 10);
        c = c - a;  c = c - b;  c = c ^ (b >> 15);

        return c;
    }

    inline void GetIpStrFromSockaddr(const struct sockaddr_storage *sock_addr, 
                                    char *ip_str, size_t max_size) {
        if (!ip_str) {
            return;  
        }

        if (sock_addr->ss_family == AF_INET) {
            const struct sockaddr_in *addr_in = reinterpret_cast<const struct sockaddr_in *>(sock_addr);
            inet_ntop(AF_INET, &(addr_in->sin_addr), ip_str, max_size);
        }
        else if (sock_addr->ss_family == AF_INET6) {
            const struct sockaddr_in6 *addr_in6 = reinterpret_cast<const struct sockaddr_in6 *>(sock_addr);
            inet_ntop(AF_INET6, &(addr_in6->sin6_addr), ip_str, max_size);
        } else {
            ip_str[0] = '\0'; 
        }
    }

    inline void GetPortStrFromSockaddr(const struct sockaddr_storage *sock_addr, 
                                    char *port_str, size_t max_size) {
        if (!port_str) {
            return;  // 如果port_str是NULL，直接返回
        }

        if (sock_addr->ss_family == AF_INET) {
            const struct sockaddr_in *addr_in = reinterpret_cast<const struct sockaddr_in *>(sock_addr);
            snprintf(port_str, max_size, "%d", ntohs(addr_in->sin_port));
        }
        else if (sock_addr->ss_family == AF_INET6) {
            const struct sockaddr_in6 *addr_in6 = reinterpret_cast<const struct sockaddr_in6 *>(sock_addr);
            snprintf(port_str, max_size, "%d", ntohs(addr_in6->sin6_port));
        } else {
            port_str[0] = '\0'; // 对于不支持的地址家族，设置为空字符串
        }
    }

    inline void SetSockAddr(const char *address_str, uint16_t server_port, 
                    struct sockaddr_storage *saddr, sa_family_t ai_family){
        struct sockaddr_in *sa_in;
        struct sockaddr_in6 *sa_in6;

        /* The server will listen on INADDR_ANY */
        memset(saddr, 0, sizeof(*saddr));

        switch (ai_family) {
        case AF_INET:
            sa_in = (struct sockaddr_in*)saddr;
            if (address_str != NULL) {
                inet_pton(AF_INET, address_str, &sa_in->sin_addr);
            } else {
                sa_in->sin_addr.s_addr = INADDR_ANY;
            }
            sa_in->sin_family = AF_INET;
            sa_in->sin_port   = htons(server_port);
            break;
        case AF_INET6:
            sa_in6 = (struct sockaddr_in6*)saddr;
            if (address_str != NULL) {
                inet_pton(AF_INET6, address_str, &sa_in6->sin6_addr);
            } else {
                sa_in6->sin6_addr = in6addr_any;
            }
            sa_in6->sin6_family = AF_INET6;
            sa_in6->sin6_port   = htons(server_port);
            break;
        default:
            fprintf(stderr, "Invalid address family");
            break;
        }
    }

    inline bool ReadSocketMessage(int sock, uint8_t* buffer, size_t buffer_size) {
        if (sock < 0 || !buffer) {
            tool::Logging(LOG_ERROR, "ReadSocketMessage", "Invalid socket or buffer\n");
            return false;
        }
        size_t bytesRead = 0;
        while (bytesRead < buffer_size) {
            int ret = read(sock, buffer + bytesRead, buffer_size - bytesRead);
            if (ret < 0) {
                tool::Logging(LOG_ERROR, "ReadSocketMessage", "Failed to read from socket\n");
                return false;
            } else if (ret == 0) {
                tool::Logging(LOG_ERROR, "ReadSocketMessage", "Socket closed\n");
                return false;
            }
            bytesRead += ret;
        }
        return true;
    }

} // namespace tool
#endif 