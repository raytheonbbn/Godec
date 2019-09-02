cmake_minimum_required(VERSION 3.7)
project(godec_core)

set(CMAKE_CXX_STANDARD 17)

set(SOURCE_FILES
        ApiEndpoint.cc
        ApiEndpoint.h
        AccumCovariance.cc
        AccumCovariance.h
        AudioPreProcessor.h
        AudioPreProcessor.cc
        Average.cc
        Average.h
        Energy.cc
        Energy.h
        FeatureMerger.cc
        FeatureMerger.h
        FeatureNormalizer.cc
        FeatureNormalizer.h
        FileFeeder.h
        FileFeeder.cc
        cnpy.h
        cnpy.cpp
        resample.h
        resample.cc
        FileWriter.h
        FileWriter.cc
        GodecMessages.cc
        GodecMessages.h
        Java.cc
        Java.h
        PythonComponent.cc
        PythonComponent.h
        MatrixApply.cc
        MatrixApply.h
        Merger.cc
        Merger.h
        NoiseAdd.cc
        NoiseAdd.h
        Router.cc
        Router.h
        SoundcardRecorder.cc
        SoundcardRecorder.h
        SoundcardPlayback.cc
        SoundcardPlayback.h
        SubModule.cc
        SubModule.h
        Subsample.cc
        Subsample.h
        )

add_library(godec_core_static STATIC ${SOURCE_FILES})

if (PLATFORM STREQUAL "Windows")
  set(GODEC_CORE_NAME "libgodec_core")
else()
  set(GODEC_CORE_NAME "godec_core")
endif()

if ((PLATFORM STREQUAL "Linux") OR (PLATFORM STREQUAL "Generic_ARM"))
  set(PLATFORM_SPECIFIC_LIBS ${JAVA_JVM_LIBRARY} -ldl -lrt)
  set(LINKER_GODEC_ARGS -Wl,--whole-archive godec_core_static -Wl,--no-whole-archive)
elseif (PLATFORM STREQUAL "Android")
  set(LINKER_GODEC_ARGS -Wl,--whole-archive godec_core_static -Wl,--no-whole-archive)
elseif (PLATFORM STREQUAL "Windows")
  set(LINKER_GODEC_ARGS godec_core_static)
endif()
if (NOT PLATFORM STREQUAL "Windows")
  list(APPEND LINKER_GODEC_ARGS -Wl,--no-undefined)
endif()

add_library(godec_core SHARED godec_core.cc)
set_target_properties(godec_core PROPERTIES OUTPUT_NAME ${GODEC_CORE_NAME})

link_directories(${JAVA_LINKER_DIR})

target_link_libraries(godec_core
        ${LINKER_GODEC_ARGS} 
        godec_static
        Boost::thread Boost::program_options Boost::regex Boost::system Boost::serialization Boost::filesystem Boost::iostreams Boost::timer Boost::chrono
        ${LINKER_LIBS}
        )

install (TARGETS godec_core DESTINATION bin/${PLATFORM_STRING})
install (TARGETS godec_core_static DESTINATION lib/${PLATFORM_STRING})
