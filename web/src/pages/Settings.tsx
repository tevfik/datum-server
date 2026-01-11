import { useState } from 'react';
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { format } from 'date-fns';
import { Copy, Plus, Trash2, Key, Check } from 'lucide-react';
import { authService } from '@/services/authService';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import {
    Table,
    TableBody,
    TableCell,
    TableHead,
    TableHeader,
    TableRow,
} from '@/components/ui/table';
import { Badge } from '@/components/ui/badge';

export default function Settings() {
    const [activeTab, setActiveTab] = useState('api-keys');
    const [isCreateOpen, setIsCreateOpen] = useState(false);
    const [newKeyName, setNewKeyName] = useState('');
    const [createdKeyData, setCreatedKeyData] = useState<{ key: string, name: string } | null>(null);

    const queryClient = useQueryClient();

    // Fetch Keys
    const { data: keys = [], isLoading } = useQuery({
        queryKey: ['api-keys'],
        queryFn: authService.getKeys,
    });

    // Create Key Mutation
    const createMutation = useMutation({
        mutationFn: authService.createKey,
        onSuccess: (data) => {
            queryClient.invalidateQueries({ queryKey: ['api-keys'] });
            setCreatedKeyData({ key: data.key, name: data.name });
            setNewKeyName(''); // Reset input but keep modal open to show key
            // Don't close modal yet, we need to show the key
        },
    });

    // Delete Key Mutation
    const deleteMutation = useMutation({
        mutationFn: authService.deleteKey,
        onSuccess: () => {
            queryClient.invalidateQueries({ queryKey: ['api-keys'] });
        },
    });

    const handleCreate = (e: React.FormEvent) => {
        e.preventDefault();
        if (!newKeyName.trim()) return;
        createMutation.mutate(newKeyName);
    };

    const handleCloseModal = () => {
        setIsCreateOpen(false);
        setCreatedKeyData(null);
        setNewKeyName('');
    };

    const copyToClipboard = (text: string) => {
        navigator.clipboard.writeText(text);
        // Could add a toast here
    };

    return (
        <div className="space-y-6">
            <h1 className="text-3xl font-bold tracking-tight">Settings</h1>

            {/* Simple Tabs */}
            <div className="flex space-x-2 border-b">
                <button
                    className={`px-4 py-2 text-sm font-medium border-b-2 transition-colors ${activeTab === 'general' ? 'border-primary text-primary' : 'border-transparent text-muted-foreground hover:text-foreground'}`}
                    onClick={() => setActiveTab('general')}
                >
                    General
                </button>
                <button
                    className={`px-4 py-2 text-sm font-medium border-b-2 transition-colors ${activeTab === 'api-keys' ? 'border-primary text-primary' : 'border-transparent text-muted-foreground hover:text-foreground'}`}
                    onClick={() => setActiveTab('api-keys')}
                >
                    API Keys
                </button>
            </div>

            {/* API Keys Content */}
            {activeTab === 'api-keys' && (
                <Card>
                    <CardHeader>
                        <div className="flex items-center justify-between">
                            <div>
                                <CardTitle>API Keys</CardTitle>
                                <CardDescription>Manage persistent access keys for external scripts and devices.</CardDescription>
                            </div>
                            <Button onClick={() => setIsCreateOpen(true)}>
                                <Plus className="mr-2 h-4 w-4" /> Create New Key
                            </Button>
                        </div>
                    </CardHeader>
                    <CardContent>
                        {isLoading ? (
                            <div>Loading keys...</div>
                        ) : keys.length === 0 ? (
                            <div className="text-center py-8 text-muted-foreground">No API keys found. Create one to get started.</div>
                        ) : (
                            <Table>
                                <TableHeader>
                                    <TableRow>
                                        <TableHead>Name</TableHead>
                                        <TableHead>Key Prefix</TableHead>
                                        <TableHead>Created</TableHead>
                                        <TableHead className="text-right">Actions</TableHead>
                                    </TableRow>
                                </TableHeader>
                                <TableBody>
                                    {keys.map((key) => (
                                        <TableRow key={key.id}>
                                            <TableCell className="font-medium">
                                                <div className="flex items-center">
                                                    <Key className="mr-2 h-4 w-4 text-muted-foreground" />
                                                    {key.name}
                                                </div>
                                            </TableCell>
                                            <TableCell>
                                                <Badge variant="outline" className="font-mono">
                                                    {key.key}
                                                </Badge>
                                            </TableCell>
                                            <TableCell>{format(new Date(key.created_at), 'MMM d, yyyy HH:mm')}</TableCell>
                                            <TableCell className="text-right">
                                                <Button
                                                    variant="ghost"
                                                    size="icon"
                                                    className="text-destructive hover:text-destructive hover:bg-destructive/10"
                                                    onClick={() => {
                                                        if (window.confirm(`Delete key "${key.name}"? This cannot be undone.`)) {
                                                            deleteMutation.mutate(key.id);
                                                        }
                                                    }}
                                                >
                                                    <Trash2 className="h-4 w-4" />
                                                </Button>
                                            </TableCell>
                                        </TableRow>
                                    ))}
                                </TableBody>
                            </Table>
                        )}
                    </CardContent>
                </Card>
            )}

            {activeTab === 'general' && (
                <Card>
                    <CardHeader>
                        <CardTitle>General Settings</CardTitle>
                        <CardDescription>Application preferences and configuration.</CardDescription>
                    </CardHeader>
                    <CardContent>
                        <p className="text-muted-foreground">No general settings available yet.</p>
                    </CardContent>
                </Card>
            )}

            {/* Create Key Modal (Custom) */}
            {isCreateOpen && (
                <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 backdrop-blur-sm p-4">
                    <div className="w-full max-w-md bg-card rounded-lg shadow-lg border p-6 space-y-4">
                        <div className="space-y-2">
                            <h2 className="text-lg font-semibold">Create API Key</h2>
                            <p className="text-sm text-muted-foreground">
                                {createdKeyData
                                    ? "Copy your new API key now. You won't be able to see it again!"
                                    : "Enter a name for the new API key."}
                            </p>
                        </div>

                        {!createdKeyData ? (
                            <form onSubmit={handleCreate} className="space-y-4">
                                <div className="space-y-2">
                                    <Input
                                        placeholder="Key Name (e.g. Home Assistant, Deployment Script)"
                                        value={newKeyName}
                                        onChange={(e) => setNewKeyName(e.target.value)}
                                        autoFocus
                                    />
                                </div>
                                <div className="flex justify-end gap-2">
                                    <Button type="button" variant="ghost" onClick={handleCloseModal}>
                                        Cancel
                                    </Button>
                                    <Button type="submit" disabled={!newKeyName.trim() || createMutation.isPending}>
                                        {createMutation.isPending ? 'Creating...' : 'Create Key'}
                                    </Button>
                                </div>
                            </form>
                        ) : (
                            <div className="space-y-4">
                                <div className="relative">
                                    <div className="bg-muted p-3 rounded-md font-mono text-sm break-all pr-10 border border-primary/20 bg-primary/5">
                                        {createdKeyData.key}
                                    </div>
                                    <Button
                                        size="icon"
                                        variant="ghost"
                                        className="absolute top-1 right-1 h-8 w-8 text-primary"
                                        onClick={() => copyToClipboard(createdKeyData.key)}
                                    >
                                        <Copy className="h-4 w-4" />
                                    </Button>
                                </div>
                                <div className="flex justify-end">
                                    <Button onClick={handleCloseModal} className="w-full">
                                        <Check className="mr-2 h-4 w-4" /> Done
                                    </Button>
                                </div>
                            </div>
                        )}
                    </div>
                </div>
            )}
        </div>
    );
}
