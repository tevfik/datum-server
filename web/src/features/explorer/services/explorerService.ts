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
    getHistory: async (deviceId: string, { start, end, limit }: {
        start?: Date; // Date object
        end?: Date; // Date object
        limit?: number;
    }): Promise<HistoryResponse> => {
        // Use unified /dev/:id/data endpoint
        let url = `/dev/${deviceId}/data`;
        const queryParams = new URLSearchParams();

        if (start) queryParams.append('start_rfc', start.toISOString()); // Map from -> start_rfc
        if (end) queryParams.append('end_rfc', end.toISOString());     // Map to -> end_rfc
        if (limit) queryParams.append('limit', limit.toString());

        if (queryParams.toString()) {
            url = `${url}?${queryParams.toString()}`;
        }

        const { data } = await api.get<HistoryResponse>(url);
        return data;
    }
};
