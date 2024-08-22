cmake_minimum_required(VERSION 3.0)
project(mymuduo)

# cmake => makefile   make
# mymuduo最终编译成so动态库，这是设置动态库路径LIBRARY_OUTPUT_PATH的代码，放在根目录（PROJECT_SOURCE_DIR）的lib文件夹下面 libmymuduo.so
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# 设置调试信息 以及启动C++11语言标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -fPIC")

# 定义参与编译的源代码文件
aux_source_directory(. SRC_LIST) # .代表当前目录全部文件  SRC_LIST是文件名

# 编译生成动态库mymuduo
add_library(mymuduo SHARED ${SRC_LIST})