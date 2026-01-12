import { api } from '@/services/api';

export interface DataPoint {
    timestamp: string;
    timestamp_ms: number;
    data: Record<string, any>;
}

export interface HistoryResponse {
    device_id: string;
    count: number;
    data: DataPoint[];
    range: {
        start: string;
        end: string;
    };
    interval: string;
}

export const explorerService = {
    getHistory: async (deviceId: string, params: {
        start?: number; // unix ms
        stop?: string; // duration string like 1h
        range?: string; // preset like 24h
        start_rfc?: string;
        end_rfc?: string;
        limit?: number;
        int?: string; // interval
    }): Promise<HistoryResponse> => {
        const { data } = await api.get<HistoryResponse>(`/data/${deviceId}/history`, { params });
        return data;
    }
};
