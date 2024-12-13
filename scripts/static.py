# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

"""static.py

West extension that can be used to run static analysis over the sources.

Checked using pylint with the following command (pip install pylint):
python -m pylint --rcfile=./scripts/.pylintrc ./scripts/*.py
Formatted using black with the following command (pip install black):
python -m black --line-length 100 ./scripts/*.py
"""

import json
import re
import subprocess
import sys
from linecache import getline
from pathlib import Path

from colored import fore, stylize
from west import log  # use this for user output
from west.commands import WestCommand  # your extension must subclass this

static_name = "static"
static_help = "run static analysis on the sources (clang-tidy)"
static_description = """\
Convenience wrapper for performing static analysis on the sources.

This command runs CodeChecker which in turn runs clang-tidy.
Since this CodeChecker needs to be run on an application this command
uses one of the samples as application.

Will build each application from scratch and using target qemu_cortex_m3.
"""

# Allowed number of lines for each .c and .h files.
# Add the comment '// NOLINENUMBERLINT' in one of such files to exclude the file from this check.
FILE_LINES_LIMIT = 1000

severity_colours = {
    "UNSPECIFIED": fore("dark_gray"),
    "STYLE": fore("magenta"),
    "LOW": fore("green"),
    "MEDIUM": fore("light_green"),
    "HIGH": fore("yellow"),
    "CRITICAL": fore("red"),
}


class WestCommandStatic(WestCommand):
    """Extension of the WestCommand class, specific for this command."""

    def __init__(self):
        super().__init__(static_name, static_help, static_description)

    def do_add_parser(self, parser_adder):
        """
        This function can be used to add custom options for this command.

        Allows you full control over the type of argparse handling you want.

        Parameters
        ----------
        parser_adder : Any
            Parser adder generated by a call to `argparse.ArgumentParser.add_subparsers()`.

        Returns
        -------
        argparse.ArgumentParser
            The argument parser for this command.
        """
        parser = parser_adder.add_parser(self.name, help=self.help, description=self.description)

        # Add some options using the standard argparse module API.
        parser.add_argument("-p", "--pristine", help="west build pristine flag", default="auto")
        parser.add_argument("-s", "--sample", help="sample to analyze", default="astarte_app")
        parser.add_argument("-e", "--export", help="an additional (optional) export type")

        return parser  # gets stored as self.parser

    # pylint: disable-next=arguments-renamed,unused-argument,too-many-locals
    def do_run(self, args, unknown_args):
        """
        Function called when the user runs the custom command, e.g.:

          $ west clean

        Parameters
        ----------
        args : Any
            Arguments pre parsed by the parser defined by `do_add_parser()`.
        unknown_args : Any
            Extra unknown arguments.
        """
        module_path = Path(self.topdir).joinpath("astarte-device-sdk-zephyr")
        codechecker_analyze_opts = [
            "--analyzers",
            "clang-tidy",
            "--analyzer-config",
            "clang-tidy:take-config-from-directory=true",
            "--ignore",
            "$PWD/skipfile.txt",
        ]
        codechecker_exports = ["json", args.export] if args.export else ["json"]
        cmd = [
            "west build",
            f"-p {args.pristine}",
            "-b native_sim",
            f"$PWD/samples/{args.sample} --",
            "-DZEPHYR_SCA_VARIANT=codechecker",
            f'-DCODECHECKER_EXPORT={",".join(codechecker_exports)}',
            f'-DCODECHECKER_ANALYZE_OPTS="{";".join(codechecker_analyze_opts)}"',
        ]
        print(stylize(" ".join(cmd), fore("cyan")))
        subprocess.run(" ".join(cmd), shell=True, cwd=module_path, timeout=180, check=True)

        has_reports = False
        summary = {"UNSPECIFIED": 0, "STYLE": 0, "LOW": 0, "MEDIUM": 0, "HIGH": 0, "CRITICAL": 0}

        result_file = (
            module_path.joinpath("build")
            .joinpath("sca")
            .joinpath("codechecker")
            .joinpath("codechecker.json")
        )
        with open(result_file, "r", encoding="utf-8") as rf:
            for report in json.load(rf).get("reports", []):
                has_reports = True
                pretty_msg = []

                severity = report.get("severity", None)
                analyzer_name = report.get("analyzer_name", None)
                checker_name = report.get("checker_name", None)
                message = report.get("message", None)
                file = report.get("file", {"path": None}).get("path", None)
                line = report.get("line", None)
                column = report.get("column", None)

                summary[severity] += 1

                pretty_msg += [
                    stylize(
                        f"{severity} [{analyzer_name}:{checker_name}]", severity_colours[severity]
                    )
                ]
                pretty_msg += [f"Message: {message}"]
                pretty_msg += [stylize("--> ", fore("blue")) + f"{file}:{line}:{column}"]
                report_line = getline(file, line).removesuffix("\n")
                pretty_msg += [stylize(" | ", fore("light_blue")) + report_line]
                cursor_line = (" " * (column - 1)) + "^"
                pretty_msg += [stylize(" | ", fore("light_blue")) + cursor_line]
                next_line = getline(file, line + 1).removesuffix("\n")
                pretty_msg += [stylize(" | ", fore("blue")) + next_line]
                next_next_line = getline(file, line + 2).removesuffix("\n")
                pretty_msg += [stylize(" | ", fore("blue")) + next_next_line]

                log.inf("\n" + "\n".join(pretty_msg))

        for file, n in calculate_file_sizes(module_path):
            has_reports = True
            pretty_msg = []

            severity = "STYLE"
            analyzer_name = "internal-analyzer"
            checker_name = "readability-file-line-limit"
            message = f"Maximum line number exceeded ({n}/{FILE_LINES_LIMIT})"

            summary[severity] += 1

            pretty_msg += [
                stylize(f"{severity} [{analyzer_name}:{checker_name}]", severity_colours[severity])
            ]
            pretty_msg += [f"Message: {message}"]
            pretty_msg += [stylize("--> ", fore("blue")) + f"{file}:{0}:{0}"]

            log.inf("\n" + "\n".join(pretty_msg))

        if has_reports:
            final_message = ["\nSummary for the diagnosed issues: "] + [
                stylize(f"{s}", severity_colours[s]) + f": {n} issues detected"
                for s, n in summary.items()
                if n
            ]
            log.inf("\n - ".join(final_message) + "\n")
            sys.exit(1)

        log.inf(stylize("No issue detected.", fore("green")))


def calculate_file_sizes(module_path: Path):
    """
    Calculate the number of lines for all the .c and .h files in the project.
    The number of lines will refer to the number of lines without counting comments and emptylines.

    Parameters
    ----------
    module_path : Path
        The module path for the Astarte device.

    Returns
    -------
    list
        A list of pairs, with the first element is the path to the file and the second element is
        the number of lines of the file.
    """
    files = (
        list(module_path.joinpath("lib").joinpath("astarte_device_sdk").glob("*.c"))
        + list(module_path.joinpath("include").joinpath("astarte_device_sdk").glob("*.h"))
        + list(
            module_path.joinpath("lib")
            .joinpath("astarte_device_sdk")
            .joinpath("include")
            .glob("*.h")
        )
    )

    file_lengths = []
    for file in files:

        with open(file, "r", encoding="utf-8") as fp:
            if any(re.match(r"^// +NOLINENUMBERLINT.*", line) for line in fp.readlines()):
                continue

        cmd = f"gcc -fpreprocessed -dD -E {file}"
        print(stylize(cmd, fore("cyan")))
        cap = subprocess.run(
            cmd,
            shell=True,
            cwd=module_path,
            timeout=60,
            check=True,
            capture_output=True,
        )
        file_line_n = len([line for line in cap.stdout.splitlines() if line.strip()])
        if file_line_n > FILE_LINES_LIMIT:
            file_lengths.append((file, file_line_n))

    return file_lengths
