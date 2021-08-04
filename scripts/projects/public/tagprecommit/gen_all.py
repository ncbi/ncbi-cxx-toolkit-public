#!/usr/bin/env python3

import os

rc=os.system('c++/scripts/common/impl/generate_all_objects.sh --clients')
if(rc!=0):
  raise Exception(rc)

