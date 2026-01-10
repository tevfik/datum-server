import { BrowserRouter, Routes, Route, Navigate } from "react-router-dom";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import SidebarLayout from "./layouts/SidebarLayout";
import Devices from "./pages/Devices";
import Dashboard from "./pages/Dashboard";

import { ThemeProvider } from "./components/theme-provider";

// Create a client
const queryClient = new QueryClient();

function App() {
  return (
    <QueryClientProvider client={queryClient}>
      <ThemeProvider defaultTheme="dark" storageKey="datum-ui-theme">
        <BrowserRouter>
          <Routes>
            <Route path="/" element={<SidebarLayout />}>
              <Route index element={<Dashboard />} />
              <Route path="devices" element={<Devices />} />
              <Route path="commands" element={<div className="p-4">Commands Page (Coming Soon)</div>} />
            </Route>
            {/* Fallback */}
            <Route path="*" element={<Navigate to="/" replace />} />
          </Routes>
        </BrowserRouter>
      </ThemeProvider>
    </QueryClientProvider>
  );
}

export default App;
