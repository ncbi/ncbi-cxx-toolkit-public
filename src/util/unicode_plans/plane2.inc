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
 *    Plan E2
 *
 */

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

/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2004/05/06 18:21:26  gouriano
 * Imported from pubmed/xmldb
 *
 * ==========================================================================
 */
