import os
import sys
import re
from subprocess import Popen, PIPE

class CMakeIncFile:
    #def __init__(self) -> None:

    def Parse(self, prt2fsm, prtfile, incfile):
        #print(prt2fsm, prtfile, incfile)

        _produced=None
        _original=None
        if os.path.exists(incfile):
            with open(incfile, "rb") as _incfile:
                _original=_incfile.read()

        cmd = [prt2fsm, "-i", prtfile]
        with Popen(cmd, stdout=PIPE) as proc:
            _produced = proc.stdout.read()

        if _original != _produced:
            with open(incfile, "wb") as _incfile:
                _incfile.write(_produced)

        pass

if __name__ == "__main__":
    obj = CMakeIncFile()
    obj.Parse(sys.argv[1], sys.argv[2], sys.argv[3])
    sys.exit(0)
