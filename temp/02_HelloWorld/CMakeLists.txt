# ESP-IDF项目主构建配置文件
# 该文件是ESP-IDF项目的入口点，负责初始化CMake构建环境并定义项目名称

# 设置CMake最低版本要求为3.16
# ESP-IDF v5.x版本要求CMake版本不低于3.16
cmake_minimum_required(VERSION 3.16)

# 包含ESP-IDF的项目构建脚本
# $ENV{IDF_PATH}: 环境变量，指向ESP-IDF SDK的安装路径
# project.cmake: ESP-IDF提供的核心构建脚本，负责加载所有必要的CMake模块和工具链配置
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 定义项目名称为"HelloWorld"
# 该名称将作为生成的固件文件和构建目录的基础名称
project(HelloWorld)