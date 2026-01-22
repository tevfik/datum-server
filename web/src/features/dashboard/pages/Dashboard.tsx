import {
    HardDrive,
    Wifi,
    Zap,
    AlertCircle
} from "lucide-react";
import {
    Area,
    AreaChart,
    CartesianGrid,
    ResponsiveContainer,
    Tooltip,
    XAxis,
    YAxis
} from "recharts";
import { useQuery } from "@tanstack/react-query";
import { deviceService } from "@/features/devices/services/deviceService";
import { AddDeviceModal } from "@/features/devices/components/AddDeviceModal";

// Mock data for chart (until we have system-wide metrics API)
const chartData = [
    { time: "00:00", value: 12 },
    { time: "04:00", value: 18 },
    { time: "08:00", value: 45 },
    { time: "12:00", value: 35 },
    { time: "16:00", value: 50 },
    { time: "20:00", value: 28 },
    { time: "23:59", value: 20 },
];

export default function Dashboard() {
    const { data: stats, isLoading } = useQuery({
        queryKey: ['devices', 'stats'],
        queryFn: deviceService.getStats,
    });

    // Calculate Stats
    const totalDevices = stats?.total_devices || 0;
    const onlineDevices = stats?.online_devices || 0;
    const offlineDevices = stats?.offline_devices || 0;

    // Calculate mock "health" (just for visuals)
    const systemHealth = totalDevices > 0 ? Math.round((onlineDevices / totalDevices) * 100) : 100;

    return (
        <div className="space-y-6">
            <div className="flex items-center justify-between">
                <h1 className="text-3xl font-bold tracking-tight">Dashboard</h1>
                <div className="flex items-center gap-2">
                    <span className={`flex h-2 w-2 rounded-full animate-pulse ${systemHealth > 80 ? 'bg-green-500' : 'bg-amber-500'}`}></span>
                    <span className="text-sm text-muted-foreground mr-4">
                        {isLoading ? 'Connecting...' : `System ${systemHealth > 80 ? 'Optimal' : 'Standard'}`}
                    </span>
                    <AddDeviceModal />
                </div>
            </div>

            {/* Stats Grid */}
            <div className="grid gap-4 md:grid-cols-2 lg:grid-cols-4">
                <StatCard
                    title="Total Devices"
                    value={isLoading ? "-" : totalDevices.toString()}
                    icon={HardDrive}
                    trend="Registered"
                />
                <StatCard
                    title="Online"
                    value={isLoading ? "-" : onlineDevices.toString()}
                    icon={Wifi}
                    trend={`${totalDevices > 0 ? ((onlineDevices / totalDevices) * 100).toFixed(0) : 0}% Uptime`}
                    className="text-green-500"
                />
                <StatCard
                    title="Offline"
                    value={isLoading ? "-" : offlineDevices.toString()}
                    icon={AlertCircle}
                    trend="Requires Attention"
                    className="text-red-500"
                />
                <StatCard
                    title="Avg Latency"
                    value="~45ms"
                    icon={Zap}
                    trend="Global Avg"
                    className="text-yellow-500"
                />
            </div>

            {/* Main Chart */}
            <div className="rounded-xl border bg-card text-card-foreground shadow">
                <div className="p-6 pb-2">
                    <h3 className="font-semibold leading-none tracking-tight">Telemetry Traffic (Mock)</h3>
                    <p className="text-sm text-muted-foreground">Incoming data points simulation</p>
                </div>
                <div className="h-[350px] w-full p-4">
                    <ResponsiveContainer width="100%" height="100%">
                        <AreaChart data={chartData} margin={{ top: 10, right: 10, left: 0, bottom: 0 }}>
                            <defs>
                                <linearGradient id="colorValue" x1="0" y1="0" x2="0" y2="1">
                                    <stop offset="5%" stopColor="hsl(var(--primary))" stopOpacity={0.3} />
                                    <stop offset="95%" stopColor="hsl(var(--primary))" stopOpacity={0} />
                                </linearGradient>
                            </defs>
                            <CartesianGrid strokeDasharray="3 3" vertical={false} stroke="hsl(var(--border))" />
                            <XAxis
                                dataKey="time"
                                stroke="#888888"
                                fontSize={12}
                                tickLine={false}
                                axisLine={false}
                            />
                            <YAxis
                                stroke="#888888"
                                fontSize={12}
                                tickLine={false}
                                axisLine={false}
                                tickFormatter={(value) => `${value}`}
                            />
                            <Tooltip
                                contentStyle={{
                                    backgroundColor: 'hsl(var(--card))',
                                    borderColor: 'hsl(var(--border))',
                                    borderRadius: '6px'
                                }}
                            />
                            <Area
                                type="monotone"
                                dataKey="value"
                                stroke="hsl(var(--primary))"
                                strokeWidth={2}
                                fillOpacity={1}
                                fill="url(#colorValue)"
                            />
                        </AreaChart>
                    </ResponsiveContainer>
                </div>
            </div>
        </div>
    );
}

function StatCard({ title, value, icon: Icon, trend, className }: any) {
    return (
        <div className="rounded-xl border bg-card text-card-foreground shadow p-6">
            <div className="flex flex-row items-center justify-between space-y-0 pb-2">
                <span className="text-sm font-medium">{title}</span>
                <Icon className={`h-4 w-4 text-muted-foreground ${className}`} />
            </div>
            <div className="pt-2">
                <div className="text-2xl font-bold">{value}</div>
                <p className="text-xs text-muted-foreground">{trend}</p>
            </div>
        </div>
    )
}
