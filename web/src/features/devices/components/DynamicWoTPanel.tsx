import { type Device } from "@/shared/types/device";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { Switch } from "@/components/ui/switch";
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select";
import { LineChart, Line, Tooltip, ResponsiveContainer } from "recharts";
import { Activity, Zap, Gauge, Loader2 } from "lucide-react";
import { deviceService } from "@/features/devices/services/deviceService";
import { useState } from "react";

interface DynamicWoTPanelProps {
    device: Device;
    shadowState?: Record<string, any>;
}

export function DynamicWoTPanel({ device, shadowState }: DynamicWoTPanelProps) {
    const td = device.thing_description;

    if (!td || !td.properties) {
        return null;
    }

    const properties = td.properties as Record<string, any>;

    return (
        <Card className="col-span-2">
            <CardHeader>
                <CardTitle className="flex items-center gap-2">
                    <Activity className="h-5 w-5" />
                    Dynamic WoT Dashboard
                </CardTitle>
                <CardDescription>
                    Auto-generated UI based on Thing Description
                </CardDescription>
            </CardHeader>
            <CardContent>
                <div className="grid gap-4 md:grid-cols-2 lg:grid-cols-3">
                    {Object.entries(properties).map(([key, prop]) => {
                        const value = shadowState ? shadowState[key] : undefined;
                        return (
                            <WoTPropertyCard
                                key={key}
                                deviceId={device.id}
                                propKey={key}
                                propDef={prop}
                                value={value}
                            />
                        );
                    })}
                </div>
            </CardContent>
        </Card>
    );
}

function WoTPropertyCard({ deviceId, propKey, propDef, value }: { deviceId: string, propKey: string, propDef: any, value: any }) {
    const unit = propDef.unit || "";
    const title = propDef.title || propKey;
    const widgetType = propDef["ui:widget"];
    const [isLoading, setIsLoading] = useState(false);

    const handleUpdate = async (newValue: any) => {
        setIsLoading(true);
        try {
            await deviceService.sendCommand(deviceId, {
                action: "update_settings",
                params: { [propKey]: newValue }
            });
            // Ideally optimistic update or wait for shadow state
        } catch (e) {
            console.error("Failed to update property", e);
        } finally {
            setIsLoading(false);
        }
    };

    // Render TimeSeries Chart
    if (widgetType === "timeseries") {
        // Mock data for now, real implementation needs history
        const mockData = [
            { time: '10:00', val: value ? value * 0.9 : 10 },
            { time: '10:05', val: value ? value * 1.1 : 12 },
            { time: '10:10', val: value || 11 },
        ];

        return (
            <Card className="col-span-2 md:col-span-1">
                <CardHeader className="pb-2">
                    <CardTitle className="text-sm font-medium text-muted-foreground flex items-center justify-between">
                        {title}
                        <Activity className="h-4 w-4 text-primary" />
                    </CardTitle>
                </CardHeader>
                <CardContent>
                    <div className="text-2xl font-bold mb-2">
                        {value !== undefined ? value : "--"} <span className="text-sm font-normal text-muted-foreground">{unit}</span>
                    </div>
                    <div className="h-[80px] w-full">
                        <ResponsiveContainer width="100%" height="100%">
                            <LineChart data={mockData}>
                                <Tooltip
                                    contentStyle={{ backgroundColor: "#1f2937", border: "none", fontSize: "12px" }}
                                    itemStyle={{ color: "#fff" }}
                                />
                                <Line type="monotone" dataKey="val" stroke="#8884d8" strokeWidth={2} dot={false} />
                            </LineChart>
                        </ResponsiveContainer>
                    </div>
                </CardContent>
            </Card>
        );
    }

    // Render Gauge (Simple Percent for now)
    if (widgetType === "gauge") {
        return (
            <Card>
                <CardHeader className="pb-2">
                    <CardTitle className="text-sm font-medium text-muted-foreground flex items-center justify-between">
                        {title}
                        <Gauge className="h-4 w-4 text-primary" />
                    </CardTitle>
                </CardHeader>
                <CardContent>
                    <div className="text-2xl font-bold">
                        {value !== undefined ? value : "--"} <span className="text-sm font-normal text-muted-foreground">{unit}</span>
                    </div>
                    {/* Placeholder for real Gauge visual */}
                    <div className="mt-2 h-2 w-full bg-secondary rounded-full overflow-hidden">
                        <div
                            className="h-full bg-primary transition-all duration-500"
                            style={{ width: `${Math.min(value || 0, 100)}%` }}
                        />
                    </div>
                </CardContent>
            </Card>
        )
    }

    // Render Color Picker (ui:widget: color)
    if (widgetType === "color") {
        return (
            <Card>
                <CardHeader className="pb-2">
                    <CardTitle className="text-sm font-medium text-muted-foreground flex items-center justify-between">
                        {title}
                        {isLoading ? <Loader2 className="h-4 w-4 animate-spin text-primary" /> : <Zap className="h-4 w-4 text-yellow-500" />}
                    </CardTitle>
                </CardHeader>
                <CardContent>
                    <div className="flex items-center justify-between">
                        <div className="text-2xl font-bold">
                            {value || "#000000"} <span className="text-sm font-normal text-muted-foreground">{unit}</span>
                        </div>
                        <div className="flex items-center space-x-2">
                            <input
                                type="color"
                                value={value || "#000000"}
                                onChange={(e) => handleUpdate(e.target.value)}
                                disabled={isLoading}
                                className="h-8 w-12 cursor-pointer rounded border border-gray-300 p-0"
                            />
                        </div>
                    </div>
                </CardContent>
            </Card>
        );
    }

    // Default Card (Number/String/Boolean/Enum)
    return (
        <Card>
            <CardHeader className="pb-2">
                <CardTitle className="text-sm font-medium text-muted-foreground flex items-center justify-between">
                    {title}
                    {propDef.readOnly ? (
                        <Badge variant="outline" className="text-[10px]">Read-Only</Badge>
                    ) : (
                        isLoading ? <Loader2 className="h-4 w-4 animate-spin text-primary" /> : <Zap className="h-4 w-4 text-yellow-500" />
                    )}
                </CardTitle>
            </CardHeader>
            <CardContent>
                <div className="flex items-center justify-between">
                    <div className="text-2xl font-bold">
                        {propDef.enum && !propDef.readOnly ? (
                            <Select
                                value={String(value || "")}
                                onValueChange={(val) => handleUpdate(val)}
                                disabled={isLoading}
                            >
                                <SelectTrigger className="w-[140px] h-8">
                                    <SelectValue placeholder="Select..." />
                                </SelectTrigger>
                                <SelectContent>
                                    {propDef.enum.map((opt: string) => (
                                        <SelectItem key={opt} value={opt}>{opt}</SelectItem>
                                    ))}
                                </SelectContent>
                            </Select>
                        ) : (
                            <span>{value !== undefined ? value : "--"} <span className="text-sm font-normal text-muted-foreground">{unit}</span></span>
                        )}
                    </div>

                    {!propDef.readOnly && propDef.type === "boolean" && (
                        <div className="flex items-center space-x-2">
                            <Switch
                                checked={value === true}
                                onCheckedChange={(chk: boolean) => handleUpdate(chk)}
                                disabled={isLoading}
                            />
                        </div>
                    )}
                </div>
            </CardContent>
        </Card>
    );
}
