/**
 * @file plugins/vuetify.ts
 * @brief Vuetify 框架配置文件
 *
 * 本文件配置 Vuetify UI 框架，包括：
 * 1. 导入 Vuetify 样式
 * 2. 配置图标集（使用 Material Design Icons）
 * 3. 设置默认主题为深色模式
 *
 * Vuetify 是一个基于 Material Design 的 Vue 组件库，
 * 提供丰富的 UI 组件和响应式布局支持。
 */

// 导入 Vuetify 全局样式
import 'vuetify/styles'

// 导入 Vuetify 的 createVuetify 函数
import { createVuetify } from 'vuetify'
// 导入 Material Design Icons 的别名和 SVG 图标集
import { aliases, mdi } from 'vuetify/iconsets/mdi-svg'

/**
 * @export default createVuetify
 * @brief 创建 Vuetify 实例并导出
 *
 * 配置 Vuetify 的主题和图标集：
 * - defaultTheme: 'dark' - 使用深色主题
 * - icons: 使用 Material Design Icons SVG 图标集
 */
export default createVuetify({
  /**
   * @property theme
   * @brief 主题配置
   *
   * 设置默认主题为深色模式，提供更好的视觉体验。
   */
  theme: {
    defaultTheme: 'dark',
  },
  
  /**
   * @property icons
   * @brief 图标配置
   *
   * 使用 Material Design Icons 作为默认图标集，
   * 支持所有 Material Design Icons 的 SVG 图标。
   */
  icons: {
    defaultSet: 'mdi',      // 默认图标集为 Material Design Icons
    aliases: {
      ...aliases,           // 展开默认图标别名
    },
    sets: {
      mdi,                  // 注册 mdi 图标集
    },
  },
})
