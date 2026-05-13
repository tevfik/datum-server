import { useEffect, useRef, useState } from 'react';
import type { DeviceInfo } from '../hooks/useRules';

interface BlocklyEditorProps {
    initialJson?: Record<string, any>;
    devices: DeviceInfo[];
    onChange: (json: Record<string, any>, compiledConditions: any[]) => void;
}

// Module-level cache so the Blockly scripts are only ever fetched once.
let blocklyLoadPromise: Promise<any> | null = null;

function loadScript(src: string): Promise<void> {
    return new Promise((resolve, reject) => {
        const existing = document.querySelector(`script[src="${src}"]`) as HTMLScriptElement | null;
        if (existing) {
            if (existing.dataset.loaded === 'true') {
                resolve();
                return;
            }
            existing.addEventListener('load', () => resolve());
            existing.addEventListener('error', () => reject(new Error(`Failed to load ${src}`)));
            return;
        }

        const script = document.createElement('script');
        script.src = src;
        script.async = false; // preserve execution order
        script.onload = () => {
            script.dataset.loaded = 'true';
            resolve();
        };
        script.onerror = () => reject(new Error(`Failed to load ${src}`));
        document.body.appendChild(script);
    });
}

function ensureBlocklyLoaded(): Promise<any> {
    const w = window as any;
    if (w.Blockly && w.Blockly.libraryBlocks) {
        return Promise.resolve(w.Blockly);
    }
    if (blocklyLoadPromise) return blocklyLoadPromise;

    // Some bundlers expose `define` (AMD). Blockly's compressed bundles will
    // route through AMD instead of attaching to window.Blockly. Temporarily
    // hide define for the duration of the load, then restore it.
    const savedDefine = w.define;
    if (savedDefine && savedDefine.amd) {
        try { delete w.define; } catch { w.define = undefined; }
    }

    blocklyLoadPromise = (async () => {
        try {
            await loadScript('/blockly/blockly_compressed.js');
            await loadScript('/blockly/blocks_compressed.js');
            await loadScript('/blockly/msg/en.js');
            if (!w.Blockly) {
                throw new Error('Blockly global not found after script load');
            }
            return w.Blockly;
        } finally {
            if (savedDefine !== undefined) w.define = savedDefine;
        }
    })();

    return blocklyLoadPromise;
}

export default function BlocklyEditor({ initialJson, devices, onChange }: BlocklyEditorProps) {
    const blocklyDiv = useRef<HTMLDivElement>(null);
    const workspaceRef = useRef<any>(null);
    const onChangeRef = useRef(onChange);
    onChangeRef.current = onChange;

    const [status, setStatus] = useState<'loading' | 'ready' | 'error'>('loading');
    const [errorMsg, setErrorMsg] = useState<string>('');

    useEffect(() => {
        let cancelled = false;
        ensureBlocklyLoaded()
            .then(() => { if (!cancelled) setStatus('ready'); })
            .catch((err) => {
                if (!cancelled) {
                    setErrorMsg(err?.message ?? String(err));
                    setStatus('error');
                }
            });
        return () => { cancelled = true; };
    }, []);

    useEffect(() => {
        if (status !== 'ready' || !blocklyDiv.current) return;

        const Blockly = (window as any).Blockly;

        // ── Custom blocks ──────────────────────────────────────────────
        Blockly.Blocks['device_property'] = {
            init: function () {
                const deviceOptions = devices.length > 0
                    ? devices.map(d => [d.device_name, d.device_id])
                    : [['No devices', '']];
                this.appendDummyInput()
                    .appendField('Device')
                    .appendField(new Blockly.FieldDropdown(deviceOptions as any), 'DEVICE_ID')
                    .appendField('Property')
                    .appendField(new Blockly.FieldTextInput('temperature'), 'PROPERTY');
                this.setOutput(true, 'Number');
                this.setColour(65);
                this.setTooltip('Get a property value from a device');
            },
        };

        Blockly.Blocks['compare_condition'] = {
            init: function () {
                this.appendValueInput('A').setCheck(['Number', 'String']);
                this.appendDummyInput()
                    .appendField(new Blockly.FieldDropdown([
                        ['>', 'gt'], ['>=', 'gte'], ['<', 'lt'], ['<=', 'lte'],
                        ['==', 'eq'], ['!=', 'neq'], ['contains', 'contains'],
                    ]), 'OP');
                this.appendValueInput('B').setCheck(['Number', 'String']);
                this.setInputsInline(true);
                this.setOutput(true, 'Boolean');
                this.setColour(210);
            },
        };

        Blockly.Blocks['logic_operation_custom'] = {
            init: function () {
                this.appendValueInput('A').setCheck('Boolean');
                this.appendDummyInput()
                    .appendField(new Blockly.FieldDropdown([['AND', 'and'], ['OR', 'or']]), 'OP');
                this.appendValueInput('B').setCheck('Boolean');
                this.setInputsInline(true);
                this.setOutput(true, 'Boolean');
                this.setColour(210);
            },
        };

        if (!Blockly.Blocks['logic_negate']) {
            Blockly.Blocks['logic_negate'] = {
                init: function () {
                    this.appendValueInput('BOOL').setCheck('Boolean').appendField('NOT');
                    this.setOutput(true, 'Boolean');
                    this.setColour(210);
                },
            };
        }

        Blockly.Blocks['trigger_rule'] = {
            init: function () {
                this.appendValueInput('CONDITION').setCheck('Boolean').appendField('IF');
                this.appendStatementInput('ACTIONS').setCheck('Action').appendField('THEN');
                this.setColour(120);
                this.setTooltip('Rule: IF condition is true, THEN execute actions');
            },
        };

        Blockly.Blocks['action_log'] = {
            init: function () {
                this.appendDummyInput()
                    .appendField('Log Event')
                    .appendField(new Blockly.FieldTextInput('Rule fired!'), 'MESSAGE');
                this.setPreviousStatement(true, 'Action');
                this.setNextStatement(true, 'Action');
                this.setColour(330);
            },
        };

        Blockly.Blocks['action_notify'] = {
            init: function () {
                this.appendDummyInput()
                    .appendField('Send Notification')
                    .appendField(new Blockly.FieldTextInput('Alert'), 'TITLE')
                    .appendField(new Blockly.FieldTextInput('Something happened'), 'MESSAGE');
                this.setPreviousStatement(true, 'Action');
                this.setNextStatement(true, 'Action');
                this.setColour(330);
            },
        };

        Blockly.Blocks['action_webhook'] = {
            init: function () {
                this.appendDummyInput().appendField('Trigger Webhook');
                this.setPreviousStatement(true, 'Action');
                this.setNextStatement(true, 'Action');
                this.setColour(330);
            },
        };

        Blockly.Blocks['action_mqtt'] = {
            init: function () {
                this.appendDummyInput()
                    .appendField('MQTT Publish topic')
                    .appendField(new Blockly.FieldTextInput('alerts/rule'), 'TOPIC');
                this.setPreviousStatement(true, 'Action');
                this.setNextStatement(true, 'Action');
                this.setColour(330);
            },
        };

        const toolbox = {
            kind: 'categoryToolbox',
            contents: [
                {
                    kind: 'category', name: 'Logic', colour: '210',
                    contents: [
                        { kind: 'block', type: 'compare_condition' },
                        { kind: 'block', type: 'logic_operation_custom' },
                        { kind: 'block', type: 'logic_negate' },
                        { kind: 'block', type: 'logic_boolean' },
                    ],
                },
                {
                    kind: 'category', name: 'Math', colour: '230',
                    contents: [
                        { kind: 'block', type: 'math_number' },
                        { kind: 'block', type: 'math_arithmetic' },
                    ],
                },
                {
                    kind: 'category', name: 'Text', colour: '160',
                    contents: [
                        { kind: 'block', type: 'text' },
                    ],
                },
                {
                    kind: 'category', name: 'Devices', colour: '65',
                    contents: [
                        { kind: 'block', type: 'device_property' },
                    ],
                },
                {
                    kind: 'category', name: 'Actions', colour: '330',
                    contents: [
                        { kind: 'block', type: 'action_log' },
                        { kind: 'block', type: 'action_notify' },
                        { kind: 'block', type: 'action_webhook' },
                        { kind: 'block', type: 'action_mqtt' },
                    ],
                },
                {
                    kind: 'category', name: 'Rule', colour: '120',
                    contents: [
                        { kind: 'block', type: 'trigger_rule' },
                    ],
                },
            ],
        };

        if (workspaceRef.current) {
            try { workspaceRef.current.dispose(); } catch { /* ignore */ }
            workspaceRef.current = null;
        }

        const workspace = Blockly.inject(blocklyDiv.current, {
            toolbox,
            theme: Blockly.Themes?.Dark ?? undefined,
            grid: { spacing: 20, length: 3, colour: '#333', snap: true },
            zoom: { controls: true, wheel: true, startScale: 1.0, maxScale: 3, minScale: 0.3 },
            trashcan: true,
        });
        workspaceRef.current = workspace;

        // Force initial size calculation (the SVG sometimes starts at 0×0).
        requestAnimationFrame(() => {
            try { Blockly.svgResize(workspace); } catch { /* ignore */ }
        });

        if (initialJson && Object.keys(initialJson).length > 0) {
            try {
                Blockly.serialization.workspaces.load(initialJson, workspace);
            } catch (e) {
                console.error('Failed to load Blockly workspace state:', e);
            }
        }

        workspace.addChangeListener((event: any) => {
            if (event.isUiEvent) return;
            const state = Blockly.serialization.workspaces.save(workspace);
            const { conditions } = compileWorkspace(workspace);
            onChangeRef.current(state, conditions);
        });

        const ro = new ResizeObserver(() => {
            try { Blockly.svgResize(workspace); } catch { /* ignore */ }
        });
        ro.observe(blocklyDiv.current);

        const onWindowResize = () => {
            try { Blockly.svgResize(workspace); } catch { /* ignore */ }
        };
        window.addEventListener('resize', onWindowResize);

        return () => {
            window.removeEventListener('resize', onWindowResize);
            ro.disconnect();
            if (workspaceRef.current) {
                try { workspaceRef.current.dispose(); } catch { /* ignore */ }
                workspaceRef.current = null;
            }
        };
    }, [status, devices, initialJson]);

    const compileWorkspace = (workspace: any) => {
        const triggerBlocks = workspace.getBlocksByType('trigger_rule');
        if (triggerBlocks.length === 0) return { conditions: [], actions: [] };

        const triggerBlock = triggerBlocks[0];
        const conditions: any[] = [];
        const conditionBlock = triggerBlock.getInputTargetBlock('CONDITION');

        const walkCondition = (block: any): any => {
            if (!block) return null;
            if (block.type === 'compare_condition') {
                const aBlock = block.getInputTargetBlock('A');
                const bBlock = block.getInputTargetBlock('B');
                if (aBlock?.type === 'device_property') {
                    return {
                        field: aBlock.getFieldValue('PROPERTY'),
                        operator: block.getFieldValue('OP'),
                        value: bBlock?.type === 'math_number'
                            ? parseFloat(bBlock.getFieldValue('NUM'))
                            : bBlock?.getFieldValue('TEXT'),
                    };
                }
            } else if (block.type === 'logic_operation_custom') {
                const left = walkCondition(block.getInputTargetBlock('A'));
                const right = walkCondition(block.getInputTargetBlock('B'));
                if (left) conditions.push(left);
                if (right) conditions.push(right);
            }
            return null;
        };

        const rootCond = walkCondition(conditionBlock);
        if (rootCond) conditions.push(rootCond);
        return { conditions, actions: [] };
    };

    return (
        <div className="relative w-full h-[640px] border rounded-lg overflow-hidden bg-[#1e1e1e]">
            {/* Always render the inject target so it has stable dimensions. */}
            <div ref={blocklyDiv} className="absolute inset-0 w-full h-full" />
            {status !== 'ready' && (
                <div className="absolute inset-0 flex items-center justify-center bg-black/60 z-10 text-sm">
                    {status === 'loading' && (
                        <span className="text-muted-foreground animate-pulse">Loading Blockly engine…</span>
                    )}
                    {status === 'error' && (
                        <span className="text-red-400">Failed to load Blockly: {errorMsg}</span>
                    )}
                </div>
            )}
        </div>
    );
}

declare global {
    interface Window {
        Blockly: any;
    }
}
