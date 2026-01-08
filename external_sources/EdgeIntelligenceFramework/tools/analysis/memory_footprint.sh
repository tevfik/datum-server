#!/bin/bash
#
# Memory Footprint Analyzer for Edge Intelligence Framework
# Analyzes RAM and Flash usage for embedded targets
#
# Usage: ./memory_footprint.sh [build_dir] [output_format]
#   build_dir: Path to build directory (default: build/)
#   output_format: text, json, or csv (default: text)
#

set -e

# Configuration
BUILD_DIR="${1:-build}"
OUTPUT_FORMAT="${2:-text}"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory '$BUILD_DIR' not found${NC}"
    echo "Run 'make' first to build the project"
    exit 1
fi

# Find all library files
LIBS=$(find "$BUILD_DIR" -name "*.a" -o -name "*.so" 2>/dev/null | sort)
if [ -z "$LIBS" ]; then
    echo -e "${RED}Error: No library files found in '$BUILD_DIR'${NC}"
    exit 1
fi

# Find executables
EXES=$(find "$BUILD_DIR/bin" -type f -executable 2>/dev/null | sort || true)

#
# Function: Analyze library memory usage
#
analyze_library() {
    local LIB=$1
    local LIB_NAME=$(basename "$LIB" .a)
    
    # Get size of text (code), data, bss (uninitialized data)
    if command -v size &> /dev/null; then
        SIZE_OUTPUT=$(size "$LIB" 2>/dev/null || echo "0 0 0")
        TEXT=$(echo "$SIZE_OUTPUT" | tail -1 | awk '{print $1}')
        DATA=$(echo "$SIZE_OUTPUT" | tail -1 | awk '{print $2}')
        BSS=$(echo "$SIZE_OUTPUT" | tail -1 | awk '{print $3}')
    else
        TEXT=0
        DATA=0
        BSS=0
    fi
    
    # Calculate totals
    FLASH=$((TEXT + DATA))  # Flash = code + initialized data
    RAM=$((DATA + BSS))      # RAM = initialized + uninitialized data
    
    echo "$LIB_NAME|$TEXT|$DATA|$BSS|$FLASH|$RAM"
}

#
# Function: Analyze executable memory usage
#
analyze_executable() {
    local EXE=$1
    local EXE_NAME=$(basename "$EXE")
    
    if command -v size &> /dev/null; then
        SIZE_OUTPUT=$(size "$EXE" 2>/dev/null || echo "0 0 0")
        TEXT=$(echo "$SIZE_OUTPUT" | tail -1 | awk '{print $1}')
        DATA=$(echo "$SIZE_OUTPUT" | tail -1 | awk '{print $2}')
        BSS=$(echo "$SIZE_OUTPUT" | tail -1 | awk '{print $3}')
    else
        TEXT=0
        DATA=0
        BSS=0
    fi
    
    FLASH=$((TEXT + DATA))
    RAM=$((DATA + BSS))
    
    echo "$EXE_NAME|$TEXT|$DATA|$BSS|$FLASH|$RAM"
}

#
# Function: Get top symbols by size
#
get_top_symbols() {
    local FILE=$1
    local COUNT=${2:-10}
    
    if command -v nm &> /dev/null; then
        nm --print-size --size-sort --radix=d "$FILE" 2>/dev/null | \
            tail -$COUNT | \
            awk '{printf "  %8d bytes: %s\n", $2, $4}'
    fi
}

#
# Collect all data
#
echo -e "${CYAN}📊 Analyzing memory footprint...${NC}"
echo ""

RESULTS=()
TOTAL_FLASH=0
TOTAL_RAM=0

# Analyze libraries
for LIB in $LIBS; do
    RESULT=$(analyze_library "$LIB")
    RESULTS+=("$RESULT")
    
    FLASH=$(echo "$RESULT" | cut -d'|' -f5)
    RAM=$(echo "$RESULT" | cut -d'|' -f6)
    TOTAL_FLASH=$((TOTAL_FLASH + FLASH))
    TOTAL_RAM=$((TOTAL_RAM + RAM))
done

#
# Output Results
#
if [ "$OUTPUT_FORMAT" = "json" ]; then
    # JSON output
    echo "{"
    echo "  \"timestamp\": \"$(date -Iseconds)\","
    echo "  \"build_dir\": \"$BUILD_DIR\","
    echo "  \"libraries\": ["
    
    FIRST=true
    for RESULT in "${RESULTS[@]}"; do
        IFS='|' read -r NAME TEXT DATA BSS FLASH RAM <<< "$RESULT"
        
        if [ "$FIRST" = true ]; then
            FIRST=false
        else
            echo ","
        fi
        
        echo -n "    {"
        echo -n "\"name\": \"$NAME\", "
        echo -n "\"text\": $TEXT, "
        echo -n "\"data\": $DATA, "
        echo -n "\"bss\": $BSS, "
        echo -n "\"flash\": $FLASH, "
        echo -n "\"ram\": $RAM"
        echo -n "}"
    done
    
    echo ""
    echo "  ],"
    echo "  \"totals\": {"
    echo "    \"flash\": $TOTAL_FLASH,"
    echo "    \"ram\": $TOTAL_RAM"
    echo "  }"
    echo "}"

elif [ "$OUTPUT_FORMAT" = "csv" ]; then
    # CSV output
    echo "Module,Text,Data,BSS,Flash,RAM"
    for RESULT in "${RESULTS[@]}"; do
        IFS='|' read -r NAME TEXT DATA BSS FLASH RAM <<< "$RESULT"
        echo "$NAME,$TEXT,$DATA,$BSS,$FLASH,$RAM"
    done
    echo "TOTAL,,,,$TOTAL_FLASH,$TOTAL_RAM"

else
    # Text output (default)
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    printf "${CYAN}%-25s %10s %10s %10s %12s %12s${NC}\n" \
        "Module" "Text" "Data" "BSS" "Flash Total" "RAM Total"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    
    for RESULT in "${RESULTS[@]}"; do
        IFS='|' read -r NAME TEXT DATA BSS FLASH RAM <<< "$RESULT"
        
        # Color code based on size
        if [ $FLASH -gt 50000 ]; then
            COLOR=$RED
        elif [ $FLASH -gt 20000 ]; then
            COLOR=$YELLOW
        else
            COLOR=$GREEN
        fi
        
        printf "${COLOR}%-25s${NC} %10s %10s %10s %12s %12s\n" \
            "$NAME" \
            "$(numfmt --to=iec $TEXT 2>/dev/null || echo $TEXT)" \
            "$(numfmt --to=iec $DATA 2>/dev/null || echo $DATA)" \
            "$(numfmt --to=iec $BSS 2>/dev/null || echo $BSS)" \
            "$(numfmt --to=iec $FLASH 2>/dev/null || echo $FLASH)" \
            "$(numfmt --to=iec $RAM 2>/dev/null || echo $RAM)"
    done
    
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    printf "${GREEN}%-25s${NC} %10s %10s %10s %12s %12s\n" \
        "TOTAL" "" "" "" \
        "$(numfmt --to=iec $TOTAL_FLASH 2>/dev/null || echo $TOTAL_FLASH)" \
        "$(numfmt --to=iec $TOTAL_RAM 2>/dev/null || echo $TOTAL_RAM)"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
    
    # Memory budget analysis for common MCUs
    echo -e "${CYAN}📱 Target Platform Analysis:${NC}"
    echo ""
    
    # Common microcontroller specs
    declare -A MCU_FLASH=(
        ["Arduino Nano 33"]=262144
        ["ESP32-S3"]=8388608
        ["ESP32"]=4194304
        ["STM32F401"]=524288
        ["STM32F407"]=1048576
        ["STM32H743"]=2097152
        ["RP2040"]=2097152
    )
    
    declare -A MCU_RAM=(
        ["Arduino Nano 33"]=262144
        ["ESP32-S3"]=524288
        ["ESP32"]=520192
        ["STM32F401"]=98304
        ["STM32F407"]=196608
        ["STM32H743"]=1048576
        ["RP2040"]=262144
    )
    
    for MCU in "Arduino Nano 33" "ESP32-S3" "ESP32" "STM32F401" "STM32F407" "STM32H743" "RP2040"; do
        FLASH_BUDGET=${MCU_FLASH[$MCU]}
        RAM_BUDGET=${MCU_RAM[$MCU]}
        
        FLASH_PERCENT=$((TOTAL_FLASH * 100 / FLASH_BUDGET))
        RAM_PERCENT=$((TOTAL_RAM * 100 / RAM_BUDGET))
        
        # Color based on usage percentage
        if [ $FLASH_PERCENT -gt 80 ] || [ $RAM_PERCENT -gt 80 ]; then
            COLOR=$RED
            STATUS="❌"
        elif [ $FLASH_PERCENT -gt 60 ] || [ $RAM_PERCENT -gt 60 ]; then
            COLOR=$YELLOW
            STATUS="⚠️ "
        else
            COLOR=$GREEN
            STATUS="✅"
        fi
        
        printf "${COLOR}${STATUS} %-20s Flash: %3d%%  RAM: %3d%%${NC}\n" \
            "$MCU" "$FLASH_PERCENT" "$RAM_PERCENT"
    done
    
    echo ""
    
    # Largest symbols analysis
    echo -e "${CYAN}🔍 Top 10 Largest Symbols (from core library):${NC}"
    CORE_LIB=$(find "$BUILD_DIR" -name "libeif_core.a" | head -1)
    if [ -n "$CORE_LIB" ]; then
        get_top_symbols "$CORE_LIB" 10
    fi
    
    echo ""
    
    # Recommendations
    echo -e "${CYAN}💡 Optimization Recommendations:${NC}"
    if [ $TOTAL_FLASH -gt 100000 ]; then
        echo "  • Consider enabling LTO (Link Time Optimization)"
        echo "  • Review large modules for optimization opportunities"
    fi
    if [ $TOTAL_RAM -gt 50000 ]; then
        echo "  • Review static buffer sizes"
        echo "  • Consider reducing memory pool sizes"
    fi
    echo "  • Use -Os flag for size optimization"
    echo "  • Remove unused modules from build"
    echo ""
fi

exit 0
