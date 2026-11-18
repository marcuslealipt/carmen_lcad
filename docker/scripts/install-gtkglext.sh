#!/usr/bin/env bash
set -euo pipefail
jobs=${1:-2}
[[ $jobs =~ ^[1-9][0-9]*$ ]] || exit 2
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$work"
curl --fail --show-error --location --retry 3 \
  https://download.gnome.org/sources/gtkglext/1.2/gtkglext-1.2.0.tar.gz -o gtkglext.tar.gz
echo 'e5073f3c6b816e7fa67d359d9745a5bb5de94a628ac85f624c992925a46844f9  gtkglext.tar.gz' | sha256sum --check -
tar -xzf gtkglext.tar.gz
cd gtkglext-1.2.0
revision=917d7c6ff529b1d290d8c834bf67bd5e69d2f9b1
for name in 01_fix_fontcache_nullderef.diff \
            02_fix_gtk-2.20_deprecated_symbols.diff \
            03_gdkglext-config-h-installation-path.diff \
            04_glibc2.27-ftbfs.diff 05_nopangox.diff libGL.so.1.diff; do
  curl --fail --show-error --location --retry 3 \
    "https://raw.githubusercontent.com/deepin-community/gtkglext/$revision/debian/patches/$name" -o "$work/$name"
  patch --batch --forward -p1 < "$work/$name"
done
autoreconf -fi
./configure --prefix=/usr/local
make -j"$jobs"
make install
ldconfig
test "$(pkg-config --modversion gtkglext-1.0)" = 1.2.0
test -f /usr/local/include/gtkglext-1.0/gdkglext-config.h
