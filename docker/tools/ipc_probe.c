/* Testa mensagens reais; não considera apenas fork/exec como sucesso. GPL-2.0-or-later. */
#include <carmen/carmen.h>
#include <carmen/base_ackerman_messages.h>
#include <carmen/behavior_selector_messages.h>
#include <carmen/simulator_ackerman_messages.h>
#include <carmen/localize_ackerman_messages.h>
#include <carmen/map_server_messages.h>
#include <carmen/proccontrol_messages.h>

static int poses, odometry, maps, positions, heartbeats, tables, bad;
static int require_localize, process_count;
static int pids[64];
static char names[64][128];
static double pose_time, odom_time, map_time, position_time, heartbeat_time, table_time;
static double px, py, speed, first_x, first_y;
static volatile sig_atomic_t interrupted;

static void stop_requested(int sig) { (void)sig; interrupted = 1; }

static void pose_handler(carmen_simulator_ackerman_truepos_message *m)
{
    if (!isfinite(m->truepose.x) || !isfinite(m->truepose.y) ||
        !isfinite(m->truepose.theta) || !isfinite(m->v)) { bad = 1; return; }
    if (m->timestamp <= pose_time) return;
    if (!poses) { first_x = m->truepose.x; first_y = m->truepose.y; }
    px = m->truepose.x; py = m->truepose.y; speed = m->v;
    pose_time = m->timestamp; poses++;
}

static void odom_handler(carmen_base_ackerman_odometry_message *m)
{
    if (!isfinite(m->x) || !isfinite(m->y) || !isfinite(m->v)) { bad = 1; return; }
    if (m->timestamp > odom_time) { odom_time = m->timestamp; odometry++; }
}

static void position_handler(carmen_localize_ackerman_globalpos_message *m)
{
    if (!isfinite(m->globalpos.x) || !isfinite(m->globalpos.y) ||
        !isfinite(m->globalpos.theta)) { bad = 1; return; }
    if (m->timestamp > position_time) { position_time = m->timestamp; positions++; }
}

static void map_handler(carmen_map_server_offline_map_message *m)
{
    if (m->size <= 0 || m->size > 10000000 || !m->complete_map ||
        m->config.x_size <= 0 || m->config.y_size <= 0 ||
        (long long)m->config.x_size * m->config.y_size != m->size ||
        !isfinite(m->config.resolution) || m->config.resolution <= 0) {
        bad = 1; return;
    }
    if (m->timestamp > map_time) { map_time = m->timestamp; maps++; }
}

static void heartbeat_handler(carmen_heartbeat_message *m)
{
    if (!strcmp(m->module_name, "simulator") && m->timestamp > heartbeat_time) {
        heartbeat_time = m->timestamp; heartbeats++;
    }
}

static void table_handler(carmen_proccontrol_pidtable_message *m)
{
    int simulator = 0, bridge = 0, localizer = 0;
    for (int i = 0; i < m->num_processes; i++) {
        carmen_proccontrol_process_t *p = &m->process[i];
        if (!p->requested_state) continue;
        if (!p->active || p->pid <= 0) { bad = 1; continue; }
        simulator |= !strcmp(p->module_name, "simulator");
        bridge |= !strcmp(p->module_name, "map_bridge");
        localizer |= !strcmp(p->module_name, "localize");
        int j;
        for (j = 0; j < process_count; j++)
            if (!strcmp(names[j], p->module_name)) break;
        if (j == process_count) {
            if (j >= 64 || strlen(p->module_name) >= sizeof(names[0])) { bad = 1; continue; }
            strcpy(names[j], p->module_name); pids[j] = p->pid; process_count++;
        } else if (pids[j] != p->pid) {
            fprintf(stderr, "Processo reiniciado: %s (%d -> %d)\n", names[j], pids[j], p->pid);
            bad = 1;
        }
    }
    if (!simulator || !bridge || (require_localize && !localizer)) bad = 1;
    table_time = m->timestamp; tables++;
}

#define SUB(NAME, FMT, TYPE, HANDLER) carmen_subscribe_message((char *)(NAME), (char *)(FMT), \
    NULL, sizeof(TYPE), (carmen_handler_t)(HANDLER), CARMEN_SUBSCRIBE_LATEST)

static void subscribe_all(void)
{
    SUB(CARMEN_SIMULATOR_ACKERMAN_TRUEPOS_NAME, CARMEN_SIMULATOR_ACKERMAN_TRUEPOS_FMT, carmen_simulator_ackerman_truepos_message, pose_handler);
    SUB(CARMEN_BASE_ACKERMAN_ODOMETRY_NAME, CARMEN_BASE_ACKERMAN_ODOMETRY_FMT, carmen_base_ackerman_odometry_message, odom_handler);
    SUB(CARMEN_MAP_SERVER_OFFLINE_MAP_NAME, CARMEN_MAP_SERVER_OFFLINE_MAP_FMT, carmen_map_server_offline_map_message, map_handler);
    SUB(CARMEN_LOCALIZE_ACKERMAN_GLOBALPOS_NAME, CARMEN_LOCALIZE_ACKERMAN_GLOBALPOS_FMT, carmen_localize_ackerman_globalpos_message, position_handler);
    SUB(CARMEN_HEARTBEAT_NAME, CARMEN_HEARTBEAT_FMT, carmen_heartbeat_message, heartbeat_handler);
    SUB(CARMEN_PROCCONTROL_PIDTABLE_NAME, CARMEN_PROCCONTROL_PIDTABLE_FMT, carmen_proccontrol_pidtable_message, table_handler);
}

static void publish_control(double velocity)
{
    carmen_behavior_selector_state_message state = {0};
    state.low_level_state = velocity == 0 ? Stopped : Free_Running;
    state.algorithm = CARMEN_BEHAVIOR_SELECTOR_A_STAR;
    state.timestamp = carmen_get_time(); state.host = carmen_get_host();
    carmen_test_ipc_exit(IPC_publishData(CARMEN_BEHAVIOR_SELECTOR_CURRENT_STATE_NAME, &state),
                        "state", CARMEN_BEHAVIOR_SELECTOR_CURRENT_STATE_NAME);
    carmen_robot_and_trailers_motion_command_t command = {0};
    command.v = velocity; command.phi = 0; command.time = 0.3;
    carmen_base_ackerman_motion_command_message message = {0};
    message.num_motion_commands = 1; message.motion_command = &command;
    message.timestamp = carmen_get_time(); message.host = carmen_get_host();
    carmen_test_ipc_exit(IPC_publishData(CARMEN_BASE_ACKERMAN_MOTION_COMMAND_NAME, &message),
                        "command", CARMEN_BASE_ACKERMAN_MOTION_COMMAND_NAME);
}

static int initialize(double x, double y)
{
    carmen_point_t mean = {x, y, 0}, std = {0.02, 0.02, 0.01};
    carmen_localize_ackerman_initialize_message m = {0};
    m.distribution = CARMEN_INITIALIZE_GAUSSIAN; m.num_modes = 1;
    m.mean = &mean; m.std = &std; m.host = carmen_get_host();
    carmen_test_ipc_exit(IPC_defineMsg(CARMEN_LOCALIZE_ACKERMAN_INITIALIZE_NAME,
        IPC_VARIABLE_LENGTH, CARMEN_LOCALIZE_ACKERMAN_INITIALIZE_FMT), "define", "initialize");
    // Mapas precisam ter chegado antes da inicialização do simulador.
    double deadline = carmen_get_time() + 10;
    while (!maps && carmen_get_time() < deadline) carmen_ipc_sleep(0.1);
    if (!maps) return 4;
    for (int i = 0; i < 12 && !interrupted; i++) {
        m.timestamp = carmen_get_time();
        carmen_test_ipc_exit(IPC_publishData(CARMEN_LOCALIZE_ACKERMAN_INITIALIZE_NAME, &m), "publish", "initialize");
        carmen_ipc_sleep(0.5);
        if (poses >= 2 && hypot(px - x, py - y) < 0.08) return 0;
    }
    fprintf(stderr, "Sem confirmação da pose inicial pelo simulador\n");
    return 4;
}

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    const char *mode = argv[1];
    if (strcmp(mode, "ipc") && strcmp(mode, "params") && strcmp(mode, "initialize") &&
        strcmp(mode, "observe") && strcmp(mode, "drive")) return 2;
    if ((!strcmp(mode, "initialize") && argc != 4) ||
        ((!strcmp(mode, "observe") || !strcmp(mode, "drive")) && argc != 3)) return 2;
    // Permite watchdog passivo e demonstração em conexões independentes.
    char connection_name[128];
    snprintf(connection_name, sizeof(connection_name), "docker_ipc_probe_%ld", (long)getpid());
    char *connection_argv[] = {connection_name, NULL};
    carmen_ipc_initialize(1, connection_argv);
    if (!strcmp(mode, "ipc")) { carmen_ipc_disconnect(); return 0; }
    carmen_param_check_version(argv[0]);
    carmen_param_set_module("robot");
    double length = 0;
    if (carmen_param_get_double("length", &length, NULL) <= 0 || length <= 0) return 3;
    if (!strcmp(mode, "params")) { printf("param_daemon: robot_length=%.3f\n", length); return 0; }
    signal(SIGINT, stop_requested); signal(SIGTERM, stop_requested);
    require_localize = argc == 3 && !strcmp(argv[2], "localized");
    subscribe_all();
    if (!strcmp(mode, "initialize")) return initialize(atof(argv[2]), atof(argv[3]));
    int drive = !strcmp(mode, "drive");
    if (drive) {
        carmen_test_ipc_exit(IPC_defineMsg(CARMEN_BEHAVIOR_SELECTOR_CURRENT_STATE_NAME,
            IPC_VARIABLE_LENGTH, CARMEN_BEHAVIOR_SELECTOR_CURRENT_STATE_FMT), "define", "state");
        carmen_test_ipc_exit(IPC_defineMsg(CARMEN_BASE_ACKERMAN_MOTION_COMMAND_NAME,
            IPC_VARIABLE_LENGTH, CARMEN_BASE_ACKERMAN_MOTION_COMMAND_FMT), "define", "command");
    }
    double begin = carmen_get_time();
    // Mede a pose antes de comandar. Ausência de mensagens nunca vira sucesso.
    while (!poses && carmen_get_time() - begin < 4) carmen_ipc_sleep(0.1);
    if (!poses) return 4;
    begin = carmen_get_time();
    while (!interrupted && !bad && carmen_get_time() - begin < 6) {
        if (drive) publish_control(0.2);
        carmen_ipc_sleep(0.1);
    }
    if (drive) {
        for (int i = 0; i < 10; i++) { publish_control(0); carmen_ipc_sleep(0.1); }
    }
    double now = carmen_get_time(), displacement = hypot(px - first_x, py - first_y);
    int ok = !bad && !interrupted && poses >= 20 && odometry >= 20 && maps >= 3 &&
        heartbeats >= 10 && tables >= 2 && now - pose_time < 1 && now - odom_time < 1 &&
        now - heartbeat_time < 1 && now - map_time < 2 && now - table_time < 3 &&
        (!require_localize || (positions >= 5 && now - position_time < 1)) &&
        (!drive || (displacement >= 0.2 && displacement <= 1.8 && fabs(speed) < 0.05));
    printf("%s: poses=%d odometry=%d maps=%d heartbeat=%d pidtables=%d globalpos=%d displacement=%.3f stopped_v=%.3f\n",
        ok ? "PASS" : "FAIL", poses, odometry, maps, heartbeats, tables, positions, displacement, speed);
    carmen_ipc_disconnect();
    return ok ? 0 : 5;
}
