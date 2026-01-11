import { useState } from 'react';
import { useMutation, useQueryClient } from '@tanstack/react-query';
import { deviceService } from '@/services/deviceService';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Label } from '@/components/ui/label';
import {
    Dialog,
    DialogContent,
    DialogDescription,
    DialogFooter,
    DialogHeader,
    DialogTitle,
    DialogTrigger,
} from "@/components/ui/dialog"
import { Plus, Copy, Check } from 'lucide-react';

export interface AddDeviceModalProps {
    open?: boolean;
    onOpenChange?: (open: boolean) => void;
}

export function AddDeviceModal({ open: externalOpen, onOpenChange: externalOnOpenChange }: AddDeviceModalProps = {}) {
    const [internalOpen, setInternalOpen] = useState(false);
    const [name, setName] = useState('');
    const [type, setType] = useState('generic');
    const [uid, setUid] = useState('');
    const [createdDevice, setCreatedDevice] = useState<{ id: string, api_key: string } | null>(null);

    const queryClient = useQueryClient();

    // Controlled vs Uncontrolled Logic
    const isControlled = externalOpen !== undefined;
    const open = isControlled ? externalOpen : internalOpen;
    const setOpen = isControlled ? (externalOnOpenChange || (() => { })) : setInternalOpen;

    const createMutation = useMutation({
        mutationFn: deviceService.create,
        onSuccess: (data) => {
            queryClient.invalidateQueries({ queryKey: ['devices'] });
            // The response might vary based on backend, assuming data has id and api_key
            // If the backend doesn't return api_key in the response body of create (it should), 
            // we might need to adjust. But standard create usually returns the resource.
            // Let's assume data has api_key.
            setCreatedDevice({ id: data.device_id, api_key: data.api_key });
        },
    });

    const handleSubmit = (e: React.FormEvent) => {
        e.preventDefault();
        createMutation.mutate({
            name,
            type,
            device_uid: uid || undefined, // Optional
        });
    };

    const handleClose = () => {
        setOpen(false);
        setCreatedDevice(null);
        setName('');
        setType('generic');
        setUid('');
    };

    const copyToClipboard = () => {
        if (createdDevice?.api_key) {
            navigator.clipboard.writeText(createdDevice.api_key);
        }
    };

    return (
        <Dialog open={open} onOpenChange={setOpen}>
            {!isControlled && (
                <DialogTrigger asChild>
                    <Button>
                        <Plus className="mr-2 h-4 w-4" /> Add Device
                    </Button>
                </DialogTrigger>
            )}
            <DialogContent className="sm:max-w-[425px]">
                <DialogHeader>
                    <DialogTitle>{createdDevice ? 'Device Created' : 'Add New Device'}</DialogTitle>
                    <DialogDescription>
                        {createdDevice
                            ? "Save this API Key securely. It will not be shown again."
                            : "Enter device details to generate a new ID and API Key."}
                    </DialogDescription>
                </DialogHeader>

                {!createdDevice ? (
                    <form onSubmit={handleSubmit} className="grid gap-4 py-4">
                        <div className="grid grid-cols-4 items-center gap-4">
                            <Label htmlFor="name" className="text-right">
                                Name
                            </Label>
                            <Input
                                id="name"
                                value={name}
                                onChange={(e) => setName(e.target.value)}
                                className="col-span-3"
                                placeholder="Living Room Sensor"
                                required
                            />
                        </div>
                        <div className="grid grid-cols-4 items-center gap-4">
                            <Label htmlFor="type" className="text-right">
                                Type
                            </Label>
                            <Input
                                id="type"
                                value={type}
                                onChange={(e) => setType(e.target.value)}
                                className="col-span-3"
                                placeholder="sensor, camera, actutator"
                                required
                            />
                        </div>
                        <div className="grid grid-cols-4 items-center gap-4">
                            <Label htmlFor="uid" className="text-right">
                                UID (Opt)
                            </Label>
                            <Input
                                id="uid"
                                value={uid}
                                onChange={(e) => setUid(e.target.value)}
                                className="col-span-3"
                                placeholder="Hardware MAC / ID"
                            />
                        </div>
                        <DialogFooter>
                            <Button type="submit" disabled={createMutation.isPending}>
                                {createMutation.isPending ? 'Creating...' : 'Create Device'}
                            </Button>
                        </DialogFooter>
                    </form>
                ) : (
                    <div className="space-y-4 py-4">
                        <div className="space-y-2">
                            <Label>API Key</Label>
                            <div className="relative">
                                <code className="relative rounded bg-muted px-[0.3rem] py-[0.2rem] font-mono text-sm font-semibold block w-full pr-10 break-all border">
                                    {createdDevice.api_key}
                                </code>
                                <Button
                                    size="icon"
                                    variant="ghost"
                                    className="absolute top-0 right-0 h-8 w-8 text-muted-foreground hover:text-foreground"
                                    onClick={copyToClipboard}
                                >
                                    <Copy className="h-4 w-4" />
                                </Button>
                            </div>
                        </div>
                        <div className="space-y-2 text-sm text-muted-foreground">
                            <p>Device ID: <span className="font-mono text-foreground">{createdDevice.id}</span></p>
                        </div>
                        <DialogFooter>
                            <Button onClick={handleClose} className="w-full">
                                <Check className="mr-2 h-4 w-4" /> Done
                            </Button>
                        </DialogFooter>
                    </div>
                )}
            </DialogContent>
        </Dialog>
    );
}
