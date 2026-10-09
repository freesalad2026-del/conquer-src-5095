cmake_minimum_required(VERSION 3.18)
project(ConquerNativeAccountServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MariaDB Connector/C and MySQL Client libraries expose the same mysql.h API.
# For Visual Studio: -DMYSQL_ROOT="C:/Program Files/MariaDB/MariaDB Connector C"
set(MYSQL_ROOT "" CACHE PATH "MySQL or MariaDB Connector/C installation")
find_path(MYSQL_INCLUDE_DIR NAMES mysql.h
    HINTS "${MYSQL_ROOT}" "${MYSQL_ROOT}/include"
    PATH_SUFFIXES include mysql mariadb)
find_library(MYSQL_LIBRARY NAMES mariadb libmariadb mysqlclient libmysql
    HINTS "${MYSQL_ROOT}" "${MYSQL_ROOT}/lib"
    PATH_SUFFIXES lib lib/mariadb)
if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARY)
    message(FATAL_ERROR "Install MariaDB Connector/C (or MySQL C client) and set MYSQL_ROOT. Missing mysql.h or client library.")
endif()

find_package(Threads REQUIRED)
add_executable(AccServerCpp src/main.cpp)
target_include_directories(AccServerCpp PRIVATE src "${MYSQL_INCLUDE_DIR}")
target_link_libraries(AccServerCpp PRIVATE "${MYSQL_LIBRARY}" Threads::Threads)
if(WIN32)
    target_link_libraries(AccServerCpp PRIVATE ws2_32)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(auth_protocol_tests tests/protocol_tests.cpp)
    target_include_directories(auth_protocol_tests PRIVATE src)
    add_test(NAME auth_protocol_tests COMMAND auth_protocol_tests)
endif()
