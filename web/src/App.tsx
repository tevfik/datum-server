import { BrowserRouter, Routes, Route, Navigate, useLocation } from "react-router-dom";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import SidebarLayout from "./layouts/SidebarLayout";
import Devices from "@/features/devices/pages/Devices";
import DeviceDetail from "@/features/devices/pages/DeviceDetail";
import Commands from "@/features/devices/pages/Commands";
import Dashboard from "@/features/dashboard/pages/Dashboard";
import Explorer from "@/features/explorer/pages/Explorer";
import Settings from "@/features/settings/pages/Settings";
import Login from "@/features/auth/pages/Login";
import ForgotPassword from "@/features/auth/pages/ForgotPassword";
import ResetPassword from "@/features/auth/pages/ResetPassword";
import { ThemeProvider } from "@/shared/components/theme-provider";
import { AuthProvider, useAuth } from "@/shared/context/AuthContext";

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
                <Route path="commands" element={<Commands />} />
                <Route path="explorer" element={<Explorer />} />
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
