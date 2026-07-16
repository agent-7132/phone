# ESP-IDF组件注册配置文件
# 本文件用于定义main组件的源代码文件和包含目录

# 注册ESP-IDF组件
# SRCS: 指定组件的源代码文件列表
# INCLUDE_DIRS: 指定组件的头文件包含目录，本示例不需要额外的头文件目录
idf_component_register(SRCS "board_check_main.c"
                    INCLUDE_DIRS "")

