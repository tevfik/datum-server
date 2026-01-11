import { useState } from 'react';
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { Database, Trash2, Eye, ChevronRight, RefreshCw } from 'lucide-react';
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
        const res = await api.get('/sys/db/collections');
        return res.data.collections || [];
    },
    getDocuments: async (userId: string, collection: string): Promise<Document[]> => {
        const res = await api.get(`/sys/db/${userId}/${collection}`);
        return res.data || [];
    },
    deleteDocument: async (userId: string, collection: string, docId: string): Promise<void> => {
        await api.delete(`/sys/db/${userId}/${collection}/${docId}`);
    },
};

export function CollectionsTab() {
    const queryClient = useQueryClient();
    const [selectedCollection, setSelectedCollection] = useState<{ userId: string; collection: string } | null>(null);
    const [viewingDoc, setViewingDoc] = useState<Document | null>(null);

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

    // Delete document mutation
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

    const handleViewDoc = (doc: Document) => {
        setViewingDoc(doc);
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
                <Button variant="outline" size="sm" onClick={() => refetch()}>
                    <RefreshCw className="h-4 w-4 mr-2" />
                    Refresh
                </Button>
            </div>

            <div className="grid gap-6 lg:grid-cols-2">
                {/* Collections List */}
                <Card>
                    <CardHeader>
                        <CardTitle className="text-lg">Collections</CardTitle>
                        <CardDescription>
                            {collections.length} collections from {Object.keys(collectionsByUser).length} users
                        </CardDescription>
                    </CardHeader>
                    <CardContent>
                        {isLoading ? (
                            <div className="text-center py-8 text-muted-foreground">Loading collections...</div>
                        ) : collections.length === 0 ? (
                            <div className="text-center py-8 text-muted-foreground">No collections found</div>
                        ) : (
                            <div className="space-y-4 max-h-96 overflow-y-auto">
                                {Object.entries(collectionsByUser).map(([userId, userCollections]) => (
                                    <div key={userId} className="space-y-1">
                                        <div className="text-xs font-medium text-muted-foreground uppercase tracking-wide">
                                            User: {userId.substring(0, 8)}...
                                        </div>
                                        {userCollections.map((col) => (
                                            <button
                                                key={`${col.user_id}-${col.collection}`}
                                                className={`w-full flex items-center justify-between p-2 rounded-md text-left transition-colors ${selectedCollection?.userId === col.user_id &&
                                                    selectedCollection?.collection === col.collection
                                                    ? 'bg-primary/10 border border-primary/20'
                                                    : 'hover:bg-muted'
                                                    }`}
                                                onClick={() => setSelectedCollection({ userId: col.user_id, collection: col.collection })}
                                            >
                                                <span className="font-medium">{col.collection}</span>
                                                <div className="flex items-center gap-2">
                                                    <Badge variant="secondary">{col.doc_count} docs</Badge>
                                                    <ChevronRight className="h-4 w-4 text-muted-foreground" />
                                                </div>
                                            </button>
                                        ))}
                                    </div>
                                ))}
                            </div>
                        )}
                    </CardContent>
                </Card>

                {/* Documents List */}
                <Card>
                    <CardHeader>
                        <CardTitle className="text-lg">
                            {selectedCollection
                                ? `Documents in "${selectedCollection.collection}"`
                                : 'Select a Collection'}
                        </CardTitle>
                        {selectedCollection && (
                            <CardDescription>
                                User: {selectedCollection.userId}
                            </CardDescription>
                        )}
                    </CardHeader>
                    <CardContent>
                        {!selectedCollection ? (
                            <div className="text-center py-8 text-muted-foreground">
                                Select a collection to view documents
                            </div>
                        ) : docsLoading ? (
                            <div className="text-center py-8 text-muted-foreground">Loading documents...</div>
                        ) : documents.length === 0 ? (
                            <div className="text-center py-8 text-muted-foreground">No documents in this collection</div>
                        ) : (
                            <Table>
                                <TableHeader>
                                    <TableRow>
                                        <TableHead>ID</TableHead>
                                        <TableHead>Created</TableHead>
                                        <TableHead className="text-right">Actions</TableHead>
                                    </TableRow>
                                </TableHeader>
                                <TableBody>
                                    {documents.map((doc) => (
                                        <TableRow key={doc.id}>
                                            <TableCell className="font-mono text-sm">
                                                {doc.id.substring(0, 12)}...
                                            </TableCell>
                                            <TableCell className="text-sm text-muted-foreground">
                                                {doc._created_at ? new Date(doc._created_at).toLocaleDateString() : '-'}
                                            </TableCell>
                                            <TableCell className="text-right">
                                                <Button
                                                    variant="ghost"
                                                    size="icon"
                                                    onClick={() => handleViewDoc(doc)}
                                                >
                                                    <Eye className="h-4 w-4" />
                                                </Button>
                                                <Button
                                                    variant="ghost"
                                                    size="icon"
                                                    className="text-destructive hover:text-destructive"
                                                    onClick={() => handleDeleteDoc(doc.id)}
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

            {/* Document Viewer Modal */}
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
