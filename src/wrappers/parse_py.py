# For a swig-produced .py, determine all of the classes,
# their base classes, and their fully qualified C++ names

import re

class ClassInfo(object):
    pass

def parse(s):
    # Python classes and their bases
    clist = re.findall('\nclass (.*)\\((.*)\\)', s)
    # qualified C++ names, extracted from __repr__ function
    qnames = re.findall('return "<C (.*) instance at',s)

    classes = {}
    for cl in clist:
        cname = cl[0]
        if not (cname.endswith('Ptr') and cname[:-3] in classes) \
               and cname != 'ncbi_object' \
               and not cname.startswith('list_') \
               and not cname.startswith('vector_') \
               and not (cname.startswith('CRef_')
                        and not cname.startswith('CRef_ext')) \
               and not cname.startswith('CConstRef_'):
            for qname in qnames:
                if qname.endswith('::' + cname):
                    cinfo = ClassInfo()
                    cinfo.py_name = cname
                    cinfo.qual_name = qname
                    cinfo.bases = []
                    for field in cl[1].split(','):
                        cinfo.bases.append(field.strip())
                    classes[cname] = cinfo
                    break
            else:
                print 'Warning: class %s skipped' % cname

    return classes


# determine whether sub "is a" super, according to classes,
# which is a dictionary like that returned by parse
def is_a(sub, super, classes):
    if sub == super:
        return 1
    for base in classes[sub].bases:
        if base in classes and is_a(base, super, classes):
            return 1
    return 0
