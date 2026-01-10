import {
    Activity,
    HardDrive,
    Wifi,
    Zap
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

const data = [
    { time: "00:00", value: 45 },
    { time: "04:00", value: 52 },
    { time: "08:00", value: 89 },
    { time: "12:00", value: 76 },
    { time: "16:00", value: 92 },
    { time: "20:00", value: 65 },
    { time: "23:59", value: 55 },
];

export default function Dashboard() {
    return (
        <div className="space-y-6">
            <div className="flex items-center justify-between">
                <h1 className="text-3xl font-bold tracking-tight">Dashboard</h1>
                <div className="flex items-center gap-2">
                    <span className="flex h-2 w-2 rounded-full bg-green-500 animate-pulse"></span>
                    <span className="text-sm text-muted-foreground">System Online</span>
                </div>
            </div>

            {/* Stats Grid */}
            <div className="grid gap-4 md:grid-cols-2 lg:grid-cols-4">
                <StatCard
                    title="Total Devices"
                    value="12"
                    icon={HardDrive}
                    trend="+2.5%"
                />
                <StatCard
                    title="Active Now"
                    value="8"
                    icon={Wifi}
                    trend="+12%"
                    className="text-green-500"
                />
                <StatCard
                    title="Events (24h)"
                    value="1.2k"
                    icon={Activity}
                    trend="-5%"
                    className="text-blue-500"
                />
                <StatCard
                    title="Avg Latency"
                    value="45ms"
                    icon={Zap}
                    trend="Stable"
                    className="text-yellow-500"
                />
            </div>

            {/* Main Chart */}
            <div className="rounded-xl border bg-card text-card-foreground shadow">
                <div className="p-6 pb-2">
                    <h3 className="font-semibold leading-none tracking-tight">Telemetry Traffic</h3>
                    <p className="text-sm text-muted-foreground">Incoming data points over last 24 hours</p>
                </div>
                <div className="h-[350px] w-full p-4">
                    <ResponsiveContainer width="100%" height="100%">
                        <AreaChart data={data} margin={{ top: 10, right: 10, left: 0, bottom: 0 }}>
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
                <p className="text-xs text-muted-foreground">{trend} from last month</p>
            </div>
        </div>
    )
}
