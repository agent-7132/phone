# ESP-IDF组件注册配置文件
# 该文件用于向ESP-IDF构建系统注册当前目录下的源代码文件

# idf_component_register: ESP-IDF提供的CMake函数，用于注册组件
# 参数说明：
#   SRCS: 指定组件的源文件列表，多个文件用空格分隔
#   INCLUDE_DIRS: 指定组件的头文件包含目录，空字符串表示无额外头文件目录
#
# 当前组件（main）仅包含一个源文件 hello_world_main.c，
# 该文件包含ESP-IDF应用程序的入口函数 app_main()
idf_component_register(SRCS "hello_world_main.c"
                    INCLUDE_DIRS "")