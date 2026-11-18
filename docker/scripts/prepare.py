#!/usr/bin/env python3
"""Materializa parâmetros e converte mapas legados sem modificar os originais."""
import argparse
import json
import math
import struct
from pathlib import Path

LABEL = b"CARMENMAPFILEv020"


def read_map(source):
    raw = Path(source).read_bytes()
    offset = 0
    while raw[offset:offset + 1] == b"#":
        offset = raw.index(b"\n", offset) + 1
    if raw[offset:offset + len(LABEL)] != LABEL:
        raise ValueError("Cabeçalho CARMEN inválido")
    offset += len(LABEL)
    while offset < len(raw):
        if offset + 5 > len(raw):
            raise ValueError("Cabeçalho de chunk truncado")
        kind, size = struct.unpack_from("<Bi", raw, offset)
        start, end = offset + 5, offset + 5 + size
        if size < 10 or end > len(raw):
            raise ValueError("Chunk truncado ou tamanho inválido")
        if kind == 1:
            nx, ny = struct.unpack_from("<ii", raw, start + 10)
            count = nx * ny
            if nx <= 0 or ny <= 0 or count > 10_000_000:
                raise ValueError("Dimensões de mapa inválidas")
            if size == 22 + 4 * count:
                scalar, width = "f", 4
            elif size == 26 + 8 * count:
                scalar, width = "d", 8
            else:
                raise ValueError("Grid incompatível: nem float32 nem float64 completos")
            resolution = struct.unpack_from("<" + scalar, raw, start + 18)[0]
            cells = struct.unpack_from("<" + str(count) + scalar, raw, start + 18 + width)
            if not math.isfinite(resolution) or not 0 < resolution <= 10:
                raise ValueError("Resolução inválida")
            if not all(math.isfinite(v) and -1.00001 <= v <= 1.00001 for v in cells):
                raise ValueError("Ocupação inválida")
            return nx, ny, resolution, cells, width
        offset = end
    raise ValueError("Gridmap não encontrado")


def write_map(destination, nx, ny, resolution, cells):
    # O leitor atual usa double tanto para resolução como para ocupação.
    payload = b"GRIDMAP   " + struct.pack("<iid", nx, ny, resolution)
    payload += struct.pack("<" + str(len(cells)) + "d", *cells)
    Path(destination).write_bytes(LABEL + struct.pack("<Bi", 1, len(payload)) + payload)


def initial_pose(nx, ny, resolution, cells):
    # Ensaio reto de 1,2 m, mais 0,66 m da frente do Pioneer e folga.
    radius = math.ceil(0.4 / resolution)
    ahead = math.ceil(2.0 / resolution)
    for x in range(radius, nx - ahead):
        for y in range(radius, ny - radius):
            if all(0 <= cells[i * ny + j] < 0.2
                   for i in range(x - radius, x + ahead + 1)
                   for j in range(y - radius, y + radius + 1)):
                return [x * resolution, y * resolution, 0.0]
    raise ValueError("Mapa sem corredor livre para o smoke test; não enviar movimento")


def parameters(path):
    result = {}
    # Converte explicitamente todas as seções do arquivo-base em um único perfil.
    # A última definição vence; nenhum include, shell ou expansão é executado.
    for line in Path(path).read_text().splitlines():
        line = line.split("#", 1)[0].strip()
        if not line or line.startswith("["):
            continue
        parts = line.split(None, 1)
        if len(parts) != 2 or parts[0].startswith("$"):
            raise ValueError("Linha de parâmetro não suportada: " + line)
        result[parts[0]] = parts[1]
    return result


def prepare(root, output):
    output.mkdir(parents=True, exist_ok=True)
    source = root / "data/mapa_do_ctvii_500k_64.map"
    nx, ny, resolution, cells, width = read_map(source)
    pose = initial_pose(nx, ny, resolution, cells)
    write_map(output / "ctvii.map", nx, ny, resolution, cells)
    values = parameters(root / "src/carmen-ford-escape.ini")
    pioneer = parameters(root / "src/carmen-pioneer-ackerman.ini")
    # Mantém parâmetros novos (PID, GUI, localização); troca apenas geometria/laser.
    geometry = ["length", "width", "distance_between_front_and_rear_axles",
                "distance_between_rear_wheels", "distance_between_rear_car_and_rear_wheels",
                "distance_between_front_car_and_front_wheels", "frontlaser_offset",
                "frontlaser_side_offset", "frontlaser_angular_offset", "frontlaser_id",
                "rearlaser_id", "min_approach_dist", "min_side_dist"]
    for name in geometry:
        values["robot_" + name] = pioneer["robot_" + name]
    values.update(parameters(root / "docker/config/simulation-overrides.ini"))
    for name in list(values):
        if name.startswith("localize_ackerman") and name.endswith("_alive"):
            values[name] = "off"
    text = "# Gerado por docker/scripts/prepare.py; fontes originais preservadas.\n[*]\n"
    text += "".join(f"{k}\t{v}\n" for k, v in sorted(values.items()))
    (output / "carmen.ini").write_text(text)
    metadata = {"map": str(source), "input_scalar_bits": width * 8,
                "output_scalar_bits": 64, "nx": nx, "ny": ny,
                "resolution": resolution, "initial_pose": pose}
    (output / "scenario.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(json.dumps(metadata))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    prepare(args.root, args.output)
