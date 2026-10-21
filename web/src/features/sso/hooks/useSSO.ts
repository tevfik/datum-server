import { useQuery, useMutation, useQueryClient } from "@tanstack/react-query";
import { api } from "@/services/api";

export interface SSOCLient {
    name: string;
    client_id: string;
    status: string;
    allowed_roles: string[];
    allowed_emails: string[];
    created_at: string;
}

export interface CreateSSOClientRequest {
    name: string;
    allowed_roles: string[];
    allowed_emails: string[];
}

export function useSSOClients() {
    return useQuery({
        queryKey: ["sso_clients"],
        queryFn: async (): Promise<SSOCLient[]> => {
            // Note: In the backend, we mounted the admin API at /auth/provider/clients
            const response = await api.get("/auth/provider/clients");
            return response.data;
        },
    });
}

export function useCreateSSOClient() {
    const queryClient = useQueryClient();

    return useMutation({
        mutationFn: async (req: CreateSSOClientRequest) => {
            const response = await api.post("/auth/provider/clients", req);
            return response.data;
        },
        onSuccess: () => {
            queryClient.invalidateQueries({ queryKey: ["sso_clients"] });
        },
    });
}

export function useDeleteSSOClient() {
    const queryClient = useQueryClient();

    return useMutation({
        mutationFn: async (name: string) => {
            const response = await api.delete(`/auth/provider/clients/${name}`);
            return response.data;
        },
        onSuccess: () => {
            queryClient.invalidateQueries({ queryKey: ["sso_clients"] });
        },
    });
}
