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
 *    Plan 04
 *
 */

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

/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2004/05/06 18:21:25  gouriano
 * Imported from pubmed/xmldb
 *
 * ==========================================================================
 */
