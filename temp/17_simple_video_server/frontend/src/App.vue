<template>
  <!-- 根应用容器，使用 Vuetify 的 v-app 组件 -->
  <v-app>
    <!-- 路由视图，根据当前路由渲染对应的页面组件 -->
    <router-view />
  </v-app>
</template>

<script lang="ts" setup>
/**
 * @file App.vue
 * @brief 根组件
 *
 * 本组件是 Vue 应用的根组件，作为所有页面的容器。
 * 使用 Vuetify 的 v-app 组件提供全局样式和主题支持，
 * 通过 router-view 实现路由视图切换。
 *
 * 结构说明：
 * - v-app: Vuetify 应用根组件，提供全局配置
 * - router-view: Vue Router 的视图出口，渲染当前路由对应的页面
 */
</script>
