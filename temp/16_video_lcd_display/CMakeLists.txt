# ============================================================
# ESP-IDF 项目根目录 CMakeLists.txt
# ============================================================
# 本文件是ESP-IDF项目的主构建配置文件，定义了项目的基本构建参数。

# CMake最低版本要求
cmake_minimum_required(VERSION 3.16)

# 添加编译选项：禁用"maybe-uninitialized"警告
# 某些情况下编译器会误报未初始化变量警告，使用此选项抑制
add_compile_options(-Wno-maybe-uninitialized)

# 包含ESP-IDF的项目构建模块
# 此文件提供了ESP-IDF项目的标准构建流程和工具链配置
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 定义项目名称
# 项目名称会影响生成的二进制文件名称和构建目录结构
project(video_lcd_display)
