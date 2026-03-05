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

def test_device(testcase_helper: TestcaseHelper):
    log.inf("Launching the device")

    testcase_helper.dut.launch()
    testcase_helper.dut.readlines_until(SHELL_IS_READY, timeout=60)

    # for interface_data in data:
    #     interface_data.test(testcase_helper)

    # Wait 5 seconds for now
    time.sleep(5)

    testcase_helper.shell.exec_command(SHELL_CMD_DISCONNECT)
    testcase_helper.dut.readlines_until(SHELL_IS_CLOSING, timeout=60)
