cmake_minimum_required(VERSION 3.20)
project(stepper_synth C CXX ASM)

add_library(synth_main main.cpp synth.cpp common.cpp)
target_include_directories(synth_main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})