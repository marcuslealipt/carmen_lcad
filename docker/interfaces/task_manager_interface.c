/* SPDX-License-Identifier: GPL-2.0-or-later
 * Transporte IPC do perfil Docker. O executor privado de missões não é implementado.
 */
#include "ipc_support.h"
#include <carmen/task_manager_interface.h>

SUBSCRIBE(carmen_task_manager_subscribe_desired_engage_state_message,
          carmen_task_manager_desired_engage_state_message,
          CARMEN_TASK_MANAGER_DESIRED_ENGAGE_STATE_MESSAGE)

SUBSCRIBE(carmen_task_manager_subscribe_set_semi_trailer_type_and_beta_message,
          carmen_task_manager_set_semi_trailer_type_and_beta_message,
          CARMEN_TASK_MANAGER_SET_SEMI_TRAILER_TYPE_AND_BETA_MESSAGE)

void
carmen_task_manager_publish_desired_engage_state_message(int state, double timestamp)
{
    carmen_task_manager_desired_engage_state_message message = {0};
    message.desired_engage_state = state;
    message.timestamp = timestamp;
    message.host = carmen_get_host();
    publish_ipc(CARMEN_TASK_MANAGER_DESIRED_ENGAGE_STATE_MESSAGE_NAME,
                CARMEN_TASK_MANAGER_DESIRED_ENGAGE_STATE_MESSAGE_FMT, &message);
}

void
carmen_task_manager_publish_set_collision_geometry_message(int geometry, double timestamp)
{
    carmen_task_manager_set_collision_geometry_message message = {0};
    message.geometry = geometry;
    message.timestamp = timestamp;
    message.host = carmen_get_host();
    publish_ipc(CARMEN_TASK_MANAGER_SET_GEOMETRY_MESSAGE_NAME,
                CARMEN_TASK_MANAGER_SET_GEOMETRY_MESSAGE_FMT, &message);
}

void
carmen_task_manager_read_semi_trailer_parameters(carmen_semi_trailers_config_t *config,
                                                int argc, char **argv, int type)
{
    (void)argc;
    (void)argv;
    /* O cenário v0.1 só admite veículo sem reboque. Falha explícita em vez de
       fingir configuração para modelos privados cuja implementação não está no git. */
    if (config == NULL || type != 0) {
        fprintf(stderr, "Docker v0.1: semi_trailer_type=%d não suportado; "
                        "use semi_trailer_initial_type 0 (veículo sem reboque).\n", type);
        exit(EXIT_FAILURE);
    }
    memset(config, 0, sizeof(*config));
}
