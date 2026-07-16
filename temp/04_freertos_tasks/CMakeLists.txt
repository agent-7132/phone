# ESP-IDF项目顶层构建文件
# 
# 此文件是ESP-IDF CMake构建系统的入口点，负责：
# 1. 设置CMake最低版本要求
# 2. 引入ESP-IDF的构建工具链
# 3. 定义项目名称
#
# 注意：
#   ESP-IDF项目必须遵循此标准结构，顶层CMakeLists.txt不应包含
#   具体的源文件配置，这些配置应放在各个组件目录的CMakeLists.txt中。

# 设置CMake最低版本要求为3.16
# ESP-IDF 5.x版本要求CMake 3.16或更高版本
cmake_minimum_required(VERSION 3.16)

# 引入ESP-IDF的项目构建模块
# $ENV{IDF_PATH}环境变量指向ESP-IDF的安装路径
# tools/cmake/project.cmake包含了ESP-IDF的核心构建逻辑
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 定义项目名称为"freertos_tasks"
# 项目名称将用于生成最终的固件文件名（如freertos_tasks.bin）
project(freertos_tasks)
