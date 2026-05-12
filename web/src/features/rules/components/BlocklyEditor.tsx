import React, { useEffect, useRef, useState } from 'react';
import { DeviceInfo } from '../hooks/useRules';

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
                // Dynamically build device dropdown
                const deviceOptions = devices.length > 0
                    ? devices.map(d => [d.device_name, d.device_id])
                    : [['No devices', '']];

                this.appendDummyInput()
                    .appendField("Device")
                    .appendField(new Blockly.FieldDropdown(deviceOptions as any), "DEVICE_ID")
                    .appendField("Property")
                    .appendField(new Blockly.FieldTextInput("temp"), "PROPERTY");
                this.setOutput(true, "Number");
                this.setColour(65);
                this.setTooltip("Get a property from a device");
                this.setHelpUrl("");
            }
        };

        Blockly.Blocks['compare_condition'] = {
            init: function() {
                this.appendValueInput("A")
                    .setCheck("Number");
                this.appendDummyInput()
                    .appendField(new Blockly.FieldDropdown([
                        [">", "gt"],
                        [">=", "gte"],
                        ["<", "lt"],
                        ["<=", "lte"],
                        ["==", "eq"],
                        ["!=", "neq"]
                    ]), "OP");
                this.appendValueInput("B")
                    .setCheck("Number");
                this.setInputsInline(true);
                this.setOutput(true, "Boolean");
                this.setColour(210);
                this.setTooltip("Compare two values");
                this.setHelpUrl("");
            }
        };

        Blockly.Blocks['trigger_rule'] = {
            init: function() {
                this.appendValueInput("CONDITION")
                    .setCheck("Boolean")
                    .appendField("If");
                this.setColour(120);
                this.setTooltip("Rule triggers when condition is true");
                this.setHelpUrl("");
            }
        };

        // 2. Define Toolbox
        const toolbox = {
            "kind": "categoryToolbox",
            "contents": [
                {
                    "kind": "category",
                    "name": "Logic",
                    "colour": "210",
                    "contents": [
                        { "kind": "block", "type": "compare_condition" },
                        { "kind": "block", "type": "logic_boolean" }
                    ]
                },
                {
                    "kind": "category",
                    "name": "Math",
                    "colour": "230",
                    "contents": [
                        { "kind": "block", "type": "math_number" },
                        { "kind": "block", "type": "math_arithmetic" }
                    ]
                },
                {
                    "kind": "category",
                    "name": "Devices",
                    "colour": "65",
                    "contents": [
                        { "kind": "block", "type": "device_property" }
                    ]
                },
                {
                    "kind": "category",
                    "name": "Rule",
                    "colour": "120",
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
            zoom: {
                controls: true,
                wheel: true,
                startScale: 1.0,
                maxScale: 3,
                minScale: 0.3,
                scaleSpeed: 1.2
            }
        });
        workspaceRef.current = workspace;

        // 4. Load initial state
        if (initialJson) {
            Blockly.serialization.workspaces.load(initialJson, workspace);
        }

        // 5. Setup change listener
        workspace.addChangeListener((event: any) => {
            // Skip UI events
            if (event.type === Blockly.Events.UI || event.type === Blockly.Events.THEME_CHANGE) {
                return;
            }

            const state = Blockly.serialization.workspaces.save(workspace);
            
            // Dummy compilation of conditions for the backend
            // In a real implementation, we would write a generator that walks the blocks
            // and builds the Conditions array.
            const compiledConditions = compileToConditions(workspace);
            
            onChange(state, compiledConditions);
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

    // Simple compiler to convert Blockly logic to datum-server JSON conditions
    const compileToConditions = (workspace: any) => {
        // Find the trigger rule block
        const triggerBlocks = workspace.getBlocksByType('trigger_rule');
        if (triggerBlocks.length === 0) return [];
        
        const triggerBlock = triggerBlocks[0];
        const conditionBlock = triggerBlock.getInputTargetBlock('CONDITION');
        
        if (!conditionBlock || conditionBlock.type !== 'compare_condition') return [];
        
        const op = conditionBlock.getFieldValue('OP');
        const aBlock = conditionBlock.getInputTargetBlock('A');
        const bBlock = conditionBlock.getInputTargetBlock('B');
        
        if (!aBlock || !bBlock) return [];
        
        if (aBlock.type === 'device_property' && bBlock.type === 'math_number') {
            const property = aBlock.getFieldValue('PROPERTY');
            const value = parseFloat(bBlock.getFieldValue('NUM'));
            
            return [{
                field: property,
                operator: op,
                value: value
            }];
        }
        
        return [];
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
