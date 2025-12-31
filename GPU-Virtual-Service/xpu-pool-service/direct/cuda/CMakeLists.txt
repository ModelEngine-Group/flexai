cmake_minimum_required(VERSION 3.22.0)
project(cuda_direct)

include_directories(/usr/local/cuda/targets/x86_64-linux/include
                    ${COMMON_INCLUDE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_custom_target(cuda_original_target ${CMAKE_CURRENT_SOURCE_DIR}/../make_lib_original.sh cuda -)

aux_source_directory(src SRC_LIST)
aux_source_directory(${COMMON_SRC} SRC_LIST)
if(UNIT_TEST)
add_library(cuda_direct STATIC src/hooks/cuda_hooks.cpp ${SRC_LIST})
add_library(gpu-monitor STATIC src/tools/monitor.cpp ${COMMON_TOOLS_SRC}/monitor_base.cpp ${SRC_LIST})
else()
add_library(cuda_direct SHARED src/hooks/cuda_hooks.cpp ${SRC_LIST})
add_executable(gpu-monitor src/tools/monitor.cpp ${COMMON_TOOLS_SRC}/monitor_base.cpp ${SRC_LIST})
set_target_properties(cuda_direct PROPERTIES NO_SONAME ON)
target_link_options(cuda_direct PUBLIC "-Wl,-soname,libcuda.so.1")
endif()

target_compile_definitions(cuda_direct PRIVATE __CUDA_API_VERSION_INTERNAL)
target_link_libraries(cuda_direct cuda-original nvidia-ml)
add_dependencies(cuda_direct cuda_original_target)

target_link_libraries(gpu-monitor cuda-original nvidia-ml)