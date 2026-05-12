import { useState } from "react";
import { ArrowLeft, Save, Zap, Code, Puzzle } from "lucide-react";
import {
    useCreateRule, useUpdateRule
} from "../hooks/useRules";
import type { Rule, DeviceInfo, RuleCondition, RuleAction } from "../hooks/useRules";
import BlocklyEditor from "./BlocklyEditor";

interface RuleEditorProps {
    rule: Rule | null;
    devices: DeviceInfo[];
    onClose: () => void;
}

const OPERATORS = [
    { value: "gt", label: "> Greater than" },
    { value: "gte", label: "≥ Greater or equal" },
    { value: "lt", label: "< Less than" },
    { value: "lte", label: "≤ Less or equal" },
    { value: "eq", label: "= Equal" },
    { value: "neq", label: "≠ Not equal" },
    { value: "contains", label: "∈ Contains" },
];

const ACTION_TYPES = [
    { value: "log", label: "📋 Log Event", description: "Write to server log" },
    { value: "notify", label: "🔔 Notification", description: "Send push notification" },
    { value: "mqtt", label: "📡 MQTT Publish", description: "Publish MQTT message" },
    { value: "command", label: "⚡ Device Command", description: "Send command to device" },
    { value: "webhook", label: "🌐 Webhook", description: "Trigger webhook" },
];

const TRIGGER_TYPES = [
    { value: "on_data", label: "📊 When data arrives", description: "Trigger on new telemetry" },
    { value: "scheduled", label: "⏰ On schedule", description: "Run on cron schedule" },
    { value: "manual", label: "👆 Manual only", description: "Only via API/button" },
];

export default function RuleEditor({ rule, devices, onClose }: RuleEditorProps) {
    const createRule = useCreateRule();
    const updateRule = useUpdateRule();
    const isEdit = !!rule;

    // Form state
    const [name, setName] = useState(rule?.name || "");
    const [description, setDescription] = useState(rule?.description || "");
    const [triggerType, setTriggerType] = useState(rule?.trigger?.type || "on_data");
    const [triggerDeviceId, setTriggerDeviceId] = useState(
        rule?.trigger?.device_id || rule?.device_id || ""
    );
    const [schedule, setSchedule] = useState(rule?.trigger?.schedule || "");
    const [logicType, setLogicType] = useState<string>(rule?.logic?.type || "conditions");
    const [logicOp, setLogicOp] = useState(rule?.logic?.logic_op || "and");
    const [conditions, setConditions] = useState<RuleCondition[]>(
        rule?.logic?.conditions || rule?.conditions || [{ field: "", operator: "gt", value: 0 }]
    );
    const [blocklyJson, setBlocklyJson] = useState<Record<string, any>>(rule?.logic?.blockly_json || {});
    const [luaScript, setLuaScript] = useState(rule?.logic?.lua_script || "-- Access device data via ctx.data\n-- Example: return ctx.data.temperature > 30\n\nreturn false");
    const [actions, setActions] = useState<RuleAction[]>(
        rule?.actions || [{ type: "log", config: {} }]
    );

    // Get properties for selected device
    const selectedDevice = devices.find(d => d.device_id === triggerDeviceId);
    const availableFields = selectedDevice?.properties?.map(p => ({
        value: p.key,
        label: `${p.title}${p.unit ? ` (${p.unit})` : ""}`
    })) || [];

    const addCondition = () => {
        setConditions([...conditions, { field: "", operator: "gt", value: 0 }]);
    };

    const removeCondition = (index: number) => {
        setConditions(conditions.filter((_, i) => i !== index));
    };

    const updateCondition = (index: number, key: string, value: any) => {
        const updated = [...conditions];
        (updated[index] as any)[key] = value;
        setConditions(updated);
    };

    const addAction = () => {
        setActions([...actions, { type: "log", config: {} }]);
    };

    const removeAction = (index: number) => {
        setActions(actions.filter((_, i) => i !== index));
    };

    const updateAction = (index: number, key: string, value: any) => {
        const updated = [...actions];
        if (key === "type") {
            updated[index] = { type: value, config: {} };
        } else {
            updated[index].config = { ...updated[index].config, [key]: value };
        }
        setActions(updated);
    };

    const handleSave = async () => {
        const ruleData: Partial<Rule> = {
            name,
            description,
            device_id: triggerDeviceId,
            trigger: {
                type: triggerType,
                device_id: triggerDeviceId,
                schedule: triggerType === "scheduled" ? schedule : undefined,
            },
            logic: {
                type: logicType,
                logic_op: logicOp,
                conditions: logicType !== "lua" ? conditions : undefined,
                blockly_json: logicType === "blockly" ? blocklyJson : undefined,
                lua_script: logicType === "lua" ? luaScript : undefined,
            },
            actions,
        };

        try {
            if (isEdit && rule) {
                await updateRule.mutateAsync({ id: rule.id, ...ruleData });
            } else {
                await createRule.mutateAsync(ruleData);
            }
            onClose();
        } catch (err) {
            console.error("Failed to save rule:", err);
        }
    };

    return (
        <div className="space-y-6 max-w-3xl mx-auto">
            {/* Header */}
            <div className="flex items-center gap-4">
                <button
                    onClick={onClose}
                    className="p-2 rounded-lg hover:bg-muted transition-colors"
                >
                    <ArrowLeft className="h-5 w-5" />
                </button>
                <div className="flex-1">
                    <h1 className="text-xl font-bold">
                        {isEdit ? "Edit Rule" : "New Rule"}
                    </h1>
                </div>
                <button
                    onClick={handleSave}
                    disabled={!name || createRule.isPending || updateRule.isPending}
                    className="flex items-center gap-2 rounded-lg bg-primary px-4 py-2 text-sm font-medium text-primary-foreground hover:bg-primary/90 disabled:opacity-50 transition-colors"
                >
                    <Save className="h-4 w-4" />
                    {createRule.isPending || updateRule.isPending ? "Saving..." : "Save"}
                </button>
            </div>

            {/* Name & Description */}
            <div className="rounded-xl border bg-card p-5 space-y-4">
                <h2 className="text-sm font-semibold text-muted-foreground uppercase tracking-wider">General</h2>
                <div>
                    <label className="text-sm font-medium">Rule Name</label>
                    <input
                        type="text"
                        value={name}
                        onChange={(e) => setName(e.target.value)}
                        placeholder="e.g., Greenhouse Temperature Alert"
                        className="mt-1 w-full rounded-lg border bg-background px-3 py-2 text-sm"
                    />
                </div>
                <div>
                    <label className="text-sm font-medium">Description</label>
                    <input
                        type="text"
                        value={description}
                        onChange={(e) => setDescription(e.target.value)}
                        placeholder="Optional description..."
                        className="mt-1 w-full rounded-lg border bg-background px-3 py-2 text-sm"
                    />
                </div>
            </div>

            {/* Trigger */}
            <div className="rounded-xl border bg-card p-5 space-y-4">
                <h2 className="text-sm font-semibold text-muted-foreground uppercase tracking-wider">Trigger</h2>
                <div className="grid grid-cols-1 sm:grid-cols-3 gap-3">
                    {TRIGGER_TYPES.map(t => (
                        <button
                            key={t.value}
                            onClick={() => setTriggerType(t.value)}
                            className={`text-left rounded-lg border p-3 transition-colors ${
                                triggerType === t.value
                                    ? "border-primary bg-primary/5"
                                    : "hover:border-muted-foreground/30"
                            }`}
                        >
                            <div className="text-sm font-medium">{t.label}</div>
                            <div className="text-xs text-muted-foreground mt-0.5">{t.description}</div>
                        </button>
                    ))}
                </div>

                {/* Device selector */}
                <div>
                    <label className="text-sm font-medium">Device</label>
                    <select
                        value={triggerDeviceId}
                        onChange={(e) => setTriggerDeviceId(e.target.value)}
                        className="mt-1 w-full rounded-lg border bg-background px-3 py-2 text-sm"
                    >
                        <option value="">All devices</option>
                        {devices.map(d => (
                            <option key={d.device_id} value={d.device_id}>
                                {d.device_name} ({d.device_type} - {d.device_id.split('_').pop()})
                            </option>
                        ))}
                    </select>
                </div>

                {triggerType === "scheduled" && (
                    <div>
                        <label className="text-sm font-medium">Cron Schedule</label>
                        <input
                            type="text"
                            value={schedule}
                            onChange={(e) => setSchedule(e.target.value)}
                            placeholder="e.g., 0 */5 * * * * (every 5 minutes)"
                            className="mt-1 w-full rounded-lg border bg-background px-3 py-2 text-sm font-mono"
                        />
                        <p className="text-xs text-muted-foreground mt-1">
                            Format: seconds minutes hours day-of-month month day-of-week
                        </p>
                    </div>
                )}
            </div>

            {/* Logic Type Selector */}
            <div className="rounded-xl border bg-card p-5 space-y-4">
                <h2 className="text-sm font-semibold text-muted-foreground uppercase tracking-wider">Logic</h2>
                <div className="grid grid-cols-1 sm:grid-cols-3 gap-3">
                    <button
                        onClick={() => setLogicType("conditions")}
                        className={`text-left rounded-lg border p-3 transition-colors ${
                            logicType === "conditions" ? "border-primary bg-primary/5" : "hover:border-muted-foreground/30"
                        }`}
                    >
                        <div className="text-sm font-medium flex items-center gap-1.5">
                            <Zap className="h-4 w-4" /> Simple Conditions
                        </div>
                        <div className="text-xs text-muted-foreground mt-0.5">If-then rules with dropdowns</div>
                    </button>
                    <button
                        onClick={() => setLogicType("blockly")}
                        className={`text-left rounded-lg border p-3 transition-colors ${
                            logicType === "blockly" ? "border-primary bg-primary/5" : "hover:border-muted-foreground/30"
                        }`}
                    >
                        <div className="text-sm font-medium flex items-center gap-1.5">
                            <Puzzle className="h-4 w-4" /> Visual Blocks
                        </div>
                        <div className="text-xs text-muted-foreground mt-0.5">Drag-and-drop Blockly editor</div>
                    </button>
                    <button
                        onClick={() => setLogicType("lua")}
                        className={`text-left rounded-lg border p-3 transition-colors ${
                            logicType === "lua" ? "border-primary bg-primary/5" : "hover:border-muted-foreground/30"
                        }`}
                    >
                        <div className="text-sm font-medium flex items-center gap-1.5">
                            <Code className="h-4 w-4" /> Lua Script
                        </div>
                        <div className="text-xs text-muted-foreground mt-0.5">Advanced custom logic</div>
                    </button>
                </div>

                {/* Conditions editor */}
                {logicType === "conditions" && (
                    <div className="space-y-3">
                        <div className="flex items-center gap-2">
                            <label className="text-sm font-medium">Match</label>
                            <select
                                value={logicOp}
                                onChange={(e) => setLogicOp(e.target.value)}
                                className="rounded-md border bg-background px-2 py-1 text-sm"
                            >
                                <option value="and">ALL conditions (AND)</option>
                                <option value="or">ANY condition (OR)</option>
                            </select>
                        </div>

                        {conditions.map((cond, i) => (
                            <div key={i} className="flex items-center gap-2">
                                <select
                                    value={cond.field}
                                    onChange={(e) => updateCondition(i, "field", e.target.value)}
                                    className="flex-1 rounded-md border bg-background px-2 py-1.5 text-sm"
                                >
                                    <option value="">Select field...</option>
                                    {availableFields.map(f => (
                                        <option key={f.value} value={f.value}>{f.label}</option>
                                    ))}
                                    {/* Allow custom field input */}
                                    {cond.field && !availableFields.find(f => f.value === cond.field) && (
                                        <option value={cond.field}>{cond.field} (custom)</option>
                                    )}
                                </select>
                                <select
                                    value={cond.operator}
                                    onChange={(e) => updateCondition(i, "operator", e.target.value)}
                                    className="rounded-md border bg-background px-2 py-1.5 text-sm"
                                >
                                    {OPERATORS.map(op => (
                                        <option key={op.value} value={op.value}>{op.label}</option>
                                    ))}
                                </select>
                                <input
                                    type="text"
                                    value={String(cond.value)}
                                    onChange={(e) => {
                                        const v = isNaN(Number(e.target.value))
                                            ? e.target.value
                                            : Number(e.target.value);
                                        updateCondition(i, "value", v);
                                    }}
                                    className="w-24 rounded-md border bg-background px-2 py-1.5 text-sm"
                                    placeholder="Value"
                                />
                                {conditions.length > 1 && (
                                    <button
                                        onClick={() => removeCondition(i)}
                                        className="p-1 rounded hover:bg-destructive/10 text-destructive"
                                    >
                                        ×
                                    </button>
                                )}
                            </div>
                        ))}
                        <button
                            onClick={addCondition}
                            className="text-sm text-primary hover:underline"
                        >
                            + Add condition
                        </button>
                    </div>
                )}

                {/* Blockly editor */}
                {logicType === "blockly" && (
                    <div className="mt-4">
                        <BlocklyEditor
                            devices={devices}
                            initialJson={blocklyJson}
                            onChange={(json, compiledConditions) => {
                                setBlocklyJson(json);
                                if (compiledConditions && compiledConditions.length > 0) {
                                    setConditions(compiledConditions);
                                }
                            }}
                        />
                    </div>
                )}

                {/* Lua editor */}
                {logicType === "lua" && (
                    <div>
                        <label className="text-sm font-medium">Lua Script</label>
                        <p className="text-xs text-muted-foreground mb-2">
                            Access device data via <code className="bg-muted px-1 rounded">ctx.data.fieldname</code>.
                            Must return <code className="bg-muted px-1 rounded">true</code> or <code className="bg-muted px-1 rounded">false</code>.
                        </p>
                        <textarea
                            value={luaScript}
                            onChange={(e) => setLuaScript(e.target.value)}
                            rows={10}
                            className="w-full rounded-lg border bg-background px-3 py-2 text-sm font-mono resize-y"
                            spellCheck={false}
                        />
                    </div>
                )}
            </div>

            {/* Actions */}
            <div className="rounded-xl border bg-card p-5 space-y-4">
                <h2 className="text-sm font-semibold text-muted-foreground uppercase tracking-wider">Actions</h2>
                <p className="text-xs text-muted-foreground">What happens when the rule fires</p>

                {actions.map((action, i) => (
                    <div key={i} className="rounded-lg border p-3 space-y-3">
                        <div className="flex items-center gap-2">
                            <select
                                value={action.type}
                                onChange={(e) => updateAction(i, "type", e.target.value)}
                                className="flex-1 rounded-md border bg-background px-2 py-1.5 text-sm"
                            >
                                {ACTION_TYPES.map(a => (
                                    <option key={a.value} value={a.value}>{a.label}</option>
                                ))}
                            </select>
                            {actions.length > 1 && (
                                <button
                                    onClick={() => removeAction(i)}
                                    className="p-1 rounded hover:bg-destructive/10 text-destructive"
                                >
                                    ×
                                </button>
                            )}
                        </div>

                        {/* Action-specific config */}
                        {action.type === "notify" && (
                            <div className="space-y-2">
                                <input
                                    type="text"
                                    value={action.config?.title || ""}
                                    onChange={(e) => updateAction(i, "title", e.target.value)}
                                    placeholder="Notification title"
                                    className="w-full rounded-md border bg-background px-2 py-1.5 text-sm"
                                />
                                <input
                                    type="text"
                                    value={action.config?.message || ""}
                                    onChange={(e) => updateAction(i, "message", e.target.value)}
                                    placeholder="Notification message"
                                    className="w-full rounded-md border bg-background px-2 py-1.5 text-sm"
                                />
                            </div>
                        )}
                        {action.type === "mqtt" && (
                            <div className="space-y-2">
                                <input
                                    type="text"
                                    value={action.config?.topic || ""}
                                    onChange={(e) => updateAction(i, "topic", e.target.value)}
                                    placeholder="MQTT topic (e.g., dev/fan/cmd/set)"
                                    className="w-full rounded-md border bg-background px-2 py-1.5 text-sm font-mono"
                                />
                            </div>
                        )}
                        {action.type === "command" && (
                            <div className="space-y-2">
                                <select
                                    value={action.config?.target_device || ""}
                                    onChange={(e) => updateAction(i, "target_device", e.target.value)}
                                    className="w-full rounded-md border bg-background px-2 py-1.5 text-sm"
                                >
                                    <option value="">Same device</option>
                                    {devices.map(d => (
                                        <option key={d.device_id} value={d.device_id}>
                                            {d.device_name} ({d.device_type} - {d.device_id.split('_').pop()})
                                        </option>
                                    ))}
                                </select>
                                <input
                                    type="text"
                                    value={action.config?.payload || ""}
                                    onChange={(e) => updateAction(i, "payload", e.target.value)}
                                    placeholder='Command payload JSON (e.g., {"fan_speed": 2000})'
                                    className="w-full rounded-md border bg-background px-2 py-1.5 text-sm font-mono"
                                />
                            </div>
                        )}
                    </div>
                ))}
                <button
                    onClick={addAction}
                    className="text-sm text-primary hover:underline"
                >
                    + Add action
                </button>
            </div>
        </div>
    );
}
