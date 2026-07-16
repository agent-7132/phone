# brookesia_core组件的CMakeLists.txt
# 负责配置和注册brookesia_core模块的所有子组件

# 设置源文件目录和编译变量
set(PROJ_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})    # 当前组件目录
set(SRCS_C "")                                   # C源文件列表
set(SRCS_CPP "")                                 # C++源文件列表
set(SRCS_COMPILE_OPTIONS "")                     # 编译选项列表
set(INCLUDE_DIRS ${PROJ_SRC_DIR})                # 包含目录列表

# ==================== AI Framework模块 ====================
# 配置AI框架相关组件，包括Agent聊天代理和Expression表情控制
if(CONFIG_ESP_BROOKESIA_ENABLE_AI_FRAMEWORK)
    set(AI_FRAMEWORK_SRC_DIR ${PROJ_SRC_DIR}/ai_framework)
    list(APPEND INCLUDE_DIRS ${AI_FRAMEWORK_SRC_DIR})

    # Agent子模块：聊天会话生命周期管理
    if(CONFIG_ESP_BROOKESIA_AI_FRAMEWORK_ENABLE_AGENT)
        set(AI_FRAMEWORK_AGENT_SRC_DIR ${AI_FRAMEWORK_SRC_DIR}/agent)
        file(GLOB_RECURSE AI_FRAMEWORK_AGENT_SRCS_C ${AI_FRAMEWORK_AGENT_SRC_DIR}/*.c)
        file(GLOB_RECURSE AI_FRAMEWORK_AGENT_SRCS_CPP ${AI_FRAMEWORK_AGENT_SRC_DIR}/*.cpp)
        list(APPEND SRCS_C ${AI_FRAMEWORK_AGENT_SRCS_C})
        list(APPEND SRCS_CPP ${AI_FRAMEWORK_AGENT_SRCS_CPP})
    endif()

    # Expression子模块：表情动画控制
    if(CONFIG_ESP_BROOKESIA_AI_FRAMEWORK_ENABLE_EXPRESSION)
        set(AI_FRAMEWORK_EXPRESSION_SRC_DIR ${AI_FRAMEWORK_SRC_DIR}/expression)
        file(GLOB_RECURSE AI_FRAMEWORK_EXPRESSION_SRCS_C ${AI_FRAMEWORK_EXPRESSION_SRC_DIR}/*.c)
        file(GLOB_RECURSE AI_FRAMEWORK_EXPRESSION_SRCS_CPP ${AI_FRAMEWORK_EXPRESSION_SRC_DIR}/*.cpp)
        list(APPEND SRCS_C ${AI_FRAMEWORK_EXPRESSION_SRCS_C})
        list(APPEND SRCS_CPP ${AI_FRAMEWORK_EXPRESSION_SRCS_CPP})
    endif()
endif()

# ==================== GUI模块 ====================
# 配置图形用户界面相关组件，包括动画播放器、Squareline UI、LVGL封装和样式系统
if(CONFIG_ESP_BROOKESIA_ENABLE_GUI)
    set(GUI_SRC_DIR ${PROJ_SRC_DIR}/gui)
    file(GLOB GUI_SRCS_CPP ${GUI_SRC_DIR}/*.cpp)
    list(APPEND SRCS_CPP ${GUI_SRCS_CPP})
    list(APPEND INCLUDE_DIRS ${GUI_SRC_DIR})

    # AnimPlayer子模块：动画播放器，支持表情和图标动画
    if(CONFIG_ESP_BROOKESIA_GUI_ENABLE_ANIM_PLAYER)
        set(GUI_ANIM_PLAYER_SRC_DIR ${GUI_SRC_DIR}/anim_player)
        file(GLOB_RECURSE GUI_ANIM_PLAYER_SRCS_C ${GUI_ANIM_PLAYER_SRC_DIR}/*.c)
        file(GLOB_RECURSE GUI_ANIM_PLAYER_SRCS_CPP ${GUI_ANIM_PLAYER_SRC_DIR}/*.cpp)
        list(APPEND SRCS_C ${GUI_ANIM_PLAYER_SRCS_C})
        list(APPEND SRCS_CPP ${GUI_ANIM_PLAYER_SRCS_CPP})
    endif()

    # Squareline子模块：Squareline UI设计工具生成的代码
    if(CONFIG_ESP_BROOKESIA_GUI_ENABLE_SQUARELINE)
        set(GUI_SQUARELINE_SRC_DIR ${GUI_SRC_DIR}/squareline)

        # UI Components：UI组件库
        if(CONFIG_ESP_BROOKESIA_SQUARELINE_ENABLE_UI_COMP)
            set(GUI_SQUARELINE_UI_COMP_SRC_DIR ${GUI_SQUARELINE_SRC_DIR}/ui_comp)
            file(GLOB_RECURSE GUI_SQUARELINE_UI_COMP_SRCS_C ${GUI_SQUARELINE_UI_COMP_SRC_DIR}/*.c)
            list(APPEND SRCS_C ${GUI_SQUARELINE_UI_COMP_SRCS_C})
        endif()

        # UI Helpers：UI辅助函数
        if(CONFIG_ESP_BROOKESIA_SQUARELINE_ENABLE_UI_HELPERS)
            set(GUI_SQUARELINE_UI_HELPERS_SRC_DIR ${GUI_SQUARELINE_SRC_DIR}/ui_helpers)
            file(GLOB_RECURSE GUI_SQUARELINE_UI_HELPERS_SRCS_C ${GUI_SQUARELINE_UI_HELPERS_SRC_DIR}/*.c)
            list(APPEND SRCS_C ${GUI_SQUARELINE_UI_HELPERS_SRCS_C})
        endif()
    endif()

    # LVGL子模块：LVGL图形库封装，提供LvLock、LvObject等工具类
    set(GUI_LVGL_SRC_DIR ${GUI_SRC_DIR}/lvgl)
    file(GLOB_RECURSE GUI_LVGL_SRCS_C ${GUI_LVGL_SRC_DIR}/*.c)
    file(GLOB_RECURSE GUI_LVGL_SRCS_CPP ${GUI_LVGL_SRC_DIR}/*.cpp)
    list(APPEND SRCS_C ${GUI_LVGL_SRCS_C})
    list(APPEND SRCS_CPP ${GUI_LVGL_SRCS_CPP})
    list(APPEND SRCS_COMPILE_OPTIONS "-DLV_LVGL_H_INCLUDE_SIMPLE")  # LVGL简化头文件包含

    # Style子模块：样式系统，定义StyleSize、StyleFont、StyleColor等样式结构
    set(GUI_STYLE_SRC_DIR ${GUI_SRC_DIR}/style)
    file(GLOB_RECURSE GUI_STYLE_SRCS_C ${GUI_STYLE_SRC_DIR}/*.c)
    file(GLOB_RECURSE GUI_STYLE_SRCS_CPP ${GUI_STYLE_SRC_DIR}/*.cpp)
    list(APPEND SRCS_C ${GUI_STYLE_SRCS_C})
    list(APPEND SRCS_CPP ${GUI_STYLE_SRCS_CPP})
endif()

# ==================== Services模块 ====================
# 配置服务层组件，包括NVS存储服务
if(CONFIG_ESP_BROOKESIA_ENABLE_SERVICES)
    set(SERVICES_SRC_DIR ${PROJ_SRC_DIR}/services)
    list(APPEND INCLUDE_DIRS ${SERVICES_SRC_DIR})

    # Storage NVS子模块：基于NVS的键值对存储服务，支持线程安全异步操作
    if(CONFIG_ESP_BROOKESIA_SERVICES_ENABLE_STORAGE_NVS)
        set(SERVICES_STORAGE_NVS_SRC_DIR ${SERVICES_SRC_DIR}/storage_nvs)
        file(GLOB_RECURSE SERVICES_STORAGE_NVS_SRCS_C ${SERVICES_STORAGE_NVS_SRC_DIR}/*.c)
        file(GLOB_RECURSE SERVICES_STORAGE_NVS_SRCS_CPP ${SERVICES_STORAGE_NVS_SRC_DIR}/*.cpp)
        list(APPEND SRCS_C ${SERVICES_STORAGE_NVS_SRCS_C})
        list(APPEND SRCS_CPP ${SERVICES_STORAGE_NVS_SRCS_CPP})
    endif()
endif()

# ==================== Systems模块 ====================
# 配置系统层组件，包括基础系统、手机系统和扬声器系统
if(CONFIG_ESP_BROOKESIA_ENABLE_SYSTEMS)
    set(SYSTEM_SRC_DIR ${PROJ_SRC_DIR}/systems)
    list(APPEND INCLUDE_DIRS ${SYSTEM_SRC_DIR})

    # Base子模块：基础系统框架，包括Event事件系统、App应用抽象、Display显示管理
    set(SYSTEM_BASE_SRC_DIR ${SYSTEM_SRC_DIR}/base)
    file(GLOB_RECURSE SYSTEM_BASE_SRCS_C ${SYSTEM_BASE_SRC_DIR}/*.c)
    file(GLOB_RECURSE SYSTEM_BASE_SRCS_CPP ${SYSTEM_BASE_SRC_DIR}/*.cpp)
    list(APPEND SRCS_C ${SYSTEM_BASE_SRCS_C})
    list(APPEND SRCS_CPP ${SYSTEM_BASE_SRCS_CPP})

    # Phone子模块：手机系统，包括Phone管理器、应用注册表、屏幕管理
    if(CONFIG_ESP_BROOKESIA_SYSTEMS_ENABLE_PHONE)
        set(SYSTEM_PHONE_SRC_DIR ${SYSTEM_SRC_DIR}/phone)
        file(GLOB_RECURSE SYSTEM_PHONE_SRCS_C ${SYSTEM_PHONE_SRC_DIR}/*.c)
        file(GLOB_RECURSE SYSTEM_PHONE_SRCS_CPP ${SYSTEM_PHONE_SRC_DIR}/*.cpp)
        list(APPEND SRCS_C ${SYSTEM_PHONE_SRCS_C})
        list(APPEND SRCS_CPP ${SYSTEM_PHONE_SRCS_CPP})
    endif()

    # Speaker子模块：扬声器系统，支持AI对话时的表情动画显示
    if(CONFIG_ESP_BROOKESIA_SYSTEMS_ENABLE_SPEAKER)
        # 添加C++概念支持和禁用字符串写入警告
        list(APPEND SRCS_COMPILE_OPTIONS "$<$<COMPILE_LANGUAGE:CXX>:-fconcepts>")
        list(APPEND SRCS_COMPILE_OPTIONS "-Wno-write-strings")

        set(SYSTEM_SPEAKER_SRC_DIR ${SYSTEM_SRC_DIR}/speaker)
        file(GLOB_RECURSE SYSTEM_SPEAKER_SRCS_C ${SYSTEM_SPEAKER_SRC_DIR}/*.c)
        file(GLOB_RECURSE SYSTEM_SPEAKER_SRCS_CPP ${SYSTEM_SPEAKER_SRC_DIR}/*.cpp)
        list(APPEND SRCS_C ${SYSTEM_SPEAKER_SRCS_C})
        list(APPEND SRCS_CPP ${SYSTEM_SPEAKER_SRCS_CPP})
        list(APPEND INCLUDE_DIRS ${SYSTEM_SPEAKER_SRC_DIR})
    endif()
endif()

# ==================== 依赖配置 ====================
# 设置组件依赖的ESP-IDF库
set(requires esp_netif esp_wifi nvs_flash)

# 根据IDF版本选择JSON库（IDF 6.x使用cjson，旧版本使用json）
if(DEFINED IDF_VERSION_MAJOR AND IDF_VERSION_MAJOR GREATER_EQUAL 6)
    list(APPEND requires cjson)
else()
    list(APPEND requires json)
endif()

# ==================== 注册组件 ====================
idf_component_register(
    SRCS ${SRCS_C} ${SRCS_CPP}              # 所有源文件
    INCLUDE_DIRS ${INCLUDE_DIRS}            # 所有包含目录
    REQUIRES ${requires}                    # 公共依赖
    PRIV_REQUIRES esp_driver_i2c esp_driver_i2s  # 私有依赖（I2C和I2S驱动）
)

# 包含包管理器并定义组件版本
include(package_manager)
cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})

# ==================== 生成扬声器动画资源 ====================
# 将动画文件打包为SPIFFS分区镜像
if(CONFIG_ESP_BROOKESIA_SYSTEMS_ENABLE_SPEAKER)
    set(SPEAKER_ASSETS_ANIMATIONS_DIR "${SYSTEM_SPEAKER_SRC_DIR}/assets/animations")

    # 启动动画分区
    spiffs_create_partition_assets(
        anim_boot
        "${SPEAKER_ASSETS_ANIMATIONS_DIR}/boot"
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".aaf"
        IMPORT_INC_PATH "${SPEAKER_ASSETS_ANIMATIONS_DIR}"
    )

    # 表情动画分区
    spiffs_create_partition_assets(
        anim_emotion
        "${SPEAKER_ASSETS_ANIMATIONS_DIR}/emotion"
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".aaf"
        IMPORT_INC_PATH "${SPEAKER_ASSETS_ANIMATIONS_DIR}"
    )

    # 图标动画分区
    spiffs_create_partition_assets(
        anim_icon
        "${SPEAKER_ASSETS_ANIMATIONS_DIR}/icon"
        FLASH_IN_PROJECT
        MMAP_FILE_SUPPORT_FORMAT ".aaf"
        IMPORT_INC_PATH "${SPEAKER_ASSETS_ANIMATIONS_DIR}"
    )
endif()

# ==================== 编译选项 ====================
# 设置公共编译选项
target_compile_options(${COMPONENT_LIB}
    PUBLIC
        ${SRCS_COMPILE_OPTIONS}
)

# C源文件编译标志：禁用格式警告
set(SRCS_C_COMPILE_FLAGS "-Wno-format")

# C++源文件编译标志：禁用缺失字段初始化和格式警告
set(SRCS_CPP_COMPILE_FLAGS "-Wno-missing-field-initializers -Wno-format")

# 应用编译标志到C++文件
set_source_files_properties(
    ${SRCS_CPP}
    PROPERTIES
        COMPILE_FLAGS "${SRCS_CPP_COMPILE_FLAGS}"
)

# 应用编译标志到C文件
set_source_files_properties(
    ${SRCS_C}
    PROPERTIES
        COMPILE_FLAGS "${SRCS_C_COMPILE_FLAGS}"
)

# 应用额外编译标志到手机应用示例文件：禁用未使用变量警告
set_source_files_properties(
    ${SYSTEM_PHONE_APP_EXAMPLES_SRCS_C}
    PROPERTIES
        COMPILE_FLAGS "${SRCS_C_COMPILE_FLAGS} -Wno-unused-variable"
)
