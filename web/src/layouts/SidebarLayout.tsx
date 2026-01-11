import { Outlet, NavLink } from "react-router-dom";
import {
    LayoutDashboard,
    Server,
    LogOut,
    Menu,
    Settings as SettingsIcon
} from "lucide-react";
import { useState } from "react";
import { cn } from "@/lib/utils";
import { useAuth } from "@/context/AuthContext";

export default function SidebarLayout() {
    const [isOpen, setIsOpen] = useState(false);
    const { logout } = useAuth();

    // Mock toggle for mobile
    const toggleSidebar = () => setIsOpen(!isOpen);

    const navItems = [
        { icon: LayoutDashboard, label: "Dashboard", href: "/" },
        { icon: Server, label: "Devices", href: "/devices" },
        { icon: SettingsIcon, label: "Settings", href: "/settings" },
    ];

    return (
        <div className="flex h-screen bg-background text-foreground overflow-hidden">
            {/* Sidebar */}
            <aside
                className={cn(
                    "fixed inset-y-0 left-0 z-50 w-64 transform bg-card border-r transition-transform duration-200 ease-in-out md:relative md:translate-x-0",
                    isOpen ? "translate-x-0" : "-translate-x-full"
                )}
            >
                <div className="flex h-16 items-center border-b px-6">
                    <span className="text-xl font-bold bg-gradient-to-r from-blue-600 to-indigo-600 bg-clip-text text-transparent">
                        datum
                    </span>
                </div>

                <nav className="flex-1 space-y-1 px-3 py-4">
                    {navItems.map((item) => (
                        <NavLink
                            key={item.href}
                            to={item.href}
                            className={({ isActive }) =>
                                cn(
                                    "flex items-center gap-3 rounded-lg px-3 py-2 text-sm font-medium transition-colors",
                                    isActive
                                        ? "bg-primary/10 text-primary"
                                        : "text-muted-foreground hover:bg-muted hover:text-foreground"
                                )
                            }
                            onClick={() => setIsOpen(false)} // Close on mobile click
                        >
                            <item.icon className="h-5 w-5" />
                            {item.label}
                        </NavLink>
                    ))}
                </nav>

                <div className="border-t p-4">
                    <button
                        onClick={() => logout()}
                        className="flex w-full items-center gap-3 rounded-lg px-3 py-2 text-sm font-medium text-destructive hover:bg-destructive/10 transition-colors"
                    >
                        <LogOut className="h-5 w-5" />
                        Sign Out
                    </button>
                </div>
            </aside>

            {/* Main Content */}
            <div className="flex flex-1 flex-col overflow-hidden">
                {/* Header (Mobile Trigger + Breadcrumbs/Actions) */}
                <header className="flex h-16 items-center justify-between border-b bg-card/50 px-6 backdrop-blur md:px-8">
                    <button className="md:hidden" onClick={toggleSidebar}>
                        <Menu className="h-6 w-6" />
                    </button>
                    <div className="flex items-center gap-4">
                        {/* Place for user profile or global actions */}
                    </div>
                </header>

                {/* Scrollable Area */}
                <main className="flex-1 overflow-y-auto p-4 md:p-8">
                    <Outlet />
                </main>
            </div>

            {/* Mobile Overlay */}
            {isOpen && (
                <div
                    className="fixed inset-0 z-40 bg-black/50 md:hidden"
                    onClick={() => setIsOpen(false)}
                />
            )}
        </div>
    );
}
