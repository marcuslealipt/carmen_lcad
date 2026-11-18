#!/usr/bin/env python3
"""Supervisor do cenário: prontidão IPC, testes com prazo e encerramento dos grupos."""
import argparse
import datetime
import json
import os
import signal
import subprocess
import sys
import time
from pathlib import Path

from prepare import prepare


def terminate_group(process):
    # Os processos são criados por este supervisor com start_new_session=True.
    try:
        os.killpg(process.pid, signal.SIGINT)
    except ProcessLookupError:
        return
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        pass
    # Mesmo se o pai saiu, remove filhos remanescentes do mesmo grupo.
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    process.wait(timeout=3)


def checked(command, timeout, logfile):
    with open(logfile, "ab", buffering=0) as out:
        process = subprocess.Popen(command, stdout=out, stderr=subprocess.STDOUT,
                                   start_new_session=True)
        try:
            code = process.wait(timeout=timeout)
        except BaseException:
            terminate_group(process)
            raise
        if code:
            raise RuntimeError(f"Comando falhou ({code}): {command}; veja {logfile}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=["core", "localized"], default="core")
    parser.add_argument("--gui", action="store_true")
    parser.add_argument("--planner", action="store_true",
                        help="Inicia navigator_ackerman para investigação; não fecha controle")
    parser.add_argument("--smoke", action="store_true", help="Move até 1,2 m, valida e encerra")
    args = parser.parse_args()
    if args.planner and (args.mode != "localized" or args.smoke):
        parser.error("--planner exige --mode localized e não pode disputar comando com --smoke")
    root = Path(os.environ["CARMEN_HOME"])
    os.chdir(root / "bin")
    os.environ["CENTRALHOST"] = "localhost"
    logdir = Path("/logs") / datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    logdir.mkdir(parents=True)
    os.environ["CARMEN_LOG_DIR"] = str(logdir)
    runtime = Path("/run/carmen")
    prepare(root, runtime)
    metadata = json.loads((runtime / "scenario.json").read_text())
    (logdir / "scenario.json").write_text(json.dumps(metadata, indent=2) + "\n")
    (runtime / "mode").write_text(args.mode)
    probe = str(root / "bin/docker_ipc_probe")
    processes, files = [], []

    def launch(name, command):
        out = open(logdir / (name + ".log"), "ab", buffering=0)
        files.append(out)
        process = subprocess.Popen(command, stdout=out, stderr=subprocess.STDOUT,
                                   start_new_session=True)
        processes.append((name, process))
        return process

    def alive():
        for name, process in processes:
            if process.poll() is not None:
                raise RuntimeError(f"{name} terminou: {process.returncode}")

    def readiness(kind):
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            alive()
            try:
                checked([probe, kind], 3, logdir / "readiness.log")
                return
            except (RuntimeError, subprocess.TimeoutExpired):
                time.sleep(0.3)
        raise RuntimeError(f"Timeout aguardando {kind}")

    def stop(signum, frame):
        raise KeyboardInterrupt

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)
    try:
        if args.gui:
            if not os.environ.get("DISPLAY") or not os.environ.get("XAUTHORITY"):
                raise RuntimeError("GUI exige DISPLAY e XAUTHORITY via run-x11.sh")
            checked(["xdpyinfo"], 5, logdir / "x11.log")
        checked(["bash", str(root / "docker/scripts/check-binaries.sh")], 30, logdir / "binaries.log")
        launch("central", ["./central"])
        readiness("ipc")
        launch("param_daemon", ["./param_daemon", str(runtime / "carmen.ini")])
        readiness("params")
        config = (root / "docker/config/process-headless.ini").read_text()
        config = config.replace("@LOCALIZE@", str(int(args.mode == "localized")))
        config = config.replace("@PLANNER@", str(int(args.planner)))
        if args.gui:
            config += (root / "docker/config/process-gui.ini").read_text()
        (runtime / "process.ini").write_text(config)
        (logdir / "process.ini").write_text(config)
        launch("proccontrol", ["./proccontrol", str(runtime / "process.ini")])
        # Prazo de inicialização: admite a primeira compilação de mapas de likelihood.
        deadline = time.monotonic() + 60
        while True:
            alive()
            try:
                x, y, _ = metadata["initial_pose"]
                checked([probe, "initialize", str(x), str(y)], 18, logdir / "initialize.log")
                checked([probe, "observe", args.mode], 15, logdir / "observe.log")
                break
            except (RuntimeError, subprocess.TimeoutExpired):
                if time.monotonic() >= deadline:
                    raise RuntimeError("Cenário sem comunicação estável após prazo de inicialização")
                time.sleep(1)
        alive()
        print(f"Cenário com IPC ativo ({args.mode}). Logs: {logdir}", flush=True)
        if args.smoke:
            checked([probe, "drive", args.mode], 15, logdir / "drive.log")
            checked([probe, "observe", args.mode], 15, logdir / "post-drive.log")
            alive()
            (logdir / "result.json").write_text(json.dumps({"status": "PASS", "mode": args.mode}) + "\n")
            print("PASS: IPC, mapa, processos estáveis, deslocamento e parada verificados.", flush=True)
            return 0
        # O watchdog permanece passivo: nenhum comando de velocidade nesta fase.
        while True:
            alive()
            checked([probe, "observe", args.mode], 15, logdir / "watchdog.log")
    except KeyboardInterrupt:
        print("Encerramento solicitado.", flush=True)
        return 130
    except Exception as error:
        (logdir / "result.json").write_text(json.dumps({"status": "FAIL", "error": str(error)}) + "\n")
        print(f"FAIL: {error}. Logs: {logdir}", file=sys.stderr, flush=True)
        return 1
    finally:
        # Evita uma segunda interrupção abandonar a limpeza.
        signal.signal(signal.SIGINT, signal.SIG_IGN)
        signal.signal(signal.SIGTERM, signal.SIG_IGN)
        for _, process in reversed(processes):
            terminate_group(process)
        for out in files:
            out.close()


if __name__ == "__main__":
    raise SystemExit(main())
