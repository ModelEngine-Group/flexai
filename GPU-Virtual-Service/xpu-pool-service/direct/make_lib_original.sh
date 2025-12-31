#!/usr/bin/bash

if [ -z $1 ]; then
    >&2 echo "provide lib name as argument"
    exit 1
fi

for path_candidate in "/usr/lib64" "${ASCEND_HOME_PATH}/lib64"; do
    if [ -e "${path_candidate}/lib${1}.so" ]; then
        path=$path_candidate
        break;
    fi
done

if [ ! -d $path ]; then
    >&2 echo "can not find the lib"
    exit 1
fi

sep=${2:--}
backup_path=/opt/xpu/bak
mkdir -p ${backup_path}
backup_file=${backup_path}/lib${1}-backup.so
original_file=${path}/lib${1}.so
patched_name=lib${1}${sep}original.so
patched_file=${path}/${patched_name}

function make_lib_backup() {
    [ -e $backup_file ] && return
    readlink $original_file -f | grep "lib${1}\\.so" || exit 1
    cp $original_file $backup_file
    ls $backup_file
}

function make_lib_original() {
    [ -e $patched_file ] && return
    cp $backup_file $patched_file
    patchelf --set-soname $patched_name $patched_file
}

make_lib_backup $1
make_lib_original $1