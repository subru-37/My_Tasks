# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.31

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/local/bin/cmake

# The command to remove a file.
RM = /usr/local/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/subra-pt7817/projects/My_Tasks/mybloomfilter

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/subra-pt7817/projects/My_Tasks/mybloomfilter/build

# Include any dependencies generated for this target.
include CMakeFiles/sbbf.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/sbbf.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/sbbf.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/sbbf.dir/flags.make

CMakeFiles/sbbf.dir/codegen:
.PHONY : CMakeFiles/sbbf.dir/codegen

CMakeFiles/sbbf.dir/src/SplitBlockBloomFilter/sbbf.c.o: CMakeFiles/sbbf.dir/flags.make
CMakeFiles/sbbf.dir/src/SplitBlockBloomFilter/sbbf.c.o: /home/subra-pt7817/projects/My_Tasks/mybloomfilter/src/SplitBlockBloomFilter/sbbf.c
CMakeFiles/sbbf.dir/src/SplitBlockBloomFilter/sbbf.c.o: CMakeFiles/sbbf.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/subra-pt7817/projects/My_Tasks/mybloomfilter/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/sbbf.dir/src/SplitBlockBloomFilter/sbbf.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT CMakeFiles/sbbf.dir/src/SplitBlockBloomFilter/sbbf.c.o -MF CMakeFiles/sbbf.dir/src/SplitBlockBloomFilter/sbbf.c.o.d -o CMakeFiles/sbbf.dir/src/SplitBlockBloomFilter/sbbf.c.o -c /home/subra-pt7817/projects/My_Tasks/mybloomfilter/src/SplitBlockBloomFilter/sbbf.c

CMakeFiles/sbbf.dir/src/SplitBlockBloomFilter/sbbf.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing C source to CMakeFiles/sbbf.dir/src/SplitBlockBloomFilter/sbbf.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/subra-pt7817/projects/My_Tasks/mybloomfilter/src/SplitBlockBloomFilter/sbbf.c > CMakeFiles/sbbf.dir/src/SplitBlockBloomFilter/sbbf.c.i

CMakeFiles/sbbf.dir/src/SplitBlockBloomFilter/sbbf.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling C source to assembly CMakeFiles/sbbf.dir/src/SplitBlockBloomFilter/sbbf.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/subra-pt7817/projects/My_Tasks/mybloomfilter/src/SplitBlockBloomFilter/sbbf.c -o CMakeFiles/sbbf.dir/src/SplitBlockBloomFilter/sbbf.c.s

# Object files for target sbbf
sbbf_OBJECTS = \
"CMakeFiles/sbbf.dir/src/SplitBlockBloomFilter/sbbf.c.o"

# External object files for target sbbf
sbbf_EXTERNAL_OBJECTS =

libsbbf.a: CMakeFiles/sbbf.dir/src/SplitBlockBloomFilter/sbbf.c.o
libsbbf.a: CMakeFiles/sbbf.dir/build.make
libsbbf.a: CMakeFiles/sbbf.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=/home/subra-pt7817/projects/My_Tasks/mybloomfilter/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C static library libsbbf.a"
	$(CMAKE_COMMAND) -P CMakeFiles/sbbf.dir/cmake_clean_target.cmake
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/sbbf.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/sbbf.dir/build: libsbbf.a
.PHONY : CMakeFiles/sbbf.dir/build

CMakeFiles/sbbf.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/sbbf.dir/cmake_clean.cmake
.PHONY : CMakeFiles/sbbf.dir/clean

CMakeFiles/sbbf.dir/depend:
	cd /home/subra-pt7817/projects/My_Tasks/mybloomfilter/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/subra-pt7817/projects/My_Tasks/mybloomfilter /home/subra-pt7817/projects/My_Tasks/mybloomfilter /home/subra-pt7817/projects/My_Tasks/mybloomfilter/build /home/subra-pt7817/projects/My_Tasks/mybloomfilter/build /home/subra-pt7817/projects/My_Tasks/mybloomfilter/build/CMakeFiles/sbbf.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : CMakeFiles/sbbf.dir/depend

