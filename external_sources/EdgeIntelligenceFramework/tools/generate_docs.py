#!/usr/bin/env python3
import os
import markdown
import glob
from jinja2 import Environment, FileSystemLoader

# Configuration
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DOCS_DIR = os.path.join(PROJECT_ROOT, "docs")
SITE_DIR = os.path.join(DOCS_DIR, "site")
TEMPLATES_DIR = os.path.join(DOCS_DIR, "templates")

# Setup Jinja2
env = Environment(loader=FileSystemLoader(TEMPLATES_DIR))
template = env.get_template("base.html")

# Navigation Structure (Ordered)
nav = {
    "Artificial Intelligence": [],
    "Machine Learning": [],
    "Computer Vision": [],
    "Signal Processing": [], # Maps to dsp_demos
    "Natural Language Processing": [],
    "Sensor Fusion & Filters": [],
    "Edge Learning": [],
    "Data Analysis": [],
    "Bayesian Filters": [],
    "Hardware Demos": [],
    "Example Projects": [],
    "Tools": [],
    "General": []
}

def ensure_dir(path):
    if not os.path.exists(path):
        os.makedirs(path)

def render_page(title, content, filename):
    output = template.render(
        nav=nav,
        content=content,
        title=title
    )
    with open(os.path.join(SITE_DIR, filename), "w") as f:
        f.write(output)

def process_markdown_file(path, category=None):
    with open(path, "r") as f:
        text = f.read()
    
    # Simple Title Extraction
    title = os.path.basename(path).replace(".md", "").replace("_", " ").title()
    for line in text.split("\n"):
        if line.startswith("# "):
            title = line.replace("# ", "").strip()
            break
            
    html = markdown.markdown(text, extensions=['fenced_code', 'tables'])
    
    # Determine Output Filename
    filename = os.path.basename(path)
    if filename == "README.md":
        # Use parent folder name: examples/dsp_demos/audio_eq/README.md -> audio_eq
        parent_dir = os.path.basename(os.path.dirname(path))
        if parent_dir == "examples": # Root examples README
             name = "examples_overview"
        elif parent_dir == "docs": # Root docs README
             name = "index"
        else:
             name = parent_dir
    else:
        name = filename.replace(".md", "")
        
    # Check for name collisions (simple appendices)
    safe_name = name + ".html"
    
    # Decide output subdir
    if category and category != "General":
        out_subdir = "tutorials"
        safe_name = f"{out_subdir}/{safe_name}"
        ensure_dir(os.path.join(SITE_DIR, out_subdir))

    # Special case: Make sure index.html stays in root
    if name == "index":
        safe_name = "index.html"

    # Add to Nav if category provided
    if category:
        # Check against duplicates
        exists = any(item['url'] == safe_name for item in nav[category])
        if not exists:
            nav[category].append({"title": title, "url": safe_name})
    
    return title, html, safe_name

def get_category(root_path):
    if "ai_demos" in root_path: return "Artificial Intelligence"
    if "ml_demos" in root_path: return "Machine Learning"
    if "cv_demos" in root_path: return "Computer Vision"
    if "dsp_demos" in root_path: return "Signal Processing"
    if "nlp_demos" in root_path: return "Natural Language Processing"
    if "filter_demos" in root_path: return "Sensor Fusion & Filters"
    if "el_demos" in root_path: return "Edge Learning"
    if "da_demos" in root_path: return "Data Analysis"
    if "bf_demos" in root_path: return "Bayesian Filters"
    if "hw_demos" in root_path: return "Hardware Demos"
    if "projects" in root_path: return "Example Projects"
    if "common" in root_path: return "Tools"
    return "General"

def main():
    print(f"Generating docs to {SITE_DIR}...")
    ensure_dir(SITE_DIR)
    
    # 1. Process Examples FIRST to populate Nav
    examples_dir = os.path.join(PROJECT_ROOT, "examples")
    for root, dirs, files in os.walk(examples_dir):
        if "README.md" in files:
            path = os.path.join(root, "README.md")
            category = get_category(root)
            # Just populate Nav, don't render yet (or render tmp)
            title, html, fname = process_markdown_file(path, category)

    # 2. Process Core Docs to populate Nav
    for root, dirs, files in os.walk(DOCS_DIR):
        if root == DOCS_DIR: # Skip root docs if we want only subdirs, or handle them as General
            pass 
        if "site" in root or "templates" in root: continue

        for file in files:
            if not file.endswith(".md") or "TODO" in file: continue
            
            path = os.path.join(root, file)
            rel_dir = os.path.relpath(root, DOCS_DIR)
            
            # Map subdirectories to Categories
            if rel_dir == ".":
                category = "General"
            elif rel_dir.startswith("guides"):
                category = "Guides"
            elif rel_dir.startswith("manual"):
                category = "Manual"
            elif rel_dir.startswith("reference"):
                category = "Reference"
            elif rel_dir.startswith("tutorials"):
                category = "Tutorials"
            elif rel_dir.startswith("reports"):
                category = "Reports"
            elif rel_dir.startswith("meta"):
                category = "Meta"
            else:
                category = "General"

            # Add to Nav if not exists
            if category not in nav: nav[category] = []
            
            # Process to get title/url
            title, html, fname = process_markdown_file(path, category)

    # 3. RENDER ALL pages now that Nav is full
    print("Rendering pages with full navigation...")
    
    # Render Examples
    for root, dirs, files in os.walk(examples_dir):
        if "README.md" in files:
            path = os.path.join(root, "README.md")
            category = get_category(root)
            title, html, fname = process_markdown_file(path, category)
            render_page(title, html, fname)

    # Render Core Docs
    for root, dirs, files in os.walk(DOCS_DIR):
        if "site" in root or "templates" in root: continue
        for file in files:
            if not file.endswith(".md") or "TODO" in file: continue
            path = os.path.join(root, file)
            # Re-derive category or assume handled
            title, html, fname = process_markdown_file(path)
            render_page(title, html, fname)
            if "README" in file or "index" in file:
                 render_page(title, html, "index.html")

    print(f"Documentation generation complete. Processed {sum(len(v) for v in nav.values())} pages.")

if __name__ == "__main__":
    main()
