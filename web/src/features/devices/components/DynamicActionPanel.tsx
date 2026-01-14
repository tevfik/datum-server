import { useState } from "react";
import { type Device } from "@/shared/types/device";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Dialog, DialogContent, DialogDescription, DialogFooter, DialogHeader, DialogTitle, DialogTrigger } from "@/components/ui/dialog";
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select";
import { Terminal, Play, Loader2 } from "lucide-react";
import { deviceService } from "@/features/devices/services/deviceService";

interface DynamicActionPanelProps {
    device: Device;
}

export function DynamicActionPanel({ device }: DynamicActionPanelProps) {
    const td = device.thing_description;

    if (!td || !td.actions) {
        return null;
    }

    const actions = td.actions as Record<string, any>;

    return (
        <Card className="col-span-2">
            <CardHeader>
                <CardTitle className="flex items-center gap-2">
                    <Terminal className="h-5 w-5" />
                    Actions
                </CardTitle>
                <CardDescription>
                    Execute device commands
                </CardDescription>
            </CardHeader>
            <CardContent>
                <div className="flex flex-wrap gap-4">
                    {Object.entries(actions).map(([key, actionDef]) => (
                        <ActionItem
                            key={key}
                            deviceId={device.id}
                            actionKey={key}
                            actionDef={actionDef}
                        />
                    ))}
                </div>
            </CardContent>
        </Card>
    );
}

function ActionItem({ deviceId, actionKey, actionDef }: { deviceId: string, actionKey: string, actionDef: any }) {
    const [open, setOpen] = useState(false);
    const [isLoading, setIsLoading] = useState(false);
    const [params, setParams] = useState<Record<string, any>>({});

    const title = actionDef.title || actionKey;
    const description = actionDef.description || "";
    const hasInput = actionDef.input && actionDef.input.properties;

    const handleExecute = async () => {
        setIsLoading(true);
        try {
            await deviceService.sendCommand(deviceId, {
                action: actionKey,
                params: params
            });
            setOpen(false); // Close dialog
            // Ideally show toast here
            alert("Command Sent: " + title);
        } catch (e) {
            console.error(e);
            alert("Failed to send command");
        } finally {
            setIsLoading(false);
            setParams({}); // Reset params
        }
    };

    // Simple Action (No Input) -> Direct Button
    if (!hasInput) {
        return (
            <Button
                variant="outline"
                onClick={handleExecute}
                disabled={isLoading}
                className="gap-2"
            >
                {isLoading ? <Loader2 className="h-4 w-4 animate-spin" /> : <Play className="h-4 w-4" />}
                {title}
            </Button>
        );
    }

    // Complex Action -> Dialog
    const inputProps = actionDef.input.properties || {};

    return (
        <Dialog open={open} onOpenChange={setOpen}>
            <DialogTrigger asChild>
                <Button variant="outline" className="gap-2">
                    <Play className="h-4 w-4" />
                    {title} ...
                </Button>
            </DialogTrigger>
            <DialogContent className="sm:max-w-[425px]">
                <DialogHeader>
                    <DialogTitle>{title}</DialogTitle>
                    <DialogDescription>
                        {description || "Enter parameters for this action."}
                    </DialogDescription>
                </DialogHeader>
                <div className="grid gap-4 py-4">
                    {Object.entries(inputProps).map(([pKey, pDef]: [string, any]) => (
                        <div key={pKey} className="grid grid-cols-4 items-center gap-4">
                            <Label htmlFor={pKey} className="text-right">
                                {pDef.title || pKey}
                            </Label>
                            {pDef.enum ? (
                                <Select
                                    onValueChange={(val) => {
                                        setParams(prev => ({
                                            ...prev,
                                            [pKey]: val
                                        }));
                                    }}
                                    value={params[pKey] ? String(params[pKey]) : ""}
                                >
                                    <SelectTrigger className="col-span-3">
                                        <SelectValue placeholder={`Select ${pDef.title || pKey}`} />
                                    </SelectTrigger>
                                    <SelectContent>
                                        {pDef.enum.map((option: any) => (
                                            <SelectItem key={String(option)} value={String(option)}>
                                                {String(option)}
                                            </SelectItem>
                                        ))}
                                    </SelectContent>
                                </Select>
                            ) : (
                                <Input
                                    id={pKey}
                                    type={pDef.type === "integer" || pDef.type === "number" ? "number" : "text"}
                                    className="col-span-3"
                                    placeholder={pDef.unit ? `(${pDef.unit})` : ""}
                                    value={params[pKey] || ""}
                                    onChange={(e) => {
                                        const val = e.target.value;
                                        setParams(prev => ({
                                            ...prev,
                                            [pKey]: pDef.type === "integer" ? parseInt(val) : (pDef.type === "number" ? parseFloat(val) : val)
                                        }));
                                    }}
                                />
                            )}
                        </div>
                    ))}
                </div>
                <DialogFooter>
                    <Button variant="outline" onClick={() => setOpen(false)}>Cancel</Button>
                    <Button onClick={handleExecute} disabled={isLoading}>
                        {isLoading && <Loader2 className="mr-2 h-4 w-4 animate-spin" />}
                        Execute
                    </Button>
                </DialogFooter>
            </DialogContent>
        </Dialog>
    );
}
