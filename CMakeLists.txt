project(cppunix CXX)
cmake_minimum_required(VERSION 3.5)
include("$ENV{CMAKI_PWD}/node_modules/npm-mas-mas/cmaki/cmaki.cmake")
cmaki_setup()
cmaki_find_package_boost()

# cmaki_find_package(design-patterns-cpp14)
# cmaki_find_package(fast-event-system)
# cmaki_find_package(asyncply)
#########################################
# cmaki_find_package(paho-mqtt3)
# cmaki_find_package(paho-mqttpp3)
# cmaki_find_package(teelogging)
# cmaki_find_package(spdlog)
# cmaki_find_package(ois)
# cmaki_find_package(json)
# cmaki_find_package(haxx-libcurl)
# cmaki_find_package(openssl)
# cmaki_find_package(spdlog)

cmaki_google_test(coroutine tests/test_coroutine.cpp PTHREADS)
cmaki_google_test(channel tests/test_channel.cpp PTHREADS)
cmaki_google_test(shell tests/test_shell.cpp PTHREADS)

