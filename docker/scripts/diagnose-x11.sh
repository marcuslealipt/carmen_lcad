#!/usr/bin/env bash
# Usa a imagem existente; monta somente os auxiliares de diagnóstico em leitura.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
[[ ${DISPLAY:-} =~ ^:[0-9]+(\.[0-9]+)?$ ]] || {
  echo 'Execute numa sessão gráfica local X11/XWayland (DISPLAY=:N).' >&2; exit 2;
}
command -v xauth >/dev/null || { echo 'xauth não está disponível no host.' >&2; exit 2; }
test -d logs || { echo 'Crie docker/logs antes de executar.' >&2; exit 2; }
export CARMEN_XAUTH
CARMEN_XAUTH=$(mktemp /tmp/carmen-diagnostic-xauth.XXXXXX)
trap 'rm -f "$CARMEN_XAUTH"' EXIT
chmod 600 "$CARMEN_XAUTH"
cookie=$(xauth nlist "$DISPLAY")
test -n "$cookie" || { echo 'Nenhum cookie X11 disponível para DISPLAY.' >&2; exit 2; }
printf '%s\n' "$cookie" | sed 's/^..../ffff/' | xauth -f "$CARMEN_XAUTH" nmerge -
docker compose -f compose.yaml -f compose.x11.yaml run --rm --no-deps --pull never \
  --user "$(id -u):$(id -g)" \
  -v "$PWD/diagnostics:/opt/carmen/docker/diagnostics:ro" \
  --entrypoint python3 carmen /opt/carmen/docker/diagnostics/diagnose_gui.py
