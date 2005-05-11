#!/usr/bin/env python

# $Id$
#
# Based on *.swig files, look at
# certain header files and instantiate some templates
# for SWIG.
#
# Author: Josh Cherry

import sys, re, os
import sppp

classes = {}
classes['list'] = {}
classes['vector'] = {}
classes['list_cref'] = {}
classes['vector_cref'] = {}
classes['cref'] = {}
classes['cconstref'] = {}

# keep track of things we skip for certain reasons
skipped = {}

# keys = classes, values = module they're found in
class_list = {}

banned = {'int': '', 'double': '', 'bool': '', 'string': '',
          'char': '', 'TSeqPos': '', 'Int8': ''}

for iname in sys.argv[1:]:
    headers = sppp.ProcessFile(iname)
    for header in headers:
        hname = os.environ['NCBI_INCLUDE'] + '/' + header
        if not os.path.exists(hname):
            sys.stderr.write('%s does not exist\n' % hname)
        else:
            fid = open(hname)
            h = fid.read()
            fid.close()

            pat = r'typedef *(list|vector) *< *([^ ].*[^ ]) *>'
            for template in re.findall(pat, h):
                (container, contained) = template
                m = re.match('CRef *< *([^ ]*) *>', contained)
                if m:
                    # a container of CRef's
                    classes[container + '_cref'][m.groups()[0]] = ''
                    classes['cref'][m.groups()[0]] = ''
                else:
                    # a container of non-CRef's
                    contained = contained.split('::')[-1]
                    if re.match('.*<.*', contained) \
                           or banned.has_key(contained)\
                           or (contained[0] != 'C' and contained[0] != 'S'):
                        # a template of templates or a "banned" type; skip it
                        skipped['%s %s' % template] = ''
                        continue
                    classes[container][contained] = ''
            # all CRefs mentioned anywhere
            for cref in re.findall('CRef *< *([^ ]+) *>', h):
                # get rid of qualifiers
                cref = cref.split('::')[-1]
                classes['cref'][cref] = ''
            # all CConstRefs
            for cconstref in re.findall('CConstRef *< *([^ ]+) *>', h):
                # get rid of qualifiers
                cref = cref.split('::')[-1]
                classes['cconstref'][cconstref] = ''

    # find all classes defined by this module
    fid = open(os.path.splitext(iname)[0] + '.py')
    py = fid.read()
    fid.close()
    import parse_py
    cl_list = parse_py.parse(py)
    for cl in cl_list:
        class_list[cl] = (iname, cl_list[cl].qual_name)
            

# some filtering
cref_blacklist = ['CMappedGraph',     # not a CObject
                  # public method in parent redeclared non-public
                  'CSeq_entry_Info', 'CBioseq_Info', 'CTSE_Info',
                  'CSeq_annot_SNP_Info']
for ref_type in ['cref', 'cconstref']:
    for klass in classes[ref_type].keys():
        if klass in cref_blacklist:
            del classes[ref_type][klass]


# open some files for output
files = {}
for iname in sys.argv[1:]:
    files[iname] = open(os.path.splitext(iname)[0] + '_templates.i', 'w')
    files[iname].write('using namespace ncbi;\n' \
                       'using namespace ncbi::objects;\n' \
                       'using namespace ncbi::blast;\n\n')


# write %template directives to appropriate files

# CRefs before containers, so containers of CRefs use typemaps
for class_name in classes['cref'].keys():
    if class_list.has_key(class_name):
        fname = class_list[class_name][0]
        qual_name = class_list[class_name][1]
        files[fname].write('%%template() ncbi::CRef<%s>;\n' % (qual_name))


for class_name in classes['cconstref'].keys():
    if class_list.has_key(class_name):
        fname = class_list[class_name][0]
        qual_name = class_list[class_name][1]
        files[fname].write('%%template(CConstRef_%s) ncbi::CConstRef<%s>;\n'
                           % (class_name, qual_name))


for class_name in classes['list_cref'].keys():
    if class_list.has_key(class_name):
        fname = class_list[class_name][0]
        qual_name = class_list[class_name][1]
        files[fname].write('LIST_CREF(list_CRef_%s, %s);\n'
                           % (class_name, qual_name))
        
for class_name in classes['vector_cref'].keys():
    if class_list.has_key(class_name):
        fname = class_list[class_name][0]
        qual_name = class_list[class_name][1]
        files[fname].write('VECTOR_CREF(vector_CRef_%s, %s);\n'
                           % (class_name, qual_name))


for class_name in classes['list'].keys():
    if class_list.has_key(class_name):
        fname = class_list[class_name][0]
        qual_name = class_list[class_name][1]
        files[fname].write('%%template(list_%s) std::list<%s>;\n'
                           % (class_name, qual_name))


for class_name in classes['vector'].keys():
    if class_list.has_key(class_name):
        fname = class_list[class_name][0]
        qual_name = class_list[class_name][1]
        files[fname].write('%%template(vector_%s) std::vector<%s>;\n'
                           % (class_name, qual_name))
        

# close the output files
for iname in sys.argv[1:]:
    files[iname].close()


# report some skipped templates
for s in skipped:
    print 'skipped %s' % s
