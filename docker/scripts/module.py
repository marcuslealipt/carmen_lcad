#!/usr/bin/env python3
"""Preserva stdout/stderr de cada módulo, inclusive falhas capturadas pelo proccontrol."""
import os
import re
import sys
from pathlib import Path

if len(sys.argv) < 3 or not re.fullmatch(r"[a-z_]+", sys.argv[1]):
    raise SystemExit(2)
path = Path(os.environ["CARMEN_LOG_DIR"]) / (sys.argv[1] + ".log")
fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0o600)
os.dup2(fd, 1)
os.dup2(fd, 2)
os.close(fd)
os.execv(sys.argv[2], sys.argv[2:])
