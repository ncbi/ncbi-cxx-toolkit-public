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
 *    Plan 21
 *
 */

static TUnicodePlan s_Plan_21h = {
    { 0, eString },  //                                             U+2100
    { 0, eString },  //                                             U+2101
    { "C", eString },  //                                              U+2102
    { 0, eString },  //                                             U+2103
    { 0, eString },  //                                             U+2104
    { "in-care-of", eString },  // old dictionary                      U+2105
    { 0, eString },  //                                             U+2106
    { 0, eString },  //                                             U+2107
    { 0, eString },  //                                             U+2108
    { 0, eString },  //                                             U+2109
    { 0, eString },  //                                             U+210A
    { "H", eString },  //                                              U+210B
    { "H", eString },  //                                              U+210C
    { 0, eString },  //                                             U+210D
    { 0, eString },  //                                             U+210E
    { "variant Planck's over 2pi", eString },  //                      U+210F
    { "I", eString },  //                                              U+2110
    { " imaginary", eString },  // old dictionary                      U+2111
    { " Lgrangian ", eString },  // old dictionary                     U+2112
    { "l", eString },  // old dictionary                               U+2113
    { 0, eString },  //                                             U+2114
    { "N", eString },  //                                              U+2115
    { "numero sign", eString },  // old dictionary                     U+2116
    { "sound recording copyright sign", eString },  // old dictionary  U+2117
    { "Weierstrass p", eString },  // old dictionary                   U+2118
    { "P", eString },  //                                              U+2119
    { "Q", eString },  //                                              U+211A
    { 0, eString },  //                                             U+211B
    { "Re", eString },  // old dictionary                              U+211C
    { "R", eString },  //                                              U+211D
    { "Rx", eString },  // old dictionary                              U+211E
    { 0, eString },  //                                             U+211F
    { 0, eString },  //                                             U+2120
    { 0, eString },  //                                             U+2121
    { "trade mark", eString },  // old dictionary                      U+2122
    { 0, eString },  //                                             U+2123
    { "Z", eString },  //                                              U+2124
    { 0, eString },  //                                             U+2125
    { " ohm", eString },  // old dictionary                            U+2126
    { "conductance", eString },  //                                    U+2127
    { 0, eString },  //                                             U+2128
    { "inverted iota", eString },  //                                  U+2129
    { 0, eString },  //                                             U+212A
    { "A", eString },  // value > 0x80 in the old dict.                U+212B
    { " Bernoulli function", eString },  // old dictionary             U+212C
    { 0, eString },  //                                             U+212D
    { 0, eString },  //                                             U+212E
    { "e", eString },  //                                              U+212F
    { "E", eString },  //                                              U+2130
    { "F", eString },  //                                              U+2131
    { 0, eString },  //                                             U+2132
    { "M", eString },  //                                              U+2133
    { "o", eString },  //                                              U+2134
    { "aleph", eString },  // old dictionary                           U+2135
    { " beth", eString },  // old dictionary                           U+2136
    { "gimel", eString },  // old dictionary                           U+2137
    { " daleth", eString },  // old dictionary                         U+2138
    { 0, eString },  //                                             U+2139
    { 0, eString },  //                                             U+213A
    { 0, eString },  //                                             U+213B
    { 0, eString },  //                                             U+213C
    { 0, eString },  //                                             U+213D
    { 0, eString },  //                                             U+213E
    { 0, eString },  //                                             U+213F
    { 0, eString },  //                                             U+2140
    { 0, eString },  //                                             U+2141
    { 0, eString },  //                                             U+2142
    { 0, eString },  //                                             U+2143
    { 0, eString },  //                                             U+2144
    { 0, eString },  //                                             U+2145
    { 0, eString },  //                                             U+2146
    { 0, eString },  //                                             U+2147
    { 0, eString },  //                                             U+2148
    { 0, eString },  //                                             U+2149
    { 0, eString },  //                                             U+214A
    { 0, eString },  //                                             U+214B
    { 0, eString },  //                                             U+214C
    { 0, eString },  //                                             U+214D
    { 0, eString },  //                                             U+214E
    { 0, eString },  //                                             U+214F
    { 0, eString },  //                                             U+2150
    { 0, eString },  //                                             U+2151
    { 0, eString },  //                                             U+2152
    { "(1/3)", eString },  // old dictionary                           U+2153
    { "(2/3)", eString },  // old dictionary                           U+2154
    { "(1/5)", eString },  // old dictionary                           U+2155
    { "(2/5)", eString },  // old dictionary                           U+2156
    { "(3/5)", eString },  // old dictionary                           U+2157
    { "(4/5)", eString },  // old dictionary                           U+2158
    { "(1/6)", eString },  // old dictionary                           U+2159
    { "(5/6)", eString },  // old dictionary                           U+215A
    { "(1/8)", eString },  // old dictionary                           U+215B
    { "(3/8)", eString },  // old dictionary                           U+215C
    { "(5/8)", eString },  // old dictionary                           U+215D
    { "(7/8)", eString },  // old dictionary                           U+215E
    { 0, eString },  //                                             U+215F
    { 0, eString },  //                                             U+2160
    { 0, eString },  //                                             U+2161
    { 0, eString },  //                                             U+2162
    { 0, eString },  //                                             U+2163
    { 0, eString },  //                                             U+2164
    { 0, eString },  //                                             U+2165
    { 0, eString },  //                                             U+2166
    { 0, eString },  //                                             U+2167
    { 0, eString },  //                                             U+2168
    { 0, eString },  //                                             U+2169
    { 0, eString },  //                                             U+216A
    { 0, eString },  //                                             U+216B
    { 0, eString },  //                                             U+216C
    { 0, eString },  //                                             U+216D
    { 0, eString },  //                                             U+216E
    { 0, eString },  //                                             U+216F
    { 0, eString },  //                                             U+2170
    { 0, eString },  //                                             U+2171
    { 0, eString },  //                                             U+2172
    { 0, eString },  //                                             U+2173
    { 0, eString },  //                                             U+2174
    { 0, eString },  //                                             U+2175
    { 0, eString },  //                                             U+2176
    { 0, eString },  //                                             U+2177
    { 0, eString },  //                                             U+2178
    { 0, eString },  //                                             U+2179
    { 0, eString },  //                                             U+217A
    { 0, eString },  //                                             U+217B
    { 0, eString },  //                                             U+217C
    { 0, eString },  //                                             U+217D
    { 0, eString },  //                                             U+217E
    { 0, eString },  //                                             U+217F
    { 0, eString },  //                                             U+2180
    { 0, eString },  //                                             U+2181
    { 0, eString },  //                                             U+2182
    { 0, eString },  //                                             U+2183
    { 0, eString },  //                                             U+2184
    { 0, eString },  //                                             U+2185
    { 0, eString },  //                                             U+2186
    { 0, eString },  //                                             U+2187
    { 0, eString },  //                                             U+2188
    { 0, eString },  //                                             U+2189
    { 0, eString },  //                                             U+218A
    { 0, eString },  //                                             U+218B
    { 0, eString },  //                                             U+218C
    { 0, eString },  //                                             U+218D
    { 0, eString },  //                                             U+218E
    { 0, eString },  //                                             U+218F
    { "<--", eString },  // old dictionary                             U+2190
    { " upward arrow", eString },  // old dictionary                   U+2191
    { "-->", eString },  // old dictionary                             U+2192
    { " downward arrow", eString },  // old dictionary                 U+2193
    { "<-->", eString },  // old dictionary                            U+2194
    { "up and down arrow ", eString },  // old dictionary              U+2195
    { "NW pointing arrow ", eString },  // old dictionary              U+2196
    { "nearrow ", eString },  // old dictionary                        U+2197
    { "SE pointing arrow", eString },  //                              U+2198
    { "SW pointing arrow", eString },  //                              U+2199
    { "not left arrow ", eString },  // old dictionary                 U+219A
    { "not right arrow ", eString },  // old dictionary                U+219B
    { 0, eString },  //                                             U+219C
    { 0, eString },  //                                             U+219D
    { "two head left arrow", eString },  // old dictionary             U+219E
    { "up two-headed arrow", eString },  //                            U+219F
    { "two head right arrow", eString },  // old dictionary            U+21A0
    { "down two-headed arrow", eString },  //                          U+21A1
    { "left arrow-tailed ", eString },  // old dictionary              U+21A2
    { "right arrow-tailed", eString },  //                             U+21A3
    { 0, eString },  //                                             U+21A4
    { 0, eString },  //                                             U+21A5
    { "mapsto", eString },  // old dictionary                          U+21A6
    { 0, eString },  //                                             U+21A7
    { 0, eString },  //                                             U+21A8
    { "left arrow-hooked ", eString },  // old dictionary              U+21A9
    { "right arrow-hooked ", eString },  // old dictionary             U+21AA
    { "left arrow-looped ", eString },  // old dictionary              U+21AB
    { "right arrow-looped ", eString },  // old dictionary             U+21AC
    { "left and right squig arrow ", eString },  // old dictionary     U+21AD
    { "not l&r arrow ", eString },  // old dictionary                  U+21AE
    { 0, eString },  //                                             U+21AF
    { "Lsh", eString },  // old dictionary                             U+21B0
    { "Rsh", eString },  // old dictionary                             U+21B1
    { "left down angled arrow", eString },  //                         U+21B2
    { "right down angled arrow", eString },  //                        U+21B3
    { 0, eString },  //                                             U+21B4
    { 0, eString },  //                                             U+21B5
    { "left curved arrow ", eString },  // old dictionary              U+21B6
    { "right curved arrow ", eString },  // old dictionary             U+21B7
    { 0, eString },  //                                             U+21B8
    { 0, eString },  //                                             U+21B9
    { "left arrow in circle ", eString },  // old dictionary           U+21BA
    { "right arrow in circle ", eString },  // old dictionary          U+21BB
    { "left harpoon-up ", eString },  // old dictionary                U+21BC
    { "left harpoon-down ", eString },  // old dictionary              U+21BD
    { "up harp-right", eString },  // old dictionary                   U+21BE
    { "up harpoon-left ", eString },  // old dictionary                U+21BF
    { "right harpoon-up ", eString },  // old dictionary               U+21C0
    { "right harpoon-down ", eString },  // old dictionary             U+21C1
    { "down harpoon-right ", eString },  // old dictionary             U+21C2
    { "down harpoon-left ", eString },  // old dictionary              U+21C3
    { "right arrow over left arrow", eString },  //                    U+21C4
    { "up arrow, down arrow ", eString },  //                          U+21C5
    { "left arrow over right arrow", eString },  //                    U+21C6
    { "two left arrows", eString },  //                                U+21C7
    { "two up arrows", eString },  //                                  U+21C8
    { "two right arrows", eString },  //                               U+21C9
    { "two down arrows", eString },  //                                U+21CA
    { "left harpoon over right harpoon", eString },  //                U+21CB
    { "right harpoon over left harpoon", eString },  //                U+21CC
    { "not implied by", eString },  //                                 U+21CD
    { "not l&r dbl arr ", eString },  // old dictionary                U+21CE
    { "not implies", eString },  //                                    U+21CF
    { "<--", eString },  // old dictionary                             U+21D0
    { "up dbl arrow ", eString },  // old dictionary                   U+21D1
    { "-->", eString },  // old dictionary                             U+21D2
    { "down double arrow ", eString },  // old dictionary              U+21D3
    { "<-->", eString },  // old dictionary                            U+21D4
    { "up and down dbl arrow ", eString },  // old dictionary          U+21D5
    { "NW pointing dbl arrow", eString },  //                          U+21D6
    { "NE pointing double arrow", eString },  //                       U+21D7
    { "SE pointing double arrow", eString },  //                       U+21D8
    { "SW pointing dbl arrow", eString },  //                          U+21D9
    { "left triple arrow ", eString },  // old dictionary              U+21DA
    { "right triple arrow ", eString },  // old dictionary             U+21DB
    { 0, eString },  //                                             U+21DC
    { "right arrow-wavy", eString },  //                               U+21DD
    { 0, eString },  //                                             U+21DE
    { 0, eString },  //                                             U+21DF
    { 0, eString },  //                                             U+21E0
    { 0, eString },  //                                             U+21E1
    { 0, eString },  //                                             U+21E2
    { 0, eString },  //                                             U+21E3
    { 0, eString },  //                                             U+21E4
    { 0, eString },  //                                             U+21E5
    { 0, eString },  //                                             U+21E6
    { 0, eString },  //                                             U+21E7
    { 0, eString },  //                                             U+21E8
    { 0, eString },  //                                             U+21E9
    { 0, eString },  //                                             U+21EA
    { 0, eString },  //                                             U+21EB
    { 0, eString },  //                                             U+21EC
    { 0, eString },  //                                             U+21ED
    { 0, eString },  //                                             U+21EE
    { 0, eString },  //                                             U+21EF
    { 0, eString },  //                                             U+21F0
    { 0, eString },  //                                             U+21F1
    { 0, eString },  //                                             U+21F2
    { 0, eString },  //                                             U+21F3
    { 0, eString },  //                                             U+21F4
    { 0, eString },  //                                             U+21F5
    { 0, eString },  //                                             U+21F6
    { 0, eString },  //                                             U+21F7
    { 0, eString },  //                                             U+21F8
    { 0, eString },  //                                             U+21F9
    { 0, eString },  //                                             U+21FA
    { 0, eString },  //                                             U+21FB
    { 0, eString },  //                                             U+21FC
    { 0, eString },  //                                             U+21FD
    { 0, eString },  //                                             U+21FE
    { 0, eString },  //                                             U+21FF
};

/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2004/05/06 18:21:25  gouriano
 * Imported from pubmed/xmldb
 *
 * ==========================================================================
 */
