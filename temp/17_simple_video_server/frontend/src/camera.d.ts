/**
 * @file camera.d.ts
 * @brief 摄像头相关 TypeScript 类型定义
 *
 * 本文件定义了前端应用中使用的摄像头相关数据类型，包括：
 * 1. QualityRange - 画质范围类型
 * 2. ImageFormat - 图像格式类型
 * 3. Resolution - 分辨率类型
 * 4. Camera - 摄像头配置类型
 *
 * 这些类型用于类型安全的状态管理和 API 数据解析。
 */

/**
 * @interface QualityRange
 * @brief 画质范围类型
 *
 * 定义图像画质的可选范围，包括最小值、最大值、步长和默认值。
 */
export type QualityRange = {
  /** 画质最小值 */
  min: number;
  /** 画质最大值 */
  max: number;
  /** 调节步长，默认为 1 */
  step?: number;
  /** 默认画质值 */
  default?: number;
};

/**
 * @interface ImageFormat
 * @brief 图像格式类型
 *
 * 定义摄像头支持的图像格式，包括格式 ID、描述和画质范围（可选）。
 */
export type ImageFormat = {
  /** 图像格式唯一标识 */
  id: string | number;
  /** 图像格式描述（如 "RGB 5-6-5 640x480"） */
  description: string;
  /** 画质范围配置，仅当格式支持画质调节时存在 */
  quality?: QualityRange;
};

/**
 * @interface Resolution
 * @brief 分辨率类型
 *
 * 定义图像的宽度和高度（像素）。
 */
export type Resolution = {
  /** 图像宽度（像素） */
  width: number;
  /** 图像高度（像素） */
  height: number;
};

/**
 * @interface Camera
 * @brief 摄像头配置类型
 *
 * 定义单个摄像头的完整配置信息，包括索引、名称、视频流地址、
 * 当前设置（帧率、图像格式、画质、分辨率）以及支持的图像格式列表。
 */
export type Camera = {
  /** 摄像头索引（唯一标识） */
  index: string | number;
  /** 摄像头名称（可选） */
  name?: string;
  /** 视频流地址（相对路径或端口路径） */
  src: string;
  /** 当前帧率（fps） */
  currentFrameRate: number;
  /** 当前图像格式 ID */
  currentImageFormat: number | string;
  /** 当前图像格式描述（可选） */
  currentImageFormatDescription?: string;
  /** 当前画质值（可选） */
  currentQuality?: number;
  /** 当前分辨率 */
  currentResolution: Resolution;
  /** 支持的图像格式列表 */
  imageFormats: ImageFormat[];
};
