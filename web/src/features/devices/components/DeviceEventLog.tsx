import { useEffect, useState } from "react";
import { type Device } from "@/shared/types/device";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { RadioReceiver } from "lucide-react";
import { mqttService } from "@/services/mqttService";
import { useAuth } from "@/context/AuthContext";
import { format } from "date-fns";

interface DeviceEventLogProps {
    device: Device;
}

interface LogEntry {
    time: Date;
    topic: string;
    payload: any;
}

export function DeviceEventLog({ device }: DeviceEventLogProps) {
    const { user } = useAuth();
    const [logs, setLogs] = useState<LogEntry[]>([]);
    const [isConnected, setIsConnected] = useState(false);

    useEffect(() => {
        if (!user) return;

        // Connect to MQTT
        mqttService.connect(user).then(() => {
            setIsConnected(true);

            // Subscribe to Data
            const topic = `dev/${device.id}/data`;
            mqttService.subscribe(topic, (t, m) => {
                const newEntry = {
                    time: new Date(),
                    topic: t,
                    payload: m
                };
                setLogs((prev) => [newEntry, ...prev].slice(0, 50)); // Keep last 50
            });
        });

        return () => {
            const topic = `dev/${device.id}/data`;
            mqttService.unsubscribe(topic, () => { });
        };
    }, [user, device.id]);

    const td = device.thing_description;
    if (!td || !td.events) {
        // Optional: Hide if not defined? Or show generic log?
        // Let's show generic log since we subscribe to 'data' which is implicit event stream.
    }

    return (
        <Card className="col-span-2 h-[400px] flex flex-col">
            <CardHeader>
                <CardTitle className="flex items-center gap-2">
                    <RadioReceiver className="h-5 w-5" />
                    Live Event Log
                    {isConnected && <Badge variant="outline" className="text-green-500 border-green-500">Connected</Badge>}
                </CardTitle>
                <CardDescription>
                    Real-time data stream (MQTT: dev/{device.id}/data)
                </CardDescription>
            </CardHeader>
            <CardContent className="flex-1 overflow-hidden">
                <div className="h-full w-full rounded border p-4 bg-zinc-950 font-mono text-xs overflow-auto">
                    {logs.length === 0 ? (
                        <div className="text-muted-foreground text-center mt-10">Waiting for events...</div>
                    ) : (
                        logs.map((log, i) => (
                            <div key={i} className="mb-2 border-b border-white/10 pb-2 last:border-0">
                                <span className="text-muted-foreground">[{format(log.time, 'HH:mm:ss')}]</span>{' '}
                                <span className="text-blue-400">{log.topic}</span>
                                <div className="text-green-300 mt-1 pl-4 whitespace-pre-wrap">
                                    {(() => {
                                        const s = typeof log.payload === 'object' ? JSON.stringify(log.payload, null, 2) : String(log.payload);
                                        return s.length > 300 ? s.substring(0, 300) + '... (truncated)' : s;
                                    })()}
                                </div>
                            </div>
                        ))
                    )}
                </div>
            </CardContent>
        </Card>
    );
}
