#!/usr/bin/env bash
set -euo pipefail
cd "$CARMEN_HOME/bin"
for binary in central param_daemon proccontrol simulator_ackerman localize_ackerman \
              navigator_ackerman navigator_gui2 docker_map_bridge docker_ipc_probe; do
  test -x "$binary" || { echo "Binário ausente: $binary" >&2; exit 1; }
  description=$(file -L "$binary")
  [[ $description == *'ELF 64-bit'* && $description == *'x86-64'* ]] || exit 1
  dependencies=$(ldd "$binary" 2>&1) || { printf '%s\n' "$dependencies"; exit 1; }
  if [[ $dependencies == *'not found'* ]]; then
    printf 'Dependência ausente em %s:\n%s\n' "$binary" "$dependencies" >&2
    exit 1
  fi
  printf 'ELF/loader OK: %s\n' "$binary"
done
echo 'Inspeção de ELF/loader concluída; execução funcional é feita pelo smoke test.'
