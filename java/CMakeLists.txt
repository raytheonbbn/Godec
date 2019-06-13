file(GLOB JavaFiles "${PROJECT_SOURCE_DIR}/java/godec/src/main/java/com/bbn/godec/*.java")

find_program(MVN mvn)
if (NOT MVN)
    message(FATAL_ERROR "No Maven executable found. Install maven and add to path")
endif()

if (PLATFORM STREQUAL "Windows")
	set(MVN_CMD cmd /C ${MVN})
else()
	set(MVN_CMD ${MVN})
endif()

add_custom_target(GodecJar ALL DEPENDS ${JavaFiles})
add_custom_command(
	TARGET GodecJar
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/java
	COMMAND ${CMAKE_COMMAND} -E env "MAVEN_OPTS=-Dhttps.protocols=TLSv1,TLSv1.1,TLSv1.2" ${MVN_CMD} clean verify
	)

install(FILES "${PROJECT_SOURCE_DIR}/java/godec/target/godec-1.0-SNAPSHOT.jar" DESTINATION java)
install(FILES "${PROJECT_SOURCE_DIR}/java/pom.xml" DESTINATION java)
install(FILES "${PROJECT_SOURCE_DIR}/java/godec/target/appassembler/repo/com/google/code/gson/gson/2.6.1/gson-2.6.1.jar" DESTINATION java)
