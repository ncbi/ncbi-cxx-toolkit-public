#!/usr/bin/env python
 
# This script determines the frame offset of J gene for Human and Mouse
# It uses the chain type and sequence length for J genes

offset_table = {'Human': {'JH': 1, 'JK': 1, 'JL': 1},
                'Mouse': {'JH': 3, 'JK': 3, 'JL': 1}}

import subprocess

for origin in ['Human', 'Mouse']:

    # read and parse chain type info
    chain_type = {}
    for line in  open(origin + '_gl.ct', 'r').readlines():
        line = line.strip()
        if len(line) == 0 or line[0]=='#': continue
        id, ct = line.split()
        chain_type[id] = ct

    # write out frame info
    f = open(origin + '_gl_J.nfm', 'w')
    f.write('# The first coding frame position within the J sequence.\n\n')
    items = subprocess.Popen('blastdbcmd -entry all -db ' + origin + '_gl_J -dbtype nucl -outfmt "%a=%l"', 
                             shell=True, stdout=subprocess.PIPE).communicate()[0].split()
    for item in items:
        id, slen = item.split('=')
        offset = (int(slen) - offset_table[origin][chain_type[id]]) % 3
        f.write('%s\t%d\n' %(id, offset))
    f.close()

    f = open(origin + '_gl_V.nfm', 'w')
    f.write('# The first coding frame position within the V sequence.\n\n')
    for line in open(origin + '_gl_V.n.dm.kabat').readlines():
	line = line.strip()
	if len(line) == 0 or line[0]=='#': continue
	id = line.split()[0]
    	f.write('%s\t0\n' %id)
    f.close()
