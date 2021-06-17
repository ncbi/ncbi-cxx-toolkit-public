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
 *
 *    File contains the following plans
 *     00, 01, 02, 03, 04,
 *     1E,
 *     20, 21, 22, 23, 24, 25, 26, 27,
 *     30,
 *     E0, E2, E3, E4, E5, E6, E7, E8, EA, EB,
 *     FB, FE
 */

static TUnicodePlan s_Plan_00h = {
    { "\x00", eString },  //                                         U+0000
    { "\x01", eString },  //                                         U+0001
    { "\x02", eString },  //                                         U+0002
    { "\x03", eString },  //                                         U+0003
    { "\x04", eString },  //                                         U+0004
    { "\x05", eString },  //                                         U+0005
    { "\x06", eString },  //                                         U+0006
    { "\x07", eString },  //                                         U+0007
    { "\x08", eString },  //                                         U+0008
    { "\x09", eString },  //                                         U+0009
    { "\x0A", eString },  //                                         U+000A
    { "\x0B", eString },  //                                         U+000B
    { "\x0C", eString },  //                                         U+000C
    { "\x0D", eString },  //                                         U+000D
    { "\x0E", eString },  //                                         U+000E
    { "\x0F", eString },  //                                         U+000F
    { "\x10", eString },  //                                         U+0010
    { "\x11", eString },  //                                         U+0011
    { "\x12", eString },  //                                         U+0012
    { "\x13", eString },  //                                         U+0013
    { "\x14", eString },  //                                         U+0014
    { "\x15", eString },  //                                         U+0015
    { "\x16", eString },  //                                         U+0016
    { "\x17", eString },  //                                         U+0017
    { "\x18", eString },  //                                         U+0018
    { "\x19", eString },  //                                         U+0019
    { "\x1A", eString },  //                                         U+001A
    { "\x1B", eString },  //                                         U+001B
    { "\x1C", eString },  //                                         U+001C
    { "\x1D", eString },  //                                         U+001D
    { "\x1E", eString },  //                                         U+001E
    { "\x1F", eString },  //                                         U+001F
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
    { "^", eString },  // old dictionary                           U+005E
    { "_", eString },  // old dictionary                           U+005F
    { "`", eString },  // old dictionary                           U+0060
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

static TUnicodePlan s_Plan_01h = {
    { "A", eString },  // old dictionary     U+0100
    { "a", eString },  //                    U+0101
    { "A", eString },  // old dictionary     U+0102
    { "a", eString },  //                    U+0103
    { "A", eString },  // old dictionary     U+0104
    { "a", eString },  // old dictionary     U+0105
    { "C", eString },  // old dictionary     U+0106
    { "c", eString },  // old dictionary     U+0107
    { "C", eString },  // old dictionary     U+0108
    { "c", eString },  //                    U+0109
    { "C", eString },  // old dictionary     U+010A
    { "c", eString },  // old dictionary     U+010B
    { "C", eString },  // old dictionary     U+010C
    { "c", eString },  // old dictionary     U+010D
    { "D", eString },  // old dictionary     U+010E
    { "d", eString },  // old dictionary     U+010F
    { "D", eString },  // old dictionary     U+0110
    { "d", eString },  // old dictionary     U+0111
    { "E", eString },  // old dictionary     U+0112
    { "e", eString },  //                    U+0113
    { "E", eString },  //                    U+0114
    { "e", eString },  //                    U+0115
    { "E", eString },  // old dictionary     U+0116
    { "e", eString },  // old dictionary     U+0117
    { "E", eString },  // old dictionary     U+0118
    { "e", eString },  // old dictionary     U+0119
    { "E", eString },  // old dictionary     U+011A
    { "e", eString },  // old dictionary     U+011B
    { "G", eString },  // old dictionary     U+011C
    { "g", eString },  //                    U+011D
    { "G", eString },  // old dictionary     U+011E
    { "g", eString },  //                    U+011F
    { "G", eString },  // old dictionary     U+0120
    { "g", eString },  // old dictionary     U+0121
    { "G", eString },  // old dictionary     U+0122
    { "g", eString },  //                    U+0123
    { "H", eString },  // old dictionary     U+0124
    { "h", eString },  //                    U+0125
    { "H", eString },  // old dictionary     U+0126
    { "h", eString },  // old dictionary     U+0127
    { "I", eString },  // old dictionary     U+0128
    { "ij", eString },  //                   U+0129
    { "I", eString },  // old dictionary     U+012A
    { "ij", eString },  //                   U+012B
    { "I", eString },  //                    U+012C
    { "i", eString },  //                    U+012D
    { "I", eString },  // old dictionary     U+012E
    { "i", eString },  // old dictionary     U+012F
    { "I", eString },  // old dictionary     U+0130
    { "i", eString },  // old dictionary     U+0131
    { "IJ", eString },  // old dictionary    U+0132
    { "ij", eString },  // old dictionary    U+0133
    { "J", eString },  // old dictionary     U+0134
    { "j", eString },  //                    U+0135
    { "K", eString },  // old dictionary     U+0136
    { "k", eString },  //                    U+0137
    { "k", eString },  // old dictionary     U+0138
    { "L", eString },  // old dictionary     U+0139
    { "l", eString },  // old dictionary     U+013A
    { "L", eString },  // old dictionary     U+013B
    { "l", eString },  //                    U+013C
    { "L", eString },  // old dictionary     U+013D
    { "l", eString },  // old dictionary     U+013E
    { "L", eString },  // old dictionary     U+013F
    { "l", eString },  // old dictionary     U+0140
    { "L", eString },  // old dictionary     U+0141
    { "l", eString },  // old dictionary     U+0142
    { "N", eString },  // old dictionary     U+0143
    { "n", eString },  // old dictionary     U+0144
    { "N", eString },  // old dictionary     U+0145
    { "n", eString },  //                    U+0146
    { "N", eString },  // old dictionary     U+0147
    { "n", eString },  // old dictionary     U+0148
    { "n", eString },  // old dictionary     U+0149
    { " ENG", eString },  // old dictionary  U+014A
    { " eng", eString },  // old dictionary  U+014B
    { "O", eString },  // old dictionary     U+014C
    { "o", eString },  //                    U+014D
    { "O", eString },  //                    U+014E
    { "o", eString },  //                    U+014F
    { "O", eString },  // old dictionary     U+0150
    { "o", eString },  // old dictionary     U+0151
    { "OE", eString },  // old dictionary    U+0152
    { "oe", eString },  // old dictionary    U+0153
    { "R", eString },  // old dictionary     U+0154
    { "r", eString },  //                    U+0155
    { "R", eString },  // old dictionary     U+0156
    { "r", eString },  //                    U+0157
    { "R", eString },  // old dictionary     U+0158
    { "r", eString },  // old dictionary     U+0159
    { "S", eString },  // old dictionary     U+015A
    { "s", eString },  //                    U+015B
    { "S", eString },  // old dictionary     U+015C
    { "s", eString },  //                    U+015D
    { "S", eString },  // old dictionary     U+015E
    { "s", eString },  //                    U+015F
    { "S", eString },  // old dictionary     U+0160
    { "s", eString },  // old dictionary     U+0161
    { "T", eString },  // old dictionary     U+0162
    { "t", eString },  //                    U+0163
    { "T", eString },  // old dictionary     U+0164
    { "t", eString },  // old dictionary     U+0165
    { "T", eString },  // old dictionary     U+0166
    { "t", eString },  // old dictionary     U+0167
    { "U", eString },  // old dictionary     U+0168
    { "u", eString },  //                    U+0169
    { "U", eString },  // old dictionary     U+016A
    { "u", eString },  //                    U+016B
    { "U", eString },  // old dictionary     U+016C
    { "u", eString },  //                    U+016D
    { "U", eString },  // old dictionary     U+016E
    { "u", eString },  //                    U+016F
    { "U", eString },  // old dictionary     U+0170
    { "u", eString },  // old dictionary     U+0171
    { "U", eString },  // old dictionary     U+0172
    { "u", eString },  // old dictionary     U+0173
    { "W", eString },  // old dictionary     U+0174
    { "w", eString },  //                    U+0175
    { "Y", eString },  // old dictionary     U+0176
    { "y", eString },  //                    U+0177
    { "Y", eString },  // old dictionary     U+0178
    { "Z", eString },  // old dictionary     U+0179
    { "z", eString },  // old dictionary     U+017A
    { "Z", eString },  // old dictionary     U+017B
    { "z", eString },  // old dictionary     U+017C
    { "Z", eString },  // old dictionary     U+017D
    { "z", eString },  // old dictionary     U+017E
    { 0, eString },  //                   U+017F
    { "b", eString },  //                    U+0180
    { "B", eString },  //                    U+0181
    { 0, eString },  //                   U+0182
    { 0, eString },  //                   U+0183
    { 0, eString },  //                   U+0184
    { 0, eString },  //                   U+0185
    { 0, eString },  //                   U+0186
    { "C", eString },  //                    U+0187
    { "c", eString },  //                    U+0188
    { "D", eString },  //                    U+0189
    { "D", eString },  //                    U+018A
    { 0, eString },  //                   U+018B
    { 0, eString },  //                   U+018C
    { 0, eString },  //                   U+018D
    { 0, eString },  //                   U+018E
    { 0, eString },  //                   U+018F
    { "E", eString },  //                    U+0190
    { "F", eString },  //                    U+0191
    { "f", eString },  // old dictionary     U+0192
    { "G", eString },  //                    U+0193
    { 0, eString },  //                   U+0194
    { 0, eString },  //                   U+0195
    { 0, eString },  //                   U+0196
    { "I", eString },  //                    U+0197
    { "K", eString },  //                    U+0198
    { "k", eString },  //                    U+0199
    { 0, eString },  //                   U+019A
    { 0, eString },  //                   U+019B
    { 0, eString },  //                   U+019C
    { "N", eString },  //                    U+019D
    { "n", eString },  //                    U+019E
    { 0, eString },  //                   U+019F
    { "O", eString },  //                    U+01A0
    { "o", eString },  //                    U+01A1
    { 0, eString },  //                   U+01A2
    { 0, eString },  //                   U+01A3
    { "P", eString },  //                    U+01A4
    { "p", eString },  //                    U+01A5
    { "R", eString },  //                    U+01A6
    { 0, eString },  //                   U+01A7
    { 0, eString },  //                   U+01A8
    { 0, eString },  //                   U+01A9
    { 0, eString },  //                   U+01AA
    { "t", eString },  //                    U+01AB
    { "T", eString },  //                    U+01AC
    { "t", eString },  //                    U+01AD
    { "T", eString },  //                    U+01AE
    { "U", eString },  //                    U+01AF
    { "u", eString },  //                    U+01B0
    { 0, eString },  //                   U+01B1
    { 0, eString },  //                   U+01B2
    { "Y", eString },  //                    U+01B3
    { "y", eString },  //                    U+01B4
    { "Z", eString },  //                    U+01B5
    { "z", eString },  //                    U+01B6
    { "Z", eString },  //                    U+01B7
    { 0, eString },  //                   U+01B8
    { 0, eString },  //                   U+01B9
    { "z", eString },  //                    U+01BA
    { 0, eString },  //                   U+01BB
    { 0, eString },  //                   U+01BC
    { 0, eString },  //                   U+01BD
    { 0, eString },  //                   U+01BE
    { 0, eString },  //                   U+01BF
    { 0, eString },  //                   U+01C0
    { 0, eString },  //                   U+01C1
    { 0, eString },  //                   U+01C2
    { "!", eString },  //                    U+01C3
    { "DZ", eString },  //                   U+01C4
    { "Dz", eString },  //                   U+01C5
    { "dz", eString },  //                   U+01C6
    { "LJ", eString },  //                   U+01C7
    { "Lj", eString },  //                   U+01C8
    { "lj", eString },  //                   U+01C9
    { "NJ", eString },  //                   U+01CA
    { "Nj", eString },  //                   U+01CB
    { "nj", eString },  //                   U+01CC
    { "A", eString },  //                    U+01CD
    { "a", eString },  //                    U+01CE
    { "I", eString },  //                    U+01CF
    { "i", eString },  //                    U+01D0
    { "O", eString },  //                    U+01D1
    { "o", eString },  //                    U+01D2
    { "U", eString },  //                    U+01D3
    { "u", eString },  //                    U+01D4
    { "U", eString },  //                    U+01D5
    { "u", eString },  //                    U+01D6
    { "U", eString },  //                    U+01D7
    { "u", eString },  //                    U+01D8
    { "U", eString },  //                    U+01D9
    { "u", eString },  //                    U+01DA
    { "U", eString },  //                    U+01DB
    { "u", eString },  //                    U+01DC
    { 0, eString },  //                   U+01DD
    { "A", eString },  //                    U+01DE
    { "a", eString },  //                    U+01DF
    { "A", eString },  //                    U+01E0
    { "a", eString },  //                    U+01E1
    { "AE", eString },  //                   U+01E2
    { "ae", eString },  //                   U+01E3
    { "G", eString },  //                    U+01E4
    { "g", eString },  //                    U+01E5
    { "G", eString },  //                    U+01E6
    { "g", eString },  //                    U+01E7
    { "K", eString },  //                    U+01E8
    { "k", eString },  //                    U+01E9
    { "O", eString },  //                    U+01EA
    { "o", eString },  //                    U+01EB
    { "O", eString },  //                    U+01EC
    { "o", eString },  //                    U+01ED
    { "Z", eString },  //                    U+01EE
    { "z", eString },  //                    U+01EF
    { "j", eString },  //                    U+01F0
    { "DZ", eString },  //                   U+01F1
    { "Dz", eString },  //                   U+01F2
    { "dz", eString },  //                   U+01F3
    { "G", eString },  //                    U+01F4
    { "g", eString },  //                    U+01F5
    { 0, eString },  //                   U+01F6
    { 0, eString },  //                   U+01F7
    { "N", eString },  //                    U+01F8
    { "n", eString },  //                    U+01F9
    { "A", eString },  //                    U+01FA
    { "a", eString },  //                    U+01FB
    { "AE", eString },  //                   U+01FC
    { "ae", eString },  //                   U+01FD
    { "O", eString },  //                    U+01FE
    { "o", eString },  //                    U+01FF
};

static TUnicodePlan s_Plan_02h = {
    { "A", eString },  //                          U+0200
    { "a", eString },  //                          U+0201
    { "A", eString },  //                          U+0202
    { "a", eString },  //                          U+0203
    { "E", eString },  //                          U+0204
    { "e", eString },  //                          U+0205
    { "E", eString },  //                          U+0206
    { "e", eString },  //                          U+0207
    { "I", eString },  //                          U+0208
    { "i", eString },  //                          U+0209
    { "I", eString },  //                          U+020A
    { "i", eString },  //                          U+020B
    { "O", eString },  //                          U+020C
    { "o", eString },  //                          U+020D
    { "O", eString },  //                          U+020E
    { "o", eString },  //                          U+020F
    { "R", eString },  //                          U+0210
    { "r", eString },  //                          U+0211
    { "R", eString },  //                          U+0212
    { "r", eString },  //                          U+0213
    { "U", eString },  //                          U+0214
    { "u", eString },  //                          U+0215
    { "U", eString },  //                          U+0216
    { "u", eString },  //                          U+0217
    { "S", eString },  //                          U+0218
    { "s", eString },  //                          U+0219
    { "T", eString },  //                          U+021A
    { "t", eString },  //                          U+021B
    { 0, eString },  //                         U+021C
    { 0, eString },  //                         U+021D
    { "H", eString },  //                          U+021E
    { "h", eString },  //                          U+021F
    { 0, eString },  //                         U+0220
    { 0, eString },  //                         U+0221
    { 0, eString },  //                         U+0222
    { 0, eString },  //                         U+0223
    { "Z", eString },  //                          U+0224
    { "z", eString },  //                          U+0225
    { "A", eString },  //                          U+0226
    { "a", eString },  //                          U+0227
    { "E", eString },  //                          U+0228
    { "e", eString },  //                          U+0229
    { "O", eString },  //                          U+022A
    { "o", eString },  //                          U+022B
    { "O", eString },  //                          U+022C
    { "o", eString },  //                          U+022D
    { "O", eString },  //                          U+022E
    { "o", eString },  //                          U+022F
    { "O", eString },  //                          U+0230
    { "o", eString },  //                          U+0231
    { "Y", eString },  //                          U+0232
    { "y", eString },  //                          U+0233
    { 0, eString },  //                         U+0234
    { 0, eString },  //                         U+0235
    { 0, eString },  //                         U+0236
    { 0, eString },  //                         U+0237
    { 0, eString },  //                         U+0238
    { 0, eString },  //                         U+0239
    { 0, eString },  //                         U+023A
    { 0, eString },  //                         U+023B
    { 0, eString },  //                         U+023C
    { 0, eString },  //                         U+023D
    { 0, eString },  //                         U+023E
    { 0, eString },  //                         U+023F
    { 0, eString },  //                         U+0240
    { 0, eString },  //                         U+0241
    { 0, eString },  //                         U+0242
    { 0, eString },  //                         U+0243
    { 0, eString },  //                         U+0244
    { 0, eString },  //                         U+0245
    { 0, eString },  //                         U+0246
    { 0, eString },  //                         U+0247
    { 0, eString },  //                         U+0248
    { 0, eString },  //                         U+0249
    { 0, eString },  //                         U+024A
    { 0, eString },  //                         U+024B
    { 0, eString },  //                         U+024C
    { 0, eString },  //                         U+024D
    { 0, eString },  //                         U+024E
    { 0, eString },  //                         U+024F
    { 0, eString },  //                         U+0250
    { "a", eString },  //                          U+0251
    { 0, eString },  //                         U+0252
    { 0, eString },  //                         U+0253
    { "open o", eString },  //                     U+0254
    { 0, eString },  //                         U+0255
    { "d", eString },  //                          U+0256
    { "d", eString },  //                          U+0257
    { 0, eString },  //                         U+0258
    { 0, eString },  //                         U+0259
    { 0, eString },  //                         U+025A
    { "varepsilon", eString },  // old dictionary  U+025B
    { 0, eString },  //                         U+025C
    { 0, eString },  //                         U+025D
    { 0, eString },  //                         U+025E
    { 0, eString },  //                         U+025F
    { "g", eString },  //                          U+0260
    { "g", eString },  //                          U+0261
    { "G", eString },  //                          U+0262
    { 0, eString },  //                         U+0263
    { 0, eString },  //                         U+0264
    { 0, eString },  //                         U+0265
    { "h", eString },  //                          U+0266
    { "h", eString },  //                          U+0267
    { "i", eString },  //                          U+0268
    { "i", eString },  //                          U+0269
    { "I", eString },  //                          U+026A
    { 0, eString },  //                         U+026B
    { 0, eString },  //                         U+026C
    { 0, eString },  //                         U+026D
    { 0, eString },  //                         U+026E
    { 0, eString },  //                         U+026F
    { 0, eString },  //                         U+0270
    { "m", eString },  //                          U+0271
    { 0, eString },  //                         U+0272
    { "n", eString },  //                          U+0273
    { "N", eString },  //                          U+0274
    { 0, eString },  //                         U+0275
    { 0, eString },  //                         U+0276
    { 0, eString },  //                         U+0277
    { 0, eString },  //                         U+0278
    { 0, eString },  //                         U+0279
    { 0, eString },  //                         U+027A
    { 0, eString },  //                         U+027B
    { 0, eString },  //                         U+027C
    { 0, eString },  //                         U+027D
    { 0, eString },  //                         U+027E
    { 0, eString },  //                         U+027F
    { "R", eString },  //                          U+0280
    { 0, eString },  //                         U+0281
    { "s", eString },  //                          U+0282
    { 0, eString },  //                         U+0283
    { 0, eString },  //                         U+0284
    { 0, eString },  //                         U+0285
    { 0, eString },  //                         U+0286
    { 0, eString },  //                         U+0287
    { "t", eString },  //                          U+0288
    { "u", eString },  //                          U+0289
    { 0, eString },  //                         U+028A
    { 0, eString },  //                         U+028B
    { 0, eString },  //                         U+028C
    { 0, eString },  //                         U+028D
    { 0, eString },  //                         U+028E
    { "Y", eString },  //                          U+028F
    { "Z", eString },  //                          U+0290
    { "Z", eString },  //                          U+0291
    { "z", eString },  //                          U+0292
    { "z", eString },  //                          U+0293
    { 0, eString },  //                         U+0294
    { 0, eString },  //                         U+0295
    { 0, eString },  //                         U+0296
    { 0, eString },  //                         U+0297
    { "O", eString },  //                          U+0298
    { "B", eString },  //                          U+0299
    { 0, eString },  //                         U+029A
    { "G", eString },  //                          U+029B
    { "H", eString },  //                          U+029C
    { "j", eString },  //                          U+029D
    { 0, eString },  //                         U+029E
    { "L", eString },  //                          U+029F
    { "q", eString },  //                          U+02A0
    { 0, eString },  //                         U+02A1
    { 0, eString },  //                         U+02A2
    { "dz", eString },  //                         U+02A3
    { "dz", eString },  //                         U+02A4
    { 0, eString },  //                         U+02A5
    { "ts", eString },  //                         U+02A6
    { 0, eString },  //                         U+02A7
    { 0, eString },  //                         U+02A8
    { 0, eString },  //                         U+02A9
    { "ls", eString },  //                         U+02AA
    { "lz", eString },  //                         U+02AB
    { "ww", eString },  //                         U+02AC
    { 0, eString },  //                         U+02AD
    { 0, eString },  //                         U+02AE
    { 0, eString },  //                         U+02AF
    { "h", eString },  //                          U+02B0
    { "h", eString },  //                          U+02B1
    { "j", eString },  //                          U+02B2
    { "r", eString },  //                          U+02B3
    { 0, eString },  //                         U+02B4
    { 0, eString },  //                         U+02B5
    { 0, eString },  //                         U+02B6
    { "w", eString },  //                          U+02B7
    { "y", eString },  //                          U+02B8
    { "'", eString },  //                          U+02B9
    { "\"", eString },  //                         U+02BA
    { "'", eString },  //                          U+02BB
    { "'", eString },  //                          U+02BC
    { "'", eString },  //                          U+02BD
    { "'", eString },  //                          U+02BE
    { "'", eString },  //                          U+02BF
    { "?", eString },  //                          U+02C0
    { "?", eString },  //                          U+02C1
    { "<", eString },  //                          U+02C2
    { ">", eString },  //                          U+02C3
    { "^", eString },  //                          U+02C4
    { "v", eString },  //                          U+02C5
    { "^", eString },  //                          U+02C6
    { " ", eString },  // old dictionary           U+02C7
    { "'", eString },  //                          U+02C8
    { "-", eString },  //                          U+02C9
    { "'", eString },  //                          U+02CA
    { "`", eString },  //                          U+02CB
    { "'", eString },  //                          U+02CC
    { "_", eString },  //                          U+02CD
    { "'", eString },  //                          U+02CE
    { "`", eString },  //                          U+02CF
    { 0, eString },  //                         U+02D0
    { 0, eString },  //                         U+02D1
    { "'", eString },  //                          U+02D2
    { "'", eString },  //                          U+02D3
    { 0, eString },  //                         U+02D4
    { 0, eString },  //                         U+02D5
    { "+", eString },  //                          U+02D6
    { "-", eString },  //                          U+02D7
    { " ", eString },  // old dictionary           U+02D8
    { " ", eString },  // old dictionary           U+02D9
    { " ", eString },  // old dictionary           U+02DA
    { " ", eString },  // old dictionary           U+02DB
    { " ", eString },  // old dictionary           U+02DC
    { " ", eString },  // old dictionary           U+02DD
    { 0, eString },  //                         U+02DE
    { "x", eString },  //                          U+02DF
    { 0, eString },  //                         U+02E0
    { 0, eString },  //                         U+02E1
    { 0, eString },  //                         U+02E2
    { 0, eString },  //                         U+02E3
    { 0, eString },  //                         U+02E4
    { 0, eString },  //                         U+02E5
    { 0, eString },  //                         U+02E6
    { 0, eString },  //                         U+02E7
    { 0, eString },  //                         U+02E8
    { 0, eString },  //                         U+02E9
    { 0, eString },  //                         U+02EA
    { 0, eString },  //                         U+02EB
    { 0, eString },  //                         U+02EC
    { 0, eString },  //                         U+02ED
    { 0, eString },  //                         U+02EE
    { 0, eString },  //                         U+02EF
    { 0, eString },  //                         U+02F0
    { "l", eString },  //                          U+02F1
    { "s", eString },  //                          U+02F2
    { "x", eString },  //                          U+02F3
    { 0, eString },  //                         U+02F4
    { 0, eString },  //                         U+02F5
    { 0, eString },  //                         U+02F6
    { 0, eString },  //                         U+02F7
    { 0, eString },  //                         U+02F8
    { 0, eString },  //                         U+02F9
    { 0, eString },  //                         U+02FA
    { 0, eString },  //                         U+02FB
    { "v", eString },  //                          U+02FC
    { "=", eString },  //                          U+02FD
    { "\"", eString },  //                         U+02FE
    { 0, eString },  //                         U+02FF
};

static TUnicodePlan s_Plan_03h = {
    { "", eString },  //                         U+0300
    { "", eString },  //                         U+0301
    { "", eString },  //                         U+0302
    { "", eString },  //                         U+0303
    { "", eString },  //                         U+0304
    { "", eString },  //                         U+0305
    { "", eString },  //                         U+0306
    { "", eString },  //                         U+0307
    { "", eString },  //                         U+0308
    { "", eString },  //                         U+0309
    { "", eString },  //                         U+030A
    { "", eString },  //                         U+030B
    { "", eString },  //                         U+030C
    { "", eString },  //                         U+030D
    { "", eString },  //                         U+030E
    { "", eString },  //                         U+030F
    { "", eString },  //                         U+0310
    { "", eString },  //                         U+0311
    { "", eString },  //                         U+0312
    { "", eString },  //                         U+0313
    { "", eString },  //                         U+0314
    { "", eString },  //                         U+0315
    { "", eString },  //                         U+0316
    { "", eString },  //                         U+0317
    { "", eString },  //                         U+0318
    { "", eString },  //                         U+0319
    { "", eString },  //                         U+031A
    { "", eString },  //                         U+031B
    { "", eString },  //                         U+031C
    { "", eString },  //                         U+031D
    { "", eString },  //                         U+031E
    { "", eString },  //                         U+031F
    { "", eString },  //                         U+0320
    { "", eString },  //                         U+0321
    { "", eString },  //                         U+0322
    { "", eString },  //                         U+0323
    { "", eString },  //                         U+0324
    { "", eString },  //                         U+0325
    { "", eString },  //                         U+0326
    { "", eString },  //                         U+0327
    { "", eString },  //                         U+0328
    { "", eString },  //                         U+0329
    { "", eString },  //                         U+032A
    { "", eString },  //                         U+032B
    { "", eString },  //                         U+032C
    { "", eString },  //                         U+032D
    { "", eString },  //                         U+032E
    { "", eString },  //                         U+032F
    { "", eString },  //                         U+0330
    { "", eString },  //                         U+0331
    { "", eString },  //                         U+0332
    { "", eString },  //                         U+0333
    { "", eString },  //                         U+0334
    { "", eString },  //                         U+0335
    { "", eString },  //                         U+0336
    { "", eString },  //                         U+0337
    { "", eString },  //                         U+0338
    { "", eString },  //                         U+0339
    { "", eString },  //                         U+033A
    { "", eString },  //                         U+033B
    { "", eString },  //                         U+033C
    { "", eString },  //                         U+033D
    { "", eString },  //                         U+033E
    { "", eString },  //                         U+033F
    { "", eString },  //                         U+0340
    { "", eString },  //                         U+0341
    { "", eString },  //                         U+0342
    { "", eString },  //                         U+0343
    { "", eString },  //                         U+0344
    { "", eString },  //                         U+0345
    { "", eString },  //                         U+0346
    { "", eString },  //                         U+0347
    { "", eString },  //                         U+0348
    { "", eString },  //                         U+0349
    { "", eString },  //                         U+034A
    { "", eString },  //                         U+034B
    { "", eString },  //                         U+034C
    { "", eString },  //                         U+034D
    { "", eString },  //                         U+034E
    { "", eString },  //                         U+034F
    { "", eString },  //                         U+0350
    { "", eString },  //                         U+0351
    { "", eString },  //                         U+0352
    { "", eString },  //                         U+0353
    { "", eString },  //                         U+0354
    { "", eString },  //                         U+0355
    { "", eString },  //                         U+0356
    { "", eString },  //                         U+0357
    { "", eString },  //                         U+0358
    { "", eString },  //                         U+0359
    { "", eString },  //                         U+035A
    { "", eString },  //                         U+035B
    { "", eString },  //                         U+035C
    { "", eString },  //                         U+035D
    { "", eString },  //                         U+035E
    { "", eString },  //                         U+035F
    { "", eString },  //                         U+0360
    { "", eString },  //                         U+0361
    { "", eString },  //                         U+0362
    { "", eString },  //                         U+0363
    { "", eString },  //                         U+0364
    { "", eString },  //                         U+0365
    { "", eString },  //                         U+0366
    { "", eString },  //                         U+0367
    { "", eString },  //                         U+0368
    { "", eString },  //                         U+0369
    { "", eString },  //                         U+036A
    { "", eString },  //                         U+036B
    { "", eString },  //                         U+036C
    { "", eString },  //                         U+036D
    { "", eString },  //                         U+036E
    { "", eString },  //                         U+036F
    { 0, eString },  //                       U+0370
    { 0, eString },  //                       U+0371
    { 0, eString },  //                       U+0372
    { 0, eString },  //                       U+0373
    { 0, eString },  //                       U+0374
    { 0, eString },  //                       U+0375
    { 0, eString },  //                       U+0376
    { 0, eString },  //                       U+0377
    { 0, eString },  //                       U+0378
    { 0, eString },  //                       U+0379
    { 0, eString },  //                       U+037A
    { 0, eString },  //                       U+037B
    { 0, eString },  //                       U+037C
    { 0, eString },  //                       U+037D
    { 0, eString },  //                       U+037E
    { 0, eString },  //                       U+037F
    { 0, eString },  //                       U+0380
    { 0, eString },  //                       U+0381
    { 0, eString },  //                       U+0382
    { 0, eString },  //                       U+0383
    { 0, eString },  //                       U+0384
    { 0, eString },  //                       U+0385
    { "Alpha", eString },  // old dictionary     U+0386
    { 0, eString },  //                       U+0387
    { "Epsilon", eString },  // old dictionary   U+0388
    { "Eta", eString },  // old dictionary       U+0389
    { "Iota", eString },  // old dictionary      U+038A
    { 0, eString },  //                       U+038B
    { "Omicron", eString },  // old dictionary   U+038C
    { 0, eString },  //                       U+038D
    { "Upsilon", eString },  // old dictionary   U+038E
    { "Omega", eString },  // old dictionary     U+038F
    { "iota", eString },  // old dictionary      U+0390
    { "Alpha", eString },  // old dictionary     U+0391
    { "Beta", eString },  // old dictionary      U+0392
    { "Gamma", eString },  // old dictionary     U+0393
    { "Delta", eString },  // old dictionary     U+0394
    { "Epsilon", eString },  // old dictionary   U+0395
    { "Zeta", eString },  // old dictionary      U+0396
    { "Eta", eString },  // old dictionary       U+0397
    { "Theta", eString },  // old dictionary     U+0398
    { "Iota", eString },  // old dictionary      U+0399
    { "Kappa", eString },  // old dictionary     U+039A
    { "Lambda", eString },  // old dictionary    U+039B
    { "Mu", eString },  //                       U+039C
    { "Nu", eString },  // old dictionary        U+039D
    { "Xi", eString },  // old dictionary        U+039E
    { "Omicron", eString },  // old dictionary   U+039F
    { "Pi", eString },  // old dictionary        U+03A0
    { "Rho", eString },  // old dictionary       U+03A1
    { 0, eString },  //                       U+03A2
    { "Sigma", eString },  // old dictionary     U+03A3
    { "Tau", eString },  // old dictionary       U+03A4
    { "Upsilon", eString },  // old dictionary   U+03A5
    { "Phi", eString },  // old dictionary       U+03A6
    { "Chi", eString },  // old dictionary       U+03A7
    { "Psi", eString },  // old dictionary       U+03A8
    { "Omega", eString },  // old dictionary     U+03A9
    { "Iota", eString },  // old dictionary      U+03AA
    { "Upsilon", eString },  // old dictionary   U+03AB
    { "alpha", eString },  // old dictionary     U+03AC
    { "epsilon", eString },  // old dictionary   U+03AD
    { "eta", eString },  // old dictionary       U+03AE
    { "iota", eString },  // old dictionary      U+03AF
    { "upsilon", eString },  // old dictionary   U+03B0
    { "alpha", eString },  // old dictionary     U+03B1
    { "beta", eString },  // old dictionary      U+03B2
    { "gamma", eString },  // old dictionary     U+03B3
    { "delta", eString },  // old dictionary     U+03B4
    { "epsilon", eString },  // old dictionary   U+03B5
    { "zeta", eString },  // old dictionary      U+03B6
    { "eta", eString },  // old dictionary       U+03B7
    { "theta;", eString },  // old dictionary    U+03B8
    { "iota", eString },  // old dictionary      U+03B9
    { "kappa", eString },  // old dictionary     U+03BA
    { "lambda", eString },  // old dictionary    U+03BB
    { "mu", eString },  //                       U+03BC
    { "nu", eString },  // old dictionary        U+03BD
    { "xi", eString },  // old dictionary        U+03BE
    { "omicron", eString },  // old dictionary   U+03BF
    { "pi", eString },  // old dictionary        U+03C0
    { "rho", eString },  // old dictionary       U+03C1
    { "varsigma", eString },  // old dictionary  U+03C2
    { "sigma", eString },  // old dictionary     U+03C3
    { "tau", eString },  // old dictionary       U+03C4
    { "upsilon", eString },  // old dictionary   U+03C5
    { "phi", eString },  //                      U+03C6
    { "chi", eString },  // old dictionary       U+03C7
    { "psi", eString },  // old dictionary       U+03C8
    { "omega", eString },  // old dictionary     U+03C9
    { "iota", eString },  // old dictionary      U+03CA
    { "upsilon", eString },  // old dictionary   U+03CB
    { "omicron", eString },  // old dictionary   U+03CC
    { "upsilon", eString },  // old dictionary   U+03CD
    { "omega", eString },  // old dictionary     U+03CE
    { 0, eString },  //                       U+03CF
    { 0, eString },  //                       U+03D0
    { "vartheta", eString },  // old dictionary  U+03D1
    { "Upsilon", eString },  // old dictionary   U+03D2
    { 0, eString },  //                       U+03D3
    { 0, eString },  //                       U+03D4
    { "varphi", eString },  // old dictionary    U+03D5
    { "varpi", eString },  // old dictionary     U+03D6
    { 0, eString },  //                       U+03D7
    { 0, eString },  //                       U+03D8
    { 0, eString },  //                       U+03D9
    { 0, eString },  //                       U+03DA
    { 0, eString },  //                       U+03DB
    { "digamma", eString },  //                  U+03DC
    { "digamma", eString },  // old dictionary   U+03DD
    { 0, eString },  //                       U+03DE
    { 0, eString },  //                       U+03DF
    { 0, eString },  //                       U+03E0
    { 0, eString },  //                       U+03E1
    { 0, eString },  //                       U+03E2
    { 0, eString },  //                       U+03E3
    { 0, eString },  //                       U+03E4
    { 0, eString },  //                       U+03E5
    { 0, eString },  //                       U+03E6
    { 0, eString },  //                       U+03E7
    { 0, eString },  //                       U+03E8
    { 0, eString },  //                       U+03E9
    { 0, eString },  //                       U+03EA
    { 0, eString },  //                       U+03EB
    { 0, eString },  //                       U+03EC
    { 0, eString },  //                       U+03ED
    { 0, eString },  //                       U+03EE
    { 0, eString },  //                       U+03EF
    { "varkappa", eString },  // old dictionary  U+03F0
    { "varrho", eString },  // old dictionary    U+03F1
    { 0, eString },  //                       U+03F2
    { 0, eString },  //                       U+03F3
    { 0, eString },  //                       U+03F4
    { 0, eString },  //                       U+03F5
    { 0, eString },  //                       U+03F6
    { 0, eString },  //                       U+03F7
    { 0, eString },  //                       U+03F8
    { 0, eString },  //                       U+03F9
    { 0, eString },  //                       U+03FA
    { 0, eString },  //                       U+03FB
    { 0, eString },  //                       U+03FC
    { 0, eString },  //                       U+03FD
    { 0, eString },  //                       U+03FE
    { 0, eString },  //                       U+03FF
};
static TUnicodePlan s_Plan_04h = {
    { 0, eString },  //                                          U+0400
    { "capital IO, Russian", eString },  // old dictionary          U+0401
    { "capital DJE, Serbian", eString },  // old dictionary         U+0402
    { "capital GJE Macedonian", eString },  // old dictionary       U+0403
    { "capital JE, Ukrainian", eString },  // old dictionary        U+0404
    { "capital DSE, Macedonian", eString },  // old dictionary      U+0405
    { "capital I, Ukrainian", eString },  // old dictionary         U+0406
    { "capital YI, Ukrainian", eString },  // old dictionary        U+0407
    { "capital JE, Serbian", eString },  // old dictionary          U+0408
    { "capital LJE, Serbian", eString },  // old dictionary         U+0409
    { "capital NJE, Serbian", eString },  // old dictionary         U+040A
    { "capital TSHE, Serbian", eString },  // old dictionary        U+040B
    { "capital KJE, Macedonian", eString },  // old dictionary      U+040C
    { 0, eString },  //                                          U+040D
    { "capital U, Byelorussian", eString },  // old dictionary      U+040E
    { "capital dze, Serbian", eString },  // old dictionary         U+040F
    { "capital A, Cyrillic", eString },  // old dictionary          U+0410
    { "capital BE, Cyrillic", eString },  // old dictionary         U+0411
    { "capital VE, Cyrillic", eString },  // old dictionary         U+0412
    { "capital GHE, Cyrillic", eString },  // old dictionary        U+0413
    { "capital DE, Cyrillic", eString },  // old dictionary         U+0414
    { "capital IE, Cyrillic", eString },  // old dictionary         U+0415
    { "capital ZHE, Cyrillic", eString },  // old dictionary        U+0416
    { "capital ZE, Cyrillic", eString },  // old dictionary         U+0417
    { "capital I, Cyrillic", eString },  // old dictionary          U+0418
    { "capital short I, Cyrillic", eString },  // old dictionary    U+0419
    { "capital KA, Cyrillic", eString },  // old dictionary         U+041A
    { "capital EL, Cyrillic", eString },  // old dictionary         U+041B
    { "capital EM, Cyrillic", eString },  // old dictionary         U+041C
    { "capital EN, Cyrillic", eString },  // old dictionary         U+041D
    { "capital O, Cyrillic", eString },  // old dictionary          U+041E
    { "capital PE, Cyrillic", eString },  // old dictionary         U+041F
    { "capital ER, Cyrillic", eString },  // old dictionary         U+0420
    { "capital ES, Cyrillic", eString },  // old dictionary         U+0421
    { "capital TE, Cyrillic", eString },  // old dictionary         U+0422
    { "capital U, Cyrillic", eString },  // old dictionary          U+0423
    { "capital EF, Cyrillic", eString },  // old dictionary         U+0424
    { "capital HA, Cyrillic", eString },  // old dictionary         U+0425
    { "capital TSE, Cyrillic", eString },  // old dictionary        U+0426
    { "capital CHE, Cyrillic", eString },  // old dictionary        U+0427
    { "capital SHA, Cyrillic", eString },  // old dictionary        U+0428
    { "capital SHCHA, Cyrillic", eString },  // old dictionary      U+0429
    { "capital HARD sign, Cyrillic", eString },  // old dictionary  U+042A
    { "capital YERU, Cyrillic", eString },  // old dictionary       U+042B
    { "capital SOFT sign, Cyrillic", eString },  // old dictionary  U+042C
    { "capital E, Cyrillic", eString },  // old dictionary          U+042D
    { "capital YU, Cyrillic", eString },  // old dictionary         U+042E
    { "capital YA, Cyrillic", eString },  // old dictionary         U+042F
    { "small a, Cyrillic", eString },  // old dictionary            U+0430
    { "small be, Cyrillic", eString },  // old dictionary           U+0431
    { "small ve, Cyrillic", eString },  // old dictionary           U+0432
    { "small ghe, Cyrillic", eString },  // old dictionary          U+0433
    { "small de, Cyrillic", eString },  // old dictionary           U+0434
    { "small ie, Cyrillic", eString },  // old dictionary           U+0435
    { "small zhe, Cyrillic", eString },  // old dictionary          U+0436
    { "small ze, Cyrillic", eString },  // old dictionary           U+0437
    { "small i, Cyrillic", eString },  // old dictionary            U+0438
    { "small short i, Cyrillic", eString },  // old dictionary      U+0439
    { "small ka, Cyrillic", eString },  // old dictionary           U+043A
    { "small el, Cyrillic", eString },  // old dictionary           U+043B
    { "small em, Cyrillic", eString },  // old dictionary           U+043C
    { "small en, Cyrillic", eString },  // old dictionary           U+043D
    { "small o, Cyrillic", eString },  // old dictionary            U+043E
    { "small pe, Cyrillic", eString },  // old dictionary           U+043F
    { "small er, Cyrillic", eString },  // old dictionary           U+0440
    { "small es, Cyrillic", eString },  // old dictionary           U+0441
    { "small te, Cyrillic", eString },  // old dictionary           U+0442
    { "small u, Cyrillic", eString },  // old dictionary            U+0443
    { "small ef, Cyrillic", eString },  // old dictionary           U+0444
    { "small ha, Cyrillic", eString },  // old dictionary           U+0445
    { "small tse, Cyrillic", eString },  // old dictionary          U+0446
    { "small che, Cyrillic", eString },  // old dictionary          U+0447
    { "small sha, Cyrillic", eString },  // old dictionary          U+0448
    { "small shcha, Cyrillic", eString },  // old dictionary        U+0449
    { "small hard sign, Cyrillic", eString },  // old dictionary    U+044A
    { "small yeru, Cyrillic", eString },  // old dictionary         U+044B
    { "small soft sign, Cyrillic", eString },  // old dictionary    U+044C
    { "small e, Cyrillic", eString },  // old dictionary            U+044D
    { "small yu, Cyrillic", eString },  // old dictionary           U+044E
    { "small ya, Cyrillic", eString },  // old dictionary           U+044F
    { 0, eString },  //                                          U+0450
    { "small io, Russian", eString },  // old dictionary            U+0451
    { "small dje, Serbian", eString },  // old dictionary           U+0452
    { "small gje, Macedonian", eString },  // old dictionary        U+0453
    { "small je, Ukrainian", eString },  // old dictionary          U+0454
    { "small dse, Macedonian", eString },  // old dictionary        U+0455
    { "small i, Ukrainian", eString },  // old dictionary           U+0456
    { "small yi, Ukrainian", eString },  // old dictionary          U+0457
    { "small je, Serbian", eString },  // old dictionary            U+0458
    { "small lje, Serbian", eString },  // old dictionary           U+0459
    { "small nje, Serbian", eString },  // old dictionary           U+045A
    { "small tshe, Serbian", eString },  // old dictionary          U+045B
    { "small kje Macedonian", eString },  // old dictionary         U+045C
    { 0, eString },  //                                          U+045D
    { "small u, Byelorussian", eString },  // old dictionary        U+045E
    { "small dze, Serbian", eString },  // old dictionary           U+045F
    { 0, eString },  //                                          U+0460
    { 0, eString },  //                                          U+0461
    { 0, eString },  //                                          U+0462
    { 0, eString },  //                                          U+0463
    { 0, eString },  //                                          U+0464
    { 0, eString },  //                                          U+0465
    { 0, eString },  //                                          U+0466
    { 0, eString },  //                                          U+0467
    { 0, eString },  //                                          U+0468
    { 0, eString },  //                                          U+0469
    { 0, eString },  //                                          U+046A
    { 0, eString },  //                                          U+046B
    { 0, eString },  //                                          U+046C
    { 0, eString },  //                                          U+046D
    { 0, eString },  //                                          U+046E
    { 0, eString },  //                                          U+046F
    { 0, eString },  //                                          U+0470
    { 0, eString },  //                                          U+0471
    { 0, eString },  //                                          U+0472
    { 0, eString },  //                                          U+0473
    { 0, eString },  //                                          U+0474
    { 0, eString },  //                                          U+0475
    { 0, eString },  //                                          U+0476
    { 0, eString },  //                                          U+0477
    { 0, eString },  //                                          U+0478
    { 0, eString },  //                                          U+0479
    { 0, eString },  //                                          U+047A
    { 0, eString },  //                                          U+047B
    { 0, eString },  //                                          U+047C
    { 0, eString },  //                                          U+047D
    { 0, eString },  //                                          U+047E
    { 0, eString },  //                                          U+047F
    { 0, eString },  //                                          U+0480
    { 0, eString },  //                                          U+0481
    { 0, eString },  //                                          U+0482
    { 0, eString },  //                                          U+0483
    { 0, eString },  //                                          U+0484
    { 0, eString },  //                                          U+0485
    { 0, eString },  //                                          U+0486
    { 0, eString },  //                                          U+0487
    { 0, eString },  //                                          U+0488
    { 0, eString },  //                                          U+0489
    { 0, eString },  //                                          U+048A
    { 0, eString },  //                                          U+048B
    { 0, eString },  //                                          U+048C
    { 0, eString },  //                                          U+048D
    { 0, eString },  //                                          U+048E
    { 0, eString },  //                                          U+048F
    { 0, eString },  //                                          U+0490
    { 0, eString },  //                                          U+0491
    { 0, eString },  //                                          U+0492
    { 0, eString },  //                                          U+0493
    { 0, eString },  //                                          U+0494
    { 0, eString },  //                                          U+0495
    { 0, eString },  //                                          U+0496
    { 0, eString },  //                                          U+0497
    { 0, eString },  //                                          U+0498
    { 0, eString },  //                                          U+0499
    { 0, eString },  //                                          U+049A
    { 0, eString },  //                                          U+049B
    { 0, eString },  //                                          U+049C
    { 0, eString },  //                                          U+049D
    { 0, eString },  //                                          U+049E
    { 0, eString },  //                                          U+049F
    { 0, eString },  //                                          U+04A0
    { 0, eString },  //                                          U+04A1
    { 0, eString },  //                                          U+04A2
    { 0, eString },  //                                          U+04A3
    { 0, eString },  //                                          U+04A4
    { 0, eString },  //                                          U+04A5
    { 0, eString },  //                                          U+04A6
    { 0, eString },  //                                          U+04A7
    { 0, eString },  //                                          U+04A8
    { 0, eString },  //                                          U+04A9
    { 0, eString },  //                                          U+04AA
    { 0, eString },  //                                          U+04AB
    { 0, eString },  //                                          U+04AC
    { 0, eString },  //                                          U+04AD
    { 0, eString },  //                                          U+04AE
    { 0, eString },  //                                          U+04AF
    { 0, eString },  //                                          U+04B0
    { 0, eString },  //                                          U+04B1
    { 0, eString },  //                                          U+04B2
    { 0, eString },  //                                          U+04B3
    { 0, eString },  //                                          U+04B4
    { 0, eString },  //                                          U+04B5
    { 0, eString },  //                                          U+04B6
    { 0, eString },  //                                          U+04B7
    { 0, eString },  //                                          U+04B8
    { 0, eString },  //                                          U+04B9
    { 0, eString },  //                                          U+04BA
    { 0, eString },  //                                          U+04BB
    { 0, eString },  //                                          U+04BC
    { 0, eString },  //                                          U+04BD
    { 0, eString },  //                                          U+04BE
    { 0, eString },  //                                          U+04BF
    { 0, eString },  //                                          U+04C0
    { 0, eString },  //                                          U+04C1
    { 0, eString },  //                                          U+04C2
    { 0, eString },  //                                          U+04C3
    { 0, eString },  //                                          U+04C4
    { 0, eString },  //                                          U+04C5
    { 0, eString },  //                                          U+04C6
    { 0, eString },  //                                          U+04C7
    { 0, eString },  //                                          U+04C8
    { 0, eString },  //                                          U+04C9
    { 0, eString },  //                                          U+04CA
    { 0, eString },  //                                          U+04CB
    { 0, eString },  //                                          U+04CC
    { 0, eString },  //                                          U+04CD
    { 0, eString },  //                                          U+04CE
    { 0, eString },  //                                          U+04CF
    { 0, eString },  //                                          U+04D0
    { 0, eString },  //                                          U+04D1
    { 0, eString },  //                                          U+04D2
    { 0, eString },  //                                          U+04D3
    { 0, eString },  //                                          U+04D4
    { 0, eString },  //                                          U+04D5
    { 0, eString },  //                                          U+04D6
    { 0, eString },  //                                          U+04D7
    { 0, eString },  //                                          U+04D8
    { 0, eString },  //                                          U+04D9
    { 0, eString },  //                                          U+04DA
    { 0, eString },  //                                          U+04DB
    { 0, eString },  //                                          U+04DC
    { 0, eString },  //                                          U+04DD
    { 0, eString },  //                                          U+04DE
    { 0, eString },  //                                          U+04DF
    { 0, eString },  //                                          U+04E0
    { 0, eString },  //                                          U+04E1
    { 0, eString },  //                                          U+04E2
    { 0, eString },  //                                          U+04E3
    { 0, eString },  //                                          U+04E4
    { 0, eString },  //                                          U+04E5
    { 0, eString },  //                                          U+04E6
    { 0, eString },  //                                          U+04E7
    { 0, eString },  //                                          U+04E8
    { 0, eString },  //                                          U+04E9
    { 0, eString },  //                                          U+04EA
    { 0, eString },  //                                          U+04EB
    { 0, eString },  //                                          U+04EC
    { 0, eString },  //                                          U+04ED
    { 0, eString },  //                                          U+04EE
    { 0, eString },  //                                          U+04EF
    { 0, eString },  //                                          U+04F0
    { 0, eString },  //                                          U+04F1
    { 0, eString },  //                                          U+04F2
    { 0, eString },  //                                          U+04F3
    { 0, eString },  //                                          U+04F4
    { 0, eString },  //                                          U+04F5
    { 0, eString },  //                                          U+04F6
    { 0, eString },  //                                          U+04F7
    { 0, eString },  //                                          U+04F8
    { 0, eString },  //                                          U+04F9
    { 0, eString },  //                                          U+04FA
    { 0, eString },  //                                          U+04FB
    { 0, eString },  //                                          U+04FC
    { 0, eString },  //                                          U+04FD
    { 0, eString },  //                                          U+04FE
    { 0, eString },  //                                          U+04FF
};
static TUnicodePlan s_Plan_1Eh = {
    { "A", eString },  //  U+1E00
    { "a", eString },  //  U+1E01
    { "B", eString },  //  U+1E02
    { "b", eString },  //  U+1E03
    { "B", eString },  //  U+1E04
    { "b", eString },  //  U+1E05
    { "B", eString },  //  U+1E06
    { "b", eString },  //  U+1E07
    { "C", eString },  //  U+1E08
    { "c", eString },  //  U+1E09
    { "D", eString },  //  U+1E0A
    { "d", eString },  //  U+1E0B
    { "D", eString },  //  U+1E0C
    { "d", eString },  //  U+1E0D
    { "D", eString },  //  U+1E0E
    { "d", eString },  //  U+1E0F
    { "D", eString },  //  U+1E10
    { "d", eString },  //  U+1E11
    { "D", eString },  //  U+1E12
    { "d", eString },  //  U+1E13
    { "E", eString },  //  U+1E14
    { "e", eString },  //  U+1E15
    { "E", eString },  //  U+1E16
    { "e", eString },  //  U+1E17
    { "E", eString },  //  U+1E18
    { "e", eString },  //  U+1E19
    { "E", eString },  //  U+1E1A
    { "e", eString },  //  U+1E1B
    { "E", eString },  //  U+1E1C
    { "e", eString },  //  U+1E1D
    { "F", eString },  //  U+1E1E
    { "f", eString },  //  U+1E1F
    { "G", eString },  //  U+1E20
    { "g", eString },  //  U+1E21
    { "H", eString },  //  U+1E22
    { "h", eString },  //  U+1E23
    { "H", eString },  //  U+1E24
    { "h", eString },  //  U+1E25
    { "H", eString },  //  U+1E26
    { "h", eString },  //  U+1E27
    { "H", eString },  //  U+1E28
    { "h", eString },  //  U+1E29
    { "H", eString },  //  U+1E2A
    { "h", eString },  //  U+1E2B
    { "I", eString },  //  U+1E2C
    { "i", eString },  //  U+1E2D
    { "I", eString },  //  U+1E2E
    { "i", eString },  //  U+1E2F
    { "K", eString },  //  U+1E30
    { "k", eString },  //  U+1E31
    { "K", eString },  //  U+1E32
    { "k", eString },  //  U+1E33
    { "K", eString },  //  U+1E34
    { "k", eString },  //  U+1E35
    { "L", eString },  //  U+1E36
    { "l", eString },  //  U+1E37
    { "L", eString },  //  U+1E38
    { "l", eString },  //  U+1E39
    { "L", eString },  //  U+1E3A
    { "l", eString },  //  U+1E3B
    { "L", eString },  //  U+1E3C
    { "l", eString },  //  U+1E3D
    { "M", eString },  //  U+1E3E
    { "m", eString },  //  U+1E3F
    { "M", eString },  //  U+1E40
    { "m", eString },  //  U+1E41
    { "M", eString },  //  U+1E42
    { "m", eString },  //  U+1E43
    { "N", eString },  //  U+1E44
    { "n", eString },  //  U+1E45
    { "N", eString },  //  U+1E46
    { "n", eString },  //  U+1E47
    { "N", eString },  //  U+1E48
    { "n", eString },  //  U+1E49
    { "N", eString },  //  U+1E4A
    { "n", eString },  //  U+1E4B
    { "O", eString },  //  U+1E4C
    { "o", eString },  //  U+1E4D
    { "O", eString },  //  U+1E4E
    { "o", eString },  //  U+1E4F
    { "O", eString },  //  U+1E50
    { "o", eString },  //  U+1E51
    { "O", eString },  //  U+1E52
    { "o", eString },  //  U+1E53
    { "P", eString },  //  U+1E54
    { "p", eString },  //  U+1E55
    { "P", eString },  //  U+1E56
    { "p", eString },  //  U+1E57
    { "R", eString },  //  U+1E58
    { "r", eString },  //  U+1E59
    { "R", eString },  //  U+1E5A
    { "r", eString },  //  U+1E5B
    { "R", eString },  //  U+1E5C
    { "r", eString },  //  U+1E5D
    { "R", eString },  //  U+1E5E
    { "r", eString },  //  U+1E5F
    { "S", eString },  //  U+1E60
    { "s", eString },  //  U+1E61
    { "S", eString },  //  U+1E62
    { "s", eString },  //  U+1E63
    { "S", eString },  //  U+1E64
    { "s", eString },  //  U+1E65
    { "S", eString },  //  U+1E66
    { "s", eString },  //  U+1E67
    { "S", eString },  //  U+1E68
    { "s", eString },  //  U+1E69
    { "T", eString },  //  U+1E6A
    { "t", eString },  //  U+1E6B
    { "T", eString },  //  U+1E6C
    { "t", eString },  //  U+1E6D
    { "T", eString },  //  U+1E6E
    { "t", eString },  //  U+1E6F
    { "T", eString },  //  U+1E70
    { "t", eString },  //  U+1E71
    { "U", eString },  //  U+1E72
    { "u", eString },  //  U+1E73
    { "U", eString },  //  U+1E74
    { "u", eString },  //  U+1E75
    { "U", eString },  //  U+1E76
    { "u", eString },  //  U+1E77
    { "U", eString },  //  U+1E78
    { "u", eString },  //  U+1E79
    { "U", eString },  //  U+1E7A
    { "u", eString },  //  U+1E7B
    { "V", eString },  //  U+1E7C
    { "v", eString },  //  U+1E7D
    { "V", eString },  //  U+1E7E
    { "v", eString },  //  U+1E7F
    { "W", eString },  //  U+1E80
    { "w", eString },  //  U+1E81
    { "W", eString },  //  U+1E82
    { "w", eString },  //  U+1E83
    { "W", eString },  //  U+1E84
    { "w", eString },  //  U+1E85
    { "W", eString },  //  U+1E86
    { "w", eString },  //  U+1E87
    { "W", eString },  //  U+1E88
    { "w", eString },  //  U+1E89
    { "X", eString },  //  U+1E8A
    { "x", eString },  //  U+1E8B
    { "X", eString },  //  U+1E8C
    { "x", eString },  //  U+1E8D
    { "Y", eString },  //  U+1E8E
    { "y", eString },  //  U+1E8F
    { "Z", eString },  //  U+1E90
    { "z", eString },  //  U+1E91
    { "Z", eString },  //  U+1E92
    { "z", eString },  //  U+1E93
    { "Z", eString },  //  U+1E94
    { "z", eString },  //  U+1E95
    { "h", eString },  //  U+1E96
    { "t", eString },  //  U+1E97
    { "w", eString },  //  U+1E98
    { "y", eString },  //  U+1E99
    { "a", eString },  //  U+1E9A
    { "f", eString },  //  U+1E9B
    { 0, eString },  // U+1E9C
    { 0, eString },  // U+1E9D
    { 0, eString },  // U+1E9E
    { 0, eString },  // U+1E9F
    { "A", eString },  //  U+1EA0
    { "a", eString },  //  U+1EA1
    { "A", eString },  //  U+1EA2
    { "a", eString },  //  U+1EA3
    { "A", eString },  //  U+1EA4
    { "a", eString },  //  U+1EA5
    { "A", eString },  //  U+1EA6
    { "a", eString },  //  U+1EA7
    { "A", eString },  //  U+1EA8
    { "a", eString },  //  U+1EA9
    { "A", eString },  //  U+1EAA
    { "a", eString },  //  U+1EAB
    { "A", eString },  //  U+1EAC
    { "a", eString },  //  U+1EAD
    { "A", eString },  //  U+1EAE
    { "a", eString },  //  U+1EAF
    { "A", eString },  //  U+1EB0
    { "a", eString },  //  U+1EB1
    { "A", eString },  //  U+1EB2
    { "a", eString },  //  U+1EB3
    { "A", eString },  //  U+1EB4
    { "a", eString },  //  U+1EB5
    { "A", eString },  //  U+1EB6
    { "a", eString },  //  U+1EB7
    { "E", eString },  //  U+1EB8
    { "e", eString },  //  U+1EB9
    { "E", eString },  //  U+1EBA
    { "e", eString },  //  U+1EBB
    { "E", eString },  //  U+1EBC
    { "e", eString },  //  U+1EBD
    { "E", eString },  //  U+1EBE
    { "e", eString },  //  U+1EBF
    { "E", eString },  //  U+1EC0
    { "e", eString },  //  U+1EC1
    { "E", eString },  //  U+1EC2
    { "e", eString },  //  U+1EC3
    { "E", eString },  //  U+1EC4
    { "e", eString },  //  U+1EC5
    { "E", eString },  //  U+1EC6
    { "e", eString },  //  U+1EC7
    { "I", eString },  //  U+1EC8
    { "i", eString },  //  U+1EC9
    { "I", eString },  //  U+1ECA
    { "i", eString },  //  U+1ECB
    { "O", eString },  //  U+1ECC
    { "o", eString },  //  U+1ECD
    { "O", eString },  //  U+1ECE
    { "o", eString },  //  U+1ECF
    { "O", eString },  //  U+1ED0
    { "o", eString },  //  U+1ED1
    { "O", eString },  //  U+1ED2
    { "o", eString },  //  U+1ED3
    { "O", eString },  //  U+1ED4
    { "o", eString },  //  U+1ED5
    { "O", eString },  //  U+1ED6
    { "o", eString },  //  U+1ED7
    { "O", eString },  //  U+1ED8
    { "o", eString },  //  U+1ED9
    { "O", eString },  //  U+1EDA
    { "o", eString },  //  U+1EDB
    { "O", eString },  //  U+1EDC
    { "o", eString },  //  U+1EDD
    { "O", eString },  //  U+1EDE
    { "o", eString },  //  U+1EDF
    { "O", eString },  //  U+1EE0
    { "o", eString },  //  U+1EE1
    { "O", eString },  //  U+1EE2
    { "o", eString },  //  U+1EE3
    { "U", eString },  //  U+1EE4
    { "u", eString },  //  U+1EE5
    { "U", eString },  //  U+1EE6
    { "u", eString },  //  U+1EE7
    { "U", eString },  //  U+1EE8
    { "u", eString },  //  U+1EE9
    { "U", eString },  //  U+1EEA
    { "u", eString },  //  U+1EEB
    { "U", eString },  //  U+1EEC
    { "u", eString },  //  U+1EED
    { "U", eString },  //  U+1EEE
    { "u", eString },  //  U+1EEF
    { "U", eString },  //  U+1EF0
    { "u", eString },  //  U+1EF1
    { "Y", eString },  //  U+1EF2
    { "y", eString },  //  U+1EF3
    { "Y", eString },  //  U+1EF4
    { "y", eString },  //  U+1EF5
    { "Y", eString },  //  U+1EF6
    { "y", eString },  //  U+1EF7
    { "Y", eString },  //  U+1EF8
    { "y", eString },  //  U+1EF9
    { 0, eString },  // U+1EFA
    { 0, eString },  // U+1EFB
    { 0, eString },  // U+1EFC
    { 0, eString },  // U+1EFD
    { 0, eString },  // U+1EFE
    { 0, eString },  // U+1EFF
};
static TUnicodePlan s_Plan_20h = {
    { 0, eString },  //                             U+2000
    { 0, eString },  //                             U+2001
    { " ", eString },  // old dictionary               U+2002
    { " ", eString },  // old dictionary               U+2003
    { " ", eString },  // old dictionary               U+2004
    { " ", eString },  // old dictionary               U+2005
    { 0, eString },  //                             U+2006
    { " ", eString },  // old dictionary               U+2007
    { " ", eString },  // old dictionary               U+2008
    { " ", eString },  // old dictionary               U+2009
    { " ", eString },  // old dictionary               U+200A
    { 0, eString },  //                             U+200B
    { 0, eString },  //                             U+200C
    { 0, eString },  //                             U+200D
    { 0, eString },  //                             U+200E
    { 0, eString },  //                             U+200F
    { "-", eString },  // old dictionary               U+2010
    { 0, eString },  //                             U+2011
    { 0, eString },  //                             U+2012
    { "-", eString },  // old dictionary               U+2013
    { "-", eString },  // old dictionary               U+2014
    { "-", eString },  // old dictionary               U+2015
    { " ||", eString },  // old dictionary             U+2016
    { 0, eString },  //                             U+2017
    { "'", eString },  // old dictionary               U+2018
    { "'", eString },  // old dictionary               U+2019
    { "'", eString },  // old dictionary               U+201A
    { 0, eString },  //                             U+201B
    { "\"", eString },  // old dictionary              U+201C
    { "\"", eString },  // old dictionary              U+201D
    { "\"", eString },  // old dictionary              U+201E
    { 0, eString },  //                             U+201F
    { "dagger", eString },  // old dictionary          U+2020
    { "double dagger", eString },  // old dictionary   U+2021
    { "*", eString },  // old dictionary               U+2022
    { 0, eString },  //                             U+2023
    { 0, eString },  //                             U+2024
    { "..", eString },  // old dictionary              U+2025
    { " em leader", eString },  // old dictionary      U+2026
    { 0, eString },  //                             U+2027
    { 0, eString },  //                             U+2028
    { 0, eString },  //                             U+2029
    { 0, eString },  //                             U+202A
    { 0, eString },  //                             U+202B
    { 0, eString },  //                             U+202C
    { 0, eString },  //                             U+202D
    { 0, eString },  //                             U+202E
    { 0, eString },  //                             U+202F
    { " per thousand", eString },  // old dictionary   U+2030
    { "per 10 thousand", eString },  //                U+2031
    { "'", eString },  // old dictionary               U+2032
    { "''", eString },  // old dictionary              U+2033
    { "'''", eString },  // old dictionary             U+2034
    { "'", eString },  // old dictionary               U+2035
    { 0, eString },  //                             U+2036
    { 0, eString },  //                             U+2037
    { 0, eString },  //                             U+2038
    { 0, eString },  //                             U+2039
    { 0, eString },  //                             U+203A
    { 0, eString },  //                             U+203B
    { 0, eString },  //                             U+203C
    { 0, eString },  //                             U+203D
    { 0, eString },  //                             U+203E
    { 0, eString },  //                             U+203F
    { 0, eString },  //                             U+2040
    { "insertion mark", eString },  // old dictionary  U+2041
    { 0, eString },  //                             U+2042
    { " rectangle", eString },  // old dictionary      U+2043
    { 0, eString },  //                             U+2044
    { 0, eString },  //                             U+2045
    { 0, eString },  //                             U+2046
    { 0, eString },  //                             U+2047
    { 0, eString },  //                             U+2048
    { 0, eString },  //                             U+2049
    { 0, eString },  //                             U+204A
    { 0, eString },  //                             U+204B
    { 0, eString },  //                             U+204C
    { 0, eString },  //                             U+204D
    { 0, eString },  //                             U+204E
    { 0, eString },  //                             U+204F
    { 0, eString },  //                             U+2050
    { 0, eString },  //                             U+2051
    { 0, eString },  //                             U+2052
    { 0, eString },  //                             U+2053
    { 0, eString },  //                             U+2054
    { 0, eString },  //                             U+2055
    { 0, eString },  //                             U+2056
    { 0, eString },  //                             U+2057
    { 0, eString },  //                             U+2058
    { 0, eString },  //                             U+2059
    { 0, eString },  //                             U+205A
    { 0, eString },  //                             U+205B
    { 0, eString },  //                             U+205C
    { 0, eString },  //                             U+205D
    { 0, eString },  //                             U+205E
    { 0, eString },  //                             U+205F
    { 0, eString },  //                             U+2060
    { 0, eString },  //                             U+2061
    { 0, eString },  //                             U+2062
    { 0, eString },  //                             U+2063
    { 0, eString },  //                             U+2064
    { 0, eString },  //                             U+2065
    { 0, eString },  //                             U+2066
    { 0, eString },  //                             U+2067
    { 0, eString },  //                             U+2068
    { 0, eString },  //                             U+2069
    { 0, eString },  //                             U+206A
    { 0, eString },  //                             U+206B
    { 0, eString },  //                             U+206C
    { 0, eString },  //                             U+206D
    { 0, eString },  //                             U+206E
    { 0, eString },  //                             U+206F
    { 0, eString },  //                             U+2070
    { 0, eString },  //                             U+2071
    { 0, eString },  //                             U+2072
    { 0, eString },  //                             U+2073
    { 0, eString },  //                             U+2074
    { 0, eString },  //                             U+2075
    { 0, eString },  //                             U+2076
    { 0, eString },  //                             U+2077
    { 0, eString },  //                             U+2078
    { 0, eString },  //                             U+2079
    { 0, eString },  //                             U+207A
    { 0, eString },  //                             U+207B
    { 0, eString },  //                             U+207C
    { 0, eString },  //                             U+207D
    { 0, eString },  //                             U+207E
    { 0, eString },  //                             U+207F
    { 0, eString },  //                             U+2080
    { 0, eString },  //                             U+2081
    { 0, eString },  //                             U+2082
    { 0, eString },  //                             U+2083
    { 0, eString },  //                             U+2084
    { 0, eString },  //                             U+2085
    { 0, eString },  //                             U+2086
    { 0, eString },  //                             U+2087
    { 0, eString },  //                             U+2088
    { 0, eString },  //                             U+2089
    { 0, eString },  //                             U+208A
    { 0, eString },  //                             U+208B
    { 0, eString },  //                             U+208C
    { 0, eString },  //                             U+208D
    { 0, eString },  //                             U+208E
    { 0, eString },  //                             U+208F
    { 0, eString },  //                             U+2090
    { 0, eString },  //                             U+2091
    { 0, eString },  //                             U+2092
    { 0, eString },  //                             U+2093
    { 0, eString },  //                             U+2094
    { 0, eString },  //                             U+2095
    { 0, eString },  //                             U+2096
    { 0, eString },  //                             U+2097
    { 0, eString },  //                             U+2098
    { 0, eString },  //                             U+2099
    { 0, eString },  //                             U+209A
    { 0, eString },  //                             U+209B
    { 0, eString },  //                             U+209C
    { 0, eString },  //                             U+209D
    { 0, eString },  //                             U+209E
    { 0, eString },  //                             U+209F
    { 0, eString },  //                             U+20A0
    { 0, eString },  //                             U+20A1
    { 0, eString },  //                             U+20A2
    { 0, eString },  //                             U+20A3
    { 0, eString },  //                             U+20A4
    { 0, eString },  //                             U+20A5
    { 0, eString },  //                             U+20A6
    { 0, eString },  //                             U+20A7
    { 0, eString },  //                             U+20A8
    { 0, eString },  //                             U+20A9
    { 0, eString },  //                             U+20AA
    { 0, eString },  //                             U+20AB
    { 0, eString },  //                             U+20AC
    { 0, eString },  //                             U+20AD
    { 0, eString },  //                             U+20AE
    { 0, eString },  //                             U+20AF
    { 0, eString },  //                             U+20B0
    { 0, eString },  //                             U+20B1
    { 0, eString },  //                             U+20B2
    { 0, eString },  //                             U+20B3
    { 0, eString },  //                             U+20B4
    { 0, eString },  //                             U+20B5
    { 0, eString },  //                             U+20B6
    { 0, eString },  //                             U+20B7
    { 0, eString },  //                             U+20B8
    { 0, eString },  //                             U+20B9
    { 0, eString },  //                             U+20BA
    { 0, eString },  //                             U+20BB
    { 0, eString },  //                             U+20BC
    { 0, eString },  //                             U+20BD
    { 0, eString },  //                             U+20BE
    { 0, eString },  //                             U+20BF
    { 0, eString },  //                             U+20C0
    { 0, eString },  //                             U+20C1
    { 0, eString },  //                             U+20C2
    { 0, eString },  //                             U+20C3
    { 0, eString },  //                             U+20C4
    { 0, eString },  //                             U+20C5
    { 0, eString },  //                             U+20C6
    { 0, eString },  //                             U+20C7
    { 0, eString },  //                             U+20C8
    { 0, eString },  //                             U+20C9
    { 0, eString },  //                             U+20CA
    { 0, eString },  //                             U+20CB
    { 0, eString },  //                             U+20CC
    { 0, eString },  //                             U+20CD
    { 0, eString },  //                             U+20CE
    { 0, eString },  //                             U+20CF
    { 0, eString },  //                             U+20D0
    { 0, eString },  //                             U+20D1
    { 0, eString },  //                             U+20D2
    { 0, eString },  //                             U+20D3
    { 0, eString },  //                             U+20D4
    { 0, eString },  //                             U+20D5
    { 0, eString },  //                             U+20D6
    { 0, eString },  //                             U+20D7
    { 0, eString },  //                             U+20D8
    { 0, eString },  //                             U+20D9
    { 0, eString },  //                             U+20DA
    { "...", eString },  // old dictionary             U+20DB
    { "Dot;", eString },  // old dictionary            U+20DC
    { 0, eString },  //                             U+20DD
    { 0, eString },  //                             U+20DE
    { 0, eString },  //                             U+20DF
    { 0, eString },  //                             U+20E0
    { 0, eString },  //                             U+20E1
    { 0, eString },  //                             U+20E2
    { 0, eString },  //                             U+20E3
    { 0, eString },  //                             U+20E4
    { 0, eString },  //                             U+20E5
    { 0, eString },  //                             U+20E6
    { 0, eString },  //                             U+20E7
    { 0, eString },  //                             U+20E8
    { 0, eString },  //                             U+20E9
    { 0, eString },  //                             U+20EA
    { 0, eString },  //                             U+20EB
    { 0, eString },  //                             U+20EC
    { 0, eString },  //                             U+20ED
    { 0, eString },  //                             U+20EE
    { 0, eString },  //                             U+20EF
    { 0, eString },  //                             U+20F0
    { 0, eString },  //                             U+20F1
    { 0, eString },  //                             U+20F2
    { 0, eString },  //                             U+20F3
    { 0, eString },  //                             U+20F4
    { 0, eString },  //                             U+20F5
    { 0, eString },  //                             U+20F6
    { 0, eString },  //                             U+20F7
    { 0, eString },  //                             U+20F8
    { 0, eString },  //                             U+20F9
    { 0, eString },  //                             U+20FA
    { 0, eString },  //                             U+20FB
    { 0, eString },  //                             U+20FC
    { 0, eString },  //                             U+20FD
    { 0, eString },  //                             U+20FE
    { 0, eString },  //                             U+20FF
};
static TUnicodePlan s_Plan_21h = {
    { 0, eString },  //                                             U+2100
    { 0, eString },  //                                             U+2101
    { "C", eString },  //                                              U+2102
    { 0, eString },  //                                             U+2103
    { 0, eString },  //                                             U+2104
    { "in-care-of", eString },  // old dictionary                      U+2105
    { 0, eString },  //                                             U+2106
    { 0, eString },  //                                             U+2107
    { 0, eString },  //                                             U+2108
    { 0, eString },  //                                             U+2109
    { 0, eString },  //                                             U+210A
    { "H", eString },  //                                              U+210B
    { "H", eString },  //                                              U+210C
    { 0, eString },  //                                             U+210D
    { 0, eString },  //                                             U+210E
    { "variant Planck's over 2pi", eString },  //                      U+210F
    { "I", eString },  //                                              U+2110
    { " imaginary", eString },  // old dictionary                      U+2111
    { " Lgrangian ", eString },  // old dictionary                     U+2112
    { "l", eString },  // old dictionary                               U+2113
    { 0, eString },  //                                             U+2114
    { "N", eString },  //                                              U+2115
    { "numero sign", eString },  // old dictionary                     U+2116
    { "sound recording copyright sign", eString },  // old dictionary  U+2117
    { "Weierstrass p", eString },  // old dictionary                   U+2118
    { "P", eString },  //                                              U+2119
    { "Q", eString },  //                                              U+211A
    { 0, eString },  //                                             U+211B
    { "Re", eString },  // old dictionary                              U+211C
    { "R", eString },  //                                              U+211D
    { "Rx", eString },  // old dictionary                              U+211E
    { 0, eString },  //                                             U+211F
    { 0, eString },  //                                             U+2120
    { 0, eString },  //                                             U+2121
    { "trade mark", eString },  // old dictionary                      U+2122
    { 0, eString },  //                                             U+2123
    { "Z", eString },  //                                              U+2124
    { 0, eString },  //                                             U+2125
    { " ohm", eString },  // old dictionary                            U+2126
    { "conductance", eString },  //                                    U+2127
    { 0, eString },  //                                             U+2128
    { "inverted iota", eString },  //                                  U+2129
    { 0, eString },  //                                             U+212A
    { "A", eString },  // value > 0x80 in the old dict.                U+212B
    { " Bernoulli function", eString },  // old dictionary             U+212C
    { 0, eString },  //                                             U+212D
    { 0, eString },  //                                             U+212E
    { "e", eString },  //                                              U+212F
    { "E", eString },  //                                              U+2130
    { "F", eString },  //                                              U+2131
    { 0, eString },  //                                             U+2132
    { "M", eString },  //                                              U+2133
    { "o", eString },  //                                              U+2134
    { "aleph", eString },  // old dictionary                           U+2135
    { " beth", eString },  // old dictionary                           U+2136
    { "gimel", eString },  // old dictionary                           U+2137
    { " daleth", eString },  // old dictionary                         U+2138
    { 0, eString },  //                                             U+2139
    { 0, eString },  //                                             U+213A
    { 0, eString },  //                                             U+213B
    { 0, eString },  //                                             U+213C
    { 0, eString },  //                                             U+213D
    { 0, eString },  //                                             U+213E
    { 0, eString },  //                                             U+213F
    { 0, eString },  //                                             U+2140
    { 0, eString },  //                                             U+2141
    { 0, eString },  //                                             U+2142
    { 0, eString },  //                                             U+2143
    { 0, eString },  //                                             U+2144
    { 0, eString },  //                                             U+2145
    { 0, eString },  //                                             U+2146
    { 0, eString },  //                                             U+2147
    { 0, eString },  //                                             U+2148
    { 0, eString },  //                                             U+2149
    { 0, eString },  //                                             U+214A
    { 0, eString },  //                                             U+214B
    { 0, eString },  //                                             U+214C
    { 0, eString },  //                                             U+214D
    { 0, eString },  //                                             U+214E
    { 0, eString },  //                                             U+214F
    { 0, eString },  //                                             U+2150
    { 0, eString },  //                                             U+2151
    { 0, eString },  //                                             U+2152
    { "(1/3)", eString },  // old dictionary                           U+2153
    { "(2/3)", eString },  // old dictionary                           U+2154
    { "(1/5)", eString },  // old dictionary                           U+2155
    { "(2/5)", eString },  // old dictionary                           U+2156
    { "(3/5)", eString },  // old dictionary                           U+2157
    { "(4/5)", eString },  // old dictionary                           U+2158
    { "(1/6)", eString },  // old dictionary                           U+2159
    { "(5/6)", eString },  // old dictionary                           U+215A
    { "(1/8)", eString },  // old dictionary                           U+215B
    { "(3/8)", eString },  // old dictionary                           U+215C
    { "(5/8)", eString },  // old dictionary                           U+215D
    { "(7/8)", eString },  // old dictionary                           U+215E
    { 0, eString },  //                                             U+215F
    { 0, eString },  //                                             U+2160
    { 0, eString },  //                                             U+2161
    { 0, eString },  //                                             U+2162
    { 0, eString },  //                                             U+2163
    { 0, eString },  //                                             U+2164
    { 0, eString },  //                                             U+2165
    { 0, eString },  //                                             U+2166
    { 0, eString },  //                                             U+2167
    { 0, eString },  //                                             U+2168
    { 0, eString },  //                                             U+2169
    { 0, eString },  //                                             U+216A
    { 0, eString },  //                                             U+216B
    { 0, eString },  //                                             U+216C
    { 0, eString },  //                                             U+216D
    { 0, eString },  //                                             U+216E
    { 0, eString },  //                                             U+216F
    { 0, eString },  //                                             U+2170
    { 0, eString },  //                                             U+2171
    { 0, eString },  //                                             U+2172
    { 0, eString },  //                                             U+2173
    { 0, eString },  //                                             U+2174
    { 0, eString },  //                                             U+2175
    { 0, eString },  //                                             U+2176
    { 0, eString },  //                                             U+2177
    { 0, eString },  //                                             U+2178
    { 0, eString },  //                                             U+2179
    { 0, eString },  //                                             U+217A
    { 0, eString },  //                                             U+217B
    { 0, eString },  //                                             U+217C
    { 0, eString },  //                                             U+217D
    { 0, eString },  //                                             U+217E
    { 0, eString },  //                                             U+217F
    { 0, eString },  //                                             U+2180
    { 0, eString },  //                                             U+2181
    { 0, eString },  //                                             U+2182
    { 0, eString },  //                                             U+2183
    { 0, eString },  //                                             U+2184
    { 0, eString },  //                                             U+2185
    { 0, eString },  //                                             U+2186
    { 0, eString },  //                                             U+2187
    { 0, eString },  //                                             U+2188
    { 0, eString },  //                                             U+2189
    { 0, eString },  //                                             U+218A
    { 0, eString },  //                                             U+218B
    { 0, eString },  //                                             U+218C
    { 0, eString },  //                                             U+218D
    { 0, eString },  //                                             U+218E
    { 0, eString },  //                                             U+218F
    { "<--", eString },  // old dictionary                             U+2190
    { " upward arrow", eString },  // old dictionary                   U+2191
    { "-->", eString },  // old dictionary                             U+2192
    { " downward arrow", eString },  // old dictionary                 U+2193
    { "<-->", eString },  // old dictionary                            U+2194
    { "up and down arrow ", eString },  // old dictionary              U+2195
    { "NW pointing arrow ", eString },  // old dictionary              U+2196
    { "nearrow ", eString },  // old dictionary                        U+2197
    { "SE pointing arrow", eString },  //                              U+2198
    { "SW pointing arrow", eString },  //                              U+2199
    { "not left arrow ", eString },  // old dictionary                 U+219A
    { "not right arrow ", eString },  // old dictionary                U+219B
    { 0, eString },  //                                             U+219C
    { 0, eString },  //                                             U+219D
    { "two head left arrow", eString },  // old dictionary             U+219E
    { "up two-headed arrow", eString },  //                            U+219F
    { "two head right arrow", eString },  // old dictionary            U+21A0
    { "down two-headed arrow", eString },  //                          U+21A1
    { "left arrow-tailed ", eString },  // old dictionary              U+21A2
    { "right arrow-tailed", eString },  //                             U+21A3
    { 0, eString },  //                                             U+21A4
    { 0, eString },  //                                             U+21A5
    { "mapsto", eString },  // old dictionary                          U+21A6
    { 0, eString },  //                                             U+21A7
    { 0, eString },  //                                             U+21A8
    { "left arrow-hooked ", eString },  // old dictionary              U+21A9
    { "right arrow-hooked ", eString },  // old dictionary             U+21AA
    { "left arrow-looped ", eString },  // old dictionary              U+21AB
    { "right arrow-looped ", eString },  // old dictionary             U+21AC
    { "left and right squig arrow ", eString },  // old dictionary     U+21AD
    { "not l&r arrow ", eString },  // old dictionary                  U+21AE
    { 0, eString },  //                                             U+21AF
    { "Lsh", eString },  // old dictionary                             U+21B0
    { "Rsh", eString },  // old dictionary                             U+21B1
    { "left down angled arrow", eString },  //                         U+21B2
    { "right down angled arrow", eString },  //                        U+21B3
    { 0, eString },  //                                             U+21B4
    { 0, eString },  //                                             U+21B5
    { "left curved arrow ", eString },  // old dictionary              U+21B6
    { "right curved arrow ", eString },  // old dictionary             U+21B7
    { 0, eString },  //                                             U+21B8
    { 0, eString },  //                                             U+21B9
    { "left arrow in circle ", eString },  // old dictionary           U+21BA
    { "right arrow in circle ", eString },  // old dictionary          U+21BB
    { "left harpoon-up ", eString },  // old dictionary                U+21BC
    { "left harpoon-down ", eString },  // old dictionary              U+21BD
    { "up harp-right", eString },  // old dictionary                   U+21BE
    { "up harpoon-left ", eString },  // old dictionary                U+21BF
    { "right harpoon-up ", eString },  // old dictionary               U+21C0
    { "right harpoon-down ", eString },  // old dictionary             U+21C1
    { "down harpoon-right ", eString },  // old dictionary             U+21C2
    { "down harpoon-left ", eString },  // old dictionary              U+21C3
    { "right arrow over left arrow", eString },  //                    U+21C4
    { "up arrow, down arrow ", eString },  //                          U+21C5
    { "left arrow over right arrow", eString },  //                    U+21C6
    { "two left arrows", eString },  //                                U+21C7
    { "two up arrows", eString },  //                                  U+21C8
    { "two right arrows", eString },  //                               U+21C9
    { "two down arrows", eString },  //                                U+21CA
    { "left harpoon over right harpoon", eString },  //                U+21CB
    { "right harpoon over left harpoon", eString },  //                U+21CC
    { "not implied by", eString },  //                                 U+21CD
    { "not l&r dbl arr ", eString },  // old dictionary                U+21CE
    { "not implies", eString },  //                                    U+21CF
    { "<--", eString },  // old dictionary                             U+21D0
    { "up dbl arrow ", eString },  // old dictionary                   U+21D1
    { "-->", eString },  // old dictionary                             U+21D2
    { "down double arrow ", eString },  // old dictionary              U+21D3
    { "<-->", eString },  // old dictionary                            U+21D4
    { "up and down dbl arrow ", eString },  // old dictionary          U+21D5
    { "NW pointing dbl arrow", eString },  //                          U+21D6
    { "NE pointing double arrow", eString },  //                       U+21D7
    { "SE pointing double arrow", eString },  //                       U+21D8
    { "SW pointing dbl arrow", eString },  //                          U+21D9
    { "left triple arrow ", eString },  // old dictionary              U+21DA
    { "right triple arrow ", eString },  // old dictionary             U+21DB
    { 0, eString },  //                                             U+21DC
    { "right arrow-wavy", eString },  //                               U+21DD
    { 0, eString },  //                                             U+21DE
    { 0, eString },  //                                             U+21DF
    { 0, eString },  //                                             U+21E0
    { 0, eString },  //                                             U+21E1
    { 0, eString },  //                                             U+21E2
    { 0, eString },  //                                             U+21E3
    { 0, eString },  //                                             U+21E4
    { 0, eString },  //                                             U+21E5
    { 0, eString },  //                                             U+21E6
    { 0, eString },  //                                             U+21E7
    { 0, eString },  //                                             U+21E8
    { 0, eString },  //                                             U+21E9
    { 0, eString },  //                                             U+21EA
    { 0, eString },  //                                             U+21EB
    { 0, eString },  //                                             U+21EC
    { 0, eString },  //                                             U+21ED
    { 0, eString },  //                                             U+21EE
    { 0, eString },  //                                             U+21EF
    { 0, eString },  //                                             U+21F0
    { 0, eString },  //                                             U+21F1
    { 0, eString },  //                                             U+21F2
    { 0, eString },  //                                             U+21F3
    { 0, eString },  //                                             U+21F4
    { 0, eString },  //                                             U+21F5
    { 0, eString },  //                                             U+21F6
    { 0, eString },  //                                             U+21F7
    { 0, eString },  //                                             U+21F8
    { 0, eString },  //                                             U+21F9
    { 0, eString },  //                                             U+21FA
    { 0, eString },  //                                             U+21FB
    { 0, eString },  //                                             U+21FC
    { 0, eString },  //                                             U+21FD
    { 0, eString },  //                                             U+21FE
    { 0, eString },  //                                             U+21FF
};
static TUnicodePlan s_Plan_22h = {
    { " for all", eString },  // old dictionary                    U+2200
    { " complement", eString },  // old dictionary                 U+2201
    { " partial differential", eString },  // old dictionary       U+2202
    { " exists", eString },  // old dictionary                     U+2203
    { "negated exists", eString },  // old dictionary              U+2204
    { "slashed circle", eString },  //                             U+2205
    { 0, eString },  //                                         U+2206
    { "nabla", eString },  // old dictionary                       U+2207
    { "in", eString },  // old dictionary                          U+2208
    { " negated set membership", eString },  // old dictionary     U+2209
    { 0, eString },  //                                         U+220A
    { "contains", eString },  //                                   U+220B
    { "negated contains, variant", eString },  //                  U+220C
    { 0, eString },  //                                         U+220D
    { 0, eString },  //                                         U+220E
    { " product operator", eString },  // old dictionary           U+220F
    { " coproduct operator", eString },  // old dictionary         U+2210
    { " summation operator", eString },  // old dictionary         U+2211
    { "-", eString },  // old dictionary                           U+2212
    { "-/+", eString },  // old dictionary                         U+2213
    { " plus sign, dot above", eString },  // old dictionary       U+2214
    { 0, eString },  //                                         U+2215
    { "\\", eString },  // old dictionary                          U+2216
    { " *", eString },  // old dictionary                          U+2217
    { " composite function", eString },  // old dictionary         U+2218
    { 0, eString },  //                                         U+2219
    { " radical", eString },  // old dictionary                    U+221A
    { 0, eString },  //                                         U+221B
    { 0, eString },  //                                         U+221C
    { " proportional, variant", eString },  // old dictionary      U+221D
    { "infinity", eString },  // old dictionary                    U+221E
    { "right (90 degree) angle", eString },  //                    U+221F
    { " angle", eString },  // old dictionary                      U+2220
    { " measured angle", eString },  // old dictionary             U+2221
    { "angle-spherical", eString },  // old dictionary             U+2222
    { "mid R:", eString },  // old dictionary                      U+2223
    { " nmid", eString },  // old dictionary                       U+2224
    { " parallel", eString },  // old dictionary                   U+2225
    { " not parallel", eString },  // old dictionary               U+2226
    { "wedge", eString },  // old dictionary                       U+2227
    { " logical or", eString },  // old dictionary                 U+2228
    { " intersection", eString },  // old dictionary               U+2229
    { " union or logical sum", eString },  // old dictionary       U+222A
    { "integral", eString },  // old dictionary                    U+222B
    { "double integral operator", eString },  //                   U+222C
    { "triple integral", eString },  //                            U+222D
    { " contour integral operator", eString },  // old dictionary  U+222E
    { "double contour integral operator", eString },  //           U+222F
    { "triple contour integral operator", eString },  //           U+2230
    { "clockwise integral", eString },  //                         U+2231
    { "contour integral, clockwise", eString },  //                U+2232
    { "contour integral, anti-clockwise", eString },  //           U+2233
    { " therefore", eString },  // old dictionary                  U+2234
    { " because", eString },  // old dictionary                    U+2235
    { "ratio", eString },  //                                      U+2236
    { "Colon, two colons", eString },  //                          U+2237
    { "minus sign, dot above", eString },  //                      U+2238
    { 0, eString },  //                                         U+2239
    { "minus with four dots, geometric properties", eString },  // U+223A
    { "homothetic", eString },  //                                 U+223B
    { " approximately ", eString },  // old dictionary             U+223C
    { " reverse similar", eString },  // old dictionary            U+223D
    { "most positive", eString },  //                              U+223E
    { 0, eString },  //                                         U+223F
    { " wreath product", eString },  // old dictionary             U+2240
    { " not similar", eString },  // old dictionary                U+2241
    { "equals, similar", eString },  //                            U+2242
    { " approximately ", eString },  // old dictionary             U+2243
    { " not similar, equals", eString },  // old dictionary        U+2244
    { " congruent with", eString },  // old dictionary             U+2245
    { "similar, not equals", eString },  //                        U+2246
    { " not congruent with", eString },  // old dictionary         U+2247
    { " approximately ", eString },  // old dictionary             U+2248
    { " not approximate", eString },  // old dictionary            U+2249
    { "approximate, equals", eString },  //                        U+224A
    { "approximately identical to", eString },  //                 U+224B
    { " everse congruent", eString },  // old dictionary           U+224C
    { " asymptotically equal to", eString },  // old dictionary    U+224D
    { " bumpy equals", eString },  // old dictionary               U+224E
    { " bumpy equals, equals", eString },  // old dictionary       U+224F
    { "equals, single dot above", eString },  // old dictionary    U+2250
    { "=...", eString },  // old dictionary                        U+2251
    { " falling dots", eString },  // old dictionary               U+2252
    { " rising dots", eString },  // old dictionary                U+2253
    { ":=", eString },  // old dictionary                          U+2254
    { "=:", eString },  // old dictionary                          U+2255
    { " circle on equals sign", eString },  // old dictionary      U+2256
    { "o=", eString },  // old dictionary                          U+2257
    { 0, eString },  //                                         U+2258
    { " wedge", eString },  // old dictionary                      U+2259
    { "logical or, equals", eString },  //                         U+225A
    { "equal, asterisk above", eString },  //                      U+225B
    { " triangle, equals", eString },  // old dictionary           U+225C
    { 0, eString },  //                                         U+225D
    { 0, eString },  //                                         U+225E
    { "equal with questionmark", eString },  //                    U+225F
    { " not equal", eString },  // old dictionary                  U+2260
    { " identical with", eString },  // old dictionary             U+2261
    { " not identical with", eString },  // old dictionary         U+2262
    { 0, eString },  //                                         U+2263
    { "</=", eString },  // old dictionary                         U+2264
    { ">/=", eString },  // old dictionary                         U+2265
    { "<==", eString },  // old dictionary                         U+2266
    { ">==", eString },  // old dictionary                         U+2267
    { " less, not double equals", eString },  // old dictionary    U+2268
    { " greater, not dbl equals", eString },  // old dictionary    U+2269
    { "<<", eString },  // old dictionary                          U+226A
    { ">>", eString },  // old dictionary                          U+226B
    { " between", eString },  // old dictionary                    U+226C
    { 0, eString },  //                                         U+226D
    { " not less-than", eString },  // old dictionary              U+226E
    { "not greater-than", eString },  //                           U+226F
    { " not less, dbl equals", eString },  // old dictionary       U+2270
    { "not gt-or-eq, slanted", eString },  //                      U+2271
    { " less, similar", eString },  // old dictionary              U+2272
    { " greater, similar", eString },  // old dictionary           U+2273
    { "not less, similar", eString },  //                          U+2274
    { "not greater, similar", eString },  //                       U+2275
    { "<>", eString },  // old dictionary                          U+2276
    { "><", eString },  // old dictionary                          U+2277
    { "not less, greater", eString },  //                          U+2278
    { "not greater, less", eString },  //                          U+2279
    { " precedes", eString },  // old dictionary                   U+227A
    { " succeeds", eString },  // old dictionary                   U+227B
    { "precedes, curly equals", eString },  //                     U+227C
    { " succeeds, equals", eString },  // old dictionary           U+227D
    { " similar", eString },  // old dictionary                    U+227E
    { " succeeds, similar", eString },  // old dictionary          U+227F
    { " not precedes", eString },  // old dictionary               U+2280
    { " not succeeds", eString },  // old dictionary               U+2281
    { " subset", eString },  // old dictionary                     U+2282
    { " superset", eString },  // old dictionary                   U+2283
    { "not subset", eString },  //                                 U+2284
    { " not superset", eString },  // old dictionary               U+2285
    { " subset, dbl equals", eString },  // old dictionary         U+2286
    { " superset, dbl equals", eString },  // old dictionary       U+2287
    { " not subset, dbl eq", eString },  // old dictionary         U+2288
    { " not superset, equals", eString },  // old dictionary       U+2289
    { " subset, not equals", eString },  // old dictionary         U+228A
    { " superset, not dbl eq", eString },  // old dictionary       U+228B
    { 0, eString },  //                                         U+228C
    { "union, with dot", eString },  //                            U+228D
    { " plus sign in union", eString },  // old dictionary         U+228E
    { " square subset", eString },  // old dictionary              U+228F
    { " square superset", eString },  // old dictionary            U+2290
    { " square subset, equals", eString },  // old dictionary      U+2291
    { " square superset, eq", eString },  // old dictionary        U+2292
    { " square intersection", eString },  // old dictionary        U+2293
    { "square union", eString },  //                               U+2294
    { " plus sign in circle", eString },  // old dictionary        U+2295
    { " minus sign in circle", eString },  // old dictionary       U+2296
    { "multiply sign in circle", eString },  //                    U+2297
    { " solidus in circle", eString },  // old dictionary          U+2298
    { "middle dot in circle", eString },  //                       U+2299
    { " open dot in circle", eString },  // old dictionary         U+229A
    { " asterisk in circle", eString },  // old dictionary         U+229B
    { 0, eString },  //                                         U+229C
    { " hyphen in circle", eString },  // old dictionary           U+229D
    { " plus sign in box", eString },  // old dictionary           U+229E
    { " minus sign in box", eString },  // old dictionary          U+229F
    { " multiply sign in box", eString },  // old dictionary       U+22A0
    { " small dot in box", eString },  // old dictionary           U+22A1
    { " vertical, dash", eString },  // old dictionary             U+22A2
    { " dash, vertical", eString },  // old dictionary             U+22A3
    { " inverted perpendicular", eString },  // old dictionary     U+22A4
    { " perpendicular", eString },  // old dictionary              U+22A5
    { 0, eString },  //                                         U+22A6
    { "models R:", eString },  // old dictionary                   U+22A7
    { " vertical, dbl dash", eString },  // old dictionary         U+22A8
    { " dbl vertical, dash", eString },  // old dictionary         U+22A9
    { " triple vertical, dash", eString },  // old dictionary      U+22AA
    { "dbl vertical, dbl dash", eString },  //                     U+22AB
    { " not vertical, dash", eString },  // old dictionary         U+22AC
    { " not vertical, dbl dash", eString },  // old dictionary     U+22AD
    { " not dbl vertical, dash", eString },  // old dictionary     U+22AE
    { " not dbl vert, dbl dash", eString },  // old dictionary     U+22AF
    { "element precedes under relation", eString },  //            U+22B0
    { 0, eString },  //                                         U+22B1
    { " left tri, open, var", eString },  // old dictionary        U+22B2
    { " right tri, open, var", eString },  // old dictionary       U+22B3
    { " left triangle, eq", eString },  // old dictionary          U+22B4
    { " right tri, eq", eString },  // old dictionary              U+22B5
    { "original of", eString },  //                                U+22B6
    { "image of", eString },  //                                   U+22B7
    { "multimap", eString },  // old dictionary                    U+22B8
    { "hermitian conjugate matrix", eString },  //                 U+22B9
    { " intercal", eString },  // old dictionary                   U+22BA
    { " logical or, bar below", eString },  // old dictionary      U+22BB
    { " logical and, bar above", eString },  // old dictionary     U+22BC
    { "bar, vee", eString },  //                                   U+22BD
    { "right angle, variant", eString },  //                       U+22BE
    { 0, eString },  //                                         U+22BF
    { "logical or operator", eString },  //                        U+22C0
    { "logical and operator", eString },  //                       U+22C1
    { "intersection operator", eString },  //                      U+22C2
    { "union operator", eString },  //                             U+22C3
    { " open diamond", eString },  // old dictionary               U+22C4
    { 0, eString },  //                                         U+22C5
    { " small star, filled", eString },  // old dictionary         U+22C6
    { " division on times", eString },  // old dictionary          U+22C7
    { " bowtie R", eString },  // old dictionary                   U+22C8
    { " times sign, left closed", eString },  // old dictionary    U+22C9
    { " times sign, right closed", eString },  // old dictionary   U+22CA
    { " leftthreetimes", eString },  // old dictionary             U+22CB
    { " rightthreetimes ", eString },  // old dictionary           U+22CC
    { " reverse similar", eString },  // old dictionary            U+22CD
    { " curly logical or", eString },  // old dictionary           U+22CE
    { " curly logical and", eString },  // old dictionary          U+22CF
    { " double subset", eString },  // old dictionary              U+22D0
    { " dbl superset", eString },  // old dictionary               U+22D1
    { " dbl intersection", eString },  // old dictionary           U+22D2
    { " dbl union", eString },  // old dictionary                  U+22D3
    { " pitchfork", eString },  // old dictionary                  U+22D4
    { "equal or parallel", eString },  //                          U+22D5
    { "less than, with dot", eString },  //                        U+22D6
    { "greater than, with dot", eString },  //                     U+22D7
    { "<<<", eString },  // old dictionary                         U+22D8
    { ">>>", eString },  // old dictionary                         U+22D9
    { "<=>", eString },  // old dictionary                         U+22DA
    { ">=<", eString },  // old dictionary                         U+22DB
    { "=/<", eString },  // old dictionary                         U+22DC
    { "=/>", eString },  // old dictionary                         U+22DD
    { " precedes", eString },  // old dictionary                   U+22DE
    { " succeeds", eString },  // old dictionary                   U+22DF
    { 0, eString },  //                                         U+22E0
    { 0, eString },  //                                         U+22E1
    { 0, eString },  //                                         U+22E2
    { 0, eString },  //                                         U+22E3
    { 0, eString },  //                                         U+22E4
    { 0, eString },  //                                         U+22E5
    { " less, not similar", eString },  // old dictionary          U+22E6
    { " greater, not similar", eString },  // old dictionary       U+22E7
    { " precedes, not approx", eString },  // old dictionary       U+22E8
    { " succeeds, not approx", eString },  // old dictionary       U+22E9
    { " not left triangle", eString },  // old dictionary          U+22EA
    { " not rt triangle", eString },  // old dictionary            U+22EB
    { " not l tri, eq", eString },  // old dictionary              U+22EC
    { " not r tri, eq", eString },  // old dictionary              U+22ED
    { " vertical ellipsis", eString },  // old dictionary          U+22EE
    { "cdots, three dots, centered", eString },  //                U+22EF
    { "three dots, ascending", eString },  //                      U+22F0
    { "ddots, three dots, descending", eString },  //              U+22F1
    { 0, eString },  //                                         U+22F2
    { 0, eString },  //                                         U+22F3
    { 0, eString },  //                                         U+22F4
    { 0, eString },  //                                         U+22F5
    { 0, eString },  //                                         U+22F6
    { 0, eString },  //                                         U+22F7
    { 0, eString },  //                                         U+22F8
    { 0, eString },  //                                         U+22F9
    { 0, eString },  //                                         U+22FA
    { 0, eString },  //                                         U+22FB
    { 0, eString },  //                                         U+22FC
    { 0, eString },  //                                         U+22FD
    { 0, eString },  //                                         U+22FE
    { 0, eString },  //                                         U+22FF
};
static TUnicodePlan s_Plan_23h = {
    { 0, eString },  //                                        U+2300
    { 0, eString },  //                                        U+2301
    { 0, eString },  //                                        U+2302
    { 0, eString },  //                                        U+2303
    { 0, eString },  //                                        U+2304
    { 0, eString },  //                                        U+2305
    { " log and, dbl bar", eString },  // old dictionary          U+2306
    { 0, eString },  //                                        U+2307
    { " left ceiling", eString },  // old dictionary              U+2308
    { " right ceiling", eString },  // old dictionary             U+2309
    { " left floor", eString },  // old dictionary                U+230A
    { " right floor", eString },  // old dictionary               U+230B
    { "downward right crop mark ", eString },  // old dictionary  U+230C
    { "downward left crop mark ", eString },  // old dictionary   U+230D
    { "upward right crop mark ", eString },  // old dictionary    U+230E
    { "upward left crop mark ", eString },  // old dictionary     U+230F
    { "reverse not", eString },  //                               U+2310
    { 0, eString },  //                                        U+2311
    { "profile of a line", eString },  //                         U+2312
    { "profile of a surface", eString },  //                      U+2313
    { 0, eString },  //                                        U+2314
    { "telephone recorder symbol", eString },  // old dictionary  U+2315
    { "register mark or target", eString },  // old dictionary    U+2316
    { 0, eString },  //                                        U+2317
    { 0, eString },  //                                        U+2318
    { 0, eString },  //                                        U+2319
    { 0, eString },  //                                        U+231A
    { 0, eString },  //                                        U+231B
    { " upper left corner", eString },  // old dictionary         U+231C
    { " upper right corner", eString },  // old dictionary        U+231D
    { " downward left corner", eString },  // old dictionary      U+231E
    { " downward right corner", eString },  // old dictionary     U+231F
    { 0, eString },  //                                        U+2320
    { 0, eString },  //                                        U+2321
    { " down curve", eString },  // old dictionary                U+2322
    { " up curve", eString },  // old dictionary                  U+2323
    { 0, eString },  //                                        U+2324
    { 0, eString },  //                                        U+2325
    { 0, eString },  //                                        U+2326
    { 0, eString },  //                                        U+2327
    { 0, eString },  //                                        U+2328
    { 0, eString },  //                                        U+2329
    { 0, eString },  //                                        U+232A
    { 0, eString },  //                                        U+232B
    { 0, eString },  //                                        U+232C
    { "cylindricity", eString },  //                              U+232D
    { "all-around profile", eString },  //                        U+232E
    { 0, eString },  //                                        U+232F
    { 0, eString },  //                                        U+2330
    { 0, eString },  //                                        U+2331
    { 0, eString },  //                                        U+2332
    { 0, eString },  //                                        U+2333
    { 0, eString },  //                                        U+2334
    { 0, eString },  //                                        U+2335
    { "top and bottom", eString },  //                            U+2336
    { 0, eString },  //                                        U+2337
    { 0, eString },  //                                        U+2338
    { 0, eString },  //                                        U+2339
    { 0, eString },  //                                        U+233A
    { 0, eString },  //                                        U+233B
    { 0, eString },  //                                        U+233C
    { 0, eString },  //                                        U+233D
    { 0, eString },  //                                        U+233E
    { 0, eString },  //                                        U+233F
    { 0, eString },  //                                        U+2340
    { 0, eString },  //                                        U+2341
    { 0, eString },  //                                        U+2342
    { 0, eString },  //                                        U+2343
    { 0, eString },  //                                        U+2344
    { 0, eString },  //                                        U+2345
    { 0, eString },  //                                        U+2346
    { 0, eString },  //                                        U+2347
    { 0, eString },  //                                        U+2348
    { 0, eString },  //                                        U+2349
    { 0, eString },  //                                        U+234A
    { 0, eString },  //                                        U+234B
    { 0, eString },  //                                        U+234C
    { 0, eString },  //                                        U+234D
    { 0, eString },  //                                        U+234E
    { 0, eString },  //                                        U+234F
    { 0, eString },  //                                        U+2350
    { 0, eString },  //                                        U+2351
    { 0, eString },  //                                        U+2352
    { 0, eString },  //                                        U+2353
    { 0, eString },  //                                        U+2354
    { 0, eString },  //                                        U+2355
    { 0, eString },  //                                        U+2356
    { 0, eString },  //                                        U+2357
    { 0, eString },  //                                        U+2358
    { 0, eString },  //                                        U+2359
    { 0, eString },  //                                        U+235A
    { 0, eString },  //                                        U+235B
    { 0, eString },  //                                        U+235C
    { 0, eString },  //                                        U+235D
    { 0, eString },  //                                        U+235E
    { 0, eString },  //                                        U+235F
    { 0, eString },  //                                        U+2360
    { 0, eString },  //                                        U+2361
    { 0, eString },  //                                        U+2362
    { 0, eString },  //                                        U+2363
    { 0, eString },  //                                        U+2364
    { 0, eString },  //                                        U+2365
    { 0, eString },  //                                        U+2366
    { 0, eString },  //                                        U+2367
    { 0, eString },  //                                        U+2368
    { 0, eString },  //                                        U+2369
    { 0, eString },  //                                        U+236A
    { 0, eString },  //                                        U+236B
    { 0, eString },  //                                        U+236C
    { 0, eString },  //                                        U+236D
    { 0, eString },  //                                        U+236E
    { 0, eString },  //                                        U+236F
    { 0, eString },  //                                        U+2370
    { 0, eString },  //                                        U+2371
    { 0, eString },  //                                        U+2372
    { 0, eString },  //                                        U+2373
    { 0, eString },  //                                        U+2374
    { 0, eString },  //                                        U+2375
    { 0, eString },  //                                        U+2376
    { 0, eString },  //                                        U+2377
    { 0, eString },  //                                        U+2378
    { 0, eString },  //                                        U+2379
    { 0, eString },  //                                        U+237A
    { 0, eString },  //                                        U+237B
    { 0, eString },  //                                        U+237C
    { 0, eString },  //                                        U+237D
    { 0, eString },  //                                        U+237E
    { 0, eString },  //                                        U+237F
    { 0, eString },  //                                        U+2380
    { 0, eString },  //                                        U+2381
    { 0, eString },  //                                        U+2382
    { 0, eString },  //                                        U+2383
    { 0, eString },  //                                        U+2384
    { 0, eString },  //                                        U+2385
    { 0, eString },  //                                        U+2386
    { 0, eString },  //                                        U+2387
    { 0, eString },  //                                        U+2388
    { 0, eString },  //                                        U+2389
    { 0, eString },  //                                        U+238A
    { 0, eString },  //                                        U+238B
    { 0, eString },  //                                        U+238C
    { 0, eString },  //                                        U+238D
    { 0, eString },  //                                        U+238E
    { 0, eString },  //                                        U+238F
    { 0, eString },  //                                        U+2390
    { 0, eString },  //                                        U+2391
    { 0, eString },  //                                        U+2392
    { 0, eString },  //                                        U+2393
    { 0, eString },  //                                        U+2394
    { 0, eString },  //                                        U+2395
    { 0, eString },  //                                        U+2396
    { 0, eString },  //                                        U+2397
    { 0, eString },  //                                        U+2398
    { 0, eString },  //                                        U+2399
    { 0, eString },  //                                        U+239A
    { 0, eString },  //                                        U+239B
    { 0, eString },  //                                        U+239C
    { 0, eString },  //                                        U+239D
    { 0, eString },  //                                        U+239E
    { 0, eString },  //                                        U+239F
    { 0, eString },  //                                        U+23A0
    { 0, eString },  //                                        U+23A1
    { 0, eString },  //                                        U+23A2
    { 0, eString },  //                                        U+23A3
    { 0, eString },  //                                        U+23A4
    { 0, eString },  //                                        U+23A5
    { 0, eString },  //                                        U+23A6
    { 0, eString },  //                                        U+23A7
    { 0, eString },  //                                        U+23A8
    { 0, eString },  //                                        U+23A9
    { 0, eString },  //                                        U+23AA
    { 0, eString },  //                                        U+23AB
    { 0, eString },  //                                        U+23AC
    { 0, eString },  //                                        U+23AD
    { 0, eString },  //                                        U+23AE
    { 0, eString },  //                                        U+23AF
    { 0, eString },  //                                        U+23B0
    { 0, eString },  //                                        U+23B1
    { 0, eString },  //                                        U+23B2
    { 0, eString },  //                                        U+23B3
    { 0, eString },  //                                        U+23B4
    { 0, eString },  //                                        U+23B5
    { 0, eString },  //                                        U+23B6
    { 0, eString },  //                                        U+23B7
    { 0, eString },  //                                        U+23B8
    { 0, eString },  //                                        U+23B9
    { 0, eString },  //                                        U+23BA
    { 0, eString },  //                                        U+23BB
    { 0, eString },  //                                        U+23BC
    { 0, eString },  //                                        U+23BD
    { 0, eString },  //                                        U+23BE
    { 0, eString },  //                                        U+23BF
    { 0, eString },  //                                        U+23C0
    { 0, eString },  //                                        U+23C1
    { 0, eString },  //                                        U+23C2
    { 0, eString },  //                                        U+23C3
    { 0, eString },  //                                        U+23C4
    { 0, eString },  //                                        U+23C5
    { 0, eString },  //                                        U+23C6
    { 0, eString },  //                                        U+23C7
    { 0, eString },  //                                        U+23C8
    { 0, eString },  //                                        U+23C9
    { 0, eString },  //                                        U+23CA
    { 0, eString },  //                                        U+23CB
    { 0, eString },  //                                        U+23CC
    { 0, eString },  //                                        U+23CD
    { 0, eString },  //                                        U+23CE
    { 0, eString },  //                                        U+23CF
    { 0, eString },  //                                        U+23D0
    { 0, eString },  //                                        U+23D1
    { 0, eString },  //                                        U+23D2
    { 0, eString },  //                                        U+23D3
    { 0, eString },  //                                        U+23D4
    { 0, eString },  //                                        U+23D5
    { 0, eString },  //                                        U+23D6
    { 0, eString },  //                                        U+23D7
    { 0, eString },  //                                        U+23D8
    { 0, eString },  //                                        U+23D9
    { 0, eString },  //                                        U+23DA
    { 0, eString },  //                                        U+23DB
    { 0, eString },  //                                        U+23DC
    { 0, eString },  //                                        U+23DD
    { 0, eString },  //                                        U+23DE
    { 0, eString },  //                                        U+23DF
    { 0, eString },  //                                        U+23E0
    { 0, eString },  //                                        U+23E1
    { 0, eString },  //                                        U+23E2
    { 0, eString },  //                                        U+23E3
    { 0, eString },  //                                        U+23E4
    { 0, eString },  //                                        U+23E5
    { 0, eString },  //                                        U+23E6
    { 0, eString },  //                                        U+23E7
    { 0, eString },  //                                        U+23E8
    { 0, eString },  //                                        U+23E9
    { 0, eString },  //                                        U+23EA
    { 0, eString },  //                                        U+23EB
    { 0, eString },  //                                        U+23EC
    { 0, eString },  //                                        U+23ED
    { 0, eString },  //                                        U+23EE
    { 0, eString },  //                                        U+23EF
    { 0, eString },  //                                        U+23F0
    { 0, eString },  //                                        U+23F1
    { 0, eString },  //                                        U+23F2
    { 0, eString },  //                                        U+23F3
    { 0, eString },  //                                        U+23F4
    { 0, eString },  //                                        U+23F5
    { 0, eString },  //                                        U+23F6
    { 0, eString },  //                                        U+23F7
    { 0, eString },  //                                        U+23F8
    { 0, eString },  //                                        U+23F9
    { 0, eString },  //                                        U+23FA
    { 0, eString },  //                                        U+23FB
    { 0, eString },  //                                        U+23FC
    { 0, eString },  //                                        U+23FD
    { 0, eString },  //                                        U+23FE
    { 0, eString },  //                                        U+23FF
};
static TUnicodePlan s_Plan_24h = {
    { 0, eString },  //                          U+2400
    { 0, eString },  //                          U+2401
    { 0, eString },  //                          U+2402
    { 0, eString },  //                          U+2403
    { 0, eString },  //                          U+2404
    { 0, eString },  //                          U+2405
    { 0, eString },  //                          U+2406
    { 0, eString },  //                          U+2407
    { 0, eString },  //                          U+2408
    { 0, eString },  //                          U+2409
    { 0, eString },  //                          U+240A
    { 0, eString },  //                          U+240B
    { 0, eString },  //                          U+240C
    { 0, eString },  //                          U+240D
    { 0, eString },  //                          U+240E
    { 0, eString },  //                          U+240F
    { 0, eString },  //                          U+2410
    { 0, eString },  //                          U+2411
    { 0, eString },  //                          U+2412
    { 0, eString },  //                          U+2413
    { 0, eString },  //                          U+2414
    { 0, eString },  //                          U+2415
    { 0, eString },  //                          U+2416
    { 0, eString },  //                          U+2417
    { 0, eString },  //                          U+2418
    { 0, eString },  //                          U+2419
    { 0, eString },  //                          U+241A
    { 0, eString },  //                          U+241B
    { 0, eString },  //                          U+241C
    { 0, eString },  //                          U+241D
    { 0, eString },  //                          U+241E
    { 0, eString },  //                          U+241F
    { 0, eString },  //                          U+2420
    { 0, eString },  //                          U+2421
    { 0, eString },  //                          U+2422
    { " ", eString },  // old dictionary            U+2423
    { 0, eString },  //                          U+2424
    { 0, eString },  //                          U+2425
    { 0, eString },  //                          U+2426
    { 0, eString },  //                          U+2427
    { 0, eString },  //                          U+2428
    { 0, eString },  //                          U+2429
    { 0, eString },  //                          U+242A
    { 0, eString },  //                          U+242B
    { 0, eString },  //                          U+242C
    { 0, eString },  //                          U+242D
    { 0, eString },  //                          U+242E
    { 0, eString },  //                          U+242F
    { 0, eString },  //                          U+2430
    { 0, eString },  //                          U+2431
    { 0, eString },  //                          U+2432
    { 0, eString },  //                          U+2433
    { 0, eString },  //                          U+2434
    { 0, eString },  //                          U+2435
    { 0, eString },  //                          U+2436
    { 0, eString },  //                          U+2437
    { 0, eString },  //                          U+2438
    { 0, eString },  //                          U+2439
    { 0, eString },  //                          U+243A
    { 0, eString },  //                          U+243B
    { 0, eString },  //                          U+243C
    { 0, eString },  //                          U+243D
    { 0, eString },  //                          U+243E
    { 0, eString },  //                          U+243F
    { 0, eString },  //                          U+2440
    { 0, eString },  //                          U+2441
    { 0, eString },  //                          U+2442
    { 0, eString },  //                          U+2443
    { 0, eString },  //                          U+2444
    { 0, eString },  //                          U+2445
    { 0, eString },  //                          U+2446
    { 0, eString },  //                          U+2447
    { 0, eString },  //                          U+2448
    { 0, eString },  //                          U+2449
    { "\\", eString },  //                          U+244A
    { 0, eString },  //                          U+244B
    { 0, eString },  //                          U+244C
    { 0, eString },  //                          U+244D
    { 0, eString },  //                          U+244E
    { 0, eString },  //                          U+244F
    { 0, eString },  //                          U+2450
    { 0, eString },  //                          U+2451
    { 0, eString },  //                          U+2452
    { 0, eString },  //                          U+2453
    { 0, eString },  //                          U+2454
    { 0, eString },  //                          U+2455
    { 0, eString },  //                          U+2456
    { 0, eString },  //                          U+2457
    { 0, eString },  //                          U+2458
    { 0, eString },  //                          U+2459
    { 0, eString },  //                          U+245A
    { 0, eString },  //                          U+245B
    { 0, eString },  //                          U+245C
    { 0, eString },  //                          U+245D
    { 0, eString },  //                          U+245E
    { 0, eString },  //                          U+245F
    { "1 in circle", eString },  //                 U+2460
    { "2 in circle", eString },  //                 U+2461
    { "3 in circle", eString },  //                 U+2462
    { "4 in circle", eString },  //                 U+2463
    { 0, eString },  //                          U+2464
    { 0, eString },  //                          U+2465
    { 0, eString },  //                          U+2466
    { 0, eString },  //                          U+2467
    { 0, eString },  //                          U+2468
    { 0, eString },  //                          U+2469
    { 0, eString },  //                          U+246A
    { 0, eString },  //                          U+246B
    { 0, eString },  //                          U+246C
    { 0, eString },  //                          U+246D
    { 0, eString },  //                          U+246E
    { 0, eString },  //                          U+246F
    { 0, eString },  //                          U+2470
    { 0, eString },  //                          U+2471
    { 0, eString },  //                          U+2472
    { 0, eString },  //                          U+2473
    { 0, eString },  //                          U+2474
    { 0, eString },  //                          U+2475
    { 0, eString },  //                          U+2476
    { 0, eString },  //                          U+2477
    { 0, eString },  //                          U+2478
    { 0, eString },  //                          U+2479
    { 0, eString },  //                          U+247A
    { 0, eString },  //                          U+247B
    { 0, eString },  //                          U+247C
    { 0, eString },  //                          U+247D
    { 0, eString },  //                          U+247E
    { 0, eString },  //                          U+247F
    { 0, eString },  //                          U+2480
    { 0, eString },  //                          U+2481
    { 0, eString },  //                          U+2482
    { 0, eString },  //                          U+2483
    { 0, eString },  //                          U+2484
    { 0, eString },  //                          U+2485
    { 0, eString },  //                          U+2486
    { 0, eString },  //                          U+2487
    { 0, eString },  //                          U+2488
    { 0, eString },  //                          U+2489
    { 0, eString },  //                          U+248A
    { 0, eString },  //                          U+248B
    { 0, eString },  //                          U+248C
    { 0, eString },  //                          U+248D
    { 0, eString },  //                          U+248E
    { 0, eString },  //                          U+248F
    { 0, eString },  //                          U+2490
    { 0, eString },  //                          U+2491
    { 0, eString },  //                          U+2492
    { 0, eString },  //                          U+2493
    { 0, eString },  //                          U+2494
    { 0, eString },  //                          U+2495
    { 0, eString },  //                          U+2496
    { 0, eString },  //                          U+2497
    { 0, eString },  //                          U+2498
    { 0, eString },  //                          U+2499
    { 0, eString },  //                          U+249A
    { 0, eString },  //                          U+249B
    { 0, eString },  //                          U+249C
    { 0, eString },  //                          U+249D
    { 0, eString },  //                          U+249E
    { 0, eString },  //                          U+249F
    { 0, eString },  //                          U+24A0
    { 0, eString },  //                          U+24A1
    { 0, eString },  //                          U+24A2
    { 0, eString },  //                          U+24A3
    { 0, eString },  //                          U+24A4
    { 0, eString },  //                          U+24A5
    { 0, eString },  //                          U+24A6
    { 0, eString },  //                          U+24A7
    { 0, eString },  //                          U+24A8
    { 0, eString },  //                          U+24A9
    { 0, eString },  //                          U+24AA
    { 0, eString },  //                          U+24AB
    { 0, eString },  //                          U+24AC
    { 0, eString },  //                          U+24AD
    { 0, eString },  //                          U+24AE
    { 0, eString },  //                          U+24AF
    { 0, eString },  //                          U+24B0
    { 0, eString },  //                          U+24B1
    { 0, eString },  //                          U+24B2
    { 0, eString },  //                          U+24B3
    { 0, eString },  //                          U+24B4
    { 0, eString },  //                          U+24B5
    { 0, eString },  //                          U+24B6
    { 0, eString },  //                          U+24B7
    { 0, eString },  //                          U+24B8
    { 0, eString },  //                          U+24B9
    { 0, eString },  //                          U+24BA
    { 0, eString },  //                          U+24BB
    { 0, eString },  //                          U+24BC
    { 0, eString },  //                          U+24BD
    { 0, eString },  //                          U+24BE
    { 0, eString },  //                          U+24BF
    { 0, eString },  //                          U+24C0
    { 0, eString },  //                          U+24C1
    { 0, eString },  //                          U+24C2
    { 0, eString },  //                          U+24C3
    { 0, eString },  //                          U+24C4
    { 0, eString },  //                          U+24C5
    { 0, eString },  //                          U+24C6
    { 0, eString },  //                          U+24C7
    { "S in circle", eString },  // old dictionary  U+24C8
    { 0, eString },  //                          U+24C9
    { 0, eString },  //                          U+24CA
    { 0, eString },  //                          U+24CB
    { 0, eString },  //                          U+24CC
    { 0, eString },  //                          U+24CD
    { 0, eString },  //                          U+24CE
    { 0, eString },  //                          U+24CF
    { 0, eString },  //                          U+24D0
    { 0, eString },  //                          U+24D1
    { 0, eString },  //                          U+24D2
    { 0, eString },  //                          U+24D3
    { 0, eString },  //                          U+24D4
    { 0, eString },  //                          U+24D5
    { 0, eString },  //                          U+24D6
    { 0, eString },  //                          U+24D7
    { 0, eString },  //                          U+24D8
    { 0, eString },  //                          U+24D9
    { 0, eString },  //                          U+24DA
    { 0, eString },  //                          U+24DB
    { 0, eString },  //                          U+24DC
    { 0, eString },  //                          U+24DD
    { 0, eString },  //                          U+24DE
    { 0, eString },  //                          U+24DF
    { 0, eString },  //                          U+24E0
    { 0, eString },  //                          U+24E1
    { 0, eString },  //                          U+24E2
    { 0, eString },  //                          U+24E3
    { 0, eString },  //                          U+24E4
    { 0, eString },  //                          U+24E5
    { 0, eString },  //                          U+24E6
    { 0, eString },  //                          U+24E7
    { 0, eString },  //                          U+24E8
    { 0, eString },  //                          U+24E9
    { 0, eString },  //                          U+24EA
    { 0, eString },  //                          U+24EB
    { 0, eString },  //                          U+24EC
    { 0, eString },  //                          U+24ED
    { 0, eString },  //                          U+24EE
    { 0, eString },  //                          U+24EF
    { 0, eString },  //                          U+24F0
    { 0, eString },  //                          U+24F1
    { 0, eString },  //                          U+24F2
    { 0, eString },  //                          U+24F3
    { 0, eString },  //                          U+24F4
    { 0, eString },  //                          U+24F5
    { 0, eString },  //                          U+24F6
    { 0, eString },  //                          U+24F7
    { 0, eString },  //                          U+24F8
    { 0, eString },  //                          U+24F9
    { 0, eString },  //                          U+24FA
    { 0, eString },  //                          U+24FB
    { 0, eString },  //                          U+24FC
    { 0, eString },  //                          U+24FD
    { 0, eString },  //                          U+24FE
    { 0, eString },  //                          U+24FF
};
static TUnicodePlan s_Plan_25h = {
    { " horizontal line ", eString },  // old dictionary                 U+2500
    { 0, eString },  //                                               U+2501
    { " vertical line", eString },  // old dictionary                    U+2502
    { 0, eString },  //                                               U+2503
    { 0, eString },  //                                               U+2504
    { 0, eString },  //                                               U+2505
    { 0, eString },  //                                               U+2506
    { 0, eString },  //                                               U+2507
    { 0, eString },  //                                               U+2508
    { 0, eString },  //                                               U+2509
    { 0, eString },  //                                               U+250A
    { 0, eString },  //                                               U+250B
    { " lower right quadrant", eString },  // old dictionary             U+250C
    { 0, eString },  //                                               U+250D
    { 0, eString },  //                                               U+250E
    { 0, eString },  //                                               U+250F
    { " lower left quadrant", eString },  // old dictionary              U+2510
    { 0, eString },  //                                               U+2511
    { 0, eString },  //                                               U+2512
    { 0, eString },  //                                               U+2513
    { " upper right quadrant", eString },  // old dictionary             U+2514
    { 0, eString },  //                                               U+2515
    { 0, eString },  //                                               U+2516
    { 0, eString },  //                                               U+2517
    { " upper left quadrant", eString },  // old dictionary              U+2518
    { 0, eString },  //                                               U+2519
    { 0, eString },  //                                               U+251A
    { 0, eString },  //                                               U+251B
    { " upper and lower right quadrants", eString },  // old dictionary  U+251C
    { 0, eString },  //                                               U+251D
    { 0, eString },  //                                               U+251E
    { 0, eString },  //                                               U+251F
    { 0, eString },  //                                               U+2520
    { 0, eString },  //                                               U+2521
    { 0, eString },  //                                               U+2522
    { 0, eString },  //                                               U+2523
    { " upper and lower left quadrants", eString },  // old dictionary   U+2524
    { 0, eString },  //                                               U+2525
    { 0, eString },  //                                               U+2526
    { 0, eString },  //                                               U+2527
    { 0, eString },  //                                               U+2528
    { 0, eString },  //                                               U+2529
    { 0, eString },  //                                               U+252A
    { 0, eString },  //                                               U+252B
    { " lower left and right quadrants", eString },  // old dictionary   U+252C
    { 0, eString },  //                                               U+252D
    { 0, eString },  //                                               U+252E
    { 0, eString },  //                                               U+252F
    { 0, eString },  //                                               U+2530
    { 0, eString },  //                                               U+2531
    { 0, eString },  //                                               U+2532
    { 0, eString },  //                                               U+2533
    { " upper left and right quadrants", eString },  // old dictionary   U+2534
    { 0, eString },  //                                               U+2535
    { 0, eString },  //                                               U+2536
    { 0, eString },  //                                               U+2537
    { 0, eString },  //                                               U+2538
    { 0, eString },  //                                               U+2539
    { 0, eString },  //                                               U+253A
    { 0, eString },  //                                               U+253B
    { " all four quadrants", eString },  // old dictionary               U+253C
    { 0, eString },  //                                               U+253D
    { 0, eString },  //                                               U+253E
    { 0, eString },  //                                               U+253F
    { 0, eString },  //                                               U+2540
    { 0, eString },  //                                               U+2541
    { 0, eString },  //                                               U+2542
    { 0, eString },  //                                               U+2543
    { 0, eString },  //                                               U+2544
    { 0, eString },  //                                               U+2545
    { 0, eString },  //                                               U+2546
    { 0, eString },  //                                               U+2547
    { 0, eString },  //                                               U+2548
    { 0, eString },  //                                               U+2549
    { 0, eString },  //                                               U+254A
    { 0, eString },  //                                               U+254B
    { 0, eString },  //                                               U+254C
    { 0, eString },  //                                               U+254D
    { 0, eString },  //                                               U+254E
    { 0, eString },  //                                               U+254F
    { " horizontal line", eString },  // old dictionary                  U+2550
    { " vertical line", eString },  // old dictionary                    U+2551
    { " lower right quadrant", eString },  // old dictionary             U+2552
    { " lower right quadrant", eString },  // old dictionary             U+2553
    { " lower right quadrant", eString },  // old dictionary             U+2554
    { " lower left quadrant", eString },  // old dictionary              U+2555
    { " lower left quadrant", eString },  // old dictionary              U+2556
    { " lower left quadrant", eString },  // old dictionary              U+2557
    { " upper right quadrant", eString },  // old dictionary             U+2558
    { " upper right quadrant", eString },  // old dictionary             U+2559
    { " upper right quadrant", eString },  // old dictionary             U+255A
    { " upper left quadrant", eString },  // old dictionary              U+255B
    { " upper left quadrant", eString },  // old dictionary              U+255C
    { " upper left quadrant", eString },  // old dictionary              U+255D
    { " upper and lower right quadrants", eString },  // old dictionary  U+255E
    { " upper and lower right quadrants", eString },  // old dictionary  U+255F
    { " upper and lower right quadrants", eString },  // old dictionary  U+2560
    { " upper and lower left quadrants", eString },  // old dictionary   U+2561
    { " upper and lower left quadrants", eString },  // old dictionary   U+2562
    { " upper and lower left quadrants", eString },  // old dictionary   U+2563
    { " lower left and right quadrants", eString },  // old dictionary   U+2564
    { " lower left and right quadrants", eString },  // old dictionary   U+2565
    { " lower left and right quadrants", eString },  // old dictionary   U+2566
    { " upper left and right quadrants", eString },  // old dictionary   U+2567
    { " upper left and right quadrants", eString },  // old dictionary   U+2568
    { " upper left and right quadrants", eString },  // old dictionary   U+2569
    { " all four quadrants", eString },  // old dictionary               U+256A
    { " all four quadrants", eString },  // old dictionary               U+256B
    { " all four quadrants", eString },  // old dictionary               U+256C
    { 0, eString },  //                                               U+256D
    { 0, eString },  //                                               U+256E
    { 0, eString },  //                                               U+256F
    { 0, eString },  //                                               U+2570
    { 0, eString },  //                                               U+2571
    { 0, eString },  //                                               U+2572
    { 0, eString },  //                                               U+2573
    { 0, eString },  //                                               U+2574
    { 0, eString },  //                                               U+2575
    { 0, eString },  //                                               U+2576
    { 0, eString },  //                                               U+2577
    { 0, eString },  //                                               U+2578
    { 0, eString },  //                                               U+2579
    { 0, eString },  //                                               U+257A
    { 0, eString },  //                                               U+257B
    { 0, eString },  //                                               U+257C
    { 0, eString },  //                                               U+257D
    { 0, eString },  //                                               U+257E
    { 0, eString },  //                                               U+257F
    { "upper half block", eString },  // old dictionary                  U+2580
    { 0, eString },  //                                               U+2581
    { 0, eString },  //                                               U+2582
    { 0, eString },  //                                               U+2583
    { "lower half block", eString },  // old dictionary                  U+2584
    { 0, eString },  //                                               U+2585
    { 0, eString },  //                                               U+2586
    { 0, eString },  //                                               U+2587
    { "full block", eString },  // old dictionary                        U+2588
    { 0, eString },  //                                               U+2589
    { 0, eString },  //                                               U+258A
    { 0, eString },  //                                               U+258B
    { 0, eString },  //                                               U+258C
    { 0, eString },  //                                               U+258D
    { 0, eString },  //                                               U+258E
    { 0, eString },  //                                               U+258F
    { 0, eString },  //                                               U+2590
    { "25% shaded block", eString },  // old dictionary                  U+2591
    { "50% shaded block", eString },  // old dictionary                  U+2592
    { "75% shaded block", eString },  // old dictionary                  U+2593
    { 0, eString },  //                                               U+2594
    { 0, eString },  //                                               U+2595
    { 0, eString },  //                                               U+2596
    { 0, eString },  //                                               U+2597
    { 0, eString },  //                                               U+2598
    { 0, eString },  //                                               U+2599
    { 0, eString },  //                                               U+259A
    { 0, eString },  //                                               U+259B
    { 0, eString },  //                                               U+259C
    { 0, eString },  //                                               U+259D
    { 0, eString },  //                                               U+259E
    { 0, eString },  //                                               U+259F
    { 0, eString },  //                                               U+25A0
    { " square", eString },  // old dictionary                           U+25A1
    { 0, eString },  //                                               U+25A2
    { 0, eString },  //                                               U+25A3
    { 0, eString },  //                                               U+25A4
    { 0, eString },  //                                               U+25A5
    { 0, eString },  //                                               U+25A6
    { 0, eString },  //                                               U+25A7
    { 0, eString },  //                                               U+25A8
    { 0, eString },  //                                               U+25A9
    { "blacksquare, square, filled", eString },  //                      U+25AA
    { 0, eString },  //                                               U+25AB
    { 0, eString },  //                                               U+25AC
    { "rectangle", eString },  // old dictionary                         U+25AD
    { "histogram marker", eString },  // old dictionary                  U+25AE
    { 0, eString },  //                                               U+25AF
    { 0, eString },  //                                               U+25B0
    { 0, eString },  //                                               U+25B1
    { 0, eString },  //                                               U+25B2
    { " big up tri, open", eString },  // old dictionary                 U+25B3
    { "black triangle", eString },  // old dictionary                    U+25B4
    { "triangle up", eString },  // old dictionary                       U+25B5
    { 0, eString },  //                                               U+25B6
    { 0, eString },  //                                               U+25B7
    { "black triangle right", eString },  // old dictionary              U+25B8
    { " triangle right", eString },  // old dictionary                   U+25B9
    { 0, eString },  //                                               U+25BA
    { 0, eString },  //                                               U+25BB
    { 0, eString },  //                                               U+25BC
    { " big dn tri, open", eString },  // old dictionary                 U+25BD
    { "black triangle down", eString },  // old dictionary               U+25BE
    { "triangle down", eString },  // old dictionary                     U+25BF
    { 0, eString },  //                                               U+25C0
    { 0, eString },  //                                               U+25C1
    { "black triangle left", eString },  // old dictionary               U+25C2
    { " triangle left", eString },  // old dictionary                    U+25C3
    { 0, eString },  //                                               U+25C4
    { 0, eString },  //                                               U+25C5
    { 0, eString },  //                                               U+25C6
    { 0, eString },  //                                               U+25C7
    { 0, eString },  //                                               U+25C8
    { 0, eString },  //                                               U+25C9
    { " lozenge", eString },  // old dictionary                          U+25CA
    { "o", eString },  // old dictionary                                 U+25CB
    { 0, eString },  //                                               U+25CC
    { 0, eString },  //                                               U+25CD
    { 0, eString },  //                                               U+25CE
    { 0, eString },  //                                               U+25CF
    { 0, eString },  //                                               U+25D0
    { 0, eString },  //                                               U+25D1
    { 0, eString },  //                                               U+25D2
    { 0, eString },  //                                               U+25D3
    { 0, eString },  //                                               U+25D4
    { 0, eString },  //                                               U+25D5
    { 0, eString },  //                                               U+25D6
    { 0, eString },  //                                               U+25D7
    { 0, eString },  //                                               U+25D8
    { 0, eString },  //                                               U+25D9
    { 0, eString },  //                                               U+25DA
    { 0, eString },  //                                               U+25DB
    { 0, eString },  //                                               U+25DC
    { 0, eString },  //                                               U+25DD
    { 0, eString },  //                                               U+25DE
    { 0, eString },  //                                               U+25DF
    { 0, eString },  //                                               U+25E0
    { 0, eString },  //                                               U+25E1
    { 0, eString },  //                                               U+25E2
    { 0, eString },  //                                               U+25E3
    { 0, eString },  //                                               U+25E4
    { 0, eString },  //                                               U+25E5
    { 0, eString },  //                                               U+25E6
    { 0, eString },  //                                               U+25E7
    { 0, eString },  //                                               U+25E8
    { 0, eString },  //                                               U+25E9
    { 0, eString },  //                                               U+25EA
    { 0, eString },  //                                               U+25EB
    { "dot in triangle", eString },  //                                  U+25EC
    { 0, eString },  //                                               U+25ED
    { 0, eString },  //                                               U+25EE
    { " large circle", eString },  // old dictionary                     U+25EF
    { 0, eString },  //                                               U+25F0
    { 0, eString },  //                                               U+25F1
    { 0, eString },  //                                               U+25F2
    { 0, eString },  //                                               U+25F3
    { 0, eString },  //                                               U+25F4
    { 0, eString },  //                                               U+25F5
    { 0, eString },  //                                               U+25F6
    { 0, eString },  //                                               U+25F7
    { 0, eString },  //                                               U+25F8
    { 0, eString },  //                                               U+25F9
    { 0, eString },  //                                               U+25FA
    { 0, eString },  //                                               U+25FB
    { 0, eString },  //                                               U+25FC
    { 0, eString },  //                                               U+25FD
    { 0, eString },  //                                               U+25FE
    { 0, eString },  //                                               U+25FF
};
static TUnicodePlan s_Plan_26h = {
    { 0, eString },  //                               U+2600
    { 0, eString },  //                               U+2601
    { 0, eString },  //                               U+2602
    { 0, eString },  //                               U+2603
    { 0, eString },  //                               U+2604
    { " bigstar", eString },  // old dictionary          U+2605
    { 0, eString },  //                               U+2606
    { 0, eString },  //                               U+2607
    { 0, eString },  //                               U+2608
    { 0, eString },  //                               U+2609
    { 0, eString },  //                               U+260A
    { 0, eString },  //                               U+260B
    { 0, eString },  //                               U+260C
    { 0, eString },  //                               U+260D
    { "telephone symbol", eString },  // old dictionary  U+260E
    { 0, eString },  //                               U+260F
    { 0, eString },  //                               U+2610
    { 0, eString },  //                               U+2611
    { 0, eString },  //                               U+2612
    { 0, eString },  //                               U+2613
    { 0, eString },  //                               U+2614
    { 0, eString },  //                               U+2615
    { 0, eString },  //                               U+2616
    { 0, eString },  //                               U+2617
    { 0, eString },  //                               U+2618
    { 0, eString },  //                               U+2619
    { 0, eString },  //                               U+261A
    { 0, eString },  //                               U+261B
    { 0, eString },  //                               U+261C
    { 0, eString },  //                               U+261D
    { 0, eString },  //                               U+261E
    { 0, eString },  //                               U+261F
    { 0, eString },  //                               U+2620
    { 0, eString },  //                               U+2621
    { 0, eString },  //                               U+2622
    { 0, eString },  //                               U+2623
    { 0, eString },  //                               U+2624
    { 0, eString },  //                               U+2625
    { 0, eString },  //                               U+2626
    { 0, eString },  //                               U+2627
    { 0, eString },  //                               U+2628
    { 0, eString },  //                               U+2629
    { 0, eString },  //                               U+262A
    { 0, eString },  //                               U+262B
    { 0, eString },  //                               U+262C
    { 0, eString },  //                               U+262D
    { 0, eString },  //                               U+262E
    { 0, eString },  //                               U+262F
    { 0, eString },  //                               U+2630
    { 0, eString },  //                               U+2631
    { 0, eString },  //                               U+2632
    { 0, eString },  //                               U+2633
    { 0, eString },  //                               U+2634
    { 0, eString },  //                               U+2635
    { 0, eString },  //                               U+2636
    { 0, eString },  //                               U+2637
    { 0, eString },  //                               U+2638
    { 0, eString },  //                               U+2639
    { 0, eString },  //                               U+263A
    { 0, eString },  //                               U+263B
    { 0, eString },  //                               U+263C
    { 0, eString },  //                               U+263D
    { 0, eString },  //                               U+263E
    { 0, eString },  //                               U+263F
    { "female symbol", eString },  // old dictionary     U+2640
    { 0, eString },  //                               U+2641
    { "male symbol", eString },  // old dictionary       U+2642
    { 0, eString },  //                               U+2643
    { 0, eString },  //                               U+2644
    { 0, eString },  //                               U+2645
    { 0, eString },  //                               U+2646
    { 0, eString },  //                               U+2647
    { 0, eString },  //                               U+2648
    { 0, eString },  //                               U+2649
    { 0, eString },  //                               U+264A
    { 0, eString },  //                               U+264B
    { 0, eString },  //                               U+264C
    { 0, eString },  //                               U+264D
    { 0, eString },  //                               U+264E
    { 0, eString },  //                               U+264F
    { 0, eString },  //                               U+2650
    { 0, eString },  //                               U+2651
    { 0, eString },  //                               U+2652
    { 0, eString },  //                               U+2653
    { 0, eString },  //                               U+2654
    { 0, eString },  //                               U+2655
    { 0, eString },  //                               U+2656
    { 0, eString },  //                               U+2657
    { 0, eString },  //                               U+2658
    { 0, eString },  //                               U+2659
    { 0, eString },  //                               U+265A
    { 0, eString },  //                               U+265B
    { 0, eString },  //                               U+265C
    { 0, eString },  //                               U+265D
    { 0, eString },  //                               U+265E
    { 0, eString },  //                               U+265F
    { "spades", eString },  // old dictionary            U+2660
    { "heart", eString },  // old dictionary             U+2661
    { 0, eString },  //                               U+2662
    { "club suit symbol", eString },  // old dictionary  U+2663
    { 0, eString },  //                               U+2664
    { 0, eString },  //                               U+2665
    { "diamond", eString },  // old dictionary           U+2666
    { 0, eString },  //                               U+2667
    { 0, eString },  //                               U+2668
    { 0, eString },  //                               U+2669
    { "music note", eString },  // old dictionary        U+266A
    { 0, eString },  //                               U+266B
    { 0, eString },  //                               U+266C
    { "musical flat", eString },  // old dictionary      U+266D
    { " music natural", eString },  // old dictionary    U+266E
    { "musical sharp", eString },  // old dictionary     U+266F
    { 0, eString },  //                               U+2670
    { 0, eString },  //                               U+2671
    { 0, eString },  //                               U+2672
    { 0, eString },  //                               U+2673
    { 0, eString },  //                               U+2674
    { 0, eString },  //                               U+2675
    { 0, eString },  //                               U+2676
    { 0, eString },  //                               U+2677
    { 0, eString },  //                               U+2678
    { 0, eString },  //                               U+2679
    { 0, eString },  //                               U+267A
    { 0, eString },  //                               U+267B
    { 0, eString },  //                               U+267C
    { 0, eString },  //                               U+267D
    { 0, eString },  //                               U+267E
    { 0, eString },  //                               U+267F
    { 0, eString },  //                               U+2680
    { 0, eString },  //                               U+2681
    { 0, eString },  //                               U+2682
    { 0, eString },  //                               U+2683
    { 0, eString },  //                               U+2684
    { 0, eString },  //                               U+2685
    { 0, eString },  //                               U+2686
    { 0, eString },  //                               U+2687
    { 0, eString },  //                               U+2688
    { 0, eString },  //                               U+2689
    { 0, eString },  //                               U+268A
    { 0, eString },  //                               U+268B
    { 0, eString },  //                               U+268C
    { 0, eString },  //                               U+268D
    { 0, eString },  //                               U+268E
    { 0, eString },  //                               U+268F
    { 0, eString },  //                               U+2690
    { 0, eString },  //                               U+2691
    { 0, eString },  //                               U+2692
    { 0, eString },  //                               U+2693
    { 0, eString },  //                               U+2694
    { 0, eString },  //                               U+2695
    { 0, eString },  //                               U+2696
    { 0, eString },  //                               U+2697
    { 0, eString },  //                               U+2698
    { 0, eString },  //                               U+2699
    { 0, eString },  //                               U+269A
    { 0, eString },  //                               U+269B
    { 0, eString },  //                               U+269C
    { 0, eString },  //                               U+269D
    { 0, eString },  //                               U+269E
    { 0, eString },  //                               U+269F
    { 0, eString },  //                               U+26A0
    { 0, eString },  //                               U+26A1
    { 0, eString },  //                               U+26A2
    { 0, eString },  //                               U+26A3
    { 0, eString },  //                               U+26A4
    { 0, eString },  //                               U+26A5
    { 0, eString },  //                               U+26A6
    { 0, eString },  //                               U+26A7
    { 0, eString },  //                               U+26A8
    { 0, eString },  //                               U+26A9
    { 0, eString },  //                               U+26AA
    { 0, eString },  //                               U+26AB
    { 0, eString },  //                               U+26AC
    { 0, eString },  //                               U+26AD
    { 0, eString },  //                               U+26AE
    { 0, eString },  //                               U+26AF
    { 0, eString },  //                               U+26B0
    { 0, eString },  //                               U+26B1
    { 0, eString },  //                               U+26B2
    { 0, eString },  //                               U+26B3
    { 0, eString },  //                               U+26B4
    { 0, eString },  //                               U+26B5
    { 0, eString },  //                               U+26B6
    { 0, eString },  //                               U+26B7
    { 0, eString },  //                               U+26B8
    { 0, eString },  //                               U+26B9
    { 0, eString },  //                               U+26BA
    { 0, eString },  //                               U+26BB
    { 0, eString },  //                               U+26BC
    { 0, eString },  //                               U+26BD
    { 0, eString },  //                               U+26BE
    { 0, eString },  //                               U+26BF
    { 0, eString },  //                               U+26C0
    { 0, eString },  //                               U+26C1
    { 0, eString },  //                               U+26C2
    { 0, eString },  //                               U+26C3
    { 0, eString },  //                               U+26C4
    { 0, eString },  //                               U+26C5
    { 0, eString },  //                               U+26C6
    { 0, eString },  //                               U+26C7
    { 0, eString },  //                               U+26C8
    { 0, eString },  //                               U+26C9
    { 0, eString },  //                               U+26CA
    { 0, eString },  //                               U+26CB
    { 0, eString },  //                               U+26CC
    { 0, eString },  //                               U+26CD
    { 0, eString },  //                               U+26CE
    { 0, eString },  //                               U+26CF
    { 0, eString },  //                               U+26D0
    { 0, eString },  //                               U+26D1
    { 0, eString },  //                               U+26D2
    { 0, eString },  //                               U+26D3
    { 0, eString },  //                               U+26D4
    { 0, eString },  //                               U+26D5
    { 0, eString },  //                               U+26D6
    { 0, eString },  //                               U+26D7
    { 0, eString },  //                               U+26D8
    { 0, eString },  //                               U+26D9
    { 0, eString },  //                               U+26DA
    { 0, eString },  //                               U+26DB
    { 0, eString },  //                               U+26DC
    { 0, eString },  //                               U+26DD
    { 0, eString },  //                               U+26DE
    { 0, eString },  //                               U+26DF
    { 0, eString },  //                               U+26E0
    { 0, eString },  //                               U+26E1
    { 0, eString },  //                               U+26E2
    { 0, eString },  //                               U+26E3
    { 0, eString },  //                               U+26E4
    { 0, eString },  //                               U+26E5
    { 0, eString },  //                               U+26E6
    { 0, eString },  //                               U+26E7
    { 0, eString },  //                               U+26E8
    { 0, eString },  //                               U+26E9
    { 0, eString },  //                               U+26EA
    { 0, eString },  //                               U+26EB
    { 0, eString },  //                               U+26EC
    { 0, eString },  //                               U+26ED
    { 0, eString },  //                               U+26EE
    { 0, eString },  //                               U+26EF
    { 0, eString },  //                               U+26F0
    { 0, eString },  //                               U+26F1
    { 0, eString },  //                               U+26F2
    { 0, eString },  //                               U+26F3
    { 0, eString },  //                               U+26F4
    { 0, eString },  //                               U+26F5
    { 0, eString },  //                               U+26F6
    { 0, eString },  //                               U+26F7
    { 0, eString },  //                               U+26F8
    { 0, eString },  //                               U+26F9
    { 0, eString },  //                               U+26FA
    { 0, eString },  //                               U+26FB
    { 0, eString },  //                               U+26FC
    { 0, eString },  //                               U+26FD
    { 0, eString },  //                               U+26FE
    { 0, eString },  //                               U+26FF
};
static TUnicodePlan s_Plan_27h = {
    { 0, eString },  //                            U+2700
    { 0, eString },  //                            U+2701
    { 0, eString },  //                            U+2702
    { 0, eString },  //                            U+2703
    { 0, eString },  //                            U+2704
    { 0, eString },  //                            U+2705
    { 0, eString },  //                            U+2706
    { 0, eString },  //                            U+2707
    { 0, eString },  //                            U+2708
    { 0, eString },  //                            U+2709
    { 0, eString },  //                            U+270A
    { 0, eString },  //                            U+270B
    { 0, eString },  //                            U+270C
    { 0, eString },  //                            U+270D
    { 0, eString },  //                            U+270E
    { 0, eString },  //                            U+270F
    { 0, eString },  //                            U+2710
    { 0, eString },  //                            U+2711
    { 0, eString },  //                            U+2712
    { "check mark", eString },  // old dictionary     U+2713
    { 0, eString },  //                            U+2714
    { 0, eString },  //                            U+2715
    { 0, eString },  //                            U+2716
    { "ballot cross", eString },  // old dictionary   U+2717
    { 0, eString },  //                            U+2718
    { 0, eString },  //                            U+2719
    { 0, eString },  //                            U+271A
    { 0, eString },  //                            U+271B
    { 0, eString },  //                            U+271C
    { 0, eString },  //                            U+271D
    { 0, eString },  //                            U+271E
    { 0, eString },  //                            U+271F
    { "maltese cross", eString },  // old dictionary  U+2720
    { 0, eString },  //                            U+2721
    { 0, eString },  //                            U+2722
    { 0, eString },  //                            U+2723
    { 0, eString },  //                            U+2724
    { 0, eString },  //                            U+2725
    { 0, eString },  //                            U+2726
    { 0, eString },  //                            U+2727
    { 0, eString },  //                            U+2728
    { 0, eString },  //                            U+2729
    { 0, eString },  //                            U+272A
    { 0, eString },  //                            U+272B
    { 0, eString },  //                            U+272C
    { 0, eString },  //                            U+272D
    { 0, eString },  //                            U+272E
    { 0, eString },  //                            U+272F
    { 0, eString },  //                            U+2730
    { 0, eString },  //                            U+2731
    { 0, eString },  //                            U+2732
    { 0, eString },  //                            U+2733
    { 0, eString },  //                            U+2734
    { 0, eString },  //                            U+2735
    { "sextile", eString },  // old dictionary        U+2736
    { 0, eString },  //                            U+2737
    { 0, eString },  //                            U+2738
    { 0, eString },  //                            U+2739
    { 0, eString },  //                            U+273A
    { 0, eString },  //                            U+273B
    { 0, eString },  //                            U+273C
    { 0, eString },  //                            U+273D
    { 0, eString },  //                            U+273E
    { 0, eString },  //                            U+273F
    { 0, eString },  //                            U+2740
    { 0, eString },  //                            U+2741
    { 0, eString },  //                            U+2742
    { 0, eString },  //                            U+2743
    { 0, eString },  //                            U+2744
    { 0, eString },  //                            U+2745
    { 0, eString },  //                            U+2746
    { 0, eString },  //                            U+2747
    { 0, eString },  //                            U+2748
    { 0, eString },  //                            U+2749
    { 0, eString },  //                            U+274A
    { 0, eString },  //                            U+274B
    { 0, eString },  //                            U+274C
    { 0, eString },  //                            U+274D
    { 0, eString },  //                            U+274E
    { 0, eString },  //                            U+274F
    { 0, eString },  //                            U+2750
    { 0, eString },  //                            U+2751
    { 0, eString },  //                            U+2752
    { 0, eString },  //                            U+2753
    { 0, eString },  //                            U+2754
    { 0, eString },  //                            U+2755
    { 0, eString },  //                            U+2756
    { 0, eString },  //                            U+2757
    { 0, eString },  //                            U+2758
    { 0, eString },  //                            U+2759
    { 0, eString },  //                            U+275A
    { 0, eString },  //                            U+275B
    { 0, eString },  //                            U+275C
    { 0, eString },  //                            U+275D
    { 0, eString },  //                            U+275E
    { 0, eString },  //                            U+275F
    { 0, eString },  //                            U+2760
    { 0, eString },  //                            U+2761
    { 0, eString },  //                            U+2762
    { 0, eString },  //                            U+2763
    { 0, eString },  //                            U+2764
    { 0, eString },  //                            U+2765
    { 0, eString },  //                            U+2766
    { 0, eString },  //                            U+2767
    { 0, eString },  //                            U+2768
    { 0, eString },  //                            U+2769
    { 0, eString },  //                            U+276A
    { 0, eString },  //                            U+276B
    { 0, eString },  //                            U+276C
    { 0, eString },  //                            U+276D
    { 0, eString },  //                            U+276E
    { 0, eString },  //                            U+276F
    { 0, eString },  //                            U+2770
    { 0, eString },  //                            U+2771
    { 0, eString },  //                            U+2772
    { 0, eString },  //                            U+2773
    { 0, eString },  //                            U+2774
    { 0, eString },  //                            U+2775
    { 0, eString },  //                            U+2776
    { 0, eString },  //                            U+2777
    { 0, eString },  //                            U+2778
    { 0, eString },  //                            U+2779
    { 0, eString },  //                            U+277A
    { 0, eString },  //                            U+277B
    { 0, eString },  //                            U+277C
    { 0, eString },  //                            U+277D
    { 0, eString },  //                            U+277E
    { 0, eString },  //                            U+277F
    { 0, eString },  //                            U+2780
    { 0, eString },  //                            U+2781
    { 0, eString },  //                            U+2782
    { 0, eString },  //                            U+2783
    { 0, eString },  //                            U+2784
    { 0, eString },  //                            U+2785
    { 0, eString },  //                            U+2786
    { 0, eString },  //                            U+2787
    { 0, eString },  //                            U+2788
    { 0, eString },  //                            U+2789
    { 0, eString },  //                            U+278A
    { 0, eString },  //                            U+278B
    { 0, eString },  //                            U+278C
    { 0, eString },  //                            U+278D
    { 0, eString },  //                            U+278E
    { 0, eString },  //                            U+278F
    { 0, eString },  //                            U+2790
    { 0, eString },  //                            U+2791
    { 0, eString },  //                            U+2792
    { 0, eString },  //                            U+2793
    { 0, eString },  //                            U+2794
    { 0, eString },  //                            U+2795
    { 0, eString },  //                            U+2796
    { 0, eString },  //                            U+2797
    { 0, eString },  //                            U+2798
    { 0, eString },  //                            U+2799
    { 0, eString },  //                            U+279A
    { 0, eString },  //                            U+279B
    { 0, eString },  //                            U+279C
    { 0, eString },  //                            U+279D
    { 0, eString },  //                            U+279E
    { 0, eString },  //                            U+279F
    { 0, eString },  //                            U+27A0
    { 0, eString },  //                            U+27A1
    { 0, eString },  //                            U+27A2
    { 0, eString },  //                            U+27A3
    { 0, eString },  //                            U+27A4
    { 0, eString },  //                            U+27A5
    { 0, eString },  //                            U+27A6
    { 0, eString },  //                            U+27A7
    { 0, eString },  //                            U+27A8
    { 0, eString },  //                            U+27A9
    { 0, eString },  //                            U+27AA
    { 0, eString },  //                            U+27AB
    { 0, eString },  //                            U+27AC
    { 0, eString },  //                            U+27AD
    { 0, eString },  //                            U+27AE
    { 0, eString },  //                            U+27AF
    { 0, eString },  //                            U+27B0
    { 0, eString },  //                            U+27B1
    { 0, eString },  //                            U+27B2
    { 0, eString },  //                            U+27B3
    { 0, eString },  //                            U+27B4
    { 0, eString },  //                            U+27B5
    { 0, eString },  //                            U+27B6
    { 0, eString },  //                            U+27B7
    { 0, eString },  //                            U+27B8
    { 0, eString },  //                            U+27B9
    { 0, eString },  //                            U+27BA
    { 0, eString },  //                            U+27BB
    { 0, eString },  //                            U+27BC
    { 0, eString },  //                            U+27BD
    { 0, eString },  //                            U+27BE
    { 0, eString },  //                            U+27BF
    { 0, eString },  //                            U+27C0
    { 0, eString },  //                            U+27C1
    { 0, eString },  //                            U+27C2
    { 0, eString },  //                            U+27C3
    { 0, eString },  //                            U+27C4
    { 0, eString },  //                            U+27C5
    { 0, eString },  //                            U+27C6
    { 0, eString },  //                            U+27C7
    { 0, eString },  //                            U+27C8
    { 0, eString },  //                            U+27C9
    { 0, eString },  //                            U+27CA
    { 0, eString },  //                            U+27CB
    { 0, eString },  //                            U+27CC
    { 0, eString },  //                            U+27CD
    { 0, eString },  //                            U+27CE
    { 0, eString },  //                            U+27CF
    { 0, eString },  //                            U+27D0
    { 0, eString },  //                            U+27D1
    { 0, eString },  //                            U+27D2
    { 0, eString },  //                            U+27D3
    { 0, eString },  //                            U+27D4
    { 0, eString },  //                            U+27D5
    { 0, eString },  //                            U+27D6
    { 0, eString },  //                            U+27D7
    { 0, eString },  //                            U+27D8
    { 0, eString },  //                            U+27D9
    { 0, eString },  //                            U+27DA
    { 0, eString },  //                            U+27DB
    { 0, eString },  //                            U+27DC
    { 0, eString },  //                            U+27DD
    { 0, eString },  //                            U+27DE
    { 0, eString },  //                            U+27DF
    { 0, eString },  //                            U+27E0
    { 0, eString },  //                            U+27E1
    { 0, eString },  //                            U+27E2
    { 0, eString },  //                            U+27E3
    { 0, eString },  //                            U+27E4
    { 0, eString },  //                            U+27E5
    { 0, eString },  //                            U+27E6
    { 0, eString },  //                            U+27E7
    { 0, eString },  //                            U+27E8
    { 0, eString },  //                            U+27E9
    { 0, eString },  //                            U+27EA
    { 0, eString },  //                            U+27EB
    { 0, eString },  //                            U+27EC
    { 0, eString },  //                            U+27ED
    { 0, eString },  //                            U+27EE
    { 0, eString },  //                            U+27EF
    { 0, eString },  //                            U+27F0
    { 0, eString },  //                            U+27F1
    { 0, eString },  //                            U+27F2
    { 0, eString },  //                            U+27F3
    { 0, eString },  //                            U+27F4
    { 0, eString },  //                            U+27F5
    { 0, eString },  //                            U+27F6
    { 0, eString },  //                            U+27F7
    { 0, eString },  //                            U+27F8
    { 0, eString },  //                            U+27F9
    { 0, eString },  //                            U+27FA
    { 0, eString },  //                            U+27FB
    { 0, eString },  //                            U+27FC
    { 0, eString },  //                            U+27FD
    { 0, eString },  //                            U+27FE
    { 0, eString },  //                            U+27FF
};
static TUnicodePlan s_Plan_30h = {
    { 0, eString },  //                          U+3000
    { 0, eString },  //                          U+3001
    { 0, eString },  //                          U+3002
    { 0, eString },  //                          U+3003
    { 0, eString },  //                          U+3004
    { 0, eString },  //                          U+3005
    { 0, eString },  //                          U+3006
    { 0, eString },  //                          U+3007
    { "<", eString },  // old dictionary            U+3008
    { ">", eString },  // old dictionary            U+3009
    { "left angle bracket, double", eString },  //  U+300A
    { "right angle bracket, double", eString },  // U+300B
    { 0, eString },  //                          U+300C
    { 0, eString },  //                          U+300D
    { 0, eString },  //                          U+300E
    { 0, eString },  //                          U+300F
    { 0, eString },  //                          U+3010
    { 0, eString },  //                          U+3011
    { 0, eString },  //                          U+3012
    { 0, eString },  //                          U+3013
    { "left broken bracket", eString },  //         U+3014
    { "right broken bracket", eString },  //        U+3015
    { 0, eString },  //                          U+3016
    { 0, eString },  //                          U+3017
    { "left open angular bracket", eString },  //   U+3018
    { "right open angular bracket", eString },  //  U+3019
    { "left open bracket", eString },  //           U+301A
    { "right open bracket", eString },  //          U+301B
    { 0, eString },  //                          U+301C
    { 0, eString },  //                          U+301D
    { 0, eString },  //                          U+301E
    { 0, eString },  //                          U+301F
    { 0, eString },  //                          U+3020
    { 0, eString },  //                          U+3021
    { 0, eString },  //                          U+3022
    { 0, eString },  //                          U+3023
    { 0, eString },  //                          U+3024
    { 0, eString },  //                          U+3025
    { 0, eString },  //                          U+3026
    { 0, eString },  //                          U+3027
    { 0, eString },  //                          U+3028
    { 0, eString },  //                          U+3029
    { 0, eString },  //                          U+302A
    { 0, eString },  //                          U+302B
    { 0, eString },  //                          U+302C
    { 0, eString },  //                          U+302D
    { 0, eString },  //                          U+302E
    { 0, eString },  //                          U+302F
    { 0, eString },  //                          U+3030
    { 0, eString },  //                          U+3031
    { 0, eString },  //                          U+3032
    { 0, eString },  //                          U+3033
    { 0, eString },  //                          U+3034
    { 0, eString },  //                          U+3035
    { 0, eString },  //                          U+3036
    { 0, eString },  //                          U+3037
    { 0, eString },  //                          U+3038
    { 0, eString },  //                          U+3039
    { 0, eString },  //                          U+303A
    { 0, eString },  //                          U+303B
    { 0, eString },  //                          U+303C
    { 0, eString },  //                          U+303D
    { 0, eString },  //                          U+303E
    { 0, eString },  //                          U+303F
    { 0, eString },  //                          U+3040
    { 0, eString },  //                          U+3041
    { 0, eString },  //                          U+3042
    { 0, eString },  //                          U+3043
    { 0, eString },  //                          U+3044
    { 0, eString },  //                          U+3045
    { 0, eString },  //                          U+3046
    { 0, eString },  //                          U+3047
    { 0, eString },  //                          U+3048
    { 0, eString },  //                          U+3049
    { 0, eString },  //                          U+304A
    { 0, eString },  //                          U+304B
    { 0, eString },  //                          U+304C
    { 0, eString },  //                          U+304D
    { 0, eString },  //                          U+304E
    { 0, eString },  //                          U+304F
    { 0, eString },  //                          U+3050
    { 0, eString },  //                          U+3051
    { 0, eString },  //                          U+3052
    { 0, eString },  //                          U+3053
    { 0, eString },  //                          U+3054
    { 0, eString },  //                          U+3055
    { 0, eString },  //                          U+3056
    { 0, eString },  //                          U+3057
    { 0, eString },  //                          U+3058
    { 0, eString },  //                          U+3059
    { 0, eString },  //                          U+305A
    { 0, eString },  //                          U+305B
    { 0, eString },  //                          U+305C
    { 0, eString },  //                          U+305D
    { 0, eString },  //                          U+305E
    { 0, eString },  //                          U+305F
    { 0, eString },  //                          U+3060
    { 0, eString },  //                          U+3061
    { 0, eString },  //                          U+3062
    { 0, eString },  //                          U+3063
    { 0, eString },  //                          U+3064
    { 0, eString },  //                          U+3065
    { 0, eString },  //                          U+3066
    { 0, eString },  //                          U+3067
    { 0, eString },  //                          U+3068
    { 0, eString },  //                          U+3069
    { 0, eString },  //                          U+306A
    { 0, eString },  //                          U+306B
    { 0, eString },  //                          U+306C
    { 0, eString },  //                          U+306D
    { 0, eString },  //                          U+306E
    { 0, eString },  //                          U+306F
    { 0, eString },  //                          U+3070
    { 0, eString },  //                          U+3071
    { 0, eString },  //                          U+3072
    { 0, eString },  //                          U+3073
    { 0, eString },  //                          U+3074
    { 0, eString },  //                          U+3075
    { 0, eString },  //                          U+3076
    { 0, eString },  //                          U+3077
    { 0, eString },  //                          U+3078
    { 0, eString },  //                          U+3079
    { 0, eString },  //                          U+307A
    { 0, eString },  //                          U+307B
    { 0, eString },  //                          U+307C
    { 0, eString },  //                          U+307D
    { 0, eString },  //                          U+307E
    { 0, eString },  //                          U+307F
    { 0, eString },  //                          U+3080
    { 0, eString },  //                          U+3081
    { 0, eString },  //                          U+3082
    { 0, eString },  //                          U+3083
    { 0, eString },  //                          U+3084
    { 0, eString },  //                          U+3085
    { 0, eString },  //                          U+3086
    { 0, eString },  //                          U+3087
    { 0, eString },  //                          U+3088
    { 0, eString },  //                          U+3089
    { 0, eString },  //                          U+308A
    { 0, eString },  //                          U+308B
    { 0, eString },  //                          U+308C
    { 0, eString },  //                          U+308D
    { 0, eString },  //                          U+308E
    { 0, eString },  //                          U+308F
    { 0, eString },  //                          U+3090
    { 0, eString },  //                          U+3091
    { 0, eString },  //                          U+3092
    { 0, eString },  //                          U+3093
    { 0, eString },  //                          U+3094
    { 0, eString },  //                          U+3095
    { 0, eString },  //                          U+3096
    { 0, eString },  //                          U+3097
    { 0, eString },  //                          U+3098
    { 0, eString },  //                          U+3099
    { 0, eString },  //                          U+309A
    { 0, eString },  //                          U+309B
    { 0, eString },  //                          U+309C
    { 0, eString },  //                          U+309D
    { 0, eString },  //                          U+309E
    { 0, eString },  //                          U+309F
    { 0, eString },  //                          U+30A0
    { 0, eString },  //                          U+30A1
    { 0, eString },  //                          U+30A2
    { 0, eString },  //                          U+30A3
    { 0, eString },  //                          U+30A4
    { 0, eString },  //                          U+30A5
    { 0, eString },  //                          U+30A6
    { 0, eString },  //                          U+30A7
    { 0, eString },  //                          U+30A8
    { 0, eString },  //                          U+30A9
    { 0, eString },  //                          U+30AA
    { 0, eString },  //                          U+30AB
    { 0, eString },  //                          U+30AC
    { 0, eString },  //                          U+30AD
    { 0, eString },  //                          U+30AE
    { 0, eString },  //                          U+30AF
    { 0, eString },  //                          U+30B0
    { 0, eString },  //                          U+30B1
    { 0, eString },  //                          U+30B2
    { 0, eString },  //                          U+30B3
    { 0, eString },  //                          U+30B4
    { 0, eString },  //                          U+30B5
    { 0, eString },  //                          U+30B6
    { 0, eString },  //                          U+30B7
    { 0, eString },  //                          U+30B8
    { 0, eString },  //                          U+30B9
    { 0, eString },  //                          U+30BA
    { 0, eString },  //                          U+30BB
    { 0, eString },  //                          U+30BC
    { 0, eString },  //                          U+30BD
    { 0, eString },  //                          U+30BE
    { 0, eString },  //                          U+30BF
    { 0, eString },  //                          U+30C0
    { 0, eString },  //                          U+30C1
    { 0, eString },  //                          U+30C2
    { 0, eString },  //                          U+30C3
    { 0, eString },  //                          U+30C4
    { 0, eString },  //                          U+30C5
    { 0, eString },  //                          U+30C6
    { 0, eString },  //                          U+30C7
    { 0, eString },  //                          U+30C8
    { 0, eString },  //                          U+30C9
    { 0, eString },  //                          U+30CA
    { 0, eString },  //                          U+30CB
    { 0, eString },  //                          U+30CC
    { 0, eString },  //                          U+30CD
    { 0, eString },  //                          U+30CE
    { 0, eString },  //                          U+30CF
    { 0, eString },  //                          U+30D0
    { 0, eString },  //                          U+30D1
    { 0, eString },  //                          U+30D2
    { 0, eString },  //                          U+30D3
    { 0, eString },  //                          U+30D4
    { 0, eString },  //                          U+30D5
    { 0, eString },  //                          U+30D6
    { 0, eString },  //                          U+30D7
    { 0, eString },  //                          U+30D8
    { 0, eString },  //                          U+30D9
    { 0, eString },  //                          U+30DA
    { 0, eString },  //                          U+30DB
    { 0, eString },  //                          U+30DC
    { 0, eString },  //                          U+30DD
    { 0, eString },  //                          U+30DE
    { 0, eString },  //                          U+30DF
    { 0, eString },  //                          U+30E0
    { 0, eString },  //                          U+30E1
    { 0, eString },  //                          U+30E2
    { 0, eString },  //                          U+30E3
    { 0, eString },  //                          U+30E4
    { 0, eString },  //                          U+30E5
    { 0, eString },  //                          U+30E6
    { 0, eString },  //                          U+30E7
    { 0, eString },  //                          U+30E8
    { 0, eString },  //                          U+30E9
    { 0, eString },  //                          U+30EA
    { 0, eString },  //                          U+30EB
    { 0, eString },  //                          U+30EC
    { 0, eString },  //                          U+30ED
    { 0, eString },  //                          U+30EE
    { 0, eString },  //                          U+30EF
    { 0, eString },  //                          U+30F0
    { 0, eString },  //                          U+30F1
    { 0, eString },  //                          U+30F2
    { 0, eString },  //                          U+30F3
    { 0, eString },  //                          U+30F4
    { 0, eString },  //                          U+30F5
    { 0, eString },  //                          U+30F6
    { 0, eString },  //                          U+30F7
    { 0, eString },  //                          U+30F8
    { 0, eString },  //                          U+30F9
    { 0, eString },  //                          U+30FA
    { 0, eString },  //                          U+30FB
    { 0, eString },  //                          U+30FC
    { 0, eString },  //                          U+30FD
    { 0, eString },  //                          U+30FE
    { 0, eString },  //                          U+30FF
};
static TUnicodePlan s_Plan_E0h = {
    { "slashed integral", eString },  //       U+E000
    { "square with times sign", eString },  // U+E001
    { "square", eString },  //                 U+E002
    { "square", eString },  //                 U+E003
    { "square", eString },  //                 U+E004
    { "white square", eString },  //           U+E005
    { "white square", eString },  //           U+E006
    { "white square", eString },  //           U+E007
    { "white square", eString },  //           U+E008
    { "striped box", eString },  //            U+E009
    { "filled semicircle", eString },  //      U+E00A
    { "3 asterisks", eString },  //            U+E00B
    { "circle", eString },  //                 U+E00C
    { "up arrow", eString },  //               U+E00D
    { "slashed line", eString },  //           U+E00E
    { "slashed line", eString },  //           U+E00F
    { "striped box", eString },  //            U+E010
    { "double ended arrow", eString },  //     U+E011
    { "^^^", eString },  //                    U+E012
    { "left arrowhead", eString },  //         U+E013
    { "down arrowhead", eString },  //         U+E014
    { "left arrowhead", eString },  //         U+E015
    { "up arrowhead", eString },  //           U+E016
    { "down arrow", eString },  //             U+E017
    { "bond", eString },  //                   U+E018
    { "double bond", eString },  //            U+E019
    { "triple bond", eString },  //            U+E01A
    { "==", eString },  //                     U+E01B
    { "", eString },  //                       U+E01C
    { "bardot", eString },  //                 U+E01D
    { "diamond with dot", eString },  //       U+E01E
    { "hourglass", eString },  //              U+E01F
    { "slashed lambda", eString },  //         U+E020
    { "white octagon", eString },  //          U+E021
    { "octagon", eString },  //                U+E022
    { "circle", eString },  //                 U+E023
    { "upper semicircle", eString },  //       U+E024
    { "bottom semicircle", eString },  //      U+E025
    { "box with dots", eString },  //          U+E026
    { "box with crosscheck", eString },  //    U+E027
    { "striped box", eString },  //            U+E028
    { "triangle with dots", eString },  //     U+E029
    { "striped box", eString },  //            U+E02A
    { "box with dots", eString },  //          U+E02B
    { "box with dots", eString },  //          U+E02C
    { "gray box", eString },  //               U+E02D
    { "striped box", eString },  //            U+E02E
    { "striped box", eString },  //            U+E02F
    { "striped box", eString },  //            U+E030
    { "circle with dots", eString },  //       U+E031
    { "striped circle", eString },  //         U+E032
    { "bold circle", eString },  //            U+E033
    { "striped box", eString },  //            U+E034
    { "", eString },  //                       U+E035
    { "1 in circle", eString },  //            U+E036
    { "2 in circle", eString },  //            U+E037
    { "3 in circle", eString },  //            U+E038
    { "4 in circle", eString },  //            U+E039
    { "5 in circle", eString },  //            U+E03A
    { "heavy cross", eString },  //            U+E03B
    { "6-point star", eString },  //           U+E03C
    { "5-point star", eString },  //           U+E03D
    { "5-point star", eString },  //           U+E03E
    { "right arrow", eString },  //            U+E03F
    { "right arrowhead", eString },  //        U+E040
    { "heavy x", eString },  //                U+E041
    { "6-point star", eString },  //           U+E042
    { "*", eString },  //                      U+E043
    { "*", eString },  //                      U+E044
    { "right arrowhead", eString },  //        U+E045
    { "cross", eString },  //                  U+E046
    { "*", eString },  //                      U+E047
    { "right arrow", eString },  //            U+E048
    { "right arrow", eString },  //            U+E049
    { "right arrow", eString },  //            U+E04A
    { "right arrow", eString },  //            U+E04B
    { "pentagon", eString },  //               U+E04C
    { "plank's constant", eString },  //       U+E04D
    { "", eString },  //                       U+E04E
    { "chempt", eString },  //                 U+E04F
    { "", eString },  //                       U+E050
    { "circle", eString },  //                 U+E051
    { 0, eString },  //                     U+E052
    { 0, eString },  //                     U+E053
    { "", eString },  //                       U+E054
    { "", eString },  //                       U+E055
    { "", eString },  //                       U+E056
    { 0, eString },  //                     U+E057
    { 0, eString },  //                     U+E058
    { "", eString },  //                       U+E059
    { "", eString },  //                       U+E05A
    { "", eString },  //                       U+E05B
    { "vector", eString },  //                 U+E05C
    { "h", eString },  //                      U+E05D
    { "..", eString },  //                     U+E05E
    { "k", eString },  //                      U+E05F
    { 0, eString },  //                     U+E060
    { 0, eString },  //                     U+E061
    { 0, eString },  //                     U+E062
    { "outline cross", eString },  //          U+E063
    { 0, eString },  //                     U+E064
    { "up arrowhead", eString },  //           U+E065
    { "solid bar", eString },  //              U+E066
    { "scissor", eString },  //                U+E067
    { "triangle", eString },  //               U+E068
    { "diamond", eString },  //                U+E069
    { "circle", eString },  //                 U+E06A
    { "diamond", eString },  //                U+E06B
    { "L", eString },  //                      U+E06C
    { 0, eString },  //                     U+E06D
    { 0, eString },  //                     U+E06E
    { 0, eString },  //                     U+E06F
    { 0, eString },  //                     U+E070
    { 0, eString },  //                     U+E071
    { 0, eString },  //                     U+E072
    { 0, eString },  //                     U+E073
    { 0, eString },  //                     U+E074
    { 0, eString },  //                     U+E075
    { 0, eString },  //                     U+E076
    { 0, eString },  //                     U+E077
    { 0, eString },  //                     U+E078
    { 0, eString },  //                     U+E079
    { 0, eString },  //                     U+E07A
    { 0, eString },  //                     U+E07B
    { 0, eString },  //                     U+E07C
    { 0, eString },  //                     U+E07D
    { 0, eString },  //                     U+E07E
    { 0, eString },  //                     U+E07F
    { 0, eString },  //                     U+E080
    { 0, eString },  //                     U+E081
    { 0, eString },  //                     U+E082
    { 0, eString },  //                     U+E083
    { 0, eString },  //                     U+E084
    { 0, eString },  //                     U+E085
    { 0, eString },  //                     U+E086
    { 0, eString },  //                     U+E087
    { 0, eString },  //                     U+E088
    { 0, eString },  //                     U+E089
    { 0, eString },  //                     U+E08A
    { 0, eString },  //                     U+E08B
    { 0, eString },  //                     U+E08C
    { 0, eString },  //                     U+E08D
    { 0, eString },  //                     U+E08E
    { 0, eString },  //                     U+E08F
    { 0, eString },  //                     U+E090
    { 0, eString },  //                     U+E091
    { 0, eString },  //                     U+E092
    { 0, eString },  //                     U+E093
    { 0, eString },  //                     U+E094
    { 0, eString },  //                     U+E095
    { 0, eString },  //                     U+E096
    { 0, eString },  //                     U+E097
    { 0, eString },  //                     U+E098
    { 0, eString },  //                     U+E099
    { 0, eString },  //                     U+E09A
    { 0, eString },  //                     U+E09B
    { 0, eString },  //                     U+E09C
    { 0, eString },  //                     U+E09D
    { 0, eString },  //                     U+E09E
    { 0, eString },  //                     U+E09F
    { 0, eString },  //                     U+E0A0
    { 0, eString },  //                     U+E0A1
    { 0, eString },  //                     U+E0A2
    { 0, eString },  //                     U+E0A3
    { 0, eString },  //                     U+E0A4
    { 0, eString },  //                     U+E0A5
    { 0, eString },  //                     U+E0A6
    { 0, eString },  //                     U+E0A7
    { 0, eString },  //                     U+E0A8
    { 0, eString },  //                     U+E0A9
    { 0, eString },  //                     U+E0AA
    { 0, eString },  //                     U+E0AB
    { 0, eString },  //                     U+E0AC
    { 0, eString },  //                     U+E0AD
    { 0, eString },  //                     U+E0AE
    { 0, eString },  //                     U+E0AF
    { 0, eString },  //                     U+E0B0
    { 0, eString },  //                     U+E0B1
    { 0, eString },  //                     U+E0B2
    { 0, eString },  //                     U+E0B3
    { 0, eString },  //                     U+E0B4
    { 0, eString },  //                     U+E0B5
    { 0, eString },  //                     U+E0B6
    { 0, eString },  //                     U+E0B7
    { 0, eString },  //                     U+E0B8
    { 0, eString },  //                     U+E0B9
    { 0, eString },  //                     U+E0BA
    { 0, eString },  //                     U+E0BB
    { 0, eString },  //                     U+E0BC
    { 0, eString },  //                     U+E0BD
    { 0, eString },  //                     U+E0BE
    { 0, eString },  //                     U+E0BF
    { 0, eString },  //                     U+E0C0
    { 0, eString },  //                     U+E0C1
    { 0, eString },  //                     U+E0C2
    { 0, eString },  //                     U+E0C3
    { 0, eString },  //                     U+E0C4
    { 0, eString },  //                     U+E0C5
    { 0, eString },  //                     U+E0C6
    { 0, eString },  //                     U+E0C7
    { 0, eString },  //                     U+E0C8
    { 0, eString },  //                     U+E0C9
    { 0, eString },  //                     U+E0CA
    { 0, eString },  //                     U+E0CB
    { 0, eString },  //                     U+E0CC
    { 0, eString },  //                     U+E0CD
    { 0, eString },  //                     U+E0CE
    { 0, eString },  //                     U+E0CF
    { 0, eString },  //                     U+E0D0
    { 0, eString },  //                     U+E0D1
    { 0, eString },  //                     U+E0D2
    { 0, eString },  //                     U+E0D3
    { 0, eString },  //                     U+E0D4
    { 0, eString },  //                     U+E0D5
    { 0, eString },  //                     U+E0D6
    { 0, eString },  //                     U+E0D7
    { 0, eString },  //                     U+E0D8
    { 0, eString },  //                     U+E0D9
    { 0, eString },  //                     U+E0DA
    { 0, eString },  //                     U+E0DB
    { 0, eString },  //                     U+E0DC
    { 0, eString },  //                     U+E0DD
    { 0, eString },  //                     U+E0DE
    { 0, eString },  //                     U+E0DF
    { 0, eString },  //                     U+E0E0
    { 0, eString },  //                     U+E0E1
    { 0, eString },  //                     U+E0E2
    { 0, eString },  //                     U+E0E3
    { 0, eString },  //                     U+E0E4
    { 0, eString },  //                     U+E0E5
    { 0, eString },  //                     U+E0E6
    { 0, eString },  //                     U+E0E7
    { 0, eString },  //                     U+E0E8
    { 0, eString },  //                     U+E0E9
    { 0, eString },  //                     U+E0EA
    { 0, eString },  //                     U+E0EB
    { 0, eString },  //                     U+E0EC
    { 0, eString },  //                     U+E0ED
    { 0, eString },  //                     U+E0EE
    { 0, eString },  //                     U+E0EF
    { 0, eString },  //                     U+E0F0
    { 0, eString },  //                     U+E0F1
    { 0, eString },  //                     U+E0F2
    { 0, eString },  //                     U+E0F3
    { 0, eString },  //                     U+E0F4
    { 0, eString },  //                     U+E0F5
    { 0, eString },  //                     U+E0F6
    { 0, eString },  //                     U+E0F7
    { 0, eString },  //                     U+E0F8
    { 0, eString },  //                     U+E0F9
    { 0, eString },  //                     U+E0FA
    { 0, eString },  //                     U+E0FB
    { 0, eString },  //                     U+E0FC
    { 0, eString },  //                     U+E0FD
    { 0, eString },  //                     U+E0FE
    { 0, eString },  //                     U+E0FF
};
static TUnicodePlan s_Plan_E2h = {
    { "long left double arrow ", eString },  // old dictionary           U+E200
    { "long left arrow", eString },  //                                  U+E201
    { "long left and right double arrow", eString },  // old dictionary  U+E202
    { "long left and right arrow ", eString },  // old dictionary        U+E203
    { "long right double arrow ", eString },  // old dictionary          U+E204
    { "long right arrow", eString },  //                                 U+E205
    { "left double broken arrow", eString },  //                         U+E206
    { "right doubly broken arrow", eString },  //                        U+E207
    { "long mapsto", eString },  //                                      U+E208
    { "two-headed right broken arrow", eString },  //                    U+E209
    { "SW arrow-hooked", eString },  //                                  U+E20A
    { "SE arrow-hooken", eString },  //                                  U+E20B
    { "NW arrow-hooked", eString },  //                                  U+E20C
    { "NE arrow-hooked", eString },  //                                  U+E20D
    { "NE/SE arrows", eString },  //                                     U+E20E
    { "SE/SW arrows", eString },  //                                     U+E20F
    { "SW/NW arrows", eString },  //                                     U+E210
    { "NW/NE arrows", eString },  //                                     U+E211
    { "two-headed mapsto", eString },  //                                U+E212
    { 0, eString },  //                                               U+E213
    { "left fish tail", eString },  //                                   U+E214
    { "right fish tail", eString },  //                                  U+E215
    { "down arrow, up arrow ", eString },  //                            U+E216
    { "down harpoon, up harpoon ", eString },  //                        U+E217
    { "up harp, down harp ", eString },  //                              U+E218
    { "right down curved arrow", eString },  //                          U+E219
    { "left down curved arrow", eString },  //                           U+E21A
    { "not right arrow-wavy", eString },  //                             U+E21B
    { "right arrow-curved", eString },  //                               U+E21C
    { "not right arrow-curved", eString },  //                           U+E21D
    { "right arrow, plus ", eString },  //                               U+E21E
    { 0, eString },  //                                               U+E21F
    { "left arrow-bar, filled square ", eString },  //                   U+E220
    { "right arrow-bar, filled square ", eString },  //                  U+E221
    { "left arrow, filled square ", eString },  //                       U+E222
    { "right arrow, filled square ", eString },  //                      U+E223
    { "right harpoon-up over right harpoon-down", eString },  //         U+E224
    { "left harpoon-up over left harpoon-down", eString },  //           U+E225
    { "up harpoon-left, up harpoon-right ", eString },  //               U+E226
    { "down harpoon-left, down harpoon-right ", eString },  //           U+E227
    { "left-down, right-up harpoon ", eString },  //                     U+E228
    { "left-up-right-down harpoon", eString },  //                       U+E229
    { "right harpoon-up over left harpoon-up", eString },  //            U+E22A
    { "left harpoon-up over right harpoon-up", eString },  //            U+E22B
    { "left harpoon-down over right harpoon-down", eString },  //        U+E22C
    { "right harpoon-down over left harpoon-down", eString },  //        U+E22D
    { "left harpoon-up over long dash", eString },  //                   U+E22E
    { "right harpoon-down below long dash", eString },  //               U+E22F
    { "right harpoon-up over long dash", eString },  //                  U+E230
    { "left harpoon-down below long dash", eString },  //                U+E231
    { "short right arrow", eString },  //                                U+E232
    { "short left arrow", eString },  //                                 U+E233
    { "similar, right arrow below ", eString },  //                      U+E234
    { "approximate, right arrow above ", eString },  //                  U+E235
    { "equal, right arrow below ", eString },  //                        U+E236
    { "up two-headed arrow above circle", eString },  //                 U+E237
    { "right arrow with dotted stem", eString },  //                     U+E238
    { "right two-headed arrow with tail", eString },  //                 U+E239
    { 0, eString },  //                                               U+E23A
    { "right double arrow-tail", eString },  //                          U+E23B
    { "left arrow-tail", eString },  //                                  U+E23C
    { "left double arrow-tail", eString },  //                           U+E23D
    { "left, curved, down arrow ", eString },  //                        U+E23E
    { "left arrow, plus ", eString },  //                                U+E23F
    { "left and right arrow with a circle", eString },  //               U+E240
    { "right open arrow", eString },  //                                 U+E241
    { "left open arrow", eString },  //                                  U+E242
    { "horizontal open arrow", eString },  //                            U+E243
    { "right zig-zag arrow", eString },  //                              U+E244
    { 0, eString },  //                                               U+E245
    { 0, eString },  //                                               U+E246
    { 0, eString },  //                                               U+E247
    { "angle with down zig-zag arrow", eString },  //                    U+E248
    { "curved right arrow with minus", eString },  //                    U+E249
    { "curved left arrow with plus", eString },  //                      U+E24A
    { "up fish tail", eString },  //                                     U+E24B
    { "down fish tail", eString },  //                                   U+E24C
    { "right arrow, similar ", eString },  //                            U+E24D
    { "left arrow, similar ", eString },  //                             U+E24E
    { "mid, circle below ", eString },  //                               U+E24F
    { "circle, mid below ", eString },  //                               U+E250
    { " amalgamation or coproduct", eString },  // old dictionary        U+E251
    { 0, eString },  //                                               U+E252
    { 0, eString },  //                                               U+E253
    { 0, eString },  //                                               U+E254
    { 0, eString },  //                                               U+E255
    { 0, eString },  //                                               U+E256
    { 0, eString },  //                                               U+E257
    { 0, eString },  //                                               U+E258
    { "intprod", eString },  //                                          U+E259
    { "plus sign, dot below", eString },  //                             U+E25A
    { "minus sign, dot below", eString },  //                            U+E25B
    { "plus sign in left half circle", eString },  //                    U+E25C
    { "plus sign in right half circle", eString },  //                   U+E25D
    { "multiply sign in left half circle ", eString },  //               U+E25E
    { "multiply sign in right half circle", eString },  //               U+E25F
    { "circle with horizontal bar", eString },  //                       U+E260
    { "intersection, with dot", eString },  //                           U+E261
    { "subset, with dot", eString },  //                                 U+E262
    { "superset, with dot", eString },  //                               U+E263
    { "smash product", eString },  //                                    U+E264
    { "wedge, bar below", eString },  //                                 U+E265
    { "plus, small circle above", eString },  //                         U+E266
    { "plus, equals", eString },  //                                     U+E267
    { "equal, plus", eString },  //                                      U+E268
    { "plus, two; Nim-addition", eString },  //                          U+E269
    { "plus, circumflex accent above", eString },  //                    U+E26A
    { "plus, similar above", eString },  //                              U+E26B
    { "plus, similar below", eString },  //                              U+E26C
    { "times, dot", eString },  //                                       U+E26D
    { "union above intersection", eString },  //                         U+E26E
    { "intersection above union", eString },  //                         U+E26F
    { "union, bar, intersection", eString },  //                         U+E270
    { "intersection, bar, union", eString },  //                         U+E271
    { "union, union, joined", eString },  //                             U+E272
    { "intersection, intersection, joined", eString },  //               U+E273
    { "union, serifs", eString },  //                                    U+E274
    { "intersection, serifs", eString },  //                             U+E275
    { "square union, serifs", eString },  //                             U+E276
    { "square intersection, serifs", eString },  //                      U+E277
    { "closed union, serifs", eString },  //                             U+E278
    { "closed intersection, serifs", eString },  //                      U+E279
    { "closed union, serifs, smash product", eString },  //              U+E27A
    { "plus in triangle", eString },  //                                 U+E27B
    { "minus in triangle", eString },  //                                U+E27C
    { "multiply in triangle", eString },  //                             U+E27D
    { "triangle, serifs at bottom", eString },  //                       U+E27E
    { "slash in square", eString },  //                                  U+E27F
    { "reverse solidus in square", eString },  //                        U+E280
    { "intersection, and", eString },  //                                U+E281
    { "union, or", eString },  //                                        U+E282
    { "bar, union", eString },  //                                       U+E283
    { "bar, intersection", eString },  //                                U+E284
    { "divide in circle", eString },  //                                 U+E285
    { "dot, solidus, dot in circle", eString },  //                      U+E286
    { "filled circle in circle", eString },  //                          U+E287
    { "less-than in circle", eString },  //                              U+E288
    { "greater-than in circle", eString },  //                           U+E289
    { "parallel in circle", eString },  //                               U+E28A
    { "perpendicular in circle", eString },  //                          U+E28B
    { "multiply sign in double circle", eString },  //                   U+E28C
    { "multiply sign in circle, circumflex accent", eString },  //       U+E28D
    { "multiply sign, bar below", eString },  //                         U+E28E
    { 0, eString },  //                                               U+E28F
    { "most positive, two lines below", eString },  //                   U+E290
    { " right paren, gt", eString },  // old dictionary                  U+E291
    { "left parenthesis, lt", eString },  //                             U+E292
    { "rmoustache", eString },  //                                       U+E293
    { "lmoustache", eString },  //                                       U+E294
    { "dbl right parenthesis, less", eString },  //                      U+E295
    { "dbl left parenthesis, greater", eString },  //                    U+E296
    { "left angle, dot", eString },  //                                  U+E297
    { "right angle, dot", eString },  //                                 U+E298
    { "left bracket, equal", eString },  //                              U+E299
    { "right bracket, equal", eString },  //                             U+E29A
    { "left bracket, solidus top corner", eString },  //                 U+E29B
    { "right bracket, solidus bottom corner", eString },  //             U+E29C
    { "left bracket, solidus bottom corner", eString },  //              U+E29D
    { "right bracket, solidus top corner", eString },  //                U+E29E
    { " greater, not approximate", eString },  // old dictionary         U+E29F
    { 0, eString },  //                                               U+E2A0
    { " gt, vert, not dbl eq", eString },  // old dictionary             U+E2A1
    { " less, not approximate", eString },  // old dictionary            U+E2A2
    { 0, eString },  //                                               U+E2A3
    { " less, vert, not dbl eq", eString },  // old dictionary           U+E2A4
    { 0, eString },  //                                               U+E2A5
    { " not greater-than-or-equal", eString },  // old dictionary        U+E2A6
    { " not less-than-or-equal", eString },  // old dictionary           U+E2A7
    { 0, eString },  //                                               U+E2A8
    { 0, eString },  //                                               U+E2A9
    { " nshortmid", eString },  // old dictionary                        U+E2AA
    { " not short par", eString },  // old dictionary                    U+E2AB
    { 0, eString },  //                                               U+E2AC
    { 0, eString },  //                                               U+E2AD
    { 0, eString },  //                                               U+E2AE
    { 0, eString },  //                                               U+E2AF
    { 0, eString },  //                                               U+E2B0
    { 0, eString },  //                                               U+E2B1
    { 0, eString },  //                                               U+E2B2
    { " precedes, not dbl eq", eString },  // old dictionary             U+E2B3
    { 0, eString },  //                                               U+E2B4
    { " succeeds, not dbl eq", eString },  // old dictionary             U+E2B5
    { 0, eString },  //                                               U+E2B6
    { 0, eString },  //                                               U+E2B7
    { " subset not dbl eq, var", eString },  // old dictionary           U+E2B8
    { " subset, not eq, var", eString },  // old dictionary              U+E2B9
    { " superset, not eq, var", eString },  // old dictionary            U+E2BA
    { " super not dbl eq, var", eString },  // old dictionary            U+E2BB
    { "not approximately identical to", eString },  //                   U+E2BC
    { 0, eString },  //                                               U+E2BD
    { 0, eString },  //                                               U+E2BE
    { 0, eString },  //                                               U+E2BF
    { 0, eString },  //                                               U+E2C0
    { 0, eString },  //                                               U+E2C1
    { 0, eString },  //                                               U+E2C2
    { 0, eString },  //                                               U+E2C3
    { 0, eString },  //                                               U+E2C4
    { "not congruent, dot", eString },  //                               U+E2C5
    { "not, vert, approximate", eString },  //                           U+E2C6
    { "not approximately equal or equal to", eString },  //              U+E2C7
    { "parallel, similar", eString },  //                                U+E2C8
    { "not, vert, much less than", eString },  //                        U+E2C9
    { "not, vert, much greater than", eString },  //                     U+E2CA
    { "not much less than, variant", eString },  //                      U+E2CB
    { "not much greater than, variant", eString },  //                   U+E2CC
    { "not triple less than", eString },  //                             U+E2CD
    { "not triple greater than", eString },  //                          U+E2CE
    { "not, vert, right triangle, equals", eString },  //                U+E2CF
    { "not, vert, left triangle, equals", eString },  //                 U+E2D0
    { "reverse nmid", eString },  //                                     U+E2D1
    { 0, eString },  //                                               U+E2D2
    { "", eString },  // old dictionary                                  U+E2D3
    { "j", eString },  // old dictionary                                 U+E2D4
    { "Planck's over 2pi", eString },  //                                U+E2D5
    { "angle, equal", eString },  //                                     U+E2D6
    { "reverse angle, equal", eString },  //                             U+E2D7
    { "not, vert, angle", eString },  //                                 U+E2D8
    { "angle-measured, arrow, up, right", eString },  //                 U+E2D9
    { "angle-measured, arrow, up, left", eString },  //                  U+E2DA
    { "angle-measured, arrow, down, right", eString },  //               U+E2DB
    { "angle-measured, arrow, down, left", eString },  //                U+E2DC
    { "angle-measured, arrow, right, up", eString },  //                 U+E2DD
    { "angle-measured, arrow, left, up", eString },  //                  U+E2DE
    { "angle-measured, arrow, right, down", eString },  //               U+E2DF
    { "angle-measured, arrow, left, down", eString },  //                U+E2E0
    { "right angle-measured, dot", eString },  //                        U+E2E1
    { "upper right triangle", eString },  //                             U+E2E2
    { "lower right triangle", eString },  //                             U+E2E3
    { "upper left triangle", eString },  //                              U+E2E4
    { "lower left triangle", eString },  //                              U+E2E5
    { "two joined squares", eString },  //                               U+E2E6
    { "circle, slash, bar above", eString },  //                         U+E2E7
    { "circle, slash, small circle above", eString },  //                U+E2E8
    { "circle, slash, right arrow above", eString },  //                 U+E2E9
    { "circle, slash, left arrow above", eString },  //                  U+E2EA
    { "vertical zig-zag line", eString },  //                            U+E2EB
    { "trapezium", eString },  //                                        U+E2EC
    { "reverse semi-colon", eString },  //                               U+E2ED
    { "bottom square bracket", eString },  //                            U+E2EE
    { "top square bracket", eString },  //                               U+E2EF
    { 0, eString },  //                                               U+E2F0
    { 0, eString },  //                                               U+E2F1
    { 0, eString },  //                                               U+E2F2
    { 0, eString },  //                                               U+E2F3
    { 0, eString },  //                                               U+E2F4
    { 0, eString },  //                                               U+E2F5
    { ">/=", eString },  // old dictionary                               U+E2F6
    { 0, eString },  //                                               U+E2F7
    { 0, eString },  //                                               U+E2F8
    { 0, eString },  //                                               U+E2F9
    { "</=", eString },  // old dictionary                               U+E2FA
    { 0, eString },  //                                               U+E2FB
    { 0, eString },  //                                               U+E2FC
    { 0, eString },  //                                               U+E2FD
    { " precedes, equals", eString },  // old dictionary                 U+E2FE
    { 0, eString },  //                                               U+E2FF
};
static TUnicodePlan s_Plan_E3h = {
    { 0, eString },  //                                                      U+E300
    { "shortmid R:", eString },  // old dictionary                              U+E301
    { " short parallel", eString },  // old dictionary                          U+E302
    { 0, eString },  //                                                      U+E303
    { 0, eString },  //                                                      U+E304
    { 0, eString },  //                                                      U+E305
    { " thick approximate", eString },  // old dictionary                       U+E306
    { 0, eString },  //                                                      U+E307
    { 0, eString },  //                                                      U+E308
    { "equal with four dots", eString },  //                                    U+E309
    { "mlcp", eString },  //                                                    U+E30A
    { "similar, less", eString },  //                                           U+E30B
    { "similar, greater", eString },  //                                        U+E30C
    { "dbl vert, bar (under)", eString },  //                                   U+E30D
    { "double colon, equals", eString },  //                                    U+E30E
    { "dbl dash, vertical", eString },  //                                      U+E30F
    { "vert, dbl bar (under)", eString },  //                                   U+E310
    { "vert, dbl bar (over)", eString },  //                                    U+E311
    { "dbl bar, vert over and under", eString },  //                            U+E312
    { "vertical, dash (long)", eString },  //                                   U+E313
    { "congruent, dot", eString },  //                                          U+E314
    { 0, eString },  //                                                      U+E315
    { "bump, equals", eString },  //                                            U+E316
    { "equal, similar", eString },  //                                          U+E317
    { "equivalent, four dots above", eString },  //                             U+E318
    { "equal, dot below", eString },  //                                        U+E319
    { "minus, comma above", eString },  //                                      U+E31A
    { "fork, variant", eString },  //                                           U+E31B
    { "fork with top", eString },  //                                           U+E31C
    { "less-than-or-equal, slanted, dot inside", eString },  //                 U+E31D
    { "greater-than-or-equal, slanted, dot inside", eString },  //              U+E31E
    { "less-than-or-equal, slanted, dot above", eString },  //                  U+E31F
    { "greater-than-or-equal, slanted, dot above", eString },  //               U+E320
    { "less-than-or-equal, slanted, dot above right", eString },  //            U+E321
    { "greater-than-or-equal, slanted, dot above left", eString },  //          U+E322
    { "equal-or-less, slanted, dot inside", eString },  //                      U+E323
    { "equal-or-greater, slanted, dot inside", eString },  //                   U+E324
    { "less than, circle inside", eString },  //                                U+E325
    { "greater than, circle inside", eString },  //                             U+E326
    { "equal-or-less", eString },  //                                           U+E327
    { "equal-or-greater", eString },  //                                        U+E328
    { "less than, questionmark above", eString },  //                           U+E329
    { "greater than, questionmark above", eString },  //                        U+E32A
    { "less, equal, slanted, greater", eString },  //                           U+E32B
    { "greater, equal, slanted, less", eString },  //                           U+E32C
    { "less, greater, equal", eString },  //                                    U+E32D
    { "greater, less, equal", eString },  //                                    U+E32E
    { "greater, less, overlapping", eString },  //                              U+E32F
    { "greater, less, apart", eString },  //                                    U+E330
    { "less, equal, slanted, greater, equal, slanted", eString },  //           U+E331
    { "greater, equal, slanted, less, equal, slanted", eString },  //           U+E332
    { "less, similar, equal", eString },  //                                    U+E333
    { "greater, similar, equal", eString },  //                                 U+E334
    { "less, similar, greater", eString },  //                                  U+E335
    { "greater, similar, less", eString },  //                                  U+E336
    { "similar, less, equal", eString },  //                                    U+E337
    { "similar, greater, equal", eString },  //                                 U+E338
    { "smaller than", eString },  //                                            U+E339
    { "larger than", eString },  //                                             U+E33A
    { "smaller than or equal", eString },  //                                   U+E33B
    { "larger than or equal", eString },  //                                    U+E33C
    { "smaller than or equal, slanted", eString },  //                          U+E33D
    { "larger than or equal, slanted", eString },  //                           U+E33E
    { "subset, right arrow", eString },  //                                     U+E33F
    { "superset, left arrow", eString },  //                                    U+E340
    { "subset, plus", eString },  //                                            U+E341
    { "superset, plus", eString },  //                                          U+E342
    { "subset, multiply", eString },  //                                        U+E343
    { "superset, multiply", eString },  //                                      U+E344
    { "subset, similar", eString },  //                                         U+E345
    { "superset, similar", eString },  //                                       U+E346
    { "subset above superset", eString },  //                                   U+E347
    { "superset above subset", eString },  //                                   U+E348
    { "subset above subset", eString },  //                                     U+E349
    { "superset above superset", eString },  //                                 U+E34A
    { "superset, subset", eString },  //                                        U+E34B
    { "superset, subset, dash joining them", eString },  //                     U+E34C
    { "reverse solidus, subset", eString },  //                                 U+E34D
    { "superset, solidus", eString },  //                                       U+E34E
    { "subset, equals, dot", eString },  //                                     U+E34F
    { "superset, equals, dot", eString },  //                                   U+E350
    { "subset, closed", eString },  //                                          U+E351
    { "superset, closed", eString },  //                                        U+E352
    { "subset, closed, equals", eString },  //                                  U+E353
    { "superset, closed, equals", eString },  //                                U+E354
    { "less than, closed by curve", eString },  //                              U+E355
    { "greater than, closed by curve", eString },  //                           U+E356
    { "less than, closed by curve, equal, slanted", eString },  //              U+E357
    { "greater than, closed by curve, equal, slanted", eString },  //           U+E358
    { "right triangle above left triangle", eString },  //                      U+E359
    { 0, eString },  //                                                      U+E35A
    { 0, eString },  //                                                      U+E35B
    { "dbl precedes", eString },  //                                            U+E35C
    { "dbl succeeds", eString },  //                                            U+E35D
    { "less than, left arrow", eString },  //                                   U+E35E
    { "greater than, right arrow", eString },  //                               U+E35F
    { 0, eString },  //                                                      U+E360
    { 0, eString },  //                                                      U+E361
    { 0, eString },  //                                                      U+E362
    { 0, eString },  //                                                      U+E363
    { 0, eString },  //                                                      U+E364
    { " if", eString },  // old dictionary                                      U+E365
    { 0, eString },  //                                                      U+E366
    { 0, eString },  //                                                      U+E367
    { 0, eString },  //                                                      U+E368
    { 0, eString },  //                                                      U+E369
    { 0, eString },  //                                                      U+E36A
    { 0, eString },  //                                                      U+E36B
    { 0, eString },  //                                                      U+E36C
    { 0, eString },  //                                                      U+E36D
    { "two logical and", eString },  //                                         U+E36E
    { "two logical or", eString },  //                                          U+E36F
    { 0, eString },  //                                                      U+E370
    { "quadruple prime", eString },  //                                         U+E371
    { "infinity sign, incomplete", eString },  //                               U+E372
    { 0, eString },  //                                                      U+E373
    { "dbl logical and", eString },  //                                         U+E374
    { "dbl logical or", eString },  //                                          U+E375
    { "integral around a point operator", eString },  //                        U+E376
    { "quaternion integral", eString },  //                                     U+E377
    { "quadruple integral", eString },  //                                      U+E378
    { "left open parenthesis", eString },  //                                   U+E379
    { "right open parenthesis", eString },  //                                  U+E37A
    { "negated set membership, variant", eString },  //                         U+E37B
    { "negated set membership, variant", eString },  //                         U+E37C
    { "contains, variant", eString },  //                                       U+E37D
    { "contains, variant", eString },  //                                       U+E37E
    { 0, eString },  //                                                      U+E37F
    { "straightness", eString },  //                                            U+E380
    { "flatness", eString },  //                                                U+E381
    { "parallel, slanted", eString },  //                                       U+E382
    { "top, circle below", eString },  //                                       U+E383
    { "homothetically congruent to", eString },  //                             U+E384
    { "similar, parallel, slanted, equal", eString },  //                       U+E385
    { "congruent and parallel", eString },  //                                  U+E386
    { "reverse not equivalent", eString },  //                                  U+E387
    { "reverse not equal", eString },  //                                       U+E388
    { "not parallel, slanted", eString },  //                                   U+E389
    { "not equal, dot", eString },  //                                          U+E38A
    { "similar, dot", eString },  //                                            U+E38B
    { "approximate, circumflex accent", eString },  //                          U+E38C
    { "not, horizontal, parallel", eString },  //                               U+E38D
    { "not, vert, infinity", eString },  //                                     U+E38E
    { 0, eString },  //                                                      U+E38F
    { "not partial differential", eString },  //                                U+E390
    { "and with middle stem", eString },  //                                    U+E391
    { "or with middle stem", eString },  //                                     U+E392
    { "or, horizontal dash", eString },  //                                     U+E393
    { "and, horizontal dash", eString },  //                                    U+E394
    { "circulation function", eString },  //                                    U+E395
    { "finite part integral", eString },  //                                    U+E396
    { "line integration, rectangular path around pole", eString },  //          U+E397
    { "line integration, semi-circular path around pole", eString },  //        U+E398
    { "line integration, not including the pole", eString },  //                U+E399
    { "integral, left arrow with hook", eString },  //                          U+E39A
    { "anti clock-wise integration", eString },  //                             U+E39B
    { "set membership, dot above", eString },  //                               U+E39C
    { "negated set membership, dot above", eString },  //                       U+E39D
    { "set membership, two horizontal strokes", eString },  //                  U+E39E
    { 0, eString },  //                                                      U+E39F
    { "set membership, long horizontal stroke", eString },  //                  U+E3A0
    { "contains, long horizontal stroke", eString },  //                        U+E3A1
    { "large set membership, vertical bar on horizontal stroke", eString },  // U+E3A2
    { "contains", eString },  //                                                U+E3A3
    { "set membership, vertical bar on horizontal stroke", eString },  //       U+E3A4
    { "contains, vertical bar on horizontal stroke", eString },  //             U+E3A5
    { "ac current", eString },  //                                              U+E3A6
    { "electrical intersection", eString },  //                                 U+E3A7
    { "circle, cross", eString },  //                                           U+E3A8
    { "solidus, bar above", eString },  //                                      U+E3A9
    { "large downward pointing angle", eString },  //                           U+E3AA
    { "large upward pointing angle", eString },  //                             U+E3AB
    { "not with two horizontal strokes", eString },  //                         U+E3AC
    { "reverse not with two horizontal strokes", eString },  //                 U+E3AD
    { "sloping large or", eString },  //                                        U+E3AE
    { 0, eString },  //                                                      U+E3AF
    { 0, eString },  //                                                      U+E3B0
    { 0, eString },  //                                                      U+E3B1
    { " fj ", eString },  // old dictionary                                     U+E3B2
    { 0, eString },  //                                                      U+E3B3
    { 0, eString },  //                                                      U+E3B4
    { 0, eString },  //                                                      U+E3B5
    { 0, eString },  //                                                      U+E3B6
    { 0, eString },  //                                                      U+E3B7
    { 0, eString },  //                                                      U+E3B8
    { 0, eString },  //                                                      U+E3B9
    { 0, eString },  //                                                      U+E3BA
    { 0, eString },  //                                                      U+E3BB
    { 0, eString },  //                                                      U+E3BC
    { 0, eString },  //                                                      U+E3BD
    { 0, eString },  //                                                      U+E3BE
    { 0, eString },  //                                                      U+E3BF
    { 0, eString },  //                                                      U+E3C0
    { 0, eString },  //                                                      U+E3C1
    { 0, eString },  //                                                      U+E3C2
    { 0, eString },  //                                                      U+E3C3
    { 0, eString },  //                                                      U+E3C4
    { 0, eString },  //                                                      U+E3C5
    { 0, eString },  //                                                      U+E3C6
    { 0, eString },  //                                                      U+E3C7
    { 0, eString },  //                                                      U+E3C8
    { 0, eString },  //                                                      U+E3C9
    { 0, eString },  //                                                      U+E3CA
    { 0, eString },  //                                                      U+E3CB
    { 0, eString },  //                                                      U+E3CC
    { 0, eString },  //                                                      U+E3CD
    { 0, eString },  //                                                      U+E3CE
    { 0, eString },  //                                                      U+E3CF
    { 0, eString },  //                                                      U+E3D0
    { 0, eString },  //                                                      U+E3D1
    { 0, eString },  //                                                      U+E3D2
    { 0, eString },  //                                                      U+E3D3
    { 0, eString },  //                                                      U+E3D4
    { 0, eString },  //                                                      U+E3D5
    { 0, eString },  //                                                      U+E3D6
    { 0, eString },  //                                                      U+E3D7
    { 0, eString },  //                                                      U+E3D8
    { 0, eString },  //                                                      U+E3D9
    { 0, eString },  //                                                      U+E3DA
    { 0, eString },  //                                                      U+E3DB
    { 0, eString },  //                                                      U+E3DC
    { 0, eString },  //                                                      U+E3DD
    { 0, eString },  //                                                      U+E3DE
    { 0, eString },  //                                                      U+E3DF
    { 0, eString },  //                                                      U+E3E0
    { 0, eString },  //                                                      U+E3E1
    { 0, eString },  //                                                      U+E3E2
    { 0, eString },  //                                                      U+E3E3
    { 0, eString },  //                                                      U+E3E4
    { 0, eString },  //                                                      U+E3E5
    { 0, eString },  //                                                      U+E3E6
    { 0, eString },  //                                                      U+E3E7
    { 0, eString },  //                                                      U+E3E8
    { 0, eString },  //                                                      U+E3E9
    { 0, eString },  //                                                      U+E3EA
    { 0, eString },  //                                                      U+E3EB
    { 0, eString },  //                                                      U+E3EC
    { 0, eString },  //                                                      U+E3ED
    { 0, eString },  //                                                      U+E3EE
    { 0, eString },  //                                                      U+E3EF
    { 0, eString },  //                                                      U+E3F0
    { 0, eString },  //                                                      U+E3F1
    { 0, eString },  //                                                      U+E3F2
    { 0, eString },  //                                                      U+E3F3
    { 0, eString },  //                                                      U+E3F4
    { 0, eString },  //                                                      U+E3F5
    { 0, eString },  //                                                      U+E3F6
    { 0, eString },  //                                                      U+E3F7
    { 0, eString },  //                                                      U+E3F8
    { 0, eString },  //                                                      U+E3F9
    { 0, eString },  //                                                      U+E3FA
    { 0, eString },  //                                                      U+E3FB
    { 0, eString },  //                                                      U+E3FC
    { 0, eString },  //                                                      U+E3FD
    { 0, eString },  //                                                      U+E3FE
    { 0, eString },  //                                                      U+E3FF
};
static TUnicodePlan s_Plan_E4h = {
    { "right, curved, down arrow ", eString },  //                  U+E400
    { 0, eString },  //                                          U+E401
    { "left broken arrow", eString },  //                           U+E402
    { 0, eString },  //                                          U+E403
    { 0, eString },  //                                          U+E404
    { "right broken arrow", eString },  //                          U+E405
    { 0, eString },  //                                          U+E406
    { 0, eString },  //                                          U+E407
    { 0, eString },  //                                          U+E408
    { "large circle in circle", eString },  //                      U+E409
    { "vertical bar in circle", eString },  //                      U+E40A
    { 0, eString },  //                                          U+E40B
    { "reverse most positive, line below", eString },  //           U+E40C
    { 0, eString },  //                                          U+E40D
    { 0, eString },  //                                          U+E40E
    { 0, eString },  //                                          U+E40F
    { 0, eString },  //                                          U+E410
    { 0, eString },  //                                          U+E411
    { 0, eString },  //                                          U+E412
    { 0, eString },  //                                          U+E413
    { 0, eString },  //                                          U+E414
    { "not, vert, similar", eString },  //                          U+E415
    { "solidus, bar through", eString },  //                        U+E416
    { 0, eString },  //                                          U+E417
    { "right angle-measured", eString },  //                        U+E418
    { "bottom above top square bracket", eString },  //             U+E419
    { "reversed circle, slash", eString },  //                      U+E41A
    { "circle, two horizontal stroked to the right", eString },  // U+E41B
    { "circle, small circle to the right", eString },  //           U+E41C
    { 0, eString },  //                                          U+E41D
    { 0, eString },  //                                          U+E41E
    { 0, eString },  //                                          U+E41F
    { 0, eString },  //                                          U+E420
    { 0, eString },  //                                          U+E421
    { 0, eString },  //                                          U+E422
    { 0, eString },  //                                          U+E423
    { 0, eString },  //                                          U+E424
    { 0, eString },  //                                          U+E425
    { 0, eString },  //                                          U+E426
    { 0, eString },  //                                          U+E427
    { 0, eString },  //                                          U+E428
    { " thick similar", eString },  // old dictionary               U+E429
    { 0, eString },  //                                          U+E42A
    { 0, eString },  //                                          U+E42B
    { 0, eString },  //                                          U+E42C
    { 0, eString },  //                                          U+E42D
    { 0, eString },  //                                          U+E42E
    { 0, eString },  //                                          U+E42F
    { 0, eString },  //                                          U+E430
    { 0, eString },  //                                          U+E431
    { 0, eString },  //                                          U+E432
    { 0, eString },  //                                          U+E433
    { 0, eString },  //                                          U+E434
    { 0, eString },  //                                          U+E435
    { 0, eString },  //                                          U+E436
    { 0, eString },  //                                          U+E437
    { 0, eString },  //                                          U+E438
    { 0, eString },  //                                          U+E439
    { 0, eString },  //                                          U+E43A
    { 0, eString },  //                                          U+E43B
    { 0, eString },  //                                          U+E43C
    { 0, eString },  //                                          U+E43D
    { 0, eString },  //                                          U+E43E
    { 0, eString },  //                                          U+E43F
    { 0, eString },  //                                          U+E440
    { 0, eString },  //                                          U+E441
    { 0, eString },  //                                          U+E442
    { 0, eString },  //                                          U+E443
    { 0, eString },  //                                          U+E444
    { 0, eString },  //                                          U+E445
    { 0, eString },  //                                          U+E446
    { 0, eString },  //                                          U+E447
    { 0, eString },  //                                          U+E448
    { 0, eString },  //                                          U+E449
    { 0, eString },  //                                          U+E44A
    { 0, eString },  //                                          U+E44B
    { 0, eString },  //                                          U+E44C
    { 0, eString },  //                                          U+E44D
    { 0, eString },  //                                          U+E44E
    { 0, eString },  //                                          U+E44F
    { 0, eString },  //                                          U+E450
    { 0, eString },  //                                          U+E451
    { 0, eString },  //                                          U+E452
    { 0, eString },  //                                          U+E453
    { 0, eString },  //                                          U+E454
    { 0, eString },  //                                          U+E455
    { 0, eString },  //                                          U+E456
    { 0, eString },  //                                          U+E457
    { 0, eString },  //                                          U+E458
    { 0, eString },  //                                          U+E459
    { 0, eString },  //                                          U+E45A
    { 0, eString },  //                                          U+E45B
    { 0, eString },  //                                          U+E45C
    { 0, eString },  //                                          U+E45D
    { 0, eString },  //                                          U+E45E
    { 0, eString },  //                                          U+E45F
    { 0, eString },  //                                          U+E460
    { 0, eString },  //                                          U+E461
    { 0, eString },  //                                          U+E462
    { 0, eString },  //                                          U+E463
    { 0, eString },  //                                          U+E464
    { 0, eString },  //                                          U+E465
    { 0, eString },  //                                          U+E466
    { 0, eString },  //                                          U+E467
    { 0, eString },  //                                          U+E468
    { 0, eString },  //                                          U+E469
    { 0, eString },  //                                          U+E46A
    { 0, eString },  //                                          U+E46B
    { 0, eString },  //                                          U+E46C
    { 0, eString },  //                                          U+E46D
    { 0, eString },  //                                          U+E46E
    { 0, eString },  //                                          U+E46F
    { 0, eString },  //                                          U+E470
    { 0, eString },  //                                          U+E471
    { 0, eString },  //                                          U+E472
    { 0, eString },  //                                          U+E473
    { 0, eString },  //                                          U+E474
    { 0, eString },  //                                          U+E475
    { 0, eString },  //                                          U+E476
    { 0, eString },  //                                          U+E477
    { 0, eString },  //                                          U+E478
    { 0, eString },  //                                          U+E479
    { 0, eString },  //                                          U+E47A
    { 0, eString },  //                                          U+E47B
    { 0, eString },  //                                          U+E47C
    { 0, eString },  //                                          U+E47D
    { 0, eString },  //                                          U+E47E
    { 0, eString },  //                                          U+E47F
    { 0, eString },  //                                          U+E480
    { 0, eString },  //                                          U+E481
    { 0, eString },  //                                          U+E482
    { 0, eString },  //                                          U+E483
    { 0, eString },  //                                          U+E484
    { 0, eString },  //                                          U+E485
    { 0, eString },  //                                          U+E486
    { 0, eString },  //                                          U+E487
    { 0, eString },  //                                          U+E488
    { 0, eString },  //                                          U+E489
    { 0, eString },  //                                          U+E48A
    { 0, eString },  //                                          U+E48B
    { 0, eString },  //                                          U+E48C
    { 0, eString },  //                                          U+E48D
    { 0, eString },  //                                          U+E48E
    { 0, eString },  //                                          U+E48F
    { 0, eString },  //                                          U+E490
    { 0, eString },  //                                          U+E491
    { 0, eString },  //                                          U+E492
    { 0, eString },  //                                          U+E493
    { 0, eString },  //                                          U+E494
    { 0, eString },  //                                          U+E495
    { 0, eString },  //                                          U+E496
    { 0, eString },  //                                          U+E497
    { 0, eString },  //                                          U+E498
    { 0, eString },  //                                          U+E499
    { 0, eString },  //                                          U+E49A
    { 0, eString },  //                                          U+E49B
    { 0, eString },  //                                          U+E49C
    { 0, eString },  //                                          U+E49D
    { 0, eString },  //                                          U+E49E
    { 0, eString },  //                                          U+E49F
    { 0, eString },  //                                          U+E4A0
    { 0, eString },  //                                          U+E4A1
    { 0, eString },  //                                          U+E4A2
    { 0, eString },  //                                          U+E4A3
    { 0, eString },  //                                          U+E4A4
    { 0, eString },  //                                          U+E4A5
    { 0, eString },  //                                          U+E4A6
    { 0, eString },  //                                          U+E4A7
    { 0, eString },  //                                          U+E4A8
    { 0, eString },  //                                          U+E4A9
    { 0, eString },  //                                          U+E4AA
    { 0, eString },  //                                          U+E4AB
    { 0, eString },  //                                          U+E4AC
    { 0, eString },  //                                          U+E4AD
    { 0, eString },  //                                          U+E4AE
    { 0, eString },  //                                          U+E4AF
    { 0, eString },  //                                          U+E4B0
    { 0, eString },  //                                          U+E4B1
    { 0, eString },  //                                          U+E4B2
    { 0, eString },  //                                          U+E4B3
    { 0, eString },  //                                          U+E4B4
    { 0, eString },  //                                          U+E4B5
    { 0, eString },  //                                          U+E4B6
    { 0, eString },  //                                          U+E4B7
    { 0, eString },  //                                          U+E4B8
    { 0, eString },  //                                          U+E4B9
    { 0, eString },  //                                          U+E4BA
    { 0, eString },  //                                          U+E4BB
    { 0, eString },  //                                          U+E4BC
    { 0, eString },  //                                          U+E4BD
    { 0, eString },  //                                          U+E4BE
    { 0, eString },  //                                          U+E4BF
    { 0, eString },  //                                          U+E4C0
    { 0, eString },  //                                          U+E4C1
    { 0, eString },  //                                          U+E4C2
    { 0, eString },  //                                          U+E4C3
    { 0, eString },  //                                          U+E4C4
    { 0, eString },  //                                          U+E4C5
    { 0, eString },  //                                          U+E4C6
    { 0, eString },  //                                          U+E4C7
    { 0, eString },  //                                          U+E4C8
    { 0, eString },  //                                          U+E4C9
    { 0, eString },  //                                          U+E4CA
    { 0, eString },  //                                          U+E4CB
    { 0, eString },  //                                          U+E4CC
    { 0, eString },  //                                          U+E4CD
    { 0, eString },  //                                          U+E4CE
    { 0, eString },  //                                          U+E4CF
    { 0, eString },  //                                          U+E4D0
    { 0, eString },  //                                          U+E4D1
    { 0, eString },  //                                          U+E4D2
    { 0, eString },  //                                          U+E4D3
    { 0, eString },  //                                          U+E4D4
    { 0, eString },  //                                          U+E4D5
    { 0, eString },  //                                          U+E4D6
    { 0, eString },  //                                          U+E4D7
    { 0, eString },  //                                          U+E4D8
    { 0, eString },  //                                          U+E4D9
    { 0, eString },  //                                          U+E4DA
    { 0, eString },  //                                          U+E4DB
    { 0, eString },  //                                          U+E4DC
    { 0, eString },  //                                          U+E4DD
    { 0, eString },  //                                          U+E4DE
    { 0, eString },  //                                          U+E4DF
    { 0, eString },  //                                          U+E4E0
    { 0, eString },  //                                          U+E4E1
    { 0, eString },  //                                          U+E4E2
    { 0, eString },  //                                          U+E4E3
    { 0, eString },  //                                          U+E4E4
    { 0, eString },  //                                          U+E4E5
    { 0, eString },  //                                          U+E4E6
    { 0, eString },  //                                          U+E4E7
    { 0, eString },  //                                          U+E4E8
    { 0, eString },  //                                          U+E4E9
    { 0, eString },  //                                          U+E4EA
    { 0, eString },  //                                          U+E4EB
    { 0, eString },  //                                          U+E4EC
    { 0, eString },  //                                          U+E4ED
    { 0, eString },  //                                          U+E4EE
    { 0, eString },  //                                          U+E4EF
    { 0, eString },  //                                          U+E4F0
    { 0, eString },  //                                          U+E4F1
    { 0, eString },  //                                          U+E4F2
    { 0, eString },  //                                          U+E4F3
    { 0, eString },  //                                          U+E4F4
    { 0, eString },  //                                          U+E4F5
    { 0, eString },  //                                          U+E4F6
    { 0, eString },  //                                          U+E4F7
    { 0, eString },  //                                          U+E4F8
    { 0, eString },  //                                          U+E4F9
    { 0, eString },  //                                          U+E4FA
    { 0, eString },  //                                          U+E4FB
    { 0, eString },  //                                          U+E4FC
    { 0, eString },  //                                          U+E4FD
    { 0, eString },  //                                          U+E4FE
    { 0, eString },  //                                          U+E4FF
};
static TUnicodePlan s_Plan_E5h = {
    { "A", eString },  //                                              U+E500
    { "B", eString },  //                                              U+E501
    { 0, eString },  //                                             U+E502
    { "D", eString },  //                                              U+E503
    { "E", eString },  //                                              U+E504
    { "F", eString },  //                                              U+E505
    { "G", eString },  //                                              U+E506
    { "H", eString },  //                                              U+E507
    { "I", eString },  //                                              U+E508
    { "J", eString },  //                                              U+E509
    { "K", eString },  //                                              U+E50A
    { "L", eString },  //                                              U+E50B
    { "M", eString },  //                                              U+E50C
    { "negated set membership, two horizontal strokes", eString },  // U+E50D
    { "O", eString },  //                                              U+E50E
    { 0, eString },  //                                             U+E50F
    { 0, eString },  //                                             U+E510
    { 0, eString },  //                                             U+E511
    { "S", eString },  //                                              U+E512
    { "T", eString },  //                                              U+E513
    { "U", eString },  //                                              U+E514
    { "V", eString },  //                                              U+E515
    { "W", eString },  //                                              U+E516
    { "X", eString },  //                                              U+E517
    { "Y", eString },  //                                              U+E518
    { 0, eString },  //                                             U+E519
    { 0, eString },  //                                             U+E51A
    { 0, eString },  //                                             U+E51B
    { 0, eString },  //                                             U+E51C
    { 0, eString },  //                                             U+E51D
    { 0, eString },  //                                             U+E51E
    { 0, eString },  //                                             U+E51F
    { "A", eString },  //                                              U+E520
    { 0, eString },  //                                             U+E521
    { "C", eString },  //                                              U+E522
    { "D", eString },  //                                              U+E523
    { 0, eString },  //                                             U+E524
    { 0, eString },  //                                             U+E525
    { "G", eString },  //                                              U+E526
    { 0, eString },  //                                             U+E527
    { 0, eString },  //                                             U+E528
    { "J", eString },  //                                              U+E529
    { "K", eString },  //                                              U+E52A
    { 0, eString },  //                                             U+E52B
    { 0, eString },  //                                             U+E52C
    { "N", eString },  //                                              U+E52D
    { "O", eString },  //                                              U+E52E
    { "P", eString },  //                                              U+E52F
    { "Q", eString },  //                                              U+E530
    { "R", eString },  //                                              U+E531
    { "S", eString },  //                                              U+E532
    { "T", eString },  //                                              U+E533
    { "U", eString },  //                                              U+E534
    { "V", eString },  //                                              U+E535
    { "W", eString },  //                                              U+E536
    { "X", eString },  //                                              U+E537
    { "Y", eString },  //                                              U+E538
    { "Z", eString },  //                                              U+E539
    { 0, eString },  //                                             U+E53A
    { 0, eString },  //                                             U+E53B
    { 0, eString },  //                                             U+E53C
    { 0, eString },  //                                             U+E53D
    { 0, eString },  //                                             U+E53E
    { 0, eString },  //                                             U+E53F
    { "a", eString },  //                                              U+E540
    { "b", eString },  //                                              U+E541
    { "c", eString },  //                                              U+E542
    { "d", eString },  //                                              U+E543
    { 0, eString },  //                                             U+E544
    { "f", eString },  //                                              U+E545
    { "g", eString },  //                                              U+E546
    { "h", eString },  //                                              U+E547
    { "i", eString },  //                                              U+E548
    { "j", eString },  //                                              U+E549
    { "k", eString },  //                                              U+E54A
    { "l", eString },  //                                              U+E54B
    { "m", eString },  //                                              U+E54C
    { "n", eString },  //                                              U+E54D
    { 0, eString },  //                                             U+E54E
    { "p", eString },  //                                              U+E54F
    { "q", eString },  //                                              U+E550
    { "r", eString },  //                                              U+E551
    { "s", eString },  //                                              U+E552
    { "t", eString },  //                                              U+E553
    { "u", eString },  //                                              U+E554
    { "v", eString },  //                                              U+E555
    { "w", eString },  //                                              U+E556
    { "x", eString },  //                                              U+E557
    { "y", eString },  //                                              U+E558
    { "z", eString },  //                                              U+E559
    { 0, eString },  //                                             U+E55A
    { 0, eString },  //                                             U+E55B
    { 0, eString },  //                                             U+E55C
    { 0, eString },  //                                             U+E55D
    { 0, eString },  //                                             U+E55E
    { 0, eString },  //                                             U+E55F
    { "A", eString },  //                                              U+E560
    { "B", eString },  //                                              U+E561
    { "C", eString },  //                                              U+E562
    { "D", eString },  //                                              U+E563
    { "E", eString },  //                                              U+E564
    { "F", eString },  //                                              U+E565
    { "G", eString },  //                                              U+E566
    { 0, eString },  //                                             U+E567
    { 0, eString },  //                                             U+E568
    { "J", eString },  //                                              U+E569
    { "K", eString },  //                                              U+E56A
    { "L", eString },  //                                              U+E56B
    { "M", eString },  //                                              U+E56C
    { "N", eString },  //                                              U+E56D
    { "O", eString },  //                                              U+E56E
    { "P", eString },  //                                              U+E56F
    { "Q", eString },  //                                              U+E570
    { 0, eString },  //                                             U+E571
    { "S", eString },  //                                              U+E572
    { "T", eString },  //                                              U+E573
    { "U", eString },  //                                              U+E574
    { "V", eString },  //                                              U+E575
    { "W", eString },  //                                              U+E576
    { "X", eString },  //                                              U+E577
    { "Y", eString },  //                                              U+E578
    { "Z", eString },  //                                              U+E579
    { 0, eString },  //                                             U+E57A
    { 0, eString },  //                                             U+E57B
    { 0, eString },  //                                             U+E57C
    { 0, eString },  //                                             U+E57D
    { 0, eString },  //                                             U+E57E
    { 0, eString },  //                                             U+E57F
    { "a", eString },  //                                              U+E580
    { "b", eString },  //                                              U+E581
    { "c", eString },  //                                              U+E582
    { "d", eString },  //                                              U+E583
    { "e", eString },  //                                              U+E584
    { "f", eString },  //                                              U+E585
    { "g", eString },  //                                              U+E586
    { "h", eString },  //                                              U+E587
    { "i", eString },  //                                              U+E588
    { "j", eString },  //                                              U+E589
    { "k", eString },  //                                              U+E58A
    { "l", eString },  //                                              U+E58B
    { "m", eString },  //                                              U+E58C
    { "n", eString },  //                                              U+E58D
    { "o", eString },  //                                              U+E58E
    { "p", eString },  //                                              U+E58F
    { "q", eString },  //                                              U+E590
    { "r", eString },  //                                              U+E591
    { "s", eString },  //                                              U+E592
    { "t", eString },  //                                              U+E593
    { "u", eString },  //                                              U+E594
    { "v", eString },  //                                              U+E595
    { "w", eString },  //                                              U+E596
    { "x", eString },  //                                              U+E597
    { "y", eString },  //                                              U+E598
    { "z", eString },  //                                              U+E599
    { 0, eString },  //                                             U+E59A
    { 0, eString },  //                                             U+E59B
    { 0, eString },  //                                             U+E59C
    { 0, eString },  //                                             U+E59D
    { 0, eString },  //                                             U+E59E
    { 0, eString },  //                                             U+E59F
    { 0, eString },  //                                             U+E5A0
    { 0, eString },  //                                             U+E5A1
    { 0, eString },  //                                             U+E5A2
    { 0, eString },  //                                             U+E5A3
    { 0, eString },  //                                             U+E5A4
    { 0, eString },  //                                             U+E5A5
    { 0, eString },  //                                             U+E5A6
    { 0, eString },  //                                             U+E5A7
    { 0, eString },  //                                             U+E5A8
    { 0, eString },  //                                             U+E5A9
    { 0, eString },  //                                             U+E5AA
    { 0, eString },  //                                             U+E5AB
    { 0, eString },  //                                             U+E5AC
    { 0, eString },  //                                             U+E5AD
    { 0, eString },  //                                             U+E5AE
    { 0, eString },  //                                             U+E5AF
    { 0, eString },  //                                             U+E5B0
    { 0, eString },  //                                             U+E5B1
    { 0, eString },  //                                             U+E5B2
    { 0, eString },  //                                             U+E5B3
    { 0, eString },  //                                             U+E5B4
    { 0, eString },  //                                             U+E5B5
    { 0, eString },  //                                             U+E5B6
    { 0, eString },  //                                             U+E5B7
    { 0, eString },  //                                             U+E5B8
    { 0, eString },  //                                             U+E5B9
    { 0, eString },  //                                             U+E5BA
    { 0, eString },  //                                             U+E5BB
    { 0, eString },  //                                             U+E5BC
    { 0, eString },  //                                             U+E5BD
    { 0, eString },  //                                             U+E5BE
    { 0, eString },  //                                             U+E5BF
    { 0, eString },  //                                             U+E5C0
    { 0, eString },  //                                             U+E5C1
    { 0, eString },  //                                             U+E5C2
    { 0, eString },  //                                             U+E5C3
    { 0, eString },  //                                             U+E5C4
    { 0, eString },  //                                             U+E5C5
    { 0, eString },  //                                             U+E5C6
    { 0, eString },  //                                             U+E5C7
    { 0, eString },  //                                             U+E5C8
    { 0, eString },  //                                             U+E5C9
    { 0, eString },  //                                             U+E5CA
    { 0, eString },  //                                             U+E5CB
    { 0, eString },  //                                             U+E5CC
    { 0, eString },  //                                             U+E5CD
    { 0, eString },  //                                             U+E5CE
    { 0, eString },  //                                             U+E5CF
    { 0, eString },  //                                             U+E5D0
    { 0, eString },  //                                             U+E5D1
    { 0, eString },  //                                             U+E5D2
    { 0, eString },  //                                             U+E5D3
    { 0, eString },  //                                             U+E5D4
    { 0, eString },  //                                             U+E5D5
    { 0, eString },  //                                             U+E5D6
    { 0, eString },  //                                             U+E5D7
    { 0, eString },  //                                             U+E5D8
    { 0, eString },  //                                             U+E5D9
    { 0, eString },  //                                             U+E5DA
    { 0, eString },  //                                             U+E5DB
    { " not precedes, equals", eString },  // old dictionary           U+E5DC
    { 0, eString },  //                                             U+E5DD
    { 0, eString },  //                                             U+E5DE
    { 0, eString },  //                                             U+E5DF
    { 0, eString },  //                                             U+E5E0
    { 0, eString },  //                                             U+E5E1
    { 0, eString },  //                                             U+E5E2
    { 0, eString },  //                                             U+E5E3
    { 0, eString },  //                                             U+E5E4
    { 0, eString },  //                                             U+E5E5
    { 0, eString },  //                                             U+E5E6
    { 0, eString },  //                                             U+E5E7
    { 0, eString },  //                                             U+E5E8
    { 0, eString },  //                                             U+E5E9
    { 0, eString },  //                                             U+E5EA
    { 0, eString },  //                                             U+E5EB
    { 0, eString },  //                                             U+E5EC
    { 0, eString },  //                                             U+E5ED
    { 0, eString },  //                                             U+E5EE
    { 0, eString },  //                                             U+E5EF
    { 0, eString },  //                                             U+E5F0
    { " not succeeds, equals", eString },  // old dictionary           U+E5F1
    { 0, eString },  //                                             U+E5F2
    { 0, eString },  //                                             U+E5F3
    { 0, eString },  //                                             U+E5F4
    { 0, eString },  //                                             U+E5F5
    { 0, eString },  //                                             U+E5F6
    { 0, eString },  //                                             U+E5F7
    { 0, eString },  //                                             U+E5F8
    { 0, eString },  //                                             U+E5F9
    { 0, eString },  //                                             U+E5FA
    { 0, eString },  //                                             U+E5FB
    { 0, eString },  //                                             U+E5FC
    { 0, eString },  //                                             U+E5FD
    { 0, eString },  //                                             U+E5FE
    { 0, eString },  //                                             U+E5FF
};
static TUnicodePlan s_Plan_E6h = {
    { 0, eString },  // U+E600
    { 0, eString },  // U+E601
    { 0, eString },  // U+E602
    { 0, eString },  // U+E603
    { 0, eString },  // U+E604
    { 0, eString },  // U+E605
    { 0, eString },  // U+E606
    { 0, eString },  // U+E607
    { 0, eString },  // U+E608
    { 0, eString },  // U+E609
    { 0, eString },  // U+E60A
    { 0, eString },  // U+E60B
    { 0, eString },  // U+E60C
    { 0, eString },  // U+E60D
    { 0, eString },  // U+E60E
    { 0, eString },  // U+E60F
    { 0, eString },  // U+E610
    { 0, eString },  // U+E611
    { 0, eString },  // U+E612
    { 0, eString },  // U+E613
    { 0, eString },  // U+E614
    { 0, eString },  // U+E615
    { 0, eString },  // U+E616
    { 0, eString },  // U+E617
    { 0, eString },  // U+E618
    { 0, eString },  // U+E619
    { 0, eString },  // U+E61A
    { 0, eString },  // U+E61B
    { 0, eString },  // U+E61C
    { 0, eString },  // U+E61D
    { 0, eString },  // U+E61E
    { 0, eString },  // U+E61F
    { 0, eString },  // U+E620
    { 0, eString },  // U+E621
    { 0, eString },  // U+E622
    { 0, eString },  // U+E623
    { 0, eString },  // U+E624
    { 0, eString },  // U+E625
    { 0, eString },  // U+E626
    { 0, eString },  // U+E627
    { 0, eString },  // U+E628
    { 0, eString },  // U+E629
    { 0, eString },  // U+E62A
    { 0, eString },  // U+E62B
    { 0, eString },  // U+E62C
    { 0, eString },  // U+E62D
    { 0, eString },  // U+E62E
    { 0, eString },  // U+E62F
    { 0, eString },  // U+E630
    { 0, eString },  // U+E631
    { 0, eString },  // U+E632
    { 0, eString },  // U+E633
    { 0, eString },  // U+E634
    { 0, eString },  // U+E635
    { 0, eString },  // U+E636
    { 0, eString },  // U+E637
    { 0, eString },  // U+E638
    { 0, eString },  // U+E639
    { 0, eString },  // U+E63A
    { 0, eString },  // U+E63B
    { 0, eString },  // U+E63C
    { 0, eString },  // U+E63D
    { 0, eString },  // U+E63E
    { 0, eString },  // U+E63F
    { 0, eString },  // U+E640
    { 0, eString },  // U+E641
    { 0, eString },  // U+E642
    { 0, eString },  // U+E643
    { 0, eString },  // U+E644
    { 0, eString },  // U+E645
    { 0, eString },  // U+E646
    { 0, eString },  // U+E647
    { 0, eString },  // U+E648
    { 0, eString },  // U+E649
    { 0, eString },  // U+E64A
    { 0, eString },  // U+E64B
    { 0, eString },  // U+E64C
    { 0, eString },  // U+E64D
    { 0, eString },  // U+E64E
    { 0, eString },  // U+E64F
    { 0, eString },  // U+E650
    { 0, eString },  // U+E651
    { 0, eString },  // U+E652
    { 0, eString },  // U+E653
    { 0, eString },  // U+E654
    { 0, eString },  // U+E655
    { 0, eString },  // U+E656
    { 0, eString },  // U+E657
    { 0, eString },  // U+E658
    { 0, eString },  // U+E659
    { 0, eString },  // U+E65A
    { 0, eString },  // U+E65B
    { 0, eString },  // U+E65C
    { 0, eString },  // U+E65D
    { 0, eString },  // U+E65E
    { 0, eString },  // U+E65F
    { 0, eString },  // U+E660
    { 0, eString },  // U+E661
    { 0, eString },  // U+E662
    { 0, eString },  // U+E663
    { 0, eString },  // U+E664
    { 0, eString },  // U+E665
    { 0, eString },  // U+E666
    { 0, eString },  // U+E667
    { 0, eString },  // U+E668
    { 0, eString },  // U+E669
    { 0, eString },  // U+E66A
    { 0, eString },  // U+E66B
    { 0, eString },  // U+E66C
    { 0, eString },  // U+E66D
    { 0, eString },  // U+E66E
    { 0, eString },  // U+E66F
    { 0, eString },  // U+E670
    { 0, eString },  // U+E671
    { 0, eString },  // U+E672
    { 0, eString },  // U+E673
    { 0, eString },  // U+E674
    { 0, eString },  // U+E675
    { 0, eString },  // U+E676
    { 0, eString },  // U+E677
    { 0, eString },  // U+E678
    { 0, eString },  // U+E679
    { 0, eString },  // U+E67A
    { 0, eString },  // U+E67B
    { 0, eString },  // U+E67C
    { 0, eString },  // U+E67D
    { 0, eString },  // U+E67E
    { 0, eString },  // U+E67F
    { 0, eString },  // U+E680
    { 0, eString },  // U+E681
    { 0, eString },  // U+E682
    { 0, eString },  // U+E683
    { 0, eString },  // U+E684
    { 0, eString },  // U+E685
    { 0, eString },  // U+E686
    { 0, eString },  // U+E687
    { 0, eString },  // U+E688
    { 0, eString },  // U+E689
    { 0, eString },  // U+E68A
    { 0, eString },  // U+E68B
    { 0, eString },  // U+E68C
    { 0, eString },  // U+E68D
    { 0, eString },  // U+E68E
    { 0, eString },  // U+E68F
    { 0, eString },  // U+E690
    { 0, eString },  // U+E691
    { 0, eString },  // U+E692
    { 0, eString },  // U+E693
    { 0, eString },  // U+E694
    { 0, eString },  // U+E695
    { 0, eString },  // U+E696
    { 0, eString },  // U+E697
    { 0, eString },  // U+E698
    { 0, eString },  // U+E699
    { 0, eString },  // U+E69A
    { 0, eString },  // U+E69B
    { 0, eString },  // U+E69C
    { 0, eString },  // U+E69D
    { 0, eString },  // U+E69E
    { 0, eString },  // U+E69F
    { 0, eString },  // U+E6A0
    { 0, eString },  // U+E6A1
    { 0, eString },  // U+E6A2
    { 0, eString },  // U+E6A3
    { 0, eString },  // U+E6A4
    { 0, eString },  // U+E6A5
    { 0, eString },  // U+E6A6
    { 0, eString },  // U+E6A7
    { 0, eString },  // U+E6A8
    { 0, eString },  // U+E6A9
    { 0, eString },  // U+E6AA
    { 0, eString },  // U+E6AB
    { 0, eString },  // U+E6AC
    { 0, eString },  // U+E6AD
    { 0, eString },  // U+E6AE
    { 0, eString },  // U+E6AF
    { 0, eString },  // U+E6B0
    { 0, eString },  // U+E6B1
    { 0, eString },  // U+E6B2
    { 0, eString },  // U+E6B3
    { 0, eString },  // U+E6B4
    { 0, eString },  // U+E6B5
    { 0, eString },  // U+E6B6
    { 0, eString },  // U+E6B7
    { 0, eString },  // U+E6B8
    { 0, eString },  // U+E6B9
    { 0, eString },  // U+E6BA
    { 0, eString },  // U+E6BB
    { 0, eString },  // U+E6BC
    { 0, eString },  // U+E6BD
    { 0, eString },  // U+E6BE
    { 0, eString },  // U+E6BF
    { 0, eString },  // U+E6C0
    { 0, eString },  // U+E6C1
    { 0, eString },  // U+E6C2
    { 0, eString },  // U+E6C3
    { 0, eString },  // U+E6C4
    { 0, eString },  // U+E6C5
    { 0, eString },  // U+E6C6
    { 0, eString },  // U+E6C7
    { 0, eString },  // U+E6C8
    { 0, eString },  // U+E6C9
    { 0, eString },  // U+E6CA
    { 0, eString },  // U+E6CB
    { 0, eString },  // U+E6CC
    { 0, eString },  // U+E6CD
    { 0, eString },  // U+E6CE
    { 0, eString },  // U+E6CF
    { 0, eString },  // U+E6D0
    { 0, eString },  // U+E6D1
    { 0, eString },  // U+E6D2
    { 0, eString },  // U+E6D3
    { 0, eString },  // U+E6D4
    { 0, eString },  // U+E6D5
    { 0, eString },  // U+E6D6
    { 0, eString },  // U+E6D7
    { 0, eString },  // U+E6D8
    { 0, eString },  // U+E6D9
    { 0, eString },  // U+E6DA
    { 0, eString },  // U+E6DB
    { 0, eString },  // U+E6DC
    { 0, eString },  // U+E6DD
    { 0, eString },  // U+E6DE
    { 0, eString },  // U+E6DF
    { 0, eString },  // U+E6E0
    { 0, eString },  // U+E6E1
    { 0, eString },  // U+E6E2
    { 0, eString },  // U+E6E3
    { 0, eString },  // U+E6E4
    { 0, eString },  // U+E6E5
    { 0, eString },  // U+E6E6
    { 0, eString },  // U+E6E7
    { 0, eString },  // U+E6E8
    { 0, eString },  // U+E6E9
    { 0, eString },  // U+E6EA
    { 0, eString },  // U+E6EB
    { 0, eString },  // U+E6EC
    { 0, eString },  // U+E6ED
    { 0, eString },  // U+E6EE
    { 0, eString },  // U+E6EF
    { 0, eString },  // U+E6F0
    { 0, eString },  // U+E6F1
    { 0, eString },  // U+E6F2
    { 0, eString },  // U+E6F3
    { 0, eString },  // U+E6F4
    { 0, eString },  // U+E6F5
    { 0, eString },  // U+E6F6
    { 0, eString },  // U+E6F7
    { 0, eString },  // U+E6F8
    { 0, eString },  // U+E6F9
    { 0, eString },  // U+E6FA
    { 0, eString },  // U+E6FB
    { 0, eString },  // U+E6FC
    { 0, eString },  // U+E6FD
    { 0, eString },  // U+E6FE
    { 0, eString },  // U+E6FF
};
static TUnicodePlan s_Plan_E7h = {
    { 0, eString },  // U+E700
    { 0, eString },  // U+E701
    { 0, eString },  // U+E702
    { 0, eString },  // U+E703
    { 0, eString },  // U+E704
    { 0, eString },  // U+E705
    { 0, eString },  // U+E706
    { 0, eString },  // U+E707
    { 0, eString },  // U+E708
    { 0, eString },  // U+E709
    { 0, eString },  // U+E70A
    { 0, eString },  // U+E70B
    { 0, eString },  // U+E70C
    { 0, eString },  // U+E70D
    { 0, eString },  // U+E70E
    { 0, eString },  // U+E70F
    { 0, eString },  // U+E710
    { 0, eString },  // U+E711
    { 0, eString },  // U+E712
    { 0, eString },  // U+E713
    { 0, eString },  // U+E714
    { 0, eString },  // U+E715
    { 0, eString },  // U+E716
    { 0, eString },  // U+E717
    { 0, eString },  // U+E718
    { 0, eString },  // U+E719
    { 0, eString },  // U+E71A
    { 0, eString },  // U+E71B
    { 0, eString },  // U+E71C
    { 0, eString },  // U+E71D
    { 0, eString },  // U+E71E
    { 0, eString },  // U+E71F
    { 0, eString },  // U+E720
    { 0, eString },  // U+E721
    { 0, eString },  // U+E722
    { 0, eString },  // U+E723
    { 0, eString },  // U+E724
    { 0, eString },  // U+E725
    { 0, eString },  // U+E726
    { 0, eString },  // U+E727
    { 0, eString },  // U+E728
    { 0, eString },  // U+E729
    { 0, eString },  // U+E72A
    { 0, eString },  // U+E72B
    { 0, eString },  // U+E72C
    { 0, eString },  // U+E72D
    { 0, eString },  // U+E72E
    { 0, eString },  // U+E72F
    { 0, eString },  // U+E730
    { 0, eString },  // U+E731
    { 0, eString },  // U+E732
    { 0, eString },  // U+E733
    { 0, eString },  // U+E734
    { 0, eString },  // U+E735
    { 0, eString },  // U+E736
    { 0, eString },  // U+E737
    { 0, eString },  // U+E738
    { 0, eString },  // U+E739
    { 0, eString },  // U+E73A
    { 0, eString },  // U+E73B
    { 0, eString },  // U+E73C
    { 0, eString },  // U+E73D
    { 0, eString },  // U+E73E
    { 0, eString },  // U+E73F
    { 0, eString },  // U+E740
    { 0, eString },  // U+E741
    { 0, eString },  // U+E742
    { 0, eString },  // U+E743
    { 0, eString },  // U+E744
    { 0, eString },  // U+E745
    { 0, eString },  // U+E746
    { 0, eString },  // U+E747
    { 0, eString },  // U+E748
    { 0, eString },  // U+E749
    { 0, eString },  // U+E74A
    { 0, eString },  // U+E74B
    { 0, eString },  // U+E74C
    { 0, eString },  // U+E74D
    { 0, eString },  // U+E74E
    { 0, eString },  // U+E74F
    { 0, eString },  // U+E750
    { 0, eString },  // U+E751
    { 0, eString },  // U+E752
    { 0, eString },  // U+E753
    { 0, eString },  // U+E754
    { 0, eString },  // U+E755
    { 0, eString },  // U+E756
    { 0, eString },  // U+E757
    { 0, eString },  // U+E758
    { 0, eString },  // U+E759
    { 0, eString },  // U+E75A
    { 0, eString },  // U+E75B
    { 0, eString },  // U+E75C
    { 0, eString },  // U+E75D
    { 0, eString },  // U+E75E
    { 0, eString },  // U+E75F
    { 0, eString },  // U+E760
    { 0, eString },  // U+E761
    { 0, eString },  // U+E762
    { 0, eString },  // U+E763
    { 0, eString },  // U+E764
    { 0, eString },  // U+E765
    { 0, eString },  // U+E766
    { 0, eString },  // U+E767
    { 0, eString },  // U+E768
    { 0, eString },  // U+E769
    { 0, eString },  // U+E76A
    { 0, eString },  // U+E76B
    { 0, eString },  // U+E76C
    { 0, eString },  // U+E76D
    { 0, eString },  // U+E76E
    { 0, eString },  // U+E76F
    { 0, eString },  // U+E770
    { 0, eString },  // U+E771
    { 0, eString },  // U+E772
    { 0, eString },  // U+E773
    { 0, eString },  // U+E774
    { 0, eString },  // U+E775
    { 0, eString },  // U+E776
    { 0, eString },  // U+E777
    { 0, eString },  // U+E778
    { 0, eString },  // U+E779
    { 0, eString },  // U+E77A
    { 0, eString },  // U+E77B
    { 0, eString },  // U+E77C
    { 0, eString },  // U+E77D
    { 0, eString },  // U+E77E
    { 0, eString },  // U+E77F
    { 0, eString },  // U+E780
    { 0, eString },  // U+E781
    { 0, eString },  // U+E782
    { 0, eString },  // U+E783
    { 0, eString },  // U+E784
    { 0, eString },  // U+E785
    { 0, eString },  // U+E786
    { 0, eString },  // U+E787
    { 0, eString },  // U+E788
    { 0, eString },  // U+E789
    { 0, eString },  // U+E78A
    { 0, eString },  // U+E78B
    { 0, eString },  // U+E78C
    { 0, eString },  // U+E78D
    { 0, eString },  // U+E78E
    { 0, eString },  // U+E78F
    { 0, eString },  // U+E790
    { 0, eString },  // U+E791
    { 0, eString },  // U+E792
    { 0, eString },  // U+E793
    { 0, eString },  // U+E794
    { 0, eString },  // U+E795
    { 0, eString },  // U+E796
    { 0, eString },  // U+E797
    { 0, eString },  // U+E798
    { 0, eString },  // U+E799
    { 0, eString },  // U+E79A
    { 0, eString },  // U+E79B
    { 0, eString },  // U+E79C
    { 0, eString },  // U+E79D
    { 0, eString },  // U+E79E
    { 0, eString },  // U+E79F
    { 0, eString },  // U+E7A0
    { 0, eString },  // U+E7A1
    { 0, eString },  // U+E7A2
    { 0, eString },  // U+E7A3
    { 0, eString },  // U+E7A4
    { 0, eString },  // U+E7A5
    { 0, eString },  // U+E7A6
    { 0, eString },  // U+E7A7
    { 0, eString },  // U+E7A8
    { 0, eString },  // U+E7A9
    { 0, eString },  // U+E7AA
    { 0, eString },  // U+E7AB
    { 0, eString },  // U+E7AC
    { 0, eString },  // U+E7AD
    { 0, eString },  // U+E7AE
    { 0, eString },  // U+E7AF
    { 0, eString },  // U+E7B0
    { 0, eString },  // U+E7B1
    { 0, eString },  // U+E7B2
    { 0, eString },  // U+E7B3
    { 0, eString },  // U+E7B4
    { 0, eString },  // U+E7B5
    { 0, eString },  // U+E7B6
    { 0, eString },  // U+E7B7
    { 0, eString },  // U+E7B8
    { 0, eString },  // U+E7B9
    { 0, eString },  // U+E7BA
    { 0, eString },  // U+E7BB
    { 0, eString },  // U+E7BC
    { 0, eString },  // U+E7BD
    { 0, eString },  // U+E7BE
    { 0, eString },  // U+E7BF
    { 0, eString },  // U+E7C0
    { 0, eString },  // U+E7C1
    { 0, eString },  // U+E7C2
    { 0, eString },  // U+E7C3
    { 0, eString },  // U+E7C4
    { 0, eString },  // U+E7C5
    { 0, eString },  // U+E7C6
    { 0, eString },  // U+E7C7
    { 0, eString },  // U+E7C8
    { 0, eString },  // U+E7C9
    { 0, eString },  // U+E7CA
    { 0, eString },  // U+E7CB
    { 0, eString },  // U+E7CC
    { 0, eString },  // U+E7CD
    { 0, eString },  // U+E7CE
    { 0, eString },  // U+E7CF
    { 0, eString },  // U+E7D0
    { 0, eString },  // U+E7D1
    { 0, eString },  // U+E7D2
    { 0, eString },  // U+E7D3
    { 0, eString },  // U+E7D4
    { 0, eString },  // U+E7D5
    { 0, eString },  // U+E7D6
    { 0, eString },  // U+E7D7
    { 0, eString },  // U+E7D8
    { 0, eString },  // U+E7D9
    { 0, eString },  // U+E7DA
    { 0, eString },  // U+E7DB
    { 0, eString },  // U+E7DC
    { 0, eString },  // U+E7DD
    { 0, eString },  // U+E7DE
    { 0, eString },  // U+E7DF
    { 0, eString },  // U+E7E0
    { 0, eString },  // U+E7E1
    { 0, eString },  // U+E7E2
    { 0, eString },  // U+E7E3
    { 0, eString },  // U+E7E4
    { 0, eString },  // U+E7E5
    { 0, eString },  // U+E7E6
    { 0, eString },  // U+E7E7
    { 0, eString },  // U+E7E8
    { 0, eString },  // U+E7E9
    { 0, eString },  // U+E7EA
    { 0, eString },  // U+E7EB
    { 0, eString },  // U+E7EC
    { 0, eString },  // U+E7ED
    { 0, eString },  // U+E7EE
    { 0, eString },  // U+E7EF
    { 0, eString },  // U+E7F0
    { 0, eString },  // U+E7F1
    { 0, eString },  // U+E7F2
    { 0, eString },  // U+E7F3
    { 0, eString },  // U+E7F4
    { 0, eString },  // U+E7F5
    { 0, eString },  // U+E7F6
    { 0, eString },  // U+E7F7
    { 0, eString },  // U+E7F8
    { 0, eString },  // U+E7F9
    { 0, eString },  // U+E7FA
    { 0, eString },  // U+E7FB
    { 0, eString },  // U+E7FC
    { 0, eString },  // U+E7FD
    { 0, eString },  // U+E7FE
    { 0, eString },  // U+E7FF
};
static TUnicodePlan s_Plan_E8h = {
    { " such that", eString },  // old dictionary      U+E800
    { 0, eString },  //                             U+E801
    { 0, eString },  //                             U+E802
    { 0, eString },  //                             U+E803
    { 0, eString },  //                             U+E804
    { 0, eString },  //                             U+E805
    { 0, eString },  //                             U+E806
    { 0, eString },  //                             U+E807
    { 0, eString },  //                             U+E808
    { 0, eString },  //                             U+E809
    { 0, eString },  //                             U+E80A
    { " black lozenge", eString },  // old dictionary  U+E80B
    { 0, eString },  //                             U+E80C
    { 0, eString },  //                             U+E80D
    { 0, eString },  //                             U+E80E
    { 0, eString },  //                             U+E80F
    { 0, eString },  //                             U+E810
    { 0, eString },  //                             U+E811
    { 0, eString },  //                             U+E812
    { 0, eString },  //                             U+E813
    { 0, eString },  //                             U+E814
    { 0, eString },  //                             U+E815
    { 0, eString },  //                             U+E816
    { 0, eString },  //                             U+E817
    { 0, eString },  //                             U+E818
    { 0, eString },  //                             U+E819
    { 0, eString },  //                             U+E81A
    { 0, eString },  //                             U+E81B
    { 0, eString },  //                             U+E81C
    { 0, eString },  //                             U+E81D
    { 0, eString },  //                             U+E81E
    { 0, eString },  //                             U+E81F
    { 0, eString },  //                             U+E820
    { 0, eString },  //                             U+E821
    { 0, eString },  //                             U+E822
    { 0, eString },  //                             U+E823
    { 0, eString },  //                             U+E824
    { 0, eString },  //                             U+E825
    { 0, eString },  //                             U+E826
    { 0, eString },  //                             U+E827
    { 0, eString },  //                             U+E828
    { 0, eString },  //                             U+E829
    { 0, eString },  //                             U+E82A
    { 0, eString },  //                             U+E82B
    { 0, eString },  //                             U+E82C
    { 0, eString },  //                             U+E82D
    { 0, eString },  //                             U+E82E
    { 0, eString },  //                             U+E82F
    { 0, eString },  //                             U+E830
    { 0, eString },  //                             U+E831
    { 0, eString },  //                             U+E832
    { 0, eString },  //                             U+E833
    { 0, eString },  //                             U+E834
    { 0, eString },  //                             U+E835
    { 0, eString },  //                             U+E836
    { 0, eString },  //                             U+E837
    { "circle with vertical bar", eString },  //       U+E838
    { 0, eString },  //                             U+E839
    { 0, eString },  //                             U+E83A
    { 0, eString },  //                             U+E83B
    { 0, eString },  //                             U+E83C
    { 0, eString },  //                             U+E83D
    { 0, eString },  //                             U+E83E
    { 0, eString },  //                             U+E83F
    { 0, eString },  //                             U+E840
    { 0, eString },  //                             U+E841
    { 0, eString },  //                             U+E842
    { 0, eString },  //                             U+E843
    { "\\", eString },  // old dictionary              U+E844
    { 0, eString },  //                             U+E845
    { 0, eString },  //                             U+E846
    { 0, eString },  //                             U+E847
    { 0, eString },  //                             U+E848
    { 0, eString },  //                             U+E849
    { 0, eString },  //                             U+E84A
    { 0, eString },  //                             U+E84B
    { 0, eString },  //                             U+E84C
    { 0, eString },  //                             U+E84D
    { 0, eString },  //                             U+E84E
    { 0, eString },  //                             U+E84F
    { 0, eString },  //                             U+E850
    { 0, eString },  //                             U+E851
    { 0, eString },  //                             U+E852
    { 0, eString },  //                             U+E853
    { 0, eString },  //                             U+E854
    { 0, eString },  //                             U+E855
    { 0, eString },  //                             U+E856
    { 0, eString },  //                             U+E857
    { 0, eString },  //                             U+E858
    { 0, eString },  //                             U+E859
    { 0, eString },  //                             U+E85A
    { 0, eString },  //                             U+E85B
    { 0, eString },  //                             U+E85C
    { 0, eString },  //                             U+E85D
    { 0, eString },  //                             U+E85E
    { 0, eString },  //                             U+E85F
    { 0, eString },  //                             U+E860
    { 0, eString },  //                             U+E861
    { 0, eString },  //                             U+E862
    { 0, eString },  //                             U+E863
    { 0, eString },  //                             U+E864
    { 0, eString },  //                             U+E865
    { 0, eString },  //                             U+E866
    { 0, eString },  //                             U+E867
    { 0, eString },  //                             U+E868
    { 0, eString },  //                             U+E869
    { 0, eString },  //                             U+E86A
    { 0, eString },  //                             U+E86B
    { 0, eString },  //                             U+E86C
    { 0, eString },  //                             U+E86D
    { 0, eString },  //                             U+E86E
    { 0, eString },  //                             U+E86F
    { 0, eString },  //                             U+E870
    { 0, eString },  //                             U+E871
    { 0, eString },  //                             U+E872
    { 0, eString },  //                             U+E873
    { 0, eString },  //                             U+E874
    { 0, eString },  //                             U+E875
    { 0, eString },  //                             U+E876
    { 0, eString },  //                             U+E877
    { 0, eString },  //                             U+E878
    { 0, eString },  //                             U+E879
    { 0, eString },  //                             U+E87A
    { 0, eString },  //                             U+E87B
    { 0, eString },  //                             U+E87C
    { 0, eString },  //                             U+E87D
    { 0, eString },  //                             U+E87E
    { 0, eString },  //                             U+E87F
    { 0, eString },  //                             U+E880
    { 0, eString },  //                             U+E881
    { 0, eString },  //                             U+E882
    { 0, eString },  //                             U+E883
    { 0, eString },  //                             U+E884
    { 0, eString },  //                             U+E885
    { 0, eString },  //                             U+E886
    { 0, eString },  //                             U+E887
    { 0, eString },  //                             U+E888
    { 0, eString },  //                             U+E889
    { 0, eString },  //                             U+E88A
    { 0, eString },  //                             U+E88B
    { 0, eString },  //                             U+E88C
    { 0, eString },  //                             U+E88D
    { 0, eString },  //                             U+E88E
    { 0, eString },  //                             U+E88F
    { 0, eString },  //                             U+E890
    { 0, eString },  //                             U+E891
    { 0, eString },  //                             U+E892
    { 0, eString },  //                             U+E893
    { 0, eString },  //                             U+E894
    { 0, eString },  //                             U+E895
    { 0, eString },  //                             U+E896
    { 0, eString },  //                             U+E897
    { 0, eString },  //                             U+E898
    { 0, eString },  //                             U+E899
    { 0, eString },  //                             U+E89A
    { 0, eString },  //                             U+E89B
    { 0, eString },  //                             U+E89C
    { 0, eString },  //                             U+E89D
    { 0, eString },  //                             U+E89E
    { 0, eString },  //                             U+E89F
    { 0, eString },  //                             U+E8A0
    { 0, eString },  //                             U+E8A1
    { 0, eString },  //                             U+E8A2
    { 0, eString },  //                             U+E8A3
    { 0, eString },  //                             U+E8A4
    { 0, eString },  //                             U+E8A5
    { 0, eString },  //                             U+E8A6
    { 0, eString },  //                             U+E8A7
    { 0, eString },  //                             U+E8A8
    { 0, eString },  //                             U+E8A9
    { 0, eString },  //                             U+E8AA
    { 0, eString },  //                             U+E8AB
    { 0, eString },  //                             U+E8AC
    { 0, eString },  //                             U+E8AD
    { 0, eString },  //                             U+E8AE
    { 0, eString },  //                             U+E8AF
    { 0, eString },  //                             U+E8B0
    { 0, eString },  //                             U+E8B1
    { 0, eString },  //                             U+E8B2
    { 0, eString },  //                             U+E8B3
    { 0, eString },  //                             U+E8B4
    { 0, eString },  //                             U+E8B5
    { 0, eString },  //                             U+E8B6
    { 0, eString },  //                             U+E8B7
    { 0, eString },  //                             U+E8B8
    { 0, eString },  //                             U+E8B9
    { 0, eString },  //                             U+E8BA
    { 0, eString },  //                             U+E8BB
    { 0, eString },  //                             U+E8BC
    { 0, eString },  //                             U+E8BD
    { 0, eString },  //                             U+E8BE
    { 0, eString },  //                             U+E8BF
    { 0, eString },  //                             U+E8C0
    { 0, eString },  //                             U+E8C1
    { 0, eString },  //                             U+E8C2
    { 0, eString },  //                             U+E8C3
    { 0, eString },  //                             U+E8C4
    { 0, eString },  //                             U+E8C5
    { 0, eString },  //                             U+E8C6
    { 0, eString },  //                             U+E8C7
    { 0, eString },  //                             U+E8C8
    { 0, eString },  //                             U+E8C9
    { 0, eString },  //                             U+E8CA
    { 0, eString },  //                             U+E8CB
    { 0, eString },  //                             U+E8CC
    { 0, eString },  //                             U+E8CD
    { 0, eString },  //                             U+E8CE
    { 0, eString },  //                             U+E8CF
    { 0, eString },  //                             U+E8D0
    { 0, eString },  //                             U+E8D1
    { 0, eString },  //                             U+E8D2
    { 0, eString },  //                             U+E8D3
    { 0, eString },  //                             U+E8D4
    { 0, eString },  //                             U+E8D5
    { 0, eString },  //                             U+E8D6
    { 0, eString },  //                             U+E8D7
    { 0, eString },  //                             U+E8D8
    { 0, eString },  //                             U+E8D9
    { 0, eString },  //                             U+E8DA
    { 0, eString },  //                             U+E8DB
    { 0, eString },  //                             U+E8DC
    { 0, eString },  //                             U+E8DD
    { 0, eString },  //                             U+E8DE
    { 0, eString },  //                             U+E8DF
    { 0, eString },  //                             U+E8E0
    { 0, eString },  //                             U+E8E1
    { 0, eString },  //                             U+E8E2
    { 0, eString },  //                             U+E8E3
    { 0, eString },  //                             U+E8E4
    { 0, eString },  //                             U+E8E5
    { 0, eString },  //                             U+E8E6
    { 0, eString },  //                             U+E8E7
    { 0, eString },  //                             U+E8E8
    { 0, eString },  //                             U+E8E9
    { 0, eString },  //                             U+E8EA
    { 0, eString },  //                             U+E8EB
    { 0, eString },  //                             U+E8EC
    { 0, eString },  //                             U+E8ED
    { 0, eString },  //                             U+E8EE
    { 0, eString },  //                             U+E8EF
    { 0, eString },  //                             U+E8F0
    { 0, eString },  //                             U+E8F1
    { 0, eString },  //                             U+E8F2
    { 0, eString },  //                             U+E8F3
    { 0, eString },  //                             U+E8F4
    { 0, eString },  //                             U+E8F5
    { 0, eString },  //                             U+E8F6
    { 0, eString },  //                             U+E8F7
    { 0, eString },  //                             U+E8F8
    { 0, eString },  //                             U+E8F9
    { 0, eString },  //                             U+E8FA
    { 0, eString },  //                             U+E8FB
    { 0, eString },  //                             U+E8FC
    { 0, eString },  //                             U+E8FD
    { 0, eString },  //                             U+E8FE
    { 0, eString },  //                             U+E8FF
};
static TUnicodePlan s_Plan_EAh = {
    { "1", eString },  //                U+EA00
    { "1", eString },  //                U+EA01
    { "2", eString },  //                U+EA02
    { "2", eString },  //                U+EA03
    { "3", eString },  //                U+EA04
    { "3", eString },  //                U+EA05
    { "4", eString },  //                U+EA06
    { "a", eString },  //                U+EA07
    { "a", eString },  //                U+EA08
    { "A", eString },  //                U+EA09
    { "a", eString },  //                U+EA0A
    { "A", eString },  //                U+EA0B
    { "AB", eString },  //               U+EA0C
    { "alpha", eString },  //            U+EA0D
    { "alpha", eString },  //            U+EA0E
    { "alpha", eString },  //            U+EA0F
    { "b", eString },  //                U+EA10
    { "B", eString },  //                U+EA11
    { "b", eString },  //                U+EA12
    { "B", eString },  //                U+EA13
    { "B", eString },  //                U+EA14
    { "B", eString },  //                U+EA15
    { "BC", eString },  //               U+EA16
    { "beta", eString },  //             U+EA17
    { "beta", eString },  //             U+EA18
    { "c", eString },  //                U+EA19
    { "c", eString },  //                U+EA1A
    { "c", eString },  //                U+EA1B
    { "C", eString },  //                U+EA1C
    { "c", eString },  //                U+EA1D
    { "c", eString },  //                U+EA1E
    { "c", eString },  //                U+EA1F
    { "c", eString },  //                U+EA20
    { "c", eString },  //                U+EA21
    { "chi", eString },  //              U+EA22
    { "chi", eString },  //              U+EA23
    { "chi", eString },  //              U+EA24
    { "d", eString },  //                U+EA25
    { "D", eString },  //                U+EA26
    { "d", eString },  //                U+EA27
    { "D", eString },  //                U+EA28
    { "d", eString },  //                U+EA29
    { "D", eString },  //                U+EA2A
    { "D", eString },  //                U+EA2B
    { "Delta", eString },  //            U+EA2C
    { "delta", eString },  //            U+EA2D
    { "delta", eString },  //            U+EA2E
    { "dl", eString },  //               U+EA2F
    { "e", eString },  //                U+EA30
    { "e", eString },  //                U+EA31
    { "l", eString },  //                U+EA32
    { "l", eString },  //                U+EA33
    { "f", eString },  //                U+EA34
    { "f", eString },  //                U+EA35
    { "F", eString },  //                U+EA36
    { "f", eString },  //                U+EA37
    { "F", eString },  //                U+EA38
    { "f", eString },  //                U+EA39
    { "F", eString },  //                U+EA3A
    { "sigma", eString },  //            U+EA3B
    { "sigma", eString },  //            U+EA3C
    { "g", eString },  //                U+EA3D
    { "g", eString },  //                U+EA3E
    { "G", eString },  //                U+EA3F
    { "g", eString },  //                U+EA40
    { "G", eString },  //                U+EA41
    { "Gamma", eString },  //            U+EA42
    { "gamma", eString },  //            U+EA43
    { "gamma", eString },  //            U+EA44
    { "gamma", eString },  //            U+EA45
    { "H", eString },  //                U+EA46
    { "h", eString },  //                U+EA47
    { "h", eString },  //                U+EA48
    { "i", eString },  //                U+EA49
    { "I", eString },  //                U+EA4A
    { "j", eString },  //                U+EA4B
    { "J", eString },  //                U+EA4C
    { "J", eString },  //                U+EA4D
    { "J", eString },  //                U+EA4E
    { "k", eString },  //                U+EA4F
    { "k", eString },  //                U+EA50
    { "k", eString },  //                U+EA51
    { "K", eString },  //                U+EA52
    { "K", eString },  //                U+EA53
    { "k", eString },  //                U+EA54
    { "l", eString },  //                U+EA55
    { "L", eString },  //                U+EA56
    { "l", eString },  //                U+EA57
    { "lambda", eString },  //           U+EA58
    { "lambda", eString },  //           U+EA59
    { "Lambda", eString },  //           U+EA5A
    { "lnV", eString },  //              U+EA5B
    { "m", eString },  //                U+EA5C
    { "M", eString },  //                U+EA5D
    { "m", eString },  //                U+EA5E
    { "m", eString },  //                U+EA5F
    { "mu", eString },  //               U+EA60
    { "mu", eString },  //               U+EA61
    { "mu", eString },  //               U+EA62
    { "n", eString },  //                U+EA63
    { "n", eString },  //                U+EA64
    { "N", eString },  //                U+EA65
    { "n", eString },  //                U+EA66
    { "N", eString },  //                U+EA67
    { "n", eString },  //                U+EA68
    { "n", eString },  //                U+EA69
    { "N", eString },  //                U+EA6A
    { "nabla", eString },  //            U+EA6B
    { "nu", eString },  //               U+EA6C
    { "nv", eString },  //               U+EA6D
    { "O", eString },  //                U+EA6E
    { "O", eString },  //                U+EA6F
    { "O", eString },  //                U+EA70
    { "omega", eString },  //            U+EA71
    { "F", eString },  //                U+EA72
    { "p", eString },  //                U+EA73
    { "P", eString },  //                U+EA74
    { "p", eString },  //                U+EA75
    { "P", eString },  //                U+EA76
    { "p", eString },  //                U+EA77
    { "P", eString },  //                U+EA78
    { "p", eString },  //                U+EA79
    { "P", eString },  //                U+EA7A
    { "phi", eString },  //              U+EA7B
    { "phi", eString },  //              U+EA7C
    { "Phi", eString },  //              U+EA7D
    { "phi", eString },  //              U+EA7E
    { "pi", eString },  //               U+EA7F
    { "pi", eString },  //               U+EA80
    { "pi", eString },  //               U+EA81
    { "Pi", eString },  //               U+EA82
    { "pi", eString },  //               U+EA83
    { "pi", eString },  //               U+EA84
    { "pi", eString },  //               U+EA85
    { "pi", eString },  //               U+EA86
    { "psi", eString },  //              U+EA87
    { "psi", eString },  //              U+EA88
    { "psi", eString },  //              U+EA89
    { "Psi", eString },  //              U+EA8A
    { "psi", eString },  //              U+EA8B
    { "Psi", eString },  //              U+EA8C
    { "q", eString },  //                U+EA8D
    { "Q", eString },  //                U+EA8E
    { "q", eString },  //                U+EA8F
    { "q", eString },  //                U+EA90
    { "Q", eString },  //                U+EA91
    { "q", eString },  //                U+EA92
    { "Q", eString },  //                U+EA93
    { "q", eString },  //                U+EA94
    { "q", eString },  //                U+EA95
    { "q", eString },  //                U+EA96
    { "Q", eString },  //                U+EA97
    { "r", eString },  //                U+EA98
    { "R", eString },  //                U+EA99
    { "R", eString },  //                U+EA9A
    { "r", eString },  //                U+EA9B
    { "R", eString },  //                U+EA9C
    { "R", eString },  //                U+EA9D
    { "r", eString },  //                U+EA9E
    { "R", eString },  //                U+EA9F
    { "r", eString },  //                U+EAA0
    { "r", eString },  //                U+EAA1
    { "R", eString },  //                U+EAA2
    { "r", eString },  //                U+EAA3
    { "r1", eString },  //               U+EAA4
    { "RE", eString },  //               U+EAA5
    { "rho", eString },  //              U+EAA6
    { "rho", eString },  //              U+EAA7
    { "rho", eString },  //              U+EAA8
    { "ri", eString },  //               U+EAA9
    { "rj", eString },  //               U+EAAA
    { "rN", eString },  //               U+EAAB
    { "s", eString },  //                U+EAAC
    { "S", eString },  //                U+EAAD
    { "S", eString },  //                U+EAAE
    { "s", eString },  //                U+EAAF
    { "S", eString },  //                U+EAB0
    { "s", eString },  //                U+EAB1
    { "S", eString },  //                U+EAB2
    { "S", eString },  //                U+EAB3
    { "B", eString },  //                U+EAB4
    { "E", eString },  //                U+EAB5
    { "G", eString },  //                U+EAB6
    { "P", eString },  //                U+EAB7
    { "Q", eString },  //                U+EAB8
    { "t", eString },  //                U+EAB9
    { "T", eString },  //                U+EABA
    { "T", eString },  //                U+EABB
    { "t", eString },  //                U+EABC
    { "T", eString },  //                U+EABD
    { "t", eString },  //                U+EABE
    { "T", eString },  //                U+EABF
    { "tau", eString },  //              U+EAC0
    { "tau", eString },  //              U+EAC1
    { "theta", eString },  //            U+EAC2
    { "theta", eString },  //            U+EAC3
    { "times", eString },  //            U+EAC4
    { "TT", eString },  //               U+EAC5
    { "u", eString },  //                U+EAC6
    { "u", eString },  //                U+EAC7
    { "U", eString },  //                U+EAC8
    { "u", eString },  //                U+EAC9
    { "u", eString },  //                U+EACA
    { "upsilon", eString },  //          U+EACB
    { "V", eString },  //                U+EACC
    { "v", eString },  //                U+EACD
    { "v", eString },  //                U+EACE
    { "V", eString },  //                U+EACF
    { "v", eString },  //                U+EAD0
    { "V", eString },  //                U+EAD1
    { "v", eString },  //                U+EAD2
    { "v", eString },  //                U+EAD3
    { "V", eString },  //                U+EAD4
    { "v", eString },  //                U+EAD5
    { "epsilon", eString },  //          U+EAD6
    { "epsilon", eString },  //          U+EAD7
    { "epsilon", eString },  //          U+EAD8
    { "phi", eString },  //              U+EAD9
    { "phi", eString },  //              U+EADA
    { "theta", eString },  //            U+EADB
    { "w", eString },  //                U+EADC
    { "w", eString },  //                U+EADD
    { "w", eString },  //                U+EADE
    { "x", eString },  //                U+EADF
    { "X", eString },  //                U+EAE0
    { "x", eString },  //                U+EAE1
    { "X", eString },  //                U+EAE2
    { "x", eString },  //                U+EAE3
    { "x", eString },  //                U+EAE4
    { "X", eString },  //                U+EAE5
    { "x", eString },  //                U+EAE6
    { "xi", eString },  //               U+EAE7
    { "y", eString },  //                U+EAE8
    { "Y", eString },  //                U+EAE9
    { "y", eString },  //                U+EAEA
    { "Y", eString },  //                U+EAEB
    { "y", eString },  //                U+EAEC
    { "y", eString },  //                U+EAED
    { "z", eString },  //                U+EAEE
    { "Z", eString },  //                U+EAEF
    { "z", eString },  //                U+EAF0
    { "z", eString },  //                U+EAF1
    { "Z", eString },  //                U+EAF2
    { "z", eString },  //                U+EAF3
    { "z", eString },  //                U+EAF4
    { "zeta", eString },  //             U+EAF5
    { "zeta", eString },  //             U+EAF6
    { "xbscrsv", eString },  //          U+EAF7
    { "xbscrtv", eString },  //          U+EAF8
    { "xescr1v", eString },  //          U+EAF9
    { "xescr2v", eString },  //          U+EAFA
    { "ngrmi", eString },  //            U+EAFB
    { "B", eString },  //                U+EAFC
    { 0, eString },  //               U+EAFD
    { "sigma with tilde", eString },  // U+EAFE
    { "A", eString },  //                U+EAFF
};
static TUnicodePlan s_Plan_EBh = {
    { "Lambda", eString },  //                   U+EB00
    { "Lambda with umlaut", eString },  //       U+EB01
    { "down triangle with a dot", eString },  // U+EB02
    { "tau with tilde", eString },  //           U+EB03
    { "w", eString },  //                        U+EB04
    { "m", eString },  //                        U+EB05
    { "M", eString },  //                        U+EB06
    { "E", eString },  //                        U+EB07
    { "W", eString },  //                        U+EB08
    { "kappa", eString },  //                    U+EB09
    { "h", eString },  //                        U+EB0A
    { "n", eString },  //                        U+EB0B
    { "underlined kappa", eString },  //         U+EB0C
    { "H", eString },  //                        U+EB0D
    { "Theta", eString },  //                    U+EB0E
    { "M", eString },  //                        U+EB0F
    { "m", eString },  //                        U+EB10
    { "Z", eString },  //                        U+EB11
    { "g", eString },  //                        U+EB12
    { "partial", eString },  //                  U+EB13
    { "C", eString },  //                        U+EB14
    { "E", eString },  //                        U+EB15
    { "sfgrvect", eString },  //                 U+EB16
    { "bgrvect", eString },  //                  U+EB17
    { "xogrvect", eString },  //                 U+EB18
    { "W", eString },  //                        U+EB19
    { "a", eString },  //                        U+EB1A
    { "b", eString },  //                        U+EB1B
    { "S", eString },  //                        U+EB1C
    { "otimesmac", eString },  //                U+EB1D
    { "J", eString },  //                        U+EB1E
    { "b", eString },  //                        U+EB1F
    { "Z", eString },  //                        U+EB20
    { "L", eString },  //                        U+EB21
    { "g", eString },  //                        U+EB22
    { "Delta with circumflex", eString },  //    U+EB23
    { "omega with tilde", eString },  //         U+EB24
    { "Lambda with circumflex", eString },  //   U+EB25
    { "var phiv with circumflex", eString },  // U+EB26
    { "Delta with tilde", eString },  //         U+EB27
    { "lambda with macron", eString },  //       U+EB28
    { "pi with macron", eString },  //           U+EB29
    { "Q", eString },  //                        U+EB2A
    { "1", eString },  //                        U+EB2B
    { "3", eString },  //                        U+EB2C
    { "E", eString },  //                        U+EB2D
    { "e", eString },  //                        U+EB2E
    { "g", eString },  //                        U+EB2F
    { "j", eString },  //                        U+EB30
    { "raised chempoint", eString },  //         U+EB31
    { "epsilon-vector", eString },  //           U+EB32
    { "l", eString },  //                        U+EB33
    { "M", eString },  //                        U+EB34
    { "l", eString },  //                        U+EB35
    { "A", eString },  //                        U+EB36
    { "phi", eString },  //                      U+EB37
    { "pupsil", eString },  //                   U+EB38
    { "h with umlaut", eString },  //            U+EB39
    { "- - - -", eString },  //                  U+EB3A
    { "F with hat", eString },  //               U+EB3B
    { "rectangle", eString },  //                U+EB3C
    { "bold dash", eString },  //                U+EB3D
    { "diamond", eString },  //                  U+EB3E
    { "R", eString },  //                        U+EB3F
    { "omega", eString },  //                    U+EB40
    { "r", eString },  //                        U+EB41
    { "circle w/criscross", eString },  //       U+EB42
    { "n", eString },  //                        U+EB43
    { "j", eString },  //                        U+EB44
    { "d", eString },  //                        U+EB45
    { "g", eString },  //                        U+EB46
    { "o", eString },  //                        U+EB47
    { "c", eString },  //                        U+EB48
    { "dotted_times", eString },  //             U+EB49
    { "large asterisk", eString },  //           U+EB4A
    { 0, eString },  //                       U+EB4B
    { 0, eString },  //                       U+EB4C
    { 0, eString },  //                       U+EB4D
    { 0, eString },  //                       U+EB4E
    { 0, eString },  //                       U+EB4F
    { 0, eString },  //                       U+EB50
    { 0, eString },  //                       U+EB51
    { 0, eString },  //                       U+EB52
    { 0, eString },  //                       U+EB53
    { 0, eString },  //                       U+EB54
    { 0, eString },  //                       U+EB55
    { 0, eString },  //                       U+EB56
    { 0, eString },  //                       U+EB57
    { 0, eString },  //                       U+EB58
    { 0, eString },  //                       U+EB59
    { 0, eString },  //                       U+EB5A
    { 0, eString },  //                       U+EB5B
    { 0, eString },  //                       U+EB5C
    { 0, eString },  //                       U+EB5D
    { 0, eString },  //                       U+EB5E
    { 0, eString },  //                       U+EB5F
    { 0, eString },  //                       U+EB60
    { 0, eString },  //                       U+EB61
    { 0, eString },  //                       U+EB62
    { 0, eString },  //                       U+EB63
    { 0, eString },  //                       U+EB64
    { 0, eString },  //                       U+EB65
    { 0, eString },  //                       U+EB66
    { 0, eString },  //                       U+EB67
    { 0, eString },  //                       U+EB68
    { 0, eString },  //                       U+EB69
    { 0, eString },  //                       U+EB6A
    { 0, eString },  //                       U+EB6B
    { 0, eString },  //                       U+EB6C
    { 0, eString },  //                       U+EB6D
    { 0, eString },  //                       U+EB6E
    { 0, eString },  //                       U+EB6F
    { 0, eString },  //                       U+EB70
    { 0, eString },  //                       U+EB71
    { 0, eString },  //                       U+EB72
    { 0, eString },  //                       U+EB73
    { 0, eString },  //                       U+EB74
    { 0, eString },  //                       U+EB75
    { 0, eString },  //                       U+EB76
    { 0, eString },  //                       U+EB77
    { 0, eString },  //                       U+EB78
    { 0, eString },  //                       U+EB79
    { 0, eString },  //                       U+EB7A
    { 0, eString },  //                       U+EB7B
    { 0, eString },  //                       U+EB7C
    { 0, eString },  //                       U+EB7D
    { 0, eString },  //                       U+EB7E
    { 0, eString },  //                       U+EB7F
    { 0, eString },  //                       U+EB80
    { 0, eString },  //                       U+EB81
    { 0, eString },  //                       U+EB82
    { 0, eString },  //                       U+EB83
    { 0, eString },  //                       U+EB84
    { 0, eString },  //                       U+EB85
    { 0, eString },  //                       U+EB86
    { 0, eString },  //                       U+EB87
    { 0, eString },  //                       U+EB88
    { 0, eString },  //                       U+EB89
    { 0, eString },  //                       U+EB8A
    { 0, eString },  //                       U+EB8B
    { 0, eString },  //                       U+EB8C
    { 0, eString },  //                       U+EB8D
    { 0, eString },  //                       U+EB8E
    { 0, eString },  //                       U+EB8F
    { 0, eString },  //                       U+EB90
    { 0, eString },  //                       U+EB91
    { 0, eString },  //                       U+EB92
    { 0, eString },  //                       U+EB93
    { 0, eString },  //                       U+EB94
    { 0, eString },  //                       U+EB95
    { 0, eString },  //                       U+EB96
    { 0, eString },  //                       U+EB97
    { 0, eString },  //                       U+EB98
    { 0, eString },  //                       U+EB99
    { 0, eString },  //                       U+EB9A
    { 0, eString },  //                       U+EB9B
    { 0, eString },  //                       U+EB9C
    { 0, eString },  //                       U+EB9D
    { 0, eString },  //                       U+EB9E
    { 0, eString },  //                       U+EB9F
    { 0, eString },  //                       U+EBA0
    { 0, eString },  //                       U+EBA1
    { 0, eString },  //                       U+EBA2
    { 0, eString },  //                       U+EBA3
    { 0, eString },  //                       U+EBA4
    { 0, eString },  //                       U+EBA5
    { 0, eString },  //                       U+EBA6
    { 0, eString },  //                       U+EBA7
    { 0, eString },  //                       U+EBA8
    { 0, eString },  //                       U+EBA9
    { 0, eString },  //                       U+EBAA
    { 0, eString },  //                       U+EBAB
    { 0, eString },  //                       U+EBAC
    { 0, eString },  //                       U+EBAD
    { 0, eString },  //                       U+EBAE
    { 0, eString },  //                       U+EBAF
    { 0, eString },  //                       U+EBB0
    { 0, eString },  //                       U+EBB1
    { 0, eString },  //                       U+EBB2
    { 0, eString },  //                       U+EBB3
    { 0, eString },  //                       U+EBB4
    { 0, eString },  //                       U+EBB5
    { 0, eString },  //                       U+EBB6
    { 0, eString },  //                       U+EBB7
    { 0, eString },  //                       U+EBB8
    { 0, eString },  //                       U+EBB9
    { 0, eString },  //                       U+EBBA
    { 0, eString },  //                       U+EBBB
    { 0, eString },  //                       U+EBBC
    { 0, eString },  //                       U+EBBD
    { 0, eString },  //                       U+EBBE
    { 0, eString },  //                       U+EBBF
    { 0, eString },  //                       U+EBC0
    { 0, eString },  //                       U+EBC1
    { 0, eString },  //                       U+EBC2
    { 0, eString },  //                       U+EBC3
    { 0, eString },  //                       U+EBC4
    { 0, eString },  //                       U+EBC5
    { 0, eString },  //                       U+EBC6
    { 0, eString },  //                       U+EBC7
    { 0, eString },  //                       U+EBC8
    { 0, eString },  //                       U+EBC9
    { 0, eString },  //                       U+EBCA
    { 0, eString },  //                       U+EBCB
    { 0, eString },  //                       U+EBCC
    { 0, eString },  //                       U+EBCD
    { 0, eString },  //                       U+EBCE
    { 0, eString },  //                       U+EBCF
    { 0, eString },  //                       U+EBD0
    { 0, eString },  //                       U+EBD1
    { 0, eString },  //                       U+EBD2
    { 0, eString },  //                       U+EBD3
    { 0, eString },  //                       U+EBD4
    { 0, eString },  //                       U+EBD5
    { 0, eString },  //                       U+EBD6
    { 0, eString },  //                       U+EBD7
    { 0, eString },  //                       U+EBD8
    { 0, eString },  //                       U+EBD9
    { 0, eString },  //                       U+EBDA
    { 0, eString },  //                       U+EBDB
    { 0, eString },  //                       U+EBDC
    { 0, eString },  //                       U+EBDD
    { 0, eString },  //                       U+EBDE
    { 0, eString },  //                       U+EBDF
    { 0, eString },  //                       U+EBE0
    { 0, eString },  //                       U+EBE1
    { 0, eString },  //                       U+EBE2
    { 0, eString },  //                       U+EBE3
    { 0, eString },  //                       U+EBE4
    { 0, eString },  //                       U+EBE5
    { 0, eString },  //                       U+EBE6
    { 0, eString },  //                       U+EBE7
    { 0, eString },  //                       U+EBE8
    { 0, eString },  //                       U+EBE9
    { 0, eString },  //                       U+EBEA
    { 0, eString },  //                       U+EBEB
    { 0, eString },  //                       U+EBEC
    { 0, eString },  //                       U+EBED
    { 0, eString },  //                       U+EBEE
    { 0, eString },  //                       U+EBEF
    { 0, eString },  //                       U+EBF0
    { 0, eString },  //                       U+EBF1
    { 0, eString },  //                       U+EBF2
    { 0, eString },  //                       U+EBF3
    { 0, eString },  //                       U+EBF4
    { 0, eString },  //                       U+EBF5
    { 0, eString },  //                       U+EBF6
    { 0, eString },  //                       U+EBF7
    { 0, eString },  //                       U+EBF8
    { 0, eString },  //                       U+EBF9
    { 0, eString },  //                       U+EBFA
    { 0, eString },  //                       U+EBFB
    { 0, eString },  //                       U+EBFC
    { 0, eString },  //                       U+EBFD
    { 0, eString },  //                       U+EBFE
    { 0, eString },  //                       U+EBFF
};
static TUnicodePlan s_Plan_FBh = {
    { " ff ", eString },  // old dictionary   U+FB00
    { " fi ", eString },  // old dictionary   U+FB01
    { " fl ", eString },  // old dictionary   U+FB02
    { " ffi ", eString },  // old dictionary  U+FB03
    { " ffl ", eString },  // old dictionary  U+FB04
    { 0, eString },  //                    U+FB05
    { 0, eString },  //                    U+FB06
    { 0, eString },  //                    U+FB07
    { 0, eString },  //                    U+FB08
    { 0, eString },  //                    U+FB09
    { 0, eString },  //                    U+FB0A
    { 0, eString },  //                    U+FB0B
    { 0, eString },  //                    U+FB0C
    { 0, eString },  //                    U+FB0D
    { 0, eString },  //                    U+FB0E
    { 0, eString },  //                    U+FB0F
    { 0, eString },  //                    U+FB10
    { 0, eString },  //                    U+FB11
    { 0, eString },  //                    U+FB12
    { 0, eString },  //                    U+FB13
    { 0, eString },  //                    U+FB14
    { 0, eString },  //                    U+FB15
    { 0, eString },  //                    U+FB16
    { 0, eString },  //                    U+FB17
    { 0, eString },  //                    U+FB18
    { 0, eString },  //                    U+FB19
    { 0, eString },  //                    U+FB1A
    { 0, eString },  //                    U+FB1B
    { 0, eString },  //                    U+FB1C
    { 0, eString },  //                    U+FB1D
    { 0, eString },  //                    U+FB1E
    { 0, eString },  //                    U+FB1F
    { 0, eString },  //                    U+FB20
    { 0, eString },  //                    U+FB21
    { 0, eString },  //                    U+FB22
    { 0, eString },  //                    U+FB23
    { 0, eString },  //                    U+FB24
    { 0, eString },  //                    U+FB25
    { 0, eString },  //                    U+FB26
    { 0, eString },  //                    U+FB27
    { 0, eString },  //                    U+FB28
    { 0, eString },  //                    U+FB29
    { 0, eString },  //                    U+FB2A
    { 0, eString },  //                    U+FB2B
    { 0, eString },  //                    U+FB2C
    { 0, eString },  //                    U+FB2D
    { 0, eString },  //                    U+FB2E
    { 0, eString },  //                    U+FB2F
    { 0, eString },  //                    U+FB30
    { 0, eString },  //                    U+FB31
    { 0, eString },  //                    U+FB32
    { 0, eString },  //                    U+FB33
    { 0, eString },  //                    U+FB34
    { 0, eString },  //                    U+FB35
    { 0, eString },  //                    U+FB36
    { 0, eString },  //                    U+FB37
    { 0, eString },  //                    U+FB38
    { 0, eString },  //                    U+FB39
    { 0, eString },  //                    U+FB3A
    { 0, eString },  //                    U+FB3B
    { 0, eString },  //                    U+FB3C
    { 0, eString },  //                    U+FB3D
    { 0, eString },  //                    U+FB3E
    { 0, eString },  //                    U+FB3F
    { 0, eString },  //                    U+FB40
    { 0, eString },  //                    U+FB41
    { 0, eString },  //                    U+FB42
    { 0, eString },  //                    U+FB43
    { 0, eString },  //                    U+FB44
    { 0, eString },  //                    U+FB45
    { 0, eString },  //                    U+FB46
    { 0, eString },  //                    U+FB47
    { 0, eString },  //                    U+FB48
    { 0, eString },  //                    U+FB49
    { 0, eString },  //                    U+FB4A
    { 0, eString },  //                    U+FB4B
    { 0, eString },  //                    U+FB4C
    { 0, eString },  //                    U+FB4D
    { 0, eString },  //                    U+FB4E
    { 0, eString },  //                    U+FB4F
    { 0, eString },  //                    U+FB50
    { 0, eString },  //                    U+FB51
    { 0, eString },  //                    U+FB52
    { 0, eString },  //                    U+FB53
    { 0, eString },  //                    U+FB54
    { 0, eString },  //                    U+FB55
    { 0, eString },  //                    U+FB56
    { 0, eString },  //                    U+FB57
    { 0, eString },  //                    U+FB58
    { 0, eString },  //                    U+FB59
    { 0, eString },  //                    U+FB5A
    { 0, eString },  //                    U+FB5B
    { 0, eString },  //                    U+FB5C
    { 0, eString },  //                    U+FB5D
    { 0, eString },  //                    U+FB5E
    { 0, eString },  //                    U+FB5F
    { 0, eString },  //                    U+FB60
    { 0, eString },  //                    U+FB61
    { 0, eString },  //                    U+FB62
    { 0, eString },  //                    U+FB63
    { 0, eString },  //                    U+FB64
    { 0, eString },  //                    U+FB65
    { 0, eString },  //                    U+FB66
    { 0, eString },  //                    U+FB67
    { 0, eString },  //                    U+FB68
    { 0, eString },  //                    U+FB69
    { 0, eString },  //                    U+FB6A
    { 0, eString },  //                    U+FB6B
    { 0, eString },  //                    U+FB6C
    { 0, eString },  //                    U+FB6D
    { 0, eString },  //                    U+FB6E
    { 0, eString },  //                    U+FB6F
    { 0, eString },  //                    U+FB70
    { 0, eString },  //                    U+FB71
    { 0, eString },  //                    U+FB72
    { 0, eString },  //                    U+FB73
    { 0, eString },  //                    U+FB74
    { 0, eString },  //                    U+FB75
    { 0, eString },  //                    U+FB76
    { 0, eString },  //                    U+FB77
    { 0, eString },  //                    U+FB78
    { 0, eString },  //                    U+FB79
    { 0, eString },  //                    U+FB7A
    { 0, eString },  //                    U+FB7B
    { 0, eString },  //                    U+FB7C
    { 0, eString },  //                    U+FB7D
    { 0, eString },  //                    U+FB7E
    { 0, eString },  //                    U+FB7F
    { 0, eString },  //                    U+FB80
    { 0, eString },  //                    U+FB81
    { 0, eString },  //                    U+FB82
    { 0, eString },  //                    U+FB83
    { 0, eString },  //                    U+FB84
    { 0, eString },  //                    U+FB85
    { 0, eString },  //                    U+FB86
    { 0, eString },  //                    U+FB87
    { 0, eString },  //                    U+FB88
    { 0, eString },  //                    U+FB89
    { 0, eString },  //                    U+FB8A
    { 0, eString },  //                    U+FB8B
    { 0, eString },  //                    U+FB8C
    { 0, eString },  //                    U+FB8D
    { 0, eString },  //                    U+FB8E
    { 0, eString },  //                    U+FB8F
    { 0, eString },  //                    U+FB90
    { 0, eString },  //                    U+FB91
    { 0, eString },  //                    U+FB92
    { 0, eString },  //                    U+FB93
    { 0, eString },  //                    U+FB94
    { 0, eString },  //                    U+FB95
    { 0, eString },  //                    U+FB96
    { 0, eString },  //                    U+FB97
    { 0, eString },  //                    U+FB98
    { 0, eString },  //                    U+FB99
    { 0, eString },  //                    U+FB9A
    { 0, eString },  //                    U+FB9B
    { 0, eString },  //                    U+FB9C
    { 0, eString },  //                    U+FB9D
    { 0, eString },  //                    U+FB9E
    { 0, eString },  //                    U+FB9F
    { 0, eString },  //                    U+FBA0
    { 0, eString },  //                    U+FBA1
    { 0, eString },  //                    U+FBA2
    { 0, eString },  //                    U+FBA3
    { 0, eString },  //                    U+FBA4
    { 0, eString },  //                    U+FBA5
    { 0, eString },  //                    U+FBA6
    { 0, eString },  //                    U+FBA7
    { 0, eString },  //                    U+FBA8
    { 0, eString },  //                    U+FBA9
    { 0, eString },  //                    U+FBAA
    { 0, eString },  //                    U+FBAB
    { 0, eString },  //                    U+FBAC
    { 0, eString },  //                    U+FBAD
    { 0, eString },  //                    U+FBAE
    { 0, eString },  //                    U+FBAF
    { 0, eString },  //                    U+FBB0
    { 0, eString },  //                    U+FBB1
    { 0, eString },  //                    U+FBB2
    { 0, eString },  //                    U+FBB3
    { 0, eString },  //                    U+FBB4
    { 0, eString },  //                    U+FBB5
    { 0, eString },  //                    U+FBB6
    { 0, eString },  //                    U+FBB7
    { 0, eString },  //                    U+FBB8
    { 0, eString },  //                    U+FBB9
    { 0, eString },  //                    U+FBBA
    { 0, eString },  //                    U+FBBB
    { 0, eString },  //                    U+FBBC
    { 0, eString },  //                    U+FBBD
    { 0, eString },  //                    U+FBBE
    { 0, eString },  //                    U+FBBF
    { 0, eString },  //                    U+FBC0
    { 0, eString },  //                    U+FBC1
    { 0, eString },  //                    U+FBC2
    { 0, eString },  //                    U+FBC3
    { 0, eString },  //                    U+FBC4
    { 0, eString },  //                    U+FBC5
    { 0, eString },  //                    U+FBC6
    { 0, eString },  //                    U+FBC7
    { 0, eString },  //                    U+FBC8
    { 0, eString },  //                    U+FBC9
    { 0, eString },  //                    U+FBCA
    { 0, eString },  //                    U+FBCB
    { 0, eString },  //                    U+FBCC
    { 0, eString },  //                    U+FBCD
    { 0, eString },  //                    U+FBCE
    { 0, eString },  //                    U+FBCF
    { 0, eString },  //                    U+FBD0
    { 0, eString },  //                    U+FBD1
    { 0, eString },  //                    U+FBD2
    { 0, eString },  //                    U+FBD3
    { 0, eString },  //                    U+FBD4
    { 0, eString },  //                    U+FBD5
    { 0, eString },  //                    U+FBD6
    { 0, eString },  //                    U+FBD7
    { 0, eString },  //                    U+FBD8
    { 0, eString },  //                    U+FBD9
    { 0, eString },  //                    U+FBDA
    { 0, eString },  //                    U+FBDB
    { 0, eString },  //                    U+FBDC
    { 0, eString },  //                    U+FBDD
    { 0, eString },  //                    U+FBDE
    { 0, eString },  //                    U+FBDF
    { 0, eString },  //                    U+FBE0
    { 0, eString },  //                    U+FBE1
    { 0, eString },  //                    U+FBE2
    { 0, eString },  //                    U+FBE3
    { 0, eString },  //                    U+FBE4
    { 0, eString },  //                    U+FBE5
    { 0, eString },  //                    U+FBE6
    { 0, eString },  //                    U+FBE7
    { 0, eString },  //                    U+FBE8
    { 0, eString },  //                    U+FBE9
    { 0, eString },  //                    U+FBEA
    { 0, eString },  //                    U+FBEB
    { 0, eString },  //                    U+FBEC
    { 0, eString },  //                    U+FBED
    { 0, eString },  //                    U+FBEE
    { 0, eString },  //                    U+FBEF
    { 0, eString },  //                    U+FBF0
    { 0, eString },  //                    U+FBF1
    { 0, eString },  //                    U+FBF2
    { 0, eString },  //                    U+FBF3
    { 0, eString },  //                    U+FBF4
    { 0, eString },  //                    U+FBF5
    { 0, eString },  //                    U+FBF6
    { 0, eString },  //                    U+FBF7
    { 0, eString },  //                    U+FBF8
    { 0, eString },  //                    U+FBF9
    { 0, eString },  //                    U+FBFA
    { 0, eString },  //                    U+FBFB
    { 0, eString },  //                    U+FBFC
    { 0, eString },  //                    U+FBFD
    { 0, eString },  //                    U+FBFE
    { 0, eString },  //                    U+FBFF
};
static TUnicodePlan s_Plan_FEh = {
    { 0, eString },  // U+FE00
    { 0, eString },  // U+FE01
    { 0, eString },  // U+FE02
    { 0, eString },  // U+FE03
    { 0, eString },  // U+FE04
    { 0, eString },  // U+FE05
    { 0, eString },  // U+FE06
    { 0, eString },  // U+FE07
    { 0, eString },  // U+FE08
    { 0, eString },  // U+FE09
    { 0, eString },  // U+FE0A
    { 0, eString },  // U+FE0B
    { 0, eString },  // U+FE0C
    { 0, eString },  // U+FE0D
    { 0, eString },  // U+FE0E
    { 0, eString },  // U+FE0F
    { 0, eString },  // U+FE10
    { 0, eString },  // U+FE11
    { 0, eString },  // U+FE12
    { 0, eString },  // U+FE13
    { 0, eString },  // U+FE14
    { 0, eString },  // U+FE15
    { 0, eString },  // U+FE16
    { 0, eString },  // U+FE17
    { 0, eString },  // U+FE18
    { 0, eString },  // U+FE19
    { 0, eString },  // U+FE1A
    { 0, eString },  // U+FE1B
    { 0, eString },  // U+FE1C
    { 0, eString },  // U+FE1D
    { 0, eString },  // U+FE1E
    { 0, eString },  // U+FE1F
    { "", eString },  //   U+FE20
    { "", eString },  //   U+FE21
    { "", eString },  //   U+FE22
    { "", eString },  //   U+FE23
    { "", eString },  //   U+FE24
    { "", eString },  //   U+FE25
    { "", eString },  //   U+FE26
    { "", eString },  //   U+FE27
    { "", eString },  //   U+FE28
    { "", eString },  //   U+FE29
    { "", eString },  //   U+FE2A
    { "", eString },  //   U+FE2B
    { "", eString },  //   U+FE2C
    { "", eString },  //   U+FE2D
    { "", eString },  //   U+FE2E
    { "", eString },  //   U+FE2F
    { 0, eString },  // U+FE30
    { 0, eString },  // U+FE31
    { 0, eString },  // U+FE32
    { 0, eString },  // U+FE33
    { 0, eString },  // U+FE34
    { 0, eString },  // U+FE35
    { 0, eString },  // U+FE36
    { 0, eString },  // U+FE37
    { 0, eString },  // U+FE38
    { 0, eString },  // U+FE39
    { 0, eString },  // U+FE3A
    { 0, eString },  // U+FE3B
    { 0, eString },  // U+FE3C
    { 0, eString },  // U+FE3D
    { 0, eString },  // U+FE3E
    { 0, eString },  // U+FE3F
    { 0, eString },  // U+FE40
    { 0, eString },  // U+FE41
    { 0, eString },  // U+FE42
    { 0, eString },  // U+FE43
    { 0, eString },  // U+FE44
    { 0, eString },  // U+FE45
    { 0, eString },  // U+FE46
    { 0, eString },  // U+FE47
    { 0, eString },  // U+FE48
    { 0, eString },  // U+FE49
    { 0, eString },  // U+FE4A
    { 0, eString },  // U+FE4B
    { 0, eString },  // U+FE4C
    { 0, eString },  // U+FE4D
    { 0, eString },  // U+FE4E
    { 0, eString },  // U+FE4F
    { 0, eString },  // U+FE50
    { 0, eString },  // U+FE51
    { 0, eString },  // U+FE52
    { 0, eString },  // U+FE53
    { 0, eString },  // U+FE54
    { 0, eString },  // U+FE55
    { 0, eString },  // U+FE56
    { 0, eString },  // U+FE57
    { 0, eString },  // U+FE58
    { 0, eString },  // U+FE59
    { 0, eString },  // U+FE5A
    { 0, eString },  // U+FE5B
    { 0, eString },  // U+FE5C
    { 0, eString },  // U+FE5D
    { 0, eString },  // U+FE5E
    { 0, eString },  // U+FE5F
    { 0, eString },  // U+FE60
    { 0, eString },  // U+FE61
    { 0, eString },  // U+FE62
    { 0, eString },  // U+FE63
    { 0, eString },  // U+FE64
    { 0, eString },  // U+FE65
    { 0, eString },  // U+FE66
    { 0, eString },  // U+FE67
    { 0, eString },  // U+FE68
    { 0, eString },  // U+FE69
    { 0, eString },  // U+FE6A
    { 0, eString },  // U+FE6B
    { 0, eString },  // U+FE6C
    { 0, eString },  // U+FE6D
    { 0, eString },  // U+FE6E
    { 0, eString },  // U+FE6F
    { 0, eString },  // U+FE70
    { 0, eString },  // U+FE71
    { 0, eString },  // U+FE72
    { 0, eString },  // U+FE73
    { 0, eString },  // U+FE74
    { 0, eString },  // U+FE75
    { 0, eString },  // U+FE76
    { 0, eString },  // U+FE77
    { 0, eString },  // U+FE78
    { 0, eString },  // U+FE79
    { 0, eString },  // U+FE7A
    { 0, eString },  // U+FE7B
    { 0, eString },  // U+FE7C
    { 0, eString },  // U+FE7D
    { 0, eString },  // U+FE7E
    { 0, eString },  // U+FE7F
    { 0, eString },  // U+FE80
    { 0, eString },  // U+FE81
    { 0, eString },  // U+FE82
    { 0, eString },  // U+FE83
    { 0, eString },  // U+FE84
    { 0, eString },  // U+FE85
    { 0, eString },  // U+FE86
    { 0, eString },  // U+FE87
    { 0, eString },  // U+FE88
    { 0, eString },  // U+FE89
    { 0, eString },  // U+FE8A
    { 0, eString },  // U+FE8B
    { 0, eString },  // U+FE8C
    { 0, eString },  // U+FE8D
    { 0, eString },  // U+FE8E
    { 0, eString },  // U+FE8F
    { 0, eString },  // U+FE90
    { 0, eString },  // U+FE91
    { 0, eString },  // U+FE92
    { 0, eString },  // U+FE93
    { 0, eString },  // U+FE94
    { 0, eString },  // U+FE95
    { 0, eString },  // U+FE96
    { 0, eString },  // U+FE97
    { 0, eString },  // U+FE98
    { 0, eString },  // U+FE99
    { 0, eString },  // U+FE9A
    { 0, eString },  // U+FE9B
    { 0, eString },  // U+FE9C
    { 0, eString },  // U+FE9D
    { 0, eString },  // U+FE9E
    { 0, eString },  // U+FE9F
    { 0, eString },  // U+FEA0
    { 0, eString },  // U+FEA1
    { 0, eString },  // U+FEA2
    { 0, eString },  // U+FEA3
    { 0, eString },  // U+FEA4
    { 0, eString },  // U+FEA5
    { 0, eString },  // U+FEA6
    { 0, eString },  // U+FEA7
    { 0, eString },  // U+FEA8
    { 0, eString },  // U+FEA9
    { 0, eString },  // U+FEAA
    { 0, eString },  // U+FEAB
    { 0, eString },  // U+FEAC
    { 0, eString },  // U+FEAD
    { 0, eString },  // U+FEAE
    { 0, eString },  // U+FEAF
    { 0, eString },  // U+FEB0
    { 0, eString },  // U+FEB1
    { 0, eString },  // U+FEB2
    { 0, eString },  // U+FEB3
    { 0, eString },  // U+FEB4
    { 0, eString },  // U+FEB5
    { 0, eString },  // U+FEB6
    { 0, eString },  // U+FEB7
    { 0, eString },  // U+FEB8
    { 0, eString },  // U+FEB9
    { 0, eString },  // U+FEBA
    { 0, eString },  // U+FEBB
    { 0, eString },  // U+FEBC
    { 0, eString },  // U+FEBD
    { 0, eString },  // U+FEBE
    { 0, eString },  // U+FEBF
    { 0, eString },  // U+FEC0
    { 0, eString },  // U+FEC1
    { 0, eString },  // U+FEC2
    { 0, eString },  // U+FEC3
    { 0, eString },  // U+FEC4
    { 0, eString },  // U+FEC5
    { 0, eString },  // U+FEC6
    { 0, eString },  // U+FEC7
    { 0, eString },  // U+FEC8
    { 0, eString },  // U+FEC9
    { 0, eString },  // U+FECA
    { 0, eString },  // U+FECB
    { 0, eString },  // U+FECC
    { 0, eString },  // U+FECD
    { 0, eString },  // U+FECE
    { 0, eString },  // U+FECF
    { 0, eString },  // U+FED0
    { 0, eString },  // U+FED1
    { 0, eString },  // U+FED2
    { 0, eString },  // U+FED3
    { 0, eString },  // U+FED4
    { 0, eString },  // U+FED5
    { 0, eString },  // U+FED6
    { 0, eString },  // U+FED7
    { 0, eString },  // U+FED8
    { 0, eString },  // U+FED9
    { 0, eString },  // U+FEDA
    { 0, eString },  // U+FEDB
    { 0, eString },  // U+FEDC
    { 0, eString },  // U+FEDD
    { 0, eString },  // U+FEDE
    { 0, eString },  // U+FEDF
    { 0, eString },  // U+FEE0
    { 0, eString },  // U+FEE1
    { 0, eString },  // U+FEE2
    { 0, eString },  // U+FEE3
    { 0, eString },  // U+FEE4
    { 0, eString },  // U+FEE5
    { 0, eString },  // U+FEE6
    { 0, eString },  // U+FEE7
    { 0, eString },  // U+FEE8
    { 0, eString },  // U+FEE9
    { 0, eString },  // U+FEEA
    { 0, eString },  // U+FEEB
    { 0, eString },  // U+FEEC
    { 0, eString },  // U+FEED
    { 0, eString },  // U+FEEE
    { 0, eString },  // U+FEEF
    { 0, eString },  // U+FEF0
    { 0, eString },  // U+FEF1
    { 0, eString },  // U+FEF2
    { 0, eString },  // U+FEF3
    { 0, eString },  // U+FEF4
    { 0, eString },  // U+FEF5
    { 0, eString },  // U+FEF6
    { 0, eString },  // U+FEF7
    { 0, eString },  // U+FEF8
    { 0, eString },  // U+FEF9
    { 0, eString },  // U+FEFA
    { 0, eString },  // U+FEFB
    { 0, eString },  // U+FEFC
    { 0, eString },  // U+FEFD
    { 0, eString },  // U+FEFE
    { 0, eString },  // U+FEFF
};
