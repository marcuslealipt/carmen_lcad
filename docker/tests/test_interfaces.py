#!/usr/bin/env python3
"""Testa interfaces contra central real, com prazo, logs e porta dedicada."""
import argparse
import os
import socket
import subprocess
import time
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--central", type=Path, required=True)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--logs", type=Path, required=True)
    args = parser.parse_args()
    args.logs.mkdir(parents=True, exist_ok=True)
    probe = str(args.probe.resolve())
    refusal = subprocess.run([probe, "--unsupported-trailer"], capture_output=True,
                             text=True, timeout=5)
    (args.logs / "unsupported-trailer.log").write_text(refusal.stdout + refusal.stderr)
    if refusal.returncode != 1 or "semi_trailer_type=1" not in refusal.stderr:
        raise RuntimeError("Modelo não suportado não foi recusado corretamente")
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    env = dict(os.environ, CENTRALHOST=f"localhost:{port}")
    with (args.logs / "central.log").open("wb") as log:
        central = subprocess.Popen([str(args.central.resolve()), f"-p{port}"],
                                   stdin=subprocess.DEVNULL, stdout=log,
                                   stderr=subprocess.STDOUT, env=env)
        try:
            deadline = time.monotonic() + 5
            while True:
                if central.poll() is not None:
                    raise RuntimeError("central terminou antes do teste")
                try:
                    with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                        break
                except OSError:
                    if time.monotonic() >= deadline:
                        raise RuntimeError("Timeout aguardando central")
                    time.sleep(0.05)
            result = subprocess.run([probe], env=env, capture_output=True, text=True,
                                    timeout=10)
            (args.logs / "roundtrip.log").write_text(result.stdout + result.stderr)
            if result.returncode or "PASS: IPC roundtrip" not in result.stdout:
                raise RuntimeError(f"Roundtrip falhou ({result.returncode}); veja {args.logs}")
            if central.poll() is not None:
                raise RuntimeError("central terminou durante o teste")
            print(result.stdout.strip())
            print("PASS: tipo de reboque não suportado recusado explicitamente")
        finally:
            central.terminate()
            try:
                central.wait(timeout=3)
            except subprocess.TimeoutExpired:
                central.kill()
                central.wait(timeout=3)


if __name__ == "__main__":
    main()
