cmake_minimum_required(VERSION 3.7)

if (NOT DEFINED PLATFORM)
  message(FATAL_ERROR "Please specify the PLATFORM variable <Linux, Android, Generic_ARM, Windows")
endif()

if (PLATFORM STREQUAL "Windows")
  set(CMAKE_GENERATOR_PLATFORM x64)
endif()

if (PLATFORM STREQUAL "Android")
  # This is some annoying acrobatic for the Android build. Issue is, when we specify Boost::thread further down, a -lpthread gets appended to the linker args. The NDK doesn't have an explicit pthread library, so this fails. This create a shell script that removes that specific parameter
  set(CXX_NO_PTHREAD "${CMAKE_CURRENT_BINARY_DIR}/android_cxx_no_pthread.sh") 
  file(WRITE "${CXX_NO_PTHREAD}" "#!/bin/bash\nfor var in \"$@\"\ndo\nif [ \"\$var\" == \"-lpthread\" ]; then\ncontinue\nfi\nNEW_ARGS=\"\$NEW_ARGS \$var\"\ndone\n$ENV{CXX} \$NEW_ARGS")
  execute_process(COMMAND "chmod" "755" "${CXX_NO_PTHREAD}")
  set(CMAKE_CXX_COMPILER "${CXX_NO_PTHREAD}")
endif()

project(godec)

if (NOT DEFINED CMAKE_BUILD_TYPE) 
 set(CMAKE_BUILD_TYPE Release) 
endif() 

set(GCC_MINIMAL_VERSION 4.9.0)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS ${GCC_MINIMAL_VERSION})
    message(FATAL_ERROR "Insufficient gcc version: ${CMAKE_CXX_COMPILER_VERSION}. Version ${GCC_MINIMAL_VERSION} or above is required.")
  endif()
endif()

if (NOT PLATFORM STREQUAL "Windows")
  add_compile_options(-Wall -Wno-sign-compare -Wno-unused-local-typedefs -Winit-self )
endif()

if ((PLATFORM STREQUAL "Linux") OR (PLATFORM STREQUAL "Generic_ARM"))
  add_compile_options(-pthread)
endif()
set(CMAKE_CXX_FLAGS_DEBUG "-g -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

find_program(CCACHE_PROGRAM ccache)
if(DEFINED ENV{CCACHE_DIR} AND CCACHE_PROGRAM)
  message(STATUS "CCACHE_DIR environment variable set to '$ENV{CCACHE_DIR}', will use ${CCACHE_PROGRAM}")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# The reason why we use static libs for Windows is because I have yet to discover the magic b2 command line args that produce the right combination of libraries for shared linkine in Boost 1.68 on Windows. Static linking works.
if (PLATFORM STREQUAL "Windows")
  set(Boost_USE_STATIC_LIBS ON)
else()
  set(Boost_USE_STATIC_LIBS OFF)
endif()
set(Boost_USE_STATIC_RUNTIME OFF)
set(Boost_USE_MULTITHREADED ON)
#set(Boost_DEBUG ON)

find_package(Boost
  1.55.0      # Minimum or EXACT version e.g. 1.36.0
  REQUIRED             # Fail with error if Boost is not found
  COMPONENTS date_time atomic regex system serialization filesystem iostreams timer chrono program_options thread 
  )

find_package(PkgConfig)
if(DEFINED Eigen3_INCLUDE_DIRS)
else()
  pkg_search_module(Eigen3 REQUIRED eigen3)
endif()
if (NOT PLATFORM STREQUAL "Android")
  if ((NOT ALSA_INCLUDE_DIRS) AND (NOT PLATFORM STREQUAL "Windows"))
    find_package(ALSA REQUIRED)
  endif()
  find_package(PythonLibs 3.3 REQUIRED)
  find_package(Java REQUIRED)
  find_package(Java COMPONENTS REQUIRED Runtime)
  find_package(Java COMPONENTS REQUIRED Development)
  find_package(JNI REQUIRED)
  find_package(ZLIB REQUIRED)
endif()

if (PLATFORM STREQUAL "Android")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast -DANDROID -marm -march=armv7-a -mfpu=neon -Wno-narrowing -fexceptions -frtti -Wno-deprecated-register -Wno-redeclared-class-member")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lOpenSLES")
elseif(PLATFORM STREQUAL "Generic_ARM")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast -mcpu=cortex-a53 -rdynamic -mfpu=neon-fp-armv8 -mneon-for-64bits -mhard-float")
elseif(PLATFORM STREQUAL "Linux")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-ignored-attributes -Wno-int-in-bool-context -msse -msse2")
elseif(PLATFORM STREQUAL "Windows")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17 /MD")
endif()

add_definitions(
  -DEIGEN_NO_DEBUG
  -DNDEBUG
  -O2 
)
if (NOT PLATFORM STREQUAL "Windows")
  add_definitions(
    -DLINUX_MACHINE=1
  )
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-undefined")
else()
  add_definitions(
    -D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS 
)
endif()

include_directories(
  ${PROJECT_SOURCE_DIR}/src/include
  ${Boost_INCLUDE_DIR}
  ${Eigen3_INCLUDE_DIRS}
  ${JNI_INCLUDE_DIRS}
  ${PYTHON_INCLUDE_DIRS}
  ${ALSA_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIRS}
  ${GODEC_ADDITIONAL_INCLUDE_DIRS}
  )

file(WRITE "${PROJECT_SOURCE_DIR}/src/include/godec/version.h" "#pragma once\n")
# Generate version SHA include file
execute_process(COMMAND git rev-parse --short HEAD OUTPUT_VARIABLE SHA)
string(REGEX REPLACE "[\ \n]+" "" SHA "${SHA}")
file(APPEND "${PROJECT_SOURCE_DIR}/src/include/godec/version.h" "#define GODEC_VERSION_STRING \"${SHA}\"\n")

# Get Eigen version
execute_process(COMMAND bash "-c" "grep EIGEN_WORLD_VERSION ${Eigen3_INCLUDE_DIRS}/Eigen/src/Core/util/Macros.h | head -1 | awk '{print $3;}'" OUTPUT_VARIABLE EIGEN_WORLD_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
file(APPEND "${PROJECT_SOURCE_DIR}/src/include/godec/version.h" "#define GODEC_EIGEN_WORLD_VERSION ${EIGEN_WORLD_VERSION}\n")
execute_process(COMMAND bash "-c" "grep EIGEN_MAJOR_VERSION ${Eigen3_INCLUDE_DIRS}/Eigen/src/Core/util/Macros.h | head -1 | awk '{print $3;}'" OUTPUT_VARIABLE EIGEN_MAJOR_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
file(APPEND "${PROJECT_SOURCE_DIR}/src/include/godec/version.h" "#define GODEC_EIGEN_MAJOR_VERSION ${EIGEN_MAJOR_VERSION}\n")

set(SOURCE_FILES
  src/include/godec/channel.h
  src/ChannelMessenger.cc
  src/include/godec/ChannelMessenger.h
  src/ComponentGraph.cc
  src/include/godec/ComponentGraph.h
  src/HelperFuncs.cc
  src/include/godec/HelperFuncs.h
  src/TimeStream.cc
  src/include/godec/TimeStream.h
  )

link_directories(${JAVA_LINKER_DIR})
link_directories(${ALSA_LINKER_DIR})
if (PYTHON_LIBRARIES)
  get_filename_component(PYTHON_LIBRARY_DIR ${PYTHON_LIBRARIES} DIRECTORY)
endif()
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_RPATH};${Boost_LIBRARY_DIRS};${PYTHON_LIBRARY_DIR}")

if (PLATFORM STREQUAL "Android")
  set(LINKER_LIBS "-llog -lz -lOpenSLES")
elseif(PLATFORM STREQUAL "Windows")
  set(LINKER_LIBS ${JAVA_JVM_LIBRARY} ${PYTHON_LIBRARIES} ${ZLIB_LIBRARY} winmm.lib)
else()
  set(LINKER_LIBS ${JAVA_JVM_LIBRARY} ${PYTHON_LIBRARIES} ${ZLIB_LIBRARY} -ldl -lrt -lasound)
endif()

add_subdirectory(src/core_components)
add_subdirectory(doc)
add_subdirectory(java)

add_library(godec_dynamic SHARED src/godec.cpp ${SOURCE_FILES})

target_link_libraries(godec_dynamic 
        godec_core_static
        Boost::program_options Boost::regex Boost::system Boost::serialization Boost::filesystem Boost::iostreams Boost::timer Boost::chrono Boost::thread
	${LINKER_LIBS}
        )

if (PLATFORM STREQUAL "Windows")
  set(GODEC_DYNAMIC_NAME "libgodec")
else()
  set(GODEC_DYNAMIC_NAME "godec")
endif()
set_target_properties(godec_dynamic PROPERTIES OUTPUT_NAME ${GODEC_DYNAMIC_NAME})

add_library(godec_static STATIC ${SOURCE_FILES})

add_executable(godec src/godec.cpp)
target_link_libraries(godec
        godec_static
        godec_core_static
        Boost::program_options Boost::regex Boost::system Boost::serialization Boost::filesystem Boost::iostreams Boost::timer Boost::chrono Boost::thread
	${LINKER_LIBS}
        )

install (TARGETS godec DESTINATION bin)
install (TARGETS godec_dynamic DESTINATION bin)
install (TARGETS godec_static DESTINATION lib)
install (DIRECTORY "${PROJECT_SOURCE_DIR}/src/include" DESTINATION .)
install (FILES "${PROJECT_SOURCE_DIR}/src/core_components/GodecMessages.h" DESTINATION include/godec/core_components)
install (FILES "${PROJECT_SOURCE_DIR}/src/core_components/AccumCovariance.h" DESTINATION include/godec/core_components)
install (FILES "${PROJECT_SOURCE_DIR}/src/core_components/cnpy.h" DESTINATION include/godec/core_components)
install (FILES "${PROJECT_SOURCE_DIR}/tools/env.sh" DESTINATION .)
install (DIRECTORY ${PROJECT_SOURCE_DIR}/doc/ DESTINATION doc/ PATTERN "CMakeLists.txt" EXCLUDE)
install (FILES ${PROJECT_SOURCE_DIR}/README.md DESTINATION .)

enable_testing()

if (PLATFORM STREQUAL "Windows")
        set(TEST_CMD cmd /C ${MVN})
else()
        set(TEST_CMD ${MVN})
        file(MAKE_DIRECTORY ${PROJECT_SOURCE_DIR}/.git/hooks) 
	ADD_CUSTOM_TARGET(link_astyle ALL COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_SOURCE_DIR}/misc/pre-commit ${PROJECT_SOURCE_DIR}/.git/hooks/pre-commit)
endif()
add_test(NAME regression COMMAND ./run_tests.sh ${CMAKE_INSTALL_PREFIX} WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/test)
