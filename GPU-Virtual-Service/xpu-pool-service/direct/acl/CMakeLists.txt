cmake_minimum_required(VERSION 3.22.0)
project(acl_direct)

include_directories(
    ${ENV{ASCEND_TOOLKIT_HOME}}/include
    ${ENV{ASCEND_HOME_PATH}}/../driver/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/runtime/inc
    ${COMMON_INCLUDE}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_custom_target(runtime_original_target ${CMAKE_CURRENT_SOURCE_DIR}/../make_lib_original.sh runtime_)

# * direct acl libs
aux_source_directory(src SRC_LIST)
aux_source_directory(${COMMON_SRC} SRC_LIST)

link_directories(
    ${ENV{ASCEND_HOME_PATH}}/${CMKE_SYSTEM_PROCESSOR}-linux/runtime
    ${ENV{ASCEND_HOME_PATH}}/lib64
    ${ENV{ASCEND_HOME_PATH}}/../driver/lib64/driver
)

if(UNIT_TEST)
    add_library(runtime_direct STATIC src/hooks/runtime_hooks.cpp ${SRC_LIST})
    add_library(npu-monitor STATIC src/tools/monitor.cpp ${COMMON_TOOLS_SRC}/monitor_base.cpp ${SRC_LIST})
else()
    # pytorch-cpu uses _GLIBCX_USE_CXX11_ABI=0 as default, we need to follow
    add_compile_definitions(_GLIBCX_USE_CXX11_ABI=0)
    add_library(runtime_direct SHARED src/hooks/runtime_hooks.cpp ${SRC_LIST})
    add_executable(npu-monitor src/tools/monitor.cpp ${COMMON_TOOLS_SRC}/monitor_base.cpp ${SRC_LIST})
    # set soname to cann
    set_target_properties(runtime_direct PROPERTIES NO_SONAME ON)
    target_link_options(runtime_direct PUBLIC "-Wl,-soname,libruntime.so")
endif()

target_link_libraries(runtime_direct runtime_original dcmi)
add_dependencies(runtime_direct runtime_original_target)

target_link_libraries(npu-monitor runtime_original dcmi)