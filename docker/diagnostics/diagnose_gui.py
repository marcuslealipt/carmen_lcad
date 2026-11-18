#!/usr/bin/env python3
"""Executa uma única GUI sobre o core e simboliza sua falha, sem ptrace ou root."""
import datetime
import os
from pathlib import Path
import re
import signal
import struct
import subprocess
import sys
import time


def symbolize(text):
    entries = re.findall(r"CARMEN_FRAME path=(.*?) offset=(0x[0-9a-f]+) address=(0x[0-9a-f]+)", text)
    result = []
    for path, offset, address in entries:
        binary = Path(path)
        try:
            with binary.open("rb") as source:
                header = source.read(18)
            if header[:4] != b"\x7fELF" or header[5] != 1:
                continue
            elf_type = struct.unpack_from("<H", header, 16)[0]
            # PIE/shared objects usam endereço relativo; ET_EXEC usa o absoluto.
            target = offset if elf_type == 3 else address
            output = subprocess.run(["addr2line", "-f", "-C", "-i", "-e", str(binary), target],
                                    capture_output=True, text=True, timeout=3, check=True)
            result.append(f"{path} {target}\n{output.stdout.rstrip()}")
        except (OSError, subprocess.SubprocessError) as error:
            result.append(f"{path}: simbolização indisponível: {error}")
    return "\n\n".join(result)


def main():
    root = Path(os.environ["CARMEN_HOME"])
    os.chdir(root / "bin")
    sys.path.insert(0, str(root / "docker/scripts"))
    from run import terminate_group

    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    logs = Path("/logs") / ("gui-diagnostic-" + stamp)
    logs.mkdir()
    print(f"Diagnóstico de uma única GUI. Logs: {logs}", flush=True)
    core = gui = None
    code = 1
    interrupted = False

    def stop(signum, frame):
        raise KeyboardInterrupt

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)
    try:
        subprocess.run(["xdpyinfo"], stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                       check=True, timeout=5)
        library = logs / "gui_trace.so"
        subprocess.run(["cc", "-shared", "-fPIC", "-g", "-O0", "-Wall", "-Wextra", "-Werror",
                        "-o", str(library), str(Path(__file__).with_name("gui_trace.c")), "-ldl"],
                       check=True, timeout=30)
        with (logs / "core.log").open("wb", buffering=0) as output:
            core = subprocess.Popen([sys.executable, str(root / "docker/scripts/run.py"),
                                     "--mode", "core"], stdout=output, stderr=subprocess.STDOUT,
                                    start_new_session=True)
            deadline = time.monotonic() + 75
            while "Cenário com IPC ativo (core)" not in (logs / "core.log").read_text(errors="replace"):
                if core.poll() is not None:
                    raise RuntimeError(f"Core terminou antes da GUI: código {core.returncode}")
                if time.monotonic() >= deadline:
                    raise RuntimeError("Core sem prontidão em 75 s; GUI não iniciada")
                time.sleep(0.2)

            env = os.environ.copy()
            env["LD_PRELOAD"] = str(library)
            print("Core pronto. Observando a GUI por até 30 s, sem enviar movimento.", flush=True)
            with (logs / "navigator_gui.log").open("wb", buffering=0) as output_gui:
                gui = subprocess.Popen(["./navigator_gui2"], stdout=output_gui,
                                       stderr=subprocess.STDOUT, env=env, start_new_session=True)
                deadline = time.monotonic() + 30
                while gui.poll() is None and time.monotonic() < deadline:
                    if core.poll() is not None:
                        raise RuntimeError(f"Core terminou durante diagnóstico: {core.returncode}")
                    time.sleep(0.1)
                if gui.poll() is None:
                    print("GUI sem queda em 30 s. Isto ainda não valida desenho ou navegação.")
                    code = 124
                else:
                    print(f"GUI terminou: código {gui.returncode} (negativo indica sinal).")
                    code = 128 - gui.returncode if gui.returncode < 0 else (gui.returncode or 1)
    except KeyboardInterrupt:
        interrupted = True
        code = 130
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"Diagnóstico interrompido: {error}", file=sys.stderr)
    finally:
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        for process in (gui, core):
            if process is not None:
                terminate_group(process)
        sections = []
        for name in ("core.log", "navigator_gui.log"):
            path = logs / name
            if path.exists():
                text = path.read_text(errors="replace")
                sections.append(f"===== {name} =====\n" + "\n".join(text.splitlines()[-120:]))
                if name == "navigator_gui.log":
                    sections.append("===== Funções e linhas =====\n" + symbolize(text))
        report = "\n\n".join(sections) + f"\n\nCódigo do diagnóstico: {code}\n"
        (logs / "report.txt").write_text(report)
        print(report, flush=True)
        print(f"Relatório: {logs}/report.txt", flush=True)
        if interrupted:
            print("Encerramento solicitado; processos de diagnóstico removidos.")
    return code


if __name__ == "__main__":
    raise SystemExit(main())
