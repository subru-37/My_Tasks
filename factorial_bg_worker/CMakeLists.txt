cmake_minimum_required(VERSION 3.10)
project(factorial_bg_worker)

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

# message(STATUS "PostgreSQL Include Directory: ${PostgreSQL_INCLUDE_DIR}")
# message(STATUS "PostgreSQL Library Directory: ${PGSQL_LIB_DIR}")
# message(STATUS "PostgreSQL Extension Directory: ${PGSQL_EXTENSION_DIR}")
# message(STATUS "PostgreSQL Source Directory: ${CMAKE_SOURCE_DIR}/../../src")
# message(STATUS "PostgreSQL regress Directory: ${CMAKE_SOURCE_DIR}/../../src/test/regress")

# include_directories("/home/subru/Documents/postgresql-14.5/src/include")
include_directories(${PostgreSQL_INCLUDE_DIR})
include_directories("/home/subru/Documents/postgresql-14.3/src/bin")
include_directories("/home/subru/Documents/postgresql-14.3/src/include")
include_directories("/home/subru/Documents/postgresql-14.3/src/include/storage")
include_directories("/home/subru/Documents/postgresql-14.3/src/include/utils")
include_directories("/home/subru/Documents/postgresql-14.3/src/test/regress")


add_library(factorial_bg_worker MODULE factorial_bg_worker.c)

set_target_properties(factorial_bg_worker PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    PREFIX ""  # Remove default 'lib' prefix
    SUFFIX ".so"
)

# Install compiled shared library
install(TARGETS factorial_bg_worker
    LIBRARY DESTINATION "${PGSQL_LIB_DIR}"
)

# Install SQL script and control file
install(FILES factorial_bg_worker.control factorial_bg_worker--1.0.sql
    DESTINATION "${PGSQL_EXTENSION_DIR}"
)

enable_testing()  # Enables CTest
find_program(PG_REGRESS pg_regress)

if (NOT PG_REGRESS)
    message(FATAL_ERROR "pg_regress not found! Please provide the correct path.")
endif()

add_test(NAME regression_tests
        COMMAND pg_regress
        --inputdir=${CMAKE_CURRENT_SOURCE_DIR}
        --outputdir=${CMAKE_CURRENT_BINARY_DIR}/expected
        --load-extension=factorial_bg_worker
        --dbname=regression
        --schedule=${CMAKE_SOURCE_DIR}/test_schedule
    )