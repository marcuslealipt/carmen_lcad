include ../../src/Makefile.conf
LINK = g++
CFLAGS += -std=c11
LFLAGS += -lmap_io -lmap_interface -lparam_interface -lglobal -lipc -lz
MODULE_NAME = DOCKER ACKERMAN TESTS
MODULE_COMMENT = Testes IPC e ponte de mapa para simulação isolada
SOURCES = map_bridge.c ipc_probe.c
TARGETS = docker_map_bridge docker_ipc_probe
PUBLIC_BINARIES = $(TARGETS)
docker_map_bridge: map_bridge.o
docker_ipc_probe: ipc_probe.o
include ../../src/Makefile.rules
