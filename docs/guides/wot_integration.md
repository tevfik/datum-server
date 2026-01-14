# Web of Things (WoT) Integration Guide

This guide details the Web of Things (WoT) implementation in the Datum IoT Platform, including adherence to W3C standards and supported semantic extensions for Rich UI generation.

## Overview

The Datum Platform uses the **W3C WoT Thing Description (TD)** standard to describe device capabilities. This allows any WoT-compliant client (like the Datum Mobile App or Web Dashboard) to automatically generate a control interface for the device without hardcoded logic.

## Standard Adherence

We adhere to the [W3C WoT Thing Description](https://www.w3.org/TR/wot-thing-description/) standard.
- **Properties:** Represent device state (e.g., `on`, `brightness`, `color`).
- **Actions:** Represent commands (e.g., `reboot`, `update_firmware`).
- **Events:** Represent asynchronous notifications (e.g., `motion_detected`).

## Semantic UI Extensions (`ui:widget`)

To enable rich user interfaces beyond simple text inputs, we use the `ui:widget` semantic extension in the Thing Description. This is a common pattern in the industry (e.g., Mozilla WebThings) to provide "hints" to the UI renderer.

**Behavior:**
- **Supported Clients:** Clients that understand `ui:widget` will render a rich component (e.g., Color Picker).
- **Standard Clients:** Clients that do *not* understand `ui:widget` will safely ignore it and render the default input based on the JSON `type` (e.g., a Text Input for a string `color`). This ensures **Full Backward Compatibility**.

### Supported Extensions

The following `ui:widget` values are supported by the Datum Web Dashboard and Mobile App:

| Widget Key | Applied To (Type) | Description | Rendered Component |
| :--- | :--- | :--- | :--- |
| `"switch"` | `boolean` | Toggles a state. | **Switch / Toggle Button** |
| `"color"` | `string` | selection of a color (Hex format). | **Color Picker** (Wheel/Palette) |
| `"slider"` | `number` / `integer` | Selection from a range. | **Slider** (requires `minimum` and `maximum` in TD) |
| `"select"` | `string` / `enum` | Selection from a list. | **Dropdown Menu** (automatically used if `enum` is present) |
| `"timeseries"`| `number` | Visualization of historical data. | **Sparkline / Line Chart** |
| `"gauge"` | `number` | Visualization of current level. | **Gauge / Dial** |

### Example Thing Description

```json
{
  "@context": "https://www.w3.org/2019/wot/td/v1",
  "title": "Smart Light",
  "properties": {
    "led_color": {
      "type": "string",
      "title": "LED Color",
      "ui:widget": "color",
      "readOnly": false
    },
    "brightness": {
      "type": "integer",
      "title": "Brightness",
      "minimum": 0,
      "maximum": 100,
      "ui:widget": "slider"
    }
  }
}
```
