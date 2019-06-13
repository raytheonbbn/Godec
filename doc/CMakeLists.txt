cmake_minimum_required(VERSION 3.7)
project(godec_doc)

message(STATUS ${CMAKE_CURRENT_SOURCE_DIR})

execute_process(COMMAND perl "GenDoc.pl" "../src/core_components" "../src/core_components,../src" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_FILE "CoreComponents.md")


