import { useMemo } from 'react';
import {
    LineChart,
    Line,
    XAxis,
    YAxis,
    CartesianGrid,
    Tooltip,
    Legend,
    ResponsiveContainer
} from 'recharts';
import { format } from 'date-fns';
import type { TelemetryPoint } from '@/types/device';
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from '@/components/ui/card';
import { Activity } from 'lucide-react';

interface TelemetryChartProps {
    data: TelemetryPoint[];
    isLoading: boolean;
}

export function TelemetryChart({ data, isLoading }: TelemetryChartProps) {
    const chartData = useMemo(() => {
        if (!data || data.length === 0) return [];
        return data.map(point => ({
            timestamp: point.timestamp,
            date: format(new Date(point.timestamp * 1000), 'HH:mm:ss'),
            ...point.data
        })).reverse(); // Recharts wants ascending time usually, API might return desc? check backend. 
        // If query is LIMIT 100, usually returns latest first? 
        // Postgres query in backend: Order("timestamp DESC"). 
        // So we need to reverse for chart (left to right time).
    }, [data]);

    const dataKeys = useMemo(() => {
        if (!data || data.length === 0) return [];
        // Extract all unique keys from all data points (in case sparse)
        const keys = new Set<string>();
        data.forEach(p => Object.keys(p.data).forEach(k => keys.add(k)));
        // Filter only numeric values for LineChart
        return Array.from(keys).filter(key => {
            const val = data[0].data[key];
            return typeof val === 'number';
        });
    }, [data]);

    // Color palette
    const colors = ["#2563eb", "#16a34a", "#db2777", "#ea580c", "#7c3aed"];

    if (isLoading) return <div className="h-[300px] flex items-center justify-center text-muted-foreground">Loading telemetry...</div>;
    if (chartData.length === 0) return (
        <Card>
            <CardHeader>
                <CardTitle className="flex items-center gap-2">
                    <Activity className="h-5 w-5" />
                    Telemetry
                </CardTitle>
                <CardDescription>Real-time device data</CardDescription>
            </CardHeader>
            <CardContent>
                <div className="h-[200px] flex items-center justify-center border border-dashed rounded text-muted-foreground text-sm">
                    No telemetry data available
                </div>
            </CardContent>
        </Card>
    );

    return (
        <Card>
            <CardHeader>
                <CardTitle className="flex items-center gap-2">
                    <Activity className="h-5 w-5" />
                    Telemetry
                </CardTitle>
                <CardDescription>Visualizing numeric data points</CardDescription>
            </CardHeader>
            <CardContent>
                <div className="h-[300px] w-full">
                    <ResponsiveContainer width="100%" height="100%">
                        <LineChart data={chartData}>
                            <CartesianGrid strokeDasharray="3 3" vertical={false} stroke="hsl(var(--border))" />
                            <XAxis
                                dataKey="date"
                                fontSize={12}
                                tickLine={false}
                                axisLine={false}
                                stroke="#888888"
                            />
                            <YAxis
                                fontSize={12}
                                tickLine={false}
                                axisLine={false}
                                stroke="#888888"
                            />
                            <Tooltip
                                contentStyle={{ borderRadius: '8px', border: '1px solid hsl(var(--border))' }}
                            />
                            <Legend />
                            {dataKeys.map((key, index) => (
                                <Line
                                    key={key}
                                    type="monotone"
                                    dataKey={key}
                                    stroke={colors[index % colors.length]}
                                    strokeWidth={2}
                                    activeDot={{ r: 4 }}
                                    dot={false}
                                />
                            ))}
                        </LineChart>
                    </ResponsiveContainer>
                </div>
            </CardContent>
        </Card>
    );
}
