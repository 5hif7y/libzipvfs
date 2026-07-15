cmake_minimum_required(VERSION 3.15)
project(libzipvfs LANGUAGES C)

# Define static library target 'zipvfs'
add_library(zipvfs STATIC kuba-zip/src/zip.c src/zipvfs.c kuba-zip/src/zip.h src/zipvfs.h kuba-zip/src/miniz.h)

# Include directories
target_include_directories(zipvfs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/kuba-zip/src)

# Disable CRT warnings on MSVC
if(MSVC)
    target_compile_definitions(zipvfs PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
