#!/usr/bin/env python3
"""
EIF Real-Time Visualizer

Plots JSON output from EIF demos in real-time.

Usage:
    ./bin/imu_fusion_demo --json | python tools/visualizer/plot_realtime.py
    ./bin/bearing_fault_demo --json | python tools/visualizer/plot_realtime.py
"""

import sys
import json
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from collections import deque

# Configuration
MAX_POINTS = 200
PLOT_INTERVAL_MS = 50

# Data buffers
times = deque(maxlen=MAX_POINTS)
pos_x = deque(maxlen=MAX_POINTS)
pos_y = deque(maxlen=MAX_POINTS)
pos_z = deque(maxlen=MAX_POINTS)
error = deque(maxlen=MAX_POINTS)

# Setup figure
plt.style.use('dark_background')
fig, axes = plt.subplots(2, 2, figsize=(12, 8))
fig.suptitle('EIF Real-Time Visualization', fontsize=14, fontweight='bold')

# Position plot (top left)
ax_pos = axes[0, 0]
ax_pos.set_title('Position (NED)')
ax_pos.set_xlabel('Time (s)')
ax_pos.set_ylabel('Position (m)')
line_x, = ax_pos.plot([], [], 'r-', label='North')
line_y, = ax_pos.plot([], [], 'g-', label='East')
line_z, = ax_pos.plot([], [], 'b-', label='Down')
ax_pos.legend(loc='upper right')
ax_pos.set_xlim(0, 30)
ax_pos.set_ylim(-25, 25)

# Trajectory plot (top right)
ax_traj = axes[0, 1]
ax_traj.set_title('Trajectory (Top View)')
ax_traj.set_xlabel('East (m)')
ax_traj.set_ylabel('North (m)')
ax_traj.set_aspect('equal')
ax_traj.set_xlim(-30, 30)
ax_traj.set_ylim(-30, 30)
line_traj_true, = ax_traj.plot([], [], 'c-', alpha=0.5, label='True')
line_traj_est, = ax_traj.plot([], [], 'y-', linewidth=2, label='Estimate')
ax_traj.legend(loc='upper right')

# Error plot (bottom left)
ax_err = axes[1, 0]
ax_err.set_title('Position Error')
ax_err.set_xlabel('Time (s)')
ax_err.set_ylabel('Error (m)')
line_err, = ax_err.plot([], [], 'm-', linewidth=2)
ax_err.set_xlim(0, 30)
ax_err.set_ylim(0, 5)
ax_err.axhline(y=2.5, color='r', linestyle='--', alpha=0.5, label='GPS noise')
ax_err.legend(loc='upper right')

# Info panel (bottom right)
ax_info = axes[1, 1]
ax_info.axis('off')
info_text = ax_info.text(0.1, 0.9, '', transform=ax_info.transAxes, 
                          fontfamily='monospace', fontsize=10, verticalalignment='top')

# Additional buffers for trajectory
traj_true_x = deque(maxlen=MAX_POINTS)
traj_true_y = deque(maxlen=MAX_POINTS)
traj_est_x = deque(maxlen=MAX_POINTS)
traj_est_y = deque(maxlen=MAX_POINTS)

def update(frame):
    try:
        line = sys.stdin.readline()
        if not line:
            return []
        
        data = json.loads(line.strip())
        
        t = data.get('t', 0)
        times.append(t)
        
        # Parse estimate data
        est = data.get('estimate', {})
        true = data.get('true', {})
        err_data = data.get('error', {})
        
        pos_x.append(est.get('x', 0))
        pos_y.append(est.get('y', 0))
        pos_z.append(est.get('z', 0))
        error.append(err_data.get('pos', 0))
        
        # Trajectory
        traj_true_x.append(true.get('y', 0))
        traj_true_y.append(true.get('x', 0))
        traj_est_x.append(est.get('y', 0))
        traj_est_y.append(est.get('x', 0))
        
        # Update position plot
        line_x.set_data(list(times), list(pos_x))
        line_y.set_data(list(times), list(pos_y))
        line_z.set_data(list(times), list(pos_z))
        
        # Update trajectory
        line_traj_true.set_data(list(traj_true_x), list(traj_true_y))
        line_traj_est.set_data(list(traj_est_x), list(traj_est_y))
        
        # Update error
        line_err.set_data(list(times), list(error))
        
        # Update info
        sensors = data.get('sensors', {})
        info_str = f"""
Time: {t:.2f} s

Position (estimate):
  N: {est.get('x', 0):+8.2f} m
  E: {est.get('y', 0):+8.2f} m
  D: {est.get('z', 0):+8.2f} m

Attitude:
  Roll:  {est.get('roll', 0):+6.1f}°
  Pitch: {est.get('pitch', 0):+6.1f}°
  Yaw:   {est.get('yaw', 0):+6.1f}°

Error: {err_data.get('pos', 0):.3f} m
"""
        info_text.set_text(info_str)
        
        # Auto-scale
        if len(times) > 2:
            ax_pos.set_xlim(max(0, t - 20), t + 2)
            ax_err.set_xlim(max(0, t - 20), t + 2)
        
        return [line_x, line_y, line_z, line_traj_true, line_traj_est, line_err, info_text]
        
    except json.JSONDecodeError:
        return []
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return []

# Run animation
ani = FuncAnimation(fig, update, interval=PLOT_INTERVAL_MS, blit=True, cache_frame_data=False)

plt.tight_layout()
plt.show()
