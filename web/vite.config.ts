import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import path from 'path'

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      "@": path.resolve(__dirname, "./src"),
    },
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
