/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

/**
 * @file simple_video_server_example.c
 * @brief ESP32-P4 简易视频服务器示例主程序
 *
 * 本文件实现了一个基于 ESP-IDF 的 Web 视频流服务器，支持以下功能：
 * - 通过 HTTP 提供实时视频流（MJPEG格式）
 * - 支持多种摄像头接口：MIPI-CSI、DVP、SPI、USB-UVC
 * - 支持多摄像头同时流输出
 * - 通过 Web API 控制摄像头参数（如 JPEG 质量）
 * - 提供静态网页资源服务（HTML、CSS、JS、图片）
 * - 支持 mDNS 和 NetBIOS 主机名解析
 *
 * 整体架构：
 * ┌─────────────────────────────────────────────────────────────┐
 * │                    HTTP Server Layer                        │
 * │  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐   │
 * │  │  /stream  │ │ /api/...  │ │ /index    │ │ /assets/  │   │
 * │  └─────┬─────┘ └─────┬─────┘ └─────┬─────┘ └─────┬─────┘   │
 * └─────────┼─────────────┼─────────────┼─────────────┼─────────┘
 *           │             │             │             │
 * ┌─────────▼─────────────▼─────────────▼─────────────▼─────────┐
 * │                  Web Camera Manager                         │
 * │  ┌───────────────────────────────────────────────────────┐  │
 * │  │  web_cam_t ──► web_cam_video_t[video_count]          │  │
 * │  │      │                  │                             │  │
 * │  │      │                  ▼                             │  │
 * │  │      │        ┌──────────────────────┐                │  │
 * │  │      │        │  V4L2 Video Device   │                │  │
 * │  │      │        │  (mmap buffers)      │                │  │
 * │  │      │        └──────────┬───────────┘                │  │
 * │  │      │                   │                             │  │
 * │  │      │                   ▼                             │  │
 * │  │      │        ┌──────────────────────┐                │  │
 * │  │      │        │  JPEG Encoder        │                │  │
 * │  │      │        │  (HW/SW)             │                │  │
 * │  │      │        └──────────────────────┘                │  │
 * │  └───────────────────────────────────────────────────────┘  │
 * └─────────────────────────────────────────────────────────────┘
 */

#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/errno.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "protocol_examples_common.h"
#include "mdns.h"
#include "lwip/inet.h"
#include "lwip/apps/netbiosns.h"
#include "example_video_common.h"

/**
 * @def EXAMPLE_CAMERA_VIDEO_BUFFER_NUMBER
 * @brief 摄像头视频缓冲区数量
 * 
 * 从 menuconfig 中 CONFIG_EXAMPLE_CAMERA_VIDEO_BUFFER_NUMBER 配置项读取，
 * 决定了 V4L2 驱动分配的视频帧缓冲区数量。缓冲区数量越多，视频流越稳定，
 * 但占用内存也越多。
 */
#define EXAMPLE_CAMERA_VIDEO_BUFFER_NUMBER  CONFIG_EXAMPLE_CAMERA_VIDEO_BUFFER_NUMBER

/**
 * @def EXAMPLE_JPEG_ENC_QUALITY
 * @brief JPEG 编码默认质量
 * 
 * 从 menuconfig 中 CONFIG_EXAMPLE_JPEG_COMPRESSION_QUALITY 配置项读取，
 * 取值范围通常为 1-100，值越大图像质量越高，但压缩率越低，数据量越大。
 */
#define EXAMPLE_JPEG_ENC_QUALITY            CONFIG_EXAMPLE_JPEG_COMPRESSION_QUALITY

/**
 * @def EXAMPLE_MDNS_INSTANCE
 * @brief mDNS 实例名称
 * 
 * 从 menuconfig 中 CONFIG_EXAMPLE_MDNS_INSTANCE 配置项读取，
 * 用于在局域网中通过 mDNS 服务发现设备。
 */
#define EXAMPLE_MDNS_INSTANCE               CONFIG_EXAMPLE_MDNS_INSTANCE

/**
 * @def EXAMPLE_MDNS_HOST_NAME
 * @brief mDNS 主机名称
 * 
 * 从 menuconfig 中 CONFIG_EXAMPLE_MDNS_HOST_NAME 配置项读取，
 * 设备在局域网中的主机名，可通过 http://<hostname>.local 访问。
 */
#define EXAMPLE_MDNS_HOST_NAME              CONFIG_EXAMPLE_MDNS_HOST_NAME

/**
 * @def EXAMPLE_PART_BOUNDARY
 * @brief HTTP multipart 分界符
 * 
 * 从 menuconfig 中 CONFIG_EXAMPLE_HTTP_PART_BOUNDARY 配置项读取，
 * 用于 MJPEG 流的 multipart/x-mixed-replace 格式中的边界标识。
 */
#define EXAMPLE_PART_BOUNDARY               CONFIG_EXAMPLE_HTTP_PART_BOUNDARY

/**
 * @def STREAM_CONTENT_TYPE
 * @brief MJPEG 流的 HTTP Content-Type
 * 
 * 使用 multipart/x-mixed-replace 格式，这是 MJPEG 实时流的标准格式，
 * 允许服务器持续推送 JPEG 图像而无需重新建立连接。
 */
static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" EXAMPLE_PART_BOUNDARY;

/**
 * @def STREAM_BOUNDARY
 * @brief MJPEG 流的分界符前缀
 * 
 * 在每个 JPEG 帧之前发送，用于分隔 multipart 流中的各个部分。
 */
static const char *STREAM_BOUNDARY = "\r\n--" EXAMPLE_PART_BOUNDARY "\r\n";

/**
 * @def STREAM_PART
 * @brief MJPEG 流的单个部分格式模板
 * 
 * 包含 Content-Type、Content-Length 和 X-Timestamp 头信息，
 * 其中 X-Timestamp 用于客户端同步显示时间。
 */
static const char *STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";

/**
 * @var index_html_gz_start
 * @brief 压缩的 HTML 首页起始地址（由链接器生成）
 * 
 * 通过 ESP-IDF 的二进制资源嵌入功能，将前端编译后的 index.html.gz 文件
 * 嵌入到固件中，链接器自动生成此符号表示资源起始位置。
 */
extern const uint8_t index_html_gz_start[] asm("_binary_index_html_gz_start");

/**
 * @var index_html_gz_end
 * @brief 压缩的 HTML 首页结束地址（由链接器生成）
 * 
 * 与 index_html_gz_start 配合使用，通过计算两者差值获取资源长度。
 */
extern const uint8_t index_html_gz_end[] asm("_binary_index_html_gz_end");

/**
 * @var loading_jpg_gz_start
 * @brief 压缩的加载图片起始地址（由链接器生成）
 * 
 * 视频流加载过程中显示的占位图片，已压缩以减少固件体积。
 */
extern const uint8_t loading_jpg_gz_start[] asm("_binary_loading_jpg_gz_start");

/**
 * @var loading_jpg_gz_end
 * @brief 压缩的加载图片结束地址（由链接器生成）
 */
extern const uint8_t loading_jpg_gz_end[] asm("_binary_loading_jpg_gz_end");

/**
 * @var favicon_ico_gz_start
 * @brief 压缩的网站图标起始地址（由链接器生成）
 */
extern const uint8_t favicon_ico_gz_start[] asm("_binary_favicon_ico_gz_start");

/**
 * @var favicon_ico_gz_end
 * @brief 压缩的网站图标结束地址（由链接器生成）
 */
extern const uint8_t favicon_ico_gz_end[] asm("_binary_favicon_ico_gz_end");

/**
 * @var assets_index_js_gz_start
 * @brief 压缩的 JavaScript 文件起始地址（由链接器生成）
 * 
 * 前端 Vue 应用编译后的主 JS 文件，包含所有交互逻辑。
 */
extern const uint8_t assets_index_js_gz_start[] asm("_binary_index_js_gz_start");

/**
 * @var assets_index_js_gz_end
 * @brief 压缩的 JavaScript 文件结束地址（由链接器生成）
 */
extern const uint8_t assets_index_js_gz_end[] asm("_binary_index_js_gz_end");

/**
 * @var assets_index_css_gz_start
 * @brief 压缩的 CSS 样式文件起始地址（由链接器生成）
 * 
 * 前端 Vuetify 框架编译后的样式文件。
 */
extern const uint8_t assets_index_css_gz_start[] asm("_binary_index_css_gz_start");

/**
 * @var assets_index_css_gz_end
 * @brief 压缩的 CSS 样式文件结束地址（由链接器生成）
 */
extern const uint8_t assets_index_css_gz_end[] asm("_binary_index_css_gz_end");

/**
 * @struct web_cam_video
 * @brief 单个摄像头视频流的控制结构
 * 
 * 封装了单个摄像头设备的所有状态和资源，包括文件描述符、编码器句柄、
 * 帧缓冲区、分辨率、像素格式等信息。
 */
typedef struct web_cam_video {
    int fd;                                          /**< V4L2 设备文件描述符，-1 表示无效 */
    uint8_t index;                                   /**< 摄像头索引（从0开始） */

    example_encoder_handle_t encoder_handle;         /**< JPEG 编码器句柄（软件编码时使用） */
    uint8_t *jpeg_out_buf;                           /**< JPEG 输出缓冲区指针 */
    uint32_t jpeg_out_size;                          /**< JPEG 输出缓冲区大小 */

    uint8_t *buffer[EXAMPLE_CAMERA_VIDEO_BUFFER_NUMBER];  /**< V4L2 内存映射缓冲区数组 */
    uint32_t buffer_size;                            /**< 单个缓冲区大小（字节） */

    uint32_t width;                                  /**< 视频帧宽度（像素） */
    uint32_t height;                                 /**< 视频帧高度（像素） */
    uint32_t pixel_format;                           /**< 像素格式（V4L2 格式，如 V4L2_PIX_FMT_JPEG） */
    uint8_t jpeg_quality;                            /**< 当前 JPEG 质量设置 */

    uint32_t frame_rate;                             /**< 当前帧率（fps） */

    SemaphoreHandle_t sem;                           /**< 编码器互斥信号量，防止多线程同时访问编码器 */

    uint32_t support_control_jpeg_quality   : 1;     /**< 位域：是否支持通过 API 控制 JPEG 质量 */
} web_cam_video_t;

/**
 * @struct web_cam
 * @brief 多摄像头设备的容器结构
 * 
 * 使用柔性数组（flexible array member）存储多个摄像头视频流，
 * 支持单摄像头和多摄像头场景。
 */
typedef struct web_cam {
    uint8_t video_count;                             /**< 摄像头数量 */
    web_cam_video_t video[0];                        /**< 摄像头视频流数组（柔性数组） */
} web_cam_t;

/**
 * @struct web_cam_video_config
 * @brief 摄像头视频初始化配置
 * 
 * 传递给摄像头初始化函数的配置参数，主要包含设备路径。
 */
typedef struct web_cam_video_config {
    const char *dev_name;                            /**< V4L2 设备路径（如 /dev/video0） */
    uint32_t buffer_count;                           /**< 缓冲区数量（预留字段） */
} web_cam_video_config_t;

/**
 * @struct request_desc
 * @brief HTTP 请求描述符
 * 
 * 用于解析 HTTP 请求中的摄像头索引参数。
 */
typedef struct request_desc {
    int index;                                       /**< 请求指定的摄像头索引 */
} request_desc_t;

/**
 * @var TAG
 * @brief 日志标签
 * 
 * 用于 ESP_LOGx 系列宏的日志输出标识，方便在日志中过滤和查找相关信息。
 */
static const char *TAG = "example";

/**
 * @brief 检查摄像头是否有效
 * 
 * 判断摄像头设备文件描述符是否有效（非 -1），用于在操作前验证设备状态。
 * 
 * @param video 摄像头视频控制结构指针
 * @return true 摄像头有效，false 摄像头无效
 */
static bool is_valid_web_cam(web_cam_video_t *video)
{
    return video->fd != -1;
}

/**
 * @brief 解码 HTTP 请求中的摄像头索引参数
 * 
 * 从 HTTP 请求的 URL 查询字符串中提取 source 参数，确定请求的摄像头索引。
 * 例如：/api/capture_image?source=0 表示请求第 0 个摄像头。
 * 
 * @param web_cam 摄像头容器结构指针
 * @param req HTTP 请求结构指针
 * @param desc 输出参数，存储解析后的摄像头索引
 * @return ESP_OK 解码成功，ESP_ERR_INVALID_ARG 索引无效，其他错误码表示读取失败
 */
static esp_err_t decode_request(web_cam_t *web_cam, httpd_req_t *req, request_desc_t *desc)
{
    esp_err_t ret;
    int index = -1;
    char buffer[32];

    /* 读取 URL 查询字符串 */
    if ((ret = httpd_req_get_url_query_str(req, buffer, sizeof(buffer))) != ESP_OK) {
        return ret;
    }
    ESP_LOGD(TAG, "source: %s", buffer);

    /* 遍历所有摄像头，匹配 source 参数 */
    for (int i = 0; i < web_cam->video_count; i++) {
        char source_str[16];

        if (snprintf(source_str, sizeof(source_str), "source=%d", i) <= 0) {
            return ESP_FAIL;
        }

        if (strcmp(buffer, source_str) == 0) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        return ESP_ERR_INVALID_ARG;
    }

    desc->index = index;
    return ESP_OK;
}

/**
 * @brief 捕获并发送单帧视频图像
 * 
 * 从 V4L2 设备获取一帧视频数据，如果需要则进行 JPEG 编码，然后发送给 HTTP 客户端。
 * 支持直接发送 JPEG 格式（摄像头硬件输出）和发送编码后的 JPEG（软件编码）两种模式。
 * 
 * @param req HTTP 请求结构指针
 * @param video 摄像头视频控制结构指针
 * @param is_jpeg 是否要求输出 JPEG 格式（true 为 JPEG，false 为原始二进制数据）
 * @return ESP_OK 成功，其他错误码表示失败
 */
static esp_err_t capture_video_image(httpd_req_t *req, web_cam_video_t *video, bool is_jpeg)
{
    esp_err_t ret;
    struct v4l2_buffer buf;
    const char *type_str = is_jpeg ? "JPEG" : "binary";
    uint32_t jpeg_encoded_size;

    /* 初始化 V4L2 缓冲区结构 */
    memset(&buf, 0, sizeof(buf));
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    /* 从 V4L2 设备出队一个缓冲区（获取一帧数据） */
    ESP_RETURN_ON_ERROR(ioctl(video->fd, VIDIOC_DQBUF, &buf), TAG, "failed to receive video frame");

    /* 检查缓冲区是否包含有效数据 */
    if (!(buf.flags & V4L2_BUF_FLAG_DONE)) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (!is_jpeg || video->pixel_format == V4L2_PIX_FMT_JPEG) {
        /* 直接发送原始缓冲区数据（非 JPEG 请求或摄像头已输出 JPEG） */
        ESP_GOTO_ON_ERROR(httpd_resp_send(req, (char *)video->buffer[buf.index], buf.bytesused), fail0, TAG, "failed to send %s", type_str);
        jpeg_encoded_size = buf.bytesused;
    } else {
        /* 需要软件编码：获取编码器互斥锁 */
        ESP_GOTO_ON_FALSE(xSemaphoreTake(video->sem, portMAX_DELAY) == pdPASS, ESP_FAIL, fail0, TAG, "failed to take semaphore");

        /* 调用编码器处理原始帧数据 */
        ret = example_encoder_process(video->encoder_handle, video->buffer[buf.index], video->buffer_size,
                                      video->jpeg_out_buf, video->jpeg_out_size, &jpeg_encoded_size);
        xSemaphoreGive(video->sem);  /* 释放编码器互斥锁 */

        ESP_GOTO_ON_ERROR(ret, fail0, TAG, "failed to encode video frame");
        ESP_GOTO_ON_ERROR(httpd_resp_send(req, (char *)video->jpeg_out_buf, jpeg_encoded_size), fail0, TAG, "failed to send %s", type_str);
    }

    /* 将缓冲区重新入队，供下一次捕获使用 */
    ESP_RETURN_ON_ERROR(ioctl(video->fd, VIDIOC_QBUF, &buf), TAG, "failed to queue video frame");

    /* 发送空块表示响应结束 */
    ESP_GOTO_ON_ERROR(httpd_resp_sendstr_chunk(req, NULL), fail0, TAG, "failed to send null");

    ESP_LOGD(TAG, "send %s image%d size: %" PRIu32, type_str, video->index, jpeg_encoded_size);

    return ESP_OK;

fail0:
    /* 错误处理：确保缓冲区被重新入队 */
    ioctl(video->fd, VIDIOC_QBUF, &buf);
    return ret;
}

/**
 * @brief 获取摄像头信息的 JSON 描述
 * 
 * 生成包含所有摄像头状态信息的 JSON 字符串，包括分辨率、帧率、格式、
 * 质量设置范围等，用于前端页面展示和配置。
 * 
 * @param web_cam 摄像头容器结构指针
 * @return JSON 字符串指针（需要调用 free() 释放），NULL 表示内存分配失败
 */
static char *get_cameras_json(web_cam_t *web_cam)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *cameras = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "cameras", cameras);

    /* 遍历所有摄像头 */
    for (int i = 0; i < web_cam->video_count; i++) {
        char src_str[32];

        /* 跳过无效摄像头 */
        if (!is_valid_web_cam(&web_cam->video[i])) {
            continue;
        }

        /* 创建摄像头信息对象 */
        cJSON *camera = cJSON_CreateObject();
        cJSON_AddNumberToObject(camera, "index", i);

        /* 构建视频流 URL（格式：:端口/stream） */
        assert(snprintf(src_str, sizeof(src_str), ":%d/stream", i + 81) > 0);
        cJSON_AddStringToObject(camera, "src", src_str);

        /* 添加当前帧率和格式信息 */
        cJSON_AddNumberToObject(camera, "currentFrameRate", web_cam->video[i].frame_rate);
        cJSON_AddNumberToObject(camera, "currentImageFormat", 0);

        /* 添加格式描述字符串（如 "JPEG 640x480"） */
        assert(snprintf(src_str, sizeof(src_str), "JPEG %" PRIu32 "x%" PRIu32, web_cam->video[i].width, web_cam->video[i].height) > 0);
        cJSON_AddStringToObject(camera, "currentImageFormatDescription", src_str);

        /* 如果支持质量控制，添加当前质量值 */
        if (web_cam->video[i].support_control_jpeg_quality) {
            cJSON_AddNumberToObject(camera, "currentQuality", web_cam->video[i].jpeg_quality);
        }

        /* 添加当前分辨率信息 */
        cJSON *current_resolution = cJSON_CreateObject();
        cJSON_AddNumberToObject(current_resolution, "width", web_cam->video[i].width);
        cJSON_AddNumberToObject(current_resolution, "height", web_cam->video[i].height);
        cJSON_AddItemToObject(camera, "currentResolution", current_resolution);

        /* 添加支持的图像格式列表 */
        cJSON *image_formats = cJSON_CreateArray();
        cJSON *image_format = cJSON_CreateObject();
        cJSON_AddNumberToObject(image_format, "id", 0);
        assert(snprintf(src_str, sizeof(src_str), "JPEG %" PRIu32 "x%" PRIu32, web_cam->video[i].width, web_cam->video[i].height) > 0);
        cJSON_AddStringToObject(image_format, "description", src_str);

        /* 如果支持质量控制，添加质量参数范围 */
        if (web_cam->video[i].support_control_jpeg_quality) {
            cJSON *image_format_quality = cJSON_CreateObject();

            int min_quality = 1;
            int max_quality = 100;
            int step_quality = 1;
            int default_quality = EXAMPLE_JPEG_ENC_QUALITY;

            /* 如果是硬件 JPEG 输出，查询实际支持的质量范围 */
            if (web_cam->video[i].pixel_format == V4L2_PIX_FMT_JPEG) {
                struct v4l2_query_ext_ctrl qctrl = {0};

                qctrl.id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
                if (ioctl(web_cam->video[i].fd, VIDIOC_QUERY_EXT_CTRL, &qctrl) == 0) {
                    min_quality = qctrl.minimum;
                    max_quality = qctrl.maximum;
                    step_quality = qctrl.step;
                    default_quality = qctrl.default_value;
                }
            }

            /* 添加质量参数到 JSON */
            cJSON_AddNumberToObject(image_format_quality, "min", min_quality);
            cJSON_AddNumberToObject(image_format_quality, "max", max_quality);
            cJSON_AddNumberToObject(image_format_quality, "step", step_quality);
            cJSON_AddNumberToObject(image_format_quality, "default", default_quality);
            cJSON_AddItemToObject(image_format, "quality", image_format_quality);
        }
        cJSON_AddItemToArray(image_formats, image_format);

        cJSON_AddItemToObject(camera, "imageFormats", image_formats);
        cJSON_AddItemToArray(cameras, camera);
    }

    /* 将 JSON 对象转换为字符串 */
    char *output = cJSON_Print(root);
    cJSON_Delete(root);  /* 释放 JSON 对象 */
    return output;
}

/**
 * @brief 设置摄像头 JPEG 编码质量
 * 
 * 根据摄像头的像素格式，选择不同的质量设置方式：
 * - V4L2_PIX_FMT_JPEG：通过 V4L2 扩展控制设置硬件编码质量
 * - 其他格式：通过软件编码器 API 设置质量
 * 
 * 如果设置的质量超出传感器支持范围，会自动调整到有效范围内。
 * 
 * @param video 摄像头视频控制结构指针
 * @param quality 请求设置的 JPEG 质量值
 * @return ESP_OK 成功，其他错误码表示失败
 */
static esp_err_t set_camera_jpeg_quality(web_cam_video_t *video, int quality)
{
    esp_err_t ret = ESP_OK;
    int quality_reset = quality;

    if (video->pixel_format == V4L2_PIX_FMT_JPEG) {
        /* 硬件 JPEG 输出：使用 V4L2 扩展控制接口 */
        struct v4l2_ext_controls controls = {0};
        struct v4l2_ext_control control[1];
        struct v4l2_query_ext_ctrl qctrl = {0};

        qctrl.id = V4L2_CID_JPEG_COMPRESSION_QUALITY;

        /* 查询传感器支持的质量范围 */
        if (ioctl(video->fd, VIDIOC_QUERY_EXT_CTRL, &qctrl) == 0) {
            /* 检查质量值是否在有效范围内 */
            if ((quality > qctrl.maximum) || (quality < qctrl.minimum) ||
                    (((quality - qctrl.minimum) % qctrl.step) != 0)) {

                /* 超出范围时自动调整到有效边界 */
                if (quality > qctrl.maximum) {
                    quality_reset = qctrl.maximum;
                } else if (quality < qctrl.minimum) {
                    quality_reset = qctrl.minimum;
                } else {
                    quality_reset = qctrl.minimum + ((quality - qctrl.minimum) / qctrl.step) * qctrl.step;
                }

                ESP_LOGW(TAG, "video%d: JPEG compression quality=%d is out of sensor's range, reset to %d", video->index, quality, quality_reset);
            }

            /* 设置 V4L2 扩展控制参数 */
            controls.ctrl_class = V4L2_CID_JPEG_CLASS;
            controls.count = 1;
            controls.controls = control;
            control[0].id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
            control[0].value = quality_reset;

            /* 应用控制设置 */
            ESP_RETURN_ON_ERROR(ioctl(video->fd, VIDIOC_S_EXT_CTRLS, &controls), TAG, "failed to set jpeg compression quality");

            video->jpeg_quality = quality_reset;
            video->support_control_jpeg_quality = 1;
        } else {
            /* 传感器不支持质量控制 */
            video->support_control_jpeg_quality = 0;
            ESP_LOGW(TAG, "video%d: JPEG compression quality control is not supported", video->index);
        }
    } else {
        /* 软件编码：调用编码器 API 设置质量 */
        ESP_RETURN_ON_ERROR(example_encoder_set_jpeg_quality(video->encoder_handle, quality_reset), TAG, "failed to set jpeg quality");
        video->jpeg_quality = quality_reset;
    }

    /* 输出成功日志 */
    if (video->support_control_jpeg_quality) {
        ESP_LOGI(TAG, "video%d: set jpeg quality %d success", video->index, quality_reset);
    }

    return ret;
}

/**
 * @brief HTTP 请求处理器：获取摄像头信息
 * 
 * 响应前端的 GET /api/get_camera_info 请求，返回所有摄像头的状态信息 JSON。
 * 
 * @param req HTTP 请求结构指针
 * @return ESP_OK 成功，其他错误码表示失败
 */
static esp_err_t camera_info_handler(httpd_req_t *req)
{
    esp_err_t ret;
    web_cam_t *web_cam = (web_cam_t *)req->user_ctx;
    char *output = get_cameras_json(web_cam);

    /* 设置响应 Content-Type 为 JSON */
    httpd_resp_set_type(req, "application/json");
    ret = httpd_resp_sendstr(req, output);
    free(output);  /* 释放 JSON 字符串内存 */

    return ret;
}

/**
 * @brief HTTP 请求处理器：设置摄像头配置
 * 
 * 响应前端的 POST /api/set_camera_config 请求，解析 JSON 格式的配置参数，
 * 并应用到指定的摄像头。支持设置图像格式和 JPEG 质量。
 * 
 * @param req HTTP 请求结构指针
 * @return ESP_OK 成功，其他错误码表示失败
 */
static esp_err_t camera_settings_handler(httpd_req_t *req)
{
    esp_err_t ret;
    char *content;
    web_cam_t *web_cam = (web_cam_t *)req->user_ctx;

    /* 分配内存存储请求体 */
    content = (char *)calloc(1, req->content_len + 1);
    ESP_RETURN_ON_FALSE(content, ESP_ERR_NO_MEM, TAG, "failed to allocate memory");

    /* 读取请求体内容 */
    ESP_GOTO_ON_FALSE(httpd_req_recv(req, content, req->content_len) > 0, ESP_FAIL, fail0, TAG, "failed to recv content");
    ESP_LOGD(TAG, "content: %s", content);

    /* 解析 JSON 请求体 */
    cJSON *json_root = cJSON_Parse(content);
    free(content);  /* 释放请求体内存 */
    content = NULL;
    ESP_GOTO_ON_FALSE(json_root, ESP_FAIL, fail0, TAG, "failed to parse JSON");

    /* 提取并验证 index 字段 */
    cJSON *json_index = cJSON_GetObjectItem(json_root, "index");
    ESP_GOTO_ON_FALSE(json_index && cJSON_IsNumber(json_index), ESP_ERR_INVALID_ARG, fail1, TAG, "missing or invalid index field");
    int index = json_index->valueint;
    ESP_GOTO_ON_FALSE(index >= 0 && index < web_cam->video_count && is_valid_web_cam(&web_cam->video[index]), ESP_ERR_INVALID_ARG, fail1, TAG, "invalid index");

    /* 提取并验证 image_format 字段 */
    cJSON *json_image_format = cJSON_GetObjectItem(json_root, "image_format");
    ESP_GOTO_ON_FALSE(json_image_format && cJSON_IsNumber(json_image_format), ESP_ERR_INVALID_ARG, fail1, TAG, "missing or invalid image_format field");
    int image_format = json_image_format->valueint;

    /* 提取并验证 jpeg_quality 字段 */
    cJSON *json_jpeg_quality = cJSON_GetObjectItem(json_root, "jpeg_quality");
    ESP_GOTO_ON_FALSE(json_jpeg_quality && cJSON_IsNumber(json_jpeg_quality), ESP_ERR_INVALID_ARG, fail1, TAG, "missing or invalid jpeg_quality field");
    int jpeg_quality = json_jpeg_quality->valueint;

    ESP_LOGI(TAG, "JSON parse success - index:%d, image_format:%d, jpeg_quality:%d", index, image_format, jpeg_quality);
    cJSON_Delete(json_root);  /* 释放 JSON 对象 */
    json_root = NULL;

    /* 应用质量设置 */
    ESP_GOTO_ON_ERROR(set_camera_jpeg_quality(&web_cam->video[index], jpeg_quality), fail1, TAG, "failed to set camera jpeg quality");

    /* 返回成功响应 */
    httpd_resp_sendstr(req, "OK");
    return ESP_OK;

fail1:
    /* 错误处理：释放 JSON 对象 */
    if (json_root) {
        cJSON_Delete(json_root);
    }
fail0:
    /* 错误处理：返回错误响应 */
    if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
        httpd_resp_send_408(req);  /* 请求超时 */
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON format");  /* 请求格式错误 */
    }
    if (content) {
        free(content);  /* 释放请求体内存 */
    }
    return ret;
}

/**
 * @brief HTTP 请求处理器：静态文件服务
 * 
 * 根据请求 URI 返回对应的静态资源文件（HTML、CSS、JS、图片），
 * 所有资源均以 gzip 压缩格式存储在固件中，通过设置 Content-Encoding 头
 * 告知浏览器自行解压。
 * 
 * @param req HTTP 请求结构指针
 * @return ESP_OK 成功，ESP_FAIL 未找到对应文件
 */
static esp_err_t static_file_handler(httpd_req_t *req)
{
    const char *uri = req->uri;

    /* 根据 URI 路由到对应的静态文件 */
    if (strcmp(uri, "/") == 0) {
        /* 根路径返回 HTML 首页 */
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        return httpd_resp_send(req, (const char *)index_html_gz_start, index_html_gz_end - index_html_gz_start);
    } else if (strcmp(uri, "/loading.jpg") == 0) {
        /* 返回加载占位图片 */
        httpd_resp_set_type(req, "image/jpeg");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        return httpd_resp_send(req, (const char *)loading_jpg_gz_start, loading_jpg_gz_end - loading_jpg_gz_start);
    } else if (strcmp(uri, "/favicon.ico") == 0) {
        /* 返回网站图标 */
        httpd_resp_set_type(req, "image/x-icon");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        return httpd_resp_send(req, (const char *)favicon_ico_gz_start, favicon_ico_gz_end - favicon_ico_gz_start);
    } else if (strcmp(uri, "/assets/index.js") == 0) {
        /* 返回 JavaScript 文件 */
        httpd_resp_set_type(req, "application/javascript");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        return httpd_resp_send(req, (const char *)assets_index_js_gz_start, assets_index_js_gz_end - assets_index_js_gz_start);
    } else if (strcmp(uri, "/assets/index.css") == 0) {
        /* 返回 CSS 样式文件 */
        httpd_resp_set_type(req, "text/css");
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        return httpd_resp_send(req, (const char *)assets_index_css_gz_start, assets_index_css_gz_end - assets_index_css_gz_start);
    }

    /* 未匹配到任何静态文件，返回 404 */
    ESP_LOGW(TAG, "File not found: %s", uri);
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

/**
 * @brief HTTP 请求处理器：视频流推送
 * 
 * 响应 GET /stream 请求，以 MJPEG 格式持续推送视频流。
 * 使用 multipart/x-mixed-replace 协议，客户端无需重新连接即可接收连续帧。
 * 
 * @param req HTTP 请求结构指针
 * @return ESP_OK 连接正常关闭，其他错误码表示异常中断
 */
static esp_err_t image_stream_handler(httpd_req_t *req)
{
    esp_err_t ret;
    struct v4l2_buffer buf;
    char http_string[128];
    bool locked = false;
    web_cam_video_t *video = (web_cam_video_t *)req->user_ctx;

    /* 格式化帧率字符串 */
    ESP_RETURN_ON_FALSE(snprintf(http_string, sizeof(http_string), "%" PRIu32, video->frame_rate) > 0,
                        ESP_FAIL, TAG, "failed to format framerate buffer");

    /* 设置响应头 */
    ESP_RETURN_ON_ERROR(httpd_resp_set_type(req, STREAM_CONTENT_TYPE), TAG, "failed to set content type");
    ESP_RETURN_ON_ERROR(httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*"), TAG, "failed to set access control allow origin");
    ESP_RETURN_ON_ERROR(httpd_resp_set_hdr(req, "X-Framerate", http_string), TAG, "failed to set x framerate");

    /* 持续循环推送视频帧 */
    while (1) {
        int hlen;
        struct timespec ts;
        uint32_t jpeg_encoded_size;

        locked = false;

        /* 初始化 V4L2 缓冲区结构 */
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        /* 从 V4L2 设备出队一个缓冲区 */
        ESP_RETURN_ON_ERROR(ioctl(video->fd, VIDIOC_DQBUF, &buf), TAG, "failed to receive video frame");

        /* 检查缓冲区状态 */
        if (!(buf.flags & V4L2_BUF_FLAG_DONE)) {
            ESP_RETURN_ON_ERROR(ioctl(video->fd, VIDIOC_QBUF, &buf), TAG, "failed to queue video frame");
            continue;
        }

        /* 发送 multipart 分界符 */
        ESP_GOTO_ON_ERROR(httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY)), fail0, TAG, "failed to send boundary");

        if (video->pixel_format == V4L2_PIX_FMT_JPEG) {
            /* 硬件 JPEG 输出：直接使用缓冲区数据 */
            video->jpeg_out_buf = video->buffer[buf.index];
            jpeg_encoded_size = buf.bytesused;
        } else {
            /* 软件编码：获取编码器互斥锁 */
            ESP_GOTO_ON_FALSE(xSemaphoreTake(video->sem, portMAX_DELAY) == pdPASS, ESP_FAIL, fail0, TAG, "failed to take semaphore");
            locked = true;

            /* 调用编码器处理帧数据 */
            ESP_GOTO_ON_ERROR(example_encoder_process(video->encoder_handle, video->buffer[buf.index], video->buffer_size,
                              video->jpeg_out_buf, video->jpeg_out_size, &jpeg_encoded_size),
                              fail0, TAG, "failed to encode video frame");
        }

        /* 获取当前时间戳 */
        ESP_GOTO_ON_ERROR(clock_gettime(CLOCK_MONOTONIC, &ts), fail0, TAG, "failed to get time");

        /* 格式化 HTTP 头部 */
        ESP_GOTO_ON_FALSE((hlen = snprintf(http_string, sizeof(http_string), STREAM_PART, jpeg_encoded_size, ts.tv_sec, ts.tv_nsec)) > 0,
                          ESP_FAIL, fail0, TAG, "failed to format part buffer");

        /* 发送 HTTP 头部 */
        ESP_GOTO_ON_ERROR(httpd_resp_send_chunk(req, http_string, hlen), fail0, TAG, "failed to send boundary");

        /* 发送 JPEG 图像数据 */
        ESP_GOTO_ON_ERROR(httpd_resp_send_chunk(req, (char *)video->jpeg_out_buf, jpeg_encoded_size), fail0, TAG, "failed to send jpeg");

        /* 释放编码器互斥锁 */
        if (locked) {
            xSemaphoreGive(video->sem);
            locked = false;
        }

        /* 将缓冲区重新入队 */
        ESP_RETURN_ON_ERROR(ioctl(video->fd, VIDIOC_QBUF, &buf), TAG, "failed to queue video frame");
    }

    return ESP_OK;

fail0:
    /* 错误处理：确保释放互斥锁和重新入队缓冲区 */
    if (locked) {
        xSemaphoreGive(video->sem);
    }
    ioctl(video->fd, VIDIOC_QBUF, &buf);
    return ret;
}

/**
 * @brief HTTP 请求处理器：捕获单帧 JPEG 图像
 * 
 * 响应 GET /api/capture_image?source=N 请求，捕获指定摄像头的一帧图像，
 * 以 JPEG 格式返回给客户端供下载。
 * 
 * @param req HTTP 请求结构指针
 * @return ESP_OK 成功，其他错误码表示失败
 */
static esp_err_t capture_image_handler(httpd_req_t *req)
{
    web_cam_t *web_cam = (web_cam_t *)req->user_ctx;

    /* 解析请求中的摄像头索引 */
    request_desc_t desc;
    ESP_RETURN_ON_ERROR(decode_request(web_cam, req, &desc), TAG, "failed to decode request");

    /* 设置响应 Content-Type（包含文件名） */
    char type_ptr[32];
    ESP_RETURN_ON_FALSE(snprintf(type_ptr, sizeof(type_ptr), "image/jpeg;name=image%d.jpg", desc.index) > 0, ESP_FAIL, TAG, "failed to format buffer");
    ESP_RETURN_ON_ERROR(httpd_resp_set_type(req, type_ptr), TAG, "failed to set content type");

    /* 捕获并发送图像 */
    return capture_video_image(req, &web_cam->video[desc.index], true);
}

/**
 * @brief HTTP 请求处理器：捕获原始二进制帧
 * 
 * 响应 GET /api/capture_binary?source=N 请求，捕获指定摄像头的一帧原始数据，
 * 以二进制格式返回给客户端供下载。这对于调试和分析原始图像数据非常有用。
 * 
 * @param req HTTP 请求结构指针
 * @return ESP_OK 成功，其他错误码表示失败
 */
static esp_err_t capture_binary_handler(httpd_req_t *req)
{
    web_cam_t *web_cam = (web_cam_t *)req->user_ctx;

    /* 解析请求中的摄像头索引 */
    request_desc_t desc;
    ESP_RETURN_ON_ERROR(decode_request(web_cam, req, &desc), TAG, "failed to decode request");

    /* 设置响应 Content-Type（二进制流） */
    char type_ptr[56];
    ESP_RETURN_ON_FALSE(snprintf(type_ptr, sizeof(type_ptr), "application/octet-stream;name=image_binary%d.bin", desc.index) > 0, ESP_FAIL, TAG, "failed to format buffer");
    ESP_RETURN_ON_ERROR(httpd_resp_set_type(req, type_ptr), TAG, "failed to set content type");

    /* 捕获并发送原始数据 */
    return capture_video_image(req, &web_cam->video[desc.index], false);
}

/**
 * @brief 初始化单个摄像头视频设备
 * 
 * 完成以下初始化步骤：
 * 1. 打开 V4L2 设备文件
 * 2. 获取当前视频格式
 * 3. 获取帧率信息
 * 4. 配置 MIPI-CSI 裁剪区域（可选）
 * 5. 请求 V4L2 缓冲区
 * 6. 内存映射缓冲区
 * 7. 初始化编码器（如果需要软件编码）
 * 8. 创建互斥信号量
 * 
 * @param video 摄像头视频控制结构指针
 * @param config 初始化配置参数
 * @param index 摄像头索引
 * @return ESP_OK 成功，其他错误码表示失败
 */
static esp_err_t init_web_cam_video(web_cam_video_t *video, const web_cam_video_config_t *config, int index)
{
    int fd;
    int ret;
    struct v4l2_format format;
    struct v4l2_streamparm sparm;
    struct v4l2_requestbuffers req;
    struct v4l2_captureparm *cparam = &sparm.parm.capture;
    struct v4l2_fract *timeperframe = &cparam->timeperframe;

    /* 打开 V4L2 设备 */
    fd = open(config->dev_name, O_RDWR);
    ESP_RETURN_ON_FALSE(fd >= 0, ESP_ERR_NOT_FOUND, TAG, "Open video device %s failed", config->dev_name);

    /* 获取当前视频格式 */
    memset(&format, 0, sizeof(struct v4l2_format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ESP_GOTO_ON_ERROR(ioctl(fd, VIDIOC_G_FMT, &format), fail0, TAG, "Failed get fmt from %s", config->dev_name);

#if CONFIG_EXAMPLE_SELECT_JPEG_HW_DRIVER
    /* 硬件 JPEG 编码器特殊处理：RGB565 字节序转换 */
    if (format.fmt.pix.pixelformat == V4L2_PIX_FMT_RGB565X) {
#if CONFIG_ESP_VIDEO_ENABLE_SWAP_BYTE
        /* 启用字节交换功能，自动转换为小端序 */
        ESP_LOGW(TAG, "The hardware JPEG encoder does not support RGB565 big endian. Instead, use RGB565 little endian by enabling the byte swap function.");

        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
        ESP_GOTO_ON_ERROR(ioctl(fd, VIDIOC_S_FMT, &format), fail0, TAG, "failed to set fmt to %s", config->dev_name);
#else
        /* 未启用字节交换，报错退出 */
        ESP_GOTO_ON_ERROR(ESP_FAIL, fail0, TAG, "The hardware JPEG encoder does not support RGB565 big endian. Please enable the byte swap function ESP_VIDEO_ENABLE_SWAP_BYTE in menuconfig.");
#endif
    }
#endif

    /* 获取帧率参数 */
    memset(&sparm, 0, sizeof(sparm));
    sparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ESP_GOTO_ON_ERROR(ioctl(fd, VIDIOC_G_PARM, &sparm), fail0, TAG, "failed to get frame rate from %s", config->dev_name);

    /* 计算帧率：timeperframe = 1/frame_rate，所以 frame_rate = denominator/numerator */
    video->frame_rate = timeperframe->denominator / timeperframe->numerator;

#if CONFIG_EXAMPLE_ENABLE_MIPI_CSI_CROP
    /**
     * MIPI-CSI 裁剪配置：必须在 VIDIOC_REQBUFS 之前调用
     * 用于从原始传感器输出中裁剪指定区域，降低分辨率或感兴趣区域
     */
    struct v4l2_selection selection;

    memset(&selection, 0, sizeof(selection));
    selection.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    selection.target = V4L2_SEL_TGT_CROP;
    selection.r.left = CONFIG_EXAMPLE_MIPI_CSI_CROP_LEFT;
    selection.r.width = CONFIG_EXAMPLE_MIPI_CSI_CROP_WIDTH;
    selection.r.top = CONFIG_EXAMPLE_MIPI_CSI_CROP_TOP;
    selection.r.height = CONFIG_EXAMPLE_MIPI_CSI_CROP_HEIGHT;

    /* 尝试设置裁剪区域，失败仅记录日志不中断初始化 */
    if (ioctl(fd, VIDIOC_S_SELECTION, &selection) != 0) {
        ESP_LOGE(TAG, "failed to set selection");
    }
#endif

    /* 请求 V4L2 缓冲区 */
    memset(&req, 0, sizeof(req));
    req.count  = EXAMPLE_CAMERA_VIDEO_BUFFER_NUMBER;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;  /* 使用内存映射方式 */
    ESP_GOTO_ON_ERROR(ioctl(fd, VIDIOC_REQBUFS, &req), fail0, TAG, "failed to req buffers from %s", config->dev_name);

    /* 遍历所有缓冲区，进行内存映射 */
    for (int i = 0; i < EXAMPLE_CAMERA_VIDEO_BUFFER_NUMBER; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;

        /* 查询缓冲区信息 */
        ESP_GOTO_ON_ERROR(ioctl(fd, VIDIOC_QUERYBUF, &buf), fail0, TAG, "failed to query vbuf from %s", config->dev_name);

        /* 内存映射：将内核缓冲区映射到用户空间 */
        video->buffer[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        ESP_GOTO_ON_FALSE(video->buffer[i] != MAP_FAILED, ESP_ERR_NO_MEM, fail0, TAG, "failed to mmap buffer");
        video->buffer_size = buf.length;

        /* 将缓冲区入队，准备接收数据 */
        ESP_GOTO_ON_ERROR(ioctl(fd, VIDIOC_QBUF, &buf), fail0, TAG, "failed to queue frame vbuf from %s", config->dev_name);
    }

    /* 保存设备信息到视频控制结构 */
    video->fd = fd;
    video->width = format.fmt.pix.width;
    video->height = format.fmt.pix.height;
    video->pixel_format = format.fmt.pix.pixelformat;
    video->jpeg_quality = EXAMPLE_JPEG_ENC_QUALITY;

    if (video->pixel_format == V4L2_PIX_FMT_JPEG) {
        /* 硬件 JPEG 输出：设置初始质量 */
        ESP_GOTO_ON_ERROR(set_camera_jpeg_quality(video, EXAMPLE_JPEG_ENC_QUALITY), fail0, TAG, "failed to set jpeg quality");
    } else {
        /* 软件编码：初始化编码器 */
        example_encoder_config_t encoder_config = {0};

        encoder_config.width = video->width;
        encoder_config.height = video->height;
        encoder_config.pixel_format = video->pixel_format;
        encoder_config.quality = EXAMPLE_JPEG_ENC_QUALITY;

        /* 创建编码器实例 */
        ESP_GOTO_ON_ERROR(example_encoder_init(&encoder_config, &video->encoder_handle), fail0, TAG, "failed to init encoder");

        /* 分配 JPEG 输出缓冲区 */
        ESP_GOTO_ON_ERROR(example_encoder_alloc_output_buffer(video->encoder_handle, &video->jpeg_out_buf, &video->jpeg_out_size),
                          fail1, TAG, "failed to alloc jpeg output buf");

        video->support_control_jpeg_quality = 1;
    }

    /* 创建编码器互斥信号量（二进制信号量） */
    video->sem = xSemaphoreCreateBinary();
    ESP_GOTO_ON_FALSE(video->sem, ESP_ERR_NO_MEM, fail2, TAG, "failed to create semaphore");
    xSemaphoreGive(video->sem);  /* 初始化为可用状态 */

    return ESP_OK;

fail2:
    /* 清理步骤 2：释放编码器输出缓冲区 */
    if (video->pixel_format != V4L2_PIX_FMT_JPEG) {
        example_encoder_free_output_buffer(video->encoder_handle, video->jpeg_out_buf);
        video->jpeg_out_buf = NULL;
    }
fail1:
    /* 清理步骤 1：反初始化编码器 */
    if (video->pixel_format != V4L2_PIX_FMT_JPEG) {
        example_encoder_deinit(video->encoder_handle);
        video->encoder_handle = NULL;
    }
fail0:
    /* 清理步骤 0：关闭设备 */
    close(fd);
    video->fd = -1;
    return ret;
}

/**
 * @brief 反初始化单个摄像头视频设备
 * 
 * 释放摄像头相关资源：信号量、编码器、设备文件。
 * 
 * @param video 摄像头视频控制结构指针
 * @return ESP_OK 成功
 */
static esp_err_t deinit_web_cam_video(web_cam_video_t *video)
{
    /* 删除信号量 */
    if (video->sem) {
        vSemaphoreDelete(video->sem);
        video->sem = NULL;
    }

    /* 释放编码器资源（软件编码时） */
    if (video->pixel_format != V4L2_PIX_FMT_JPEG) {
        example_encoder_free_output_buffer(video->encoder_handle, video->jpeg_out_buf);
        example_encoder_deinit(video->encoder_handle);
    }

    /* 关闭设备文件 */
    close(video->fd);
    return ESP_OK;
}

/**
 * @brief 创建并初始化多摄像头设备容器
 * 
 * 分配摄像头容器内存，依次初始化每个摄像头设备，最后启动视频流。
 * 支持部分摄像头初始化失败（会跳过并记录警告），但至少需要一个摄像头成功。
 * 
 * @param config 摄像头配置数组
 * @param config_count 配置数量
 * @param ret_wc 输出参数，返回创建的摄像头容器指针
 * @return ESP_OK 成功，其他错误码表示失败
 */
static esp_err_t new_web_cam(const web_cam_video_config_t *config, int config_count, web_cam_t **ret_wc)
{
    int i;
    web_cam_t *wc;
    esp_err_t ret = ESP_FAIL;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    /* 分配摄像头容器内存（包含柔性数组） */
    wc = calloc(1, sizeof(web_cam_t) + config_count * sizeof(web_cam_video_t));
    ESP_RETURN_ON_FALSE(wc, ESP_ERR_NO_MEM, TAG, "failed to alloc web cam");
    wc->video_count = config_count;

    /* 初始化每个摄像头 */
    for (i = 0; i < config_count; i++) {
        wc->video[i].index = i;
        wc->video[i].fd = -1;  /* 初始化为无效状态 */

        ret = init_web_cam_video(&wc->video[i], &config[i], i);
        if (ret == ESP_ERR_NOT_FOUND) {
            /* 设备未找到：跳过，继续尝试其他设备 */
            ESP_LOGW(TAG, "failed to find web_cam %d", i);
            continue;
        } else if (ret != ESP_OK) {
            /* 其他错误：终止初始化 */
            ESP_LOGE(TAG, "failed to initialize web_cam %d", i);
            goto fail0;
        }

        /* 输出摄像头信息日志 */
        ESP_LOGI(TAG, "video%d: width=%" PRIu32 " height=%" PRIu32 " format=" V4L2_FMT_STR, i, wc->video[i].width,
                 wc->video[i].height, V4L2_FMT_STR_ARG(wc->video[i].pixel_format));
    }

    /* 启动所有有效摄像头的视频流 */
    for (i = 0; i < config_count; i++) {
        if (is_valid_web_cam(&wc->video[i])) {
            ESP_GOTO_ON_ERROR(ioctl(wc->video[i].fd, VIDIOC_STREAMON, &type), fail1, TAG, "failed to start stream");
        }
    }

    *ret_wc = wc;

    return ESP_OK;

fail1:
    /* 错误处理：停止已启动的视频流 */
    for (int j = i - 1; j >= 0; j--) {
        if (is_valid_web_cam(&wc->video[j])) {
            ioctl(wc->video[j].fd, VIDIOC_STREAMOFF, &type);
        }
    }
    i = config_count;  /* 设置为全部清理 */
fail0:
    /* 错误处理：反初始化所有已初始化的摄像头 */
    for (int j = i - 1; j >= 0; j--) {
        if (is_valid_web_cam(&wc->video[j])) {
            deinit_web_cam_video(&wc->video[j]);
        }
    }
    free(wc);  /* 释放容器内存 */
    return ret;
}

/**
 * @brief 释放多摄像头设备容器
 * 
 * 依次反初始化每个摄像头，然后释放容器内存。
 * 
 * @param web_cam 摄像头容器结构指针
 */
static void free_web_cam(web_cam_t *web_cam)
{
    /* 反初始化每个摄像头 */
    for (int i = 0; i < web_cam->video_count; i++) {
        if (is_valid_web_cam(&web_cam->video[i])) {
            deinit_web_cam_video(&web_cam->video[i]);
        }
    }
    /* 释放容器内存 */
    free(web_cam);
}

/**
 * @brief 初始化 HTTP 服务器
 * 
 * 创建 HTTP 服务器实例，注册以下 URI 处理器：
 * - /*: 静态文件服务（HTML、CSS、JS、图片）
 * - /api/get_camera_info: 获取摄像头信息
 * - /api/set_camera_config: 设置摄像头配置
 * - /api/capture_image: 捕获单帧 JPEG 图像
 * - /api/capture_binary: 捕获原始二进制帧
 * - /stream: 视频流推送（每个摄像头单独一个端口）
 * 
 * 每个摄像头使用独立的 HTTP 服务器实例和端口，端口从默认端口开始递增。
 * 
 * @param web_cam 摄像头容器结构指针
 * @return ESP_OK 成功
 */
static esp_err_t http_server_init(web_cam_t *web_cam)
{
    httpd_handle_t stream_httpd;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;  /* 启用通配符 URI 匹配 */

    /* 静态文件处理器：匹配所有未被其他处理器匹配的请求 */
    httpd_uri_t static_file_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = static_file_handler,
        .user_ctx = (void *)web_cam
    };

    /* API 处理器：获取摄像头信息 */
    httpd_uri_t capture_image_uri = {
        .uri = "/api/capture_image",
        .method = HTTP_GET,
        .handler = capture_image_handler,
        .user_ctx = (void *)web_cam
    };

    /* API 处理器：捕获原始二进制帧 */
    httpd_uri_t capture_binary_uri = {
        .uri = "/api/capture_binary",
        .method = HTTP_GET,
        .handler = capture_binary_handler,
        .user_ctx = (void *)web_cam
    };

    /* API 处理器：获取摄像头信息 */
    httpd_uri_t camera_info_uri = {
        .uri = "/api/get_camera_info",
        .method = HTTP_GET,
        .handler = camera_info_handler,
        .user_ctx = (void *)web_cam
    };

    /* API 处理器：设置摄像头配置 */
    httpd_uri_t camera_settings_uri = {
        .uri = "/api/set_camera_config",
        .method = HTTP_POST,
        .handler = camera_settings_handler,
        .user_ctx = (void *)web_cam
    };

    /* 设置 HTTP 服务器堆栈大小 */
    config.stack_size = 1024 * 6;
    ESP_LOGI(TAG, "Starting stream server on port: '%d'", config.server_port);

    /* 启动主 HTTP 服务器（端口 80） */
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        /* 先注册更具体的 API 处理器（优先级更高） */
        httpd_register_uri_handler(stream_httpd, &capture_image_uri);
        httpd_register_uri_handler(stream_httpd, &capture_binary_uri);
        httpd_register_uri_handler(stream_httpd, &camera_info_uri);
        httpd_register_uri_handler(stream_httpd, &camera_settings_uri);

        /* 最后注册通配符静态文件处理器 */
        httpd_register_uri_handler(stream_httpd, &static_file_uri);
    }

    /* 为每个摄像头创建独立的视频流服务器 */
    for (int i = 0; i < web_cam->video_count; i++) {
        if (!is_valid_web_cam(&web_cam->video[i])) {
            continue;
        }

        /* 视频流处理器配置 */
        httpd_uri_t stream_0_uri = {
            .uri = "/stream",
            .method = HTTP_GET,
            .handler = image_stream_handler,
            .user_ctx = (void *) &web_cam->video[i]  /* 传递对应摄像头的控制结构 */
        };

        config.stack_size = 1024 * 6;
        config.server_port += 1;  /* 端口递增：81, 82, ... */
        config.ctrl_port += 1;

        /* 启动视频流服务器 */
        if (httpd_start(&stream_httpd, &config) == ESP_OK) {
            httpd_register_uri_handler(stream_httpd, &stream_0_uri);
        }
    }

    return ESP_OK;
}

/**
 * @brief 启动摄像头 Web 服务器
 * 
 * 组合摄像头初始化和 HTTP 服务器初始化的高层接口函数。
 * 
 * @param config 摄像头配置数组
 * @param config_count 配置数量
 * @return ESP_OK 成功，其他错误码表示失败
 */
static esp_err_t start_cam_web_server(const web_cam_video_config_t *config, int config_count)
{
    esp_err_t ret;
    web_cam_t *web_cam;

    /* 初始化摄像头设备 */
    ESP_RETURN_ON_ERROR(new_web_cam(config, config_count, &web_cam), TAG, "Failed to new web cam");

    /* 初始化 HTTP 服务器 */
    ESP_GOTO_ON_ERROR(http_server_init(web_cam), fail0, TAG, "Failed to init http server");

    return ESP_OK;

fail0:
    /* 错误处理：释放摄像头资源 */
    free_web_cam(web_cam);
    return ret;
}

/**
 * @brief 初始化 mDNS 服务
 * 
 * 配置 mDNS 主机名、实例名，并注册 HTTP 服务，使设备可通过
 * http://<hostname>.local 在局域网中被发现。
 */
static void initialise_mdns(void)
{
    /* 初始化 mDNS 服务 */
    ESP_ERROR_CHECK(mdns_init());

    /* 设置主机名 */
    ESP_ERROR_CHECK(mdns_hostname_set(EXAMPLE_MDNS_HOST_NAME));

    /* 设置实例名 */
    ESP_ERROR_CHECK(mdns_instance_name_set(EXAMPLE_MDNS_INSTANCE));

    /* 服务文本记录（附加信息） */
    mdns_txt_item_t serviceTxtData[] = {
        {"board", CONFIG_IDF_TARGET},  /* 目标芯片型号 */
        {"path", "/"}                   /* 服务路径 */
    };

    /* 注册 HTTP 服务 */
    ESP_ERROR_CHECK(mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, serviceTxtData,
                                     sizeof(serviceTxtData) / sizeof(serviceTxtData[0])));
}

/**
 * @brief 应用程序主入口函数
 * 
 * 执行以下初始化流程：
 * 1. 初始化 NVS Flash（用于存储配置）
 * 2. 初始化视频系统（摄像头驱动）
 * 3. 初始化网络接口和事件循环
 * 4. 初始化 mDNS 和 NetBIOS 服务
 * 5. 连接到 Wi-Fi 或以太网
 * 6. 根据配置创建摄像头配置数组
 * 7. 启动摄像头 Web 服务器
 * 
 * @note 对于需要主机提供 XCLK 时钟的摄像头设备，video_init() 必须在设备重启后立即调用，
 *       否则摄像头可能因缺少主时钟而无法启动。
 */
void app_main(void)
{
    esp_err_t ret = nvs_flash_init();

    /* NVS Flash 初始化失败处理 */
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* NVS 分区被截断或版本不匹配，需要擦除后重新初始化 */
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /**
     * 初始化视频系统
     * 
     * 对于需要主机提供 XCLK 时钟的摄像头设备，必须在设备重启后立即调用此函数，
     * 否则摄像头设备可能因缺少主时钟而无法启动。
     */
    ESP_ERROR_CHECK(example_video_init());

    /* 初始化网络接口 */
    ESP_ERROR_CHECK(esp_netif_init());

    /* 创建默认事件循环 */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* 初始化 mDNS 服务 */
    initialise_mdns();

    /* 初始化 NetBIOS 服务 */
    netbiosns_init();
    netbiosns_set_name(EXAMPLE_MDNS_HOST_NAME);

    /**
     * 连接到网络
     * 
     * 此辅助函数根据 menuconfig 中的配置自动选择 Wi-Fi 或以太网连接。
     * 详细说明请参考 examples/protocols/README.md 中的 "Establishing Wi-Fi or Ethernet Connection" 章节。
     */
    ESP_ERROR_CHECK(example_connect());

    /* 创建摄像头配置数组（根据编译配置选择启用的接口） */
    web_cam_video_config_t config[] = {
#if EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR
        {
            .dev_name = ESP_VIDEO_MIPI_CSI_DEVICE_NAME,  /* MIPI-CSI 设备路径 */
        },
#endif /* EXAMPLE_ENABLE_MIPI_CSI_CAM_SENSOR */
#if EXAMPLE_ENABLE_DVP_CAM_SENSOR
        {
            .dev_name = ESP_VIDEO_DVP_DEVICE_NAME,       /* DVP 设备路径 */
        },
#endif /* EXAMPLE_ENABLE_DVP_CAM_SENSOR */
#if EXAMPLE_ENABLE_SPI_CAM_0_SENSOR
        {
            .dev_name = ESP_VIDEO_SPI_DEVICE_NAME,       /* SPI 摄像头 0 设备路径 */
        },
#endif /* EXAMPLE_ENABLE_SPI_CAM_0_SENSOR */
#if EXAMPLE_ENABLE_SPI_CAM_1_SENSOR
        {
            .dev_name = ESP_VIDEO_SPI_DEVICE_1_NAME,     /* SPI 摄像头 1 设备路径 */
        },
#endif /* EXAMPLE_ENABLE_SPI_CAM_1_SENSOR */
#if EXAMPLE_ENABLE_USB_UVC_CAM_SENSOR
        {
            .dev_name = ESP_VIDEO_USB_UVC_DEVICE_NAME(0), /* USB UVC 设备路径 */
        },
#endif /* EXAMPLE_ENABLE_USB_UVC_CAM_SENSOR */
    };

    /* 计算配置数量 */
    int config_count = sizeof(config) / sizeof(config[0]);

    /* 断言至少有一个摄像头配置 */
    assert(config_count > 0);

    /* 启动摄像头 Web 服务器 */
    ESP_ERROR_CHECK(start_cam_web_server(config, config_count));

    /* 输出启动成功日志 */
    ESP_LOGI(TAG, "Camera web server starts");
}