import { useQuery } from '@tanstack/react-query';
import { deviceService } from '@/services/deviceService';
import { useNavigate } from 'react-router-dom';
import { Button } from '@/components/ui/button';
import {
    Table,
    TableBody,
    TableCell,
    TableHead,
    TableHeader,
    TableRow,
} from '@/components/ui/table';
import { Badge } from '@/components/ui/badge';
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from '@/components/ui/card';
import { RefreshCw, Plus, Monitor } from 'lucide-react';
import { formatDistanceToNow } from 'date-fns';

export default function Devices() {
    const navigate = useNavigate();
    const { data: devices, isLoading, isError, refetch } = useQuery({
        queryKey: ['devices'],
        queryFn: deviceService.getAll,
    });

    return (
        <div className="space-y-6">
            <div className="flex items-center justify-between">
                <div>
                    <h1 className="text-3xl font-bold tracking-tight">Devices</h1>
                    <p className="text-muted-foreground">
                        Manage and monitor your connected IoT devices
                    </p>
                </div>
                <div className="flex gap-2">
                    <Button variant="outline" size="sm" onClick={() => refetch()} disabled={isLoading}>
                        <RefreshCw className={`mr-2 h-4 w-4 ${isLoading ? 'animate-spin' : ''}`} />
                        Refresh
                    </Button>
                    <Button size="sm">
                        <Plus className="mr-2 h-4 w-4" />
                        Add Device
                    </Button>
                </div>
            </div>

            <Card>
                <CardHeader>
                    <CardTitle>Device List</CardTitle>
                    <CardDescription>
                        A list of all devices registered in your account.
                    </CardDescription>
                </CardHeader>
                <CardContent>
                    {isError ? (
                        <div className="p-4 text-center text-red-500">Failed to load devices</div>
                    ) : isLoading ? (
                        <div className="p-8 text-center text-muted-foreground">Loading devices...</div>
                    ) : devices?.length === 0 ? (
                        <div className="p-12 text-center border-dashed border-2 rounded-lg">
                            <Monitor className="mx-auto h-12 w-12 text-muted-foreground/50" />
                            <h3 className="mt-4 text-lg font-semibold">No devices found</h3>
                            <p className="text-sm text-muted-foreground">Get started by creating a new device.</p>
                        </div>
                    ) : (
                        <Table>
                            <TableHeader>
                                <TableRow>
                                    <TableHead>Status</TableHead>
                                    <TableHead>Name</TableHead>
                                    <TableHead>Type</TableHead>
                                    <TableHead>Public IP</TableHead>
                                    <TableHead>Last Seen</TableHead>
                                    <TableHead className="text-right">Actions</TableHead>
                                </TableRow>
                            </TableHeader>
                            <TableBody>
                                {devices?.map((device) => (
                                    <TableRow
                                        key={device.id}
                                        className="cursor-pointer hover:bg-muted/50"
                                        onClick={() => navigate(`/devices/${device.id}`)}
                                    >
                                        <TableCell>
                                            <Badge variant={device.status === 'online' ? 'default' : 'secondary'} className={device.status === 'online' ? 'bg-green-500 hover:bg-green-600' : ''}>
                                                {device.status}
                                            </Badge>
                                        </TableCell>
                                        <TableCell className="font-medium">
                                            <div className="flex flex-col">
                                                <span>{device.name}</span>
                                                <span className="text-xs text-muted-foreground font-mono">{device.device_uid || device.id}</span>
                                            </div>
                                        </TableCell>
                                        <TableCell>{device.type}</TableCell>
                                        <TableCell className="font-mono text-xs">{device.public_ip || '-'}</TableCell>
                                        <TableCell className="text-muted-foreground text-sm">
                                            {device.last_seen ? formatDistanceToNow(new Date(device.last_seen), { addSuffix: true }) : 'Never'}
                                        </TableCell>
                                        <TableCell className="text-right">
                                            <Button variant="ghost" size="sm">Manage</Button>
                                        </TableCell>
                                    </TableRow>
                                ))}
                            </TableBody>
                        </Table>
                    )}
                </CardContent>
            </Card>
        </div>
    );
}
