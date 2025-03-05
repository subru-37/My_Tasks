cmake_minimum_required(VERSION 3.10)
project(my_logger)

# Find pg_config to dynamically get PostgreSQL paths
find_program(PG_CONFIG_EXECUTABLE NAMES pg_config)

if(NOT PG_CONFIG_EXECUTABLE)
    message(FATAL_ERROR "pg_config not found! Ensure PostgreSQL is installed and pg_config is in PATH.")
endif()

# Retrieve PostgreSQL include, library, and extension directories
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

# Print paths for debugging
message(STATUS "PostgreSQL Include Directory: ${PostgreSQL_INCLUDE_DIR}")
message(STATUS "PostgreSQL Library Directory: ${PGSQL_LIB_DIR}")
message(STATUS "PostgreSQL Extension Directory: ${PGSQL_EXTENSION_DIR}")
message(STATUS "PostgreSQL Source Directory: ${CMAKE_SOURCE_DIR}/../../")

# Include and link PostgreSQL
include_directories(${PostgreSQL_INCLUDE_DIR})
include_directories("${CMAKE_SOURCE_DIR}/../../src/bin")
include_directories("${CMAKE_SOURCE_DIR}/../../src/include")

# Create the shared library for the extension
add_library(my_logger MODULE my_logger.c)

# Set correct .so file properties
set_target_properties(my_logger PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    PREFIX ""  # Remove default 'lib' prefix
    SUFFIX ".so"
)

# Install compiled shared library
install(TARGETS my_logger
    LIBRARY DESTINATION "${PGSQL_LIB_DIR}"
)

# Install SQL script and control file
install(FILES my_logger.control my_logger--1.0.sql
    DESTINATION "${PGSQL_EXTENSION_DIR}"
)