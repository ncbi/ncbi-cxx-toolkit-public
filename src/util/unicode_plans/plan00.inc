/*  $Id$
 * ==========================================================================
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
 * ==========================================================================
 *
 * Author: Aleksey Vinokurov
 *
 * File Description:
 *    Unicode transformation library
 *    Plan 00 
 *
 */

static TUnicodePlan s_Plan_00h = {
    { 0, eString },  //                                         U+0000
    { 0, eString },  //                                         U+0001
    { 0, eString },  //                                         U+0002
    { 0, eString },  //                                         U+0003
    { 0, eString },  //                                         U+0004
    { 0, eString },  //                                         U+0005
    { 0, eString },  //                                         U+0006
    { 0, eString },  //                                         U+0007
    { 0, eString },  //                                         U+0008
    { 0, eString },  //                                         U+0009
    { 0, eString },  //                                         U+000A
    { 0, eString },  //                                         U+000B
    { 0, eString },  //                                         U+000C
    { 0, eString },  //                                         U+000D
    { 0, eString },  //                                         U+000E
    { 0, eString },  //                                         U+000F
    { 0, eString },  //                                         U+0010
    { 0, eString },  //                                         U+0011
    { 0, eString },  //                                         U+0012
    { 0, eString },  //                                         U+0013
    { 0, eString },  //                                         U+0014
    { 0, eString },  //                                         U+0015
    { 0, eString },  //                                         U+0016
    { 0, eString },  //                                         U+0017
    { 0, eString },  //                                         U+0018
    { 0, eString },  //                                         U+0019
    { 0, eString },  //                                         U+001A
    { 0, eString },  //                                         U+001B
    { 0, eString },  //                                         U+001C
    { 0, eString },  //                                         U+001D
    { 0, eString },  //                                         U+001E
    { 0, eString },  //                                         U+001F
    { " ", eString },  //                                          U+0020
    { "!", eString },  // old dictionary                           U+0021
    { "\"", eString },  // old dictionary                          U+0022
    { "#", eString },  // old dictionary                           U+0023
    { "$", eString },  // old dictionary                           U+0024
    { "%", eString },  // old dictionary                           U+0025
    { "&", eString },  // old dictionary                           U+0026
    { "'", eString },  // old dictionary                           U+0027
    { "(", eString },  // old dictionary                           U+0028
    { ")", eString },  // old dictionary                           U+0029
    { "*", eString },  // old dictionary                           U+002A
    { "+", eString },  // old dictionary                           U+002B
    { ",", eString },  // old dictionary                           U+002C
    { "-", eString },  //                                          U+002D
    { ".", eString },  // old dictionary                           U+002E
    { "/", eString },  // old dictionary                           U+002F
    { "0", eString },  //                                          U+0030
    { "1", eString },  //                                          U+0031
    { "2", eString },  //                                          U+0032
    { "3", eString },  //                                          U+0033
    { "4", eString },  //                                          U+0034
    { "5", eString },  //                                          U+0035
    { "6", eString },  //                                          U+0036
    { "7", eString },  //                                          U+0037
    { "8", eString },  //                                          U+0038
    { "9", eString },  //                                          U+0039
    { ":", eString },  // old dictionary                           U+003A
    { ";", eString },  // old dictionary                           U+003B
    { "<", eString },  // old dictionary                           U+003C
    { "=", eString },  // old dictionary                           U+003D
    { ">", eString },  // old dictionary                           U+003E
    { "?", eString },  // old dictionary                           U+003F
    { "@", eString },  // old dictionary                           U+0040
    { "A", eString },  //                                          U+0041
    { "B", eString },  //                                          U+0042
    { "C", eString },  //                                          U+0043
    { "D", eString },  //                                          U+0044
    { "E", eString },  //                                          U+0045
    { "F", eString },  //                                          U+0046
    { "G", eString },  //                                          U+0047
    { "H", eString },  //                                          U+0048
    { "I", eString },  //                                          U+0049
    { "J", eString },  //                                          U+004A
    { "K", eString },  //                                          U+004B
    { "L", eString },  //                                          U+004C
    { "M", eString },  //                                          U+004D
    { "N", eString },  //                                          U+004E
    { "O", eString },  //                                          U+004F
    { "P", eString },  //                                          U+0050
    { "Q", eString },  //                                          U+0051
    { "R", eString },  //                                          U+0052
    { "S", eString },  //                                          U+0053
    { "T", eString },  //                                          U+0054
    { "U", eString },  //                                          U+0055
    { "V", eString },  //                                          U+0056
    { "W", eString },  //                                          U+0057
    { "X", eString },  //                                          U+0058
    { "Y", eString },  //                                          U+0059
    { "Z", eString },  //                                          U+005A
    { "[", eString },  // old dictionary                           U+005B
    { "\\", eString },  // old dictionary                          U+005C
    { "]", eString },  // old dictionary                           U+005D
    { ";", eString },  // old dictionary                           U+005E
    { "_", eString },  // old dictionary                           U+005F
    { ";", eString },  // old dictionary                           U+0060
    { "a", eString },  //                                          U+0061
    { "b", eString },  //                                          U+0062
    { "c", eString },  //                                          U+0063
    { "d", eString },  //                                          U+0064
    { "e", eString },  //                                          U+0065
    { "f", eString },  //                                          U+0066
    { "g", eString },  //                                          U+0067
    { "h", eString },  //                                          U+0068
    { "i", eString },  //                                          U+0069
    { "j", eString },  //                                          U+006A
    { "k", eString },  //                                          U+006B
    { "l", eString },  //                                          U+006C
    { "m", eString },  //                                          U+006D
    { "n", eString },  //                                          U+006E
    { "o", eString },  //                                          U+006F
    { "p", eString },  //                                          U+0070
    { "q", eString },  //                                          U+0071
    { "r", eString },  //                                          U+0072
    { "s", eString },  //                                          U+0073
    { "t", eString },  //                                          U+0074
    { "u", eString },  //                                          U+0075
    { "v", eString },  //                                          U+0076
    { "w", eString },  //                                          U+0077
    { "x", eString },  //                                          U+0078
    { "y", eString },  //                                          U+0079
    { "z", eString },  //                                          U+007A
    { "{", eString },  // old dictionary                           U+007B
    { "|", eString },  // old dictionary                           U+007C
    { "}", eString },  // old dictionary                           U+007D
    { "~", eString },  //                                          U+007E
    { 0, eString },  //                                         U+007F
    { 0, eString },  //                                         U+0080
    { 0, eString },  //                                         U+0081
    { 0, eString },  //                                         U+0082
    { 0, eString },  //                                         U+0083
    { 0, eString },  //                                         U+0084
    { 0, eString },  //                                         U+0085
    { 0, eString },  //                                         U+0086
    { 0, eString },  //                                         U+0087
    { 0, eString },  //                                         U+0088
    { 0, eString },  //                                         U+0089
    { 0, eString },  //                                         U+008A
    { 0, eString },  //                                         U+008B
    { 0, eString },  //                                         U+008C
    { 0, eString },  //                                         U+008D
    { 0, eString },  //                                         U+008E
    { 0, eString },  //                                         U+008F
    { 0, eString },  //                                         U+0090
    { 0, eString },  //                                         U+0091
    { 0, eString },  //                                         U+0092
    { 0, eString },  //                                         U+0093
    { 0, eString },  //                                         U+0094
    { 0, eString },  //                                         U+0095
    { 0, eString },  //                                         U+0096
    { 0, eString },  //                                         U+0097
    { 0, eString },  //                                         U+0098
    { 0, eString },  //                                         U+0099
    { 0, eString },  //                                         U+009A
    { 0, eString },  //                                         U+009B
    { 0, eString },  //                                         U+009C
    { 0, eString },  //                                         U+009D
    { 0, eString },  //                                         U+009E
    { 0, eString },  //                                         U+009F
    { " ", eString },  // old dictionary                           U+00A0
    { " inverted exclamation mark", eString },  // old dictionary  U+00A1
    { " cent", eString },  // old dictionary                       U+00A2
    { " pound", eString },  // old dictionary                      U+00A3
    { " currency", eString },  // old dictionary                   U+00A4
    { " yen", eString },  // old dictionary                        U+00A5
    { "|", eString },  // old dictionary                           U+00A6
    { " section sign", eString },  // old dictionary               U+00A7
    { " ", eString },  // old dictionary                           U+00A8
    { "(c)", eString },  // old dictionary                         U+00A9
    { " feminine", eString },  // old dictionary                   U+00AA
    { "<<", eString },  // old dictionary                          U+00AB
    { " not", eString },  // old dictionary                        U+00AC
    { "-", eString },  // old dictionary                           U+00AD
    { "(R)", eString },  // old dictionary                         U+00AE
    { " ", eString },  // old dictionary                           U+00AF
    { " degrees ", eString },  // old dictionary                   U+00B0
    { "+/-", eString },  // old dictionary                         U+00B1
    { "(2)", eString },  // old dictionary                         U+00B2
    { "(3)", eString },  // old dictionary                         U+00B3
    { " ", eString },  // old dictionary                           U+00B4
    { "micro", eString },  //                                      U+00B5
    { " paragraph sign", eString },  // old dictionary             U+00B6
    { ".", eString },  // old dictionary           U+00B7
    { " ", eString },  // old dictionary                           U+00B8
    { "(1)", eString },  // old dictionary                         U+00B9
    { " masculine", eString },  // old dictionary                  U+00BA
    { ">>", eString },  // old dictionary                          U+00BB
    { "(1/4)", eString },  // old dictionary                       U+00BC
    { "(1/2)", eString },  // old dictionary                       U+00BD
    { "(3/4)", eString },  // old dictionary                       U+00BE
    { " inverted question mark", eString },  // old dictionary     U+00BF
    { "A", eString },  // value > 0x80 in the old dict.            U+00C0
    { "A", eString },  // value > 0x80 in the old dict.            U+00C1
    { "A", eString },  // value > 0x80 in the old dict.            U+00C2
    { "A", eString },  // value > 0x80 in the old dict.            U+00C3
    { "A", eString },  // value > 0x80 in the old dict.            U+00C4
    { "A", eString },  // value > 0x80 in the old dict.            U+00C5
    { "AE", eString },  // old dictionary                          U+00C6
    { "C", eString },  // value > 0x80 in the old dict.            U+00C7
    { "E", eString },  // value > 0x80 in the old dict.            U+00C8
    { "E", eString },  // value > 0x80 in the old dict.            U+00C9
    { "E", eString },  // value > 0x80 in the old dict.            U+00CA
    { "E", eString },  // value > 0x80 in the old dict.            U+00CB
    { "I", eString },  // value > 0x80 in the old dict.            U+00CC
    { "I", eString },  // value > 0x80 in the old dict.            U+00CD
    { "I", eString },  // value > 0x80 in the old dict.            U+00CE
    { "I", eString },  // value > 0x80 in the old dict.            U+00CF
    { "Eth", eString },  // value > 0x80 in the old dict.          U+00D0
    { "N", eString },  // value > 0x80 in the old dict.            U+00D1
    { "O", eString },  // value > 0x80 in the old dict.            U+00D2
    { "O", eString },  // value > 0x80 in the old dict.            U+00D3
    { "O", eString },  // value > 0x80 in the old dict.            U+00D4
    { "O", eString },  // value > 0x80 in the old dict.            U+00D5
    { "O", eString },  // value > 0x80 in the old dict.            U+00D6
    { "x", eString },  // old dictionary                           U+00D7
    { "O", eString },  // value > 0x80 in the old dict.            U+00D8
    { "U", eString },  // value > 0x80 in the old dict.            U+00D9
    { "U", eString },  // value > 0x80 in the old dict.            U+00DA
    { "U", eString },  // value > 0x80 in the old dict.            U+00DB
    { "U", eString },  // value > 0x80 in the old dict.            U+00DC
    { "Y", eString },  // value > 0x80 in the old dict.            U+00DD
    { "Thorn", eString },  // value > 0x80 in the old dict.        U+00DE
    { "ss", eString },  // old dictionary                          U+00DF
    { "a", eString },  // value > 0x80 in the old dict.            U+00E0
    { "a", eString },  // value > 0x80 in the old dict.            U+00E1
    { "a", eString },  // value > 0x80 in the old dict.            U+00E2
    { "a", eString },  // value > 0x80 in the old dict.            U+00E3
    { "a", eString },  // value > 0x80 in the old dict.            U+00E4
    { "a", eString },  // value > 0x80 in the old dict.            U+00E5
    { "ae", eString },  // old dictionary                          U+00E6
    { "c", eString },  // value > 0x80 in the old dict.            U+00E7
    { "e", eString },  // value > 0x80 in the old dict.            U+00E8
    { "e", eString },  // value > 0x80 in the old dict.            U+00E9
    { "e", eString },  // value > 0x80 in the old dict.            U+00EA
    { "e", eString },  // value > 0x80 in the old dict.            U+00EB
    { "i", eString },  // value > 0x80 in the old dict.            U+00EC
    { "i", eString },  // value > 0x80 in the old dict.            U+00ED
    { "i", eString },  // value > 0x80 in the old dict.            U+00EE
    { "i", eString },  // value > 0x80 in the old dict.            U+00EF
    { "eth", eString },  // value > 0x80 in the old dict.          U+00F0
    { "n", eString },  // value > 0x80 in the old dict.            U+00F1
    { "o", eString },  // value > 0x80 in the old dict.            U+00F2
    { "o", eString },  // value > 0x80 in the old dict.            U+00F3
    { "o", eString },  // value > 0x80 in the old dict.            U+00F4
    { "o", eString },  // value > 0x80 in the old dict.            U+00F5
    { "o", eString },  // value > 0x80 in the old dict.            U+00F6
    { "/", eString },  // old dictionary                           U+00F7
    { "o", eString },  // value > 0x80 in the old dict.            U+00F8
    { "u", eString },  // value > 0x80 in the old dict.            U+00F9
    { "u", eString },  // value > 0x80 in the old dict.            U+00FA
    { "u", eString },  // value > 0x80 in the old dict.            U+00FB
    { "u", eString },  // value > 0x80 in the old dict.            U+00FC
    { "y", eString },  // value > 0x80 in the old dict.            U+00FD
    { "thorn", eString },  // value > 0x80 in the old dict.        U+00FE
    { "y", eString },  // value > 0x80 in the old dict.            U+00FF
};

/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2004/05/06 18:21:25  gouriano
 * Imported from pubmed/xmldb
 *
 * ==========================================================================
 */
