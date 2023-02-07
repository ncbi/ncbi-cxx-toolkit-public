import os
import sys
import re

class CBuildTestNames:
    def __init__(self, incfile) -> None:
        self.incfile = incfile
        self.new_inc_file=''
        self.old_inc_file=''
        self.all_codes=list()
        self.autofix=list()

        self.LoadIncFile()

    def Parse(self, modules):
        for n in modules:
            self.ParseFile(n)

        self.all_codes.sort()
        self.autofix.sort();

        format_str = '''
enum class eTestNames
{{
    {0},
    max_test_names,
    notset,
}};

#define DISC_AUTOFIX_TESTNAMES \\
    eTestNames::{1}

'''

        self.new_inc_file = format_str.format(',\n    '.join(self.all_codes), ",  \\\n    eTestNames::".join(self.autofix) )
        if self.new_inc_file != self.old_inc_file:
            with open(self.incfile, 'w') as file:
                file.write(self.new_inc_file)
        else:
            print("Not saving new codes")

    def ParseFile(self, filename):
        with open(filename, "r") as file:
            for line in file.readlines():
                if re.match(r'^DISCREPANCY_CASE[0-9]*\(', line):
                    code = re.sub(r'^DISCREPANCY_CASE[0-9]*\(([A-Za-z0-9_]+).*[\r\n]', r'\1', line)
                    if code:
                        self.all_codes.append(code)
                elif re.match(r'^DISCREPANCY_AUTOFIX\(', line):
                    code = re.sub(r'^DISCREPANCY_AUTOFIX\(([A-Za-z0-9_]+).*[\r\n]', r'\1', line)
                    if code:
                        self.autofix.append(code)

    def LoadIncFile(self):
        self.old_inc_file=''
        if os.path.isfile(self.incfile):
            with open(self.incfile, "r") as file:
                for line in file.readlines():
                    line=re.sub(r'[\r\n]', '', line)
                    self.old_inc_file += line
                    self.old_inc_file += "\n"
                    #self.testnames.append(line)


if __name__ == "__main__":
    obj = CBuildTestNames(sys.argv[1])
    obj.Parse(sys.argv[2:])
    sys.exit(0)
