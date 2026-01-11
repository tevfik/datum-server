import { useState } from 'react';
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { deviceService } from '@/services/deviceService';
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Label } from '@/components/ui/label';
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '@/components/ui/select';
import { Textarea } from '@/components/ui/textarea';
import { Badge } from '@/components/ui/badge';
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { Terminal, Send, RefreshCw, Zap, Play, Square, Camera, Sun, Power } from 'lucide-react';
import { formatDistanceToNow } from 'date-fns';

export default function Commands() {
    const [selectedDeviceId, setSelectedDeviceId] = useState<string>("");

    // Fetch Devices
    const { data: devices } = useQuery({
        queryKey: ['devices'],
        queryFn: deviceService.getAll,
    });

    return (
        <div className="space-y-6 container mx-auto max-w-5xl">
            <div className="flex flex-col gap-2">
                <h1 className="text-3xl font-bold tracking-tight">Command Center</h1>
                <p className="text-muted-foreground">Send remote commands to your devices.</p>
            </div>

            <div className="grid gap-6 md:grid-cols-12">
                <div className="md:col-span-4 space-y-4">
                    <Card>
                        <CardHeader>
                            <CardTitle>Select Target</CardTitle>
                            <CardDescription>Choose a device to command</CardDescription>
                        </CardHeader>
                        <CardContent>
                            <Select value={selectedDeviceId} onValueChange={setSelectedDeviceId}>
                                <SelectTrigger>
                                    <SelectValue placeholder="Select a device..." />
                                </SelectTrigger>
                                <SelectContent>
                                    {devices?.map((d) => (
                                        <SelectItem key={d.id} value={d.id}>
                                            <div className="flex items-center gap-2">
                                                <div className={`h-2 w-2 rounded-full ${d.status === 'online' ? 'bg-green-500' : 'bg-gray-400'}`} />
                                                <span>{d.name}</span>
                                                <span className="text-xs text-muted-foreground font-mono">({d.id})</span>
                                            </div>
                                        </SelectItem>
                                    ))}
                                </SelectContent>
                            </Select>
                        </CardContent>
                    </Card>

                    {selectedDeviceId && (
                        <CommandHistory deviceId={selectedDeviceId} />
                    )}
                </div>

                <div className="md:col-span-8">
                    {selectedDeviceId ? (
                        <CommandBuilder deviceId={selectedDeviceId} />
                    ) : (
                        <Card className="h-full flex items-center justify-center p-12 text-muted-foreground border-dashed">
                            <div className="text-center">
                                <Terminal className="h-12 w-12 mx-auto mb-4 opacity-20" />
                                <p>Select a device to start sending commands</p>
                            </div>
                        </Card>
                    )}
                </div>
            </div>
        </div>
    );
}

function CommandBuilder({ deviceId }: { deviceId: string }) {
    const [activeTab, setActiveTab] = useState("presets");
    const [customAction, setCustomAction] = useState("");
    const [customParams, setCustomParams] = useState("{}");
    const queryClient = useQueryClient();

    const sendMutation = useMutation({
        mutationFn: (data: { action: string; params: any }) =>
            deviceService.sendCommand(deviceId, data),
        onSuccess: (data) => {
            queryClient.invalidateQueries({ queryKey: ['commands', deviceId] });
            // Don't alert on presets for smoother flow, maybe toast?
            // For now just console
            console.log("Command sent", data);
        },
        onError: (err: any) => {
            alert(`Failed: ${err.response?.data?.error || err.message}`);
        }
    });

    const send = (action: string, params: any = {}) => {
        sendMutation.mutate({ action, params });
    };

    return (
        <Card>
            <CardHeader>
                <CardTitle className="flex items-center gap-2">
                    <Zap className="h-5 w-5 text-yellow-500" />
                    New Command
                </CardTitle>
                <CardDescription>Send an instruction to {deviceId}</CardDescription>
            </CardHeader>
            <CardContent>
                <Tabs value={activeTab} onValueChange={setActiveTab}>
                    <TabsList className="grid w-full grid-cols-2">
                        <TabsTrigger value="presets">Quick Actions</TabsTrigger>
                        <TabsTrigger value="custom">Custom JSON</TabsTrigger>
                    </TabsList>

                    <TabsContent value="presets" className="space-y-4 pt-4">
                        <div className="grid grid-cols-2 gap-4">
                            {/* Streaming Controls */}
                            <div className="space-y-2 p-4 border rounded-md bg-muted/20">
                                <h3 className="font-medium text-sm flex items-center gap-2"><Camera className="h-4 w-4" /> Streaming</h3>
                                <div className="grid grid-cols-2 gap-2">
                                    <Button variant="outline" size="sm" onClick={() => send("stream", { state: "on" })} disabled={sendMutation.isPending}>
                                        <Play className="mr-2 h-3 w-3" /> Start
                                    </Button>
                                    <Button variant="outline" size="sm" onClick={() => send("stream", { state: "off" })} disabled={sendMutation.isPending}>
                                        <Square className="mr-2 h-3 w-3" /> Stop
                                    </Button>
                                    <Button variant="secondary" size="sm" className="col-span-2" onClick={() => send("snap", { resolution: "HD" })} disabled={sendMutation.isPending}>
                                        <Camera className="mr-2 h-3 w-3" /> Take Snapshot
                                    </Button>
                                </div>
                            </div>

                            {/* System Controls */}
                            <div className="space-y-2 p-4 border rounded-md bg-muted/20">
                                <h3 className="font-medium text-sm flex items-center gap-2"><Power className="h-4 w-4" /> System</h3>
                                <div className="grid grid-cols-1 gap-2">
                                    <Button variant="destructive" size="sm" onClick={() => send("restart")} disabled={sendMutation.isPending}>
                                        <RefreshCw className="mr-2 h-3 w-3" /> Reboot Device
                                    </Button>
                                    <Button variant="outline" size="sm" onClick={() => send("update_firmware", { url: "" })} disabled={sendMutation.isPending}>
                                        <RefreshCw className="mr-2 h-3 w-3" /> OTA Update...
                                    </Button>
                                </div>
                            </div>

                            {/* LED Controls */}
                            <div className="space-y-2 p-4 border rounded-md bg-muted/20">
                                <h3 className="font-medium text-sm flex items-center gap-2"><Sun className="h-4 w-4" /> LED / Flash</h3>
                                <div className="grid grid-cols-2 gap-2">
                                    <Button variant="outline" size="sm" onClick={() => send("led", { led: true })} disabled={sendMutation.isPending}>
                                        On
                                    </Button>
                                    <Button variant="outline" size="sm" onClick={() => send("led", { led: false })} disabled={sendMutation.isPending}>
                                        Off
                                    </Button>
                                    <Button variant="outline" size="sm" className="col-span-2" onClick={() => send("update_settings", { lcol: "#FFFFFF" })} disabled={sendMutation.isPending}>
                                        White
                                    </Button>
                                    <Button variant="outline" size="sm" className="col-span-2" onClick={() => send("update_settings", { lcol: "#FF0000" })} disabled={sendMutation.isPending}>
                                        Red
                                    </Button>
                                </div>
                            </div>

                            {/* Config Controls */}
                            <div className="space-y-2 p-4 border rounded-md bg-muted/20">
                                <h3 className="font-medium text-sm flex items-center gap-2"><Terminal className="h-4 w-4" /> Config</h3>
                                <div className="grid grid-cols-1 gap-2">
                                    <Button variant="outline" size="sm" onClick={() => send("update_settings", { vres: "VGA" })} disabled={sendMutation.isPending}>
                                        Set VGA (Low Res)
                                    </Button>
                                    <Button variant="outline" size="sm" onClick={() => send("update_settings", { vres: "HD" })} disabled={sendMutation.isPending}>
                                        Set HD (High Res)
                                    </Button>
                                </div>
                            </div>
                        </div>
                    </TabsContent>

                    <TabsContent value="custom" className="space-y-4 pt-4">
                        <div className="space-y-2">
                            <Label>Action</Label>
                            <Input
                                placeholder="e.g. reboot"
                                value={customAction}
                                onChange={(e) => setCustomAction(e.target.value)}
                            />
                        </div>
                        <div className="space-y-2">
                            <Label>Parameters (JSON)</Label>
                            <Textarea
                                placeholder="{}"
                                className="font-mono"
                                rows={5}
                                value={customParams}
                                onChange={(e) => setCustomParams(e.target.value)}
                            />
                        </div>
                        <Button className="w-full" onClick={() => {
                            try {
                                const p = JSON.parse(customParams);
                                send(customAction, p);
                            } catch (e) { alert("Invalid JSON"); }
                        }} disabled={sendMutation.isPending}>
                            <Send className="mr-2 h-4 w-4" /> Send Raw Command
                        </Button>
                    </TabsContent>
                </Tabs>
            </CardContent>
        </Card>
    );
}

function CommandHistory({ deviceId }: { deviceId: string }) {
    const { data: commands } = useQuery({
        queryKey: ['commands', deviceId],
        queryFn: () => deviceService.getCommands(deviceId),
        refetchInterval: 3000,
    });

    return (
        <Card className="flex-1">
            <CardHeader className="pb-3">
                <CardTitle className="text-base">History</CardTitle>
            </CardHeader>
            <CardContent className="px-3 pb-3">
                {!commands || commands.length === 0 ? (
                    <div className="text-sm text-muted-foreground text-center py-4">No recent commands</div>
                ) : (
                    <div className="max-h-[500px] overflow-y-auto space-y-2 pr-1">
                        {commands.map((cmd) => (
                            <div key={cmd.command_id} className="text-xs border p-3 rounded-md bg-background flex flex-col gap-2">
                                <div className="flex justify-between items-start">
                                    <span className="font-bold font-mono text-primary">{cmd.action}</span>
                                    <Badge variant={cmd.status === 'success' ? 'default' : cmd.status === 'failed' ? 'destructive' : 'outline'} className="text-[10px] capitalize">
                                        {cmd.status || 'pending'}
                                    </Badge>
                                </div>

                                {cmd.params && Object.keys(cmd.params).length > 0 && (
                                    <div className="bg-muted/50 p-2 rounded text-[10px] font-mono break-all text-muted-foreground">
                                        {JSON.stringify(cmd.params)}
                                    </div>
                                )}

                                <div className="flex justify-between text-[10px] text-muted-foreground pt-1 border-t mt-1">
                                    <span>{new Date(cmd.created_at).toLocaleTimeString()}</span>
                                    <span>{formatDistanceToNow(new Date(cmd.created_at), { addSuffix: true })}</span>
                                </div>
                            </div>
                        ))}
                    </div>
                )}
            </CardContent>
        </Card>
    );
}
