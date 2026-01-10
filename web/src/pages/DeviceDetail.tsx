import { useNavigate, useParams } from 'react-router-dom';
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { deviceService } from '@/services/deviceService';
import { Button } from '@/components/ui/button';
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from '@/components/ui/card';
import { Badge } from '@/components/ui/badge';
import { ArrowLeft, Trash2, Calendar, HardDrive, Globe, Activity } from 'lucide-react';
import { format } from 'date-fns';

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

                {/* Placeholder for Commands / Telemetry */}
                <Card>
                    <CardHeader>
                        <CardTitle>Actions & State</CardTitle>
                        <CardDescription>Control and monitor your device</CardDescription>
                    </CardHeader>
                    <CardContent>
                        <div className="flex h-32 items-center justify-center rounded-md border border-dashed text-muted-foreground">
                            Telemetry & Command Center Coming Soon
                        </div>
                    </CardContent>
                </Card>
            </div>
        </div>
    );
}
