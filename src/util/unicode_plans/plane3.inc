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
 *    Plan E3
 *
 */

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

/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2004/05/06 18:21:26  gouriano
 * Imported from pubmed/xmldb
 *
 * ==========================================================================
 */
