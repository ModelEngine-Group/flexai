#!/bin/bash
set -o errexit
set -o pipefail
set -o nounset

print_error() {
    echo -e "\e[1m\e[31mError occurred!\e[0m"
    exit 1
}

print_success() {
    echo -e "\e[1m\e[32mSuccessfully built executable!\e[0m"
    if [ $1 = "ON" ]; then
        echo "To run the client application, set the env variables:"
        echo 'LD_PRELOAD=./out/lib64/libcuda_hook.so LD_LIBRARY_PATH=./out/lib64:$LD_LIBRARY_PATH'
    else 
        echo "Run the server: ./out/Server"
    fi
    exit 0
}

compile() {
    BUILD_FOR_CLIENT=$1
    local CLIENT_BUILD=ON
    local SERVER_BUILD=OFF

    if [[ $BUILD_FOR_CLIENT == 1 ]]; then
        CLIENT_BUILD=ON
        SERVER_BUILD=OFF
    else
        CLIENT_BUILD=OFF
        SERVER_BUILD=ON
    fi  

    echo_cmd "rm -rf build out"

    echo_cmd "cd include/shmqueue"
    echo_cmd "rm -rf build"
    echo_cmd "mkdir build"
    echo_cmd "cd build"
    echo_cmd "cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_INSTALL_PREFIX=$WORK_PATH/out .."
    echo_cmd "make -j$(nproc --ignore=2)"
    echo_cmd "make install"

    echo_cmd "cd $WORK_PATH"
    echo_cmd "mkdir build"
    echo_cmd "cd build"
    echo_cmd "cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_CLIENT=$CLIENT_BUILD -DBUILD_SERVER=$SERVER_BUILD -DBUILD_WITH_SAMPLE=$WITH_SAMPLE -DHOOK_VERBOSE_MAKEFILE=$VERBOSE_MAKEFILE -DCMAKE_INSTALL_PREFIX=$WORK_PATH/out -DCMAKE_SKIP_RPATH=ON .."
    echo_cmd "make -j$(nproc --ignore=2)"
    echo_cmd "make install" 

    if [[ $BUILD_FOR_CLIENT == 1 ]]; then
        echo "========== create soft link =========="
        echo_cmd "cd $WORK_PATH/out/lib64"
        echo_cmd "ln -s libcuda_hook_drv.so libcuda.so.1"
        echo_cmd "ln -s libcuda.so.1 libcuda.so"
    fi

    echo_cmd "cd $WORK_PATH"
}

trap print_error ERR

WORK_PATH=$(cd $(dirname $0) && pwd) && cd $WORK_PATH

CUDA_ARCHITECTURE=70 # a: (Tesla P100: 60, GTX1080Ti: 61, Tesla V100: 70, RTX2080Ti: 75, NVIDIA A100: 80, RTX3080Ti / RTX3090 / RTX A6000: 86, RTX4090: 89, NVIDIA H100: 90)
BUILD_TYPE=Release # t: (Debug, Release)
WITH_SAMPLE=ON # w: (ON, OFF)
VERBOSE_MAKEFILE=OFF # b: (ON, OFF)
CLIENT_BUILD=ON
SERVER_BUILD=OFF

echo_cmd() {
    echo $1
    $1
}

while getopts ":r:t:b:csahwd" opt
do
    case $opt in
        r)
        CUDA_ARCHITECTURE=$OPTARG
        echo "CUDA_ARCHITECTURE: $CUDA_ARCHITECTURE"
        ;;
        t)
        BUILD_TYPE=$OPTARG
        echo "BUILD_TYPE: $BUILD_TYPE"
        ;;
        b)
        VERBOSE_MAKEFILE=$OPTARG
        echo "VERBOSE_MAKEFILE: $VERBOSE_MAKEFILE"
        ;;
        c)
        CLIENT_BUILD=ON
        SERVER_BUILD=OFF
        echo "BUILD FOR CLIENT, NOT FOR SERVER"
        ;;
        s)
        CLIENT_BUILD=OFF
        SERVER_BUILD=ON
        WITH_SAMPLE=OFF
        echo "BUILD FOR SERVER, NOT FOR CLIENT"
        ;;
        a)
        CLIENT_BUILD=ON
        SERVER_BUILD=ON
        WITH_SAMPLE=OFF
        echo "BUILD FOR BOTH CLIENT AND SERVER"
        ;;
        w)
        WITH_SAMPLE=$OPTARG
        echo "WITH_SAMPLE: $WITH_SAMPLE"
        ;;
        d)
        echo "clean the output directory"
        echo_cmd "rm -rf out"
        echo "clean the build directory"
        echo_cmd "rm -rf build"
        exit 0
        ;;
        h)
        echo "  Options:"
        echo "    -c                     - Build for client, not for server"
        echo "    -s                     - Build for server, not for client"
        echo "    -a                     - Build for both client and server"
        echo "    -w                     - Build with CUDA Sample Codes: ON / OFF"
        echo "    -r <CUDA_ARCHITECTURE> - Build for specific CUDA (GPU) architecture (default: 70)"
        echo "                               60 - Tesla P100"
        echo "                               61 - GTX1080Ti"
        echo "                               70 - Tesla V100"
        echo "                               75 - RTX2080Ti"
        echo "                               80 - NVIDIA A100"
        echo "                               86 - RTX3080Ti / RTX3090 / RTX A6000"
        echo "                               89 - RTX4090"
        echo "                               90 - NVIDIA H100"
        echo "    -t <BUILD_TYPE>        - Debug / Release (default: Release)"
        echo "    -b                     - Verbose Makefile: ON / OFF (default: OFF)"
        exit 1
        ;;
        ?)
        echo "invalid param: $OPTARG"
        exit 1
        ;;
    esac
done

if [[ $SERVER_BUILD == "ON" ]]; then
    echo "========== build for server =========="
    compile 0

    if [[ $CLIENT_BUILD == "ON" ]]; then
        echo_cmd "mv ./out/Server ./"
        echo "========== build for client =========="
        compile 1
        echo_cmd "mv ./Server ./out/"
    fi
else 
    echo "========== build for client =========="
    compile 1
fi

echo "========== build info =========="
if [ -z "$CUDA_VISIBLE_DEVICES" ]; then
    echo "CUDA_VISIBLE_DEVICES: None"
else
    echo "CUDA_VISIBLE_DEVICES: $CUDA_VISIBLE_DEVICES"
fi
echo "UCX: $INSTALL_UCX_PATH"

# BRANCH=`git rev-parse --abbrev-ref HEAD`
# COMMIT=`git rev-parse HEAD`
# GCC_VERSION=`gcc -dumpversion`
# COMPILE_TIME=$(date "+%H:%M:%S %Y-%m-%d")

# echo "branch: $BRANCH" >> $WORK_PATH/out/hook_version
# echo "commit: $COMMIT" >> $WORK_PATH/out/hook_version
# echo "gcc_version: $GCC_VERSION" >> $WORK_PATH/out/hook_version
# echo "compile_time: $COMPILE_TIME" >> $WORK_PATH/out/hook_version

print_success "$CLIENT_BUILD"

echo "========== build exit =========="
