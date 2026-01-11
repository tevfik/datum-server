import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import path from 'path'

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: [
      // Specific aliases first (order matters!)
      { find: "@/features", replacement: path.resolve(__dirname, "./src/features") },
      { find: "@/shared", replacement: path.resolve(__dirname, "./src/shared") },
      { find: "@/components", replacement: path.resolve(__dirname, "./src/shared/components") },
      { find: "@/context", replacement: path.resolve(__dirname, "./src/shared/context") },
      { find: "@/lib", replacement: path.resolve(__dirname, "./src/shared/lib") },
      { find: "@/types", replacement: path.resolve(__dirname, "./src/shared/types") },
      { find: "@/services", replacement: path.resolve(__dirname, "./src/services") },
      // Catch-all last
      { find: "@", replacement: path.resolve(__dirname, "./src") },
    ],
  },
  server: {
    proxy: {
      '/api': {
        target: 'http://localhost:8000',
        changeOrigin: true,
      },
      '/auth': {
        target: 'http://localhost:8000',
        changeOrigin: true,
      },
      '/devices': {
        target: 'http://localhost:8000',
        changeOrigin: true,
      },
      '/data': {
        target: 'http://localhost:8000',
        changeOrigin: true,
      },
      '/system': {
        target: 'http://localhost:8000',
        changeOrigin: true,
      }
    }
  },
  build: {
    rollupOptions: {
      output: {
        manualChunks: {
          vendor: ['react', 'react-dom', 'react-router-dom'],
          mqtt: ['mqtt'],
          charts: ['recharts'],
          ui: ['@radix-ui/react-dialog', '@radix-ui/react-label', '@radix-ui/react-select', '@radix-ui/react-slot', 'lucide-react', 'class-variance-authority', 'clsx', 'tailwind-merge'],
          utils: ['date-fns', 'axios', '@tanstack/react-query']
        }
      }
    }
  }
})
