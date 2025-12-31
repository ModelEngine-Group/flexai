#!/bin/bash
# Copyright (C) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
set -e

build_target=$1
image_name=$2
current_dir=$(
    cd "$(dirname "$0")" || exit 1
    pwd
)
top_dir=$(dirname "$(dirname "${current_dir}")")
pkg_dir=${top_dir}/pkg
host_scripts_dir=${current_dir}/host_scripts

echo "current_dir=${current_dir}"
echo "top_dir=${top_dir}"
echo "pkg_dir=${pkg_dir}"
echo "build_target=${build_target}"
echo "image_name=${image_name}"

function arch_config() {
    arch=$(uname -m)
    if [[ ${arch} == "x86_64" ]]; then
        platform="x86"
    elif [[ ${arch} == "aarch64" ]]; then
        platform="arm"
    else
        echo "incorrect arch mode"
        exit 1
    fi
}

function mk_xpu_pkg_dir() {
    [ -e "${pkg_dir}" ] && rm -rf "${pkg_dir}"
    mkdir -p "${pkg_dir}"/images
    mkdir -p "${pkg_dir}"/templates
    chmod -R 750 "${pkg_dir}
}

function build_xpu_component() {
    echo "build xpu component begin"
    cd ${top_dir}/ci && sh build.sh
    echo "build xpu component end"
}

function get_helm_package() {
    cd ${top_dir}/install/helm && helm package gpupool 
    cp -P --remove-destination -rf gpupool-0.1.0.tgz "${pkg_dir}/templates"
    cp -P --remove-destination -rf ../install.sh "${pkg_dir}/templates"
    cp -P --remove-destination -rf ../uninstall.sh "${pkg_dir}"
}

function mknod_func() {
    loopfile_firstname="/dev/loop0"
    loopfile_num=0
    loopfile_name=/dev/loop"${loopfile_num}"
    while true; do
        if [ -b "${loopfile_name}" ]; then
            loopfile_num=$(expr ${loopfile_num} + 1)
            loopfile_name=/dev/loop${loopfile_num}
        else
            sudo mknod ${loopfile_name} b 7 "${loopfile_num}"
            sudo chmod 660 "${loopfile_name}"
            sudo chown root:disk "${loopfile_name}"
            echo "${loopfile_name}"
            break
        fi
    done
}

function make_docker_base_image() {
    # CurrentDir: code_branch/XPUPoolService/ci/xpu_pool/
    mkdir -p "${current_dir}/xpu_docker_build/exporter/euler"
    mkdir -p "${top_dir}/plugin-market/euler"
    mknod_func
    sudo mount "${top_dir}"/../EulerOS_Server/"${platform}"/EulerOS-*-dvd.iso "${current_dir}/xpu_docker_build/exporter/euler"
    sudo mount "${top_dir}"/../EulerOS_Server/"${platform}"/EulerOS-*-dvd.iso "${top_dir}/plugin-market/euler"
    cd "${current_dir}"
    docker import "${top_dir}"/../EulerOS_Server/"${platform}"/EulerOS_Server_*.tar.xz euleros:econtainer
}

function build_image() {
    echo "build $2 image begin"
    cd ${current_dir}/xpu_docker_build/$1
    local tag="$2:${image_tag}"
    docker build --squash --no-cache -t $tag .
    echo "build $2 image end"
    shift 2
    for package in "$@"; do
        image_export_list[$package]+="$tag"
    done
}

function export_images() {
    echo "save images begin"
    docker save -o "${pkg_dir}/images/gpupool_${platform}.tar" ${image_export_list[gpu]}
    docker save -o "${pkg_dir}/images/npupool_${platform}.tar" ${image_export_list[npu]}
    echo "save images end"
}

function build_output_packages() {
    cd "${pkg_dir}"
    mkdir -p ${WORKSPACE}/output/software
    upload_arch=$(echo ${arch} | sed 's/_/-/g')
    zip -1 -y ${WORKSPACE}/output/software/${xpupool_plugin}_${upload_arch}.zip *
    mkdir -p ${WORKSPACE}/output/inner
    cp -P --remove-destination -rf ${top_dir}/XPU_symbols.tar.gz \
        ${WORKSPACE}/output/inner/${xpupool_plugin}_${upload_arch}_sym.tar.gz
    cd -
}

function main() {
    local -A image_export_list
    arch_config
    mk_xpu_pkg_dir
    build_xpu_component
    get_helm_package
    make_docker_base_image
    cd ${top_dir}/plugin-market &&sh build_daemonset.sh
    build_image "cuda_client" "cuda_client_update" gpu
    build_image "acl_client" "acl_client_update" npu
    build_image "gpu-device-plugin" "gpu_device_plugin" gpu
    build_image "npu-device-plugin" "npu_device_plugin" npu
    build_image "exporter" "xpu_exporter" gpu npu
    export_images
    sh ${current_dir}/../cms_signature.sh ${pkg_dir}
    build_output_packages
    sh ${current_dir}/../hwp7s_signature.sh ${WORKSPACE}/output/software
}

main "$@"