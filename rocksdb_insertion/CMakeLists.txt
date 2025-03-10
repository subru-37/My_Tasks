cmake_minimum_required(VERSION 3.10)

project(rockdb_insertor)

find_program(PG_CONFIG_EXECUTABLE NAMES pg_config)
find_package(PkgConfig REQUIRED)
pkg_check_modules(UUID REQUIRED uuid)
if(NOT PG_CONFIG_EXECUTABLE)
    message(FATAL_ERROR "pg_config not found! Ensure PostgreSQL is installed and pg_config is in PATH.")
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -Wall -Wextra -fPIC")
execute_process(COMMAND ${PG_CONFIG_EXECUTABLE} --includedir
                OUTPUT_VARIABLE PostgreSQL_INCLUDE_DIR
                OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(COMMAND ${PG_CONFIG_EXECUTABLE} --pkglibdir
                OUTPUT_VARIABLE PGSQL_LIB_DIR
                OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(COMMAND ${PG_CONFIG_EXECUTABLE} --sharedir
                OUTPUT_VARIABLE PGSQL_SHARE_DIR
                OUTPUT_STRIP_TRAILING_WHITESPACE)

set(PGSQL_EXTENSION_DIR "${PGSQL_SHARE_DIR}/extension")

include_directories(${PostgreSQL_INCLUDE_DIR})
include_directories("/home/subra-pt7817/Downloads/postgresql-17.2/src/bin")
include_directories("/home/subra-pt7817/Downloads/postgresql-17.2/src/include")
include_directories("/home/subra-pt7817/Downloads/postgresql-17.2/src/include/storage")
include_directories("/home/subra-pt7817/Downloads/postgresql-17.2/src/include/utils")
include_directories("/home/subra-pt7817/Downloads/postgresql-17.2/src/test/regress")

# Add your custom RocksDB include directory
include_directories("/home/subra-pt7817/RocksDB/usr/local/include")
# Add RocksDB lib directory
link_directories("/home/subra-pt7817/RocksDB/usr/local/lib")

add_library(rocksdb_insertion SHARED rocksdb_insertion.c)
target_link_libraries(rocksdb_insertion ${PostgreSQL_LIBRARIES})
target_link_libraries(rocksdb_insertion PRIVATE uuid)
target_link_libraries(rocksdb_insertion PRIVATE "/home/subra-pt7817/RocksDB/usr/local/lib/librocksdb.so.10")
set_target_properties(rocksdb_insertion PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    PREFIX ""  # Remove default 'lib' prefix
    SUFFIX ".so"
)

target_link_libraries(rocksdb_insertion PRIVATE ${UUID_LIBRARIES})
target_include_directories(rocksdb_insertion PRIVATE ${UUID_INCLUDE_DIRS})


install(TARGETS rocksdb_insertion
    LIBRARY DESTINATION "${PGSQL_LIB_DIR}"
)

# Install SQL script and control file
install(FILES rocksdb_insertion.control rocksdb_insertion--1.0.sql
    DESTINATION "${PGSQL_EXTENSION_DIR}"
)


