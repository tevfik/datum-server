#!/usr/bin/env python3
"""
EIF Plotter - Universal Real-Time Visualization Tool

Supports multiple visualization modes:
- Serial port (embedded device)
- Pipe (stdin from demo)
- File (replay recorded data)
- CV pipeline output (image processing results)

Protocol: JSON-over-UART/Pipe
Format: {"timestamp": 1234, "signals": [0.1, 0.5, -0.2], "prediction": "anomaly"}

Usage:
    # From serial port
    python eif_plotter.py --port /dev/ttyUSB0 --baud 115200
    
    # From pipe
    ./bin/imu_fusion_demo --json | python eif_plotter.py --stdin
    
    # From file
    python eif_plotter.py --file data.jsonl
    
    # CV pipeline mode (shows images from edge_detection)
    ./bin/edge_detection_demo --json | python eif_plotter.py --stdin --cv
"""

import sys
import json
import argparse
import threading
import queue
import os
from collections import deque
from datetime import datetime

try:
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation
    from matplotlib.patches import Rectangle, Circle
    from matplotlib.gridspec import GridSpec
    import matplotlib.image as mpimg
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Warning: matplotlib not installed. Install with: pip install matplotlib")

try:
    import numpy as np
    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False

try:
    import serial
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False

# ============================================================================
# Configuration
# ============================================================================

MAX_POINTS = 500
UPDATE_INTERVAL_MS = 50

# Signal colors
COLORS = ['#FF6B6B', '#4ECDC4', '#45B7D1', '#96CEB4', '#FFEAA7', '#DDA0DD', '#98D8C8']

# Demo type colors
DEMO_COLORS = {
    'normal': '#4ECDC4',
    'anomaly': '#FF6B6B',
    'fault': '#FF6B6B',
    'CIRCLE': '#45B7D1',
    'SWIPE': '#96CEB4',
    'SHAKE': '#FFEAA7',
    'HIGH': '#FF6B6B',
    'MEDIUM': '#FFEAA7',
    'LOW': '#4ECDC4',
    'NONE': '#888888',
}

# ============================================================================
# Data Reader Classes
# ============================================================================

class DataReader:
    """Base class for data readers"""
    def readline(self):
        raise NotImplementedError
    
    def close(self):
        pass

class StdinReader(DataReader):
    """Read from stdin (pipe)"""
    def readline(self):
        try:
            line = sys.stdin.readline()
            return line.strip() if line else None
        except:
            return None

class SerialReader(DataReader):
    """Read from serial port"""
    def __init__(self, port, baud):
        if not HAS_SERIAL:
            raise ImportError("pyserial not installed. Run: pip install pyserial")
        self.ser = serial.Serial(port, baud, timeout=0.1)
        
    def readline(self):
        try:
            line = self.ser.readline().decode('utf-8', errors='ignore')
            return line.strip() if line else None
        except:
            return None
    
    def close(self):
        self.ser.close()

class FileReader(DataReader):
    """Read from JSONL file"""
    def __init__(self, filepath):
        self.file = open(filepath, 'r')
        
    def readline(self):
        line = self.file.readline()
        return line.strip() if line else None
    
    def close(self):
        self.file.close()

# ============================================================================
# CV Pipeline Visualizer
# ============================================================================

class CVPipelineViewer:
    """Visualize CV pipeline outputs"""
    
    def __init__(self, reader):
        self.reader = reader
        self.cv_data = None
        self.images = []
        
    def run(self):
        """Process CV pipeline JSON and display images"""
        while True:
            line = self.reader.readline()
            if not line:
                break
            
            try:
                data = json.loads(line)
                if data.get('type') == 'cv_pipeline':
                    self.cv_data = data
                    self.display_pipeline(data)
            except json.JSONDecodeError:
                continue
        
        if self.cv_data:
            plt.show()
    
    def display_pipeline(self, data):
        """Display CV pipeline stages"""
        outputs = data.get('outputs', [])
        n = len(outputs)
        if n == 0:
            return
        
        # Calculate grid dimensions
        cols = min(3, n)
        rows = (n + cols - 1) // cols
        
        fig, axes = plt.subplots(rows, cols, figsize=(cols * 4, rows * 3))
        fig.suptitle(f"CV Pipeline: {data.get('name', 'unknown')}", fontsize=14, fontweight='bold')
        
        if rows == 1 and cols == 1:
            axes = [[axes]]
        elif rows == 1:
            axes = [axes]
        elif cols == 1:
            axes = [[ax] for ax in axes]
        
        for i, output in enumerate(outputs):
            row, col = i // cols, i % cols
            ax = axes[row][col] if rows > 1 else axes[0][col]
            
            filepath = output.get('file', '')
            name = output.get('name', f'output_{i}')
            
            if os.path.exists(filepath):
                try:
                    img = mpimg.imread(filepath)
                    ax.imshow(img, cmap='gray')
                    
                    # Add stats if available
                    stats = []
                    if 'mean' in output:
                        stats.append(f"mean={output['mean']:.1f}")
                    if 'edge_pixels' in output:
                        stats.append(f"edges={output['edge_pixels']}")
                    if 'density' in output:
                        stats.append(f"density={output['density']:.1f}%")
                    
                    title = name
                    if stats:
                        title += f"\n({', '.join(stats)})"
                    ax.set_title(title, fontsize=9)
                except Exception as e:
                    ax.text(0.5, 0.5, f"Error loading\n{filepath}", 
                            ha='center', va='center', transform=ax.transAxes)
                    ax.set_title(name)
            else:
                ax.text(0.5, 0.5, f"File not found:\n{filepath}", 
                        ha='center', va='center', transform=ax.transAxes)
                ax.set_title(name)
            
            ax.axis('off')
        
        # Hide unused axes
        for i in range(n, rows * cols):
            row, col = i // cols, i % cols
            ax = axes[row][col] if rows > 1 else axes[0][col]
            ax.axis('off')
        
        plt.tight_layout()

# ============================================================================
# Tracking Visualizer
# ============================================================================

class TrackingViewer:
    """Visualize object tracking data"""
    
    def __init__(self, reader, title="Object Tracking"):
        self.reader = reader
        self.title = title
        self.track_data = []
        self.running = True
        self.data_queue = queue.Queue()
        
    def read_data_thread(self):
        while self.running:
            try:
                line = self.reader.readline()
                if line:
                    self.data_queue.put(line)
            except:
                pass
    
    def run(self):
        # Start reader thread
        reader_thread = threading.Thread(target=self.read_data_thread, daemon=True)
        reader_thread.start()
        
        # Create figure
        plt.style.use('dark_background')
        self.fig, (self.ax_track, self.ax_error) = plt.subplots(1, 2, figsize=(14, 6))
        self.fig.suptitle(self.title, fontsize=14, fontweight='bold')
        
        self.ax_track.set_title('Object Trajectory')
        self.ax_track.set_xlabel('X')
        self.ax_track.set_ylabel('Y')
        self.ax_track.grid(True, alpha=0.3)
        
        self.ax_error.set_title('Tracking Error')
        self.ax_error.set_xlabel('Frame')
        self.ax_error.set_ylabel('Error (pixels)')
        self.ax_error.grid(True, alpha=0.3)
        
        self.true_line, = self.ax_track.plot([], [], 'g-', label='True', linewidth=2)
        self.pred_line, = self.ax_track.plot([], [], 'r--', label='Predicted', linewidth=2)
        self.error_line, = self.ax_error.plot([], [], 'c-', linewidth=2)
        
        self.ax_track.legend()
        
        self.anim = FuncAnimation(self.fig, self.update_plot, interval=UPDATE_INTERVAL_MS,
                                   blit=False, cache_frame_data=False)
        
        plt.tight_layout()
        
        try:
            plt.show()
        except KeyboardInterrupt:
            pass
        finally:
            self.running = False
            self.reader.close()
    
    def update_plot(self, frame):
        while not self.data_queue.empty():
            try:
                line = self.data_queue.get_nowait()
                data = json.loads(line)
                
                if data.get('type') == 'tracking' and 'tracks' in data:
                    tracks = data['tracks']
                    if tracks:
                        track = tracks[0]
                        self.track_data.append({
                            'frame': data.get('frame', len(self.track_data)),
                            'x': track.get('x', 0),
                            'y': track.get('y', 0),
                            'pred_x': track.get('pred_x', 0),
                            'pred_y': track.get('pred_y', 0),
                        })
                
            except (json.JSONDecodeError, queue.Empty):
                continue
        
        if self.track_data:
            # Track trajectory
            true_x = [d['x'] for d in self.track_data]
            true_y = [d['y'] for d in self.track_data]
            pred_x = [d['pred_x'] for d in self.track_data]
            pred_y = [d['pred_y'] for d in self.track_data]
            
            self.true_line.set_data(true_x, true_y)
            self.pred_line.set_data(pred_x, pred_y)
            
            # Error over time
            frames = [d['frame'] for d in self.track_data]
            errors = [((d['x'] - d['pred_x'])**2 + (d['y'] - d['pred_y'])**2)**0.5 
                      for d in self.track_data]
            self.error_line.set_data(frames, errors)
            
            self.ax_track.relim()
            self.ax_track.autoscale_view()
            self.ax_error.relim()
            self.ax_error.autoscale_view()
        
        return [self.true_line, self.pred_line, self.error_line]

# ============================================================================
# Standard Real-Time Plotter
# ============================================================================

class EIFPlotter:
    def __init__(self, reader, title="EIF Real-Time Plotter"):
        self.reader = reader
        self.title = title
        
        # Data storage
        self.timestamps = deque(maxlen=MAX_POINTS)
        self.signal_data = {}
        self.predictions = deque(maxlen=50)
        
        # Thread-safe queue for data
        self.data_queue = queue.Queue()
        self.running = True
        
        # Stats
        self.packet_count = 0
        self.error_count = 0
        self.last_prediction = None
        self.last_type = None
        
    def read_data_thread(self):
        while self.running:
            try:
                line = self.reader.readline()
                if line:
                    self.data_queue.put(line)
            except Exception as e:
                print(f"Read error: {e}", file=sys.stderr)
    
    def parse_packet(self, line):
        try:
            data = json.loads(line)
            self.packet_count += 1
            return data
        except json.JSONDecodeError:
            self.error_count += 1
            return None
    
    def update_data(self, data):
        t = data.get('timestamp', data.get('t', self.packet_count))
        self.timestamps.append(t)
        
        self.last_type = data.get('type', self.last_type)
        
        # Signals (array or dict)
        signals = data.get('signals', {})
        if isinstance(signals, list):
            for i, val in enumerate(signals):
                name = f"signal_{i}"
                if name not in self.signal_data:
                    self.signal_data[name] = deque(maxlen=MAX_POINTS)
                self.signal_data[name].append(val)
        elif isinstance(signals, dict):
            for name, val in signals.items():
                if name not in self.signal_data:
                    self.signal_data[name] = deque(maxlen=MAX_POINTS)
                if isinstance(val, (int, float)):
                    self.signal_data[name].append(val)
        
        # Handle nested keys
        for key in ['estimate', 'true', 'state', 'errors', 'probs']:
            if key in data and isinstance(data[key], dict):
                for subkey, val in data[key].items():
                    name = f"{key}.{subkey}"
                    if name not in self.signal_data:
                        self.signal_data[name] = deque(maxlen=MAX_POINTS)
                    if isinstance(val, (int, float)):
                        self.signal_data[name].append(val)
        
        # Prediction
        pred = data.get('prediction', None)
        if pred:
            self.last_prediction = pred
            self.predictions.append((t, pred))
    
    def create_figure(self):
        plt.style.use('dark_background')
        self.fig, self.axes = plt.subplots(2, 1, figsize=(12, 8))
        self.fig.suptitle(self.title, fontsize=14, fontweight='bold')
        
        self.ax_signals = self.axes[0]
        self.ax_signals.set_title('Signals')
        self.ax_signals.set_xlabel('Time')
        self.ax_signals.set_ylabel('Value')
        self.ax_signals.grid(True, alpha=0.3)
        self.signal_lines = {}
        
        self.ax_stats = self.axes[1]
        self.ax_stats.set_title('Status')
        self.ax_stats.axis('off')
        self.stats_text = self.ax_stats.text(0.02, 0.95, '', transform=self.ax_stats.transAxes,
                                              fontfamily='monospace', fontsize=10,
                                              verticalalignment='top')
        
        self.pred_rect = Rectangle((0.7, 0.1), 0.28, 0.8, transform=self.ax_stats.transAxes,
                                    facecolor='green', alpha=0.3)
        self.ax_stats.add_patch(self.pred_rect)
        self.pred_text = self.ax_stats.text(0.84, 0.5, '', transform=self.ax_stats.transAxes,
                                             fontsize=16, fontweight='bold',
                                             horizontalalignment='center',
                                             verticalalignment='center')
        
        plt.tight_layout()
    
    def update_plot(self, frame):
        while not self.data_queue.empty():
            try:
                line = self.data_queue.get_nowait()
                data = self.parse_packet(line)
                if data:
                    self.update_data(data)
            except queue.Empty:
                break
        
        if len(self.timestamps) > 0:
            times = list(self.timestamps)
            
            for i, (name, values) in enumerate(self.signal_data.items()):
                if name not in self.signal_lines:
                    color = COLORS[i % len(COLORS)]
                    line, = self.ax_signals.plot([], [], color=color, label=name, linewidth=1.5)
                    self.signal_lines[name] = line
                    self.ax_signals.legend(loc='upper right', fontsize=8)
                
                vals = list(values)
                if len(vals) < len(times):
                    vals = [0] * (len(times) - len(vals)) + vals
                elif len(vals) > len(times):
                    vals = vals[-len(times):]
                
                self.signal_lines[name].set_data(times[-len(vals):], vals)
            
            self.ax_signals.relim()
            self.ax_signals.autoscale_view()
        
        stats_str = f"""
Type:    {self.last_type or 'N/A'}
Packets: {self.packet_count}
Errors:  {self.error_count}
Signals: {len(self.signal_data)}
Points:  {len(self.timestamps)}

Last Prediction: {self.last_prediction or 'N/A'}
"""
        self.stats_text.set_text(stats_str)
        
        if self.last_prediction:
            self.pred_text.set_text(str(self.last_prediction))
            pred_str = str(self.last_prediction).lower()
            if 'fault' in pred_str or 'anomaly' in pred_str or 'high' in pred_str:
                self.pred_rect.set_facecolor('red')
            else:
                self.pred_rect.set_facecolor('green')
        
        return list(self.signal_lines.values()) + [self.stats_text, self.pred_text]
    
    def run(self):
        if not HAS_MATPLOTLIB:
            print("Cannot run: matplotlib not installed")
            return
        
        reader_thread = threading.Thread(target=self.read_data_thread, daemon=True)
        reader_thread.start()
        
        self.create_figure()
        
        self.anim = FuncAnimation(self.fig, self.update_plot, interval=UPDATE_INTERVAL_MS,
                                   blit=False, cache_frame_data=False)
        
        try:
            plt.show()
        except KeyboardInterrupt:
            pass
        finally:
            self.running = False
            self.reader.close()

# ============================================================================
# CLI Interface
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description='EIF Real-Time Plotter')
    
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--stdin', action='store_true', help='Read from stdin (pipe)')
    source.add_argument('--port', type=str, help='Serial port (e.g., /dev/ttyUSB0)')
    source.add_argument('--file', type=str, help='JSONL file path')
    
    parser.add_argument('--baud', type=int, default=115200, help='Baud rate for serial')
    parser.add_argument('--title', type=str, default='EIF Real-Time Plotter', help='Window title')
    parser.add_argument('--cv', action='store_true', help='CV pipeline mode (display images)')
    parser.add_argument('--tracking', action='store_true', help='Tracking visualization mode')
    
    args = parser.parse_args()
    
    # Create reader
    if args.stdin:
        reader = StdinReader()
    elif args.port:
        reader = SerialReader(args.port, args.baud)
    elif args.file:
        reader = FileReader(args.file)
    
    # Select visualization mode
    if args.cv:
        viewer = CVPipelineViewer(reader)
        viewer.run()
    elif args.tracking:
        viewer = TrackingViewer(reader, title=args.title)
        viewer.run()
    else:
        plotter = EIFPlotter(reader, title=args.title)
        plotter.run()

if __name__ == '__main__':
    main()
