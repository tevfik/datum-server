# Datum IoT Platform - Step-by-Step Test Scenarios

This document provides a comprehensive guide to testing the Datum IoT Platform using the `datumctl` CLI tool. These scenarios cover the entire lifecycle of an IoT application, from system setup to device management and data streaming.

## Prerequisites

- **Datum Server** running (default: `http://localhost:8000`)
- **datumctl** CLI tool installed and available in your path

## Scenario 1: System Initialization

**Goal**: Initialize a fresh Datum Server instance with an admin account.

1.  **Check Server Status**
    Ensure the server is running and ready for initialization.
    ```bash
    curl http://localhost:8000/health
    ```

2.  **Run Setup Wizard**
    Initialize the platform with your admin credentials.
    ```bash
    datumctl setup --platform "Test IoT Platform" --email "admin@example.com" --password "securepassword123" --yes
    ```
    *Expected Output*: `✅ System initialized successfully!`

3.  **Verify Initialization**
    Check if the system status now reports as initialized.
    ```bash
    curl http://localhost:8000/system/status
    ```

## Scenario 2: Authentication

**Goal**: Authenticate the CLI with the server to perform administrative tasks.

1.  **Login as Admin**
    ```bash
    datumctl login --email "admin@example.com" --password "securepassword123"
    ```
    *Expected Output*: `✅ Login successful!`

2.  **Verify Session**
    Check if the CLI is authenticated by listing devices (should be empty but successful).
    ```bash
    datumctl device list
    ```

## Scenario 3: Device Management Lifecycle

**Goal**: Create, manage, and delete IoT devices.

1.  **Create a New Device**
    Create a temperature sensor. **Important**: Save the API Key returned!
    ```bash
    datumctl device create --name "Living Room Sensor" --type "temperature-sensor" --id "sensor-001"
    ```
    *Expected Output*: JSON output containing `api_key`.

2.  **List Devices**
    Verify the device appears in the registry.
    ```bash
    datumctl device list
    ```

3.  **Get Device Details**
    Retrieve detailed metadata for the specific device.
    ```bash
    datumctl device get sensor-001
    ```

## Scenario 4: Data Ingestion & Retrieval

**Goal**: Simulate a device sending data and verify it can be queried.

1.  **Simulate Data Ingestion (HTTP)**
    Use `curl` or `datumctl` to send telemetry data using the Device API Key obtained in Scenario 3.
    ```bash
    # Replace YOUR_DEVICE_API_KEY with the actual key
    datumctl data post --device sensor-001 --api-key YOUR_DEVICE_API_KEY --data '{"temperature": 22.5, "humidity": 45}'
    ```

2.  **Ingest More Data**
    Send a few more data points to create a time series.
    ```bash
    datumctl data post --device sensor-001 --api-key YOUR_DEVICE_API_KEY --data '{"temperature": 23.0, "humidity": 46}'
    datumctl data post --device sensor-001 --api-key YOUR_DEVICE_API_KEY --data '{"temperature": 23.5, "humidity": 44}'
    ```

3.  **Query Recent Data**
    Retrieve the data ingested in the last hour.
    ```bash
    datumctl data get --device sensor-001 --last 1h
    ```

4.  **Get Data Statistics**
    View aggregated stats for the device.
    ```bash
    datumctl data stats --device sensor-001
    ```

## Scenario 5: Command & Control

**Goal**: Send commands to a device and verify delivery.

1.  **Send a Command**
    Queue a command for the device to execute (e.g., reboot or update config).
    ```bash
    datumctl command send sensor-001 reboot
    ```

2.  **Send Command with Parameters**
    ```bash
    datumctl command send sensor-001 update-config --param interval=60
    ```

3.  **List Pending Commands**
    Check the command queue for the device.
    ```bash
    datumctl command list sensor-001
    ```

4.  **Verify Command Details**
    Get the status of a specific command (use the Command ID from the list output).
    ```bash
    datumctl command get sensor-001 <COMMAND_ID>
    ```

## Scenario 6: Mobile Provisioning Workflow

**Goal**: Simulate the mobile app provisioning flow where a device is registered for WiFi setup.

1.  **Register Device for Provisioning**
    Simulate a mobile app registering a new device found via Bluetooth/WiFi.
    ```bash
    datumctl provision register \
      --uid "AA:BB:CC:DD:EE:FF" \
      --name "Smart Bulb" \
      --type "light" \
      --wifi-ssid "MyHomeWiFi" \
      --wifi-pass "secret123"
    ```
    *Expected Output*: JSON containing `provisioning_id` and `secret`.

2.  **Verify Provisioning Status**
    (Optional) Check if the device appears in the list (it might be in a 'provisioning' state if supported, or just created).
    ```bash
    datumctl device list
    ```

## Scenario 7: Cleanup

**Goal**: Remove test resources to leave the system clean.

1.  **Delete Device**
    Remove the test device.
    ```bash
    datumctl device delete sensor-001 --force
    ```

2.  **Verify Deletion**
    Ensure the device is gone.
    ```bash
    datumctl device list
    ```

---

## Troubleshooting

- **Connection Refused**: Ensure the server is running on port 8000.
- **Unauthorized**: Check if your token has expired; run `datumctl login` again.
- **Device Not Found**: Verify the Device ID used in commands matches the created one.
