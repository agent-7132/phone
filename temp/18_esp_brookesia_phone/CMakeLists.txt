# ESP-Brookesia手机项目主CMakeLists.txt
# 详细文档参考：https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html

# 设置CMake最低版本要求
cmake_minimum_required(VERSION 3.5)

# 包含ESP-IDF项目构建系统
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 设置项目名称为phone_p4_function_ev_board
# 该项目是基于ESP32-P4功能评估板的手机演示应用
project(phone_p4_function_ev_board)
