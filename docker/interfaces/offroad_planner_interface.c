/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ipc_support.h"
#include <carmen/offroad_planner_interface.h>

SUBSCRIBE(carmen_offroad_planner_subscribe_plan_message,
          carmen_offroad_planner_plan_message, CARMEN_OFFROAD_PLANNER_PLAN_MESSAGE)

void
carmen_offroad_planner_publish_plan(carmen_offroad_planner_plan_message *message)
{
    publish_ipc(CARMEN_OFFROAD_PLANNER_PLAN_MESSAGE_NAME,
                CARMEN_OFFROAD_PLANNER_PLAN_MESSAGE_FMT, message);
}
