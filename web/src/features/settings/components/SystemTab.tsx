import { useState, useEffect } from "react";
import { useQuery, useMutation, useQueryClient } from "@tanstack/react-query";
import { adminService } from "@/features/settings/services/adminService";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { AlertCircle, RefreshCw, Trash2, Power, Save } from "lucide-react";

export function SystemTab() {
    const queryClient = useQueryClient();

    // Config State
    const [retentionDays, setRetentionDays] = useState(7);
    const [allowRegister, setAllowRegister] = useState(false);
    const [configError, setConfigError] = useState("");

    // Fetch Config
    const { data: config } = useQuery({
        queryKey: ['system-config'],
        queryFn: adminService.getSystemConfig,
    });

    // Update local state when config fetches
    useEffect(() => {
        if (config) {
            setRetentionDays(config.retention.days);
            // allow_register is not in SystemConfig, handled via systemStats below
        }
    }, [config]);

    const { data: systemStats } = useQuery({
        queryKey: ['admin-stats'], // Reusing key from AdminSettings
        queryFn: adminService.getSystemStats,
    });

    useEffect(() => {
        if (systemStats) {
            setAllowRegister(!!systemStats.allow_register);
        }
    }, [systemStats]);

    // Fetch Logs
    const { data: logsData, refetch: refetchLogs, isFetching: logsFetching } = useQuery({
        queryKey: ['system-logs'],
        queryFn: () => adminService.getLogs(500),
    });

    // Mutation: Update Retention
    const retentionMutation = useMutation({
        mutationFn: adminService.updateRetention,
        onSuccess: () => {
            alert("Retention policy updated.");
            queryClient.invalidateQueries({ queryKey: ['system-config'] });
        },
        onError: (err: any) => setConfigError(err.response?.data?.error || "Failed to update retention")
    });

    // Mutation: Toggle Registration
    const registrationMutation = useMutation({
        mutationFn: adminService.updateRegistration,
        onSuccess: () => {
            queryClient.invalidateQueries({ queryKey: ['admin-stats'] });
        }
    });

    // Mutation: Clear Logs
    const clearLogsMutation = useMutation({
        mutationFn: adminService.clearLogs,
        onSuccess: () => refetchLogs()
    });

    // Mutation: Reset System
    const resetMutation = useMutation({
        mutationFn: adminService.resetSystem,
        onSuccess: () => {
            alert("System has been reset. You will be logged out.");
            window.location.reload();
        }
    });

    const handleSaveRetention = () => {
        retentionMutation.mutate({ days: retentionDays, check_interval_hours: 6 });
    };

    const handleToggleRegister = () => {
        registrationMutation.mutate({ allow_register: !allowRegister });
    };

    const handleReset = () => {
        const confirmStr = prompt("DANGER: This will delete ALL data. Type 'RESET' to confirm:");
        if (confirmStr === 'RESET') {
            resetMutation.mutate();
        }
    };

    return (
        <div className="space-y-6">
            <div className="grid gap-4 md:grid-cols-2">
                {/* Configuration Card */}
                <Card>
                    <CardHeader>
                        <CardTitle>System Configuration</CardTitle>
                        <CardDescription>Manage core system settings.</CardDescription>
                    </CardHeader>
                    <CardContent className="space-y-4">
                        {configError && (
                            <div className="flex items-center gap-2 rounded-md bg-destructive/15 p-3 text-sm text-destructive">
                                <AlertCircle className="h-4 w-4" />
                                <p>{configError}</p>
                            </div>
                        )}

                        <div className="space-y-2">
                            <Label>Data Retention (Days)</Label>
                            <div className="flex gap-2">
                                <Input
                                    type="number"
                                    value={retentionDays}
                                    onChange={(e) => setRetentionDays(parseInt(e.target.value) || 0)}
                                    min={1}
                                    max={365}
                                />
                                <Button onClick={handleSaveRetention} disabled={retentionMutation.isPending}>
                                    <Save className="h-4 w-4 mr-2" /> Save
                                </Button>
                            </div>
                            <p className="text-xs text-muted-foreground">
                                Data older than this will be automatically deleted.
                            </p>
                        </div>

                        <div className="flex items-center justify-between border-t pt-4">
                            <div className="space-y-0.5">
                                <Label>Public Registration</Label>
                                <p className="text-xs text-muted-foreground">Allow new users to sign up.</p>
                            </div>
                            <Button
                                variant={allowRegister ? "default" : "outline"}
                                onClick={handleToggleRegister}
                                disabled={registrationMutation.isPending}
                            >
                                {allowRegister ? "Enabled" : "Disabled"}
                            </Button>
                        </div>

                        <div className="flex items-center justify-between border-t pt-4">
                            <div className="space-y-0.5">
                                <Label className="text-destructive">Factory Reset</Label>
                                <p className="text-xs text-muted-foreground">Wipe all data and reset to fresh state.</p>
                            </div>
                            <Button
                                variant="destructive"
                                onClick={handleReset}
                                disabled={resetMutation.isPending}
                            >
                                <Power className="h-4 w-4 mr-2" /> Reset System
                            </Button>
                        </div>
                    </CardContent>
                </Card>

                {/* Logs Card */}
                <Card className="flex flex-col h-[500px]">
                    <CardHeader className="flex flex-row items-center justify-between">
                        <div>
                            <CardTitle>System Logs</CardTitle>
                            <CardDescription>View recent server activity.</CardDescription>
                        </div>
                        <div className="flex gap-2">
                            <Button variant="outline" size="icon" onClick={() => refetchLogs()} disabled={logsFetching}>
                                <RefreshCw className={`h-4 w-4 ${logsFetching ? 'animate-spin' : ''}`} />
                            </Button>
                            <Button variant="outline" size="icon" onClick={() => clearLogsMutation.mutate()} disabled={clearLogsMutation.isPending}>
                                <Trash2 className="h-4 w-4" />
                            </Button>
                        </div>
                    </CardHeader>
                    <CardContent className="flex-1 overflow-hidden p-0">
                        <div className="h-full overflow-auto bg-muted/50 p-4 font-mono text-xs">
                            {logsData?.logs && logsData.logs.length > 0 ? (
                                logsData.logs.map((log, i) => (
                                    <div key={i} className="whitespace-pre-wrap border-b border-border/50 py-1 last:border-0 hover:bg-muted/80">
                                        {log.raw}
                                    </div>
                                ))
                            ) : (
                                <div className="text-muted-foreground italic p-4">No logs available or logging disabled.</div>
                            )}
                        </div>
                    </CardContent>
                </Card>
            </div>
        </div>
    );
}
