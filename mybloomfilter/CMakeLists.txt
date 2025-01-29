cmake_minimum_required(VERSION 3.13)
project(MyBloomFilter)
enable_language(C)
set(CMAKE_INSTALL_PREFIX /home/subra-pt7817/projects/mybloomfilter/bin)

include(CheckCCompilerFlag)

# Check if AVX2 is supported
check_c_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)

if(COMPILER_SUPPORTS_AVX2)
    message(STATUS "Compiler supports AVX2.")
else()
    message(STATUS "Compiler does not support AVX2.")
endif()


set(CMAKE_INSTALL_SOURCE src/bloomfilter src/murmur3 src/libfilter/c/include/filter src/SplitBlockBloomFilter)

add_library(bloomfilter STATIC ./src/bloomfilter/bloomfilter.c)
add_library(murmur3 STATIC ./src/murmur3/murmur3.c) # Creates a static library from hashmap.c
add_subdirectory(src/libfilter)
add_library(sbbf STATIC ./src/SplitBlockBloomFilter/sbbf.c)
add_executable(main main.c)

target_link_libraries(main PRIVATE bloomfilter) # Links the shared library to the executable.
target_link_libraries(main PRIVATE murmur3) # Links the shared library to the executable.
target_link_libraries(main PRIVATE bloomfilter murmur3 m)
target_link_libraries(main PRIVATE libfilter_c)
target_link_libraries(sbbf PRIVATE libfilter_c)


# Detect architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    message(STATUS "Building for x86_64 (Enabling AVX2)")
    # add_compile_options(-mavx2)  # Enable AVX2
    target_compile_options(libfilter_c PRIVATE -mavx2)
    target_compile_options(sbbf PRIVATE -mavx2)
    target_compile_options(main PRIVATE -mavx2)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    message(STATUS "Building for ARM64 (Enabling NEON)")
    # add_compile_options(-mfpu=neon)  # Enable NEON
    target_compile_options(libfilter_c PRIVATE -mfpu=neon)
    target_compile_options(sbbf PRIVATE -mfpu=neon)
    target_compile_options(main PRIVATE -mfpu=neon)
else()
    message(WARNING "Unknown architecture, no SIMD optimizations enabled.")
endif()

target_link_libraries(main PRIVATE sbbf murmur3 libfilter_c)
# target_include_directories(main PRIVATE bin/include) # Ensures the header file is found during compilation.
# if(EXISTS "${CMAKE_INSTALL_PREFIX}/include")
#     target_include_directories(main PRIVATE bin/include/)
#     target_include_directories(main PRIVATE bin/include/filter)
# else()
target_include_directories(main PRIVATE ${CMAKE_INSTALL_SOURCE})
# endif()
# Check for AVX2 support

install(TARGETS murmur3 DESTINATION lib)
install(FILES src/murmur3/murmur3.h DESTINATION include)


install(TARGETS bloomfilter DESTINATION lib)
install(FILES src/bloomfilter/bloomfilter.h DESTINATION include)

install(TARGETS sbbf DESTINATION lib)
install(FILES src/SplitBlockBloomFilter/sbbf.h DESTINATION include)


