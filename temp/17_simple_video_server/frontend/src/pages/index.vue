<template>
  <!-- 页面主容器，最大宽度限制为 600px -->
  <v-container max-width="600">
    <!-- 遍历所有摄像头，为每个摄像头渲染一个 CameraCard 组件 -->
    <CameraCard 
      v-for="(item, index) of mainStore.clientCameras" 
      :key="index" 
      :cam-num="index" 
    />
  </v-container>

  <!-- 网络请求错误弹窗，持久显示直到用户刷新页面 -->
  <v-dialog v-model="webRequestErr" persistent>
    <div class="mx-auto my-auto text-center font-large">
      Web Request Error <br>
      Please Refresh This Page
    </div>
  </v-dialog>
</template>

<script lang="ts" setup>
/**
 * @file pages/index.vue
 * @brief 首页页面组件
 *
 * 本页面是视频服务器前端的主页面，负责展示所有摄像头的实时画面和控制界面。
 * 主要功能包括：
 * 1. 展示所有可用摄像头的视频流
 * 2. 在页面加载时获取摄像头状态信息
 * 3. 处理网络请求错误并显示提示弹窗
 *
 * 组件依赖：
 * - CameraCard: 摄像头卡片组件，展示单个摄像头的视频流和控制按钮
 * - useMainStore: Pinia 状态管理，存储摄像头信息和网络状态
 */

// 导入 Vue 的 ref 响应式 API
import { ref } from 'vue';

// 导入 Pinia 状态管理 store
import { useMainStore } from '@/store/mainstore';
// 导入摄像头卡片组件
import CameraCard from '@/components/CameraCard.vue';

// 创建状态管理实例
const mainStore = useMainStore()

/**
 * @var webRequestErr
 * @brief 网络请求错误标志
 *
 * 当 API 请求失败时设置为 true，触发错误弹窗显示。
 */
const webRequestErr = ref<boolean>(false)

/**
 * @function onBeforeMount
 * @brief 组件挂载前钩子
 *
 * 在组件挂载到 DOM 之前调用，初始化摄像头状态信息。
 * 调用 mainStore.updateCameraStatus() 获取所有摄像头的配置信息。
 */
onBeforeMount(() => {
  mainStore.updateCameraStatus();
})
</script>

<style lang="css" scoped>
/**
 * 居中对齐样式
 */
.text-center {
  text-align: center;
}

/**
 * 大号字体样式
 */
.font-large {
  font-size: large;
}
</style>
