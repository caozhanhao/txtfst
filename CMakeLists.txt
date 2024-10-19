cmake_minimum_required(VERSION 3.28)
project(txtfst)

set(CMAKE_CXX_STANDARD 23)

include_directories(include)

add_executable(txtfst-tokenizer src/tokenizer.cpp)
add_executable(txtfst-build src/build.cpp)
add_executable(txtfst-search src/search.cpp)
