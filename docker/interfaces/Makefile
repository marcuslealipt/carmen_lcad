# Bibliotecas IPC limitadas ao cenário Docker; headers oficiais já exportados.
ifndef CARMEN_HOME
$(error Defina CARMEN_HOME antes de compilar as interfaces Docker)
endif

CPPFLAGS += -I$(CARMEN_HOME)/include
CFLAGS += -std=c11 -D_DEFAULT_SOURCE -O2 -fPIC -Wall -Wextra -Werror
INTERFACES = route_planner task_manager frenet_path_planner offroad_planner audit user_app_server
ARCHIVES = $(addprefix lib,$(addsuffix _interface.a,$(INTERFACES)))

.PHONY: all install clean test
all: $(ARCHIVES)

%_interface.o: %_interface.c ipc_support.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

lib%_interface.a: %_interface.o
	$(AR) rcs $@ $<

install: all
	install -d "$(CARMEN_HOME)/lib"
	install -m 644 $(ARCHIVES) "$(CARMEN_HOME)/lib/"

interface_roundtrip.o: ../tests/interface_roundtrip.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

interface_roundtrip: interface_roundtrip.o $(ARCHIVES)
	$(CXX) -o $@ $< -Wl,--start-group $(ARCHIVES) \
		-L"$(CARMEN_HOME)/lib" -lglobal -lparam_interface -lipc -Wl,--end-group -lpthread -lm -lz

test: interface_roundtrip
	python3 ../tests/test_interfaces.py --central "$(CARMEN_HOME)/bin/central" \
		--probe ./interface_roundtrip --logs ../logs/interfaces

clean:
	$(RM) *.o $(ARCHIVES) interface_roundtrip
