#!/usr/bin/env python3
"""Verifica captura e simbolização de SIGSEGV real, sem exigir GTK ou Docker."""
import importlib.util
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("diagnose_gui", ROOT / "docker/diagnostics/diagnose_gui.py")
diagnose = importlib.util.module_from_spec(spec)
spec.loader.exec_module(diagnose)


class TraceTests(unittest.TestCase):
    def test_real_crash_with_pie_and_fixed_address(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            library = path / "trace.so"
            subprocess.run(["cc", "-shared", "-fPIC", "-g", "-Wall", "-Wextra", "-Werror",
                            str(ROOT / "docker/diagnostics/gui_trace.c"), "-o", str(library), "-ldl"],
                           check=True, timeout=30)
            source = path / "intentional_crash.c"
            source.write_text("__attribute__((noinline)) void crash_here(void) {\n"
                              "  volatile int *p = (int *)0; *p = 1;\n"
                              "}\nint main(void) { crash_here(); return 0; }\n")
            for linkmode in ("-pie", "-no-pie"):
                with self.subTest(linkmode=linkmode):
                    binary = path / ("test" + linkmode)
                    subprocess.run(["cc", "-g", "-O0", "-fno-omit-frame-pointer", linkmode,
                                    str(source), "-o", str(binary)], check=True, timeout=30)
                    result = subprocess.run([str(binary)], env={**os.environ, "LD_PRELOAD": str(library)},
                                            capture_output=True, text=True, timeout=5)
                    self.assertEqual(result.returncode, -signal.SIGSEGV)
                    self.assertIn("CARMEN_GUI_CRASH signal=11", result.stderr)
                    decoded = diagnose.symbolize(result.stderr)
                    self.assertIn("crash_here", decoded)
                    self.assertIn("intentional_crash.c:2", decoded)

    def test_normal_exit_is_not_reported_as_crash(self):
        with tempfile.TemporaryDirectory() as directory:
            library = Path(directory) / "trace.so"
            subprocess.run(["cc", "-shared", "-fPIC", str(ROOT / "docker/diagnostics/gui_trace.c"),
                            "-o", str(library), "-ldl"], check=True, timeout=30)
            result = subprocess.run(["/bin/true"], env={**os.environ, "LD_PRELOAD": str(library)},
                                    capture_output=True, text=True, timeout=5)
            self.assertEqual(result.returncode, 0)
            self.assertNotIn("CARMEN_GUI_CRASH", result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
