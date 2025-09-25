cmake_minimum_required(VERSION 3.14)

#
# This tells CMake that your project's name is homeexam-01, and that the programs
# here are written in C.
# 
project(HomeExam
	VERSION 1.0
	DESCRIPTION "CMake configuration file for the home exam IN2140 v25"
        LANGUAGES C CXX)

#
# This tells CMake that the compilation should use the version of the C standard
# from 2011.
# This has no effect on Windows.
#
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

#
# This tells CMake to create compilation rules that add debug information. When
# your program is built successfully, you will be able to search for bugs with
# gdb and valgrind. The build type Release would make a program that cannot be
# debugged with these tools.
#
set(CMAKE_BUILD_TYPE Debug)

# The compile flag -pg can be added to compilation and linking if you want to use
# the gprof tool on Linux.
# add_compile_options(-pg)
# add_link_options(-pg)

#
# Include the top source directory in the search path for include files.
#
include_directories(${CMAKE_SOURCE_DIR})

#
# This tells CMake to create rules for making an executable program named homeexam-01
# from the source files tests.c the_apple.c and the_apple.h
#
# add_executable(	ncur ncur.c curses_window.c curses_window.h )

# target_link_libraries( ncur ncurses )

add_executable( maze-client
                maze-client.c
		l4sap.c l4sap.c
		l2sap.c l2sap.h
		maze.c maze.h
		maze-plot.c )

add_executable( transport-test-client
                transport-test-client.c
		l4sap.c l4sap.c
		l2sap.c l2sap.h )

add_executable( datalink-test-client
                datalink-test-client.c
		l2sap.c l2sap.h )

#
# This creates a make rule that helps you create your delivery.
# You call it with "make package_source"
# This will generate the archive file HomeExam01-1.0-Source.zip,
# which you deliver.
#
set(CPACK_SOURCE_GENERATOR "ZIP")
set(CPACK_SOURCE_IGNORE_FILES
	/\.git/
	/build/
	".*Makefile"
	/CMakeFiles/
	".*CMakeCache.txt"
	".*CPack.*"
	".*cmake"
	".*.zip"
	".*~$" )
include(CPack)

