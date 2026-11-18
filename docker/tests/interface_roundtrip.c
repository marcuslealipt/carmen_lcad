/* SPDX-License-Identifier: GPL-2.0-or-later
 * Exercita serialização pelo central real, sem mock e sem iniciar planejadores.
 */
#include <carmen/carmen.h>
#include <carmen/behavior_selector_messages.h>
#include <carmen/route_planner_interface.h>
#include <carmen/task_manager_interface.h>
#include <carmen/offroad_planner_interface.h>
#include <carmen/frenet_path_planner_interface.h>
#include <carmen/audit_interface.h>
#include <carmen/user_app_server_interface.h>

static unsigned received;
static const unsigned expected = (1u << 11) - 1;

static void road(carmen_route_planner_road_network_message *m)
{
    if (m->number_of_poses == 1 && m->poses && m->poses[0].x == 12.25 &&
        m->annotations && m->annotations[0] == 7) received |= 1u << 0;
}
static void engage(carmen_task_manager_desired_engage_state_message *m)
{
    if (m->desired_engage_state == ENGAGED && m->timestamp == 1234.5) received |= 1u << 1;
}
static void trailer(carmen_task_manager_set_semi_trailer_type_and_beta_message *m)
{
    if (m->semi_trailer_type == 0 && m->num_trailers == 0 && m->timestamp == 1234.5) received |= 1u << 2;
}
static void offroad(carmen_offroad_planner_plan_message *m)
{
    if (m->number_of_poses == 1 && m->poses && m->poses[0].x == 12.25 &&
        m->offroad_planner_feedback == PLAN_OK) received |= 1u << 3;
}
static void paths(carmen_frenet_path_planner_set_of_paths *m)
{
    if (m->set_of_paths_size == 1 && m->set_of_paths && m->set_of_paths[0].x == 12.25 &&
        m->annotations && m->annotations[0] == 7) received |= 1u << 4;
}
static void audit(carmen_audit_status_message *m)
{
    if (m->decision_making_status == 17 && m->timestamp == 1234.5) received |= 1u << 5;
}
static void destination(carmen_route_planner_destination_message *m)
{
    if (m->destination && !strcmp(m->destination, "docker-test") &&
        m->destination_point.x == 12.25 && m->timestamp > 0) received |= 1u << 6;
}
static void predefined(carmen_route_planner_predefined_route_message *m)
{
    if (m->predefined_route && !strcmp(m->predefined_route, "test-route") &&
        m->code == 1 && m->timestamp > 0) received |= 1u << 7;
}
static void geometry(carmen_task_manager_set_collision_geometry_message *m)
{
    if (m->geometry == 1 && m->timestamp == 1234.5) received |= 1u << 8;
}
static void mission(carmen_user_app_server_execute_mission_message *m)
{
    if (m->mission && !strcmp(m->mission, "test-payload") && m->timestamp == 1234.5) received |= 1u << 9;
}
static void update(carmen_user_app_server_update_mission_message *m)
{
    if (m->mission_uptate_code == USER_APP_SERVER_MISSION_ABORT && m->timestamp == 1234.5) received |= 1u << 10;
}

#define RAW_SUB(TYPE, SCHEMA, HANDLER) \
    carmen_subscribe_message(SCHEMA##_NAME, SCHEMA##_FMT, NULL, sizeof(TYPE), \
                            (carmen_handler_t)HANDLER, CARMEN_SUBSCRIBE_ALL)

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--unsupported-trailer")) {
        carmen_semi_trailers_config_t config = {0};
        carmen_task_manager_read_semi_trailer_parameters(&config, argc, argv, 1);
        return 99; /* O runner exige a recusa explícita, não apenas qualquer falha. */
    }
    carmen_semi_trailers_config_t config;
    memset(&config, 0xff, sizeof(config));
    carmen_task_manager_read_semi_trailer_parameters(&config, argc, argv, 0);
    if (config.num_semi_trailers != 0) return 2;
    carmen_ipc_initialize(argc, argv);
    IPC_setCapacity(32);
    carmen_route_planner_subscribe_road_network_message(NULL, (carmen_handler_t)road, CARMEN_SUBSCRIBE_ALL);
    carmen_task_manager_subscribe_desired_engage_state_message(NULL, (carmen_handler_t)engage, CARMEN_SUBSCRIBE_ALL);
    carmen_task_manager_subscribe_set_semi_trailer_type_and_beta_message(NULL, (carmen_handler_t)trailer, CARMEN_SUBSCRIBE_ALL);
    carmen_offroad_planner_subscribe_plan_message(NULL, (carmen_handler_t)offroad, CARMEN_SUBSCRIBE_ALL);
    carmen_frenet_path_planner_subscribe_set_of_paths_message(NULL, (carmen_handler_t)paths, CARMEN_SUBSCRIBE_ALL);
    carmen_audit_subscribe_status_message(NULL, (carmen_handler_t)audit, CARMEN_SUBSCRIBE_ALL);
    RAW_SUB(carmen_route_planner_destination_message, CARMEN_ROUTE_PLANNER_DESTINATION_MESSAGE, destination);
    RAW_SUB(carmen_route_planner_predefined_route_message, CARMEN_ROUTE_PLANNER_PREDEFINED_ROUTE_MESSAGE, predefined);
    RAW_SUB(carmen_task_manager_set_collision_geometry_message, CARMEN_TASK_MANAGER_SET_GEOMETRY_MESSAGE, geometry);
    RAW_SUB(carmen_user_app_server_execute_mission_message, CARMEN_USER_APP_SERVER_EXECUTE_MISSION_MESSAGE, mission);
    RAW_SUB(carmen_user_app_server_update_mission_message, CARMEN_USER_APP_SERVER_UPDATE_MISSION_MESSAGE, update);

    carmen_robot_and_trailers_traj_point_t pose = {0};
    pose.x = 12.25;
    int annotation = 7;
    carmen_route_planner_road_network_message network = {0};
    network.number_of_poses = 1;
    network.poses = &pose;
    network.annotations = network.annotations_codes = &annotation;
    network.timestamp = 1234.5;
    network.host = carmen_get_host();
    carmen_offroad_planner_plan_message plan = {0};
    plan.number_of_poses = 1;
    plan.poses = &pose;
    plan.offroad_planner_feedback = PLAN_OK;
    plan.timestamp = 1234.5;
    plan.host = carmen_get_host();
    carmen_frenet_path_planner_set_of_paths set = {0};
    set.set_of_paths_size = set.number_of_poses = 1;
    set.set_of_paths = set.rddf_poses_ahead = &pose;
    set.annotations = set.annotations_codes = &annotation;
    set.timestamp = 1234.5;
    set.host = carmen_get_host();
    carmen_audit_status_message status = {0};
    status.decision_making_status = 17;
    status.timestamp = 1234.5;
    status.host = carmen_get_host();
    carmen_task_manager_set_semi_trailer_type_and_beta_message trailers = {0};
    trailers.timestamp = 1234.5;
    trailers.host = carmen_get_host();
    carmen_point_t target = {12.25, 3.5, 0};
    double deadline = carmen_get_time() + 5;
    while (received != expected && carmen_get_time() < deadline) {
        carmen_route_planner_publish_road_network_message(&network);
        carmen_task_manager_publish_desired_engage_state_message(ENGAGED, 1234.5);
        carmen_test_ipc_exit(IPC_publishData(CARMEN_TASK_MANAGER_SET_SEMI_TRAILER_TYPE_AND_BETA_MESSAGE_NAME,
                                            &trailers), "publish test", "trailers");
        carmen_offroad_planner_publish_plan(&plan);
        carmen_frenet_path_planner_publish_set_of_paths_message(&set);
        carmen_audit_publish_status_message(&status);
        carmen_route_planner_set_destination("docker-test", target);
        carmen_route_planner_set_predefined_route("test-route", 1);
        carmen_task_manager_publish_set_collision_geometry_message(1, 1234.5);
        carmen_user_app_server_publish_execute_mission_message("test-payload", 1234.5);
        carmen_user_app_server_publish_update_mission_message(USER_APP_SERVER_MISSION_ABORT, 1234.5);
        carmen_ipc_sleep(0.1);
    }
    carmen_ipc_disconnect();
    printf("%s: IPC roundtrip mask=0x%x expected=0x%x (11 mensagens)\n",
           received == expected ? "PASS" : "FAIL", received, expected);
    return received == expected ? 0 : 1;
}
