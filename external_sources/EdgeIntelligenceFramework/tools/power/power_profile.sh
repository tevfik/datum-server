#!/bin/bash
#
# Power Profiling Tool for Edge Intelligence Framework
# Analyzes power consumption and estimates battery life for embedded platforms
#
# Usage: ./power_profile.sh [platform] [model]
#   platform: esp32, esp32s3, stm32f4, stm32h7, rp2040, arduino
#   model: path to .eif model file (optional)
#

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m'

# Platform specifications (typical values)
declare -A PLATFORM_ACTIVE_MA=(
    ["esp32"]=160
    ["esp32s3"]=180
    ["stm32f4"]=120
    ["stm32h7"]=140
    ["rp2040"]=50
    ["arduino"]=80
)

declare -A PLATFORM_SLEEP_MA=(
    ["esp32"]=0.01
    ["esp32s3"]=0.01
    ["stm32f4"]=0.002
    ["stm32h7"]=0.003
    ["rp2040"]=0.18
    ["arduino"]=0.005
)

declare -A PLATFORM_CPU_MHZ=(
    ["esp32"]=240
    ["esp32s3"]=240
    ["stm32f4"]=168
    ["stm32h7"]=480
    ["rp2040"]=133
    ["arduino"]=48
)

declare -A PLATFORM_VOLTAGE=(
    ["esp32"]=3.3
    ["esp32s3"]=3.3
    ["stm32f4"]=3.3
    ["stm32h7"]=3.3
    ["rp2040"]=3.3
    ["arduino"]=3.3
)

#
# Function: Calculate inference power consumption
#
calculate_inference_power() {
    local PLATFORM=$1
    local INFERENCE_MS=$2
    local DUTY_CYCLE=$3  # Percentage of time active (0-100)
    
    local ACTIVE_MA=${PLATFORM_ACTIVE_MA[$PLATFORM]}
    local SLEEP_MA=${PLATFORM_SLEEP_MA[$PLATFORM]}
    local VOLTAGE=${PLATFORM_VOLTAGE[$PLATFORM]}
    
    # Average current = (active_ma * duty_cycle + sleep_ma * (1 - duty_cycle))
    local DUTY_FRACTION=$(echo "scale=4; $DUTY_CYCLE / 100" | bc)
    local AVG_MA=$(echo "scale=4; ($ACTIVE_MA * $DUTY_FRACTION) + ($SLEEP_MA * (1 - $DUTY_FRACTION))" | bc)
    
    # Power in mW
    local AVG_MW=$(echo "scale=2; $AVG_MA * $VOLTAGE" | bc)
    
    echo "$AVG_MA|$AVG_MW"
}

#
# Function: Estimate battery life
#
estimate_battery_life() {
    local AVG_MA=$1
    local BATTERY_MAH=$2
    
    # Battery life in hours
    local HOURS=$(echo "scale=1; $BATTERY_MAH / $AVG_MA" | bc)
    local DAYS=$(echo "scale=1; $HOURS / 24" | bc)
    local MONTHS=$(echo "scale=1; $DAYS / 30" | bc)
    
    echo "$HOURS|$DAYS|$MONTHS"
}

#
# Function: Analyze model power characteristics
#
analyze_model() {
    local MODEL_FILE=$1
    local PLATFORM=$2
    
    if [ ! -f "$MODEL_FILE" ]; then
        echo -e "${RED}Model file not found: $MODEL_FILE${NC}"
        return 1
    fi
    
    local MODEL_SIZE=$(stat -f%z "$MODEL_FILE" 2>/dev/null || stat -c%s "$MODEL_FILE" 2>/dev/null)
    local MODEL_SIZE_KB=$((MODEL_SIZE / 1024))
    
    # Estimate inference time based on model size (very rough)
    # Typical: 1KB model ≈ 0.5ms on ESP32-S3, scale by CPU MHz
    local BASE_MS=$(echo "scale=2; $MODEL_SIZE_KB * 0.5" | bc)
    local CPU_MHZ=${PLATFORM_CPU_MHZ[$PLATFORM]}
    local INFERENCE_MS=$(echo "scale=2; $BASE_MS * (240 / $CPU_MHZ)" | bc)
    
    echo "$INFERENCE_MS"
}

#
# Main analysis
#
PLATFORM=${1:-esp32s3}
MODEL_FILE=${2}

# Validate platform
if [ -z "${PLATFORM_ACTIVE_MA[$PLATFORM]}" ]; then
    echo -e "${RED}Unknown platform: $PLATFORM${NC}"
    echo "Supported platforms: ${!PLATFORM_ACTIVE_MA[@]}"
    exit 1
fi

echo ""
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${CYAN}⚡ Power Profiling Report - ${PLATFORM^^}${NC}"
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

# Platform specs
echo -e "${BLUE}📋 Platform Specifications:${NC}"
echo "  CPU Frequency:     ${PLATFORM_CPU_MHZ[$PLATFORM]} MHz"
echo "  Operating Voltage: ${PLATFORM_VOLTAGE[$PLATFORM]} V"
echo "  Active Current:    ${PLATFORM_ACTIVE_MA[$PLATFORM]} mA"
echo "  Sleep Current:     ${PLATFORM_SLEEP_MA[$PLATFORM]} mA"
echo ""

# Model analysis
if [ -n "$MODEL_FILE" ]; then
    echo -e "${BLUE}🤖 Model Analysis:${NC}"
    INFERENCE_MS=$(analyze_model "$MODEL_FILE" "$PLATFORM")
    MODEL_SIZE=$(stat -f%z "$MODEL_FILE" 2>/dev/null || stat -c%s "$MODEL_FILE" 2>/dev/null)
    MODEL_SIZE_KB=$((MODEL_SIZE / 1024))
    
    echo "  Model File:        $(basename $MODEL_FILE)"
    echo "  Model Size:        ${MODEL_SIZE_KB} KB"
    echo "  Est. Inference:    ${INFERENCE_MS} ms"
    echo ""
else
    # Use default inference time
    INFERENCE_MS=10
    echo -e "${YELLOW}ℹ️  No model specified, using default inference time: ${INFERENCE_MS}ms${NC}"
    echo ""
fi

# Power consumption scenarios
echo -e "${CYAN}⚡ Power Consumption Analysis:${NC}"
echo ""

printf "%-25s %12s %12s %15s %15s\n" "Scenario" "Duty Cycle" "Avg Current" "Avg Power" "Battery Life*"
echo "  $(printf '%.0s─' {1..80})"

# Common battery capacities
BATTERY_2000=2000   # 2000mAh - typical Li-ion 18650
BATTERY_1000=1000   # 1000mAh - typical small Li-Po
BATTERY_250=250     # 250mAh - coin cell

# Scenario 1: Continuous inference (100% duty cycle)
IFS='|' read -r AVG_MA AVG_MW <<< "$(calculate_inference_power "$PLATFORM" "$INFERENCE_MS" 100)"
IFS='|' read -r HOURS DAYS MONTHS <<< "$(estimate_battery_life "$AVG_MA" "$BATTERY_2000")"
printf "%-25s %11s%% %10.2f mA %12.2f mW %13.1f hrs\n" \
    "Continuous" "100" "$AVG_MA" "$AVG_MW" "$HOURS"

# Scenario 2: 10Hz inference (1% duty cycle if 10ms inference)
DUTY_10HZ=$(echo "scale=2; ($INFERENCE_MS * 10) / 1000 * 100" | bc)
IFS='|' read -r AVG_MA AVG_MW <<< "$(calculate_inference_power "$PLATFORM" "$INFERENCE_MS" "$DUTY_10HZ")"
IFS='|' read -r HOURS DAYS MONTHS <<< "$(estimate_battery_life "$AVG_MA" "$BATTERY_2000")"
printf "%-25s %11.2f%% %10.2f mA %12.2f mW %13.1f hrs\n" \
    "10 Hz sampling" "$DUTY_10HZ" "$AVG_MA" "$AVG_MW" "$HOURS"

# Scenario 3: 1Hz inference
DUTY_1HZ=$(echo "scale=2; ($INFERENCE_MS * 1) / 1000 * 100" | bc)
IFS='|' read -r AVG_MA AVG_MW <<< "$(calculate_inference_power "$PLATFORM" "$INFERENCE_MS" "$DUTY_1HZ")"
IFS='|' read -r HOURS DAYS MONTHS <<< "$(estimate_battery_life "$AVG_MA" "$BATTERY_2000")"
printf "%-25s %11.2f%% %10.2f mA %12.2f mW %11.0f days\n" \
    "1 Hz sampling" "$DUTY_1HZ" "$AVG_MA" "$AVG_MW" "$DAYS"

# Scenario 4: Event-driven (0.1% duty cycle)
IFS='|' read -r AVG_MA AVG_MW <<< "$(calculate_inference_power "$PLATFORM" "$INFERENCE_MS" 0.1)"
IFS='|' read -r HOURS DAYS MONTHS <<< "$(estimate_battery_life "$AVG_MA" "$BATTERY_2000")"
printf "%-25s %11.1f%% %10.2f mA %12.2f mW %10.0f months\n" \
    "Event-driven (rare)" "0.1" "$AVG_MA" "$AVG_MW" "$MONTHS"

# Scenario 5: Deep sleep dominant (0.01% duty cycle)
IFS='|' read -r AVG_MA AVG_MW <<< "$(calculate_inference_power "$PLATFORM" "$INFERENCE_MS" 0.01)"
IFS='|' read -r HOURS DAYS MONTHS <<< "$(estimate_battery_life "$AVG_MA" "$BATTERY_2000")"
printf "%-25s %11.2f%% %10.2f mA %12.2f mW %10.0f months\n" \
    "Ultra low power" "0.01" "$AVG_MA" "$AVG_MW" "$MONTHS"

echo ""
echo "  * Battery life based on 2000mAh Li-ion battery"
echo ""

# Battery comparison table
echo -e "${CYAN}🔋 Battery Life Estimates (1 Hz sampling):${NC}"
echo ""

DUTY_1HZ=$(echo "scale=2; ($INFERENCE_MS * 1) / 1000 * 100" | bc)
IFS='|' read -r AVG_MA AVG_MW <<< "$(calculate_inference_power "$PLATFORM" "$INFERENCE_MS" "$DUTY_1HZ")"

printf "%-30s %15s %15s\n" "Battery Type" "Capacity" "Est. Life"
echo "  $(printf '%.0s─' {1..60})"

# 18650 Li-ion
IFS='|' read -r HOURS DAYS MONTHS <<< "$(estimate_battery_life "$AVG_MA" 2000)"
printf "%-30s %14s %12.0f days\n" "18650 Li-ion" "2000 mAh" "$DAYS"

# Small Li-Po
IFS='|' read -r HOURS DAYS MONTHS <<< "$(estimate_battery_life "$AVG_MA" 1000)"
printf "%-30s %14s %12.0f days\n" "Small Li-Po" "1000 mAh" "$DAYS"

# AA Alkaline
IFS='|' read -r HOURS DAYS MONTHS <<< "$(estimate_battery_life "$AVG_MA" 2500)"
printf "%-30s %14s %12.0f days\n" "AA Alkaline (x2)" "2500 mAh" "$DAYS"

# Coin cell
IFS='|' read -r HOURS DAYS MONTHS <<< "$(estimate_battery_life "$AVG_MA" 250)"
printf "%-30s %14s %12.0f days\n" "CR2032 Coin Cell" "250 mAh" "$DAYS"

echo ""

# Optimization tips
echo -e "${CYAN}💡 Power Optimization Tips:${NC}"
echo ""

ACTIVE_MA=${PLATFORM_ACTIVE_MA[$PLATFORM]}
if [ $ACTIVE_MA -gt 100 ]; then
    echo "  • ${YELLOW}High active current (${ACTIVE_MA}mA)${NC} - maximize sleep time"
    echo "  • Use deep sleep between inferences"
    echo "  • Consider event-driven inference (interrupt-based)"
fi

if [ $INFERENCE_MS -gt 50 ]; then
    echo "  • ${YELLOW}Long inference time (${INFERENCE_MS}ms)${NC}"
    echo "  • Enable SIMD optimizations (ESP-NN, ARM NEON)"
    echo "  • Use quantized models (Q15 fixed-point)"
    echo "  • Reduce model complexity"
fi

echo "  • Disable unused peripherals (WiFi, Bluetooth, sensors)"
echo "  • Lower CPU frequency during inference if possible"
echo "  • Use DMA for data transfers to reduce CPU usage"
echo "  • Batch multiple sensor readings per inference"
echo ""

# Platform-specific recommendations
case $PLATFORM in
    esp32|esp32s3)
        echo "  ${BLUE}ESP32 Specific:${NC}"
        echo "  • Use light sleep (5-10mA) between quick inferences"
        echo "  • Use deep sleep (<1mA) for longer intervals"
        echo "  • WiFi adds ~80mA - disable when not needed"
        echo "  • ULP coprocessor for ultra-low power monitoring"
        ;;
    stm32f4|stm32h7)
        echo "  ${BLUE}STM32 Specific:${NC}"
        echo "  • Use Stop mode (10-50µA) instead of Sleep mode"
        echo "  • Use Standby mode (~1µA) for long intervals"
        echo "  • Enable voltage regulator low-power mode"
        echo "  • Use RTC wakeup for periodic inference"
        ;;
    rp2040)
        echo "  ${BLUE}RP2040 Specific:${NC}"
        echo "  • Use dormant mode (180µA) for deep sleep"
        echo "  • Power down unused core"
        echo "  • Lower core voltage if frequency allows"
        echo "  • Use PIO for sensor sampling without waking CPU"
        ;;
esac

echo ""
echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

# Export option
echo -e "${GREEN}💾 Export results:${NC}"
echo "  JSON: ./tools/power/power_profile.sh $PLATFORM $MODEL_FILE --json > power_report.json"
echo "  CSV:  ./tools/power/power_profile.sh $PLATFORM $MODEL_FILE --csv > power_report.csv"
echo ""

exit 0
