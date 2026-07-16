/**
 * @file mock/es.mock.ts
 * @brief 前端 Mock 数据和 API 模拟
 *
 * 本文件提供前端开发时的 Mock 数据和 API 模拟，用于在没有后端的情况下
 * 测试前端功能。包含以下 Mock API：
 * 1. GET /api/get_camera_info - 获取摄像头信息
 * 2. GET /api/capture_image - 捕获图像（JPEG）
 * 3. GET /api/capture_binary - 捕获原始图像（BIN）
 * 4. POST /api/set_camera_config - 设置摄像头配置
 *
 * Mock 数据包含 3 个模拟摄像头，每个摄像头支持多种图像格式。
 */

// 导入 Mock 服务器类型定义
import type { MockHandler } from "vite-plugin-mock-server";
// 导入摄像头类型定义
import type { Camera } from "../src/camera";

/**
 * @constant mockCameras
 * @brief Mock 摄像头配置数组
 *
 * 包含 3 个模拟摄像头的完整配置信息，用于前端开发测试。
 */
const mockCameras: Camera[] = [
  {
    index: 0,
    name: "Front Camera",
    src: "https://picsum.photos/640/480",
    currentFrameRate: 30,
    currentImageFormat: 1,
    currentImageFormatDescription: "RGB 5-6-5 640x480",
    currentQuality: 85,
    currentResolution: {
      width: 640,
      height: 480,
    },
    imageFormats: [
      {
        id: 1,
        description: "RGB 5-6-5 640x480",
        quality: {
          min: 50,
          max: 100,
          step: 5,
          default: 85,
        },
      },
      {
        id: 2,
        description: "RGB 8-8-8 640x480",
        quality: {
          min: 80,
          max: 100,
          step: 1,
          default: 95,
        },
      },
    ],
  },
  {
    index: 1,
    name: "Back Camera",
    src: "https://picsum.photos/1920/1080",
    currentFrameRate: 25,
    currentImageFormat: 1,
    currentImageFormatDescription: "RGB 5-6-5 1920x1080",
    currentQuality: 90,
    currentResolution: {
      width: 1920,
      height: 1080,
    },
    imageFormats: [
      {
        id: 1,
        description: "RGB 5-6-5 1920x1080",
        quality: {
          min: 50,
          max: 100,
          step: 5,
          default: 90,
        },
      },
      {
        id: 2,
        description: "RGB 8-8-8 1920x1080",
        quality: {
          min: 80,
          max: 100,
          step: 1,
          default: 95,
        },
      },
      {
        id: 3,
        description: "RGB 8-8-8 1080x720",
        quality: {
          min: 60,
          max: 100,
          step: 10,
          default: 80,
        },
      },
    ],
  },
  {
    index: 2,
    name: "Side Camera",
    src: "https://picsum.photos/800/600",
    currentFrameRate: 15,
    currentImageFormat: 2,
    currentImageFormatDescription: "RGB 8-8-8 800x600",
    currentQuality: 95,
    currentResolution: {
      width: 800,
      height: 600,
    },
    imageFormats: [
      {
        id: 1,
        description: "RGB 8-8-8 800x600",
        quality: {
          min: 50,
          max: 100,
          step: 5,
          default: 85,
        },
      },
      {
        id: 2,
        description: "YUV 4-2-2 800x600",
        quality: {
          min: 80,
          max: 100,
          step: 1,
          default: 95,
        },
      },
    ],
  },
];

/**
 * @function updateCameraSrc
 * @brief 更新摄像头视频流地址
 *
 * 根据摄像头的当前分辨率更新视频流地址，用于模拟配置变更后的效果。
 *
 * @param camera 摄像头配置对象
 */
const updateCameraSrc = (camera: Camera) => {
  const { width, height } = camera.currentResolution;
  camera.src = `https://picsum.photos/${width}/${height}`;
};

/**
 * @export default
 * @brief 创建 Mock API 处理器数组
 *
 * 返回一个包含所有 Mock API 处理器的数组，用于 vite-plugin-mock-server。
 *
 * @returns MockHandler[] Mock API 处理器数组
 */
export default (): MockHandler[] => [
  /**
   * @mock GET /api/get_camera_info
   * @brief 获取摄像头信息
   *
   * 返回所有 Mock 摄像头的配置信息。
   */
  {
    pattern: "/api/get_camera_info",
    method: "GET",
    handle: (req, res) => {
      res.setHeader("Content-Type", "application/json");
      res.end(
        JSON.stringify({
          cameras: mockCameras,
        })
      );
    },
  },

  /**
   * @mock GET /api/capture_image
   * @brief 捕获图像（JPEG）
   *
   * 根据 source 参数指定的摄像头索引，重定向到对应分辨率的随机图片。
   *
   * @param source 摄像头索引
   */
  {
    pattern: "/api/capture_image",
    method: "GET",
    handle: (req, res) => {
      const url = new URL(req.url!, `http://${req.headers.host}`);
      const source = url.searchParams.get("source");

      // 参数校验
      if (!source) {
        res.statusCode = 400;
        res.end("Missing source parameter");
        return;
      }

      // 查找对应的摄像头
      const camera = mockCameras.find((cam) => cam.index.toString() === source);
      if (!camera) {
        res.statusCode = 404;
        res.end("Camera not found");
        return;
      }

      // 重定向到随机图片
      res.statusCode = 302;
      res.setHeader(
        "Location",
        `https://picsum.photos/${camera.currentResolution.width}/${camera.currentResolution.height}`
      );
      res.end();
    },
  },

  /**
   * @mock GET /api/capture_binary
   * @brief 捕获原始图像（BIN）
   *
   * 根据 source 参数指定的摄像头索引，返回模拟的二进制数据。
   *
   * @param source 摄像头索引
   */
  {
    pattern: "/api/capture_binary",
    method: "GET",
    handle: (req, res) => {
      const url = new URL(req.url!, `http://${req.headers.host}`);
      const source = url.searchParams.get("source");

      // 参数校验
      if (!source) {
        res.statusCode = 400;
        res.end("Missing source parameter");
        return;
      }

      // 查找对应的摄像头
      const camera = mockCameras.find((cam) => cam.index.toString() === source);
      if (!camera) {
        res.statusCode = 404;
        res.end("Camera not found");
        return;
      }

      // 返回模拟的二进制数据
      const mockBinaryData = Buffer.from(
        `Mock binary data for camera ${source}`,
        "utf-8"
      );
      res.setHeader("Content-Type", "application/octet-stream");
      res.setHeader(
        "Content-Disposition",
        `attachment; filename="camera_${source}_raw.bin"`
      );
      res.end(mockBinaryData);
    },
  },

  /**
   * @mock POST /api/set_camera_config
   * @brief 设置摄像头配置
   *
   * 根据请求体中的配置信息更新指定摄像头的设置。
   *
   * 请求体格式：
   * {
   *   "index": number,          // 摄像头索引
   *   "image_format": number,   // 图像格式 ID
   *   "jpeg_quality": number    // JPEG 画质值
   * }
   */
  {
    pattern: "/api/set_camera_config",
    method: "POST",
    handle: (req, res) => {
      let body = "";
      
      // 读取请求体
      req.on("data", (chunk) => {
        body += chunk.toString();
      });

      // 请求体读取完成后处理
      req.on("end", () => {
        try {
          // 解析 JSON 请求体
          const config = JSON.parse(body);
          const { index, image_format, jpeg_quality } = config;

          // 查找对应的摄像头
          const camera = mockCameras.find(
            (cam) => cam.index.toString() === index.toString()
          );
          if (!camera) {
            res.statusCode = 404;
            res.setHeader("Content-Type", "application/json");
            res.end(JSON.stringify({ error: "Camera not found" }));
            return;
          }

          // 更新图像格式（如果提供）
          if (image_format !== undefined) {
            camera.currentImageFormat = image_format;
            const format = camera.imageFormats.find(
              (f) => f.id === image_format
            );
            if (format) {
              camera.currentImageFormatDescription = format.description;
            }
          }

          // 更新画质（如果提供）
          if (jpeg_quality !== undefined) {
            camera.currentQuality = jpeg_quality;
          }

          // 更新视频流地址
          updateCameraSrc(camera);

          // 返回成功响应
          res.setHeader("Content-Type", "application/json");
          res.end(
            JSON.stringify({
              success: true,
              message: "Camera configuration updated successfully",
            })
          );
        } catch (error) {
          // 处理 JSON 解析错误
          console.error(error);
          res.statusCode = 400;
          res.setHeader("Content-Type", "application/json");
          res.end(JSON.stringify({ error: "Invalid JSON in request body" }));
        }
      });
    },
  },
];
