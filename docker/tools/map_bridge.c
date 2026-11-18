/* Ponte experimental: arquivo CARMEN -> mapa offline + mapa do mapper. GPL-2.0-or-later. */
#include <carmen/carmen.h>
#include <carmen/map_server_messages.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Uso: docker_map_bridge mapa.map\n");
        return 2;
    }
    carmen_map_t map = {0};
    if (carmen_map_read_gridmap_chunk(argv[1], &map) < 0 ||
        map.config.x_size <= 0 || map.config.y_size <= 0 || !map.complete_map)
        return 3;
    carmen_ipc_initialize(argc, argv);
    carmen_param_check_version(argv[0]);
    carmen_test_ipc_exit(IPC_defineMsg(CARMEN_MAP_SERVER_OFFLINE_MAP_NAME,
        IPC_VARIABLE_LENGTH, CARMEN_MAP_SERVER_OFFLINE_MAP_FMT), "define", "offline_map");
    carmen_test_ipc_exit(IPC_defineMsg(CARMEN_MAPPER_MAP_MESSAGE_NAME,
        IPC_VARIABLE_LENGTH, CARMEN_MAPPER_MAP_MESSAGE_FMT), "define", "mapper_map");
    carmen_mapper_map_message message = {0};
    message.config = map.config;
    message.complete_map = map.complete_map;
    message.size = map.config.x_size * map.config.y_size;
    message.host = carmen_get_host();
    fprintf(stderr, "Mapa carregado: %dx%d, resolução %.6f m\n",
        map.config.x_size, map.config.y_size, map.config.resolution);
    while (1) {
        message.timestamp = carmen_get_time();
        carmen_test_ipc_exit(IPC_publishData(CARMEN_MAP_SERVER_OFFLINE_MAP_NAME, &message),
                            "publish", "offline_map");
        carmen_test_ipc_exit(IPC_publishData(CARMEN_MAPPER_MAP_MESSAGE_NAME, &message),
                            "publish", "mapper_map");
        carmen_publish_heartbeat("map_bridge");
        carmen_ipc_sleep(1.0);
    }
}
