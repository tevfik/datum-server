import { useState } from "react";
import { Shield, Plus, Trash2, Key, Users, Globe } from "lucide-react";
import { useSSOClients, useDeleteSSOClient, useCreateSSOClient } from "../hooks/useSSO";

export default function SSOApps() {
    const { data: clients = [], isLoading } = useSSOClients();
    const deleteClient = useDeleteSSOClient();
    const createClient = useCreateSSOClient();

    const [isAddOpen, setIsAddOpen] = useState(false);
    const [newName, setNewName] = useState("");
    const [newRoles, setNewRoles] = useState("admin");
    const [newEmails, setNewEmails] = useState("");
    const [creationResult, setCreationResult] = useState<{client_id: string, client_secret: string} | null>(null);

    const handleCreate = async (e: React.FormEvent) => {
        e.preventDefault();
        
        const roles = newRoles.split(",").map(r => r.trim()).filter(r => r);
        const emails = newEmails.split(",").map(e => e.trim()).filter(e => e);

        try {
            const result = await createClient.mutateAsync({
                name: newName,
                allowed_roles: roles,
                allowed_emails: emails
            });
            setCreationResult({
                client_id: result.client_id,
                client_secret: result.client_secret
            });
            setNewName("");
            setNewRoles("admin");
            setNewEmails("");
            // We don't close the modal immediately so the user can see the secret
        } catch (error) {
            alert("Failed to create client: " + error);
        }
    };

    const closeAddModal = () => {
        setIsAddOpen(false);
        setCreationResult(null);
    };

    const origin = window.location.origin;

    return (
        <div className="space-y-6">
            {/* Header */}
            <div className="flex items-center justify-between">
                <div>
                    <h1 className="text-2xl font-bold flex items-center gap-2">
                        <Shield className="h-6 w-6 text-indigo-500" />
                        SSO Applications
                    </h1>
                    <p className="text-muted-foreground mt-1">
                        Manage Traefik ForwardAuth clients and OIDC/OAuth2 applications.
                    </p>
                </div>
                <button
                    onClick={() => setIsAddOpen(true)}
                    className="flex items-center gap-2 rounded-lg bg-primary px-4 py-2 text-sm font-medium text-primary-foreground hover:bg-primary/90 transition-colors"
                >
                    <Plus className="h-4 w-4" />
                    New App
                </button>
            </div>

            {/* OIDC Config Helper Box */}
            <div className="rounded-xl border bg-muted/20 p-4 space-y-3">
                <div className="font-semibold text-sm flex items-center gap-2 text-primary">
                    <Key className="h-4 w-4 text-indigo-500" />
                    OIDC / OAuth2 Config Endpoints
                </div>
                <p className="text-xs text-muted-foreground">
                    Use these URLs to integrate Datum with standard OAuth2 / OIDC clients (e.g. LiteLLM, Grafana, Portainer):
                </p>
                <div className="grid gap-2 text-xs font-mono bg-card p-3 rounded-lg border">
                    <div className="flex flex-col sm:flex-row sm:justify-between sm:items-center gap-1 py-1 border-b">
                        <span className="text-muted-foreground font-semibold">Authorization Endpoint</span>
                        <span className="select-all text-indigo-400">{origin}/auth/provider/oauth/authorize</span>
                    </div>
                    <div className="flex flex-col sm:flex-row sm:justify-between sm:items-center gap-1 py-1 border-b">
                        <span className="text-muted-foreground font-semibold">Token Endpoint</span>
                        <span className="select-all text-indigo-400">{origin}/auth/provider/oauth/token</span>
                    </div>
                    <div className="flex flex-col sm:flex-row sm:justify-between sm:items-center gap-1 py-1">
                        <span className="text-muted-foreground font-semibold">Userinfo Endpoint</span>
                        <span className="select-all text-indigo-400">{origin}/auth/provider/oauth/userinfo</span>
                    </div>
                </div>
            </div>

            {/* List */}
            {isLoading ? (
                <div className="text-center py-12 text-muted-foreground">Loading applications...</div>
            ) : clients.length === 0 ? (
                <div className="text-center py-12 border rounded-xl bg-card">
                    <Globe className="h-12 w-12 mx-auto text-muted-foreground/30" />
                    <h3 className="mt-4 text-lg font-medium">No SSO apps configured</h3>
                    <p className="text-muted-foreground mt-1">
                        Create an app to restrict access to a specific subdomain
                    </p>
                    <button
                        onClick={() => setIsAddOpen(true)}
                        className="mt-4 inline-flex items-center gap-2 rounded-lg bg-primary px-4 py-2 text-sm font-medium text-primary-foreground hover:bg-primary/90"
                    >
                        <Plus className="h-4 w-4" />
                        Add App
                    </button>
                </div>
            ) : (
                <div className="space-y-3">
                    {clients.map((client) => (
                        <div
                            key={client.client_id}
                            className="flex items-center gap-4 rounded-xl border bg-card p-4 transition-colors"
                        >
                            <div className={`h-3 w-3 rounded-full flex-shrink-0 ${client.status === "active" ? "bg-green-500" : "bg-red-500"}`} />

                            <div className="flex-1 min-w-0">
                                <div className="font-medium text-lg flex items-center gap-2">
                                    <Globe className="h-4 w-4 text-muted-foreground" />
                                    {client.name}
                                </div>
                                <div className="text-sm text-muted-foreground flex flex-wrap items-center gap-x-4 gap-y-1 mt-1">
                                    <span className="flex items-center gap-1">
                                        <Key className="h-3 w-3" /> ID: {client.client_id}
                                    </span>
                                    {client.allowed_roles?.length > 0 && (
                                        <span className="flex items-center gap-1 bg-indigo-500/10 text-indigo-400 px-1.5 py-0.5 rounded">
                                            <Shield className="h-3 w-3" /> Roles: {client.allowed_roles.join(", ")}
                                        </span>
                                    )}
                                    {client.allowed_emails?.length > 0 && (
                                        <span className="flex items-center gap-1 bg-blue-500/10 text-blue-400 px-1.5 py-0.5 rounded">
                                            <Users className="h-3 w-3" /> Emails: {client.allowed_emails.join(", ")}
                                        </span>
                                    )}
                                    {(!client.allowed_roles?.length && !client.allowed_emails?.length) && (
                                        <span className="text-yellow-500">Public (No Restrictions)</span>
                                    )}
                                </div>
                            </div>

                            <div className="flex items-center gap-1 flex-shrink-0">
                                <button
                                    onClick={() => {
                                        if (confirm(`Delete SSO configuration for "${client.name}"?`)) {
                                            deleteClient.mutate(client.name);
                                        }
                                    }}
                                    className="p-2 rounded-md hover:bg-destructive/10 transition-colors text-destructive"
                                    title="Delete"
                                >
                                    <Trash2 className="h-4 w-4" />
                                </button>
                            </div>
                        </div>
                    ))}
                </div>
            )}

            {/* Add Modal */}
            {isAddOpen && (
                <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4 backdrop-blur-sm">
                    <div className="w-full max-w-md rounded-xl border bg-card p-6 shadow-xl">
                        <h2 className="text-xl font-bold mb-4">Add SSO Application</h2>
                        
                        {creationResult ? (
                            <div className="space-y-4">
                                <div className="bg-green-500/10 border border-green-500/20 text-green-400 p-4 rounded-lg">
                                    Client created successfully!
                                </div>
                                <div>
                                    <label className="text-sm text-muted-foreground">Client ID</label>
                                    <div className="bg-muted p-2 rounded text-sm font-mono mt-1 break-all select-all">
                                        {creationResult.client_id}
                                    </div>
                                </div>
                                <div>
                                    <label className="text-sm text-muted-foreground">Client Secret (Save this now!)</label>
                                    <div className="bg-muted p-2 rounded text-sm font-mono mt-1 break-all select-all text-yellow-500">
                                        {creationResult.client_secret}
                                    </div>
                                </div>
                                <button
                                    type="button"
                                    onClick={closeAddModal}
                                    className="w-full rounded-lg bg-primary px-4 py-2 text-sm font-medium text-primary-foreground hover:bg-primary/90 mt-4"
                                >
                                    Close
                                </button>
                            </div>
                        ) : (
                            <form onSubmit={handleCreate} className="space-y-4">
                                <div>
                                    <label className="text-sm font-medium">Domain Name</label>
                                    <input
                                        type="text"
                                        required
                                        placeholder="e.g., app.example.com"
                                        className="mt-1 w-full rounded-md border bg-background px-3 py-2 text-sm focus:border-primary focus:outline-none focus:ring-1 focus:ring-primary"
                                        value={newName}
                                        onChange={e => setNewName(e.target.value)}
                                    />
                                </div>
                                <div>
                                    <label className="text-sm font-medium">Allowed Roles</label>
                                    <input
                                        type="text"
                                        placeholder="admin, user"
                                        className="mt-1 w-full rounded-md border bg-background px-3 py-2 text-sm focus:border-primary focus:outline-none focus:ring-1 focus:ring-primary"
                                        value={newRoles}
                                        onChange={e => setNewRoles(e.target.value)}
                                    />
                                    <p className="text-xs text-muted-foreground mt-1">Comma-separated roles.</p>
                                </div>
                                <div>
                                    <label className="text-sm font-medium">Allowed Emails (Optional)</label>
                                    <input
                                        type="text"
                                        placeholder="user@example.com"
                                        className="mt-1 w-full rounded-md border bg-background px-3 py-2 text-sm focus:border-primary focus:outline-none focus:ring-1 focus:ring-primary"
                                        value={newEmails}
                                        onChange={e => setNewEmails(e.target.value)}
                                    />
                                    <p className="text-xs text-muted-foreground mt-1">Leave empty to only check roles.</p>
                                </div>
                                <div className="flex justify-end gap-2 mt-6">
                                    <button
                                        type="button"
                                        onClick={closeAddModal}
                                        className="rounded-lg px-4 py-2 text-sm font-medium hover:bg-muted"
                                    >
                                        Cancel
                                    </button>
                                    <button
                                        type="submit"
                                        disabled={createClient.isPending}
                                        className="rounded-lg bg-primary px-4 py-2 text-sm font-medium text-primary-foreground hover:bg-primary/90 disabled:opacity-50"
                                    >
                                        {createClient.isPending ? "Creating..." : "Create"}
                                    </button>
                                </div>
                            </form>
                        )}
                    </div>
                </div>
            )}
        </div>
    );
}
