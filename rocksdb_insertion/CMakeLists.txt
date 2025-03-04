cmake_minimum_required(VERSION 3.10)

project(rockdb_insertor)

find_program(PG_CONFIG_EXECUTABLE NAMES pg_config)

if(NOT PG_CONFIG_EXECUTABLE)
    message(FATAL_ERROR "pg_config not found! Ensure PostgreSQL is installed and pg_config is in PATH.")
endif()

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
include_directories("/home/subru/Documents/postgresql-14.3/src/bin")
include_directories("/home/subru/Documents/postgresql-14.3/src/include")
include_directories("/home/subru/Documents/postgresql-14.3/src/include/storage")
include_directories("/home/subru/Documents/postgresql-14.3/src/include/utils")
include_directories("/home/subru/Documents/postgresql-14.3/src/test/regress")

add_library(rocksdb_insertion MODULE rocksdb_insertion.c)

set_target_properties(rocksdb_insertion PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    PREFIX ""  # Remove default 'lib' prefix
    SUFFIX ".so"
)

install(TARGETS rocksdb_insertion
    LIBRARY DESTINATION "${PGSQL_LIB_DIR}"
)

# Install SQL script and control file
install(FILES rocksdb_insertion.control rocksdb_insertion--1.0.sql
    DESTINATION "${PGSQL_EXTENSION_DIR}"
)


