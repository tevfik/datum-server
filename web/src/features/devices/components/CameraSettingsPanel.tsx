import { type Device } from "@/shared/types/device";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select";
import { Switch } from "@/components/ui/switch";
import { Label } from "@/components/ui/label";
import { Monitor, FlipVertical, FlipHorizontal, Video } from "lucide-react";
import { deviceService } from "@/features/devices/services/deviceService";
import { useState } from "react";

interface CameraSettingsPanelProps {
    device: Device;
    shadowState?: Record<string, any>;
}

export function CameraSettingsPanel({ device, shadowState }: CameraSettingsPanelProps) {
    const [isLoading, setIsLoading] = useState<string | null>(null);

    const td = device.thing_description;
    if (!td || !td.properties) return null;

    const getValue = (key: string) => shadowState ? shadowState[key] : undefined;

    const handleUpdate = async (key: string, value: any) => {
        setIsLoading(key);
        try {
            await deviceService.sendCommand(device.id, {
                action: "update_settings",
                params: { [key]: value }
            });
        } catch (e) {
            console.error(`Failed to update ${key}`, e);
        } finally {
            setIsLoading(null);
        }
    };

    // Helper to check if property exists in TD
    const hasProp = (key: string) => !!(td.properties as any)[key];

    if (!hasProp('stream_resolution') && !hasProp('snapshot_resolution') && !hasProp('stream_enabled')) {
        return null;
    }

    return (
        <Card className="col-span-2 md:col-span-2 lg:col-span-1">
            <CardHeader className="pb-3">
                <CardTitle className="flex items-center gap-2">
                    <Monitor className="h-5 w-5" />
                    Camera Settings
                </CardTitle>
                <CardDescription>Display & Stream Configuration</CardDescription>
            </CardHeader>
            <CardContent className="space-y-4">
                {/* Stream Toggle */}
                {hasProp('stream_enabled') && (
                    <div className="flex items-center justify-between border-b pb-3">
                        <div className="space-y-0.5">
                            <Label className="text-base flex items-center gap-2">
                                <Video className="h-4 w-4 text-muted-foreground" />
                                Stream Active
                            </Label>
                            <span className="text-xs text-muted-foreground">Enable/Disable MJPEG Stream</span>
                        </div>
                        <Switch
                            checked={!!getValue('stream_enabled')}
                            onCheckedChange={(checked) => handleUpdate('stream_enabled', checked)}
                            disabled={isLoading === 'stream_enabled'}
                        />
                    </div>
                )}

                {/* Resolutions */}
                <div className="grid grid-cols-1 gap-3">
                    {hasProp('stream_resolution') && (
                        <div className="grid gap-1.5">
                            <Label className="text-xs font-medium text-muted-foreground">Stream Resolution</Label>
                            <Select
                                value={getValue('stream_resolution') || ''}
                                onValueChange={(val) => handleUpdate('stream_resolution', val)}
                                disabled={isLoading === 'stream_resolution'}
                            >
                                <SelectTrigger>
                                    <SelectValue placeholder="Select Resolution" />
                                </SelectTrigger>
                                <SelectContent>
                                    {(td.properties as any)['stream_resolution'].enum?.map((opt: string) => (
                                        <SelectItem key={opt} value={opt}>{opt}</SelectItem>
                                    ))}
                                </SelectContent>
                            </Select>
                        </div>
                    )}

                    {hasProp('snapshot_resolution') && (
                        <div className="grid gap-1.5">
                            <Label className="text-xs font-medium text-muted-foreground">Snapshot Resolution</Label>
                            <Select
                                value={getValue('snapshot_resolution') || ''}
                                onValueChange={(val) => handleUpdate('snapshot_resolution', val)}
                                disabled={isLoading === 'snapshot_resolution'}
                            >
                                <SelectTrigger>
                                    <SelectValue placeholder="Select Resolution" />
                                </SelectTrigger>
                                <SelectContent>
                                    {(td.properties as any)['snapshot_resolution'].enum?.map((opt: string) => (
                                        <SelectItem key={opt} value={opt}>{opt}</SelectItem>
                                    ))}
                                </SelectContent>
                            </Select>
                        </div>
                    )}
                </div>

                {/* Flip & Mirror */}
                <div className="flex items-center gap-4 pt-2">
                    {hasProp('vflip') && (
                        <div className="flex items-center gap-2 border rounded-md p-2 flex-1 justify-center cursor-pointer hover:bg-muted/50 transition-colors"
                            onClick={() => handleUpdate('vflip', !getValue('vflip'))}>
                            <FlipVertical className={`h-4 w-4 ${getValue('vflip') ? 'text-primary' : 'text-muted-foreground'}`} />
                            <span className={`text-sm ${getValue('vflip') ? 'font-medium' : 'text-muted-foreground'}`}>V-Flip</span>
                        </div>
                    )}
                    {hasProp('hmirror') && (
                        <div className="flex items-center gap-2 border rounded-md p-2 flex-1 justify-center cursor-pointer hover:bg-muted/50 transition-colors"
                            onClick={() => handleUpdate('hmirror', !getValue('hmirror'))}>
                            <FlipHorizontal className={`h-4 w-4 ${getValue('hmirror') ? 'text-primary' : 'text-muted-foreground'}`} />
                            <span className={`text-sm ${getValue('hmirror') ? 'font-medium' : 'text-muted-foreground'}`}>Mirror</span>
                        </div>
                    )}
                </div>
            </CardContent>
        </Card>
    );
}
