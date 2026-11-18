#!/usr/bin/env python3
"""Testes locais independentes de Docker. Não substituem o smoke IPC."""
import importlib.util
import math
import os
import signal
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "docker/scripts"))
import prepare
import run


class MapTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "test.map"

    def legacy(self, resolution=0.1, cells=None):
        cells = cells if cells is not None else [-1.0, 0.0, 0.5, 1.0]
        payload = b"GRIDMAP   " + struct.pack("<iif4f", 2, 2, resolution, *cells)
        self.path.write_bytes(b"# mapa de teste\n" + prepare.LABEL + struct.pack("<Bi", 1, len(payload)) + payload)

    def test_float32_conversion_preserves_values(self):
        self.legacy()
        nx, ny, r, cells, width = prepare.read_map(self.path)
        self.assertEqual(width, 4)
        prepare.write_map(self.path, nx, ny, r, cells)
        a, b, s, converted, width = prepare.read_map(self.path)
        self.assertEqual((nx, ny, r, cells), (a, b, s, converted))
        self.assertEqual(width, 8)

    def test_truncated_map_rejected(self):
        self.legacy()
        self.path.write_bytes(self.path.read_bytes()[:-2])
        with self.assertRaises(ValueError):
            prepare.read_map(self.path)

    def test_nan_resolution_rejected(self):
        self.legacy(resolution=math.nan)
        with self.assertRaises(ValueError):
            prepare.read_map(self.path)

    def test_invalid_occupancy_rejected(self):
        self.legacy(cells=[0, 0, math.inf, 0])
        with self.assertRaises(ValueError):
            prepare.read_map(self.path)

    def test_unknown_map_not_drivable(self):
        with self.assertRaises(ValueError):
            prepare.initial_pose(100, 100, 0.1, [-1] * 10000)

    def test_wrong_grid_payload_rejected(self):
        self.legacy()
        data = bytearray(self.path.read_bytes())
        start = data.index(prepare.LABEL) + len(prepare.LABEL)
        struct.pack_into("<i", data, start + 15, 3)
        self.path.write_bytes(data)
        with self.assertRaises(ValueError):
            prepare.read_map(self.path)

    def test_real_map_conversion_and_free_corridor(self):
        original = ROOT / "data/mapa_do_ctvii_500k_64.map"
        before = original.read_bytes()
        nx, ny, r, cells, width = prepare.read_map(original)
        self.assertEqual((nx, ny, width), (237, 210, 4))
        pose = prepare.initial_pose(nx, ny, r, cells)
        self.assertAlmostEqual(pose[0], 1.6, places=5)
        self.assertAlmostEqual(pose[1], 5.4, places=5)
        prepare.write_map(self.path, nx, ny, r, cells)
        self.assertEqual(prepare.read_map(self.path)[:4], (nx, ny, r, cells))
        self.assertEqual(before, original.read_bytes())

    def test_generated_parameters_preserve_sources(self):
        base = ROOT / "src/carmen-ford-escape.ini"
        before = base.read_bytes()
        prepare.prepare(ROOT, Path(self.temp.name))
        values = prepare.parameters(Path(self.temp.name) / "carmen.ini")
        self.assertEqual(values["simulator_use_velocity_nn"], "off")
        self.assertEqual(values["simulator_use_phi_nn"], "off")
        self.assertEqual(values["localize_ackerman_prediction_type"], "1")
        self.assertEqual(values["robot_publish_odometry"], "1")
        self.assertAlmostEqual(float(values["robot_length"]), 0.75)
        self.assertIn("robot_PID_steering_kp", values)
        self.assertEqual(before, base.read_bytes())


class SupervisorTests(unittest.TestCase):
    def test_nonzero_child_is_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(RuntimeError):
                run.checked([sys.executable, "-c", "raise SystemExit(9)"], 2,
                            Path(directory) / "failure.log")

    def test_timeout_is_failure_and_child_terminated(self):
        with tempfile.TemporaryDirectory() as directory:
            pidpath = Path(directory) / "pid"
            code = "import os,time,pathlib; pathlib.Path(%r).write_text(str(os.getpid())); time.sleep(60)" % str(pidpath)
            with self.assertRaises(subprocess.TimeoutExpired):
                run.checked([sys.executable, "-c", code], 0.5, Path(directory) / "timeout.log")
            with self.assertRaises(ProcessLookupError):
                os.kill(int(pidpath.read_text()), 0)

    def test_successful_child_returns(self):
        with tempfile.TemporaryDirectory() as directory:
            run.checked([sys.executable, "-c", "print('ok')"], 2, Path(directory) / "success.log")


if __name__ == "__main__":
    unittest.main(verbosity=2)
