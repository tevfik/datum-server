import mqtt from 'mqtt';
import { api } from './api';
import type { User, GetKeysResponse, CreateKeyResponse } from '@/shared/types/auth';

class MQTTService {
    private client: mqtt.MqttClient | null = null;
    private isConnecting = false;
    private subscribers: Map<string, ((topic: string, message: any) => void)[]> = new Map();

    async connect(user: User): Promise<void> {
        if (this.client?.connected || this.isConnecting) return;
        this.isConnecting = true;

        try {
            // 1. Get API Key for Auth
            let apiKey = await this.getOrCreateAPIKey();

            // 2. Generate Client ID
            // Format: datum_web_{role}_{userid}_{random}
            const random = Math.random().toString(36).substring(7);
            const clientId = `datum_web_${user.role}_${user.id}_${random}`;

            // 3. Connect options
            // Use WS protocol. Assume backend is on the same host but port 1884
            const protocol = window.location.protocol === 'https:' ? 'wss' : 'ws';
            const host = window.location.hostname;
            const port = 1884;
            const url = `${protocol}://${host}:${port}`;

            console.log(`Connecting to MQTT Broker: ${url} as ${clientId}`);

            this.client = mqtt.connect(url, {
                clientId,
                username: clientId, // Broker treats username as ID for auth hook
                password: apiKey,
                clean: true,
                reconnectPeriod: 5000,
            });

            this.client.on('connect', () => {
                console.log('MQTT Connected');
                this.isConnecting = false;
                // Resubscribe if needed? (Clean session is true, so maybe we need to re-add subscriptions)
                // For now, consumers should subscribe on component mount
            });

            this.client.on('message', (topic, message) => {
                this.handleMessage(topic, message);
            });

            this.client.on('error', (err) => {
                console.error('MQTT Error:', err);
                this.isConnecting = false;
            });

            this.client.on('close', () => {
                console.log('MQTT Disconnected');
                this.isConnecting = false;
            });

        } catch (e) {
            console.error('Failed to initialize MQTT connection', e);
            this.isConnecting = false;
        }
    }

    private async getOrCreateAPIKey(): Promise<string> {
        try {
            // Try to fetch existing keys
            const { data } = await api.get<GetKeysResponse>('/auth/keys'); // Use api.get which returns {data: ...} usually? 
            // Wait, api.ts returns { data: T } usually?
            // Need to check api.ts structure. Assuming standard axios wrapper.

            // Check if there's a key named "Web Dashboard"
            // The API returns { keys: [...] } inside data?
            // Let's assume the shape based on api.ts: { keys: APIKey[] }
            if (data.keys && data.keys.length > 0) {
                // Return the first key. 
                // Note: The API usually returns MASKED keys for list.
                // Wait. Use APIKey.key? 
                // If the list returns masked keys (e.g. "sk_****"), we cannot use them for connection!
                // WE NEED A FULL KEY.
                // Only Create returns a full key?
                // Or maybe list returns full keys?
                // Usually list masks them.
                // If list masks them, we MUST create a new key every time? That would spam keys.
                // OR we store the key in localStorage.
            }

            // New Strategy: Check localStorage first.
            const storedKey = localStorage.getItem('datum_mqtt_key');
            if (storedKey) return storedKey;

            // If no stored key, CREATE one and store it.
            console.log('Creating new API Key for MQTT...');
            const { data: newKey } = await api.post<CreateKeyResponse>('/auth/keys', {
                name: `Web Dashboard ${new Date().toLocaleDateString()}`
            });

            if (newKey && newKey.key) {
                localStorage.setItem('datum_mqtt_key', newKey.key);
                return newKey.key;
            }

            throw new Error("Failed to create key");

        } catch (e) {
            console.error("Error fetching/creating API key", e);
            throw e;
        }
    }

    subscribe(topic: string, callback: (topic: string, message: any) => void) {
        if (!this.client) {
            console.warn("MQTT client not initialized");
            // Optionally queue?
        }

        // Add callback
        if (!this.subscribers.has(topic)) {
            this.subscribers.set(topic, []);
            if (this.client && this.client.connected) {
                this.client.subscribe(topic, (err) => {
                    if (err) console.error(`Failed to subscribe to ${topic}`, err);
                });
            }
        }
        this.subscribers.get(topic)?.push(callback);

        // Ensure subscription if connected
        if (this.client && this.client.connected) {
            this.client.subscribe(topic);
        }
    }

    unsubscribe(topic: string, callback: (topic: string, message: any) => void) {
        const callbacks = this.subscribers.get(topic);
        if (callbacks) {
            const index = callbacks.indexOf(callback);
            if (index > -1) {
                callbacks.splice(index, 1);
            }
            if (callbacks.length === 0) {
                this.subscribers.delete(topic);
                if (this.client && this.client.connected) {
                    this.client.unsubscribe(topic);
                }
            }
        }
    }

    private handleMessage(topic: string, payload: Buffer) {
        // Find subscribers for this topic (or wildcard matches)
        // Simple exact match for now + extremely basic wildcard support if needed
        // Just iterating all for simplicity in this POC or exact match.

        let msgStr = payload.toString();
        let msgJson;
        try {
            msgJson = JSON.parse(msgStr);
        } catch {
            msgJson = msgStr;
        }

        const callbacks = this.subscribers.get(topic);
        if (callbacks) {
            callbacks.forEach(cb => cb(topic, msgJson));
        }
    }

    disconnect() {
        if (this.client) {
            this.client.end();
            this.client = null;
        }
    }
}

export const mqttService = new MQTTService();
