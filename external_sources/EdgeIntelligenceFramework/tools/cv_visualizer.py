#!/usr/bin/env python3
"""
EIF CV Visualizer - Advanced Image Processing Demo Visualization

Features:
- Load PGM/PPM images from EIF demos
- Real-time visualization from JSON annotations
- Interactive histogram with threshold slider
- Multi-image grid comparison
- Side-by-side comparison (original vs processed)
- Overlay keypoints, edges, detections
- Image statistics (mean, std, min, max)
- Export to PNG

Usage:
    # Visualize with histogram panel
    python cv_visualizer.py --image output/edge.pgm --histogram
    
    # Interactive threshold adjustment
    python cv_visualizer.py --image output/edge.pgm --interactive
    
    # Multi-image grid
    python cv_visualizer.py --grid image1.pgm image2.pgm image3.pgm image4.pgm
    
    # Live JSON annotations from demo
    ./bin/edge_detection_demo --json | python cv_visualizer.py --stdin
    
    # Watch directory for new images
    python cv_visualizer.py --watch output/
    
    # Export to PNG
    python cv_visualizer.py --image input.pgm --export output.png

Keyboard shortcuts (interactive mode):
    h       Toggle histogram
    s       Show statistics overlay
    r       Reset view
    t       Toggle threshold slider
    e       Export current view
    q       Quit
"""

import sys
import os
import json
import argparse
import time
from pathlib import Path
from datetime import datetime

try:
    import matplotlib.pyplot as plt
    import matplotlib.patches as patches
    from matplotlib.widgets import Slider, Button
    from matplotlib.animation import FuncAnimation
    import numpy as np
    HAS_PLT = True
except ImportError:
    HAS_PLT = False
    print("Warning: matplotlib not found. Install with: pip install matplotlib numpy")

# ============================================================================
# PGM/PPM Image Loading
# ============================================================================

def load_pgm(filepath):
    """Load PGM (grayscale) image"""
    with open(filepath, 'rb') as f:
        magic = f.readline().decode().strip()
        if magic not in ('P5', 'P2'):
            raise ValueError(f"Not a PGM file: {magic}")
        
        line = f.readline()
        while line.startswith(b'#'):
            line = f.readline()
        
        width, height = map(int, line.decode().split())
        maxval = int(f.readline().decode())
        
        if magic == 'P5':
            data = np.frombuffer(f.read(width * height), dtype=np.uint8)
        else:
            data = np.array([int(x) for x in f.read().split()], dtype=np.uint8)
        
        return data.reshape(height, width)

def load_ppm(filepath):
    """Load PPM (RGB) image"""
    with open(filepath, 'rb') as f:
        magic = f.readline().decode().strip()
        if magic not in ('P6', 'P3'):
            raise ValueError(f"Not a PPM file: {magic}")
        
        line = f.readline()
        while line.startswith(b'#'):
            line = f.readline()
        
        width, height = map(int, line.decode().split())
        maxval = int(f.readline().decode())
        
        if magic == 'P6':
            data = np.frombuffer(f.read(width * height * 3), dtype=np.uint8)
        else:
            data = np.array([int(x) for x in f.read().split()], dtype=np.uint8)
        
        return data.reshape(height, width, 3)

def load_image(filepath):
    """Load PGM or PPM based on extension"""
    ext = Path(filepath).suffix.lower()
    if ext == '.pgm':
        return load_pgm(filepath), 'gray'
    elif ext == '.ppm':
        return load_ppm(filepath), 'rgb'
    else:
        raise ValueError(f"Unsupported format: {ext}")

def compute_stats(img):
    """Compute image statistics"""
    return {
        'mean': np.mean(img),
        'std': np.std(img),
        'min': np.min(img),
        'max': np.max(img),
        'median': np.median(img),
        'shape': img.shape,
        'dtype': str(img.dtype),
        'nonzero': np.count_nonzero(img),
        'zero_pct': 100 * (1 - np.count_nonzero(img) / img.size)
    }

# ============================================================================
# Enhanced Visualization Class
# ============================================================================

class CVVisualizer:
    def __init__(self, mode='image'):
        self.mode = mode
        self.fig = None
        self.ax = None
        self.annotations = []
        self.current_img = None
        self.current_path = None
        self.threshold = 128
        self.show_stats = False
        self.show_hist = False
        
    def show_image(self, filepath, title=None, with_histogram=False):
        """Display single image with optional histogram"""
        img, fmt = load_image(filepath)
        self.current_img = img
        self.current_path = filepath
        
        if with_histogram and fmt == 'gray':
            self._show_with_histogram(img, title or Path(filepath).name)
        else:
            plt.figure(figsize=(10, 8))
            if fmt == 'gray':
                plt.imshow(img, cmap='gray', vmin=0, vmax=255)
            else:
                plt.imshow(img)
            plt.title(title or Path(filepath).name)
            plt.colorbar(label='Pixel Value')
            plt.xlabel('X')
            plt.ylabel('Y')
            self._add_stats_text(img)
            plt.tight_layout()
            plt.show()
    
    def _show_with_histogram(self, img, title):
        """Show image with side histogram panel"""
        fig, (ax_img, ax_hist) = plt.subplots(1, 2, figsize=(14, 6), 
                                               gridspec_kw={'width_ratios': [2, 1]})
        
        # Image panel
        ax_img.imshow(img, cmap='gray', vmin=0, vmax=255)
        ax_img.set_title(title)
        ax_img.set_xlabel('X')
        ax_img.set_ylabel('Y')
        
        # Histogram panel
        hist, bins = np.histogram(img.flatten(), bins=256, range=(0, 256))
        ax_hist.fill_between(range(256), hist, alpha=0.7, color='steelblue')
        ax_hist.set_xlabel('Pixel Value')
        ax_hist.set_ylabel('Frequency')
        ax_hist.set_title('Histogram')
        ax_hist.set_xlim(0, 255)
        
        # Add statistics
        stats = compute_stats(img)
        stats_text = (f"μ={stats['mean']:.1f}  σ={stats['std']:.1f}\n"
                      f"min={stats['min']}  max={stats['max']}\n"
                      f"median={stats['median']:.0f}")
        ax_hist.text(0.95, 0.95, stats_text, transform=ax_hist.transAxes,
                     verticalalignment='top', horizontalalignment='right',
                     fontsize=10, family='monospace',
                     bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))
        
        # Mark mean and median on histogram
        ax_hist.axvline(stats['mean'], color='red', linestyle='--', 
                        label=f"Mean: {stats['mean']:.1f}", linewidth=2)
        ax_hist.axvline(stats['median'], color='green', linestyle=':', 
                        label=f"Median: {stats['median']:.0f}", linewidth=2)
        ax_hist.legend(loc='upper left', fontsize=8)
        
        plt.tight_layout()
        plt.show()
    
    def _add_stats_text(self, img, ax=None):
        """Add statistics overlay to current axes"""
        if ax is None:
            ax = plt.gca()
        stats = compute_stats(img)
        stats_text = (f"Size: {stats['shape'][1]}×{stats['shape'][0]}\n"
                      f"Mean: {stats['mean']:.2f}\n"
                      f"Std: {stats['std']:.2f}\n"
                      f"Range: [{stats['min']}, {stats['max']}]")
        ax.text(0.02, 0.98, stats_text, transform=ax.transAxes,
                verticalalignment='top', fontsize=9, family='monospace',
                bbox=dict(boxstyle='round', facecolor='black', alpha=0.7),
                color='white')
    
    def show_interactive(self, filepath):
        """Interactive viewer with threshold slider and controls"""
        img, fmt = load_image(filepath)
        if fmt != 'gray':
            print("Interactive mode only supports grayscale images")
            return self.show_image(filepath)
        
        self.current_img = img
        self.current_path = filepath
        
        # Create figure with subplots
        fig = plt.figure(figsize=(14, 8))
        gs = fig.add_gridspec(3, 3, height_ratios=[4, 0.3, 0.3], 
                              width_ratios=[2, 1, 0.5])
        
        ax_img = fig.add_subplot(gs[0, 0])
        ax_thresh = fig.add_subplot(gs[0, 1])
        ax_hist = fig.add_subplot(gs[0, 2])
        ax_slider = fig.add_subplot(gs[1, :2])
        ax_buttons = fig.add_subplot(gs[2, :2])
        
        # Original image
        img_display = ax_img.imshow(img, cmap='gray', vmin=0, vmax=255)
        ax_img.set_title(f'{Path(filepath).name} - Original')
        
        # Thresholded image
        thresh_img = (img > self.threshold).astype(np.uint8) * 255
        thresh_display = ax_thresh.imshow(thresh_img, cmap='gray', vmin=0, vmax=255)
        ax_thresh.set_title(f'Threshold > {self.threshold}')
        
        # Histogram
        hist, _ = np.histogram(img.flatten(), bins=256, range=(0, 256))
        ax_hist.barh(range(256), hist, height=1, color='steelblue', alpha=0.7)
        thresh_line = ax_hist.axhline(self.threshold, color='red', linewidth=2)
        ax_hist.set_ylim(0, 255)
        ax_hist.set_xlabel('Count')
        ax_hist.set_ylabel('Value')
        ax_hist.invert_xaxis()
        
        # Threshold slider
        ax_slider.set_facecolor('lightgray')
        slider = Slider(ax_slider, 'Threshold', 0, 255, 
                        valinit=self.threshold, valstep=1)
        
        def update_threshold(val):
            self.threshold = int(val)
            thresh_img = (img > self.threshold).astype(np.uint8) * 255
            thresh_display.set_array(thresh_img)
            ax_thresh.set_title(f'Threshold > {self.threshold} '
                               f'({np.count_nonzero(thresh_img)} px)')
            thresh_line.set_ydata([self.threshold, self.threshold])
            fig.canvas.draw_idle()
        
        slider.on_changed(update_threshold)
        
        # Buttons
        ax_buttons.axis('off')
        btn_positions = [(0.05, 0.2, 0.12, 0.6), (0.2, 0.2, 0.12, 0.6),
                         (0.35, 0.2, 0.12, 0.6), (0.5, 0.2, 0.12, 0.6)]
        
        btn_reset = Button(plt.axes(btn_positions[0]), 'Reset')
        btn_otsu = Button(plt.axes(btn_positions[1]), 'Otsu')
        btn_export = Button(plt.axes(btn_positions[2]), 'Export')
        btn_stats = Button(plt.axes(btn_positions[3]), 'Stats')
        
        def reset_view(event):
            slider.set_val(128)
        
        def auto_otsu(event):
            # Simple Otsu implementation
            hist, _ = np.histogram(img.flatten(), bins=256, range=(0, 256))
            total = img.size
            sum_total = np.sum(np.arange(256) * hist)
            
            best_thresh = 0
            best_var = 0
            sum_bg = 0
            weight_bg = 0
            
            for t in range(256):
                weight_bg += hist[t]
                if weight_bg == 0:
                    continue
                weight_fg = total - weight_bg
                if weight_fg == 0:
                    break
                
                sum_bg += t * hist[t]
                mean_bg = sum_bg / weight_bg
                mean_fg = (sum_total - sum_bg) / weight_fg
                
                var = weight_bg * weight_fg * (mean_bg - mean_fg) ** 2
                if var > best_var:
                    best_var = var
                    best_thresh = t
            
            slider.set_val(best_thresh)
        
        def export_view(event):
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            export_path = f'cv_export_{timestamp}.png'
            fig.savefig(export_path, dpi=150, bbox_inches='tight')
            print(f"Exported to {export_path}")
        
        def show_stats_popup(event):
            stats = compute_stats(img)
            stats_str = '\n'.join([f"{k}: {v}" for k, v in stats.items()])
            print(f"\n=== Image Statistics ===\n{stats_str}\n")
        
        btn_reset.on_clicked(reset_view)
        btn_otsu.on_clicked(auto_otsu)
        btn_export.on_clicked(export_view)
        btn_stats.on_clicked(show_stats_popup)
        
        plt.tight_layout()
        plt.show()
    
    def show_grid(self, filepaths, cols=2):
        """Show multiple images in a grid layout"""
        n = len(filepaths)
        rows = (n + cols - 1) // cols
        
        fig, axes = plt.subplots(rows, cols, figsize=(5*cols, 4*rows))
        axes = np.atleast_2d(axes)
        
        for idx, filepath in enumerate(filepaths):
            row, col = divmod(idx, cols)
            ax = axes[row, col]
            
            try:
                img, fmt = load_image(filepath)
                if fmt == 'gray':
                    ax.imshow(img, cmap='gray', vmin=0, vmax=255)
                else:
                    ax.imshow(img)
                
                stats = compute_stats(img)
                ax.set_title(f'{Path(filepath).name}\n'
                            f'μ={stats["mean"]:.1f} σ={stats["std"]:.1f}',
                            fontsize=9)
            except Exception as e:
                ax.text(0.5, 0.5, f'Error: {e}', ha='center', va='center')
                ax.set_title(Path(filepath).name)
            
            ax.axis('off')
        
        # Hide empty subplots
        for idx in range(n, rows * cols):
            row, col = divmod(idx, cols)
            axes[row, col].axis('off')
        
        plt.suptitle(f'EIF CV Grid View ({n} images)', fontsize=12)
        plt.tight_layout()
        plt.show()
    
    def show_comparison(self, original, processed, titles=None):
        """Show original vs processed side-by-side with difference"""
        fig, axes = plt.subplots(1, 3, figsize=(16, 5))
        
        img1, fmt1 = load_image(original)
        img2, fmt2 = load_image(processed)
        
        # Original
        if fmt1 == 'gray':
            axes[0].imshow(img1, cmap='gray', vmin=0, vmax=255)
        else:
            axes[0].imshow(img1)
        axes[0].set_title(titles[0] if titles else 'Original')
        self._add_stats_text(img1, axes[0])
        
        # Processed
        if fmt2 == 'gray':
            axes[1].imshow(img2, cmap='gray', vmin=0, vmax=255)
        else:
            axes[1].imshow(img2)
        axes[1].set_title(titles[1] if titles else 'Processed')
        self._add_stats_text(img2, axes[1])
        
        # Difference (for grayscale)
        if fmt1 == 'gray' and fmt2 == 'gray' and img1.shape == img2.shape:
            diff = np.abs(img1.astype(np.int16) - img2.astype(np.int16))
            im_diff = axes[2].imshow(diff, cmap='hot', vmin=0, vmax=255)
            axes[2].set_title(f'Difference (Σ={np.sum(diff):,})')
            plt.colorbar(im_diff, ax=axes[2], label='|Δ|')
        else:
            axes[2].text(0.5, 0.5, 'N/A\n(size mismatch)', 
                        ha='center', va='center', fontsize=12)
            axes[2].set_title('Difference')
        
        for ax in axes:
            ax.axis('off')
        
        plt.tight_layout()
        plt.show()
    
    def show_with_annotations(self, filepath, annotations):
        """Show image with overlaid annotations (rects, points, edges)"""
        img, fmt = load_image(filepath)
        
        fig, ax = plt.subplots(figsize=(10, 8))
        
        if fmt == 'gray':
            ax.imshow(img, cmap='gray')
        else:
            ax.imshow(img)
        
        colors = {'rect': 'red', 'point': 'lime', 'edge': 'cyan', 
                  'circle': 'yellow', 'line': 'orange'}
        
        for ann in annotations:
            ann_type = ann.get('type')
            color = ann.get('color', colors.get(ann_type, 'red'))
            
            if ann_type == 'rect':
                rect = patches.Rectangle(
                    (ann['x'], ann['y']), ann['w'], ann['h'],
                    linewidth=2, edgecolor=color, facecolor='none'
                )
                ax.add_patch(rect)
                if 'label' in ann:
                    ax.text(ann['x'], ann['y'] - 5, 
                           f"{ann['label']} ({ann.get('score', 0):.2f})",
                           color=color, fontsize=10, weight='bold')
            
            elif ann_type == 'point':
                ax.plot(ann['x'], ann['y'], 'o', color=color, 
                       markersize=ann.get('size', 8))
                if 'label' in ann:
                    ax.annotate(ann['label'], (ann['x'], ann['y']),
                               xytext=(5, 5), textcoords='offset points',
                               color=color, fontsize=8)
            
            elif ann_type == 'edge':
                strength = ann.get('strength', 1.0)
                ax.plot([ann['x1'], ann['x2']], [ann['y1'], ann['y2']], 
                       '-', color=color, linewidth=max(1, strength * 3),
                       alpha=min(1.0, 0.3 + strength * 0.7))
            
            elif ann_type == 'circle':
                circle = patches.Circle(
                    (ann['x'], ann['y']), ann['r'],
                    linewidth=2, edgecolor=color, facecolor='none'
                )
                ax.add_patch(circle)
            
            elif ann_type == 'line':
                ax.plot([ann['x1'], ann['x2']], [ann['y1'], ann['y2']], 
                       '-', color=color, linewidth=2)
        
        # Legend for annotation types
        ann_types = set(a.get('type') for a in annotations)
        legend_patches = [patches.Patch(color=colors.get(t, 'red'), label=t) 
                         for t in ann_types if t]
        if legend_patches:
            ax.legend(handles=legend_patches, loc='upper right')
        
        ax.set_title(f'{Path(filepath).name} - {len(annotations)} annotations')
        plt.tight_layout()
        plt.show()
    
    def watch_directory(self, directory, interval=1.0):
        """Watch directory for new images and display them"""
        seen = set()
        print(f"Watching {directory} for new images... (Ctrl+C to stop)")
        print("  [h] Show histogram  [s] Stats  [q] Quit")
        
        try:
            while True:
                for ext in ['*.pgm', '*.ppm']:
                    for f in Path(directory).glob(ext):
                        if f not in seen:
                            seen.add(f)
                            print(f"\n📷 New image: {f}")
                            try:
                                self.show_image(str(f), f.name, with_histogram=True)
                            except Exception as e:
                                print(f"❌ Error loading {f}: {e}")
                time.sleep(interval)
        except KeyboardInterrupt:
            print("\n✓ Stopped watching.")
    
    def live_json(self):
        """Real-time JSON annotation stream from stdin"""
        print("Reading JSON annotations from stdin...")
        print("Expected format: {\"image\": \"path.pgm\", \"type\": \"point\", \"x\": 10, \"y\": 20}")
        
        plt.ion()
        fig, (ax_img, ax_hist) = plt.subplots(1, 2, figsize=(14, 6),
                                               gridspec_kw={'width_ratios': [2, 1]})
        
        img_data = np.zeros((100, 100), dtype=np.uint8)
        img_handle = ax_img.imshow(img_data, cmap='gray')
        ax_img.set_title('Waiting for data...')
        
        ax_hist.set_xlim(0, 255)
        ax_hist.set_title('Histogram')
        hist_bars = ax_hist.bar(range(256), np.zeros(256), width=1, 
                                color='steelblue', alpha=0.7)
        
        plt.tight_layout()
        plt.show()
        
        annotations = []
        
        for line in sys.stdin:
            try:
                data = json.loads(line.strip())
                
                if 'image' in data:
                    img, fmt = load_image(data['image'])
                    img_handle.set_array(img)
                    ax_img.set_xlim(0, img.shape[1])
                    ax_img.set_ylim(img.shape[0], 0)
                    
                    # Update histogram
                    if fmt == 'gray':
                        hist, _ = np.histogram(img.flatten(), bins=256, range=(0, 256))
                        for bar, h in zip(hist_bars, hist):
                            bar.set_height(h)
                        ax_hist.set_ylim(0, max(hist) * 1.1)
                
                if 'type' in data:
                    annotations.append(data)
                    ax_img.set_title(f'{len(annotations)} annotations')
                    
                    if data['type'] == 'point':
                        ax_img.plot(data['x'], data['y'], 'ro', markersize=5)
                    elif data['type'] == 'rect':
                        rect = patches.Rectangle(
                            (data['x'], data['y']), data['w'], data['h'],
                            linewidth=1, edgecolor='red', facecolor='none'
                        )
                        ax_img.add_patch(rect)
                    elif data['type'] == 'edge':
                        ax_img.plot([data['x1'], data['x2']], 
                                   [data['y1'], data['y2']], 'g-', linewidth=1)
                
                fig.canvas.draw()
                fig.canvas.flush_events()
                
            except json.JSONDecodeError:
                pass
            except Exception as e:
                print(f"Error: {e}", file=sys.stderr)
        
        plt.ioff()
        plt.show()
    
    def export_image(self, filepath, output, dpi=150):
        """Export visualization to PNG"""
        img, fmt = load_image(filepath)
        
        fig, ax = plt.subplots(figsize=(10, 8))
        if fmt == 'gray':
            ax.imshow(img, cmap='gray', vmin=0, vmax=255)
        else:
            ax.imshow(img)
        ax.set_title(Path(filepath).name)
        ax.axis('off')
        
        fig.savefig(output, dpi=dpi, bbox_inches='tight', 
                    facecolor='white', edgecolor='none')
        plt.close(fig)
        print(f"✓ Exported to {output}")

# ============================================================================
# Edge Detection Specific Visualization
# ============================================================================

def visualize_edges(input_img, edge_img):
    """Specialized visualization for edge detection with overlay"""
    orig, _ = load_image(input_img)
    edges, _ = load_image(edge_img)
    
    fig, axes = plt.subplots(2, 2, figsize=(12, 10))
    
    # Original
    axes[0, 0].imshow(orig, cmap='gray', vmin=0, vmax=255)
    axes[0, 0].set_title('Original')
    
    # Edges
    axes[0, 1].imshow(edges, cmap='gray', vmin=0, vmax=255)
    axes[0, 1].set_title(f'Edges ({np.count_nonzero(edges > 0):,} edge pixels)')
    
    # Overlay (edges in red on original)
    overlay = np.stack([orig, orig, orig], axis=-1)
    edge_mask = edges > 127
    overlay[edge_mask] = [255, 50, 50]
    axes[1, 0].imshow(overlay)
    axes[1, 0].set_title('Edge Overlay (Red)')
    
    # Edge strength heatmap
    im = axes[1, 1].imshow(edges, cmap='hot', vmin=0, vmax=255)
    axes[1, 1].set_title('Edge Strength Heatmap')
    plt.colorbar(im, ax=axes[1, 1], label='Magnitude')
    
    for ax in axes.flat:
        ax.axis('off')
    
    # Statistics
    stats = compute_stats(edges)
    fig.text(0.02, 0.02, 
             f"Edge stats: mean={stats['mean']:.1f}, std={stats['std']:.1f}, "
             f"nonzero={stats['nonzero']:,} ({100-stats['zero_pct']:.1f}%)",
             fontsize=10, family='monospace')
    
    plt.tight_layout()
    plt.show()

# ============================================================================
# CLI
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='EIF CV Visualizer - Advanced Image Processing Visualization',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --image edges.pgm                    # View single image
  %(prog)s --image edges.pgm --histogram        # View with histogram
  %(prog)s --image edges.pgm --interactive      # Interactive mode
  %(prog)s --grid img1.pgm img2.pgm img3.pgm    # Grid view
  %(prog)s --compare original.pgm edges.pgm     # Side-by-side
  %(prog)s --watch ./output/                    # Watch directory
  %(prog)s --stdin                              # Live JSON from stdin
        """)
    
    parser.add_argument('--image', type=str, help='Single image file')
    parser.add_argument('--histogram', '-H', action='store_true', 
                        help='Show histogram panel')
    parser.add_argument('--interactive', '-i', action='store_true',
                        help='Interactive mode with threshold slider')
    parser.add_argument('--compare', nargs=2, metavar=('ORIG', 'PROC'),
                        help='Compare two images with difference')
    parser.add_argument('--grid', nargs='+', metavar='IMG',
                        help='Show multiple images in grid')
    parser.add_argument('--watch', type=str, metavar='DIR',
                        help='Watch directory for new images')
    parser.add_argument('--stdin', action='store_true', 
                        help='Read JSON annotations from stdin')
    parser.add_argument('--edges', nargs=2, metavar=('INPUT', 'EDGES'),
                        help='Edge detection visualization')
    parser.add_argument('--export', type=str, metavar='OUTPUT',
                        help='Export to PNG file')
    parser.add_argument('--cols', type=int, default=2,
                        help='Columns for grid view (default: 2)')
    
    args = parser.parse_args()
    
    if not HAS_PLT:
        print("matplotlib required. Install with: pip install matplotlib numpy")
        return 1
    
    viz = CVVisualizer()
    
    if args.image:
        if args.export:
            viz.export_image(args.image, args.export)
        elif args.interactive:
            viz.show_interactive(args.image)
        elif args.histogram:
            viz.show_image(args.image, with_histogram=True)
        else:
            viz.show_image(args.image)
    elif args.compare:
        viz.show_comparison(args.compare[0], args.compare[1])
    elif args.grid:
        viz.show_grid(args.grid, cols=args.cols)
    elif args.watch:
        viz.watch_directory(args.watch)
    elif args.stdin:
        viz.live_json()
    elif args.edges:
        visualize_edges(args.edges[0], args.edges[1])
    else:
        parser.print_help()
    
    return 0

if __name__ == '__main__':
    sys.exit(main())
