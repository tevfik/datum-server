import { useEffect, useRef, useState } from 'react';
import type { DeviceInfo } from '../hooks/useRules';

interface BlocklyEditorProps {
    initialJson?: Record<string, any>;
    devices: DeviceInfo[];
    onChange: (json: Record<string, any>, compiledConditions: any[]) => void;
}

export default function BlocklyEditor({ initialJson, devices, onChange }: BlocklyEditorProps) {
    const blocklyDiv = useRef<HTMLDivElement>(null);
    const workspaceRef = useRef<any>(null);
    const [isLoaded, setIsLoaded] = useState(false);

    // Dynamically load Blockly from CDN
    useEffect(() => {
        if (window.Blockly) {
            setIsLoaded(true);
            return;
        }

        const script = document.createElement('script');
        script.src = '/blockly/blockly_compressed.js';
        script.onload = () => {
            const blocksScript = document.createElement('script');
            blocksScript.src = '/blockly/blocks_compressed.js';
            blocksScript.onload = () => {
                const msgScript = document.createElement('script');
                msgScript.src = '/blockly/msg/en.js';
                msgScript.onload = () => {
                    setIsLoaded(true);
                };
                document.body.appendChild(msgScript);
            };
            document.body.appendChild(blocksScript);
        };
        document.body.appendChild(script);

        return () => {
            // Cleanup logic if component unmounts before loading
        };
    }, []);

    useEffect(() => {
        if (!isLoaded || !blocklyDiv.current) return;

        const Blockly = window.Blockly;

        // 1. Define custom blocks
        Blockly.Blocks['device_property'] = {
            init: function() {
                const deviceOptions = devices.length > 0
                    ? devices.map(d => [d.device_name, d.device_id])
                    : [['No devices', '']];

                this.appendDummyInput()
                    .appendField("Device")
                    .appendField(new Blockly.FieldDropdown(deviceOptions as any, function(this: any, newValue: string) {
                        // When device changes, update the property dropdown if possible
                        // Note: In standard Blockly this is tricky, we'll use a text field with 
                        // a dropdown helper or just a text field for now to avoid complexity
                        return newValue;
                    }), "DEVICE_ID")
                    .appendField("Property")
                    .appendField(new Blockly.FieldTextInput("temperature"), "PROPERTY");
                this.setOutput(true, "Number");
                this.setColour(65);
                this.setTooltip("Get a property from a device");
            }
        };

        Blockly.Blocks['compare_condition'] = {
            init: function() {
                this.appendValueInput("A").setCheck(["Number", "String"]);
                this.appendDummyInput()
                    .appendField(new Blockly.FieldDropdown([
                        [">", "gt"], [">=", "gte"], ["<", "lt"], ["<=", "lte"], 
                        ["==", "eq"], ["!=", "neq"], ["contains", "contains"]
                    ]), "OP");
                this.appendValueInput("B").setCheck(["Number", "String"]);
                this.setInputsInline(true);
                this.setOutput(true, "Boolean");
                this.setColour(210);
            }
        };

        Blockly.Blocks['logic_operation'] = {
            init: function() {
                this.appendValueInput("A").setCheck("Boolean");
                this.appendDummyInput()
                    .appendField(new Blockly.FieldDropdown([["AND", "and"], ["OR", "or"]]), "OP");
                this.appendValueInput("B").setCheck("Boolean");
                this.setInputsInline(true);
                this.setOutput(true, "Boolean");
                this.setColour(210);
            }
        };

        Blockly.Blocks['logic_negate'] = {
            init: function() {
                this.appendValueInput("BOOL").setCheck("Boolean").appendField("NOT");
                this.setOutput(true, "Boolean");
                this.setColour(210);
            }
        };

        Blockly.Blocks['trigger_rule'] = {
            init: function() {
                this.appendValueInput("CONDITION")
                    .setCheck("Boolean")
                    .appendField("IF");
                this.appendStatementInput("ACTIONS")
                    .setCheck("Action")
                    .appendField("THEN");
                this.setColour(120);
                this.setTooltip("Rule definition: IF condition is true, THEN execute actions");
            }
        };

        Blockly.Blocks['action_log'] = {
            init: function() {
                this.appendDummyInput()
                    .appendField("Log Event")
                    .appendField(new Blockly.FieldTextInput("Rule fired!"), "MESSAGE");
                this.setPreviousStatement(true, "Action");
                this.setNextStatement(true, "Action");
                this.setColour(330);
            }
        };

        Blockly.Blocks['action_notify'] = {
            init: function() {
                this.appendDummyInput()
                    .appendField("Send Notification")
                    .appendField(new Blockly.FieldTextInput("Alert"), "TITLE")
                    .appendField(new Blockly.FieldTextInput("Something happened"), "MESSAGE");
                this.setPreviousStatement(true, "Action");
                this.setNextStatement(true, "Action");
                this.setColour(330);
            }
        };

        // 2. Define Toolbox
        const toolbox = {
            "kind": "categoryToolbox",
            "contents": [
                {
                    "kind": "category", "name": "Logic", "colour": "210",
                    "contents": [
                        { "kind": "block", "type": "compare_condition" },
                        { "kind": "block", "type": "logic_operation" },
                        { "kind": "block", "type": "logic_negate" },
                        { "kind": "block", "type": "logic_boolean" }
                    ]
                },
                {
                    "kind": "category", "name": "Math", "colour": "230",
                    "contents": [
                        { "kind": "block", "type": "math_number" },
                        { "kind": "block", "type": "math_arithmetic" }
                    ]
                },
                {
                    "kind": "category", "name": "Text", "colour": "160",
                    "contents": [
                        { "kind": "block", "type": "text" }
                    ]
                },
                {
                    "kind": "category", "name": "Devices", "colour": "65",
                    "contents": [
                        { "kind": "block", "type": "device_property" }
                    ]
                },
                {
                    "kind": "category", "name": "Actions", "colour": "330",
                    "contents": [
                        { "kind": "block", "type": "action_log" },
                        { "kind": "block", "type": "action_notify" }
                    ]
                },
                {
                    "kind": "category", "name": "Rule", "colour": "120",
                    "contents": [
                        { "kind": "block", "type": "trigger_rule" }
                    ]
                }
            ]
        };

        // 3. Inject workspace
        if (workspaceRef.current) {
            workspaceRef.current.dispose();
        }

        const workspace = Blockly.inject(blocklyDiv.current, {
            toolbox: toolbox,
            theme: Blockly.Themes.Dark,
            grid: { spacing: 20, length: 3, colour: '#333', snap: true },
            zoom: { controls: true, wheel: true, startScale: 1.0 }
        });
        workspaceRef.current = workspace;

        // 4. Load initial state
        if (initialJson && Object.keys(initialJson).length > 0) {
            try {
                Blockly.serialization.workspaces.load(initialJson, workspace);
            } catch (e) {
                console.error("Failed to load Blockly state:", e);
            }
        }

        // 5. Setup change listener
        workspace.addChangeListener((event: any) => {
            if (event.type === Blockly.Events.UI || event.type === Blockly.Events.THEME_CHANGE) return;

            const state = Blockly.serialization.workspaces.save(workspace);
            
            // Compile to engine-compatible conditions and actions
            const { conditions, actions } = compileWorkspace(workspace);
            onChange(state, conditions);
        });

        // Handle resize
        const onResize = () => Blockly.svgResize(workspace);
        window.addEventListener('resize', onResize);

        return () => {
            window.removeEventListener('resize', onResize);
            if (workspaceRef.current) {
                workspaceRef.current.dispose();
                workspaceRef.current = null;
            }
        };
    }, [isLoaded, devices]);

    // Enhanced compiler
    const compileWorkspace = (workspace: any) => {
        const triggerBlocks = workspace.getBlocksByType('trigger_rule');
        if (triggerBlocks.length === 0) return { conditions: [], actions: [] };
        
        const triggerBlock = triggerBlocks[0];
        
        // 1. Compile Conditions
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
                        value: bBlock?.type === 'math_number' ? parseFloat(bBlock.getFieldValue('NUM')) : bBlock?.getFieldValue('TEXT')
                    };
                }
            } else if (block.type === 'logic_operation') {
                // Flattening AND/OR is not fully supported by simple conditions array
                // but we can try to return the first level
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
        <div className="flex flex-col h-[500px] border rounded-lg overflow-hidden bg-[#1e1e1e]">
            {!isLoaded ? (
                <div className="flex-1 flex items-center justify-center text-muted-foreground">
                    Loading Blockly engine...
                </div>
            ) : (
                <div ref={blocklyDiv} className="flex-1 w-full" />
            )}
        </div>
    );
}

// Add global TypeScript definition
declare global {
    interface Window {
        Blockly: any;
    }
}
