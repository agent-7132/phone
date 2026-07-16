# ============================================================
# 主组件 CMakeLists.txt
# ============================================================
# 本文件定义了项目的主组件（main component）配置，包括源文件和依赖。

# 定义源文件列表
# 主组件包含的C源文件，会被编译链接到最终的应用程序中
set(srcs "main.c" "app_video.c")

# 注册ESP-IDF组件
# SRCS: 指定源文件列表
# PRIV_INCLUDE_DIRS: 指定私有头文件目录（当前目录）
idf_component_register(SRCS "${srcs}"
                       PRIV_INCLUDE_DIRS .)
