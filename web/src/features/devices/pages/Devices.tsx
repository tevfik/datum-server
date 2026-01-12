import { useState } from 'react';
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { deviceService } from '@/features/devices/services/deviceService';
import { useNavigate } from 'react-router-dom';
import { useAuth } from '@/shared/context/AuthContext';
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
import { RefreshCw, Plus, Monitor, Pencil, Trash2, Pause, Play } from 'lucide-react';
import { formatDistanceToNow } from 'date-fns';
import { SortableHeader } from '@/components/ui/sortable-header';
import { AddDeviceModal } from '@/features/devices/components/AddDeviceModal';

export default function Devices() {
    const navigate = useNavigate();
    const queryClient = useQueryClient();
    const { user } = useAuth();
    const isAdmin = user?.role === 'admin';
    const [isAddOpen, setIsAddOpen] = useState(false);
    const [sortColumn, setSortColumn] = useState('status');
    const [sortDirection, setSortDirection] = useState<'asc' | 'desc'>('asc');

    const { data: devices, isLoading, isError, refetch } = useQuery({
        queryKey: ['devices', isAdmin],
        queryFn: isAdmin ? deviceService.getAllAdmin : deviceService.getAll,
    });

    const updateStatusMutation = useMutation({
        mutationFn: (params: { id: string; status: 'active' | 'suspended' }) =>
            deviceService.updateStatus(params.id, params.status),
        onSuccess: () => {
            queryClient.invalidateQueries({ queryKey: ['devices'] });
        },
    });

    const deleteMutation = useMutation({
        mutationFn: isAdmin ? deviceService.deleteAdmin : deviceService.delete,
        onSuccess: () => {
            queryClient.invalidateQueries({ queryKey: ['devices'] });
        },
    });

    const handleDelete = (e: React.MouseEvent, id: string, name: string) => {
        e.stopPropagation(); // Prevent row click
        if (window.confirm(`Are you sure you want to delete device "${name}"? This action cannot be undone.`)) {
            deleteMutation.mutate(id);
        }
    };

    const handleEdit = (e: React.MouseEvent, id: string) => {
        e.stopPropagation();
        navigate(`/devices/${id}`);
    };

    const handleToggleStatus = (e: React.MouseEvent, id: string, currentAdminStatus: string | undefined) => {
        e.stopPropagation();
        const newStatus = currentAdminStatus === 'active' || !currentAdminStatus ? 'suspended' : 'active';
        updateStatusMutation.mutate({ id, status: newStatus });
    };

    const handleSort = (column: string) => {
        if (sortColumn === column) {
            setSortDirection(sortDirection === 'asc' ? 'desc' : 'asc');
        } else {
            setSortColumn(column);
            setSortDirection('asc');
        }
    };

    const sortedDevices = devices ? [...devices].sort((a: any, b: any) => {
        let aVal = a[sortColumn];
        let bVal = b[sortColumn];

        // Handle string comparison
        if (typeof aVal === 'string') {
            aVal = aVal.toLowerCase();
            bVal = bVal.toLowerCase();
        }

        // Handle dates
        if (['last_seen', 'created_at'].includes(sortColumn)) {
            aVal = aVal ? new Date(aVal).getTime() : 0;
            bVal = bVal ? new Date(bVal).getTime() : 0;
        }

        if (aVal < bVal) return sortDirection === 'asc' ? -1 : 1;
        if (aVal > bVal) return sortDirection === 'asc' ? 1 : -1;
        return 0;
    }) : [];

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
                    <Button size="sm" onClick={() => setIsAddOpen(true)}>
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
                                    <TableHead>
                                        <SortableHeader column="status" label="Status" currentSort={sortColumn} sortDirection={sortDirection} onSort={handleSort} />
                                    </TableHead>
                                    <TableHead>
                                        <SortableHeader column="name" label="Name" currentSort={sortColumn} sortDirection={sortDirection} onSort={handleSort} />
                                    </TableHead>
                                    <TableHead>
                                        <SortableHeader column="type" label="Type" currentSort={sortColumn} sortDirection={sortDirection} onSort={handleSort} />
                                    </TableHead>
                                    <TableHead>
                                        <SortableHeader column="public_ip" label="Public IP" currentSort={sortColumn} sortDirection={sortDirection} onSort={handleSort} />
                                    </TableHead>
                                    <TableHead>
                                        <SortableHeader column="last_seen" label="Last Seen" currentSort={sortColumn} sortDirection={sortDirection} onSort={handleSort} />
                                    </TableHead>
                                    <TableHead className="text-right">Actions</TableHead>
                                </TableRow>
                            </TableHeader>
                            <TableBody>
                                {sortedDevices.map((device) => (
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
                                            <div className="flex justify-end gap-2">
                                                {isAdmin && (
                                                    <Button
                                                        variant="ghost"
                                                        size="icon"
                                                        className="h-8 w-8"
                                                        onClick={(e) => handleToggleStatus(e, device.id, device.admin_status)}
                                                        title={(device.admin_status === 'active' || !device.admin_status) ? 'Suspend Device' : 'Resume Device'}
                                                    >
                                                        {(device.admin_status === 'active' || !device.admin_status) ? (
                                                            <Pause className="h-4 w-4 text-amber-500" />
                                                        ) : (
                                                            <Play className="h-4 w-4 text-emerald-500" />
                                                        )}
                                                    </Button>
                                                )}
                                                <Button
                                                    variant="ghost"
                                                    size="icon"
                                                    className="h-8 w-8 text-muted-foreground hover:text-primary"
                                                    onClick={(e) => handleEdit(e, device.id)}
                                                    title="Manage Device"
                                                >
                                                    <Pencil className="h-4 w-4" />
                                                </Button>
                                                <Button
                                                    variant="ghost"
                                                    size="icon"
                                                    className="h-8 w-8 hover:bg-destructive/10"
                                                    onClick={(e) => handleDelete(e, device.id, device.name)}
                                                    title="Delete Device"
                                                >
                                                    <Trash2 className="h-4 w-4 text-destructive" />
                                                </Button>
                                            </div>
                                        </TableCell>
                                    </TableRow>
                                ))}
                            </TableBody>
                        </Table>
                    )}
                </CardContent>
            </Card>
            <AddDeviceModal open={isAddOpen} onOpenChange={setIsAddOpen} />
        </div>
    );
}
