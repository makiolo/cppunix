project(cppunix CXX)
cmake_minimum_required(VERSION 3.0)

set(CMAKI_REPOSITORY "http://artifacts.myftp.biz:8080")
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../cmaki")
	include("${CMAKE_CURRENT_LIST_DIR}/../cmaki/cmaki.cmake")
else()
	include("${CMAKE_CURRENT_LIST_DIR}/node_modules/cmaki/cmaki.cmake")
endif()
cmaki_setup()

cmaki_find_package(boost-headers)
cmaki_find_package(boost-coroutine2)
cmaki_find_package(boost-filesystem)
cmaki_find_package(boost-regex)
cmaki_find_package(teelogging)
cmaki_find_package(fast-event-system)
cmaki_find_package(asyncply)

cmaki2_gtest(coroutine tests/test_coroutine.cpp PTHREADS)
cmaki2_gtest(shell tests/test_shell.cpp PTHREADS)
cmaki2_gtest(channel tests/test_channel.cpp PTHREADS)

