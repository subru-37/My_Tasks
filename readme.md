# Internship tasks

## Task 1 - Implementation of HashMap in C

### Features:
- Ability to add user-defined hash function
- Ability to add user-defined collision function
- Uses `djb2Hash` as the primary hashing function
- Uses `Open Addressing - Quadratic Probing` to prevent collisions
- Uses a secondary hashing function to prevent secondary clustering

### Setup the project

Note: Clone the project using: `git clone --recurse-submodules https://github.com/subru-37/My_Tasks`

1. Ensure that `CMake` has been installed in your system
2. Run `cmake ..` from the `myhashmap/build` path to configure build parameters
3. Run `cmake --build .` from the `myhashmap/build` path to compile the program with the libraries.
4. Run `./main` from the same directory to run the program.

## Task 2 - Implementation of Split Block Bloom Filter

### Description

The new architecture introduced in Split Block Bloom Filter leverages SIMD instructions (AVX2) from intrinsic functions and uses constant number of hashing function to make the no: of hash functions independent of the False Positive Rate or number of elements the filter can store, which significantly boosts the performance of the filter at the cost of extra space (Normal Bloom Filter has smaller False Positive Rate compared to Split Block Bloom filter with the same max_size)

Ref: (https://arxiv.org/pdf/2101.01719)[https://arxiv.org/pdf/2101.01719]

### Setup the project 

Note: Clone the project using: `git clone --recurse-submodules https://github.com/subru-37/My_Tasks`

1. Ensure that `CMake` has been installed in your system
2. Run `cmake ..` from the `mybloomfilter/build` path to configure build parameters
3. Run `cmake --build .` from the `mybloomfilter/build` path to compile the program with the libraries.
4. Run `./main` from the same directory to run the program.