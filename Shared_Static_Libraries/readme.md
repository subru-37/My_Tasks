# Libraries

## Shared Libraries 

The object files of the libraries are linked via references and the symboles are resolved only during program runtime. After the linking process, the linker leaves address location/reference of the library and is only resolved when the function is needed. 

```bash
gcc -shared -fPIC -o libhashmap.so hashmap.c 
```

_-shared_ flag instructs the compiler that we are building a shared library. _-fPIC_ is to generate **position independendent code**. **PIC** is just a way to generating the library such that it can be loaded to any location in the memory for it be used. The function has to be named such that it starts with the keyword _lib*.so_. In Linux systems, the shared library format has a _.so_ extension while windows systems have _.dll_ files. Essentially, both are binary files. The compiler has to know which location the _lib*.so_ files are located. Otherwise we have to explicitly mention it. 

```bash
gcc main.c -L /home/dircectory_of_library_object_file/ -lhashmap -o sample 
```

If the above command doesn't work, in Linux systems, follow the below steps. 

```bash
sudo cp /path/to/*.so /usr/local/lib && sudo ldconfig && gcc -o main main.c -lhashmap
```

The above command will explicitly tell the system's dynamic linker about the locations of different shared/static libraries and updates the cache that stores the information about libraries and their locations. The 3rd part handles the compilation of the `main.c` file with the `hashmap` library contents attached as just reference. 


The option -L is a hint to the compiler to look in the directory followed by the option for any shared libraries (during link-time only). 

If you invoke the executable, the dynamic linker will not be able to find the required shared library. By default, it won’t look into the current working directory. You have to explicitly instruct the tool chain to provide proper paths. The dynamic linker searches standard paths available in the LD_LIBRARY_PATH(`/usr/local/lib`) and also searches in the system cache(`ldconfig`). 

## Static libraries

The contents of the object files of the libraries are pasted to the main executable by the linker during the linking process. After the compilation process, we do not need the `lib*.a` files to run the executable files (unlike shared libraries).

1. Compile your library file (hashmap.c) into an object file (.o):
   
    ```bash
    gcc -c hashmap.c -o hashmap.o
    ```
    `-c` indicates the gcc to compile without linking to generate the object file of the library
2. Create the Static Library

    ```bash
    ar rcs libhashmap.a hashmap.o
    ```
    `ar` archiver tool to generate static libraries
    `rcs` flags indicate the following
    - `r`: Insert the object files into the library.
    - `c`: Create the library if it doesn't exist.
    - `s`: Create an index (or symbol table) for the library to optimize linking.

3. Compile the Program with the Static Library

    ```bash
    gcc -o main main.c -L. -lhashmap
    ```
    You can also avoid the `-L.` flag by pasting the `lib*.a` file in `/usr/local/lib` file and updating the linker directory information list / cache updation by running `sudo ldconfig`. 

## CMake


It is a cross-platform build tool which abstracts the process of compiling and running the program. It facilities easy integration of external libraries and tracks dependencies effortlessly. 

How CMake Works (Basic Workflow)

1. Write a CMakeLists.txt file
   Define your project's source files, include directories, libraries, and build options.
2. Run cmake to generate build files
    ```bash
    cmake -B build
    ```
3. Compile the project: Once build files are generated, use the appropriate build tool (e.g., make or ninja):

    ```bash
    cmake --build build
    ```

4. Run the compiled program: Executable files are usually found in the build directory.


## Defining a `CMakeLists.txt` file

### CMake version

```cmake
cmake_minimum_required(VERSION 3.30) # can also write VERSION 3.10...3.20 for versioning
```

### Project Name

```cmake
project(HashMapProject) # Define the project name
```

We can make different updates by defining project attributes

```cmake
 project(MyLibrary VERSION 2.1.3 
    DESCRIPTION "A high-performance hashmap library" 
    HOMEPAGE_URL "https://github.com/mylibrary" 
    LANGUAGES C CXX)
```

### Set the C standard to use

In CMake, the set() command is used to define and assign values to variables. The variables set using set() can control various aspects of the build process, such as compiler options, build directories, and project configurations. It's syntax is followed by: 
```cmake
set(<variable_name> <value> [CACHE <type> <docstring>] [PARENT_SCOPE])
```

- **variable_name**: The name of the variable you want to define.
- **value**: The value to assign to the variable.
- **CACHE**: Stores the variable persistently across multiple runs of CMake.
- **PARENT_SCOPE**: Propagates the variable to the parent scope (used inside functions or subdirectories).

Example: 
```cmake
set(CMAKE_C_STANDARD 99) # Specifies the C standard to use.
set(CMAKE_C_STANDARD_REQUIRED True)
```

To reference those variables later, we use

```cmake
add_executable(${PROJECT_NAME} ${SOURCE_FILES})
```

### Adding libraries

We can shared or static libraries to be linked with the executable at later stages by: 

```cmake
add_library(<name> <type> <source1> <source2> ...)
```

- The above command will create a file called `lib<name>.so` (or `lib<name>.a` for static libraries) (or `lib<name>.dll` for shared libraries in windows). 
- The type can be **shared** or **static**. 

### Adding Executables (optional)

It is used to create an executable target which can be used to link and run multiple `*.c` files simulateneously. 

```cmake
add_executable(<name> <source1> <source2> ...)
```

- **name**: The name of the executable to be created.
- **source1**, **source2** ...: List of source files that will be compiled and linked to produce the final executable.

### Link libraries and include headers to the executable

1. To link libraries: 
    ```cmake
    target_link_libraries(<target> <visibility> <libraries>)
    ```

    - **target**: The executable or library target that should be linked(e.g., main).
    - **visibility**: Specifies how the linked libraries are handled:

        - PRIVATE: The linked libraries are only used by the target and not exposed to dependents.
        - PUBLIC: The linked libraries are used by the target and exposed to dependents.
        - INTERFACE: The linked libraries are not used by the target but are required by dependents.

    - **libraries**: The libraries to be linked (e.g., hashmap).

    Example: 

    ```cmake
    target_link_libraries(main PRIVATE hashmap) # Links the library to the executable
    ```
2. To include header files during compilation: 

```cmake
target_include_directories(<target> <visibility> <directories>)
```
- This command tells CMake where to find header files when compiling main.c
- If main.c includes #include "hashmap.h", and hashmap.h is inside the src/ directory, this command ensures the compiler knows where to look.
- It is essential if header files are located in custom directories rather than standard paths.

### Note: 

```
Do we need both commands?

Yes, both commands are needed in most cases. Here's why:

    target_link_libraries(main PRIVATE hashmap)
        Required to link the compiled object code from hashmap.c to main.
        Without it, linking will fail due to unresolved function references.

    target_include_directories(main PRIVATE src)
        Required to provide access to hashmap.h during compilation.
        Without it, the compiler may throw an error like fatal error: hashmap.h: No such file or directory.

If the hashmap.h file is located in the same directory as main.c, you might not need the target_include_directories command, since CMake will find the headers in the same directory by default. However, for better organization and separation of concerns, it's best practice to keep source files and headers in different directories, making this command essential.
```

### Installing a library in your system

```cmake
install(TARGETS hashmap DESTINATION lib)
```
Syntax:
```cmake
install(TARGETS <target> [<target>...]
        [EXPORT <export-name>]
        [RUNTIME DESTINATION <dir>]
        [LIBRARY DESTINATION <dir>]
        [ARCHIVE DESTINATION <dir>]
        [PERMISSIONS <permissions>]
        [COMPONENT <component-name>])
```
- `TARGETS`: Specifies that the following targets are to be installed. In this case, it installs the hashmap target.
- `<target>`: The target to install (e.g., hashmap is the library created in your CMakeLists.txt).
- `DESTINATION`: Specifies the directory within the installation path where the target should be copied. In this case, lib is the destination directory for libraries.

Explanation:

- This command installs the compiled hashmap library into the lib directory (typically for libraries).
- When you run the make install command, it copies the compiled hashmap library (.a or .so) to the lib folder of the installation directory (e.g., /usr/local lib).
- This is useful when you want to distribute or deploy your library.


### Adding headerfiles in your system
```cmake
install(FILES src/hashmap.h DESTINATION include)
```
Syntax:
```cmake
install(FILES <file> [<file>...]
        DESTINATION <dir>
        [PERMISSIONS <permissions>]
        [COMPONENT <component-name>])
```
- `FILES`: Specifies the files to be installed (e.g., hashmap.h).
- `<file>`: The source file or files that you want to install (e.g., src/hashmap.h).
- `DESTINATION`: Specifies the directory within the installation path where the file(s) should be copied. In this case, include is the destination directory for header files.

Explanation:

- This command installs the header file hashmap.h into the include directory (typically for header files).
- When you run the make install command, it copies the hashmap.h file to the include directory of the installation directory (e.g., /usr/local/include).
- This is useful when you want to distribute or deploy header files that need to be included in other projects.


