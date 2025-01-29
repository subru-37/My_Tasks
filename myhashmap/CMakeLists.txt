cmake_minimum_required(VERSION 3.10) 

project(HashMapProject) # Define the project name


set(CMAKE_C_STANDARD 99) # Specifies the C standard to use.
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_INSTALL_PREFIX /home/subra-pt7817/projects/myhashmap/bin)

add_library(hashmap SHARED src/hashmap.c) # Creates a static library from hashmap.c

add_executable(main main.c) # Adds an executable for testing.

target_link_libraries(main PRIVATE hashmap) # Links the static library to the executable.

target_include_directories(main PRIVATE src) # Ensures the header file is found during compilation.

install(TARGETS hashmap DESTINATION lib)
install(FILES src/hashmap.h DESTINATION include)