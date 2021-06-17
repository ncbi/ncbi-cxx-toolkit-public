#!/usr/bin/python

#
# Run this script without arguments to obtain usage informatioin.
#

import sys
import os

NAME_STR = 'windowmasker_2.2.22_adapter.py'
DESCR_STR = \
'old style to new style command line options\
 converter for windowmasker'
VERSION_STR = '1.0'

HELP_STR = """
------------------------------------------------------------------------------
WINDOWMASKER COMMAND LINE OPTIONS CONVERTER
------------------------------------------------------------------------------

windowmasker_2.2.22_adapter.py takes old style command line for windowmasker, 
converts it to the new format and runs the resulting command.

USAGE: windowmasker_2.2.22_adapter.py [--print-only] <wm_command> <options>

If '--print-only' flag is given as the first argument, then the resulting
command line is printed out, but windowmasker is not run.

wm_command is the path to windowmasker executable. It will be left unchanged
by the script.
------------------------------------------------------------------------------
"""

WM_NAME = 'windowmasker'

def title():
    global NAME_STR, DESCR_STR, VERSION_STR
    return NAME_STR + ': ' + DESCR_STR + '\nversion ' + VERSION_STR

def process( start_idx ):
    old_wm_opt_info = { 
        '-h'            : True, 
        '-help'         : True,
        '-xmlhelp'      : True,
        '-ustat'        : False,
        '-in'           : False,
        '-out'          : False,
        '-checkdup'     : False,
        '-window'       : False,
        '-t_extend'     : False,
        '-t_thres'      : False,
        '-t_high'       : False,
        '-t_low'        : False,
        '-set_t_high'   : False,
        '-set_t_low'    : False,
        '-infmt'        : False,
        '-parse_seqids' : False,
        '-outfmt'       : False,
        '-sformat'      : False,
        '-mk_counts'    : False,
        '-convert'      : False,
        '-fa_list'      : False,
        '-mem'          : False,
        '-smem'         : False,
        '-unit'         : False,
        '-genome_size'  : False,
        '-dust'         : False,
        '-dust_level'   : False,
        '-exclude_ids'  : False,
        '-ids'          : False,
        '-text_match'   : False,
        '-use_ba'       : False,
        '-version'      : True,
        '-version-full' : True
    }
    tmpres = []
    arg_pos = start_idx + 1
    wm_mode = 'mask'
    while True:
        if arg_pos >= len( sys.argv ): break
        arg = sys.argv[arg_pos]
        if arg not in old_wm_opt_info.keys(): tmpres.append( arg )
        else:
            if arg == '-convert':
                arg_pos += 1
                if arg_pos < len( sys.argv ):
                    if sys.argv[arg_pos] in ['true', 'T', '1']:
                        tmpres.append( arg )
                        wm_mode = 'convert'
            elif arg == '-mk_counts':
                arg_pos += 1
                if arg_pos < len( sys.argv ):
                    if sys.argv[arg_pos] in ['true', 'T', '1'] \
                            and wm_mode == 'mask':
                        tmpres.append( arg )
                        wm_mode = 'mk_counts'
            elif old_wm_opt_info[arg]: tmpres.append( arg )
            else:
                tmpres.append( arg )
                arg_pos += 1
                if arg_pos < len( sys.argv ): 
                    tmpres.append( sys.argv[arg_pos] )
        arg_pos += 1
#print tmpres, wm_mode
    excludes = {
        'mk_counts' : [
                '-outfmt',
                '-ustat',
                '-window',
                '-t_thres',
                '-t_extend',
                '-set_t_low',
                '-set_t_high',
                '-dust',
                '-dust_level',
                '-convert'
            ],
        'convert' : [
                '-mk_counts',
                '-ustat',
                '-checkdup',
                '-window',
                '-t_extend',
                '-t_thres',
                '-t_high',
                '-t_low',
                '-set_t_high',
                '-set_t_low',
                '-infmt',
                '-outfmt',
                '-parse_seqids',
                '-fa_list',
                '-mem',
                '-unit',
                '-genome_size',
                '-dust',
                '-dust_level',
                '-exclude_ids',
                '-ids',
                '-text_match',
                '-use_ba'
            ],
        'mask' : [
                '-convert',
                '-mk_counts',
                '-checkdup',
                '-fa_list',
                '-mem',
                '-unit',
                '-genome_list',
                '-sformat',
                '-smem',
            ]
    }
    result = []
    state = 'check'
    for arg in tmpres:
        if state == 'skip': state = 'check'
        elif state == 'copy': 
            result.append( arg )
            state = 'check'
        elif arg in old_wm_opt_info.keys():
            if arg in excludes[wm_mode]:
                if not old_wm_opt_info[arg]: state = 'skip'
            else:
                result.append( arg )
                if not old_wm_opt_info[arg]: state = 'copy'
        else: result.append( arg )
    return result

def main():
    global HELP_STR
    mode = 'exec'
    if len( sys.argv ) <= 1: 
        print HELP_STR
        return
    else:
         if sys.argv[1] == '--print-only': 
            mode = 'print'
            if len( sys.argv ) <= 2: 
                print HELP_STR
                return
            else: 
                  wm_name = sys.argv[2]
                  start_idx = 2
         else: 
            wm_name = sys.argv[1]
            start_idx = 1
    new_args = process( start_idx )
    result_str = wm_name + ' ' + ' '.join( new_args )
    if mode == 'print': print result_str
    else:
        print 'running "' + result_str + '"'
        os.system( result_str )

if __name__ == '__main__':
    main()

