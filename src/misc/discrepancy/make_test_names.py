import os
import sys
import re

class CBuildTestNames:
    def __init__(self, incfile) -> None:
        self.incfile = incfile
        self.newcodes=list()
        self.LoadIncFile()

    def Parse(self, modules):
        for n in modules:
            self.ParseFile(n)
        self.newcodes.sort()
        if self.newcodes != self.testnames:
            with open(self.incfile, 'w') as file:
                for c in self.newcodes:
                    file.write(c + "\n")
        #else:
        #    print("Not saving new codes")

    def ParseFile(self, filename):
        with open(filename, "r") as cppfile:
            for line in cppfile:
                if re.match(r'^DISCREPANCY_CASE[0-9]*\(', line):
                    code = re.sub(r'^DISCREPANCY_CASE[0-9]*\(([A-Za-z0-9_]+).*\n', r'\1', line)
                    if code:
                        self.newcodes.append(code + ",")

    def LoadIncFile(self):
        self.testnames=list()
        print(self.incfile)
        if os.path.isfile(self.incfile):
            with open(self.incfile, "r") as file:
                for line in file.readlines():
                    line=re.sub(r'\n', '', line)
                    self.testnames.append(line)

if __name__ == "__main__":
    obj = CBuildTestNames(sys.argv[1])
    obj.Parse(sys.argv[2:])
    sys.exit(0)
