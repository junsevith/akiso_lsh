cmake_minimum_required(VERSION 3.22)
project(lsh C)

set(CMAKE_C_STANDARD 23)

add_executable(lsh main.c
        executor.c
        executor.h
        reader.c
        reader.h
        piper.c
        piper.h)
