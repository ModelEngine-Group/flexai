# The lowest Cmake version required
cmake_minimum_required(VERSION 3.22.0)

# project info
project(XPUPoolService)

add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-D_GNU_SOURCE)

# security options
include(CheckPIESupported)
check_pie_supported()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_compile_options(
    -Wall -Wextra -Werror -Wnull-dereference -Wfloat-equal 
    -fstack-protector-all -fno-common -fvisibility=internal -g
)

set(CMAKE_CXX_FLAGS_RELEASE "-D_FORTIFY_SOURCE=2 -O2")
add_link_options(-Wl,-z,relro,-z,now,-z,defs,-z,noexecstack)

add_subdirectory(direct)
