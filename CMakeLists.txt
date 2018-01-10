cmake_minimum_required(VERSION 3.1)
project(glimp)

set(CMAKE_CXX_STANDARD 14)

file(GLOB_RECURSE CPP_FILES src/*.cpp)
file(GLOB_RECURSE HPP_FILES src/*.h src/*.hpp)

list(APPEND INCLUDE_DIRS src)


if (WIN32)	 
    add_definitions(-D_SCL_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS -DSTRSAFE_NO_DEPRECATE -DNOMINMAX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
	list(APPEND CPP_FILES src/opengl/windows.cpp)
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-type -Werror") 
	list(APPEND CPP_FILES src/opengl/linux.cpp)
endif ()

if (WITH_TIFF)
    add_definitions(-D_XKEYCHECK_H -DWITH_TIFF)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "BUILD_SHARED_LIBS" FORCE)
    add_subdirectory(libtiff)
    list(APPEND LIBS tiff)
endif (WITH_TIFF)

# Android JNI libs
if (ANDROID)
    find_library(LLOG NAMES log PATHS $ENV{SYSROOT}/usr/lib)
    list(APPEND LIBS ${LLOG})
endif ()

set(GL_LIBS)
set(GL_SOURCES)


	
if (ANDROID)		
	list(APPEND LIBS -landroid -lEGL -lGLESv3)
else ()	
	# Download GLES emu https://developer.arm.com/products/software-development-tools/graphics-development-tools/opengl-es-emulator/downloads
	find_library(GLESv2_LIB NAMES GLESv2 PATHS ../gles-emu ../../gles-emu ../../../gles-emu NO_DEFAULT_PATH)
	find_library(EGL_EMU_LIB NAMES EGL PATHS ../gles-emu ../../gles-emu ../../../gles-emu NO_DEFAULT_PATH)
	list(APPEND LIBS ${GLESv2_LIB} ${EGL_EMU_LIB})
	list(APPEND INCLUDE_DIRS ../gles-emu/include ../../gles-emu/include ../../../gles-emu/include )
endif ()

if (NOT WIN32 AND NOT ANDROID)
    # linux X stuff for opengl
    list(APPEND LIBS -ldl -lpthread -lz -g)
endif ()


list(APPEND INCLUDE_DIRS ${ANDROID_NDK}/sources/third_party/googletest/googletest/include)


set(SOURCES ${CPP_FILES} ${HPP_FILES})
include_directories(${INCLUDE_DIRS})



add_library(glimp SHARED ${SOURCES})
target_link_libraries(glimp ${LIBS})



if (ANDROID)
    set_target_properties(aligna PROPERTIES LINK_FLAGS "-Wl,-undefined,dynamic_lookup")
endif ()


if (${CMAKE_CURRENT_BINARY_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    message(FATAL_ERROR "Should not build in source tree")
endif ()


#-------------------
# Tests
#-------------------
set(WITH_TESTS ON)
if (WITH_TESTS)
    file(GLOB_RECURSE TEST_SRC_FILES ${PROJECT_SOURCE_DIR}/test/*_test.cpp)
    file(GLOB SHOWTEST_SRC_FILES ${PROJECT_SOURCE_DIR}/test/*_showtest.cpp) # TODO

    set(SOURCES_TEST ${SOURCES})

    list(REMOVE_ITEM SOURCES_TEST ${PROJECT_SOURCE_DIR}/src/main.cpp ${PROJECT_SOURCE_DIR}/src/main.c)

    if (ANDROID)
        set(GOOGLETEST_ROOT ${ANDROID_NDK}/sources/third_party/googletest/googletest)
        add_library(gtest STATIC ${GOOGLETEST_ROOT}/src/gtest_main.cc ${GOOGLETEST_ROOT}/src/gtest-all.cc)
        target_include_directories(gtest PRIVATE ${GOOGLETEST_ROOT})
        target_include_directories(gtest PUBLIC ${GOOGLETEST_ROOT}/include)

        #add_executable(footest src/main/jni/foo_unittest.cc)
        #target_link_libraries(footest gtest)
    else ()
        add_subdirectory(ext/gtest)
        enable_testing()
    endif ()


    include_directories(${PROJECT_SOURCE_DIR}/test/build/ext/gtest/src/googletest/googletest/include)

    add_executable(aligna_test ${TEST_SRC_FILES} ${SOURCES_TEST})
    add_executable(aligna_showtest ${SHOWTEST_SRC_FILES} ${SOURCES_TEST})

    if (NOT ANDROID)
        add_dependencies(aligna_test googletest)
        add_dependencies(aligna_showtest googletest)
        find_package(Threads REQUIRED)
    endif ()


    target_include_directories(aligna_benchmark PUBLIC $<BUILD_INTERFACE:src ${GTEST_INCLUDE_DIRS}>)
     target_include_directories(aligna_test PUBLIC $<BUILD_INTERFACE:src ${GTEST_INCLUDE_DIRS}>)
    target_include_directories(aligna_showtest PUBLIC $<BUILD_INTERFACE:src ${GTEST_INCLUDE_DIRS}>)

    if (ANDROID)
        target_link_libraries(aligna_test gtest)
    elseif (NOT WIN32 OR MINGW)
        target_link_libraries(aligna_test           ${GTEST_LIBS_DIR}/libgtest.a               ${GTEST_LIBS_DIR}/libgtest_main.a            )
    else ()
        target_link_libraries(aligna_test
                debug ${GTEST_LIBS_DIR}/DebugLibs/${CMAKE_FIND_LIBRARY_PREFIXES}gtestd${CMAKE_FIND_LIBRARY_SUFFIXES}
                optimized ${GTEST_LIBS_DIR}/ReleaseLibs/${CMAKE_FIND_LIBRARY_PREFIXES}gtest${CMAKE_FIND_LIBRARY_SUFFIXES}
                )
        target_link_libraries(aligna_test
                debug ${GTEST_LIBS_DIR}/DebugLibs/${CMAKE_FIND_LIBRARY_PREFIXES}gtest_maind${CMAKE_FIND_LIBRARY_SUFFIXES}
                optimized ${GTEST_LIBS_DIR}/ReleaseLibs/${CMAKE_FIND_LIBRARY_PREFIXES}gtest_main${CMAKE_FIND_LIBRARY_SUFFIXES}
                )
    endif ()

    target_link_libraries(glimp_test ${CMAKE_THREAD_LIBS_INIT} ${LIBS})
    add_test(test1 glimp_test)
endif ()


set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT aligna)
