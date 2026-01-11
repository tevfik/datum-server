import { useState } from 'react';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Input } from '@/components/ui/input';
import { Button } from '@/components/ui/button';
import { Textarea } from '@/components/ui/textarea';
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '@/components/ui/select';
import { Play, RotateCw } from 'lucide-react';
import axios from 'axios';

interface ResponseData {
    status: number;
    statusText: string;
    headers: Record<string, string>;
    data: any;
    duration: number;
    size: number;
}

export function HttpTab() {
    const [method, setMethod] = useState('GET');
    const [url, setUrl] = useState('/api/admin/config'); // Default to a safe GET
    const [headers, setHeaders] = useState('{\n  "Content-Type": "application/json"\n}');
    const [body, setBody] = useState('{\n\n}');
    const [response, setResponse] = useState<ResponseData | null>(null);
    const [isLoading, setIsLoading] = useState(false);
    const [error, setError] = useState<string | null>(null);

    const [autoAuth, setAutoAuth] = useState(true);

    const handleSend = async () => {
        setIsLoading(true);
        setError(null);
        setResponse(null);

        const startTime = performance.now();

        try {
            // Parse headers
            let headerObj: Record<string, string> = {};
            try {
                headerObj = JSON.parse(headers);
            } catch (e) {
                throw new Error("Invalid Headers JSON");
            }

            // Auto-Inject Auth Token
            if (autoAuth) {
                const token = localStorage.getItem('datum_token');
                if (token) {
                    headerObj['Authorization'] = `Bearer ${token}`;
                }
            }

            // Parse body
            let bodyData = undefined;
            if (method !== 'GET' && method !== 'HEAD') {
                try {
                    bodyData = JSON.parse(body);
                } catch (e) {
                    throw new Error("Invalid Body JSON");
                }
            }

            // Execute Request
            const res = await axios({
                method,
                url,
                headers: headerObj,
                data: bodyData,
                validateStatus: () => true, // Don't throw on error status
            });

            const endTime = performance.now();

            // Calculate size (approx)
            const size = JSON.stringify(res.data).length + JSON.stringify(res.headers).length;

            setResponse({
                status: res.status,
                statusText: res.statusText,
                headers: res.headers as Record<string, string>,
                data: res.data,
                duration: Math.round(endTime - startTime),
                size: size
            });

        } catch (err: any) {
            setError(err.message || "Request Failed");
        } finally {
            setIsLoading(false);
        }
    };

    return (
        <div className="space-y-6">
            <Card>
                <CardHeader>
                    <CardTitle>HTTP Client</CardTitle>
                    <CardDescription>Test API endpoints directly from the dashboard.</CardDescription>
                </CardHeader>
                <CardContent className="space-y-4">
                    {/* Request Line */}
                    <div className="flex gap-2">
                        <Select value={method} onValueChange={setMethod}>
                            <SelectTrigger className="w-[100px] font-mono font-bold">
                                <SelectValue placeholder="Method" />
                            </SelectTrigger>
                            <SelectContent>
                                <SelectItem value="GET">GET</SelectItem>
                                <SelectItem value="POST">POST</SelectItem>
                                <SelectItem value="PUT">PUT</SelectItem>
                                <SelectItem value="DELETE">DELETE</SelectItem>
                                <SelectItem value="PATCH">PATCH</SelectItem>
                            </SelectContent>
                        </Select>
                        <Input
                            value={url}
                            onChange={(e) => setUrl(e.target.value)}
                            placeholder="/api/..."
                            className="font-mono"
                        />
                        <Button onClick={handleSend} disabled={isLoading}>
                            {isLoading ? <RotateCw className="mr-2 h-4 w-4 animate-spin" /> : <Play className="mr-2 h-4 w-4" />}
                            Send
                        </Button>
                    </div>

                    <div className="flex items-center space-x-2">
                        <input
                            type="checkbox"
                            id="autoAuth"
                            checked={autoAuth}
                            onChange={(e) => setAutoAuth(e.target.checked)}
                            className="h-4 w-4 rounded border-gray-300 text-primary focus:ring-primary"
                        />
                        <label htmlFor="autoAuth" className="text-sm font-medium leading-none peer-disabled:cursor-not-allowed peer-disabled:opacity-70">
                            Auto-Inject Auth Token
                        </label>
                    </div>

                    {/* Editors */}
                    <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                        <div className="space-y-2">
                            <label className="text-sm font-medium">Headers (JSON)</label>
                            <Textarea
                                value={headers}
                                onChange={(e) => setHeaders(e.target.value)}
                                className="font-mono text-xs h-[150px]"
                            />
                        </div>
                        <div className="space-y-2">
                            <label className="text-sm font-medium">Body (JSON)</label>
                            <Textarea
                                value={body}
                                onChange={(e) => setBody(e.target.value)}
                                className="font-mono text-xs h-[150px]"
                                disabled={method === 'GET' || method === 'HEAD'}
                            />
                        </div>
                    </div>

                    {/* Error */}
                    {error && (
                        <div className="bg-destructive/15 text-destructive p-3 rounded-md text-sm">
                            Error: {error}
                        </div>
                    )}

                    {/* Response */}
                    {response && (
                        <div className="space-y-2 border-t pt-4">
                            <div className="flex items-center justify-between">
                                <h3 className="text-sm font-semibold">Response</h3>
                                <div className="flex gap-4 text-xs text-muted-foreground">
                                    <span className={response.status >= 200 && response.status < 300 ? "text-green-500 font-bold" : "text-red-500 font-bold"}>
                                        {response.status} {response.statusText}
                                    </span>
                                    <span>{response.duration}ms</span>
                                    <span>{response.size} B</span>
                                </div>
                            </div>

                            <div className="bg-muted p-4 rounded-md overflow-x-auto">
                                <pre className="text-xs font-mono text-foreground">
                                    {typeof response.data === 'object'
                                        ? JSON.stringify(response.data, null, 2)
                                        : response.data}
                                </pre>
                            </div>

                            <div className="space-y-1">
                                <h4 className="text-xs font-semibold text-muted-foreground">Response Headers</h4>
                                <div className="bg-muted/50 p-2 rounded-md text-xs font-mono h-24 overflow-y-auto">
                                    {Object.entries(response.headers).map(([k, v]) => (
                                        <div key={k} className="flex gap-2">
                                            <span className="font-semibold">{k}:</span>
                                            <span className="break-all">{v}</span>
                                        </div>
                                    ))}
                                </div>
                            </div>
                        </div>
                    )}
                </CardContent>
            </Card>
        </div>
    );
}

// TODO: Replace with generic axios instance if available globally? 
// Current axios usages seem to stem from services/authService.ts axiosInstance
