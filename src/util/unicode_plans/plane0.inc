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
 *    Plan E0
 *
 */

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

/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2004/05/06 18:21:25  gouriano
 * Imported from pubmed/xmldb
 *
 * ==========================================================================
 */
