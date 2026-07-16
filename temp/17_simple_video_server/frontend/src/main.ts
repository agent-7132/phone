/**
 * @file main.ts
 * @brief Vue 应用入口文件
 *
 * 本文件是视频服务器前端应用的入口点，负责：
 * 1. 创建 Vue 应用实例
 * 2. 注册所有插件（Vuetify、路由等）
 * 3. 将应用挂载到 DOM 元素上
 *
 * 执行流程：
 * 1. 导入插件注册函数和根组件
 * 2. 使用 createApp 创建应用实例
 * 3. 调用 registerPlugins 注册所有插件
 * 4. 将应用挂载到 #app DOM 元素
 */

// 导入插件注册函数，包含 Vuetify、路由等核心插件
import { registerPlugins } from '@/plugins'

// 导入根组件 App.vue
import App from './App.vue'

// 导入 Vue 的 createApp 组合式 API
import { createApp } from 'vue'

// 创建 Vue 应用实例
const app = createApp(App)

// 注册所有插件（Vuetify、Router、Pinia 等）
registerPlugins(app)

// 将应用挂载到 index.html 中的 #app DOM 元素
app.mount('#app')
