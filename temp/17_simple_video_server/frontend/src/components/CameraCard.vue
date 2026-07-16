<template>
  <!-- 摄像头卡片容器，底部间距 3 -->
  <v-card class="mb-3">
    <!-- 视频图像显示区域 -->
    <v-img 
      :src="imgSrc" 
      :aspect-ratio="camera.currentResolution.width / camera.currentResolution.height" 
      cover
      :id="`cam-${props.camNum}-v-img`" 
      crossorigin="anonymous" 
      @error="onImageError"
    >
      <!-- 图片加载中占位符 -->
      <template #placeholder>
        <div class="d-flex align-center justify-center fill-height">
          <v-progress-circular color="grey-lighten-4" indeterminate />
        </div>
      </template>
      <!-- 图片加载错误提示 -->
      <template #error>
        <div class="d-flex align-center justify-center fill-height">
          <div style="height: 100px;" class="d-flex flex-column">
            <div class="my-auto" style="font-size: larger; font-weight: bold;">
              Something went wrong
            </div>
            <div class="mx-auto" style="font-size: smaller; opacity: 70%;">
              Retrying in 3 seconds...
            </div>
          </div>
        </div>
      </template>
    </v-img>
    <!-- 摄像头信息和控制按钮区域 -->
    <div class="d-flex justify-space-between align-center my-2 mx-3">
      <!-- 摄像头信息 -->
      <div>
        <div style="font-weight: bold; font-size: larger;">
          {{ camera.name && camera.name.length > 0 ? camera.name : `Camera #${camera.index}` }}
        </div>
        <div style="font-size: smaller; opacity: 70%;">
          {{ camera.currentImageFormatDescription }} @ {{ camera.currentFrameRate }} fps
          <span v-if="camera.currentQuality">
            (Quality: {{ camera.currentQuality }})
          </span>
        </div>
      </div>
      <!-- 控制按钮组 -->
      <div class="d-flex">
        <!-- 设置按钮 -->
        <v-btn 
          variant="tonal" 
          @click="settingsDialog = true" 
          :icon="mdiCog" 
          class="mr-1" 
          aria-label="Settings" 
        />
        <!-- 截图按钮 -->
        <v-btn 
          variant="tonal" 
          @click="captureFrame" 
          :icon="mdiCameraOutline" 
          class="mr-1"
          aria-label="Download Frame" 
        />
        <!-- 原始图像下载按钮 -->
        <v-btn 
          variant="tonal" 
          @click="captureRawFrame" 
          :icon="mdiRaw" 
          class="mr-1"
          aria-label="Download Raw Image (BIN)" 
        />
      </div>
    </div>
  </v-card>

  <!-- 设置对话框 -->
  <v-dialog v-model="settingsDialog" max-width="500">
    <v-card>
      <v-card-title :prepend-icon="mdiCog">
        Camera Settings
      </v-card-title>
      <v-card-text>
        <!-- 图像格式选择 -->
        <v-select 
          v-model="selectedImageFormatId" 
          :items="camera.imageFormats" 
          item-title="description" 
          item-value="id"
          :disabled="settingsSaving" 
          label="Image Format" 
        />
        <!-- 画质滑块（仅当所选格式支持画质设置时显示） -->
        <v-slider 
          v-model="selectedQuality" 
          v-if="selectedFormat?.quality" 
          :min="selectedFormat?.quality.min ?? 80"
          :max="selectedFormat?.quality.max ?? 95" 
          :step="selectedFormat?.quality.step ?? 1" 
          :disabled="settingsSaving"
          label="Quality"
        >
          <template #append>
            {{ selectedQuality }}
          </template>
        </v-slider>
        <!-- 不支持画质设置时的提示 -->
        <div v-else class="text-center mb-4">This image format may not support quality settings</div>
        <!-- 保存/取消按钮 -->
        <v-row>
          <v-col cols="9">
            <v-btn 
              variant="tonal" 
              @click="saveSettings" 
              width="100%" 
              :loading="settingsSaving"
            >Save</v-btn>
          </v-col>
          <v-col cols="3">
            <v-btn 
              variant="tonal" 
              @click="settingsDialog = false" 
              color="error" 
              width="100%"
              :disabled="settingsSaving"
            >Cancel</v-btn>
          </v-col>
        </v-row>
      </v-card-text>
    </v-card>
  </v-dialog>

  <!-- 保存状态提示条 -->
  <v-snackbar v-model="saveStatusSnackbar" :timeout="2000" color="success">
    {{ saveStatusSnackbarText }}
  </v-snackbar>
</template>

<script setup lang="ts">
/**
 * @file components/CameraCard.vue
 * @brief 摄像头卡片组件
 *
 * 本组件负责展示单个摄像头的视频流和控制界面，主要功能包括：
 * 1. 显示摄像头实时画面（MJPEG 流）
 * 2. 显示摄像头基本信息（名称、分辨率、帧率、画质）
 * 3. 提供设置按钮，支持切换图像格式和调整画质
 * 4. 提供截图功能（JPEG 格式）
 * 5. 提供原始图像下载功能（BIN 格式）
 * 6. 图像加载失败时自动重试
 *
 * Props:
 * - camNum: 摄像头索引，从 0 开始
 *
 * 数据流转：
 * 1. 从 Pinia store 获取摄像头配置信息
 * 2. 根据配置构建视频流 URL
 * 3. 通过 v-img 组件渲染视频流
 * 4. 用户操作通过 API 请求发送到后端
 */

// 导入 Vue 的响应式 API 和组合式 API
import { ref, computed, watch, onMounted, onUnmounted } from 'vue'
// 导入 Material Design 图标
import { mdiCameraOutline, mdiRaw, mdiCog } from '@mdi/js';
// 导入 Pinia 状态管理
import { useMainStore } from '@/store/mainstore';

/**
 * @constant LOADING_IMAGE_SRC
 * @brief 加载中占位图像路径
 *
 * 当视频流正在加载时显示的占位图像。
 */
const LOADING_IMAGE_SRC = "/loading.jpg"

// 创建状态管理实例
const mainStore = useMainStore()

/**
 * @prop camNum
 * @brief 摄像头索引
 *
 * 标识当前组件对应的摄像头序号，用于从 store 中获取摄像头信息。
 */
const props = defineProps<{
  camNum: number,
}>()

/**
 * @computed camera
 * @brief 当前摄像头的配置信息
 *
 * 从 Pinia store 中获取指定索引的摄像头配置。
 */
const camera = computed(() => mainStore.clientCameras[props.camNum])

/**
 * @var imgSrc
 * @brief 当前显示的图像源 URL
 *
 * 用于绑定到 v-img 组件的 src 属性，显示摄像头视频流。
 */
const imgSrc = ref<string>(LOADING_IMAGE_SRC)

/**
 * @var settingsDialog
 * @brief 设置对话框显示状态
 *
 * 控制摄像头设置对话框的打开和关闭。
 */
const settingsDialog = ref<boolean>(false)

/**
 * @var selectedImageFormatId
 * @brief 当前选中的图像格式 ID
 *
 * 用于设置对话框中的图像格式选择器。
 */
const selectedImageFormatId = ref<number | string>(camera.value.currentImageFormat)

/**
 * @var selectedQuality
 * @brief 当前选中的画质值
 *
 * 用于设置对话框中的画质滑块。
 */
const selectedQuality = ref<number>(camera.value.currentQuality ?? 80)

/**
 * @var settingsSaving
 * @brief 设置保存中状态
 *
 * 控制保存按钮的 loading 状态，防止重复提交。
 */
const settingsSaving = ref<boolean>(false)

/**
 * @var saveStatusSnackbar
 * @brief 保存状态提示条显示状态
 */
const saveStatusSnackbar = ref<boolean>(false)

/**
 * @var saveStatusSnackbarText
 * @brief 保存状态提示条文本
 */
const saveStatusSnackbarText = ref<string>("")

/**
 * @var retryTimeoutId
 * @brief 重试定时器 ID
 *
 * 用于图像加载失败后自动重试的定时器。
 */
const retryTimeoutId = ref<ReturnType<typeof setTimeout> | null>(null)

/**
 * @var reloadTimeoutId
 * @brief 重新加载定时器 ID
 *
 * 用于刷新摄像头视频流的定时器。
 */
const reloadTimeoutId = ref<ReturnType<typeof setTimeout> | null>(null)

/**
 * @computed selectedFormat
 * @brief 当前选中的图像格式详细信息
 *
 * 根据 selectedImageFormatId 从摄像头支持的格式列表中查找对应的格式配置。
 */
const selectedFormat = computed(() => {
  return camera.value.imageFormats.find(format => format.id === selectedImageFormatId.value)
})

/**
 * @function onImageError
 * @brief 图像加载错误处理
 *
 * 当视频流加载失败时触发，3 秒后自动重试加载。
 */
const onImageError = () => {
  // 如果当前是加载中状态，不进行重试
  if (imgSrc.value === LOADING_IMAGE_SRC) return;

  // 清除之前的重试定时器
  if (retryTimeoutId.value) {
    clearTimeout(retryTimeoutId.value)
  }

  // 3 秒后重新加载摄像头视频流
  retryTimeoutId.value = setTimeout(() => {
    reloadCameraSrc(1000)
    retryTimeoutId.value = null
  }, 3000)
}

/**
 * @computed realCameraUrl
 * @brief 实际的摄像头视频流 URL
 *
 * 根据摄像头配置的 src 属性构建完整的视频流 URL。
 * 支持两种格式：
 * 1. 相对路径格式（如 "/stream"）
 * 2. 端口路径格式（如 ":8080/stream"）
 */
const realCameraUrl = computed(() => {
  let port: number | null = null;
  let path: string | null = null;

  // 处理端口路径格式
  if (camera.value.src.startsWith(':')) {
    const [, portStr, pathStr] = camera.value.src.split(/[:\/]/);
    port = Number(portStr);
    path = pathStr;

    const realUrl = new URL(path, location.href);
    realUrl.port = port.toString();
    return realUrl.toString();
  } else {
    // 处理相对路径格式
    path = camera.value.src;
    return new URL(path, location.href).toString();
  }
})

/**
 * @function reloadCameraSrc
 * @brief 重新加载摄像头视频流
 *
 * 先显示加载中图像，延迟指定时间后切换到实际视频流 URL。
 *
 * @param ms 延迟时间（毫秒），默认为 100ms
 */
const reloadCameraSrc = (ms: number = 100) => {
  imgSrc.value = LOADING_IMAGE_SRC;

  // 清除之前的重新加载定时器
  if (reloadTimeoutId.value) {
    clearTimeout(reloadTimeoutId.value)
    reloadTimeoutId.value = null
  }

  // 延迟后加载实际视频流
  reloadTimeoutId.value = setTimeout(() => {
    imgSrc.value = realCameraUrl.value;
    reloadTimeoutId.value = null;
  }, ms);
}

/**
 * @function captureFrame
 * @brief 捕获当前帧（JPEG 格式）
 *
 * 通过创建下载链接的方式，调用 API 捕获当前摄像头画面并下载为 JPEG 文件。
 */
const captureFrame = () => {
  const url = `/api/capture_image?source=${camera.value.index}`;

  const link = document.createElement('a');
  link.href = url;
  link.download = `camera_${camera.value.index}_image.jpg`;
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
}

/**
 * @function captureRawFrame
 * @brief 捕获原始图像（BIN 格式）
 *
 * 通过创建下载链接的方式，调用 API 捕获原始图像数据并下载为 BIN 文件。
 */
const captureRawFrame = () => {
  const url = `/api/capture_binary?source=${camera.value.index}`;

  const link = document.createElement('a');
  link.href = url;
  link.download = `camera_${camera.value.index}_raw.bin`;
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
}

/**
 * @function saveSettings
 * @brief 保存摄像头设置
 *
 * 将用户在设置对话框中选择的图像格式和画质设置发送到后端 API。
 * 保存成功后刷新视频流并显示提示信息。
 */
const saveSettings = async () => {
  // 显示加载状态
  imgSrc.value = LOADING_IMAGE_SRC;
  settingsSaving.value = true;

  // 发送 POST 请求保存设置
  await fetch('/api/set_camera_config', {
    method: 'POST',
    body: JSON.stringify({
      index: camera.value.index,
      image_format: selectedImageFormatId.value,
      jpeg_quality: selectedQuality.value
    })
  }).then(res => {
    // 根据响应状态显示成功或失败提示
    if (res.ok) {
      saveStatusSnackbar.value = true;
      saveStatusSnackbarText.value = "Settings saved";
    } else {
      saveStatusSnackbar.value = true;
      saveStatusSnackbarText.value = "Failed to save settings";
    }
  }).finally(() => {
    // 恢复界面状态并刷新视频流
    settingsSaving.value = false;
    settingsDialog.value = false;
    reloadCameraSrc();
  })
}

/**
 * @watch realCameraUrl
 * @brief 监听视频流 URL 变化
 *
 * 当摄像头配置改变导致视频流 URL 变化时，自动更新显示的图像源。
 */
watch(realCameraUrl, (newUrl) => {
  if (imgSrc.value !== LOADING_IMAGE_SRC) {
    imgSrc.value = newUrl;
  }
})

/**
 * @watch selectedImageFormatId
 * @brief 监听图像格式选择变化
 *
 * 当用户选择不同的图像格式时，自动更新画质滑块的默认值。
 */
watch(selectedImageFormatId, () => {
  if (selectedFormat.value?.quality) {
    selectedQuality.value = selectedFormat.value?.quality.default ?? selectedFormat.value?.quality.max ?? 90;
  }
})

/**
 * @function onMounted
 * @brief 组件挂载钩子
 *
 * 组件挂载到 DOM 后，开始加载摄像头视频流。
 */
onMounted(() => {
  reloadCameraSrc();
})

/**
 * @function onUnmounted
 * @brief 组件卸载钩子
 *
 * 组件从 DOM 移除前，清除所有定时器，防止内存泄漏。
 */
onUnmounted(() => {
  if (retryTimeoutId.value) {
    clearTimeout(retryTimeoutId.value)
    retryTimeoutId.value = null
  }
})
</script>
