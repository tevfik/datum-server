import { useEffect, useRef } from 'react';
import * as Blockly from 'blockly';
import { blocks } from 'blockly/blocks';
import * as En from 'blockly/msg/en';
import type { DeviceInfo } from '../hooks/useRules';

// Register standard blocks and locale once.
Blockly.setLocale(En as any);
Blockly.common.defineBlocks(blocks);

interface BlocklyEditorProps {
    initialJson?: Record<string, any>;
    devices: DeviceInfo[];
    onChange: (json: Record<string, any>, compiledConditions: any[]) => void;
}

export default function BlocklyEditor({ initialJson, devices, onChange }: BlocklyEditorProps) {
    const blocklyDiv = useRef<HTMLDivElement>(null);
    const workspaceRef = useRef<Blockly.WorkspaceSvg | null>(null);
    const onChangeRef = useRef(onChange);
    onChangeRef.current = onChange;

    useEffect(() => {
        if (!blocklyDiv.current) return;

        // ── Custom blocks ──────────────────────────────────────────────
        Blockly.Blocks['device_property'] = {
            init(this: Blockly.Block) {
                const deviceOptions: [string, string][] = devices.length > 0
                    ? devices.map(d => [d.device_name, d.device_id] as [string, string])
                    : [['No devices', '']];
                this.appendDummyInput()
                    .appendField('Device')
                    .appendField(new Blockly.FieldDropdown(deviceOptions), 'DEVICE_ID')
                    .appendField('Property')
                    .appendField(new Blockly.FieldTextInput('temperature'), 'PROPERTY');
                this.setOutput(true, 'Number');
                this.setColour(65);
                this.setTooltip('Get a property value from a device');
            },
        };

        Blockly.Blocks['compare_condition'] = {
            init(this: Blockly.Block) {
                this.appendValueInput('A').setCheck(['Number', 'String']);
                this.appendDummyInput()
                    .appendField(new Blockly.FieldDropdown([
                        ['>', 'gt'], ['>=', 'gte'], ['<', 'lt'], ['<=', 'lte'],
                        ['==', 'eq'], ['!=', 'neq'], ['contains', 'contains'],
                    ] as [string, string][]), 'OP');
                this.appendValueInput('B').setCheck(['Number', 'String']);
                this.setInputsInline(true);
                this.setOutput(true, 'Boolean');
                this.setColour(210);
                this.setTooltip('Compare two values');
            },
        };

        Blockly.Blocks['logic_op_custom'] = {
            init(this: Blockly.Block) {
                this.appendValueInput('A').setCheck('Boolean');
                this.appendDummyInput()
                    .appendField(new Blockly.FieldDropdown([['AND', 'and'], ['OR', 'or']] as [string, string][]), 'OP');
                this.appendValueInput('B').setCheck('Boolean');
                this.setInputsInline(true);
                this.setOutput(true, 'Boolean');
                this.setColour(210);
                this.setTooltip('Combine two conditions');
            },
        };

        Blockly.Blocks['logic_negate_custom'] = {
            init(this: Blockly.Block) {
                this.appendValueInput('BOOL').setCheck('Boolean').appendField('NOT');
                this.setOutput(true, 'Boolean');
                this.setColour(210);
            },
        };

        Blockly.Blocks['trigger_rule'] = {
            init(this: Blockly.Block) {
                this.appendValueInput('CONDITION').setCheck('Boolean').appendField('IF');
                this.appendStatementInput('ACTIONS').setCheck('Action').appendField('THEN');
                this.setColour(120);
                this.setTooltip('Rule: IF condition THEN execute actions');
            },
        };

        Blockly.Blocks['action_log'] = {
            init(this: Blockly.Block) {
                this.appendDummyInput()
                    .appendField('Log Event')
                    .appendField(new Blockly.FieldTextInput('Rule fired!'), 'MESSAGE');
                this.setPreviousStatement(true, 'Action');
                this.setNextStatement(true, 'Action');
                this.setColour(330);
            },
        };

        Blockly.Blocks['action_notify'] = {
            init(this: Blockly.Block) {
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
            init(this: Blockly.Block) {
                this.appendDummyInput().appendField('Trigger Webhook');
                this.setPreviousStatement(true, 'Action');
                this.setNextStatement(true, 'Action');
                this.setColour(330);
            },
        };

        Blockly.Blocks['action_mqtt'] = {
            init(this: Blockly.Block) {
                this.appendDummyInput()
                    .appendField('MQTT Publish topic')
                    .appendField(new Blockly.FieldTextInput('alerts/rule'), 'TOPIC');
                this.setPreviousStatement(true, 'Action');
                this.setNextStatement(true, 'Action');
                this.setColour(330);
            },
        };

        // ── Toolbox ────────────────────────────────────────────────────
        const toolbox: Blockly.utils.toolbox.ToolboxDefinition = {
            kind: 'categoryToolbox',
            contents: [
                {
                    kind: 'category', name: 'Rule', colour: '120',
                    contents: [{ kind: 'block', type: 'trigger_rule' }],
                },
                {
                    kind: 'category', name: 'Logic', colour: '210',
                    contents: [
                        { kind: 'block', type: 'compare_condition' },
                        { kind: 'block', type: 'logic_op_custom' },
                        { kind: 'block', type: 'logic_negate_custom' },
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
                    contents: [{ kind: 'block', type: 'text' }],
                },
                {
                    kind: 'category', name: 'Devices', colour: '65',
                    contents: [{ kind: 'block', type: 'device_property' }],
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
            ],
        };

        // Dispose any prior workspace.
        if (workspaceRef.current) {
            try { workspaceRef.current.dispose(); } catch { /* ignore */ }
            workspaceRef.current = null;
        }

        const workspace = Blockly.inject(blocklyDiv.current, {
            toolbox,
            theme: Blockly.Theme.defineTheme('darkDatum', {
                name: 'darkDatum',
                base: Blockly.Themes.Classic,
                componentStyles: { workspaceBackgroundColour: '#1e1e1e', toolboxBackgroundColour: '#2c2c2c', toolboxForegroundColour: '#ccc' },
            }),
            grid: { spacing: 20, length: 3, colour: '#333', snap: true },
            zoom: { controls: true, wheel: true, startScale: 1.0, maxScale: 3, minScale: 0.3 },
            trashcan: true,
        });
        workspaceRef.current = workspace;

        // Restore saved state.
        if (initialJson && Object.keys(initialJson).length > 0) {
            try {
                Blockly.serialization.workspaces.load(initialJson, workspace);
            } catch (e) {
                console.error('Failed to load Blockly workspace state:', e);
            }
        }

        // Emit compiled conditions on every meaningful change.
        workspace.addChangeListener((event: Blockly.Events.Abstract) => {
            if (event.isUiEvent) return;
            const state = Blockly.serialization.workspaces.save(workspace);
            const { conditions } = compileWorkspace(workspace);
            onChangeRef.current(state, conditions);
        });

        // Keep Blockly SVG sized to its container.
        const ro = new ResizeObserver(() => Blockly.svgResize(workspace));
        ro.observe(blocklyDiv.current);
        requestAnimationFrame(() => Blockly.svgResize(workspace));

        return () => {
            ro.disconnect();
            if (workspaceRef.current) {
                try { workspaceRef.current.dispose(); } catch { /* ignore */ }
                workspaceRef.current = null;
            }
        };
    // Re-run only when devices list identity changes; initialJson is applied once.
    // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [devices]);

    return (
        <div
            ref={blocklyDiv}
            className="w-full border rounded-lg overflow-hidden bg-[#1e1e1e]"
            style={{ height: '640px' }}
        />
    );
}

// ── Workspace → rule conditions compiler ────────────────────────────────────
function compileWorkspace(workspace: Blockly.WorkspaceSvg): { conditions: any[]; actions: any[] } {
    const triggerBlocks = workspace.getBlocksByType('trigger_rule');
    if (triggerBlocks.length === 0) return { conditions: [], actions: [] };

    const triggerBlock = triggerBlocks[0];
    const conditions: any[] = [];

    const walkCondition = (block: Blockly.Block | null): any => {
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
        } else if (block.type === 'logic_op_custom') {
            const left = walkCondition(block.getInputTargetBlock('A'));
            const right = walkCondition(block.getInputTargetBlock('B'));
            if (left) conditions.push(left);
            if (right) conditions.push(right);
        }
        return null;
    };

    const rootCond = walkCondition(triggerBlock.getInputTargetBlock('CONDITION'));
    if (rootCond) conditions.push(rootCond);
    return { conditions, actions: [] };
}
