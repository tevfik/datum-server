import { useEffect, useRef } from 'react';
import * as Blockly from 'blockly';
import { blocks } from 'blockly/blocks';
import * as En from 'blockly/msg/en';
import type { DeviceInfo } from '../hooks/useRules';

// Register standard blocks and locale once at module level.
Blockly.setLocale(En as any);
Blockly.common.defineBlocks(blocks);

// Module-level device map so dynamic dropdown generators can access it
// without captures (Blockly re-calls generators after workspace init).
let _devices: DeviceInfo[] = [];

const FALLBACK_PROPS: [string, string][] = [['temperature', 'temperature']];
const FALLBACK_DEVICES: [string, string][] = [['No devices', '__none__']];

/** Returns property options for a given device id from the global device cache. */
function getPropertyOptions(deviceId: string): [string, string][] {
    const dev = _devices.find(d => d.device_id === deviceId);
    if (!dev || dev.properties.length === 0) return FALLBACK_PROPS;
    return dev.properties.map(p => {
        const label = p.unit ? `${p.title} (${p.unit})` : p.title;
        return [label, p.key] as [string, string];
    });
}

function getDeviceOptions(): [string, string][] {
    if (_devices.length === 0) return FALLBACK_DEVICES;
    return _devices.map(d => [d.device_name, d.device_id] as [string, string]);
}

function defineCustomBlocks() {
    // ── device_property ────────────────────────────────────────────────
    // Device dropdown → property dropdown auto-populated from WoT.
    Blockly.Blocks['device_property'] = {
        init(this: Blockly.Block) {
            const devField = new Blockly.FieldDropdown(
                () => getDeviceOptions(),
                (newDeviceId: string) => {
                    // Rebuild PROPERTY dropdown when device changes.
                    const propField = this.getField('PROPERTY') as Blockly.FieldDropdown | null;
                    if (propField) {
                        // Force options update on next render cycle.
                        (propField as any).menuGenerator_ = () => getPropertyOptions(newDeviceId);
                        // Reset to first valid option.
                        const opts = getPropertyOptions(newDeviceId);
                        propField.setValue(opts[0][1]);
                    }
                    return newDeviceId;
                }
            );

            const firstDeviceId = _devices.length > 0 ? _devices[0].device_id : '__none__';

            const propField = new Blockly.FieldDropdown(
                () => getPropertyOptions(devField.getValue() || firstDeviceId)
            );

            this.appendDummyInput()
                .appendField('Device')
                .appendField(devField, 'DEVICE_ID')
                .appendField('  Property')
                .appendField(propField, 'PROPERTY');

            this.setOutput(true, 'Number');
            this.setColour(65);
            this.setTooltip('Get a property value from a device (properties loaded from WoT Thing Description)');
        },
    };

    // ── compare_condition ──────────────────────────────────────────────
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

    // ── logic_op_custom ────────────────────────────────────────────────
    Blockly.Blocks['logic_op_custom'] = {
        init(this: Blockly.Block) {
            this.appendValueInput('A').setCheck('Boolean');
            this.appendDummyInput()
                .appendField(new Blockly.FieldDropdown([
                    ['AND', 'and'], ['OR', 'or'],
                ] as [string, string][]), 'OP');
            this.appendValueInput('B').setCheck('Boolean');
            this.setInputsInline(true);
            this.setOutput(true, 'Boolean');
            this.setColour(210);
            this.setTooltip('Combine two conditions with AND / OR');
        },
    };

    // ── logic_negate_custom ────────────────────────────────────────────
    Blockly.Blocks['logic_negate_custom'] = {
        init(this: Blockly.Block) {
            this.appendValueInput('BOOL').setCheck('Boolean').appendField('NOT');
            this.setOutput(true, 'Boolean');
            this.setColour(210);
        },
    };

    // ── trigger_rule ───────────────────────────────────────────────────
    Blockly.Blocks['trigger_rule'] = {
        init(this: Blockly.Block) {
            this.appendValueInput('CONDITION').setCheck('Boolean').appendField('IF');
            this.appendStatementInput('ACTIONS').setCheck('Action').appendField('THEN');
            this.setColour(120);
            this.setTooltip('Rule: IF condition THEN execute actions');
        },
    };

    // ── action_log ──────────────────────────────────────────────────────
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

    // ── action_notify ──────────────────────────────────────────────────
    Blockly.Blocks['action_notify'] = {
        init(this: Blockly.Block) {
            this.appendDummyInput()
                .appendField('Notify')
                .appendField(new Blockly.FieldTextInput('Alert'), 'TITLE')
                .appendField(new Blockly.FieldTextInput('Something happened'), 'MESSAGE');
            this.setPreviousStatement(true, 'Action');
            this.setNextStatement(true, 'Action');
            this.setColour(330);
        },
    };

    // ── action_webhook ─────────────────────────────────────────────────
    Blockly.Blocks['action_webhook'] = {
        init(this: Blockly.Block) {
            this.appendDummyInput().appendField('Trigger Webhook');
            this.setPreviousStatement(true, 'Action');
            this.setNextStatement(true, 'Action');
            this.setColour(330);
        },
    };

    // ── action_mqtt ────────────────────────────────────────────────────
    // Device dropdown + fixed prefix "dev/<device_id>/cmd/" + editable suffix.
    // This ensures MQTT publishes stay within the user's own device command topics.
    Blockly.Blocks['action_mqtt'] = {
        init(this: Blockly.Block) {
            const devDropdown = new Blockly.FieldDropdown(
                () => getDeviceOptions()
            );
            this.appendDummyInput()
                .appendField('MQTT → device')
                .appendField(devDropdown, 'DEVICE_ID')
                .appendField('cmd/')
                .appendField(new Blockly.FieldTextInput('set'), 'CMD');
            this.setTooltip(
                'Publishes to dev/<device_id>/cmd/<cmd>. ' +
                'Topic is scoped to your device command channel.'
            );
            this.setPreviousStatement(true, 'Action');
            this.setNextStatement(true, 'Action');
            this.setColour(330);
        },
    };
}

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

        // Update global device cache so dynamic dropdowns see the latest devices.
        _devices = devices;
        defineCustomBlocks();

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

        if (workspaceRef.current) {
            try { workspaceRef.current.dispose(); } catch { /* ignore */ }
            workspaceRef.current = null;
        }

        const workspace = Blockly.inject(blocklyDiv.current, {
            toolbox,
            theme: Blockly.Theme.defineTheme('darkDatum', {
                name: 'darkDatum',
                base: Blockly.Themes.Classic,
                componentStyles: {
                    workspaceBackgroundColour: '#1e1e1e',
                    toolboxBackgroundColour: '#2c2c2c',
                    toolboxForegroundColour: '#cccccc',
                    flyoutBackgroundColour: '#252525',
                    flyoutForegroundColour: '#ccc',
                    scrollbarColour: '#555',
                },
            }),
            grid: { spacing: 20, length: 3, colour: '#333', snap: true },
            zoom: { controls: true, wheel: true, startScale: 1.0, maxScale: 3, minScale: 0.3 },
            trashcan: true,
            scrollbars: true,
            move: { scrollbars: true, drag: true, wheel: true },
        });
        workspaceRef.current = workspace;

        if (initialJson && Object.keys(initialJson).length > 0) {
            try {
                Blockly.serialization.workspaces.load(initialJson, workspace);
            } catch (e) {
                console.error('Failed to load Blockly workspace state:', e);
            }
        }

        workspace.addChangeListener((event: Blockly.Events.Abstract) => {
            if (event.isUiEvent) return;
            const state = Blockly.serialization.workspaces.save(workspace);
            const { conditions } = compileWorkspace(workspace);
            onChangeRef.current(state, conditions);
        });

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
    // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [devices]);

    return (
        <div
            ref={blocklyDiv}
            className="w-full border rounded-lg overflow-hidden bg-[#1e1e1e]"
            style={{ height: '70vh', minHeight: '560px', maxHeight: '900px' }}
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
                        : (bBlock?.getFieldValue('TEXT') ?? ''),
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
