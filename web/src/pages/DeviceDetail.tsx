import { useNavigate, useParams } from 'react-router-dom';
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { deviceService } from '@/services/deviceService';
import { Button } from '@/components/ui/button';
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from '@/components/ui/card';
import { Badge } from '@/components/ui/badge';
import { Input } from '@/components/ui/input';
import { Label } from '@/components/ui/label';
import { Textarea } from '@/components/ui/textarea';
import { ArrowLeft, Trash2, Calendar, HardDrive, Globe, Activity, Terminal, Send } from 'lucide-react';
import { format, formatDistanceToNow } from 'date-fns';
import { useState } from 'react';

export default function DeviceDetail() {
    const { id } = useParams<{ id: string }>();
    const navigate = useNavigate();
    const queryClient = useQueryClient();

    const { data: device, isLoading, error } = useQuery({
        queryKey: ['device', id],
        queryFn: () => deviceService.getById(id!),
        enabled: !!id,
    });

    const deleteMutation = useMutation({
        mutationFn: deviceService.delete,
        onSuccess: () => {
            queryClient.invalidateQueries({ queryKey: ['devices'] });
            navigate('/devices');
        },
    });

    if (isLoading) return <div className="p-8">Loading device details...</div>;
    if (error || !device) return <div className="p-8 text-destructive">Device not found</div>;

    return (
        <div className="space-y-6 p-6">
            {/* Header */}
            <div className="flex items-center justify-between">
                <div className="flex items-center gap-4">
                    <Button variant="outline" size="icon" onClick={() => navigate('/devices')}>
                        <ArrowLeft className="h-4 w-4" />
                    </Button>
                    <div>
                        <h1 className="text-3xl font-bold tracking-tight">{device.name}</h1>
                        <div className="flex items-center gap-2 text-muted-foreground">
                            <span className="text-sm font-mono">{device.id}</span>
                            <Badge variant={device.status === 'online' ? 'default' : 'secondary'}>
                                {device.status}
                            </Badge>
                        </div>
                    </div>
                </div>
                <Button
                    variant="destructive"
                    onClick={() => {
                        if (confirm('Are you sure you want to delete this device? This action cannot be undone.')) {
                            deleteMutation.mutate(device.id);
                        }
                    }}
                    disabled={deleteMutation.isPending}
                >
                    <Trash2 className="mr-2 h-4 w-4" />
                    Delete Device
                </Button>
            </div>

            <div className="grid gap-6 md:grid-cols-2">
                {/* Device Info Card */}
                <Card>
                    <CardHeader>
                        <CardTitle>Device Information</CardTitle>
                        <CardDescription>Technical details and connectivity status</CardDescription>
                    </CardHeader>
                    <CardContent className="space-y-4">
                        <div className="grid grid-cols-2 gap-4">
                            <div className="space-y-1">
                                <p className="text-sm font-medium leading-none text-muted-foreground">Type</p>
                                <div className="flex items-center gap-2">
                                    <HardDrive className="h-4 w-4 text-primary" />
                                    <span className="font-medium capitalize">{device.type}</span>
                                </div>
                            </div>
                            <div className="space-y-1">
                                <p className="text-sm font-medium leading-none text-muted-foreground">Public IP</p>
                                <div className="flex items-center gap-2">
                                    <Globe className="h-4 w-4 text-primary" />
                                    <span className="font-mono">{device.public_ip || 'N/A'}</span>
                                </div>
                            </div>
                            <div className="space-y-1">
                                <p className="text-sm font-medium leading-none text-muted-foreground">Last Seen</p>
                                <div className="flex items-center gap-2">
                                    <Activity className="h-4 w-4 text-primary" />
                                    <span>{format(new Date(device.last_seen), 'PPP p')}</span>
                                </div>
                            </div>
                            <div className="space-y-1">
                                <p className="text-sm font-medium leading-none text-muted-foreground">Created At</p>
                                <div className="flex items-center gap-2">
                                    <Calendar className="h-4 w-4 text-primary" />
                                    <span>{format(new Date(device.created_at), 'PPP')}</span>
                                </div>
                            </div>
                        </div>
                    </CardContent>
                </Card>

                {/* Command Center */}
                <Card className="flex flex-col">
                    <CardHeader>
                        <CardTitle className="flex items-center gap-2">
                            <Terminal className="h-5 w-5" />
                            Command Center
                        </CardTitle>
                        <CardDescription>Send commands to your device</CardDescription>
                    </CardHeader>
                    <CardContent className="space-y-4">
                        <CommandSender deviceId={device.id} />
                        <CommandHistory deviceId={device.id} />
                    </CardContent>
                </Card>
            </div>
        </div>
    );
}

function CommandSender({ deviceId }: { deviceId: string }) {
    const [action, setAction] = useState('reboot');
    const [params, setParams] = useState('{}');
    const queryClient = useQueryClient();

    const sendMutation = useMutation({
        mutationFn: (data: { action: string; params: any }) =>
            deviceService.sendCommand(deviceId, data),
        onSuccess: (data) => {
            alert(`Command sent! ID: ${data.command_id}`);
            queryClient.invalidateQueries({ queryKey: ['commands', deviceId] });
            setParams('{}');
        },
        onError: (err) => {
            alert('Failed to send command');
            console.error(err);
        }
    });

    const handleSend = () => {
        try {
            const parsedParams = JSON.parse(params);
            sendMutation.mutate({ action, params: parsedParams });
        } catch (e: any) {
            alert('Invalid JSON params');
        }
    };

    return (
        <div className="space-y-3 border rounded-lg p-4 bg-muted/20">
            <div className="grid gap-2">
                <Label>Action</Label>
                <Input
                    placeholder="e.g. reboot, update_config"
                    value={action}
                    onChange={(e) => setAction(e.target.value)}
                />
            </div>
            <div className="grid gap-2">
                <Label>Parameters (JSON)</Label>
                <Textarea
                    placeholder="{}"
                    className="font-mono text-xs"
                    value={params}
                    onChange={(e) => setParams(e.target.value)}
                />
            </div>
            <Button className="w-full" onClick={handleSend} disabled={sendMutation.isPending}>
                <Send className="mr-2 h-4 w-4" />
                {sendMutation.isPending ? 'Sending...' : 'Send Command'}
            </Button>
        </div>
    );
}

function CommandHistory({ deviceId }: { deviceId: string }) {
    const { data: commands, isLoading } = useQuery({
        queryKey: ['commands', deviceId],
        queryFn: () => deviceService.getCommands(deviceId),
        refetchInterval: 5000,
    });

    if (isLoading) return <div className="text-sm text-muted-foreground">Loading history...</div>;

    return (
        <div className="space-y-2">
            <h4 className="text-sm font-medium text-muted-foreground">Pending / Recent Commands</h4>
            {!commands || commands.length === 0 ? (
                <div className="text-xs text-muted-foreground italic">No pending commands</div>
            ) : (
                <div className="max-h-40 overflow-y-auto space-y-2">
                    {commands.map((cmd) => (
                        <div key={cmd.command_id} className="text-xs border p-2 rounded flex justify-between items-center bg-background">
                            <div>
                                <span className="font-bold text-primary">{cmd.action}</span>
                                <span className="ml-2 text-muted-foreground">
                                    {formatDistanceToNow(new Date(cmd.created_at), { addSuffix: true })}
                                </span>
                            </div>
                            <Badge variant="outline" className="text-[10px]">{cmd.status || 'pending'}</Badge>
                        </div>
                    ))}
                </div>
            )}
        </div>
    );
}
