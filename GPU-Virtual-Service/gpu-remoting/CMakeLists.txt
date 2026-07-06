cmake_minimum_required (VERSION 3.12)

project (FlexGV LANGUAGES C CXX)

set (CMAKE_VERBOSE_MAKEFILE ${HOOK_VERBOSE_MAKEFILE})

# set (CMAKE_C_VISIBILITY_PRESET hidden) # useless for our cuda hook
# set (CMAKE_CXX_VISIBILITY_PRESET hidden)
set (CMAKE_POSITION_INDEPENDENT_CODE ON)

set (CMAKE_C_FLAGS "-std=c17")
set (CMAKE_C_FLAGS_DEBUG "$ENV{CFLAGS} -O0 -g2 -ggdb -DHOOK_BUILD_DEBUG")
set (CMAKE_C_FLAGS_RELEASE "$ENV{CFLAGS} -O3")

set (CMAKE_CXX_FLAGS "-std=c++17")
set (CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g2 -ggdb -DHOOK_BUILD_DEBUG")
set (CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3")

set (CMAKE_SHARED_LINKER_FLAGS "-s -Wl,--exclude-libs,ALL")
set (CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed")

set(Boost_ROOT "/usr") # avoid searching from Anaconda
find_package(Boost 1.36.0 REQUIRED COMPONENTS thread system serialization)
if(Boost_FOUND) 
    include_directories(${Boost_INCLUDE_DIRS}) 
    link_directories(${Boost_LIBRARY_DIRS})
    set(BOOST_LIBRARY_OBJ boost_thread boost_system boost_serialization boost_regex boost_chrono)
    # ignore BOOST deprecated headers
    add_definitions("-DBOOST_ALLOW_DEPRECATED_HEADERS")
    add_definitions("-DBOOST_BIND_GLOBAL_PLACEHOLDERS")
else()
    message(FATAL_ERROR "Cannot find boost library")
endif()

find_package(OpenSSL REQUIRED)
if(OPENSSL_FOUND) 
    message(STATUS "Find OpenSSL version: ${OPENSSL_VERSION}")
    if(OPENSSL_VERSION VERSION_LESS "1.1")
        message(FATAL_ERROR "OpenSSL version is less than 1.1")
    endif()
    set(OPENSSL_LIBRARY_OBJ ssl crypto)
else()
    message(FATAL_ERROR "Cannot find OpenSSL library")
endif()

set(UCX_INSTALL_PATH $ENV{INSTALL_UCX_PATH})
if (NOT UCX_INSTALL_PATH)
    message(FATAL_ERROR "INSTALL_UCX_PATH is not set")
else()
    message(STATUS "UCX_INSTALL_PATH: ${UCX_INSTALL_PATH}")
    include_directories(${UCX_INSTALL_PATH}/include)
    link_directories(${UCX_INSTALL_PATH}/lib)
    set(UCX_LIBRARY_OBJ ucp ucs uct ucm)
endif()

find_path(LIBELF_INCLUDE_DIR NAMES libelf.h)
find_library(LIBELF_LIBRARY NAMES elf)
if(LIBELF_INCLUDE_DIR AND LIBELF_LIBRARY)
    message(STATUS "Found libelf: ${LIBELF_LIBRARY}")
else()
    message(FATAL_ERROR "Cannot find libelf library")
endif()

find_package(Python3 COMPONENTS Development REQUIRED)
if (Python3_FOUND)
    include_directories(${Python3_INCLUDE_DIRS})
    set(PYTHON_LIBRARY_OBJ ${Python3_LIBRARIES})
else()
    message(FATAL_ERROR "Cannot find Python3 library")
endif()

if(${CMAKE_BUILD_TYPE} STREQUAL "Release")
    set(SYSTEM_LIBRARY_OBJ pthread jemalloc rt)
else()
    set(SYSTEM_LIBRARY_OBJ pthread rt)
endif()

set(THIRD_OBJ ${OPENSSL_LIBRARY_OBJ} ${BOOST_LIBRARY_OBJ} ${UCX_LIBRARY_OBJ} ${SYSTEM_LIBRARY_OBJ} ${LIBELF_LIBRARY} ${PYTHON_LIBRARY_OBJ})

include_directories (
    ${PROJECT_SOURCE_DIR}/include
)

########### client ###########
if (BUILD_CLIENT)
    file (GLOB CLIENT_SRCS 
        ${PROJECT_SOURCE_DIR}/src/client/*.cpp
        ${PROJECT_SOURCE_DIR}/src/client/*.cc
        ${PROJECT_SOURCE_DIR}/src/client/*.c
        ${PROJECT_SOURCE_DIR}/src/common/*.cc
    )

    file (GLOB CUDA_HOOK_DRV_SRCS
        ${PROJECT_SOURCE_DIR}/src/client/cudaHook.cc
        ${PROJECT_SOURCE_DIR}/src/common/*.cc
    )

    find_package(CUDA REQUIRED)
    include_directories(${CUDA_INCLUDE_DIRS})
    find_library(CUBLAS_LIBRARY cublas HINTS ${CUDA_TOOLKIT_ROOT_DIR} PATH_SUFFIXES lib64 lib)
    find_library(CUBLASLT_LIBRARY cublasLt HINTS ${CUDA_TOOLKIT_ROOT_DIR} PATH_SUFFIXES lib64 lib)
    find_library(CUDNN_LIBRARY cudnn HINTS ${CUDA_TOOLKIT_ROOT_DIR} PATH_SUFFIXES lib lib64)
    find_library(NVML_LIBRARY nvidia-ml HINTS ${CUDA_TOOLKIT_ROOT_DIR} PATH_SUFFIXES lib64 lib)
    add_library(cuda_hook SHARED ${CLIENT_SRCS})
    add_library(cuda_hook_drv SHARED ${CUDA_HOOK_DRV_SRCS})
    set_target_properties(cuda_hook PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set_target_properties(cuda_hook_drv PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(cuda_hook ${CUDA_LIBRARIES} ${CUBLAS_LIBRARY} ${CUBLASLT_LIBRARY} ${CUDNN_LIBRARY} ${THIRD_OBJ} dl)
    target_link_libraries(cuda_hook_drv ${CUDA_LIBRARIES} ${THIRD_OBJ} dl)
    install(TARGETS cuda_hook LIBRARY DESTINATION lib64)
    install(TARGETS cuda_hook_drv LIBRARY DESTINATION lib64)
endif()

########### server ###########
if (BUILD_SERVER)
    file (GLOB SERVER_SRCS 
        ${PROJECT_SOURCE_DIR}/src/server/*.cpp
        ${PROJECT_SOURCE_DIR}/src/server/*.cc
        ${PROJECT_SOURCE_DIR}/include/shmqueue/*.cc
        ${PROJECT_SOURCE_DIR}/src/common/*.cc
    )

    unset (CUDA_USE_STATIC_CUDA_RUNTIME CACHE)
    option (CUDA_USE_STATIC_CUDA_RUNTIME OFF)
    # `find_package` must be called after these options; otherwise, the addresses for device code will be invalid.

    find_package(CUDA REQUIRED) # instread of `enable_language(CUDA)`, which leads to stack smashing
    if(CUDA_FOUND)
        include_directories(${CUDA_INCLUDE_DIRS})
        find_library(CUDA_DRIVER_LIBRARY cuda)
        find_library(NCCL_LIBRARY nccl HINTS ${CUDA_TOOLKIT_ROOT_DIR} PATH_SUFFIXES lib64 lib)
        find_library(CUBLAS_LIBRARY cublas HINTS ${CUDA_TOOLKIT_ROOT_DIR} PATH_SUFFIXES lib64 lib)
        find_library(CUBLASLT_LIBRARY cublasLt HINTS ${CUDA_TOOLKIT_ROOT_DIR} PATH_SUFFIXES lib64 lib)
        find_library(CUDNN_LIBRARY cudnn HINTS ${CUDA_TOOLKIT_ROOT_DIR} PATH_SUFFIXES lib lib64)
        find_library(NVML_LIBRARY nvidia-ml HINTS ${CUDA_TOOLKIT_ROOT_DIR} PATH_SUFFIXES lib64 lib)
        find_library(CUFILT_LIBRARY cufilt HINTS ${CUDA_TOOLKIT_ROOT_DIR} PATH_SUFFIXES lib64 lib)
        # find_library(CUFILE_LIBRARY cufile HINTS ${CUDA_TOOLKIT_ROOT_DIR} PATH_SUFFIXES lib64 lib)
        # set(CUDA_LIBRARY_OBJ ${CUDA_LIBRARIES} ${CUBLAS_LIBRARY} ${CUBLASLT_LIBRARY} ${CUDNN_LIBRARY} ${CUDA_DRIVER_LIBRARY} ${NCCL_LIBRARY} ${CUFILT_LIBRARY} ${CUFILE_LIBRARY} ${NVML_LIBRARY})
        set(CUDA_LIBRARY_OBJ ${CUDA_LIBRARIES} ${CUBLAS_LIBRARY} ${CUBLASLT_LIBRARY} ${CUDNN_LIBRARY} ${CUDA_DRIVER_LIBRARY} ${NCCL_LIBRARY} ${CUFILT_LIBRARY} ${NVML_LIBRARY})
        add_definitions(-DUSE_CUDA) # for ucpUtil.h: copy data from network buffer to CUDA Memory
    else()
        message(FATAL_ERROR "Cannot find CUDA library")
    endif()

    add_executable(Server ${SERVER_SRCS})
    target_link_libraries(Server ${THIRD_OBJ} ${CUDA_LIBRARY_OBJ})
    install(TARGETS Server DESTINATION .)
endif()

########### sample ###########
if (BUILD_WITH_SAMPLE)
    file (GLOB CUDA_SAMPLE_SRCS
        ${PROJECT_SOURCE_DIR}/src/sample/*.cu
        ${PROJECT_SOURCE_DIR}/src/sample/*.cc
        ${PROJECT_SOURCE_DIR}/src/sample/*.cpp
    )

    find_package(CUDA REQUIRED) 
    unset (CUDA_USE_STATIC_CUDA_RUNTIME CACHE)
    option (CUDA_USE_STATIC_CUDA_RUNTIME OFF)
    
    if(CUDA_FOUND)
        include_directories(${CUDA_INCLUDE_DIRS})
        find_library(CUDA_DRIVER_LIBRARY cuda)
        find_library(NCCL_LIBRARY nccl HINTS ${CUDA_TOOLKIT_ROOT_DIR} PATH_SUFFIXES lib64 lib)
        find_library(CUBLAS_LIBRARY cublas HINTS ${CUDA_TOOLKIT_ROOT_DIR} PATH_SUFFIXES lib64 lib)
        find_library(CUBLASLT_LIBRARY cublasLt HINTS ${CUDA_TOOLKIT_ROOT_DIR} PATH_SUFFIXES lib64 lib)
        find_library(CUDNN_LIBRARY cudnn HINTS ${CUDA_TOOLKIT_ROOT_DIR} PATH_SUFFIXES lib lib64)
        set(CUDA_LIBRARY_OBJ ${CUDA_LIBRARIES} ${CUBLAS_LIBRARY} ${CUBLASLT_LIBRARY} ${CUDNN_LIBRARY} ${CUDA_DRIVER_LIBRARY} ${NCCL_LIBRARY})
    else()
        message(FATAL_ERROR "Cannot find CUDA library")
    endif()


    set (CUDA_VERBOSE_BUILD ${HOOK_VERBOSE_MAKEFILE})
    set (CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS} -std=c++17")

    include_directories (
        ${PROJECT_SOURCE_DIR}/src/sample/include
    )

    foreach(CUDA_FILE ${CUDA_SAMPLE_SRCS})
        get_filename_component(TARGET_NAME ${CUDA_FILE} NAME_WE)
        cuda_add_executable(${TARGET_NAME} ${CUDA_FILE})
        target_link_libraries(${TARGET_NAME} ${CUDA_LIBRARY_OBJ} dl pthread)
        install(TARGETS ${TARGET_NAME} RUNTIME DESTINATION .)
    endforeach()
endif ()