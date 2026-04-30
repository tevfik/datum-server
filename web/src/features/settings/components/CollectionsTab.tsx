import { useState, useEffect } from 'react';
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { Database, Trash2, Eye, RefreshCw, Plus, Edit, X } from 'lucide-react';
import { Button } from '@/components/ui/button';
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
import { Input } from '@/components/ui/input';
import { Label } from '@/components/ui/label';
import { Textarea } from '@/components/ui/textarea';
import { api } from '@/services/api';

interface CollectionInfo {
    user_id: string;
    collection: string;
    doc_count: number;
}

interface Document {
    id: string;
    _owner_id?: string;
    _created_at?: string;
    _updated_at?: string;
    [key: string]: any;
}

// API Service for Collections
const collectionsService = {
    getCollections: async (): Promise<CollectionInfo[]> => {
        const res = await api.get('/admin/database/collections');
        return res.data.collections || [];
    },
    getDocuments: async (userId: string, collection: string): Promise<Document[]> => {
        const res = await api.get(`/admin/database/${userId}/${collection}`);
        return res.data || [];
    },
    createDocument: async (userId: string, collection: string, doc: any): Promise<void> => {
        await api.post(`/admin/database/${userId}/${collection}`, doc);
    },
    updateDocument: async (userId: string, collection: string, docId: string, doc: any): Promise<void> => {
        await api.put(`/admin/database/${userId}/${collection}/${docId}`, doc);
    },
    deleteDocument: async (userId: string, collection: string, docId: string): Promise<void> => {
        await api.delete(`/admin/database/${userId}/${collection}/${docId}`);
    },
};

export function CollectionsTab() {
    const queryClient = useQueryClient();
    const [selectedCollection, setSelectedCollection] = useState<{ userId: string; collection: string } | null>(null);
    const [viewingDoc, setViewingDoc] = useState<Document | null>(null);
    const [isCreateOpen, setIsCreateOpen] = useState(false);
    const [editingDoc, setEditingDoc] = useState<Document | null>(null);

    // Create Modal State
    const [newDocUserId, setNewDocUserId] = useState('');
    const [newDocCollection, setNewDocCollection] = useState('');
    const [newDocJson, setNewDocJson] = useState('{\n  "name": "example"\n}');

    // Edit Modal State
    const [editDocJson, setEditDocJson] = useState('');

    // Pre-fill create modal if a collection is selected
    useEffect(() => {
        if (isCreateOpen && selectedCollection) {
            setNewDocUserId(selectedCollection.userId);
            setNewDocCollection(selectedCollection.collection);
        }
    }, [isCreateOpen, selectedCollection]);

    // Pre-fill edit modal
    useEffect(() => {
        if (editingDoc) {
            setEditDocJson(JSON.stringify(editingDoc, null, 2));
        }
    }, [editingDoc]);


    // Fetch all collections
    const { data: collections = [], isLoading, refetch } = useQuery({
        queryKey: ['admin-collections'],
        queryFn: collectionsService.getCollections,
    });

    // Fetch documents for selected collection
    const { data: documents = [], isLoading: docsLoading } = useQuery({
        queryKey: ['admin-documents', selectedCollection?.userId, selectedCollection?.collection],
        queryFn: () => selectedCollection
            ? collectionsService.getDocuments(selectedCollection.userId, selectedCollection.collection)
            : Promise.resolve([]),
        enabled: !!selectedCollection,
    });

    // Mutations
    const createMutation = useMutation({
        mutationFn: ({ userId, collection, doc }: { userId: string; collection: string; doc: any }) =>
            collectionsService.createDocument(userId, collection, doc),
        onSuccess: () => {
            queryClient.invalidateQueries({ queryKey: ['admin-documents'] });
            queryClient.invalidateQueries({ queryKey: ['admin-collections'] });
            setIsCreateOpen(false);
            setNewDocJson('{\n  "name": "example"\n}');
        },
    });

    const updateMutation = useMutation({
        mutationFn: ({ userId, collection, docId, doc }: { userId: string; collection: string; docId: string; doc: any }) =>
            collectionsService.updateDocument(userId, collection, docId, doc),
        onSuccess: () => {
            queryClient.invalidateQueries({ queryKey: ['admin-documents'] });
            setEditingDoc(null);
        },
    });

    const deleteMutation = useMutation({
        mutationFn: ({ userId, collection, docId }: { userId: string; collection: string; docId: string }) =>
            collectionsService.deleteDocument(userId, collection, docId),
        onSuccess: () => {
            queryClient.invalidateQueries({ queryKey: ['admin-documents'] });
            queryClient.invalidateQueries({ queryKey: ['admin-collections'] });
        },
    });

    // Group collections by user
    const collectionsByUser = collections.reduce((acc, col) => {
        if (!acc[col.user_id]) {
            acc[col.user_id] = [];
        }
        acc[col.user_id].push(col);
        return acc;
    }, {} as Record<string, CollectionInfo[]>);

    // Handlers
    const handleCreate = () => {
        try {
            const doc = JSON.parse(newDocJson);
            createMutation.mutate({
                userId: newDocUserId,
                collection: newDocCollection,
                doc,
            });
        } catch (e) {
            alert("Invalid JSON format");
        }
    };

    const handleUpdate = () => {
        if (!editingDoc || !selectedCollection) return;
        try {
            const doc = JSON.parse(editDocJson);
            // Don't allow changing ID or meta fields easily via this raw edit, 
            // but backend will handle ID immutability mostly.
            // Remove _ fields to be safe or let backend ignore them?
            // Backend usually updates the fields provided.
            updateMutation.mutate({
                userId: selectedCollection.userId,
                collection: selectedCollection.collection,
                docId: editingDoc.id,
                doc,
            });
        } catch (e) {
            alert("Invalid JSON format");
        }
    };

    const handleDeleteDoc = (docId: string) => {
        if (!selectedCollection) return;
        if (window.confirm(`Delete document "${docId}"?`)) {
            deleteMutation.mutate({
                userId: selectedCollection.userId,
                collection: selectedCollection.collection,
                docId,
            });
        }
    };

    return (
        <div className="space-y-6">
            <div className="flex items-center justify-between">
                <div>
                    <h2 className="text-xl font-semibold flex items-center gap-2">
                        <Database className="h-5 w-5" />
                        Document Collections
                    </h2>
                    <p className="text-sm text-muted-foreground">
                        Browse and manage user document collections
                    </p>
                </div>
                <div className="flex gap-2">
                    <Button onClick={() => setIsCreateOpen(true)}>
                        <Plus className="h-4 w-4 mr-2" />
                        New Document
                    </Button>
                    <Button variant="outline" size="icon" onClick={() => refetch()}>
                        <RefreshCw className="h-4 w-4" />
                    </Button>
                </div>
            </div>

            <div className="grid gap-6 lg:grid-cols-3">
                {/* Collections List */}
                <Card className="lg:col-span-1">
                    <CardHeader>
                        <CardTitle className="text-lg">Collections</CardTitle>
                        <CardDescription>
                            {collections.length} collections
                        </CardDescription>
                    </CardHeader>
                    <CardContent className="p-0">
                        {isLoading ? (
                            <div className="text-center py-8 text-muted-foreground">Loading...</div>
                        ) : collections.length === 0 ? (
                            <div className="p-4 text-center text-muted-foreground">
                                No collections found. Create a document to start.
                            </div>
                        ) : (
                            <div className="max-h-[500px] overflow-y-auto">
                                {Object.entries(collectionsByUser).map(([userId, userCollections]) => (
                                    <div key={userId} className="border-b last:border-0 p-2">
                                        <div className="px-2 py-1 text-xs font-bold text-muted-foreground bg-muted/50 rounded flex justify-between items-center group mb-1">
                                            <span className="truncate max-w-[150px]" title={userId}>{userId}</span>
                                            <Button
                                                variant="ghost"
                                                size="icon"
                                                className="h-4 w-4 opacity-0 group-hover:opacity-100"
                                                onClick={(e) => {
                                                    e.stopPropagation();
                                                    setNewDocUserId(userId);
                                                    setNewDocCollection("");
                                                    setIsCreateOpen(true);
                                                }}
                                                title="Add to this user"
                                            >
                                                <Plus className="h-3 w-3" />
                                            </Button>
                                        </div>
                                        <div className="space-y-1">
                                            {userCollections.map((col) => (
                                                <button
                                                    key={`${col.user_id}-${col.collection}`}
                                                    className={`w-full flex items-center justify-between px-3 py-2 rounded-md text-left text-sm transition-colors ${selectedCollection?.userId === col.user_id &&
                                                        selectedCollection?.collection === col.collection
                                                        ? 'bg-primary text-primary-foreground'
                                                        : 'hover:bg-muted'
                                                        }`}
                                                    onClick={() => setSelectedCollection({ userId: col.user_id, collection: col.collection })}
                                                >
                                                    <span className="font-medium truncate">{col.collection}</span>
                                                    <Badge variant={selectedCollection?.userId === col.user_id && selectedCollection?.collection === col.collection ? "secondary" : "outline"} className="text-[10px] px-1 h-5">
                                                        {col.doc_count}
                                                    </Badge>
                                                </button>
                                            ))}
                                        </div>
                                    </div>
                                ))}
                            </div>
                        )}
                    </CardContent>
                </Card>

                {/* Documents List */}
                <Card className="lg:col-span-2">
                    <CardHeader>
                        <div className="flex items-center justify-between">
                            <div>
                                <CardTitle className="text-lg">
                                    {selectedCollection
                                        ? selectedCollection.collection
                                        : 'Select Collection'}
                                </CardTitle>
                                {selectedCollection && (
                                    <CardDescription className="font-mono text-xs mt-1">
                                        User: {selectedCollection.userId}
                                    </CardDescription>
                                )}
                            </div>
                            {selectedCollection && (
                                <Button size="sm" variant="outline" onClick={() => {
                                    setNewDocUserId(selectedCollection.userId);
                                    setNewDocCollection(selectedCollection.collection);
                                    setIsCreateOpen(true);
                                }}>
                                    <Plus className="h-4 w-4 mr-2" /> Add Doc
                                </Button>
                            )}
                        </div>
                    </CardHeader>
                    <CardContent>
                        {!selectedCollection ? (
                            <div className="flex flex-col items-center justify-center py-12 text-muted-foreground border-2 border-dashed rounded-lg">
                                <Database className="h-10 w-10 mb-4 opacity-20" />
                                <p>Select a collection to manage documents</p>
                            </div>
                        ) : docsLoading ? (
                            <div className="text-center py-12 text-muted-foreground">Loading documents...</div>
                        ) : documents.length === 0 ? (
                            <div className="text-center py-12 text-muted-foreground">No documents in this collection</div>
                        ) : (
                            <Table>
                                <TableHeader>
                                    <TableRow>
                                        <TableHead className="w-[150px]">ID</TableHead>
                                        <TableHead>Preview</TableHead>
                                        <TableHead className="w-[120px] text-right">Actions</TableHead>
                                    </TableRow>
                                </TableHeader>
                                <TableBody>
                                    {documents.map((doc) => (
                                        <TableRow key={doc.id}>
                                            <TableCell className="font-mono text-xs font-medium">
                                                {doc.id}
                                            </TableCell>
                                            <TableCell className="text-xs text-muted-foreground max-w-[300px] truncate">
                                                {JSON.stringify(doc).substring(0, 100)}...
                                            </TableCell>
                                            <TableCell className="text-right">
                                                <div className="flex justify-end gap-1">
                                                    <Button
                                                        variant="ghost"
                                                        size="icon"
                                                        className="h-8 w-8"
                                                        onClick={() => setViewingDoc(doc)}
                                                        title="View"
                                                    >
                                                        <Eye className="h-4 w-4" />
                                                    </Button>
                                                    <Button
                                                        variant="ghost"
                                                        size="icon"
                                                        className="h-8 w-8"
                                                        onClick={() => setEditingDoc(doc)}
                                                        title="Edit"
                                                    >
                                                        <Edit className="h-4 w-4" />
                                                    </Button>
                                                    <Button
                                                        variant="ghost"
                                                        size="icon"
                                                        className="h-8 w-8 text-destructive hover:text-destructive"
                                                        onClick={() => handleDeleteDoc(doc.id)}
                                                        title="Delete"
                                                    >
                                                        <Trash2 className="h-4 w-4" />
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
            </div>

            {/* Create Document Modal */}
            {isCreateOpen && (
                <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 backdrop-blur-sm p-4">
                    <div className="w-full max-w-lg bg-card rounded-lg shadow-lg border flex flex-col max-h-[90vh]">
                        <div className="p-6 border-b flex items-center justify-between bg-muted/40">
                            <h3 className="text-lg font-semibold">Create New Document</h3>
                            <Button variant="ghost" size="icon" onClick={() => setIsCreateOpen(false)} className="h-8 w-8">
                                <X className="h-4 w-4" />
                            </Button>
                        </div>
                        <div className="p-6 space-y-4 overflow-y-auto flex-1">
                            <div className="space-y-2">
                                <Label htmlFor="userId">User ID</Label>
                                <Input
                                    id="userId"
                                    value={newDocUserId}
                                    onChange={(e) => setNewDocUserId(e.target.value)}
                                    placeholder="Target User ID"
                                />
                                <p className="text-xs text-muted-foreground">The document will belong to this user.</p>
                            </div>
                            <div className="space-y-2">
                                <Label htmlFor="collection">Collection Name</Label>
                                <Input
                                    id="collection"
                                    value={newDocCollection}
                                    onChange={(e) => setNewDocCollection(e.target.value)}
                                    placeholder="e.g., todos, settings"
                                />
                                <p className="text-xs text-muted-foreground">New collection will be created if it doesn't exist.</p>
                            </div>
                            <div className="space-y-2 flex-1 flex flex-col">
                                <Label htmlFor="json">Document Content (JSON)</Label>
                                <Textarea
                                    id="json"
                                    value={newDocJson}
                                    onChange={(e) => setNewDocJson(e.target.value)}
                                    className="font-mono text-xs min-h-[200px] flex-1"
                                    placeholder="{}"
                                />
                            </div>
                        </div>
                        <div className="p-4 border-t bg-muted/40 flex justify-end gap-2">
                            <Button variant="outline" onClick={() => setIsCreateOpen(false)}>Cancel</Button>
                            <Button onClick={handleCreate} disabled={createMutation.isPending || !newDocUserId || !newDocCollection}>
                                {createMutation.isPending ? 'Creating...' : 'Create Document'}
                            </Button>
                        </div>
                    </div>
                </div>
            )}

            {/* Edit Document Modal */}
            {editingDoc && (
                <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 backdrop-blur-sm p-4">
                    <div className="w-full max-w-lg bg-card rounded-lg shadow-lg border flex flex-col max-h-[90vh]">
                        <div className="p-6 border-b flex items-center justify-between bg-muted/40">
                            <h3 className="text-lg font-semibold">Edit Document</h3>
                            <Button variant="ghost" size="icon" onClick={() => setEditingDoc(null)} className="h-8 w-8">
                                <X className="h-4 w-4" />
                            </Button>
                        </div>
                        <div className="p-6 space-y-4 overflow-y-auto flex-1">
                            <div className="flex gap-4 text-sm">
                                <div>
                                    <span className="font-semibold text-muted-foreground">ID: </span>
                                    <span className="font-mono">{editingDoc.id}</span>
                                </div>
                                {selectedCollection && (
                                    <div>
                                        <span className="font-semibold text-muted-foreground">Collection: </span>
                                        <span>{selectedCollection.collection}</span>
                                    </div>
                                )}
                            </div>

                            <div className="space-y-2 flex-1 flex flex-col">
                                <Label htmlFor="editJson">Document Content (JSON)</Label>
                                <Textarea
                                    id="editJson"
                                    value={editDocJson}
                                    onChange={(e) => setEditDocJson(e.target.value)}
                                    className="font-mono text-xs min-h-[300px] flex-1"
                                />
                            </div>
                        </div>
                        <div className="p-4 border-t bg-muted/40 flex justify-end gap-2">
                            <Button variant="outline" onClick={() => setEditingDoc(null)}>Cancel</Button>
                            <Button onClick={handleUpdate} disabled={updateMutation.isPending}>
                                {updateMutation.isPending ? 'Saving...' : 'Save Changes'}
                            </Button>
                        </div>
                    </div>
                </div>
            )}

            {/* View Document Modal (Read Only) */}
            {viewingDoc && (
                <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 backdrop-blur-sm p-4">
                    <div className="w-full max-w-2xl bg-card rounded-lg shadow-lg border p-6 space-y-4 max-h-[80vh] overflow-hidden flex flex-col">
                        <div className="flex items-center justify-between">
                            <h3 className="font-semibold">Document: {viewingDoc.id}</h3>
                            <Button variant="ghost" size="sm" onClick={() => setViewingDoc(null)}>
                                Close
                            </Button>
                        </div>
                        <pre className="flex-1 overflow-auto bg-muted p-4 rounded-md text-sm font-mono">
                            {JSON.stringify(viewingDoc, null, 2)}
                        </pre>
                    </div>
                </div>
            )}
        </div>
    );
}
