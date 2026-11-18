#!/usr/bin/env python3
"""Reinicializa a pose e executa um único movimento limitado, no container isolado."""
import json
import os
import subprocess
from pathlib import Path

root = Path(os.environ["CARMEN_HOME"])
scenario = json.loads(Path("/run/carmen/scenario.json").read_text())
mode = Path("/run/carmen/mode").read_text().strip()
if "navigator planning 1" in Path("/run/carmen/process.ini").read_text():
    raise SystemExit("Pare o perfil com --planner antes de usar a demonstração direta.")
probe = str(root / "bin/docker_ipc_probe")
x, y, _ = scenario["initial_pose"]
for args, timeout in [(["initialize", str(x), str(y)], 18),
                      (["observe", mode], 15), (["drive", mode], 15)]:
    subprocess.run([probe, *args], check=True, timeout=timeout)
