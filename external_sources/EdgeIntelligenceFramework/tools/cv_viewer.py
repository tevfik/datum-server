#!/usr/bin/env python3
"""
EIF Computer Vision Viewer

Real-time visualization tool for CV demo outputs.

Usage:
    python cv_viewer.py output/              # View all images in folder
    python cv_viewer.py output/ --live       # Auto-refresh on changes
    python cv_viewer.py output/ --web        # Start web server
    python cv_viewer.py image.ppm            # View single image
"""

import sys
import os
import glob
import time
import argparse
from pathlib import Path

try:
    import numpy as np
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False

def read_ppm_pgm(filename):
    """Read PPM (P6) or PGM (P5) image file."""
    with open(filename, 'rb') as f:
        # Read magic number
        magic = f.readline().decode('ascii').strip()
        if magic not in ('P5', 'P6'):
            raise ValueError(f"Unsupported format: {magic}")
        
        # Skip comments
        line = f.readline()
        while line.startswith(b'#'):
            line = f.readline()
        
        # Read dimensions
        width, height = map(int, line.decode('ascii').split())
        
        # Read max value
        maxval = int(f.readline().decode('ascii').strip())
        
        # Read pixel data
        if magic == 'P5':  # Grayscale
            data = np.frombuffer(f.read(width * height), dtype=np.uint8)
            img = data.reshape((height, width))
        else:  # P6 RGB
            data = np.frombuffer(f.read(width * height * 3), dtype=np.uint8)
            img = data.reshape((height, width, 3))
        
        return img

def view_single(filename):
    """View a single image."""
    if not HAS_MATPLOTLIB:
        print("Error: matplotlib not installed. Run: pip install matplotlib numpy")
        sys.exit(1)
    
    img = read_ppm_pgm(filename)
    plt.figure(figsize=(10, 8))
    if len(img.shape) == 2:
        plt.imshow(img, cmap='gray')
    else:
        plt.imshow(img)
    plt.title(os.path.basename(filename))
    plt.axis('off')
    plt.tight_layout()
    plt.show()

def view_folder(folder, live=False, interval=500):
    """View all images in a folder."""
    if not HAS_MATPLOTLIB:
        print("Error: matplotlib not installed. Run: pip install matplotlib numpy")
        sys.exit(1)
    
    def get_images():
        files = glob.glob(os.path.join(folder, '*.ppm')) + \
                glob.glob(os.path.join(folder, '*.pgm'))
        return sorted(files, key=os.path.getmtime)
    
    files = get_images()
    if not files:
        print(f"No PPM/PGM files found in {folder}")
        return
    
    # Grid layout
    n = len(files)
    cols = min(4, n)
    rows = (n + cols - 1) // cols
    
    fig, axes = plt.subplots(rows, cols, figsize=(4*cols, 4*rows))
    if n == 1:
        axes = np.array([[axes]])
    elif rows == 1:
        axes = axes.reshape(1, -1)
    elif cols == 1:
        axes = axes.reshape(-1, 1)
    
    def update(frame=None):
        files = get_images()
        for idx, ax in enumerate(axes.flat):
            ax.clear()
            ax.axis('off')
            if idx < len(files):
                try:
                    img = read_ppm_pgm(files[idx])
                    if len(img.shape) == 2:
                        ax.imshow(img, cmap='gray', vmin=0, vmax=255)
                    else:
                        ax.imshow(img)
                    ax.set_title(os.path.basename(files[idx]), fontsize=8)
                except Exception as e:
                    ax.set_title(f"Error: {e}", fontsize=8)
        plt.tight_layout()
        return axes.flat
    
    update()
    
    if live:
        print(f"Live mode: Watching {folder} for changes (Ctrl+C to exit)")
        ani = FuncAnimation(fig, update, interval=interval, cache_frame_data=False)
        plt.show()
    else:
        plt.show()

def start_web_server(folder, port=8080):
    """Start simple web server with auto-refresh."""
    from http.server import HTTPServer, SimpleHTTPRequestHandler
    import threading
    
    class CVHandler(SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, directory=folder, **kwargs)
        
        def do_GET(self):
            if self.path == '/':
                self.send_response(200)
                self.send_header('Content-type', 'text/html')
                self.end_headers()
                
                files = glob.glob(os.path.join(folder, '*.ppm')) + \
                        glob.glob(os.path.join(folder, '*.pgm'))
                
                html = """<!DOCTYPE html>
<html>
<head>
    <title>EIF CV Viewer</title>
    <meta http-equiv="refresh" content="1">
    <style>
        body { background: #1a1a2e; color: #eee; font-family: monospace; }
        .grid { display: flex; flex-wrap: wrap; gap: 10px; padding: 20px; }
        .img-container { background: #16213e; padding: 10px; border-radius: 8px; }
        img { max-width: 300px; height: auto; }
        h3 { margin: 5px 0; font-size: 12px; }
    </style>
</head>
<body>
    <h1>🖼️ EIF Computer Vision Viewer</h1>
    <div class="grid">
"""
                for f in sorted(files):
                    name = os.path.basename(f)
                    html += f'<div class="img-container"><h3>{name}</h3><img src="{name}"></div>\n'
                
                html += """
    </div>
</body>
</html>"""
                self.wfile.write(html.encode())
            else:
                super().do_GET()
    
    server = HTTPServer(('localhost', port), CVHandler)
    print(f"Starting web server at http://localhost:{port}")
    print("Press Ctrl+C to stop")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nServer stopped")

def main():
    parser = argparse.ArgumentParser(description='EIF Computer Vision Viewer')
    parser.add_argument('path', help='Image file or folder to view')
    parser.add_argument('--live', action='store_true', help='Auto-refresh on changes')
    parser.add_argument('--web', action='store_true', help='Start web server')
    parser.add_argument('--port', type=int, default=8080, help='Web server port')
    parser.add_argument('--interval', type=int, default=500, help='Refresh interval (ms)')
    
    args = parser.parse_args()
    
    if os.path.isfile(args.path):
        view_single(args.path)
    elif os.path.isdir(args.path):
        if args.web:
            start_web_server(args.path, args.port)
        else:
            view_folder(args.path, args.live, args.interval)
    else:
        print(f"Error: {args.path} not found")
        sys.exit(1)

if __name__ == '__main__':
    main()
