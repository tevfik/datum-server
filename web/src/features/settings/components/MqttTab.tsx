import { useState, useEffect, useRef } from "react";
import { useQuery, useMutation } from "@tanstack/react-query";
import { adminService } from "@/features/settings/services/adminService";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from "@/components/ui/table";
import { AlertCircle, RefreshCw, Send, Wifi, Play, Square, PlugZap } from "lucide-react";
import mqtt from "mqtt";
import { useAuth } from "@/context/AuthContext";
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select";

interface MqttMessage {
    id: number;
    topic: string;
    payload: string;
    timestamp: Date;
    retain: boolean;
}

import { deviceService } from "@/features/devices/services/deviceService";

export function MqttTab() {
    const { user } = useAuth();
    // Publish State
    const [topic, setTopic] = useState("");
    const [message, setMessage] = useState("");
    const [retain, setRetain] = useState(false);
    const [publishError, setPublishError] = useState("");

    // Subscribe State
    const [subTopic, setSubTopic] = useState("#");
    const [apiKey, setApiKey] = useState("");
    const [wsUrl, setWsUrl] = useState(() => {
        const protocol = window.location.protocol === 'https:' ? 'wss' : 'ws';
        return `${protocol}://${window.location.host}/mqtt`;
    });
    const [isConnected, setIsConnected] = useState(false);
    const [messages, setMessages] = useState<MqttMessage[]>([]);
    const clientRef = useRef<mqtt.MqttClient | null>(null);

    // Fetch Stats (Admin only)
    const { data: stats } = useQuery({
        queryKey: ['mqtt-stats'],
        queryFn: adminService.getMqttStats,
        refetchInterval: 5000,
        enabled: user?.role === 'admin',
    });

    // Fetch Clients (Admin only)
    const { data: clients } = useQuery({
        queryKey: ['mqtt-clients'],
        queryFn: adminService.getMqttClients,
        refetchInterval: 5000,
        enabled: user?.role === 'admin',
    });

    // Fetch Devices (For Topic Autocomplete - All Users)
    const { data: devices } = useQuery({
        queryKey: ['devices'],
        queryFn: deviceService.getAll,
    });

    // Publish Mutation (Admin usage via HTTP)
    const publishMutation = useMutation({
        mutationFn: adminService.publishMqttMessage,
        onSuccess: () => {
            setTopic("");
            setMessage("");
            setPublishError("");
        },
        onError: (err: any) => {
            setPublishError(err.response?.data?.error || "Failed to publish message via API");
        }
    });

    const handlePublish = (e: React.FormEvent) => {
        e.preventDefault();

        // 1. Admin: Use HTTP API (God Mode)
        if (user?.role === 'admin') {
            publishMutation.mutate({ topic, message, retain });
            return;
        }

        // 2. User: Use WebSocket (Client Mode)
        if (!isConnected || !clientRef.current) {
            setPublishError("You must be connected to the Live Monitor (WebSocket) to publish.");
            return;
        }

        try {
            clientRef.current.publish(topic, message, { retain }, (err) => {
                if (err) {
                    setPublishError("MQTT Publish Error: " + err.message);
                } else {
                    setTopic("");
                    setMessage("");
                    setPublishError("");
                    // Optimistically add to log? No, we subscribed to it likely.
                }
            });
        } catch (err: any) {
            setPublishError("Failed to publish: " + err.message);
        }
    };

    // Subscribe Handler... (Keep existing handleConnectToggle logic)
    const handleConnectToggle = () => {
        if (isConnected) {
            if (clientRef.current) {
                clientRef.current.end();
                clientRef.current = null;
            }
            setIsConnected(false);
            return;
        }

        if (!apiKey) {
            alert("Please provide an API Key for authentication");
            return;
        }

        try {
            const client = mqtt.connect(wsUrl, {
                username: 'admin_dashboard_' + Math.floor(Math.random() * 1000),
                password: apiKey,
                clean: true,
                clientId: `datum_web_${user?.role}_${user?.id}_${Math.floor(Math.random() * 10000)}`,
            });

            client.on('connect', () => {
                console.log("MQTT Connected via WS");
                setIsConnected(true);
                setPublishError(""); // Clear previous errors
                client.subscribe(subTopic, (err) => {
                    if (err) {
                        console.error("Subsription error", err);
                    }
                });
            });

            client.on('message', (topic, payload, packet) => {
                const msg: MqttMessage = {
                    id: Date.now() + Math.random(),
                    topic: topic,
                    payload: payload.toString(),
                    timestamp: new Date(),
                    retain: packet.retain
                };
                setMessages(prev => [msg, ...prev].slice(0, 100)); // Keep last 100
            });

            client.on('error', (err) => {
                console.error("MQTT Error", err);
                setIsConnected(false);
                client.end();
                setPublishError("Connection Error: " + err.message);
            });

            client.on('close', () => {
                if (isConnected) setIsConnected(false);
            });

            clientRef.current = client;

        } catch (e) {
            console.error("Connection failed", e);
            alert("Connection failed");
        }
    };

    // Cleanup... (Keep existing useEffect)
    useEffect(() => {
        return () => {
            if (clientRef.current) {
                clientRef.current.end();
            }
        };
    }, []);

    const onDeviceSelect = (deviceId: string) => {
        // Find device
        const device = devices?.find(d => d.id === deviceId);
        if (device) {
            // Suggest a topic. Priority: specific UID -> ID
            // Firmware subscribes to "cmd/" + deviceID. Use ID.
            const target = device.id;
            setTopic(`cmd/${target}`);
        }
    };

    return (
        <div className="space-y-6">
            {/* Stats Cards (Admin Only) */}
            {user?.role === 'admin' && (
                <div className="grid gap-4 md:grid-cols-2 lg:grid-cols-4">
                    <Card>
                        <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
                            <CardTitle className="text-sm font-medium">Clients Connected</CardTitle>
                            <Wifi className="h-4 w-4 text-muted-foreground" />
                        </CardHeader>
                        <CardContent>
                            <div className="text-2xl font-bold">{stats?.clients_connected ?? '-'}</div>
                        </CardContent>
                    </Card>
                    <Card>
                        <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
                            <CardTitle className="text-sm font-medium">Subscriptions</CardTitle>
                        </CardHeader>
                        <CardContent>
                            <div className="text-2xl font-bold">{stats?.subscriptions ?? '-'}</div>
                        </CardContent>
                    </Card>
                    <Card>
                        <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
                            <CardTitle className="text-sm font-medium">Bytes Sent</CardTitle>
                        </CardHeader>
                        <CardContent>
                            <div className="text-2xl font-bold">{stats?.bytes_sent ?? '-'}</div>
                        </CardContent>
                    </Card>
                    <Card>
                        <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
                            <CardTitle className="text-sm font-medium">Bytes Recv</CardTitle>
                        </CardHeader>
                        <CardContent>
                            <div className="text-2xl font-bold">{stats?.bytes_recv ?? '-'}</div>
                        </CardContent>
                    </Card>
                </div>
            )}

            <div className={`grid gap-4 ${user?.role === 'admin' ? 'md:grid-cols-2 lg:grid-cols-7' : 'grid-cols-1'}`}>
                {/* Publish Form (Visible to All) */}
                <Card className={user?.role === 'admin' ? 'col-span-3' : 'col-span-1'}>
                    <CardHeader>
                        <CardTitle>Publish Message</CardTitle>
                        <CardDescription>
                            {user?.role === 'admin'
                                ? "Send an MQTT message via API (Admin override)"
                                : "Send a message via WebSocket (Subject to device ownership ACLs)"}
                        </CardDescription>
                    </CardHeader>
                    <CardContent>
                        <form onSubmit={handlePublish} className="space-y-4">
                            {publishError && (
                                <div className="flex items-center gap-2 rounded-md bg-destructive/15 p-3 text-sm text-destructive">
                                    <AlertCircle className="h-4 w-4" />
                                    <p>{publishError}</p>
                                </div>
                            )}

                            {/* Device Selector Helper */}
                            <div className="space-y-2">
                                <Label>Pick Device (Auto-fill Topic)</Label>
                                <Select onValueChange={onDeviceSelect}>
                                    <SelectTrigger>
                                        <SelectValue placeholder="Select a device to target..." />
                                    </SelectTrigger>
                                    <SelectContent>
                                        {devices?.map((device) => (
                                            <SelectItem key={device.id} value={device.id}>
                                                {device.name} <span className="text-xs text-muted-foreground">({device.device_uid || device.id})</span>
                                            </SelectItem>
                                        ))}
                                    </SelectContent>
                                </Select>
                            </div>

                            <div className="space-y-2">
                                <Label htmlFor="topic">Topic</Label>
                                <Input
                                    id="topic"
                                    placeholder="cmd/device_123"
                                    value={topic}
                                    onChange={(e) => setTopic(e.target.value)}
                                    required
                                />
                                <p className="text-xs text-muted-foreground">
                                    Users can only publish to topics containing their device IDs.
                                </p>
                            </div>
                            <div className="space-y-2">
                                <Label htmlFor="message">Message</Label>
                                <Input
                                    id="message"
                                    placeholder='{"action": "reboot"}'
                                    value={message}
                                    onChange={(e) => setMessage(e.target.value)}
                                    required
                                />
                            </div>
                            <div className="flex items-center space-x-2">
                                <input
                                    type="checkbox"
                                    id="retain"
                                    checked={retain}
                                    onChange={(e) => setRetain(e.target.checked)}
                                    className="h-4 w-4 rounded border-gray-300 text-primary focus:ring-primary"
                                />
                                <Label htmlFor="retain">Retain Message</Label>
                            </div>
                            <Button type="submit" disabled={publishMutation.isPending && user?.role === 'admin'} className="w-full">
                                {publishMutation.isPending && user?.role === 'admin' ? <RefreshCw className="mr-2 h-4 w-4 animate-spin" /> : <Send className="mr-2 h-4 w-4" />}
                                Publish
                            </Button>
                        </form>
                    </CardContent>
                </Card>

                {/* Clients List (Admin Only) */}
                {user?.role === 'admin' && (
                    <Card className="col-span-4 max-h-[500px] flex flex-col">
                        <CardHeader>
                            <CardTitle>Connected Clients</CardTitle>
                            <CardDescription>
                                List of currently currently connected MQTT clients ({clients?.length || 0})
                            </CardDescription>
                        </CardHeader>
                        <CardContent className="overflow-auto">
                            <Table>
                                <TableHeader>
                                    <TableRow>
                                        <TableHead>Client ID</TableHead>
                                        <TableHead>IP Address</TableHead>
                                        <TableHead>Status</TableHead>
                                    </TableRow>
                                </TableHeader>
                                <TableBody>
                                    {clients?.map((client) => (
                                        <TableRow key={client.id}>
                                            <TableCell className="font-mono text-xs">{client.id}</TableCell>
                                            <TableCell>{client.ip}</TableCell>
                                            <TableCell>
                                                <span className={`inline-flex items-center rounded-full px-2.5 py-0.5 text-xs font-medium ${client.connected ? 'bg-green-100 text-green-800' : 'bg-red-100 text-red-800'}`}>
                                                    {client.connected ? 'Connected' : 'Disconnected'}
                                                </span>
                                            </TableCell>
                                        </TableRow>
                                    ))}
                                    {clients?.length === 0 && (
                                        <TableRow>
                                            <TableCell colSpan={3} className="text-center text-muted-foreground">
                                                No clients connected.
                                            </TableCell>
                                        </TableRow>
                                    )}
                                </TableBody>
                            </Table>
                        </CardContent>
                    </Card>
                )}
            </div>

            {/* Subscribe Section is preserved below... */}
            <Card>
                {/* ... keep logic ... */}
                <CardHeader>
                    {/* ... */}

                    <div className="flex items-center justify-between">
                        <div>
                            <CardTitle>Live Monitor</CardTitle>
                            <CardDescription>Subscribe to topics and view real-time messages via WebSocket</CardDescription>
                        </div>
                        <div className="flex items-center gap-2">
                            <span className={`h-2 w-2 rounded-full ${isConnected ? 'bg-green-500' : 'bg-red-500'}`} />
                            <span className="text-sm font-medium text-muted-foreground">{isConnected ? 'Connected' : 'Disconnected'}</span>
                        </div>
                    </div>
                </CardHeader>
                <CardContent className="space-y-4">
                    <div className="grid md:grid-cols-4 gap-4 items-end">
                        <div className="space-y-2 md:col-span-2">
                            <Label>Broker URL (WS)</Label>
                            <Input value={wsUrl} onChange={(e) => setWsUrl(e.target.value)} disabled={isConnected} placeholder="ws://localhost:1884" />
                        </div>
                        <div className="space-y-2">
                            <Label>API Key (Auth)</Label>
                            <Input
                                type="password"
                                value={apiKey}
                                onChange={(e) => setApiKey(e.target.value)}
                                disabled={isConnected}
                                placeholder="ak_..."
                            />
                        </div>
                        <div className="space-y-2">
                            <Button
                                variant={isConnected ? "destructive" : "default"}
                                onClick={handleConnectToggle}
                                className="w-full"
                            >
                                {isConnected ? <Square className="mr-2 h-4 w-4" /> : <Play className="mr-2 h-4 w-4" />}
                                {isConnected ? "Disconnect" : "Connect"}
                            </Button>
                        </div>
                    </div>

                    <div className="flex gap-4 items-end border-t pt-4">
                        <div className="flex-1 space-y-2">
                            <Label>Subscription Topic</Label>
                            <Input
                                value={subTopic}
                                onChange={(e) => setSubTopic(e.target.value)}
                                placeholder="#"
                            />
                        </div>
                        <Button
                            variant="secondary"
                            disabled={!isConnected}
                            onClick={() => {
                                clientRef.current?.subscribe(subTopic);
                            }}
                        >
                            <PlugZap className="mr-2 h-4 w-4" /> Update
                        </Button>
                        <Button variant="outline" onClick={() => setMessages([])}>
                            Clear Log
                        </Button>
                    </div>

                    <div className="h-[300px] border rounded-md bg-zinc-950 p-4 overflow-y-auto font-mono text-xs text-green-400">
                        {messages.length === 0 ? (
                            <div className="text-zinc-500 text-center py-10">Waiting for messages...</div>
                        ) : (
                            messages.map((msg) => (
                                <div key={msg.id} className="mb-2 break-all border-b border-zinc-800 pb-2 last:border-0 hover:bg-zinc-900/50 p-1 rounded transition-colors">
                                    <div className="flex gap-2 text-zinc-500 mb-1">
                                        <span>[{msg.timestamp.toLocaleTimeString()}]</span>
                                        <span className="text-blue-400">{msg.topic}</span>
                                        {msg.retain && <span className="text-yellow-500 bg-yellow-500/10 px-1 rounded text-[10px]">RETAIN</span>}
                                    </div>
                                    <div className="pl-4 text-zinc-300">
                                        {msg.payload}
                                    </div>
                                </div>
                            ))
                        )}
                    </div>
                </CardContent>
            </Card>
        </div>
    );
}
