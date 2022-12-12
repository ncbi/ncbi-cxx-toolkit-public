import os
import sys
import re
from subprocess import Popen, PIPE

def MakeIncFile(outfile, cmd_args):

    returncode=1
    _produced=None
    _original=None
    if os.path.exists(outfile):
        with open(outfile, "rb") as _outfile:
            _original=_outfile.read()

    with Popen(cmd_args, stdout=PIPE) as proc:
        _produced = proc.stdout.read()
        returncode = proc.returncode

    if returncode == 0:
        if _original != _produced:
            with open(outfile, "wb") as _outfile:
                _outfile.write(_produced)

    return returncode

if __name__ == "__main__":
    returncode = MakeIncFile(sys.argv[1], sys.argv[2:])
    sys.exit(returncode)
