SET(CMAKE_JavaC_COMPILER "/usr/bin/gcj" CACHE PATH "JavaC Compiler")
CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/src/libjackcess/CMakeJavaCInformation.cmake
  ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeJavaCCompiler.cmake IMMEDIATE @ONLY)
#MESSAGE(STATUS "OUTPUT")
SET(CMAKE_JavaC_COMPILER_ENV_VAR "JAVA_COMPILER")