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
 *    Plan E5
 *
 */

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

/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2004/05/06 18:21:25  gouriano
 * Imported from pubmed/xmldb
 *
 * ==========================================================================
 */
