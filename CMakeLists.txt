project(project_name CXX)
cmake_minimum_required(VERSION 3.2.2)

set(CMAKI_REPOSITORY "http://artifacts.myftp.biz:8080")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmaki/cmaki.cmake")
cmaki_setup()

cmaki_find_package(boost-coroutine2)
cmaki2_gtest(pipeline tests/test_pipeline.cpp PTHREADS)

