#!/usr/bin/env python3
"""Executa a lógica C++ real de startup; IPC/instrumentos substituídos por sentinelas.

Não testa GTK/OpenGL. Detecta assinatura/callback antes da criação dos instrumentos.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


class PanelStartupTests(unittest.TestCase):
    def test_startup_before_ipc_with_and_without_arguments(self):
        panel = (ROOT / "src/car_panel_gui/panel.cpp").read_text()
        constructor = (ROOT / "src/car_panel_gui/car_panel.cpp").read_text()
        cpp = r'''
#include <cassert>
#include <cstdlib>
#include <cstring>
static int initialized, subscriptions, selected = -1, use_truepos_local;
enum { CARMEN_PARAM_ONOFF };
struct carmen_param_t { char *module; char *name; int type; int *value; int flag; void *callback; };
void initializeComponents() { assert(!initialized); initialized = 1; }
void carmen_param_check_version(char *) { assert(initialized); }
void carmen_param_install_params(int, char **, carmen_param_t *, int) { assert(initialized); }
void subscribe_messages(int type) {
    // Simula despacho imediato do callback durante a assinatura IPC.
    assert(initialized); ++subscriptions; selected = type;
}
class car_panel { public: car_panel(int, char **); };
'''
        cpp += "\nint\n" + function(panel, "checkArguments(int argc, char *argv[])")
        cpp += "\n" + function(constructor, "car_panel::car_panel(int argc, char *argv[])")
        cpp += r'''
int main(int argc, char **argv) {
    car_panel panel(argc, argv);
    assert(initialized == 1 && subscriptions == 1);
    assert(selected == ((argc == 3) ? atoi(argv[2]) : 3));
}
'''
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "startup.cpp"
            executable = Path(directory) / "startup"
            source.write_text(cpp)
            subprocess.run(["g++", "-std=c++14", "-Wall", "-Wextra", "-Werror",
                            str(source), "-o", str(executable)], check=True, timeout=30)
            for args in ([], ["-type_msg", "0"], ["-type_msg", "3"], ["-type_msg", "4"], ["-unused"]):
                with self.subTest(args=args):
                    subprocess.run([str(executable), *args], check=True, timeout=3)


if __name__ == "__main__":
    unittest.main(verbosity=2)
