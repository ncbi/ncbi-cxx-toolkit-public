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
 *    Plan 30
 *
 */

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

/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2004/05/06 18:21:26  gouriano
 * Imported from pubmed/xmldb
 *
 * ==========================================================================
 */
