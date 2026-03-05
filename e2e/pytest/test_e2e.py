# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

from west import log

from conftest import TestcaseHelper
from data import data
import time

SHELL_IS_READY="dvcshellcmd Device shell ready$"
SHELL_IS_CLOSING="dvcshellcmd Device shell closing$"
SHELL_CMD_DISCONNECT="dvcshellcmd_disconnect"
SHELL_CMD_SEND="dvcshellcmd_send"

def test_device(testcase_helper: TestcaseHelper):
    log.inf("Launching the device")

    testcase_helper.dut.launch()
    testcase_helper.dut.readlines_until(SHELL_IS_READY, timeout=60)

    # TODO: remove this
    # Wait a couple of seconds
    time.sleep(5)

    from test_utilities import encode_shell_bson
    from datetime import datetime, timezone

    # interface = "org.astarte-platform.zephyr.e2etest.DeviceDatastream"
    # path = "/integer_endpoint"
    # value = 42
    # timestamp = datetime.now(tz=timezone.utc)
    # bson_base64 = encode_shell_bson(value)
    # unix_t = int(timestamp.timestamp() * 1000)
    # command = f"{SHELL_CMD_SEND} individual {interface} {path} {bson_base64} {unix_t}"
    # testcase_helper.shell.exec_command(command)

    # interface = "org.astarte-platform.zephyr.e2etest.DeviceAggregate"
    # path = "/sensor42"
    # aggregate_data = {
    #     "binaryblob_endpoint": b"SGVsbG8=",
    #     "binaryblobarray_endpoint": [b"SGVsbG8=", b"dDk5Yg=="],
    #     "boolean_endpoint": True,
    #     "booleanarray_endpoint": [True, False, True],
    #     "datetime_endpoint": datetime.fromtimestamp(1710940988, tz=timezone.utc),
    #     "datetimearray_endpoint": [
    #         datetime.fromtimestamp(17109409814, tz=timezone.utc),
    #         datetime.fromtimestamp(1710940988, tz=timezone.utc),
    #     ],
    #     "double_endpoint": 15.42,
    #     "doublearray_endpoint": [1542.25, 88852.6],
    #     "integer_endpoint": 42,
    #     "integerarray_endpoint": [4525, 0, 11],
    #     "longinteger_endpoint": 8589934592,
    #     "longintegerarray_endpoint": [8589930067, 42, 8589934592],
    #     "string_endpoint": "Hello world!",
    #     "stringarray_endpoint": ["Hello ", "world!"],
    # }
    # bson_base64 = encode_shell_bson(aggregate_data)
    # command = f"{SHELL_CMD_SEND} object {interface} {path} {bson_base64}"
    # testcase_helper.shell.exec_command(command)

    # interface = "org.astarte-platform.zephyr.e2etest.DeviceProperty"
    # path = "/sensor36/string_endpoint"
    # value = "Hello from the shell!"
    # bson_base64 = encode_shell_bson(value)
    # command = f"{SHELL_CMD_SEND} property set {interface} {path} {bson_base64}"
    # testcase_helper.shell.exec_command(command)

    # interface = "org.astarte-platform.zephyr.e2etest.DeviceProperty"
    # path = "/sensor36/string_endpoint"
    # command = f"{SHELL_CMD_SEND} property unset {interface} {path}"
    # testcase_helper.shell.exec_command(command)

    # for interface_data in data:
    #     interface_data.test(testcase_helper)

    # TODO: remove this
    # Wait a couple of seconds
    time.sleep(5)

    testcase_helper.shell.exec_command(SHELL_CMD_DISCONNECT)
    testcase_helper.dut.readlines_until(SHELL_IS_CLOSING, timeout=60)
