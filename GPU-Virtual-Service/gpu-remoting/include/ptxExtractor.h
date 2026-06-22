#ifndef PTX_EXTRACTOR_H
#define PTX_EXTRACTOR_H

#include "configure.h"
#include "./hashing/robin_hood.h"
#include "./conqueue/readerwriterqueue.h"
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>
#include <nv_decode.h>

size_t inline SafeStr2Ull(const std::string& str) {
    try {
        return std::stoull(str);
    } catch (...) {
        return 0;
    }
}

class PTXExtractor {
private:
    const char* myName_ = "PTXExtractor";
    std::mutex mutex_;
    boost::asio::thread_pool pool_;
    // std::vector<void*> imageList_;

public:
    moodycamel::BlockingReaderWriterQueue< std::pair<void*, size_t> >* _imageQueue;
    robin_hood::unordered_flat_map<std::string, uint64_t>* _kernelDevMap;
    volatile bool _readyClosed = false;
    volatile bool _finished = false;
    

    PTXExtractor(moodycamel::BlockingReaderWriterQueue< std::pair<void*, size_t> >* iq, 
                 robin_hood::unordered_flat_map<std::string, uint64_t>* kdm)
        : _imageQueue(iq), _kernelDevMap(kdm), pool_(std::thread::hardware_concurrency()) {
        // imageList_.reserve(240);
        tool::Logging(LOG_INFO, myName_, "PTXExtractor is ready\n");
    }

    ~PTXExtractor() {
        tool::Logging(myName_, "ready to close PTXExtractor\n");
        if (_finished == false) { // PTXExtractor is still working
            if (_readyClosed == false) { // cudaLaunchKernel has not been called
                _readyClosed = true;
            }
            while (_finished == false) { // waiting the Run() to finish
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        // for (auto& image : imageList_) {
        //     free(image);
        // }
        delete _imageQueue;
        delete _kernelDevMap;
        tool::Logging(LOG_INFO, myName_, "close the PTXExtractor\n");
    }

    void Extract(void* image, size_t imageSize) {
        // step1: writing the image to a file

        // 创建一个临时文件来存储image内容
        char temp_filename[] = "/tmp/cuda_image_XXXXXX";
        int fd = mkstemp(temp_filename);
        if (fd == -1) {
            throw std::runtime_error("Failed to create temp file");
        }
        // 使用RAII来确保文件描述符被关闭
        struct FDCloser {
            void operator()(int *fd) const { if (*fd != -1) close(*fd); }
        };
        std::unique_ptr<int, FDCloser> fd_closer(&fd);
        // 写入image内容到临时文件
        if (write(fd, image, imageSize) != static_cast<ssize_t>(imageSize)) {
            throw std::runtime_error("Failed to write image to temp file");
        }

        // step2: disassemble the image to PTX codes
        
        // 使用cuobjdump命令处理临时文件
        std::string ptxCodes;
        ptxCodes.reserve(1024 * 1024);
        std::array<char, 4096> buffer;
        std::string cmd = "cuobjdump -ptx " + std::string(temp_filename);
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) {
            throw std::runtime_error("popen() failed!");
        }

        // 读取PTX代码
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            ptxCodes += buffer.data();
        }   

        // 删除临时文件
        std::remove(temp_filename);

        // step3: match kernel body and extract dev ptr from kernel params

        // 提取PTX代码中的一系列Kernel
        std::vector<KernelPtx_t> kernels;
        kernels.reserve(150);
        ExtractKernelsFromPtx(ptxCodes, kernels);

        for (const auto& kernel : kernels) {
            tool::Logging(myName_, "Kernel name: %s\n", kernel.name.c_str());

            // // if (kernel.name == "_ZN2at6native43_GLOBAL__N__eaa73ff3_10_Dropout_cu_f338191124fused_dropout_kernel_vecIffjLi1ELi4EbEEvNS_4cuda6detail10TensorInfoIT_T1_EES8_NS5_IT4_S7_EES7_T0_NS_15PhiloxCudaStateE"
            // if (kernel.name == "_ZN2at6native18elementwise_kernelILi128ELi2EZNS0_15gpu_kernel_implINS0_15CUDAFunctor_addIfEEEEvRNS_18TensorIteratorBaseERKT_EUliE_EEviT1_" 
            // ) {
            //     std::ofstream outFile("flexgv-log-complex-ptx", std::ios::out | std::ios::app); 
            //     outFile << kernel.name << std::endl;
            //     outFile << kernel.body << std::endl;
            //     outFile.close();
            // }

        /*
            // 提取 ld.param.u64 行的寄存器、参数序号和参数偏移量
            robin_hood::unordered_flat_map<std::string, LdParamInfo_t> param_map;
            ExtractParamsFromKernelPtx(kernel.body, param_map);

            // 提取 cvta.to.global.u64 行的源寄存器
            auto dev_ptr_list = new std::vector<std::pair<size_t, size_t>>();
            dev_ptr_list->reserve(param_map.size());
            MatchDevPtrsFromKernelPtx(kernel.body, param_map, dev_ptr_list);
        */

            // 直接提取 ld.param.u64 行的参数序号和参数偏移量
            auto dev_ptr_list = new std::vector<std::pair<size_t, size_t>>();
            dev_ptr_list->reserve(200);
            ExtractDevptrFromKernelPtx(kernel.body, kernel.name, dev_ptr_list);

            // 将Kernel名称和设备内存指针映射关系存入_kernelDevMap，共享给serverEp
            mutex_.lock();
            _kernelDevMap->insert({kernel.name, reinterpret_cast<uint64_t>(dev_ptr_list)});
            mutex_.unlock();

            // 打印设备指针列表
            for (const auto& dev_ptr : *dev_ptr_list) {
                tool::Logging(myName_, "\tParam #%zu, Offset: %zu\n", dev_ptr.first, dev_ptr.second);
            }         
        }
        tool::Logging(myName_, "finished Extract().\n");
        // free(image); 
        // imageList_.push_back(image); // the image will be loaded by cuModuleLoadData
    }
    
    void Run() {
        std::pair<void*, size_t> image;
        while(!_readyClosed || _imageQueue->size_approx() > 0) {
            if (_imageQueue->wait_dequeue_timed(image, std::chrono::milliseconds(5))) {
                tool::Logging(LOG_DEBUG, myName_, "pop image: %p, size: %zu\n", image.first, image.second);
                boost::asio::post(pool_, 
                    std::bind(&PTXExtractor::Extract, this, image.first, image.second));
            }
        }
        pool_.join();
        _finished = true;
        tool::Logging(LOG_INFO, myName_, "finished PTXExtractor work\n");
    }

    // Extract kernel body from PTX codes by matching brackets
    size_t ExtractKernelBody(const std::string& ptx_code, size_t start_pos) {
        std::stack<char> brackets;
        size_t end_pos = start_pos;
        
        for (; end_pos < ptx_code.length(); ++end_pos) {
            if (ptx_code[end_pos] == '{') {
                brackets.push('{');
            } else if (ptx_code[end_pos] == '}') {
                if (!brackets.empty()) {
                    brackets.pop();
                }
                if (brackets.empty()) {
                    ++end_pos; // include the last '}'
                    break;
                }
            }
        }
        return end_pos;
    }

    // Extract kernel's PTX name and its PTX body from PTX codes
    void ExtractKernelsFromPtx(const std::string& ptx_code, std::vector<KernelPtx_t>& kernels) {
        static const boost::regex kernel_pattern(R"(\.entry\s+(\w+)\s*\()");
        boost::sregex_iterator kernels_begin(ptx_code.begin(), ptx_code.end(), kernel_pattern);
        boost::sregex_iterator kernels_end;

        for (auto i = kernels_begin; i != kernels_end; ++i) {
            const boost::smatch& match = *i;
            const std::string& kernel_name = match[1].str();
            size_t body_start_pos = ptx_code.find('{', match.position());
            if (body_start_pos != std::string::npos) {
                size_t body_end_pos = ExtractKernelBody(ptx_code, body_start_pos);
                kernels.emplace_back(
                    kernel_name.c_str(), kernel_name.length(),
                    ptx_code.c_str() + body_start_pos, body_end_pos - body_start_pos
                );
            }
        }
    }

    // Extract param index and param offset from ld.param.u64 line
    static void ExtractDevptrFromKernelPtx(const std::string& kernel_body, const std::string& kernel_name,
                    std::vector<std::pair<size_t, size_t>>* dev_ptr_list) {
        // static const boost::regex ld_pattern(R"(ld\.param\.u64\s+(%\w+),\s+\[\w+_param_(\d+)(?:\+(\d+))?\];|ld\.param\.u64\s+(%\w+),\s+\[%\w+\+(\d+)\];)");
        static const boost::regex ld_pattern(R"(ld\.param\.u64\s+(%\w+),\s+\[\w+_param_(\d+)(?:\+(\d+))?\];|ld\.param\.u64\s+(%\w+),\s+\[(%\w+)(?:\+(\d+))?\];)");
        static const boost::regex mov_b64_pattern(R"(mov\.b64\s+(%\w+),\s+\w+_param_(\d+);)");
        boost::sregex_iterator params_begin(kernel_body.begin(), kernel_body.end(), ld_pattern);
        boost::sregex_iterator params_end;

        for (auto i = params_begin; i != params_end; ++i) {
            const boost::smatch& match = *i;

            if (match[1].matched && match[2].matched) {
                // First pattern: ld.param.u64 dst_register_name, [Kernel_param_0+offset];
                std::string dst_register_name(match[1].first, match[1].second);
                size_t param_index = SafeStr2Ull(match[2].str());
                size_t param_offset = match[3].matched ? SafeStr2Ull(match[3].str()) : 0;

                dev_ptr_list->emplace_back(param_index, param_offset);
            } else if (match[4].matched && match[5].matched) {
                // Second pattern: ld.param.u64 dst_register_name, [Register+Offset]; 
                std::string dst_register_name(match[4].first, match[4].second);
                size_t param_index = 0; // For this form, need to find the corresponding mov.u64 & mov.b64
                size_t param_offset = match[6].matched ? SafeStr2Ull(match[6].str()) : 0;
                std::string related_register(match[5].first, match[5].second);

                // Step 1: Find pre_register based on related_register: mov.u64 pre_register, related_register;
                const boost::regex mov_pattern(R"(mov\.u64\s+)" + related_register + R"(,\s+(%\w+);)");
                boost::sregex_iterator mov_begin(kernel_body.begin(), kernel_body.end(), mov_pattern);
                boost::sregex_iterator mov_end;
                std::string pre_register;
                for (auto mov_it = mov_begin; mov_it != mov_end; ++mov_it) {
                    const boost::smatch& mov_match = *mov_it;
                    pre_register = mov_match[1].str();
                    break; // Assuming only one match needed
                }

                // Step 2: Find param_index based on pre_register: mov.b64 pre_register, Kernel_param_x;
                if (!pre_register.empty()) {
                    // static const boost::regex b64_pattern(R"(mov\.b64\s+)" + pre_register + R"(,\s+\w+_param_(\d+);)");
                    const boost::regex b64_pattern(R"(mov\.b64\s+)" + pre_register + R"(,\s+\w+_param_(\d+);)");
                    boost::sregex_iterator b64_begin(kernel_body.begin(), kernel_body.end(), b64_pattern);
                    boost::sregex_iterator b64_end;

                    for (auto b64_it = b64_begin; b64_it != b64_end; ++b64_it) {
                        const boost::smatch& b64_match = *b64_it;
                        param_index = SafeStr2Ull(b64_match[1].str());
                        break; // Assuming only one match needed
                    }
                }

                dev_ptr_list->emplace_back(param_index, param_offset);
            }
        }

        // Handle mov.b64 pattern: mov.b64 dst_register_name, Kernel_param_x; (no offset)
        boost::sregex_iterator mov_b64_begin(kernel_body.begin(), kernel_body.end(), mov_b64_pattern);
        for (auto i = mov_b64_begin; i != params_end; ++i) {
            const boost::smatch& match = *i;
            if (match[1].matched && match[2].matched) {
                std::string dst_register_name(match[1].first, match[1].second);
                size_t param_index = SafeStr2Ull(match[2].str());

                // special case: gpu_kernel_impl<at::native::CUDAFunctor_add<float> >(at::TensorIteratorBase&, at::native::CUDAFunctor_add<float> const&)
                if (kernel_name.find("_ZN2at6native18elementwise_kernelILi128ELi2EZNS0_15gpu_kernel_implINS0_15CUDAFunctor_addIfEEEEvRNS_18TensorIteratorBaseERKT_EUliE_EEviT1_") != std::string::npos) {
                    continue; // skipping the mov.b64 pattern (the memory contains some device pointers, so it's not a real device pointer)
                }

                dev_ptr_list->emplace_back(param_index, 0); // No offset in this case

                // special case: CatArrInputTensorMetadata related kernel, https://github.com/pytorch/pytorch/blob/feef057691b357481981fd24483a817ce87c8517/aten/src/ATen/native/cuda/Shape.cu#L155
                if (kernel_name.find("CatArrInputTensorMetadata") != std::string::npos) {
                    size_t  size = 1000;
                    char    realname[size];
                    int     status;
                    __cu_demangle(kernel_name.c_str(), realname, &size, &status);
                    std::string realname_str(realname);
                    boost::regex bracket_pattern("<[^<>]*>.*?<[^<>]*?,\\s*[^<>]*?,\\s*[^<>]*?,\\s*\\(int\\)(\\d+)");
                    boost::smatch newmatch;
                    if (boost::regex_search(realname_str, newmatch, bracket_pattern)) {
                        size_t batchSize = SafeStr2Ull(newmatch[1].str());  // matching the Template parameter(batch size)
                        for (size_t i = 0; i < batchSize; ++i) { // push all the device pointers from input buffer
                            dev_ptr_list->emplace_back(param_index, i * 8); 
                        }
                    }
                }
            }
        }
        std::sort((*dev_ptr_list).begin(), (*dev_ptr_list).end());
        (*dev_ptr_list).erase(std::unique((*dev_ptr_list).begin(), (*dev_ptr_list).end()), (*dev_ptr_list).end()); // avoid loading the same device pointer repeatedly
    }

    // [not used] Extract register, param index and param offset from ld.param.u64 line
    void ExtractParamsFromKernelPtx(const std::string& kernel_body, 
                    robin_hood::unordered_flat_map<std::string, LdParamInfo_t>& param_map) {
        // static const boost::regex ld_pattern(R"(ld\.param\.u64\s+(%\w+),\s+\[\w+_param_(\d+)(?:\+(\d+))?\];|ld\.param\.u64\s+(%\w+),\s+\[%\w+\+(\d+)\];)");
        static const boost::regex ld_pattern(R"(ld\.param\.u64\s+(%\w+),\s+\[\w+_param_(\d+)(?:\+(\d+))?\];|ld\.param\.u64\s+(%\w+),\s+\[(%\w+)(?:\+(\d+))?\];)");
        boost::sregex_iterator params_begin(kernel_body.begin(), kernel_body.end(), ld_pattern);
        boost::sregex_iterator params_end;

        for (auto i = params_begin; i != params_end; ++i) {
            const boost::smatch& match = *i;

            if (match[1].matched && match[2].matched) {
                // First pattern: ld.param.u64 dst_register_name, [Kernel_param_0+offset];
                std::string dst_register_name(match[1].first, match[1].second);
                size_t param_index = SafeStr2Ull(match[2].str());
                size_t param_offset = match[3].matched ? SafeStr2Ull(match[3].str()) : 0;

                param_map.insert({std::move(dst_register_name), LdParamInfo_t(param_index, param_offset)});
            } else if (match[4].matched && match[5].matched) {
                // Second pattern: ld.param.u64 dst_register_name, [Register+offset];
                std::string dst_register_name(match[4].first, match[4].second);
                size_t param_index = 0; // For this form, need to find the corresponding mov.u64 & mov.b64
                size_t param_offset = match[6].matched ? SafeStr2Ull(match[6].str()) : 0;
                std::string related_register(match[5].first, match[5].second);

                // Step 1: Find pre_register based on related_register
                const boost::regex mov_pattern(R"(mov\.u64\s+)" + related_register + R"(,\s+(%\w+);)");
                boost::sregex_iterator mov_begin(kernel_body.begin(), kernel_body.end(), mov_pattern);
                boost::sregex_iterator mov_end;
                std::string pre_register;
                for (auto mov_it = mov_begin; mov_it != mov_end; ++mov_it) {
                    const boost::smatch& mov_match = *mov_it;
                    pre_register = mov_match[1].str();
                    break; // Assuming only one match needed
                }

                // Step 2: Find param_index based on pre_register
                if (!pre_register.empty()) {
                    // static const boost::regex b64_pattern(R"(mov\.b64\s+)" + pre_register + R"(,\s+\w+_param_(\d+);)");
                    const boost::regex b64_pattern(R"(mov\.b64\s+)" + pre_register + R"(,\s+\w+_param_(\d+);)");
                    boost::sregex_iterator b64_begin(kernel_body.begin(), kernel_body.end(), b64_pattern);
                    boost::sregex_iterator b64_end;

                    for (auto b64_it = b64_begin; b64_it != b64_end; ++b64_it) {
                        const boost::smatch& b64_match = *b64_it;
                        param_index = SafeStr2Ull(b64_match[1].str());
                        break; // Assuming only one match needed
                    }
                }

                param_map.insert({std::move(dst_register_name), LdParamInfo_t(param_index, param_offset)});
            }
        }
    }

    // [not used] Extract source register from cvta.to.global.u64 line and match with ld.param.u64, to extract the location of devPtr
    void MatchDevPtrsFromKernelPtx(const std::string& kernel_body, 
                    robin_hood::unordered_flat_map<std::string, LdParamInfo_t>& param_map,
                    std::vector<std::pair<size_t, size_t>>* dev_ptr_list) {
        static const boost::regex cvta_pattern(R"(cvta\.to\.global\.u64\s+\%\w+,\s+(%\w+);)");
        boost::sregex_iterator cvta_begin(kernel_body.begin(), kernel_body.end(), cvta_pattern);
        boost::sregex_iterator cvta_end;

        for (auto i = cvta_begin; i != cvta_end; ++i) {
            const boost::smatch& match = *i;
            boost::ssub_match source_register_match = match[1];
            
            auto it = param_map.find(std::string(source_register_match.first, source_register_match.second));
            if (it != param_map.end()) {
                if (it->second.isUsed) {
                    continue; // avoid extracting the same source register repeatedly
                }
                (*dev_ptr_list).emplace_back(it->second.index, it->second.offset);
                it->second.isUsed = true;
            }
        }

        // for (const auto& entry : param_map) {
        //     (*dev_ptr_list).emplace_back(entry.second.index, entry.second.offset);
        // }
        
        std::sort((*dev_ptr_list).begin(), (*dev_ptr_list).end());
        (*dev_ptr_list).erase(std::unique((*dev_ptr_list).begin(), (*dev_ptr_list).end()), (*dev_ptr_list).end()); // avoid loading the same device pointer repeatedly
    }

    //     // Extract param index and param offset from ld.param.u64 line
    // static void ExtractDevptrFromKernelPtx(const std::string& kernel_body, 
    //                 std::vector<std::pair<size_t, size_t>>* dev_ptr_list) {
    //     // static const boost::regex ld_pattern(R"(ld\.param\.u64\s+(%\w+),\s+\[\w+_param_(\d+)(?:\+(\d+))?\];|ld\.param\.u64\s+(%\w+),\s+\[%\w+\+(\d+)\];)");
    //     static const boost::regex ld_pattern(R"(ld\.param\.u64\s+(%\w+),\s+\[\w+_param_(\d+)(?:\+(\d+))?\];|mov\.b64\s+%\w+,\s+\w+_param_(\d+);)");
    //     boost::sregex_iterator params_begin(kernel_body.begin(), kernel_body.end(), ld_pattern);
    //     boost::sregex_iterator params_end;

    //     for (auto i = params_begin; i != params_end; ++i) {
    //         const boost::smatch& match = *i;

    //         if (match[1].matched && match[2].matched) {
    //             // First pattern: ld.param.u64 dst_register_name, [Kernel_param_0+offset];
    //             std::string dst_register_name(match[1].first, match[1].second);
    //             size_t param_index = SafeStr2Ull(match[2].str());
    //             size_t param_offset = match[3].matched ? SafeStr2Ull(match[3].str()) : 0;

    //             dev_ptr_list->emplace_back(param_index, param_offset);
    //         } else if (match[4].matched && match[5].matched) {
    //             // Second pattern: mov.b64 dst_register_name, Kernel_param_0;
    //             std::string dst_register_name(match[4].first, match[4].second);
    //             size_t param_index = SafeStr2Ull(match[5].str());
    //             size_t param_offset = match[6].matched ? SafeStr2Ull(match[6].str()) : 0;
    //             dev_ptr_list->emplace_back(param_index, param_offset);
    //         }
    //     }

    //     std::sort((*dev_ptr_list).begin(), (*dev_ptr_list).end());
    //     (*dev_ptr_list).erase(std::unique((*dev_ptr_list).begin(), (*dev_ptr_list).end()), (*dev_ptr_list).end()); // avoid loading the same device pointer repeatedly
    // }
};

#endif
