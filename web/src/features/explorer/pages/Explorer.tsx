import { useState, useMemo, useEffect } from 'react';
import { useQuery } from '@tanstack/react-query';
import { deviceService } from '@/features/devices/services/deviceService';
import { explorerService } from '@/features/explorer/services/explorerService';
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
import { Button } from '@/components/ui/button';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import {
    Select,
    SelectContent,
    SelectItem,
    SelectTrigger,
    SelectValue,
} from '@/components/ui/select';
import { Checkbox } from '@/features/explorer/components/Checkbox';
import { Input } from '@/components/ui/input';
import { Label } from '@/components/ui/label';
import { Download, RefreshCw, Activity } from 'lucide-react';
import { format } from 'date-fns';

type RangeType = '1h' | '24h' | '7d' | '30d' | 'custom';

export default function Explorer() {
    const [selectedDeviceId, setSelectedDeviceId] = useState<string>('');
    const [rangeType, setRangeType] = useState<RangeType>('24h');
    const [customStart, setCustomStart] = useState<string>('');
    const [customEnd, setCustomEnd] = useState<string>('');
    const [selectedMetrics, setSelectedMetrics] = useState<string[]>([]);

    // Fetch Devices
    const { data: devices = [], isLoading: isLoadingDevices } = useQuery({
        queryKey: ['devices'],
        queryFn: deviceService.getAll,
    });

    // Auto-select first device if none selected
    if (!selectedDeviceId && devices.length > 0) {
        setSelectedDeviceId(devices[0].id);
    }

    // Prepare Query Params
    const queryParams = useMemo(() => {
        const now = new Date();
        let start: Date | undefined;
        let end: Date = now;

        if (rangeType === 'custom') {
            if (!customStart || !customEnd) return null;
            start = new Date(customStart);
            end = new Date(customEnd);
        } else {
            const timeMap: Record<string, number> = {
                '1h': 60 * 60 * 1000,
                '24h': 24 * 60 * 60 * 1000,
                '7d': 7 * 24 * 60 * 60 * 1000,
                '30d': 30 * 24 * 60 * 60 * 1000,
            };
            const duration = timeMap[rangeType];
            if (duration) {
                start = new Date(now.getTime() - duration);
            }
        }

        return { start, end };
    }, [rangeType, customStart, customEnd]);

    // Fetch History
    const { data: history, isLoading, refetch } = useQuery({
        queryKey: ['history', selectedDeviceId, queryParams],
        queryFn: () => selectedDeviceId && queryParams
            ? explorerService.getHistory(selectedDeviceId, queryParams)
            : Promise.resolve(null),
        enabled: !!selectedDeviceId && !!queryParams,
    });

    // Extract available metrics from data
    const availableMetrics = useMemo(() => {
        if (!history?.data || history.data.length === 0) return [];
        const keys = new Set<string>();
        // Scan first 50 points to find keys
        history.data.slice(0, 50).forEach(pt => {
            Object.keys(pt.data).forEach(k => {
                if (typeof pt.data[k] === 'number') {
                    keys.add(k);
                }
            });
        });
        return Array.from(keys);
    }, [history]);

    // Auto-select metrics ONLY when availableMetrics change (e.g. new device/range)
    // and we don't have a valid selection yet.
    useEffect(() => {
        if (availableMetrics.length > 0) {
            // Check if our current selection is valid (intersects with available)
            const hasValidSelection = selectedMetrics.some(m => availableMetrics.includes(m));

            if (!hasValidSelection) {
                // Default to first 3 metrics
                setSelectedMetrics(availableMetrics.slice(0, 3));
            }
        }
    }, [availableMetrics]);

    const handleMetricToggle = (metric: string) => {
        setSelectedMetrics(prev =>
            prev.includes(metric)
                ? prev.filter(m => m !== metric)
                : [...prev, metric]
        );
    };

    const handleExport = () => {
        if (!history?.data) return;

        // CSV Header
        const headers = ['timestamp', ...selectedMetrics];
        const csvRows = [headers.join(',')];

        history.data.forEach(pt => {
            const row = [
                pt.timestamp,
                ...selectedMetrics.map(m => pt.data[m] ?? '')
            ];
            csvRows.push(row.join(','));
        });

        const blob = new Blob([csvRows.join('\n')], { type: 'text/csv' });
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `export-${selectedDeviceId}-${new Date().toISOString()}.csv`;
        a.click();
    };

    // Chart Colors
    const colors = ["#2563eb", "#16a34a", "#dc2626", "#d97706", "#9333ea", "#0891b2"];

    return (
        <div className="space-y-6">
            <div className="flex flex-col gap-4 md:flex-row md:items-center md:justify-between">
                <div>
                    <h1 className="text-3xl font-bold tracking-tight">Data Explorer</h1>
                    <p className="text-muted-foreground">Analyze historical device telemetry.</p>
                </div>
                <div className="flex items-center gap-2">
                    <Button variant="outline" onClick={handleExport} disabled={!history?.data?.length}>
                        <Download className="mr-2 h-4 w-4" /> Export CSV
                    </Button>
                    <Button variant="outline" size="icon" onClick={() => refetch()}>
                        <RefreshCw className={`h-4 w-4 ${isLoading ? 'animate-spin' : ''}`} />
                    </Button>
                </div>
            </div>

            {/* Empty State */}
            {!isLoadingDevices && devices.length === 0 ? (
                <div className="flex flex-col items-center justify-center p-12 border-2 border-dashed rounded-lg">
                    <div className="text-xl font-semibold mb-2">No Devices Found</div>
                    <p className="text-muted-foreground mb-4">Add a device to start exploring data.</p>
                </div>
            ) : (
                <>
                    {/* Controls Toolbar */}
                    <Card>
                        <CardContent className="p-4">
                            <div className="flex flex-col gap-4 md:flex-row md:items-end">
                                {/* Device Selector */}
                                <div className="space-y-2 min-w-[200px]">
                                    <Label>Device</Label>
                                    <Select value={selectedDeviceId} onValueChange={setSelectedDeviceId}>
                                        <SelectTrigger>
                                            <SelectValue placeholder="Select Device" />
                                        </SelectTrigger>
                                        <SelectContent>
                                            {devices.map(d => (
                                                <SelectItem key={d.id} value={d.id}>{d.name || d.id}</SelectItem>
                                            ))}
                                        </SelectContent>
                                    </Select>
                                </div>

                                {/* Range Selector */}
                                <div className="space-y-2 min-w-[150px]">
                                    <Label>Time Range</Label>
                                    <Select value={rangeType} onValueChange={(v) => setRangeType(v as RangeType)}>
                                        <SelectTrigger>
                                            <SelectValue />
                                        </SelectTrigger>
                                        <SelectContent>
                                            <SelectItem value="1h">Last Hour</SelectItem>
                                            <SelectItem value="24h">Last 24 Hours</SelectItem>
                                            <SelectItem value="7d">Last 7 Days</SelectItem>
                                            <SelectItem value="30d">Last 30 Days</SelectItem>
                                            <SelectItem value="custom">Custom Range</SelectItem>
                                        </SelectContent>
                                    </Select>
                                </div>

                                {/* Custom Date Inputs */}
                                {rangeType === 'custom' && (
                                    <>
                                        <div className="space-y-2">
                                            <Label>Start</Label>
                                            <Input
                                                type="datetime-local"
                                                value={customStart}
                                                onChange={e => setCustomStart(e.target.value)}
                                                className="w-full md:w-auto"
                                            />
                                        </div>
                                        <div className="space-y-2">
                                            <Label>End</Label>
                                            <Input
                                                type="datetime-local"
                                                value={customEnd}
                                                onChange={e => setCustomEnd(e.target.value)}
                                                className="w-full md:w-auto"
                                            />
                                        </div>
                                    </>
                                )}
                            </div>
                        </CardContent>
                    </Card>

                    {/* Metric Selection */}
                    {availableMetrics.length > 0 && (
                        <div className="flex flex-wrap gap-4 p-4 border rounded-lg bg-card">
                            <div className="text-sm font-medium flex items-center gap-2">
                                <Activity className="h-4 w-4" /> Metrics:
                            </div>
                            {availableMetrics.map((metric, idx) => (
                                <div key={metric} className="flex items-center space-x-2">
                                    <Checkbox
                                        id={`m-${metric}`}
                                        checked={selectedMetrics.includes(metric)}
                                        onCheckedChange={() => handleMetricToggle(metric)}
                                    />
                                    <label
                                        htmlFor={`m-${metric}`}
                                        className="text-sm font-medium leading-none peer-disabled:cursor-not-allowed peer-disabled:opacity-70"
                                        style={{ color: colors[idx % colors.length] }}
                                    >
                                        {metric}
                                    </label>
                                </div>
                            ))}
                        </div>
                    )}

                    {/* Chart Area */}
                    <Card className="min-h-[400px]">
                        <CardHeader>
                            <CardTitle>Telemetry Chart</CardTitle>
                            {history?.range && (
                                <CardDescription>
                                    {format(new Date(history.range.start), 'MMM d HH:mm')} - {format(new Date(history.range.end), 'MMM d HH:mm')}
                                    {history.interval && ` (${history.count} points, interval: ${history.interval})`}
                                </CardDescription>
                            )}
                        </CardHeader>
                        <CardContent>
                            {isLoading ? (
                                <div className="h-[300px] flex items-center justify-center text-muted-foreground">Loading data...</div>
                            ) : !history?.data || history.data.length === 0 ? (
                                <div className="h-[300px] flex items-center justify-center text-muted-foreground">
                                    No data available for this range.
                                </div>
                            ) : (
                                <div className="h-[400px] w-full">
                                    <ResponsiveContainer width="100%" height="100%">
                                        <LineChart data={history.data}>
                                            <CartesianGrid strokeDasharray="3 3" vertical={false} stroke="hsl(var(--border))" />
                                            <XAxis
                                                dataKey="timestamp"
                                                tickFormatter={(val) => format(new Date(val), rangeType === '1h' ? 'HH:mm' : 'd MMM HH:mm')}
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
                                            />
                                            <Tooltip
                                                labelFormatter={(val) => format(new Date(val), 'yyyy-MM-dd HH:mm:ss')}
                                                contentStyle={{
                                                    backgroundColor: 'hsl(var(--card))',
                                                    borderColor: 'hsl(var(--border))',
                                                    borderRadius: '6px'
                                                }}
                                            />
                                            <Legend />
                                            {selectedMetrics.map((metric, idx) => (
                                                <Line
                                                    key={metric}
                                                    type="monotone"
                                                    dataKey={`data.${metric}`}
                                                    name={metric}
                                                    stroke={colors[idx % colors.length]}
                                                    strokeWidth={2}
                                                    dot={false}
                                                    activeDot={{ r: 4 }}
                                                />
                                            ))}
                                        </LineChart>
                                    </ResponsiveContainer>
                                </div>
                            )}
                        </CardContent>
                    </Card>
                </>
            )}
        </div>
    );
}
