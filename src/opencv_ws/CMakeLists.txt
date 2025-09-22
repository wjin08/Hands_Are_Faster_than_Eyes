cmake_minimum_required(VERSION 3.12)
project(client LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)

add_executable(palm_center_show_cuda_clinet palm_center_show_cuda_clinet.cpp)
target_link_libraries(palm_center_show_cuda_clinet PRIVATE ${OpenCV_LIBS} Threads::Threads)
target_compile_options(palm_center_show_cuda_clinet PRIVATE -O2 -Wall -Wextra)

