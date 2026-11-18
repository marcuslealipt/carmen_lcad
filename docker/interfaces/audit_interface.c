/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ipc_support.h"
#include <carmen/audit_interface.h>

SUBSCRIBE(carmen_audit_subscribe_status_message,
          carmen_audit_status_message, CARMEN_AUDIT_STATUS_MESSAGE)

void
carmen_audit_publish_status_message(carmen_audit_status_message *message)
{
    publish_ipc(CARMEN_AUDIT_STATUS_MESSAGE_NAME,
                CARMEN_AUDIT_STATUS_MESSAGE_FMT, message);
}
