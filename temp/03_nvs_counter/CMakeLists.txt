# 项目顶层构建文件：ESP-IDF 项目的入口点
# 
# 此文件是 ESP-IDF CMake 构建系统的顶层配置文件，负责：
# 1. 设置 CMake 最低版本要求
# 2. 引入 ESP-IDF 的 CMake 构建工具链
# 3. 定义项目名称
#
# 注意：
#   ESP-IDF 项目必须遵循此标准结构，顶层 CMakeLists.txt 不应包含
#   具体的源文件配置，这些配置应放在各个组件目录的 CMakeLists.txt 中。

# 设置 CMake 最低版本要求为 3.16
# ESP-IDF 5.x 版本要求 CMake 3.16 或更高版本
cmake_minimum_required(VERSION 3.16)

# 引入 ESP-IDF 的项目构建模块
# $ENV{IDF_PATH} 环境变量指向 ESP-IDF 的安装路径
# tools/cmake/project.cmake 包含了 ESP-IDF 的核心构建逻辑
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 定义项目名称为 "nvs_counter"
# 项目名称将用于生成最终的固件文件名（如 nvs_counter.bin）
project(nvs_counter)
