import { useState } from "react";
import {
    Zap, Plus, Power, PowerOff, Trash2, Play, Clock, ChevronRight, Code
} from "lucide-react";
import {
    useRules, useDeleteRule, useToggleRule, useTriggerRule,
    useDeviceDiscovery
} from "../hooks/useRules";
import type { Rule } from "../hooks/useRules";
import RuleEditor from "../components/RuleEditor";

export default function RulesPage() {
    const { data: rules = [], isLoading } = useRules();
    const { data: devices = [] } = useDeviceDiscovery();
    const deleteRule = useDeleteRule();
    const toggleRule = useToggleRule();
    const triggerRule = useTriggerRule();

    const [editorOpen, setEditorOpen] = useState(false);
    const [editingRule, setEditingRule] = useState<Rule | null>(null);

    const handleEdit = (rule: Rule) => {
        setEditingRule(rule);
        setEditorOpen(true);
    };

    const handleNew = () => {
        setEditingRule(null);
        setEditorOpen(true);
    };

    const handleEditorClose = () => {
        setEditorOpen(false);
        setEditingRule(null);
    };

    if (editorOpen) {
        return (
            <RuleEditor
                rule={editingRule}
                devices={devices}
                onClose={handleEditorClose}
            />
        );
    }

    return (
        <div className="space-y-6">
            {/* Header */}
            <div className="flex items-center justify-between">
                <div>
                    <h1 className="text-2xl font-bold flex items-center gap-2">
                        <Zap className="h-6 w-6 text-yellow-500" />
                        Rule Engine
                    </h1>
                    <p className="text-muted-foreground mt-1">
                        Create automation rules for your IoT devices using visual blocks or Lua scripts
                    </p>
                </div>
                <button
                    onClick={handleNew}
                    className="flex items-center gap-2 rounded-lg bg-primary px-4 py-2 text-sm font-medium text-primary-foreground hover:bg-primary/90 transition-colors"
                >
                    <Plus className="h-4 w-4" />
                    New Rule
                </button>
            </div>

            {/* Stats */}
            <div className="grid grid-cols-1 sm:grid-cols-3 gap-4">
                <div className="rounded-xl border bg-card p-4">
                    <div className="text-sm text-muted-foreground">Total Rules</div>
                    <div className="text-2xl font-bold mt-1">{rules.length}</div>
                </div>
                <div className="rounded-xl border bg-card p-4">
                    <div className="text-sm text-muted-foreground">Active</div>
                    <div className="text-2xl font-bold mt-1 text-green-500">
                        {rules.filter(r => r.enabled).length}
                    </div>
                </div>
                <div className="rounded-xl border bg-card p-4">
                    <div className="text-sm text-muted-foreground">Total Fires</div>
                    <div className="text-2xl font-bold mt-1 text-yellow-500">
                        {rules.reduce((sum, r) => sum + (r.fire_count || 0), 0)}
                    </div>
                </div>
            </div>

            {/* Rule List */}
            {isLoading ? (
                <div className="text-center py-12 text-muted-foreground">Loading rules...</div>
            ) : rules.length === 0 ? (
                <div className="text-center py-12 border rounded-xl bg-card">
                    <Zap className="h-12 w-12 mx-auto text-muted-foreground/30" />
                    <h3 className="mt-4 text-lg font-medium">No rules yet</h3>
                    <p className="text-muted-foreground mt-1">
                        Create your first automation rule to get started
                    </p>
                    <button
                        onClick={handleNew}
                        className="mt-4 inline-flex items-center gap-2 rounded-lg bg-primary px-4 py-2 text-sm font-medium text-primary-foreground hover:bg-primary/90"
                    >
                        <Plus className="h-4 w-4" />
                        Create Rule
                    </button>
                </div>
            ) : (
                <div className="space-y-3">
                    {rules.map((rule) => (
                        <div
                            key={rule.id}
                            className="flex items-center gap-4 rounded-xl border bg-card p-4 hover:border-primary/30 transition-colors cursor-pointer"
                            onClick={() => handleEdit(rule)}
                        >
                            {/* Status indicator */}
                            <div className={`h-3 w-3 rounded-full flex-shrink-0 ${
                                rule.enabled ? "bg-green-500" : "bg-muted-foreground/30"
                            }`} />

                            {/* Info */}
                            <div className="flex-1 min-w-0">
                                <div className="font-medium truncate">{rule.name}</div>
                                <div className="text-sm text-muted-foreground flex items-center gap-2 mt-0.5">
                                    {rule.logic?.type === "lua" && (
                                        <span className="inline-flex items-center gap-1 text-xs bg-purple-500/10 text-purple-500 px-1.5 py-0.5 rounded">
                                            <Code className="h-3 w-3" /> Lua
                                        </span>
                                    )}
                                    {rule.logic?.type === "blockly" && (
                                        <span className="inline-flex items-center gap-1 text-xs bg-blue-500/10 text-blue-500 px-1.5 py-0.5 rounded">
                                            <Zap className="h-3 w-3" /> Blockly
                                        </span>
                                    )}
                                    {rule.trigger?.type === "scheduled" && (
                                        <span className="inline-flex items-center gap-1 text-xs bg-amber-500/10 text-amber-500 px-1.5 py-0.5 rounded">
                                            <Clock className="h-3 w-3" /> {rule.trigger.schedule}
                                        </span>
                                    )}
                                    {rule.description && (
                                        <span className="truncate">{rule.description}</span>
                                    )}
                                </div>
                            </div>

                            {/* Fire count */}
                            <div className="text-sm text-muted-foreground text-right flex-shrink-0">
                                <div className="font-mono">{rule.fire_count || 0}×</div>
                                {rule.last_fired && (
                                    <div className="text-xs">
                                        {new Date(rule.last_fired).toLocaleString()}
                                    </div>
                                )}
                            </div>

                            {/* Actions */}
                            <div className="flex items-center gap-1 flex-shrink-0" onClick={(e) => e.stopPropagation()}>
                                <button
                                    onClick={() => triggerRule.mutate(rule.id)}
                                    className="p-1.5 rounded-md hover:bg-muted transition-colors"
                                    title="Trigger now"
                                >
                                    <Play className="h-4 w-4 text-blue-500" />
                                </button>
                                <button
                                    onClick={() => toggleRule.mutate({ id: rule.id, enabled: !rule.enabled })}
                                    className="p-1.5 rounded-md hover:bg-muted transition-colors"
                                    title={rule.enabled ? "Disable" : "Enable"}
                                >
                                    {rule.enabled
                                        ? <Power className="h-4 w-4 text-green-500" />
                                        : <PowerOff className="h-4 w-4 text-muted-foreground" />
                                    }
                                </button>
                                <button
                                    onClick={() => {
                                        if (confirm(`Delete rule "${rule.name}"?`)) {
                                            deleteRule.mutate(rule.id);
                                        }
                                    }}
                                    className="p-1.5 rounded-md hover:bg-destructive/10 transition-colors"
                                    title="Delete"
                                >
                                    <Trash2 className="h-4 w-4 text-destructive" />
                                </button>
                            </div>

                            <ChevronRight className="h-4 w-4 text-muted-foreground flex-shrink-0" />
                        </div>
                    ))}
                </div>
            )}
        </div>
    );
}
