# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.13

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/oles/code/eventhub/src/test

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/oles/code/eventhub/src/test

# Include any dependencies generated for this target.
include CMakeFiles/websocket_response_test.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/websocket_response_test.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/websocket_response_test.dir/flags.make

CMakeFiles/websocket_response_test.dir/home/oles/code/eventhub/src/websocket_response.cpp.o: CMakeFiles/websocket_response_test.dir/flags.make
CMakeFiles/websocket_response_test.dir/home/oles/code/eventhub/src/websocket_response.cpp.o: /home/oles/code/eventhub/src/websocket_response.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/oles/code/eventhub/src/test/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/websocket_response_test.dir/home/oles/code/eventhub/src/websocket_response.cpp.o"
	/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/websocket_response_test.dir/home/oles/code/eventhub/src/websocket_response.cpp.o -c /home/oles/code/eventhub/src/websocket_response.cpp

CMakeFiles/websocket_response_test.dir/home/oles/code/eventhub/src/websocket_response.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/websocket_response_test.dir/home/oles/code/eventhub/src/websocket_response.cpp.i"
	/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/oles/code/eventhub/src/websocket_response.cpp > CMakeFiles/websocket_response_test.dir/home/oles/code/eventhub/src/websocket_response.cpp.i

CMakeFiles/websocket_response_test.dir/home/oles/code/eventhub/src/websocket_response.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/websocket_response_test.dir/home/oles/code/eventhub/src/websocket_response.cpp.s"
	/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/oles/code/eventhub/src/websocket_response.cpp -o CMakeFiles/websocket_response_test.dir/home/oles/code/eventhub/src/websocket_response.cpp.s

CMakeFiles/websocket_response_test.dir/websocket_response_test.cpp.o: CMakeFiles/websocket_response_test.dir/flags.make
CMakeFiles/websocket_response_test.dir/websocket_response_test.cpp.o: websocket_response_test.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/oles/code/eventhub/src/test/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/websocket_response_test.dir/websocket_response_test.cpp.o"
	/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/websocket_response_test.dir/websocket_response_test.cpp.o -c /home/oles/code/eventhub/src/test/websocket_response_test.cpp

CMakeFiles/websocket_response_test.dir/websocket_response_test.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/websocket_response_test.dir/websocket_response_test.cpp.i"
	/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/oles/code/eventhub/src/test/websocket_response_test.cpp > CMakeFiles/websocket_response_test.dir/websocket_response_test.cpp.i

CMakeFiles/websocket_response_test.dir/websocket_response_test.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/websocket_response_test.dir/websocket_response_test.cpp.s"
	/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/oles/code/eventhub/src/test/websocket_response_test.cpp -o CMakeFiles/websocket_response_test.dir/websocket_response_test.cpp.s

# Object files for target websocket_response_test
websocket_response_test_OBJECTS = \
"CMakeFiles/websocket_response_test.dir/home/oles/code/eventhub/src/websocket_response.cpp.o" \
"CMakeFiles/websocket_response_test.dir/websocket_response_test.cpp.o"

# External object files for target websocket_response_test
websocket_response_test_EXTERNAL_OBJECTS =

websocket_response_test: CMakeFiles/websocket_response_test.dir/home/oles/code/eventhub/src/websocket_response.cpp.o
websocket_response_test: CMakeFiles/websocket_response_test.dir/websocket_response_test.cpp.o
websocket_response_test: CMakeFiles/websocket_response_test.dir/build.make
websocket_response_test: CMakeFiles/websocket_response_test.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/oles/code/eventhub/src/test/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking CXX executable websocket_response_test"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/websocket_response_test.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/websocket_response_test.dir/build: websocket_response_test

.PHONY : CMakeFiles/websocket_response_test.dir/build

CMakeFiles/websocket_response_test.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/websocket_response_test.dir/cmake_clean.cmake
.PHONY : CMakeFiles/websocket_response_test.dir/clean

CMakeFiles/websocket_response_test.dir/depend:
	cd /home/oles/code/eventhub/src/test && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/oles/code/eventhub/src/test /home/oles/code/eventhub/src/test /home/oles/code/eventhub/src/test /home/oles/code/eventhub/src/test /home/oles/code/eventhub/src/test/CMakeFiles/websocket_response_test.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/websocket_response_test.dir/depend

