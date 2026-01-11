import { BrowserRouter, Routes, Route, Navigate, useLocation } from "react-router-dom";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import SidebarLayout from "./layouts/SidebarLayout";
import Devices from "./pages/Devices";
import DeviceDetail from "./pages/DeviceDetail";
import Dashboard from "./pages/Dashboard";
import Settings from "./pages/Settings";
import Login from "./pages/Login";
import ForgotPassword from "./pages/ForgotPassword";
import ResetPassword from "./pages/ResetPassword";
import { ThemeProvider } from "./components/theme-provider";
import { AuthProvider, useAuth } from "./context/AuthContext";

// Create a client
const queryClient = new QueryClient();

// Protected Route Wrapper
function ProtectedRoute({ children }: { children: React.ReactElement }) {
  const { isAuthenticated, isLoading } = useAuth();
  const location = useLocation();

  if (isLoading) {
    return <div className="flex h-screen items-center justify-center">Loading...</div>;
  }

  if (!isAuthenticated) {
    return <Navigate to="/login" state={{ from: location }} replace />;
  }

  return children;
}

function App() {
  return (
    <QueryClientProvider client={queryClient}>
      <ThemeProvider defaultTheme="dark" storageKey="datum-ui-theme">
        <AuthProvider>
          <BrowserRouter>
            <Routes>
              <Route path="/login" element={<Login />} />
              <Route path="/forgot-password" element={<ForgotPassword />} />
              <Route path="/reset-password" element={<ResetPassword />} />

              <Route path="/" element={
                <ProtectedRoute>
                  <SidebarLayout />
                </ProtectedRoute>
              }>
                <Route index element={<Dashboard />} />
                <Route path="devices" element={<Devices />} />
                <Route path="devices/:id" element={<DeviceDetail />} />
                <Route path="commands" element={<div className="p-4">Commands Page (Coming Soon)</div>} />
                <Route path="settings" element={<Settings />} />
              </Route>

              {/* Fallback */}
              <Route path="*" element={<Navigate to="/" replace />} />
            </Routes>
          </BrowserRouter>
        </AuthProvider>
      </ThemeProvider>
    </QueryClientProvider>
  );
}

export default App;
