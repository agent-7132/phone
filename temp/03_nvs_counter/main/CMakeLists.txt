# 组件注册：将当前目录声明为 ESP-IDF 组件
# 
# idf_component_register() 是 ESP-IDF CMake 构建系统的核心函数，用于注册一个组件。
# 该函数告诉构建系统如何编译和链接当前目录下的源代码。
#
# 参数说明：
#   SRCS "nvs_counter_main.c"：指定组件的源文件列表，这里只有一个主程序文件
#   INCLUDE_DIRS ""：指定组件的头文件搜索路径，当前目录为空表示不需要额外的头文件路径
#
# 注意：
#   1. 此组件不需要显式声明依赖（REQUIRES），因为 nvs_flash、esp_log、freertos 等核心组件
#      已被 ESP-IDF 自动包含（通过 ESP-IDF 的预定义组件依赖）
#   2. 组件名称默认为目录名（main），这是 ESP-IDF 的标准约定
idf_component_register(SRCS "nvs_counter_main.c"
                    INCLUDE_DIRS "")
