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
 *    Plan 02
 *
 */

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

/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2004/05/06 18:21:26  gouriano
 * Imported from pubmed/xmldb
 *
 * ==========================================================================
 */
