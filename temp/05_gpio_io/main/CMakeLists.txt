# ESP-IDF组件注册配置文件
# 
# idf_component_register() 是 ESP-IDF CMake 构建系统的核心函数，用于注册组件。
# 
# 参数说明：
#   SRCS "gpio_io_main.c"：指定组件的源文件列表，包含GPIO输入输出示例代码
#   INCLUDE_DIRS ""：指定组件的头文件搜索路径，当前目录为空表示不需要额外的头文件路径
#
# 注意：
#   1. driver/gpio组件已被ESP-IDF自动包含，无需显式声明依赖
#   2. 组件名称默认为目录名（main），这是ESP-IDF的标准约定
idf_component_register(SRCS "gpio_io_main.c"
                    INCLUDE_DIRS "")
