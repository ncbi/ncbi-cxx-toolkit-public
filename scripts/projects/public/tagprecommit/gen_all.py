#!/usr/bin/env python3

import os
import subprocess

os.chdir('c++')

rc=os.system('scripts/common/impl/generate_all_objects.sh --clients')
if(rc!=0):
  raise Exception(rc)

updated = subprocess.run(['svn', 'status'], capture_output=True)
updated = updated.stdout.split('\n')

for fname in updated:
  if not fname.startswith('?'):
    continue
  fname = fname[1:].strip()
  rc=os.system(f'svn add {fname}')
  if rc !=0:
    raise Exception(rc)

