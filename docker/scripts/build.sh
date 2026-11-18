#!/usr/bin/env bash
set -euo pipefail
jobs=${1:-2}
[[ $jobs =~ ^[1-9][0-9]*$ ]] || exit 2
cd "$CARMEN_HOME/src"
test "$(gcc -dumpversion | cut -d. -f1)" = 15
# Entrada finita: Enter em todas as perguntas. Evita SIGPIPE de yes | configure.
printf '\n\n\n\n\n\n\n\n\n\n\n\n' > /tmp/carmen-configure.answers
timeout 120 ./configure --nojava --nocuda --noqt3 < /tmp/carmen-configure.answers
for variable in NO_JAVA NO_CUDA NO_PYTHON; do
  grep -Eq "^$variable = 1$" Makefile.vars
done
if grep -Eq '^(NO_CPP|QT3|NO_GRAPHICS) = 1$' Makefile.vars; then
  echo 'configure produziu flags incompatíveis com o perfil Docker' >&2
  exit 1
fi
grep -Eq '^CARMEN_HOME = /opt/carmen$' Makefile.vars
# Os módulos modernos compartilham muitas interfaces. Preserva a ordem upstream
# de export/phase1; phase2 fica restrita aos executáveis da primeira experiência.
make -j"$jobs" export
make -j"$jobs" phase1
make prebuilt
for module in ipc global maptools param_daemon proccontrol simulator_ackerman \
              localize_ackerman navigator_ackerman navigator_gui2; do
  make -C "$module" -j"$jobs" phase2
done
make -C "$CARMEN_HOME/docker/tools" -j"$jobs" phase2
