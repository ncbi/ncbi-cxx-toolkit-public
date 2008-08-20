/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * =========================================================================
 *
 * Author: Kirill Rotmistrovsky
 *
 * ========================================================================= */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "getopt.h"

int optind=1;
int optopt=-1;
int opterr=0;
char switchar = '-';
const char* optarg=0;
static char * nextarg=0;

int getopt(int argc, char ** argv, const char* optstring) 
{
	return getopt_long( argc, argv, optstring, 0, 0 );
}

int getopt_long( int argc, char ** argv, const char * optstring, const struct option * longoption, int * longindex )
{
	int parsePositional = *optstring == '-' ? ++optstring : 0;
	optarg = 0;
    while( optind < argc ) {

        if( nextarg == 0 ) {
            if( argv[optind][0] == switchar ) {
                nextarg = argv[optind] + 1;
            } else {
				if( parsePositional ) {
					// positionals are parsed as flags wich optopt of ASCII(1) as of specs
					optarg = argv[optind++];
					return optopt = 1;
				} else {
                	// first non-flag
                	return optopt = -1;
				}
            }
        } 
        switch( optopt = *nextarg ) {
        case '-': 
			if( nextarg[1] == 0 ) { nextarg = 0; ++optind; return optopt = -1; } // -- explicitely stops processing, ignored
			else if( longoption ) {
				const char * begin = nextarg + 1;
				const char * end = strchr( begin, '=' );
				nextarg = 0;
				int l = end ? end - begin : strlen( begin );
				int index = 0;
				for( ; longoption->name != 0; ++index, ++longoption ) {
					if( strncmp( longoption->name, begin, l ) == 0 && strlen( longoption->name ) == l ) {
						// found!
						if( longoption->has_arg > 0 ) {
							if( end == 0 ) {
								if( longoption->has_arg == 1 ) {
									fprintf( stderr, "getopt_long: option `--%s' requires an argument\n", longoption->name );
									++opterr;
									++optind;
									return ':';
								}
								optarg = begin + l;
							} else {
								optarg = end + 1;
							}
						} else {
							if( end != 0 ) {
								fprintf( stderr, "getopt_long: option `--%s' takes no argument\n", longoption->name );
								++opterr;
							}
							optarg = 0;
						} 
						if( longindex != 0 ) *longindex = index;
						++optind;
						if( longoption->flag ) {
							longoption->flag[0] = optopt = longoption->val;
							return 0;
						} else {
							return optopt = longoption->val;
						}
					}
				}
				fprintf( stderr, "getopt_long: unrecognized option `--%s'\n", begin );
				++optind;
				return '?';
			} else if( parsePositional ) {
				optarg = argv[optind++];
				return optopt = 1;
			} else return -1;
        case 0:   return optopt = -1; // - stops processing, passed
        default: 
            do {
                const char * x = strchr( optstring, optopt );
                if( x ) {
                    if( x[1] == ':' ) {
                        if( nextarg[1] ) {
                            optarg = nextarg + 1;
                        }
                        else {
                            if( ++optind >= argc ) {
                                fprintf( stderr,
                                         "getopt: option requires an argument -- %c\n",
                                         optopt);
                                ++opterr;
                                return ':';
                            }
                            optarg = argv[optind];
                        }
                        nextarg = 0;
                        ++optind;
                    }
                    else {
                        optarg = 0;
                        if( nextarg[1] ) ++nextarg;
                        else { nextarg = 0; ++optind; }
                    }
                    return optopt;
                } else {
                    fprintf( stderr,
                             "getopt: invalid option -- %c\n",
                             optopt);
                    if( nextarg[1] ) ++nextarg;
                    else { nextarg = 0; ++optind; }
                    ++opterr;
					return '?';
                }
            } while( 0 );
            break;
        case ':': // ':' should not be used as option
            fprintf( stderr, "getopt: invalid option -- %c\n", optopt );
            if( nextarg[1] ) ++nextarg;
            else { nextarg = 0; ++optind; }
            ++opterr;
			return ':';
            break;
        }
    }
    
    return optopt = -1;
}
    
