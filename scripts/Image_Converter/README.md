# LVGL Image Converter\

This directory contains two Python scripts for processing images and converting them to LVGL format:

## 1. LVGLImage (LVGLImage.py)

Adapted from the LVGL [official repository](https://github.com/lvgl/lvgl) conversion script[LVGLImage.py](https://github.com/lvgl/lvgl/blob/master/scripts/LVGLImage.py)\

## 2. LVGL Image Converter (lvgl_tools_gui.py)

Uses `LVGLImage.py` to batch-convert images to LVGL format\
Can be used to change Xiaozhi’s default expressions; see the modification tutorial [here](https://www.bilibili.com/video/BV12FQkYeEJ3/)

### Features

- Graphical interface for easier operation
- Supports batch image conversion
- Automatically detects image formats and selects the best color format for conversion
- Supports multiple resolutions

### Usage

Create a virtual environment
```bash
# Create virtual environment
python -m venv venv
# Activate environment
source venv/bin/activate  # Linux/Mac
venv\Scripts\activate      # Windows
```

Install dependencies
```bash
pip install -r requirements.txt
```

Run the conversion tool

```bash
# Activate environment
source venv/bin/activate  # Linux/Mac
venv\Scripts\activate      # Windows
# Run
python lvgl_tools_gui.py
```
