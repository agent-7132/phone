/**
 * @file store/mainstore.ts
 * @brief Pinia 状态管理 Store
 *
 * 本文件定义了视频服务器前端的核心状态管理，负责：
 * 1. 存储所有摄像头的配置信息
 * 2. 跟踪网络请求状态
 * 3. 定期更新摄像头状态（每 3 秒）
 *
 * Store 结构：
 * - clientCameras: 摄像头配置数组
 * - netRequestError: 网络请求错误标志
 * - updateCameraStatus: 更新摄像头状态的方法
 */

// 导入 Pinia 的 defineStore 函数
import { defineStore } from "pinia";
// 导入 Vue 的 ref 响应式 API
import { ref } from "vue";
// 导入摄像头类型定义
import type { Camera } from "@/camera";

/**
 * @function useMainStore
 * @brief 创建 Pinia Store 实例
 *
 * 使用 Pinia 的 setup 语法创建状态管理，包含响应式状态和操作方法。
 */
export const useMainStore = defineStore("main", () => {
  /**
   * @var clientCameras
   * @brief 摄像头配置数组
   *
   * 存储所有可用摄像头的配置信息，包括名称、分辨率、帧率、图像格式等。
   */
  const clientCameras = ref<Camera[]>([]);

  /**
   * @var netRequestError
   * @brief 网络请求错误标志
   *
   * 当 API 请求失败时设置为 true，用于触发错误提示。
   */
  const netRequestError = ref<boolean>(false);

  /**
   * @var updateIntervalId
   * @brief 定期更新定时器 ID
   *
   * 用于每 3 秒自动更新摄像头状态的定时器。
   */
  let updateIntervalId: ReturnType<typeof setInterval> | null = null;

  /**
   * @function updateCameraStatus
   * @brief 更新摄像头状态
   *
   * 从后端 API 获取所有摄像头的配置信息，并更新本地状态。
   * 该方法会清除之前的定时器，重新设置 3 秒间隔的自动更新。
   *
   * @returns Promise<void>
   */
  const updateCameraStatus = async () => {
    // 清除之前的定时器，避免重复更新
    if (updateIntervalId) clearInterval(updateIntervalId);
    updateIntervalId = null;

    try {
      // 发送 GET 请求获取摄像头信息
      const response = await fetch("/api/get_camera_info");
      // 解析 JSON 响应
      const data: { cameras: Camera[] } = await response.json();
      
      // 更新摄像头数组长度
      clientCameras.value.length = data.cameras.length;

      // 逐个更新摄像头配置
      for (const camNum in data.cameras) {
        const camIndex = Number(camNum);
        clientCameras.value[camIndex] = data.cameras[camIndex];
      }
    } catch (e) {
      // 请求失败时记录错误并设置错误标志
      console.error(e);
      netRequestError.value = true;
    } finally {
      // 设置 3 秒间隔的自动更新定时器
      updateIntervalId = setInterval(updateCameraStatus, 3000);
    }
  }

  /**
   * @returns 返回 Store 的公开状态和方法
   */
  return {
    clientCameras,       // 摄像头配置数组
    netRequestError,     // 网络请求错误标志
    updateCameraStatus,  // 更新摄像头状态方法
  };
});
