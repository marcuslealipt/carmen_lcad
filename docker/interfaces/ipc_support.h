/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef CARMEN_DOCKER_IPC_SUPPORT_H
#define CARMEN_DOCKER_IPC_SUPPORT_H

#include <carmen/carmen.h>

/* Usa os formatos públicos do fork, sem reproduzir structs ou inventar mensagens. */
#define SUBSCRIBE(FUNCTION, TYPE, SCHEMA) \
    void FUNCTION(TYPE *message, carmen_handler_t handler, carmen_subscribe_t how) \
    { \
        carmen_subscribe_message(SCHEMA##_NAME, SCHEMA##_FMT, message, \
                                 sizeof(TYPE), handler, how); \
    }

static inline void
publish_ipc(const char *name, const char *format, void *message)
{
    carmen_test_ipc_exit(IPC_defineMsg(name, IPC_VARIABLE_LENGTH, format),
                        "Could not define Docker interface message", name);
    carmen_test_ipc_exit(IPC_publishData(name, message),
                        "Could not publish Docker interface message", name);
}

#endif
