project(project_name CXX)
cmake_minimum_required(VERSION 3.2.2)

set(CMAKI_REPOSITORY "http://artifacts.myftp.biz:8080")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmaki/cmaki.cmake")
cmaki_setup()

cmaki_find_package(boost-coroutine2)
cmaki_find_package(boost-filesystem)
cmaki_find_package(boost-regex)
# cmaki_find_package(github://makiolo/teelogging)
cmaki_find_package(teelogging)
cmaki2_gtest(coroutine tests/test_coroutine.cpp PTHREADS)
cmaki2_gtest(shell tests/test_shell.cpp PTHREADS)
cmaki2_gtest(channel tests/test_channel.cpp PTHREADS)

