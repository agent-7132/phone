# main组件的CMakeLists.txt
# 负责注册main.cpp源文件并配置编译选项

# 注册main组件，指定源文件和包含目录
idf_component_register(
    SRCS "main.cpp"          # 源文件：main.cpp
    INCLUDE_DIRS "."         # 包含目录：当前目录
)

# 设置编译选项：禁用缺失字段初始化警告
target_compile_options(${COMPONENT_LIB} PUBLIC -Wno-missing-field-initializers)

# 获取组件库的函数
# 通过组件名称查找对应的库目标，支持普通组件名和espressif__前缀格式
function(get_component_library component_name output_var)
    # 获取所有构建组件列表
    idf_build_get_property(build_components BUILD_COMPONENTS)
    set(TARGET_COMPONENT "")

    # 遍历组件列表查找匹配的组件
    foreach(COMPONENT ${build_components})
        if(COMPONENT MATCHES "${component_name}" OR COMPONENT MATCHES "espressif__${component_name}")
            set(TARGET_COMPONENT ${COMPONENT})
            break()
        endif()
    endforeach()

    # 获取组件的库目标
    if(TARGET_COMPONENT STREQUAL "")
        message(FATAL_ERROR "Component '${component_name}' not found.")
    else()
        idf_component_get_property(COMPONENT_LIB ${TARGET_COMPONENT} COMPONENT_LIB)
        set(${output_var} ${COMPONENT_LIB} PARENT_SCOPE)
    endif()
endfunction()

# 获取LVGL组件的库目标并设置编译选项
get_component_library("lvgl" LVGL_LIB)
target_compile_options(
    ${LVGL_LIB}
    PUBLIC
        "-Wno-attributes"    # 禁用属性相关警告
)
