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
 *    Plan 01
 *
 */

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

/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2004/05/06 18:21:25  gouriano
 * Imported from pubmed/xmldb
 *
 * ==========================================================================
 */
