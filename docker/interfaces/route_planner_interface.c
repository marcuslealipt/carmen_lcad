/* SPDX-License-Identifier: GPL-2.0-or-later
 * Transporte IPC público necessário ao simulador/GUI; não implementa planejamento.
 */
#include "ipc_support.h"
#include <carmen/route_planner_interface.h>

SUBSCRIBE(carmen_route_planner_subscribe_road_network_message,
          carmen_route_planner_road_network_message, CARMEN_ROUTE_PLANNER_ROAD_NETWORK_MESSAGE)

void
carmen_route_planner_publish_road_network_message(carmen_route_planner_road_network_message *message)
{
    publish_ipc(CARMEN_ROUTE_PLANNER_ROAD_NETWORK_MESSAGE_NAME,
                CARMEN_ROUTE_PLANNER_ROAD_NETWORK_MESSAGE_FMT, message);
}

void
carmen_route_planner_set_destination(char *destination, carmen_point_t point)
{
    carmen_route_planner_destination_message message = {0};
    message.destination = destination;
    message.destination_point = point;
    message.timestamp = carmen_get_time();
    message.host = carmen_get_host();
    publish_ipc(CARMEN_ROUTE_PLANNER_DESTINATION_MESSAGE_NAME,
                CARMEN_ROUTE_PLANNER_DESTINATION_MESSAGE_FMT, &message);
}

void
carmen_route_planner_set_predefined_route(char *route, int code)
{
    carmen_route_planner_predefined_route_message message = {0};
    message.predefined_route = route;
    message.code = code;
    message.timestamp = carmen_get_time();
    message.host = carmen_get_host();
    publish_ipc(CARMEN_ROUTE_PLANNER_PREDEFINED_ROUTE_MESSAGE_NAME,
                CARMEN_ROUTE_PLANNER_PREDEFINED_ROUTE_MESSAGE_FMT, &message);
}
