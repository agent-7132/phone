# ESP-IDF项目根CMake配置文件
# 本文件定义项目的基本构建配置，必须按固定顺序编写才能正确工作

# 指定CMake最低版本要求为3.16
cmake_minimum_required(VERSION 3.16)

# 引入ESP-IDF的CMake构建系统
# $ENV{IDF_PATH}环境变量指向ESP-IDF框架的安装路径
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 设置项目名称为"board_check"
# 此名称将用于生成最终的固件文件（如board_check.bin）
project(board_check)

