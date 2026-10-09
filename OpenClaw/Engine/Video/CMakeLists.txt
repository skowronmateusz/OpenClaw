cmake_minimum_required(VERSION 3.2)

target_sources(openclaw
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/MovieArchive.h
    ${CMAKE_CURRENT_SOURCE_DIR}/MovieArchive.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SmackerDecoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SmackerDecoder.cpp
)
