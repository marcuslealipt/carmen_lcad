/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ipc_support.h"
#include <carmen/behavior_selector_messages.h>
#include <carmen/frenet_path_planner_interface.h>

SUBSCRIBE(carmen_frenet_path_planner_subscribe_set_of_paths_message,
          carmen_frenet_path_planner_set_of_paths, CARMEN_FRENET_PATH_PLANNER_SET_OF_PATHS_MESSAGE)

void
carmen_frenet_path_planner_publish_set_of_paths_message(carmen_frenet_path_planner_set_of_paths *message)
{
    publish_ipc(CARMEN_FRENET_PATH_PLANNER_SET_OF_PATHS_MESSAGE_NAME,
                CARMEN_FRENET_PATH_PLANNER_SET_OF_PATHS_MESSAGE_FMT, message);
}
