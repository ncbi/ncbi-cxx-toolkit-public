import os
import sys
import re
import subprocess
from subprocess import Popen, PIPE

def MakeIncFile(outfile, cmd_args):

    returncode=1
    _produced=None
    _original=None
    if os.path.exists(outfile):
        with open(outfile, "rb") as _outfile:
            _original=_outfile.read()

    proc = subprocess.run(cmd_args, stdout=PIPE)
    returncode = proc.returncode

    if returncode == 0:
        _produced = proc.stdout
        if _original != _produced:
            with open(outfile, "wb") as _outfile:
                _outfile.write(_produced)

    return returncode

if __name__ == "__main__":
    returncode = MakeIncFile(sys.argv[1], sys.argv[2:])
    sys.exit(returncode)
