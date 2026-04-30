import { useState } from 'react';
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { format, formatDistanceToNow } from 'date-fns';
import { Monitor, Smartphone, Globe, Trash2, LogOut, Shield, Bell, BellOff, Loader2, User, Plus } from 'lucide-react';
import { authService, type Session, type PushToken } from '@/features/auth/services/authService';
import { useAuth } from '@/context/AuthContext';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Label } from '@/components/ui/label';
import { Badge } from '@/components/ui/badge';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import {
    Table,
    TableBody,
    TableCell,
    TableHead,
    TableHeader,
    TableRow,
} from '@/components/ui/table';

function uaIcon(ua: string) {
    const lower = ua.toLowerCase();
    if (lower.includes('mobile') || lower.includes('android') || lower.includes('iphone')) {
        return <Smartphone className="h-4 w-4 text-muted-foreground" />;
    }
    if (lower.includes('curl') || lower.includes('python') || lower.includes('go-http')) {
        return <Globe className="h-4 w-4 text-muted-foreground" />;
    }
    return <Monitor className="h-4 w-4 text-muted-foreground" />;
}

function platformIcon(platform: string) {
    if (platform === 'ios' || platform === 'android') return <Smartphone className="h-4 w-4" />;
    return <Bell className="h-4 w-4" />;
}

export function ProfileTab() {
    const queryClient = useQueryClient();
    const { user } = useAuth();
    const [displayName, setDisplayName] = useState('');
    const [nameEditing, setNameEditing] = useState(false);
    const [newPlatform, setNewPlatform] = useState('ntfy');
    const [newToken, setNewToken] = useState('');
    const [showAddToken, setShowAddToken] = useState(false);

    // Profile
    const { data: profile, isLoading: profileLoading } = useQuery({
        queryKey: ['profile'],
        queryFn: authService.getProfile,
        select: (data) => {
            if (!nameEditing && data.display_name) setDisplayName(data.display_name);
            return data;
        },
    });

    // Sessions
    const { data: sessions = [] } = useQuery({
        queryKey: ['sessions'],
        queryFn: authService.getSessions,
        refetchInterval: 30_000,
    });

    // Push tokens
    const { data: pushTokens = [] } = useQuery({
        queryKey: ['push-tokens'],
        queryFn: authService.getPushTokens,
    });

    const updateProfileMutation = useMutation({
        mutationFn: (name: string) => authService.updateProfile(name),
        onSuccess: () => {
            queryClient.invalidateQueries({ queryKey: ['profile'] });
            setNameEditing(false);
        },
    });

    const revokeSessionMutation = useMutation({
        mutationFn: (jti: string) => authService.revokeSession(jti),
        onSuccess: () => queryClient.invalidateQueries({ queryKey: ['sessions'] }),
    });

    const addTokenMutation = useMutation({
        mutationFn: () => authService.registerPushToken(newPlatform, newToken),
        onSuccess: () => {
            queryClient.invalidateQueries({ queryKey: ['push-tokens'] });
            setNewToken('');
            setShowAddToken(false);
        },
    });

    const deleteTokenMutation = useMutation({
        mutationFn: (id: string) => authService.deletePushToken(id),
        onSuccess: () => queryClient.invalidateQueries({ queryKey: ['push-tokens'] }),
    });

    if (profileLoading) {
        return <div className="flex justify-center py-12"><Loader2 className="h-6 w-6 animate-spin text-muted-foreground" /></div>;
    }

    return (
        <div className="space-y-6">
            {/* Profile Info */}
            <Card>
                <CardHeader>
                    <CardTitle className="flex items-center gap-2">
                        <User className="h-5 w-5" /> Profile
                    </CardTitle>
                    <CardDescription>Manage your account details.</CardDescription>
                </CardHeader>
                <CardContent className="space-y-4 max-w-md">
                    <div className="space-y-2">
                        <Label>Email</Label>
                        <p className="text-sm text-muted-foreground font-mono">{profile?.email}</p>
                    </div>
                    <div className="space-y-2">
                        <Label>Role</Label>
                        <div>
                            <Badge variant={user?.role === 'admin' ? 'default' : 'secondary'}>
                                {user?.role}
                            </Badge>
                        </div>
                    </div>
                    <div className="space-y-2">
                        <Label htmlFor="display-name">Display Name</Label>
                        <div className="flex gap-2">
                            <Input
                                id="display-name"
                                value={displayName}
                                onChange={(e) => { setDisplayName(e.target.value); setNameEditing(true); }}
                                placeholder="Your name"
                            />
                            {nameEditing && (
                                <Button
                                    size="sm"
                                    disabled={updateProfileMutation.isPending}
                                    onClick={() => updateProfileMutation.mutate(displayName)}
                                >
                                    {updateProfileMutation.isPending ? <Loader2 className="h-4 w-4 animate-spin" /> : 'Save'}
                                </Button>
                            )}
                        </div>
                    </div>
                    {profile?.ntfy_topic && (
                        <div className="space-y-2">
                            <Label>ntfy Topic</Label>
                            <p className="text-xs font-mono text-muted-foreground break-all">{profile.ntfy_topic}</p>
                            <p className="text-xs text-muted-foreground">Subscribe to this topic in the ntfy app to receive push notifications when the mobile app is closed.</p>
                        </div>
                    )}
                    {profile?.last_login_at && (
                        <p className="text-xs text-muted-foreground">
                            Last login: {formatDistanceToNow(new Date(profile.last_login_at), { addSuffix: true })}
                        </p>
                    )}
                </CardContent>
            </Card>

            {/* Active Sessions */}
            <Card>
                <CardHeader>
                    <CardTitle className="flex items-center gap-2">
                        <Shield className="h-5 w-5" /> Active Sessions
                    </CardTitle>
                    <CardDescription>
                        All devices currently logged in to your account. Revoke any session you don't recognise.
                    </CardDescription>
                </CardHeader>
                <CardContent>
                    {sessions.length === 0 ? (
                        <p className="text-sm text-muted-foreground text-center py-4">No active sessions.</p>
                    ) : (
                        <Table>
                            <TableHeader>
                                <TableRow>
                                    <TableHead>Device</TableHead>
                                    <TableHead>IP</TableHead>
                                    <TableHead>Created</TableHead>
                                    <TableHead>Expires</TableHead>
                                    <TableHead className="w-12"></TableHead>
                                </TableRow>
                            </TableHeader>
                            <TableBody>
                                {sessions.map((s: Session) => (
                                    <TableRow key={s.jti}>
                                        <TableCell>
                                            <div className="flex items-center gap-2">
                                                {uaIcon(s.user_agent)}
                                                <span className="text-xs text-muted-foreground max-w-[200px] truncate" title={s.user_agent}>
                                                    {s.user_agent || 'Unknown'}
                                                </span>
                                            </div>
                                        </TableCell>
                                        <TableCell className="text-xs font-mono">{s.ip}</TableCell>
                                        <TableCell className="text-xs">
                                            {formatDistanceToNow(new Date(s.created_at), { addSuffix: true })}
                                        </TableCell>
                                        <TableCell className="text-xs text-muted-foreground">
                                            {format(new Date(s.expires_at), 'MMM d, yyyy')}
                                        </TableCell>
                                        <TableCell>
                                            <Button
                                                size="icon"
                                                variant="ghost"
                                                className="h-7 w-7 text-destructive hover:text-destructive"
                                                disabled={revokeSessionMutation.isPending}
                                                onClick={() => revokeSessionMutation.mutate(s.jti)}
                                                title="Revoke session"
                                            >
                                                <LogOut className="h-4 w-4" />
                                            </Button>
                                        </TableCell>
                                    </TableRow>
                                ))}
                            </TableBody>
                        </Table>
                    )}
                </CardContent>
            </Card>

            {/* Push Tokens */}
            <Card>
                <CardHeader>
                    <div className="flex items-center justify-between">
                        <div>
                            <CardTitle className="flex items-center gap-2">
                                <Bell className="h-5 w-5" /> Push Tokens
                            </CardTitle>
                            <CardDescription>
                                Registered devices for push notifications. Mobile devices with type "mobile" also receive in-app commands.
                            </CardDescription>
                        </div>
                        <Button size="sm" variant="outline" onClick={() => setShowAddToken(!showAddToken)}>
                            <Plus className="h-4 w-4 mr-1" /> Add
                        </Button>
                    </div>
                </CardHeader>
                <CardContent className="space-y-4">
                    {showAddToken && (
                        <div className="flex gap-2 items-end p-3 border rounded-md bg-muted/30">
                            <div className="space-y-1">
                                <Label className="text-xs">Platform</Label>
                                <select
                                    className="h-9 rounded-md border border-input bg-background px-3 text-sm"
                                    value={newPlatform}
                                    onChange={(e) => setNewPlatform(e.target.value)}
                                >
                                    <option value="ntfy">ntfy</option>
                                    <option value="ios">iOS (APNs)</option>
                                    <option value="android">Android (FCM)</option>
                                    <option value="web">Web Push</option>
                                </select>
                            </div>
                            <div className="flex-1 space-y-1">
                                <Label className="text-xs">Token / Topic</Label>
                                <Input
                                    placeholder="Token or ntfy topic name"
                                    value={newToken}
                                    onChange={(e) => setNewToken(e.target.value)}
                                />
                            </div>
                            <Button
                                size="sm"
                                disabled={!newToken || addTokenMutation.isPending}
                                onClick={() => addTokenMutation.mutate()}
                            >
                                {addTokenMutation.isPending ? <Loader2 className="h-4 w-4 animate-spin" /> : 'Register'}
                            </Button>
                        </div>
                    )}

                    {pushTokens.length === 0 && !showAddToken ? (
                        <div className="flex flex-col items-center gap-2 py-6 text-muted-foreground">
                            <BellOff className="h-8 w-8" />
                            <p className="text-sm">No push tokens registered.</p>
                        </div>
                    ) : (
                        <Table>
                            <TableHeader>
                                <TableRow>
                                    <TableHead>Platform</TableHead>
                                    <TableHead>Token</TableHead>
                                    <TableHead>Added</TableHead>
                                    <TableHead className="w-12"></TableHead>
                                </TableRow>
                            </TableHeader>
                            <TableBody>
                                {pushTokens.map((t: PushToken) => (
                                    <TableRow key={t.id}>
                                        <TableCell>
                                            <div className="flex items-center gap-2">
                                                {platformIcon(t.platform)}
                                                <span className="capitalize">{t.platform}</span>
                                            </div>
                                        </TableCell>
                                        <TableCell>
                                            <span className="text-xs font-mono text-muted-foreground max-w-[200px] truncate block" title={t.token}>
                                                {t.token}
                                            </span>
                                        </TableCell>
                                        <TableCell className="text-xs">
                                            {formatDistanceToNow(new Date(t.created_at), { addSuffix: true })}
                                        </TableCell>
                                        <TableCell>
                                            <Button
                                                size="icon"
                                                variant="ghost"
                                                className="h-7 w-7 text-destructive hover:text-destructive"
                                                disabled={deleteTokenMutation.isPending}
                                                onClick={() => deleteTokenMutation.mutate(t.id)}
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
        </div>
    );
}
