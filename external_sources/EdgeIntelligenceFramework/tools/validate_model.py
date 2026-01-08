#!/usr/bin/env python3
"""
EIF Model Validator

Validates .eif model files for correctness, compatibility, and memory requirements.

Usage:
    python3 tools/validate_model.py model.eif
    python3 tools/validate_model.py models/  # Validate all in directory
"""

import sys
import struct
import os
from pathlib import Path
from typing import Dict, List, Tuple, Optional
import json

# EIF Binary Format Constants
MAGIC_NUMBER = b'MFIE'  # "EIFM" reversed (little-endian)
VERSION = 1

# Layer Types
LAYER_DENSE = 0x01
LAYER_CONV2D = 0x02
LAYER_DEPTHWISE_CONV2D = 0x03
LAYER_POOL_MAX = 0x04
LAYER_POOL_AVG = 0x05
LAYER_FLATTEN = 0x06
LAYER_ACTIVATION = 0x07
LAYER_BATCH_NORM = 0x08
LAYER_DROPOUT = 0x09
LAYER_ADD = 0x0A
LAYER_CONCAT = 0x0B
LAYER_RNN = 0x0C
LAYER_LSTM = 0x0D
LAYER_GRU = 0x0E

LAYER_NAMES = {
    0x01: "Dense",
    0x02: "Conv2D",
    0x03: "DepthwiseConv2D",
    0x04: "MaxPool",
    0x05: "AvgPool",
    0x06: "Flatten",
    0x07: "Activation",
    0x08: "BatchNorm",
    0x09: "Dropout",
    0x0A: "Add",
    0x0B: "Concat",
    0x0C: "RNN",
    0x0D: "LSTM",
    0x0E: "GRU",
}

# Activation Types
ACT_NONE = 0x00
ACT_RELU = 0x01
ACT_RELU6 = 0x02
ACT_SIGMOID = 0x03
ACT_TANH = 0x04
ACT_SOFTMAX = 0x05

ACTIVATION_NAMES = {
    0x00: "None",
    0x01: "ReLU",
    0x02: "ReLU6",
    0x03: "Sigmoid",
    0x04: "Tanh",
    0x05: "Softmax",
}

# Colors for output
class Color:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    CYAN = '\033[0;36m'
    NC = '\033[0m'


class ValidationError:
    """Represents a validation error or warning"""
    def __init__(self, severity: str, message: str, details: str = ""):
        self.severity = severity  # 'ERROR', 'WARNING', 'INFO'
        self.message = message
        self.details = details


class EIFModelValidator:
    """Validates EIF model files"""
    
    def __init__(self, filepath: str):
        self.filepath = filepath
        self.errors: List[ValidationError] = []
        self.warnings: List[ValidationError] = []
        self.info: List[ValidationError] = []
        
        # Model properties
        self.version = 0
        self.num_layers = 0
        self.input_size = 0
        self.layers: List[Dict] = []
        self.total_memory = 0
        self.total_params = 0
        
    def validate(self) -> bool:
        """Run all validation checks"""
        
        # Check file exists and size
        if not os.path.exists(self.filepath):
            self.errors.append(ValidationError('ERROR', f"File not found: {self.filepath}"))
            return False
            
        file_size = os.path.getsize(self.filepath)
        if file_size < 16:
            self.errors.append(ValidationError('ERROR', "File too small to be valid EIF model"))
            return False
            
        # Parse model
        try:
            with open(self.filepath, 'rb') as f:
                data = f.read()
                
            # Validate header
            if not self._validate_header(data):
                return False
                
            # Parse layers
            if not self._parse_layers(data):
                return False
                
            # Validate layer compatibility
            self._validate_layer_compatibility()
            
            # Calculate memory requirements
            self._calculate_memory()
            
            # Check for common issues
            self._check_common_issues()
            
        except Exception as e:
            self.errors.append(ValidationError('ERROR', f"Failed to parse model: {str(e)}"))
            return False
            
        return len(self.errors) == 0
        
    def _validate_header(self, data: bytes) -> bool:
        """Validate EIF header"""
        
        # Check magic number
        magic = data[0:4]
        if magic != MAGIC_NUMBER:
            self.errors.append(ValidationError(
                'ERROR',
                f"Invalid magic number: {magic.hex()} (expected {MAGIC_NUMBER.hex()})"
            ))
            return False
            
        # Check version
        self.version = struct.unpack('<I', data[4:8])[0]
        if self.version != VERSION:
            self.warnings.append(ValidationError(
                'WARNING',
                f"Version mismatch: {self.version} (expected {VERSION})"
            ))
            
        # Get number of layers
        self.num_layers = struct.unpack('<I', data[8:12])[0]
        if self.num_layers == 0:
            self.errors.append(ValidationError('ERROR', "Model has zero layers"))
            return False
        if self.num_layers > 1000:
            self.warnings.append(ValidationError(
                'WARNING',
                f"Very large number of layers: {self.num_layers}"
            ))
            
        # Get input size
        self.input_size = struct.unpack('<I', data[12:16])[0]
        if self.input_size == 0:
            self.errors.append(ValidationError('ERROR', "Input size is zero"))
            return False
            
        self.info.append(ValidationError(
            'INFO',
            f"Model: {self.num_layers} layers, input size: {self.input_size}"
        ))
        
        return True
        
    def _parse_layers(self, data: bytes) -> bool:
        """Parse layer information"""
        
        offset = 16  # After header
        
        for i in range(self.num_layers):
            if offset + 8 > len(data):
                self.errors.append(ValidationError(
                    'ERROR',
                    f"Truncated data at layer {i}"
                ))
                return False
                
            layer_type = data[offset]
            layer_size = struct.unpack('<I', data[offset+4:offset+8])[0]
            
            if layer_type not in LAYER_NAMES:
                self.warnings.append(ValidationError(
                    'WARNING',
                    f"Unknown layer type: 0x{layer_type:02x} at layer {i}"
                ))
                layer_name = f"Unknown(0x{layer_type:02x})"
            else:
                layer_name = LAYER_NAMES[layer_type]
                
            layer_info = {
                'index': i,
                'type': layer_type,
                'name': layer_name,
                'size': layer_size,
                'offset': offset
            }
            
            self.layers.append(layer_info)
            offset += layer_size
            
        return True
        
    def _validate_layer_compatibility(self):
        """Check layer sequence compatibility"""
        
        for i in range(len(self.layers) - 1):
            curr_layer = self.layers[i]
            next_layer = self.layers[i + 1]
            
            # Check for invalid sequences
            if curr_layer['type'] == LAYER_FLATTEN:
                if next_layer['type'] in [LAYER_CONV2D, LAYER_DEPTHWISE_CONV2D, LAYER_POOL_MAX, LAYER_POOL_AVG]:
                    self.errors.append(ValidationError(
                        'ERROR',
                        f"Invalid sequence: {curr_layer['name']} → {next_layer['name']} at layers {i}-{i+1}",
                        "Flatten must be followed by Dense, not Conv/Pool"
                    ))
                    
            # Check Conv2D after Dense (likely wrong)
            if curr_layer['type'] == LAYER_DENSE:
                if next_layer['type'] in [LAYER_CONV2D, LAYER_DEPTHWISE_CONV2D]:
                    self.warnings.append(ValidationError(
                        'WARNING',
                        f"Unusual sequence: {curr_layer['name']} → {next_layer['name']} at layers {i}-{i+1}",
                        "Conv layers after Dense are uncommon"
                    ))
                    
    def _calculate_memory(self):
        """Calculate memory requirements"""
        
        # Rough estimates based on typical layer sizes
        for layer in self.layers:
            layer_type = layer['type']
            
            # Dense: input * output * 2 (Q15 weights)
            if layer_type == LAYER_DENSE:
                layer_mem = layer['size']  # Weights data
                self.total_memory += layer_mem
                self.total_params += layer_mem // 2  # Q15 = 2 bytes per param
                
            # Conv2D: kernel_h * kernel_w * in_ch * out_ch * 2
            elif layer_type == LAYER_CONV2D:
                layer_mem = layer['size']
                self.total_memory += layer_mem
                self.total_params += layer_mem // 2
                
            # Other layers (activations, pooling, etc.)
            else:
                layer_mem = layer['size']
                self.total_memory += layer_mem
                
        self.info.append(ValidationError(
            'INFO',
            f"Total model size: {self.total_memory:,} bytes ({self.total_memory/1024:.1f} KB)"
        ))
        
        self.info.append(ValidationError(
            'INFO',
            f"Total parameters: {self.total_params:,}"
        ))
        
    def _check_common_issues(self):
        """Check for common model issues"""
        
        # Check model size
        if self.total_memory > 1024 * 1024:  # > 1MB
            self.warnings.append(ValidationError(
                'WARNING',
                f"Large model size: {self.total_memory/1024/1024:.2f} MB",
                "May not fit on resource-constrained devices"
            ))
            
        # Check for missing activation on final layer
        if self.layers:
            last_layer = self.layers[-1]
            if last_layer['type'] == LAYER_DENSE:
                self.info.append(ValidationError(
                    'INFO',
                    "Final layer is Dense without explicit activation",
                    "Ensure softmax/sigmoid is applied for classification"
                ))
                
    def print_report(self, verbose: bool = False):
        """Print validation report"""
        
        filename = os.path.basename(self.filepath)
        print(f"\n{Color.CYAN}{'='*70}{Color.NC}")
        print(f"{Color.CYAN}Model Validation Report: {filename}{Color.NC}")
        print(f"{Color.CYAN}{'='*70}{Color.NC}\n")
        
        # Errors
        if self.errors:
            print(f"{Color.RED}❌ ERRORS ({len(self.errors)}):{Color.NC}")
            for err in self.errors:
                print(f"  {Color.RED}•{Color.NC} {err.message}")
                if err.details:
                    print(f"    → {err.details}")
            print()
        else:
            print(f"{Color.GREEN}✅ No errors found{Color.NC}\n")
            
        # Warnings
        if self.warnings:
            print(f"{Color.YELLOW}⚠️  WARNINGS ({len(self.warnings)}):{Color.NC}")
            for warn in self.warnings:
                print(f"  {Color.YELLOW}•{Color.NC} {warn.message}")
                if warn.details:
                    print(f"    → {warn.details}")
            print()
            
        # Info
        if verbose and self.info:
            print(f"{Color.BLUE}ℹ️  INFORMATION:{Color.NC}")
            for info in self.info:
                print(f"  {Color.BLUE}•{Color.NC} {info.message}")
                if info.details:
                    print(f"    → {info.details}")
            print()
            
        # Layer summary
        if verbose and self.layers:
            print(f"{Color.CYAN}📋 Layer Summary:{Color.NC}")
            print(f"  {'Index':<8} {'Type':<20} {'Size':<12}")
            print(f"  {'-'*40}")
            for layer in self.layers[:10]:  # Show first 10
                print(f"  {layer['index']:<8} {layer['name']:<20} {layer['size']:>8,} B")
            if len(self.layers) > 10:
                print(f"  ... ({len(self.layers) - 10} more layers)")
            print()
            
        # Platform compatibility
        print(f"{Color.CYAN}🎯 Platform Compatibility:{Color.NC}")
        platforms = {
            'Arduino Nano 33': 256 * 1024,
            'ESP32-S3': 8 * 1024 * 1024,
            'STM32F401': 512 * 1024,
            'STM32F407': 1024 * 1024,
            'RP2040': 2 * 1024 * 1024,
        }
        
        for platform, flash_size in platforms.items():
            if self.total_memory < flash_size * 0.5:  # < 50% usage
                status = f"{Color.GREEN}✅{Color.NC}"
            elif self.total_memory < flash_size * 0.8:  # < 80% usage
                status = f"{Color.YELLOW}⚠️ {Color.NC}"
            else:
                status = f"{Color.RED}❌{Color.NC}"
                
            usage_pct = (self.total_memory / flash_size) * 100
            print(f"  {status} {platform:<20} {usage_pct:>5.1f}% Flash usage")
            
        print(f"\n{Color.CYAN}{'='*70}{Color.NC}\n")
        
        # Final verdict
        if not self.errors:
            print(f"{Color.GREEN}✅ Model is valid and ready for deployment!{Color.NC}\n")
        else:
            print(f"{Color.RED}❌ Model validation failed. Fix errors before deployment.{Color.NC}\n")
            
    def to_json(self) -> str:
        """Export validation results as JSON"""
        
        result = {
            'filepath': self.filepath,
            'valid': len(self.errors) == 0,
            'version': self.version,
            'num_layers': self.num_layers,
            'input_size': self.input_size,
            'total_memory': self.total_memory,
            'total_params': self.total_params,
            'errors': [{'severity': e.severity, 'message': e.message, 'details': e.details} for e in self.errors],
            'warnings': [{'severity': w.severity, 'message': w.message, 'details': w.details} for w in self.warnings],
            'info': [{'severity': i.severity, 'message': i.message, 'details': i.details} for i in self.info],
            'layers': self.layers
        }
        
        return json.dumps(result, indent=2)


def validate_directory(directory: str, verbose: bool = False):
    """Validate all .eif files in a directory"""
    
    path = Path(directory)
    eif_files = list(path.glob('*.eif'))
    
    if not eif_files:
        print(f"{Color.YELLOW}No .eif files found in {directory}{Color.NC}")
        return
        
    print(f"{Color.CYAN}Found {len(eif_files)} model(s) to validate{Color.NC}\n")
    
    results = []
    for eif_file in eif_files:
        validator = EIFModelValidator(str(eif_file))
        is_valid = validator.validate()
        validator.print_report(verbose)
        
        results.append({
            'file': eif_file.name,
            'valid': is_valid,
            'errors': len(validator.errors),
            'warnings': len(validator.warnings)
        })
        
    # Summary
    print(f"{Color.CYAN}{'='*70}{Color.NC}")
    print(f"{Color.CYAN}Validation Summary{Color.NC}")
    print(f"{Color.CYAN}{'='*70}{Color.NC}\n")
    
    valid_count = sum(1 for r in results if r['valid'])
    print(f"  Total models: {len(results)}")
    print(f"  Valid: {Color.GREEN}{valid_count}{Color.NC}")
    print(f"  Invalid: {Color.RED}{len(results) - valid_count}{Color.NC}\n")
    

def main():
    """Main entry point"""
    
    import argparse
    
    parser = argparse.ArgumentParser(
        description='Validate EIF model files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 validate_model.py model.eif
  python3 validate_model.py models/ -v
  python3 validate_model.py model.eif --json > report.json
        """
    )
    
    parser.add_argument('path', help='Path to .eif file or directory')
    parser.add_argument('-v', '--verbose', action='store_true', help='Show detailed information')
    parser.add_argument('--json', action='store_true', help='Output as JSON')
    
    args = parser.parse_args()
    
    # Check if path is directory or file
    if os.path.isdir(args.path):
        if args.json:
            print(f"{Color.YELLOW}JSON output not supported for directory validation{Color.NC}")
            return 1
        validate_directory(args.path, args.verbose)
    else:
        validator = EIFModelValidator(args.path)
        is_valid = validator.validate()
        
        if args.json:
            print(validator.to_json())
        else:
            validator.print_report(args.verbose)
            
        return 0 if is_valid else 1
        
    return 0


if __name__ == '__main__':
    sys.exit(main())
