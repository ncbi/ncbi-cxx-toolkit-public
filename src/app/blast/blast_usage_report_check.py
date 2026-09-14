#!/usr/bin/env python3
# $Id$
# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================
#
# Author: Christiam Camacho
#
# Portable checks for the blast_usage_report command line app.
#

from __future__ import annotations

import configparser
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


APP_NAME: str = "blast_usage_report"
ENV_KEYS_TO_CLEAR: tuple[str, ...] = (
    "BLAST_USAGE_REPORT",
    "DO_NOT_TRACK",
    "NCBI",
    "NCBI_CONFIG_OVERRIDES",
    "NCBI_USAGE_REPORT_ENABLED",
)
USAGE: str = f"""\
Usage: {Path(__file__).name} [blast_usage_report_executable] [options]

Run portable command-line checks for the blast_usage_report app.

Options:
  -h, --help   Show this help message and exit.
"""


class CheckError(RuntimeError):
    pass


def wants_help(argv: list[str]) -> bool:
    return any(arg in ("-h", "--help") for arg in argv[1:])


def find_app(argv: list[str]) -> Path:
    args = [
        arg for arg in argv[1:]
        if not arg.startswith("/CHECK_NAME=") and arg not in ("-h", "--help")
    ]
    if args:
        app = Path(args[0])
        if app.exists():
            return app.resolve()
        if os.name == "nt" and not app.exists():
            exe_app = app.parent / (app.name + ".exe")
            if exe_app.exists():
                return exe_app.resolve()
        return app

    names = [APP_NAME]
    if os.name == "nt":
        names.insert(0, APP_NAME + ".exe")

    for name in names:
        found = shutil.which(name)
        if found:
            return Path(found).resolve()

    for directory in (Path.cwd(), Path(__file__).resolve().parent):
        for name in names:
            candidate = directory / name
            if candidate.exists():
                return candidate.resolve()

    raise CheckError(f"Cannot find {APP_NAME}")


def make_env(home: Path) -> dict[str, str]:
    env = dict(os.environ)
    keys_to_clear = set(ENV_KEYS_TO_CLEAR)
    for key in list(env):
        if key.upper() in keys_to_clear:
            env.pop(key, None)

    env["HOME"] = str(home)
    env["USERPROFILE"] = str(home)
    env["NCBI_DONT_USE_NCBIRC"] = "1"

    if os.name == "nt":
        drive, tail = os.path.splitdrive(str(home))
        if drive:
            env["HOMEDRIVE"] = drive
            env["HOMEPATH"] = tail or "\\"

    return env


def local_config_path(home: Path) -> Path:
    return home / ("ncbi.ini" if os.name == "nt" else ".ncbirc")


def run_app(
    app: Path, args: list[str], env: dict[str, str]
) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        [str(app), *args],
        capture_output=True,
        env=env,
        text=True,
        timeout=60,
    )
    if result.returncode != 0:
        raise CheckError(
            f"{app.name} {' '.join(args)} failed with exit code "
            f"{result.returncode}\nstdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    return result


def require(condition: bool, message: str) -> None:
    if not condition:
        raise CheckError(message)


def read_usage_value(config_path: Path) -> str:
    require(config_path.exists(), f"Missing config file: {config_path}")
    parser = configparser.ConfigParser()
    parser.optionxform = str
    parser.read(config_path, encoding="utf-8")
    try:
        return (
            parser["BLAST"]["BLAST_USAGE_REPORT"]
            .strip()
            .strip("\"'")
            .lower()
        )
    except KeyError as exc:
        raise CheckError(
            f"Missing [BLAST] BLAST_USAGE_REPORT in {config_path}"
        ) from exc


def check_status(result: subprocess.CompletedProcess[str], expected: str) -> None:
    require(
        f"BLAST Usage Report : {expected}" in result.stdout,
        f"Expected status {expected!r}\nstdout:\n{result.stdout}\nstderr:\n{result.stderr}",
    )


def main(argv: list[str]) -> int:
    if wants_help(argv):
        print(USAGE, end="")
        return 0

    app = find_app(argv)
    with tempfile.TemporaryDirectory(prefix="blast_usage_report_home_") as tmp:
        home = Path(tmp)
        env = make_env(home)
        config_path = local_config_path(home)

        run_app(app, ["-on"], env)
        require(read_usage_value(config_path) == "true", "-on did not save true")

        override_env = dict(env)
        override_env["BLAST_USAGE_REPORT"] = "false"
        check_status(run_app(app, ["-status"], override_env), "Disabled")
        require(
            read_usage_value(config_path) == "true",
            "Environment override changed saved preference",
        )

        check_status(run_app(app, ["-status"], env), "Enabled")
        require(read_usage_value(config_path) == "true", "Saved preference changed")

        do_not_track_env = dict(env)
        do_not_track_env["DO_NOT_TRACK"] = "dummy"
        result = run_app(app, ["-status"], do_not_track_env)
        check_status(result, "Disabled")
        require(
            "invalid boolean value" not in (result.stdout + result.stderr),
            "DO_NOT_TRACK=dummy produced an invalid-boolean warning",
        )

        run_app(app, ["-off"], env)
        require(read_usage_value(config_path) == "false", "-off did not save false")
        check_status(run_app(app, ["-status"], env), "Disabled")

    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv))
    except CheckError as err:
        print(f"ERROR: {err}", file=sys.stderr)
        sys.exit(1)
