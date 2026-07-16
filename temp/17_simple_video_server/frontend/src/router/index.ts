/**
 * @file router/index.ts
 * @brief Vue Router 路由配置
 *
 * 本文件定义了前端应用的路由配置，目前仅包含首页路由。
 * 使用 createWebHistory 创建 HTML5 历史模式的路由。
 *
 * 路由列表：
 * - '/' -> index.vue (首页，展示所有摄像头)
 */

// 导入 Vue Router 的核心函数
import { createRouter, createWebHistory } from 'vue-router'
// 导入首页组件
import index from '@/pages/index.vue'

/**
 * @constant router
 * @brief 路由实例
 *
 * 使用 createRouter 创建路由实例，配置历史模式和路由表。
 */
const router = createRouter({
  /**
   * @property history
   * @brief 历史模式配置
   *
   * 使用 createWebHistory 创建 HTML5 历史模式，
   * 支持不带 # 的 URL 路径。
   * BASE_URL 从环境变量 import.meta.env.BASE_URL 获取。
   */
  history: createWebHistory(import.meta.env.BASE_URL),
  
  /**
   * @property routes
   * @brief 路由表配置
   *
   * 定义所有路由规则，当前仅包含首页路由。
   */
  routes: [
    { 
      path: '/',           // 路由路径
      component: index     // 对应的组件
    }
  ],
})

/**
 * @export router
 * @brief 导出路由实例
 *
 * 在 main.ts 中被导入并注册到 Vue 应用。
 */
export default router
