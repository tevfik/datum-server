import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { api } from '@/services/api';

export interface RuleCondition {
    field: string;
    operator: string;
    value: number | string | boolean;
}

export interface RuleAction {
    type: string;
    config?: Record<string, any>;
}

export interface RuleTrigger {
    type: string;
    schedule?: string;
    device_id?: string;
}

export interface RuleLogic {
    type: string;
    conditions?: RuleCondition[];
    logic_op?: string;
    blockly_json?: Record<string, any>;
    lua_script?: string;
}

export interface Rule {
    id: string;
    owner_id?: string;
    name: string;
    description?: string;
    device_id?: string;
    trigger: RuleTrigger;
    logic: RuleLogic;
    conditions?: RuleCondition[];
    actions: RuleAction[];
    enabled: boolean;
    created_at: string;
    updated_at?: string;
    last_fired?: string;
    fire_count: number;
}

export interface DeviceProperty {
    key: string;
    title: string;
    type: string;
    unit?: string;
    read_only: boolean;
    widget?: string;
}

export interface DeviceInfo {
    device_id: string;
    device_name: string;
    device_type: string;
    properties: DeviceProperty[];
}

// ── Hooks ───────────────────────────────────────────────────────────────────

export function useRules() {
    return useQuery<Rule[]>({
        queryKey: ['rules'],
        queryFn: async () => {
            const { data } = await api.get('/rules');
            return data.rules || [];
        },
    });
}

export function useDeviceDiscovery() {
    return useQuery<DeviceInfo[]>({
        queryKey: ['rules', 'discovery'],
        queryFn: async () => {
            const { data } = await api.get('/rules/discovery');
            return data.devices || [];
        },
    });
}

export function useBlockDefinitions() {
    return useQuery({
        queryKey: ['rules', 'blocks'],
        queryFn: async () => {
            const { data } = await api.get('/rules/blocks');
            return data;
        },
    });
}

export function useCreateRule() {
    const qc = useQueryClient();
    return useMutation({
        mutationFn: async (rule: Partial<Rule>) => {
            const { data } = await api.post('/rules', rule);
            return data;
        },
        onSuccess: () => qc.invalidateQueries({ queryKey: ['rules'] }),
    });
}

export function useUpdateRule() {
    const qc = useQueryClient();
    return useMutation({
        mutationFn: async ({ id, ...rule }: Partial<Rule> & { id: string }) => {
            const { data } = await api.put(`/rules/${id}`, rule);
            return data;
        },
        onSuccess: () => qc.invalidateQueries({ queryKey: ['rules'] }),
    });
}

export function useDeleteRule() {
    const qc = useQueryClient();
    return useMutation({
        mutationFn: async (id: string) => {
            await api.delete(`/rules/${id}`);
        },
        onSuccess: () => qc.invalidateQueries({ queryKey: ['rules'] }),
    });
}

export function useToggleRule() {
    const qc = useQueryClient();
    return useMutation({
        mutationFn: async ({ id, enabled }: { id: string; enabled: boolean }) => {
            const { data } = await api.put(`/rules/${id}/${enabled ? 'enable' : 'disable'}`);
            return data;
        },
        onSuccess: () => qc.invalidateQueries({ queryKey: ['rules'] }),
    });
}

export function useTriggerRule() {
    return useMutation({
        mutationFn: async (id: string) => {
            const { data } = await api.post(`/rules/${id}/trigger`);
            return data;
        },
    });
}
