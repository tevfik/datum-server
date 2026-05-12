# Blockly & Lua Rules Engine

Datum IoT Platform supports a highly flexible Visual Rules Engine that allows makers, researchers, and general users to define automation rules without extensive coding knowledge.

The rules engine supports three distinct logic modes:
1. **Simple Conditions**: Dropdown-based `IF x > y THEN z` rules.
2. **Visual Blocks (Blockly)**: Drag-and-drop block interface for complex logic trees.
3. **Lua Scripting**: Full scripting capability in an isolated, secure environment.

---

## 1. Trigger Types

Every rule must have a trigger that determines *when* it is evaluated.

*   **`on_data` (Data Arrival)**: The rule is evaluated automatically every time the associated device sends a new telemetry payload to the server. This is the most efficient and recommended trigger for realtime automations (e.g., "turn on fan when temperature exceeds 30°C").
*   **`scheduled` (Cron)**: The rule is evaluated on a fixed time schedule using standard Cron expressions. When triggered, the engine fetches the *most recent* data from the database. (e.g., "check the soil moisture every day at 08:00 AM" -> `0 8 * * *`).
*   **`manual`**: The rule is only evaluated when explicitly triggered via the `/api/v1/rules/:id/trigger` API endpoint or via the "Play" button in the web/mobile dashboard.

---

## 2. Visual Blocks (Blockly)

The Blockly editor provides an intuitive interface for building logic.

### Dynamic Device Properties
When you open the Blockly editor, the engine automatically fetches all devices you own and parses their **Thing Descriptions (TD)**. 
The "Devices" toolbox is dynamically populated with these properties. 

**Example Block:** `[ Device: Greenhouse Sensor ] [ Property: temperature ]`

### Logic Construction
*   Use the **Compare** block to evaluate device properties (e.g., `>`, `<`, `==`).
*   Combine multiple conditions using **AND / OR** blocks.
*   Enclose your condition in the **"If" Trigger Rule** block. The rule fires if the condition inside evaluates to `true`.

*(Note: The Web Dashboard and Mobile Flutter App both share the exact same Blockly layout and capabilities using a shared CDN/HTML asset).*

---

## 3. Lua Scripting

For advanced users, Datum provides a **Sandboxed Lua VM** (`gopher-lua`).

### Sandbox Security Rules
*   **Time Limit**: Every script execution is strictly limited to 100 milliseconds.
*   **Disabled Modules**: `os`, `io`, `debug`, `dofile`, `loadfile` are completely disabled to prevent server access.
*   **Read-Only Context**: You cannot modify the underlying data; scripts are strictly for evaluation.

### Writing Lua Rules
Your Lua script must **always** return a boolean value (`true` to fire the rule actions, `false` to ignore).

The engine injects a global `ctx` (context) variable containing the current evaluation state:
*   `ctx.device_id`: The ID of the device that triggered the evaluation.
*   `ctx.data`: A Lua table containing the telemetry payload.
*   `ctx.time`: The current UNIX timestamp.

**Example 1: Heat Index Calculation**
```lua
local temp = ctx.data.temperature
local hum = ctx.data.humidity

if temp == nil or hum == nil then
    return false
end

-- Simplified heat index calculation
local heat_index = temp + (0.5 * (hum - 40))

-- Fire the rule if it feels hotter than 35°C
return heat_index > 35
```

**Example 2: String Matching**
```lua
local status = ctx.data.status

if status ~= nil and string.find(status, "error") then
    return true
end

return false
```

---

## 4. Actions

When a rule's logic evaluates to `true`, all defined actions are executed asynchronously.

| Action Type | Description |
| :--- | :--- |
| **Log Event** (`log`) | Writes the event to the server's backend log. Excellent for debugging. |
| **Notification** (`notify`) | Emits a push notification event to the device owner via the WebHook dispatcher. |
| **MQTT Publish** (`mqtt`) | Publishes a raw payload to a specific MQTT topic. |
| **Device Command** (`command`) | Formats and sends a command JSON payload to the `dev/{id}/cmd/set` MQTT topic of a specific device. |
| **Webhook** (`webhook`) | Dispatches the trigger event to configured external webhooks. |

---

## 5. API Reference

Rule configurations are managed via REST. 

*   `GET /api/v1/rules/discovery`: Discover all owned devices and their property schemas.
*   `GET /api/v1/rules/blocks`: Fetch metadata for available Blockly blocks.
*   `POST /api/v1/rules/:id/trigger`: Manually execute a rule against the latest data.
