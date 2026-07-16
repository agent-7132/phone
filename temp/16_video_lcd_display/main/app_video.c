/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/**
 * @file app_video.c
 * @brief 视频应用模块实现文件
 * 
 * 本模块封装了视频设备的操作接口，实现了基于V4L2标准的视频采集功能：
 * 1. 视频设备初始化和配置
 * 2. 帧缓冲区管理（用户指针模式）
 * 3. 视频流控制（开始/停止）
 * 4. 帧处理回调机制
 * 
 * 主要使用V4L2（Video for Linux 2）接口进行视频设备操作。
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/errno.h>
#include "esp_err.h"
#include "esp_log.h"
#include "linux/videodev2.h"
#include "esp_video_init.h"
#include "app_video.h"

/**
 * @brief 日志标签
 * 
 * 用于ESP_LOG系列宏的日志输出标识，便于在日志中区分视频模块。
 */
static const char *TAG = "app_video";

/**
 * @brief 最大帧缓冲区数量
 * 
 * 视频采集支持的最大帧缓冲区数量，用于多缓冲采集以提高帧率和避免丢帧。
 */
#define MAX_BUFFER_COUNT                (3)

/**
 * @brief 最小帧缓冲区数量
 * 
 * 视频采集所需的最小帧缓冲区数量，至少需要2个缓冲区实现双缓冲。
 */
#define MIN_BUFFER_COUNT                (2)

/**
 * @brief 视频流任务堆栈大小
 * 
 * FreeRTOS视频流处理任务的堆栈大小，单位为字节。
 * 4KB堆栈适用于常规视频处理场景。
 */
#define VIDEO_TASK_STACK_SIZE           (4 * 1024)

/**
 * @brief 视频流任务优先级
 * 
 * FreeRTOS视频流处理任务的优先级，数值越大优先级越高。
 * 优先级4适用于需要及时响应但不抢占关键任务的场景。
 */
#define VIDEO_TASK_PRIORITY             (4)

/**
 * @brief 视频应用状态结构体
 * 
 * 保存视频设备的配置和运行状态信息，是视频模块的核心状态管理器。
 * 采用单例模式，全局仅维护一个实例（app_camera_video）。
 */
typedef struct {
    uint8_t *camera_buffer[MAX_BUFFER_COUNT];           /**< 摄像头帧缓冲区指针数组，支持最多3个缓冲区 */
    size_t camera_buf_size;                             /**< 单个帧缓冲区大小（字节），根据分辨率和像素格式计算 */
    uint8_t camera_buf_count;                           /**< 当前使用的帧缓冲区数量 */
    uint32_t camera_buf_hes;                            /**< 摄像头帧水平分辨率（像素） */
    uint32_t camera_buf_ves;                            /**< 摄像头帧垂直分辨率（像素） */
    struct v4l2_buffer v4l2_buf;                        /**< V4L2缓冲区状态结构，用于缓冲区队列管理 */
    uint8_t camera_mem_mode;                            /**< 内存模式：V4L2_MEMORY_MMAP或V4L2_MEMORY_USERPTR */
    app_video_frame_operation_cb_t user_camera_video_frame_operation_cb;  /**< 用户注册的帧处理回调函数指针 */
    TaskHandle_t video_stream_task_handle;               /**< FreeRTOS视频流任务句柄，用于任务管理 */
    uint8_t video_task_core_id;                         /**< 视频任务绑定的CPU核心ID（0或1） */
    bool video_task_delete;                             /**< 视频任务删除请求标志，为true时任务在下一循环退出 */
    void *video_task_user_data;                         /**< 用户自定义数据指针，透传给帧处理回调 */
} app_video_t;

/**
 * @brief 视频应用全局状态实例
 * 
 * 视频模块的全局状态管理器，存储视频设备的所有配置和运行时状态。
 */
static app_video_t app_camera_video;

/**
 * @brief 初始化视频摄像头子系统
 * 
 * 配置CSI摄像头的SCCB（I2C兼容）通信参数，然后调用esp_video_init初始化视频设备。
 * 
 * SCCB（Serial Camera Control Bus）是摄像头控制总线，与I2C协议兼容但时序略有差异。
 * ESP32-P4通过I2C主机控制器模拟SCCB通信来配置摄像头传感器。
 * 
 * 配置说明：
 * - init_sccb: 设置为false，表示使用已初始化的I2C总线
 * - i2c_handle: BSP提供的I2C总线句柄
 * - freq: I2C时钟频率，由CONFIG_BSP_I2C_CLK_SPEED_HZ配置
 * - reset_pin: 摄像头复位引脚，-1表示不使用硬件复位
 * - pwdn_pin: 摄像头电源控制引脚，-1表示不使用硬件电源控制
 * 
 * @param i2c_bus_handle I2C总线句柄，用于SCCB通信
 * 
 * @return 
 *          - ESP_OK 初始化成功
 *          - 其他错误码 初始化失败
 */
esp_err_t app_video_main(i2c_master_bus_handle_t i2c_bus_handle)
{
    /* 配置CSI摄像头SCCB参数 */
    esp_video_init_csi_config_t csi_config[] = {
        {
            .sccb_config = {
                .init_sccb = false,           /**< 使用已初始化的I2C总线 */
                .i2c_handle = i2c_bus_handle, /**< I2C总线句柄 */
                .freq      = CONFIG_BSP_I2C_CLK_SPEED_HZ, /**< I2C时钟频率 */
            },
            .reset_pin = -1,  /**< 不使用硬件复位引脚 */
            .pwdn_pin  = -1,  /**< 不使用硬件电源控制引脚 */
        },
    };

    /* 配置视频初始化参数 */
    esp_video_init_config_t cam_config = {
        .csi      = csi_config,  /**< CSI摄像头配置数组 */
    };

    /* 初始化视频设备 */
    return esp_video_init(&cam_config);
}

/**
 * @brief 打开视频采集设备并配置格式
 * 
 * 按照V4L2标准流程初始化视频采集设备：
 * 1. 打开视频设备文件（只读模式）
 * 2. 查询设备能力（VIDIOC_QUERYCAP），验证设备支持视频采集
 * 3. 获取当前格式（VIDIOC_G_FMT），保存摄像头分辨率
 * 4. 如果当前像素格式与目标格式不同，设置所需的像素格式（VIDIOC_S_FMT）
 * 5. 设置垂直翻转（V4L2_CID_VFLIP）和水平翻转（V4L2_CID_HFLIP）
 * 
 * V4L2（Video for Linux 2）是Linux系统中标准的视频设备驱动接口，
 * ESP-IDF通过移植V4L2接口实现了对CSI摄像头的统一访问。
 * 
 * @param dev 视频设备路径（如"/dev/video0"，由ESP_VIDEO_MIPI_CSI_DEVICE_NAME定义）
 * @param init_fmt 初始像素格式，支持RGB565、RGB888等（详见video_fmt_t枚举）
 * 
 * @return 
 *          - >=0 视频设备文件描述符，用于后续视频操作
 *          - -1 打开失败，错误原因通过日志输出
 */
int app_video_open(char *dev, video_fmt_t init_fmt)
{
    struct v4l2_format default_format;      /**< V4L2格式结构，用于获取和设置格式 */
    struct v4l2_capability capability;      /**< V4L2设备能力结构 */
    const int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  /**< 视频捕获类型 */

    struct v4l2_ext_controls controls;      /**< V4L2扩展控制结构 */
    struct v4l2_ext_control control[1];     /**< V4L2扩展控制数组 */

    /* 打开视频设备文件 */
    int fd = open(dev, O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG, "Open video failed");
        return -1;
    }

    /* 查询设备能力，验证设备是否支持视频采集 */
    if (ioctl(fd, VIDIOC_QUERYCAP, &capability)) {
        ESP_LOGE(TAG, "failed to get capability");
        goto exit_0;
    }

    /* 输出设备信息日志 */
    ESP_LOGI(TAG, "version: %d.%d.%d", (uint16_t)(capability.version >> 16),
             (uint8_t)(capability.version >> 8),
             (uint8_t)capability.version);
    ESP_LOGI(TAG, "driver:  %s", capability.driver);
    ESP_LOGI(TAG, "card:    %s", capability.card);
    ESP_LOGI(TAG, "bus:     %s", capability.bus_info);

    /* 获取当前视频格式 */
    memset(&default_format, 0, sizeof(struct v4l2_format));
    default_format.type = type;
    if (ioctl(fd, VIDIOC_G_FMT, &default_format) != 0) {
        ESP_LOGE(TAG, "failed to get format");
        goto exit_0;
    }

    /* 输出当前分辨率 */
    ESP_LOGI(TAG, "width=%" PRIu32 " height=%" PRIu32, default_format.fmt.pix.width, default_format.fmt.pix.height);

    /* 保存摄像头分辨率到全局状态 */
    app_camera_video.camera_buf_hes = default_format.fmt.pix.width;
    app_camera_video.camera_buf_ves = default_format.fmt.pix.height;

    /* 如果当前像素格式与目标格式不同，则设置新格式 */
    if (default_format.fmt.pix.pixelformat != init_fmt) {
        struct v4l2_format format = {
            .type = type,
            .fmt.pix.width = default_format.fmt.pix.width,    /**< 保持原有宽度 */
            .fmt.pix.height = default_format.fmt.pix.height,  /**< 保持原有高度 */
            .fmt.pix.pixelformat = init_fmt,                  /**< 设置目标像素格式 */
        };

        if (ioctl(fd, VIDIOC_S_FMT, &format) != 0) {
            ESP_LOGE(TAG, "failed to set format");
            goto exit_0;
        }
    }

    /* 设置垂直翻转（VFLIP），使图像上下翻转 */
    controls.ctrl_class = V4L2_CTRL_CLASS_USER;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_VFLIP;
    control[0].value    = 1;
    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "failed to mirror the frame vertically and skip this step");
    }

    /* 设置水平翻转（HFLIP），使图像左右翻转 */
    controls.ctrl_class = V4L2_CTRL_CLASS_USER;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_HFLIP;
    control[0].value    = 1;
    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "failed to mirror the frame horizontally and skip this step");
    }

    /* 返回设备文件描述符 */
    return fd;

exit_0:
    /* 错误处理：关闭设备文件 */
    close(fd);
    return -1;
}

/**
 * @brief 设置视频采集缓冲区
 * 
 * 配置视频设备使用指定数量的帧缓冲区，支持两种内存模式：
 * 
 * 内存模式说明：
 * - MMAP模式（V4L2_MEMORY_MMAP）：由内核分配缓冲区，用户空间通过mmap映射访问
 *   优点：内核管理内存，效率高；缺点：内存位置不可控
 * - USERPTR模式（V4L2_MEMORY_USERPTR）：用户空间分配缓冲区，传递指针给内核
 *   优点：内存位置可控（可分配到SPIRAM）；缺点：需要用户自行管理内存
 * 
 * 执行流程：
 * 1. 验证缓冲区数量在有效范围内（2~3个）
 * 2. 请求缓冲区（VIDIOC_REQBUFS），通知内核分配缓冲区
 * 3. 查询缓冲区信息（VIDIOC_QUERYBUF），获取缓冲区大小和偏移量
 * 4. 如果是MMAP模式，调用mmap映射缓冲区；如果是USERPTR模式，使用用户提供的缓冲区
 * 5. 将缓冲区入队（VIDIOC_QBUF），准备接收数据
 * 
 * @param video_fd 视频设备文件描述符
 * @param fb_num 帧缓冲区数量（必须在MIN_BUFFER_COUNT和MAX_BUFFER_COUNT之间）
 * @param fb 用户提供的帧缓冲区数组（USERPTR模式），NULL表示使用MMAP模式
 * 
 * @return 
 *          - ESP_OK 设置成功
 *          - ESP_FAIL 设置失败，设备文件已关闭
 */
esp_err_t app_video_set_bufs(int video_fd, uint32_t fb_num, const void **fb)
{
    /* 验证缓冲区数量 */
    if (fb_num > MAX_BUFFER_COUNT) {
        ESP_LOGE(TAG, "buffer num is too large");
        return ESP_FAIL;
    } else if (fb_num < MIN_BUFFER_COUNT) {
        ESP_LOGE(TAG, "At least two buffers are required");
        return ESP_FAIL;
    }

    struct v4l2_requestbuffers req;  /**< V4L2缓冲区请求结构 */
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  /**< 视频捕获类型 */

    /* 初始化缓冲区请求结构 */
    memset(&req, 0, sizeof(req));
    req.count = fb_num;                             /**< 请求的缓冲区数量 */
    app_camera_video.camera_buf_count = fb_num;     /**< 保存缓冲区数量到全局状态 */
    req.type = type;                                /**< 设置缓冲区类型 */

    /* 根据是否提供用户缓冲区选择内存模式 */
    app_camera_video.camera_mem_mode = req.memory = fb ? V4L2_MEMORY_USERPTR : V4L2_MEMORY_MMAP;

    /* 请求内核分配缓冲区 */
    if (ioctl(video_fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "req bufs failed");
        goto errout_req_bufs;
    }

    /* 遍历所有缓冲区，进行映射或注册 */
    for (int i = 0; i < fb_num; i++) {
        struct v4l2_buffer buf;  /**< V4L2缓冲区状态结构 */
        memset(&buf, 0, sizeof(buf));
        buf.type = type;        /**< 设置缓冲区类型 */
        buf.memory = req.memory; /**< 设置内存模式 */
        buf.index = i;          /**< 设置缓冲区索引 */

        /* 查询缓冲区信息 */
        if (ioctl(video_fd, VIDIOC_QUERYBUF, &buf) != 0) {
            ESP_LOGE(TAG, "query buf failed");
            goto errout_req_bufs;
        }

        /* 根据内存模式处理缓冲区 */
        if (req.memory == V4L2_MEMORY_MMAP) {
            /* MMAP模式：映射内核缓冲区到用户空间 */
            app_camera_video.camera_buffer[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, video_fd, buf.m.offset);
            if (app_camera_video.camera_buffer[i] == NULL) {
                ESP_LOGE(TAG, "mmap failed");
                goto errout_req_bufs;
            }
        } else {
            /* USERPTR模式：使用用户提供的缓冲区 */
            if (!fb[i]) {
                ESP_LOGE(TAG, "frame buffer is NULL");
                goto errout_req_bufs;
            }
            buf.m.userptr = (unsigned long)fb[i];              /**< 设置用户缓冲区指针 */
            app_camera_video.camera_buffer[i] = (uint8_t *)fb[i];  /**< 保存缓冲区指针到全局状态 */
        }

        /* 保存缓冲区大小到全局状态 */
        app_camera_video.camera_buf_size = buf.length;

        /* 将缓冲区入队，准备接收数据 */
        if (ioctl(video_fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "queue frame buffer failed");
            goto errout_req_bufs;
        }
    }

    return ESP_OK;

errout_req_bufs:
    /* 错误处理：关闭设备文件 */
    close(video_fd);
    return ESP_FAIL;
}

/**
 * @brief 获取视频采集缓冲区指针
 * 
 * 将已配置的帧缓冲区指针填充到用户提供的数组中。
 * 该函数用于在MMAP模式下获取内核分配的缓冲区指针。
 * 
 * @param fb_num 要获取的缓冲区数量（必须与之前设置的数量一致）
 * @param fb 输出参数，缓冲区指针数组，用于接收缓冲区地址
 * 
 * @return 
 *          - ESP_OK 获取成功
 *          - ESP_FAIL 获取失败（缓冲区数量超出范围或缓冲区未初始化）
 */
esp_err_t app_video_get_bufs(int fb_num, void **fb)
{
    /* 验证缓冲区数量 */
    if (fb_num > MAX_BUFFER_COUNT) {
        ESP_LOGE(TAG, "buffer num is too large");
        return ESP_FAIL;
    } else if (fb_num < MIN_BUFFER_COUNT) {
        ESP_LOGE(TAG, "At least two buffers are required");
        return ESP_FAIL;
    }

    /* 将缓冲区指针填充到用户数组 */
    for (int i = 0; i < fb_num; i++) {
        if (app_camera_video.camera_buffer[i] != NULL) {
            fb[i] = app_camera_video.camera_buffer[i];
        } else {
            ESP_LOGE(TAG, "frame buffer is NULL");
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

/**
 * @brief 获取视频帧缓冲区大小
 * 
 * 根据摄像头分辨率和像素格式计算单个帧缓冲区的大小。
 * 
 * 计算公式：缓冲区大小 = 宽度 × 高度 × 每像素字节数
 * - RGB565格式：每像素2字节
 * - RGB888格式：每像素3字节
 * 
 * @return 单个帧缓冲区大小（字节）
 */
uint32_t app_video_get_buf_size(void)
{
    /* 计算缓冲区大小：宽度 × 高度 × 每像素字节数 */
    uint32_t buf_size = app_camera_video.camera_buf_hes * app_camera_video.camera_buf_ves * (APP_VIDEO_FMT == APP_VIDEO_FMT_RGB565 ? 2 : 3);

    return buf_size;
}

/**
 * @brief 接收视频帧（出队）
 * 
 * 从视频设备的缓冲区队列中出队一个已填充数据的缓冲区。
 * 调用VIDIOC_DQBUF后，缓冲区中包含最新采集的视频帧数据。
 * 
 * @param video_fd 视频设备文件描述符
 * 
 * @return 
 *          - ESP_OK 接收成功，缓冲区信息保存在app_camera_video.v4l2_buf中
 *          - ESP_FAIL 接收失败
 */
static inline esp_err_t video_receive_video_frame(int video_fd)
{
    /* 初始化V4L2缓冲区结构 */
    memset(&app_camera_video.v4l2_buf, 0, sizeof(app_camera_video.v4l2_buf));
    app_camera_video.v4l2_buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;  /**< 设置缓冲区类型 */
    app_camera_video.v4l2_buf.memory = app_camera_video.camera_mem_mode;  /**< 设置内存模式 */

    /* 出队缓冲区，获取已填充数据的缓冲区 */
    int res = ioctl(video_fd, VIDIOC_DQBUF, &(app_camera_video.v4l2_buf));
    if (res != 0) {
        ESP_LOGE(TAG, "failed to receive video frame");
        goto errout;
    }

    return ESP_OK;

errout:
    return ESP_FAIL;
}

/**
 * @brief 处理视频帧
 * 
 * 调用用户注册的帧处理回调函数，将视频帧数据传递给应用层处理。
 * 该函数是视频流处理的核心，负责将原始帧数据传递给用户自定义的处理逻辑。
 * 
 * @param video_fd 视频设备文件描述符
 */
static inline void video_operation_video_frame(int video_fd)
{
    /* 更新缓冲区用户指针和长度 */
    app_camera_video.v4l2_buf.m.userptr = (unsigned long)app_camera_video.camera_buffer[app_camera_video.v4l2_buf.index];
    app_camera_video.v4l2_buf.length = app_camera_video.camera_buf_size;

    /* 获取当前缓冲区索引 */
    uint8_t buf_index = app_camera_video.v4l2_buf.index;

    /* 调用用户注册的帧处理回调函数 */
    app_camera_video.user_camera_video_frame_operation_cb(
                        app_camera_video.camera_buffer[buf_index],  /**< 帧缓冲区指针 */
                        buf_index,                                  /**< 缓冲区索引 */
                        app_camera_video.camera_buf_hes,            /**< 水平分辨率 */
                        app_camera_video.camera_buf_ves,            /**< 垂直分辨率 */
                        app_camera_video.camera_buf_size,           /**< 缓冲区大小 */
                        app_camera_video.video_task_user_data       /**< 用户自定义数据 */
                    );
}

/**
 * @brief 释放视频帧（入队）
 * 
 * 将处理完的缓冲区重新入队，供下次视频采集使用。
 * 调用VIDIOC_QBUF后，缓冲区回到空闲队列，等待摄像头填充新数据。
 * 
 * @param video_fd 视频设备文件描述符
 * 
 * @return 
 *          - ESP_OK 释放成功，缓冲区已重新入队
 *          - ESP_FAIL 释放失败
 */
static inline esp_err_t video_free_video_frame(int video_fd)
{
    /* 将缓冲区重新入队 */
    if (ioctl(video_fd, VIDIOC_QBUF, &(app_camera_video.v4l2_buf)) != 0) {
        ESP_LOGE(TAG, "failed to free video frame");
        goto errout;
    }

    return ESP_OK;

errout:
    return ESP_FAIL;
}

/**
 * @brief 开始视频流采集
 * 
 * 启动视频设备的采集功能，摄像头开始向已入队的缓冲区填充数据。
 * 调用VIDIOC_STREAMON后，视频流进入运行状态。
 * 
 * @param video_fd 视频设备文件描述符
 * 
 * @return 
 *          - ESP_OK 启动成功
 *          - ESP_FAIL 启动失败
 */
static inline esp_err_t video_stream_start(int video_fd)
{
    ESP_LOGI(TAG, "Video Stream Start");

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    /* 启动视频流 */
    if (ioctl(video_fd, VIDIOC_STREAMON, &type)) {
        ESP_LOGE(TAG, "failed to start stream");
        goto errout;
    }

    /* 获取当前视频格式（用于验证） */
    struct v4l2_format format = {0};
    format.type = type;
    if (ioctl(video_fd, VIDIOC_G_FMT, &format) != 0) {
        ESP_LOGE(TAG, "get fmt failed");
        goto errout;
    }

    return ESP_OK;

errout:
    return ESP_FAIL;
}

/**
 * @brief 停止视频流采集
 * 
 * 停止视频设备的采集功能，摄像头停止向缓冲区填充数据。
 * 调用VIDIOC_STREAMOFF后，视频流进入停止状态。
 * 
 * @param video_fd 视频设备文件描述符
 * 
 * @return 
 *          - ESP_OK 停止成功
 *          - ESP_FAIL 停止失败
 */
static inline esp_err_t video_stream_stop(int video_fd)
{
    ESP_LOGI(TAG, "Video Stream Stop");

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    /* 停止视频流 */
    if (ioctl(video_fd, VIDIOC_STREAMOFF, &type)) {
        ESP_LOGE(TAG, "failed to stop stream");
        goto errout;
    }

    return ESP_OK;

errout:
    return ESP_FAIL;
}

/**
 * @brief 视频流处理任务
 * 
 * FreeRTOS任务函数，负责持续处理视频流数据。
 * 循环执行以下操作：
 * 1. 接收视频帧（从设备出队已填充数据的缓冲区）
 * 2. 处理视频帧（调用用户注册的回调函数）
 * 3. 释放视频帧（将缓冲区重新入队，供下次采集使用）
 * 4. 检查退出标志，如需要则停止流并删除任务
 * 
 * 任务优先级：VIDEO_TASK_PRIORITY（4）
 * 任务堆栈：VIDEO_TASK_STACK_SIZE（4KB）
 * 
 * @param arg 视频设备文件描述符指针（int *类型）
 */
static void video_stream_task(void *arg)
{
    /* 获取视频设备文件描述符 */
    int video_fd = *((int *)arg);

    /* 视频流处理主循环 */
    while (1) {
        /* 步骤1：接收视频帧（出队缓冲区） */
        ESP_ERROR_CHECK(video_receive_video_frame(video_fd));

        /* 步骤2：处理视频帧（调用用户回调） */
        video_operation_video_frame(video_fd);

        /* 步骤3：释放视频帧（入队缓冲区） */
        ESP_ERROR_CHECK(video_free_video_frame(video_fd));

        /* 步骤4：检查退出标志 */
        if (app_camera_video.video_task_delete) {
            app_camera_video.video_task_delete = false;  /**< 清除退出标志 */
            ESP_ERROR_CHECK(video_stream_stop(video_fd));  /**< 停止视频流 */
            vTaskDelete(NULL);  /**< 删除当前任务 */
        }
    }
    /* 防止编译器警告，理论上不会执行到这里 */
    vTaskDelete(NULL);
}

/**
 * @brief 启动视频流任务
 * 
 * 启动视频流并创建FreeRTOS任务持续处理视频帧。
 * 
 * 执行流程：
 * 1. 保存任务配置（CPU核心ID和用户数据）到全局状态
 * 2. 启动视频流采集
 * 3. 创建FreeRTOS任务并绑定到指定CPU核心
 * 4. 如果任务创建失败，停止视频流并返回错误
 * 
 * @param video_fd 视频设备文件描述符
 * @param core_id 任务运行的CPU核心ID（0或1）
 * @param user_data 用户自定义数据，会透传给帧处理回调函数
 * 
 * @return 
 *          - ESP_OK 启动成功
 *          - ESP_FAIL 启动失败（任务创建失败，视频流已停止）
 */
esp_err_t app_video_stream_task_start(int video_fd, int core_id, void *user_data)
{
    /* 保存任务配置到全局状态 */
    app_camera_video.video_task_core_id = core_id;           /**< 任务绑定的CPU核心 */
    app_camera_video.video_task_user_data = user_data;       /**< 用户自定义数据 */

    /* 启动视频流采集 */
    video_stream_start(video_fd);

    /* 创建FreeRTOS任务并绑定到指定CPU核心 */
    BaseType_t result = xTaskCreatePinnedToCore(
        video_stream_task,          /**< 任务函数 */
        "video stream task",        /**< 任务名称 */
        VIDEO_TASK_STACK_SIZE,      /**< 任务堆栈大小 */
        &video_fd,                  /**< 任务参数（视频设备文件描述符指针） */
        VIDEO_TASK_PRIORITY,        /**< 任务优先级 */
        &app_camera_video.video_stream_task_handle,  /**< 任务句柄 */
        core_id                     /**< CPU核心ID */
    );

    /* 检查任务创建结果 */
    if (result != pdPASS) {
        ESP_LOGE(TAG, "failed to create video stream task");
        goto errout;
    }

    return ESP_OK;

errout:
    /* 错误处理：停止视频流 */
    video_stream_stop(video_fd);
    return ESP_FAIL;
}

/**
 * @brief 重启视频流任务
 * 
 * 重新设置缓冲区并重启视频流任务。用于在视频流异常后恢复。
 * 
 * 执行流程：
 * 1. 重新设置帧缓冲区（使用之前保存的缓冲区配置）
 * 2. 启动视频流任务
 * 3. 如果启动失败，返回错误
 * 
 * @param video_fd 视频设备文件描述符
 * 
 * @return 
 *          - ESP_OK 重启成功
 *          - ESP_FAIL 重启失败
 */
esp_err_t app_video_stream_task_restart(int video_fd)
{
    /* 重新设置帧缓冲区 */
    app_video_set_bufs(video_fd, app_camera_video.camera_buf_count, (const void **)app_camera_video.camera_buffer);

    /* 启动视频流任务 */
    esp_err_t ret = app_video_stream_task_start(video_fd, app_camera_video.video_task_core_id, app_camera_video.video_task_user_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to restart video stream task");
        goto errout;
    }

    return ESP_OK;

errout:
    return ESP_FAIL;
}

/**
 * @brief 停止视频流任务
 * 
 * 设置任务删除标志，视频流任务会在下一次循环中检测到标志并自行停止。
 * 
 * 注意：该函数不会立即停止任务，而是设置标志。任务会在完成当前帧处理后停止。
 * 
 * @param video_fd 视频设备文件描述符（未使用，保留接口一致性）
 * 
 * @return ESP_OK 停止请求已发送
 */
esp_err_t app_video_stream_task_stop(int video_fd)
{
    /* 设置任务删除标志 */
    app_camera_video.video_task_delete = true;

    return ESP_OK;
}

/**
 * @brief 注册帧处理回调函数
 * 
 * 设置用户定义的帧处理回调函数，当视频设备接收到一帧新数据时会调用该回调。
 * 
 * 回调函数的参数：
 * - camera_buf: 帧缓冲区指针
 * - camera_buf_index: 缓冲区索引（0或1）
 * - camera_buf_hes: 水平分辨率
 * - camera_buf_ves: 垂直分辨率
 * - camera_buf_len: 缓冲区大小
 * - user_data: 用户自定义数据
 * 
 * @param operation_cb 帧处理回调函数指针
 * 
 * @return ESP_OK 注册成功
 */
esp_err_t app_video_register_frame_operation_cb(app_video_frame_operation_cb_t operation_cb)
{
    /* 保存回调函数指针到全局状态 */
    app_camera_video.user_camera_video_frame_operation_cb = operation_cb;

    return ESP_OK;
}
