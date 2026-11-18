/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ipc_support.h"
#include <carmen/user_app_server_interface.h>

void
carmen_user_app_server_publish_execute_mission_message(const char *mission, double timestamp)
{
    carmen_user_app_server_execute_mission_message message = {0};
    message.mission = (char *)mission;
    message.timestamp = timestamp;
    message.host = carmen_get_host();
    publish_ipc(CARMEN_USER_APP_SERVER_EXECUTE_MISSION_MESSAGE_NAME,
                CARMEN_USER_APP_SERVER_EXECUTE_MISSION_MESSAGE_FMT, &message);
}

void
carmen_user_app_server_publish_update_mission_message(int code, double timestamp)
{
    carmen_user_app_server_update_mission_message message = {0};
    message.mission_uptate_code = code;
    message.timestamp = timestamp;
    message.host = carmen_get_host();
    publish_ipc(CARMEN_USER_APP_SERVER_UPDATE_MISSION_MESSAGE_NAME,
                CARMEN_USER_APP_SERVER_UPDATE_MISSION_MESSAGE_FMT, &message);
}
