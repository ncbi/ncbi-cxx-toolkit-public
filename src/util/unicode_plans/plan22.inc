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
 *    Plan 22
 *
 */

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

/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2004/05/06 18:21:26  gouriano
 * Imported from pubmed/xmldb
 *
 * ==========================================================================
 */
