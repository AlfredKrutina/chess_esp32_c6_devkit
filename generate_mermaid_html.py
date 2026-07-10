#!/usr/bin/env python3
"""Kořenový launcher — skutečný skript: scripts/docs/generate_mermaid_html.py"""
import os
import runpy
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
os.chdir(ROOT)
sys.argv[0] = str(ROOT / "scripts" / "docs" / "generate_mermaid_html.py")
runpy.run_path(str(ROOT / "scripts" / "docs" / "generate_mermaid_html.py"), run_name="__main__")
