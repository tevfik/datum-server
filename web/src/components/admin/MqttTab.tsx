import { useState } from "react";
import { useQuery, useMutation } from "@tanstack/react-query";
import { adminService } from "@/services/adminService";
import { Card, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from "@/components/ui/table";
import { AlertCircle, RefreshCw, Send, Wifi } from "lucide-react";

export function MqttTab() {
    const [topic, setTopic] = useState("");
    const [message, setMessage] = useState("");
    const [retain, setRetain] = useState(false);
    const [publishError, setPublishError] = useState("");

    // Fetch Stats
    const { data: stats } = useQuery({
        queryKey: ['mqtt-stats'],
        queryFn: adminService.getMqttStats,
        refetchInterval: 5000,
    });

    // Fetch Clients
    const { data: clients } = useQuery({
        queryKey: ['mqtt-clients'],
        queryFn: adminService.getMqttClients,
        refetchInterval: 5000,
    });

    // Publish Mutation
    const publishMutation = useMutation({
        mutationFn: adminService.publishMqttMessage,
        onSuccess: () => {
            setTopic("");
            setMessage("");
            setPublishError("");
        },
        onError: (err: any) => {
            setPublishError(err.response?.data?.error || "Failed to publish message");
        }
    });

    const handlePublish = (e: React.FormEvent) => {
        e.preventDefault();
        publishMutation.mutate({ topic, message, retain });
    };

    return (
        <div className="space-y-6">
            {/* Stats Cards */}
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

            <div className="grid gap-4 md:grid-cols-2 lg:grid-cols-7">
                {/* Publish Form */}
                <Card className="col-span-3">
                    <CardHeader>
                        <CardTitle>Publish Message</CardTitle>
                        <CardDescription>Send an MQTT message to a specific topic</CardDescription>
                    </CardHeader>
                    <CardContent>
                        <form onSubmit={handlePublish} className="space-y-4">
                            {publishError && (
                                <div className="flex items-center gap-2 rounded-md bg-destructive/15 p-3 text-sm text-destructive">
                                    <AlertCircle className="h-4 w-4" />
                                    <p>{publishError}</p>
                                </div>
                            )}
                            <div className="space-y-2">
                                <Label htmlFor="topic">Topic</Label>
                                <Input
                                    id="topic"
                                    placeholder="cmd/device_123"
                                    value={topic}
                                    onChange={(e) => setTopic(e.target.value)}
                                    required
                                />
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
                            <Button type="submit" disabled={publishMutation.isPending} className="w-full">
                                {publishMutation.isPending ? <RefreshCw className="mr-2 h-4 w-4 animate-spin" /> : <Send className="mr-2 h-4 w-4" />}
                                Publish
                            </Button>
                        </form>
                    </CardContent>
                </Card>

                {/* Clients List */}
                <Card className="col-span-4 max-h-[500px] flex flex-col">
                    <CardHeader>
                        <CardTitle>Connected Clients</CardTitle>
                        <CardDescription>
                            List of currently connected MQTT clients ({clients?.length || 0})
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
            </div>
        </div>
    );
}
