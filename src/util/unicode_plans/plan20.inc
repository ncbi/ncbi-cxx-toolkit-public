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
 *    Plan 20
 *
 */

static TUnicodePlan s_Plan_20h = {
    { 0, eString },  //                             U+2000
    { 0, eString },  //                             U+2001
    { " ", eString },  // old dictionary               U+2002
    { " ", eString },  // old dictionary               U+2003
    { " ", eString },  // old dictionary               U+2004
    { " ", eString },  // old dictionary               U+2005
    { 0, eString },  //                             U+2006
    { " ", eString },  // old dictionary               U+2007
    { " ", eString },  // old dictionary               U+2008
    { " ", eString },  // old dictionary               U+2009
    { " ", eString },  // old dictionary               U+200A
    { 0, eString },  //                             U+200B
    { 0, eString },  //                             U+200C
    { 0, eString },  //                             U+200D
    { 0, eString },  //                             U+200E
    { 0, eString },  //                             U+200F
    { "-", eString },  // old dictionary               U+2010
    { 0, eString },  //                             U+2011
    { 0, eString },  //                             U+2012
    { "-", eString },  // old dictionary               U+2013
    { "-", eString },  // old dictionary               U+2014
    { "-", eString },  // old dictionary               U+2015
    { " ||", eString },  // old dictionary             U+2016
    { 0, eString },  //                             U+2017
    { "'", eString },  // old dictionary               U+2018
    { "'", eString },  // old dictionary               U+2019
    { "'", eString },  // old dictionary               U+201A
    { 0, eString },  //                             U+201B
    { "\"", eString },  // old dictionary              U+201C
    { "\"", eString },  // old dictionary              U+201D
    { "\"", eString },  // old dictionary              U+201E
    { 0, eString },  //                             U+201F
    { "dagger", eString },  // old dictionary          U+2020
    { "double dagger", eString },  // old dictionary   U+2021
    { "*", eString },  // old dictionary               U+2022
    { 0, eString },  //                             U+2023
    { 0, eString },  //                             U+2024
    { "..", eString },  // old dictionary              U+2025
    { " em leader", eString },  // old dictionary      U+2026
    { 0, eString },  //                             U+2027
    { 0, eString },  //                             U+2028
    { 0, eString },  //                             U+2029
    { 0, eString },  //                             U+202A
    { 0, eString },  //                             U+202B
    { 0, eString },  //                             U+202C
    { 0, eString },  //                             U+202D
    { 0, eString },  //                             U+202E
    { 0, eString },  //                             U+202F
    { " per thousand", eString },  // old dictionary   U+2030
    { "per 10 thousand", eString },  //                U+2031
    { "'", eString },  // old dictionary               U+2032
    { "''", eString },  // old dictionary              U+2033
    { "'''", eString },  // old dictionary             U+2034
    { "'", eString },  // old dictionary               U+2035
    { 0, eString },  //                             U+2036
    { 0, eString },  //                             U+2037
    { 0, eString },  //                             U+2038
    { 0, eString },  //                             U+2039
    { 0, eString },  //                             U+203A
    { 0, eString },  //                             U+203B
    { 0, eString },  //                             U+203C
    { 0, eString },  //                             U+203D
    { 0, eString },  //                             U+203E
    { 0, eString },  //                             U+203F
    { 0, eString },  //                             U+2040
    { "insertion mark", eString },  // old dictionary  U+2041
    { 0, eString },  //                             U+2042
    { " rectangle", eString },  // old dictionary      U+2043
    { 0, eString },  //                             U+2044
    { 0, eString },  //                             U+2045
    { 0, eString },  //                             U+2046
    { 0, eString },  //                             U+2047
    { 0, eString },  //                             U+2048
    { 0, eString },  //                             U+2049
    { 0, eString },  //                             U+204A
    { 0, eString },  //                             U+204B
    { 0, eString },  //                             U+204C
    { 0, eString },  //                             U+204D
    { 0, eString },  //                             U+204E
    { 0, eString },  //                             U+204F
    { 0, eString },  //                             U+2050
    { 0, eString },  //                             U+2051
    { 0, eString },  //                             U+2052
    { 0, eString },  //                             U+2053
    { 0, eString },  //                             U+2054
    { 0, eString },  //                             U+2055
    { 0, eString },  //                             U+2056
    { 0, eString },  //                             U+2057
    { 0, eString },  //                             U+2058
    { 0, eString },  //                             U+2059
    { 0, eString },  //                             U+205A
    { 0, eString },  //                             U+205B
    { 0, eString },  //                             U+205C
    { 0, eString },  //                             U+205D
    { 0, eString },  //                             U+205E
    { 0, eString },  //                             U+205F
    { 0, eString },  //                             U+2060
    { 0, eString },  //                             U+2061
    { 0, eString },  //                             U+2062
    { 0, eString },  //                             U+2063
    { 0, eString },  //                             U+2064
    { 0, eString },  //                             U+2065
    { 0, eString },  //                             U+2066
    { 0, eString },  //                             U+2067
    { 0, eString },  //                             U+2068
    { 0, eString },  //                             U+2069
    { 0, eString },  //                             U+206A
    { 0, eString },  //                             U+206B
    { 0, eString },  //                             U+206C
    { 0, eString },  //                             U+206D
    { 0, eString },  //                             U+206E
    { 0, eString },  //                             U+206F
    { 0, eString },  //                             U+2070
    { 0, eString },  //                             U+2071
    { 0, eString },  //                             U+2072
    { 0, eString },  //                             U+2073
    { 0, eString },  //                             U+2074
    { 0, eString },  //                             U+2075
    { 0, eString },  //                             U+2076
    { 0, eString },  //                             U+2077
    { 0, eString },  //                             U+2078
    { 0, eString },  //                             U+2079
    { 0, eString },  //                             U+207A
    { 0, eString },  //                             U+207B
    { 0, eString },  //                             U+207C
    { 0, eString },  //                             U+207D
    { 0, eString },  //                             U+207E
    { 0, eString },  //                             U+207F
    { 0, eString },  //                             U+2080
    { 0, eString },  //                             U+2081
    { 0, eString },  //                             U+2082
    { 0, eString },  //                             U+2083
    { 0, eString },  //                             U+2084
    { 0, eString },  //                             U+2085
    { 0, eString },  //                             U+2086
    { 0, eString },  //                             U+2087
    { 0, eString },  //                             U+2088
    { 0, eString },  //                             U+2089
    { 0, eString },  //                             U+208A
    { 0, eString },  //                             U+208B
    { 0, eString },  //                             U+208C
    { 0, eString },  //                             U+208D
    { 0, eString },  //                             U+208E
    { 0, eString },  //                             U+208F
    { 0, eString },  //                             U+2090
    { 0, eString },  //                             U+2091
    { 0, eString },  //                             U+2092
    { 0, eString },  //                             U+2093
    { 0, eString },  //                             U+2094
    { 0, eString },  //                             U+2095
    { 0, eString },  //                             U+2096
    { 0, eString },  //                             U+2097
    { 0, eString },  //                             U+2098
    { 0, eString },  //                             U+2099
    { 0, eString },  //                             U+209A
    { 0, eString },  //                             U+209B
    { 0, eString },  //                             U+209C
    { 0, eString },  //                             U+209D
    { 0, eString },  //                             U+209E
    { 0, eString },  //                             U+209F
    { 0, eString },  //                             U+20A0
    { 0, eString },  //                             U+20A1
    { 0, eString },  //                             U+20A2
    { 0, eString },  //                             U+20A3
    { 0, eString },  //                             U+20A4
    { 0, eString },  //                             U+20A5
    { 0, eString },  //                             U+20A6
    { 0, eString },  //                             U+20A7
    { 0, eString },  //                             U+20A8
    { 0, eString },  //                             U+20A9
    { 0, eString },  //                             U+20AA
    { 0, eString },  //                             U+20AB
    { 0, eString },  //                             U+20AC
    { 0, eString },  //                             U+20AD
    { 0, eString },  //                             U+20AE
    { 0, eString },  //                             U+20AF
    { 0, eString },  //                             U+20B0
    { 0, eString },  //                             U+20B1
    { 0, eString },  //                             U+20B2
    { 0, eString },  //                             U+20B3
    { 0, eString },  //                             U+20B4
    { 0, eString },  //                             U+20B5
    { 0, eString },  //                             U+20B6
    { 0, eString },  //                             U+20B7
    { 0, eString },  //                             U+20B8
    { 0, eString },  //                             U+20B9
    { 0, eString },  //                             U+20BA
    { 0, eString },  //                             U+20BB
    { 0, eString },  //                             U+20BC
    { 0, eString },  //                             U+20BD
    { 0, eString },  //                             U+20BE
    { 0, eString },  //                             U+20BF
    { 0, eString },  //                             U+20C0
    { 0, eString },  //                             U+20C1
    { 0, eString },  //                             U+20C2
    { 0, eString },  //                             U+20C3
    { 0, eString },  //                             U+20C4
    { 0, eString },  //                             U+20C5
    { 0, eString },  //                             U+20C6
    { 0, eString },  //                             U+20C7
    { 0, eString },  //                             U+20C8
    { 0, eString },  //                             U+20C9
    { 0, eString },  //                             U+20CA
    { 0, eString },  //                             U+20CB
    { 0, eString },  //                             U+20CC
    { 0, eString },  //                             U+20CD
    { 0, eString },  //                             U+20CE
    { 0, eString },  //                             U+20CF
    { 0, eString },  //                             U+20D0
    { 0, eString },  //                             U+20D1
    { 0, eString },  //                             U+20D2
    { 0, eString },  //                             U+20D3
    { 0, eString },  //                             U+20D4
    { 0, eString },  //                             U+20D5
    { 0, eString },  //                             U+20D6
    { 0, eString },  //                             U+20D7
    { 0, eString },  //                             U+20D8
    { 0, eString },  //                             U+20D9
    { 0, eString },  //                             U+20DA
    { "...", eString },  // old dictionary             U+20DB
    { "Dot;", eString },  // old dictionary            U+20DC
    { 0, eString },  //                             U+20DD
    { 0, eString },  //                             U+20DE
    { 0, eString },  //                             U+20DF
    { 0, eString },  //                             U+20E0
    { 0, eString },  //                             U+20E1
    { 0, eString },  //                             U+20E2
    { 0, eString },  //                             U+20E3
    { 0, eString },  //                             U+20E4
    { 0, eString },  //                             U+20E5
    { 0, eString },  //                             U+20E6
    { 0, eString },  //                             U+20E7
    { 0, eString },  //                             U+20E8
    { 0, eString },  //                             U+20E9
    { 0, eString },  //                             U+20EA
    { 0, eString },  //                             U+20EB
    { 0, eString },  //                             U+20EC
    { 0, eString },  //                             U+20ED
    { 0, eString },  //                             U+20EE
    { 0, eString },  //                             U+20EF
    { 0, eString },  //                             U+20F0
    { 0, eString },  //                             U+20F1
    { 0, eString },  //                             U+20F2
    { 0, eString },  //                             U+20F3
    { 0, eString },  //                             U+20F4
    { 0, eString },  //                             U+20F5
    { 0, eString },  //                             U+20F6
    { 0, eString },  //                             U+20F7
    { 0, eString },  //                             U+20F8
    { 0, eString },  //                             U+20F9
    { 0, eString },  //                             U+20FA
    { 0, eString },  //                             U+20FB
    { 0, eString },  //                             U+20FC
    { 0, eString },  //                             U+20FD
    { 0, eString },  //                             U+20FE
    { 0, eString },  //                             U+20FF
};

/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2004/05/06 18:21:25  gouriano
 * Imported from pubmed/xmldb
 *
 * ==========================================================================
 */
