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
 *    Plan 25
 *
 */

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

/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2004/05/06 18:21:25  gouriano
 * Imported from pubmed/xmldb
 *
 * ==========================================================================
 */
