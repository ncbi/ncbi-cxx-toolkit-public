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
 *    Plan EA
 *
 */

static TUnicodePlan s_Plan_EAh = {
    { "1", eString },  //                U+EA00
    { "1", eString },  //                U+EA01
    { "2", eString },  //                U+EA02
    { "2", eString },  //                U+EA03
    { "3", eString },  //                U+EA04
    { "3", eString },  //                U+EA05
    { "4", eString },  //                U+EA06
    { "a", eString },  //                U+EA07
    { "a", eString },  //                U+EA08
    { "A", eString },  //                U+EA09
    { "a", eString },  //                U+EA0A
    { "A", eString },  //                U+EA0B
    { "AB", eString },  //               U+EA0C
    { "alpha", eString },  //            U+EA0D
    { "alpha", eString },  //            U+EA0E
    { "alpha", eString },  //            U+EA0F
    { "b", eString },  //                U+EA10
    { "B", eString },  //                U+EA11
    { "b", eString },  //                U+EA12
    { "B", eString },  //                U+EA13
    { "B", eString },  //                U+EA14
    { "B", eString },  //                U+EA15
    { "BC", eString },  //               U+EA16
    { "beta", eString },  //             U+EA17
    { "beta", eString },  //             U+EA18
    { "c", eString },  //                U+EA19
    { "c", eString },  //                U+EA1A
    { "c", eString },  //                U+EA1B
    { "C", eString },  //                U+EA1C
    { "c", eString },  //                U+EA1D
    { "c", eString },  //                U+EA1E
    { "c", eString },  //                U+EA1F
    { "c", eString },  //                U+EA20
    { "c", eString },  //                U+EA21
    { "chi", eString },  //              U+EA22
    { "chi", eString },  //              U+EA23
    { "chi", eString },  //              U+EA24
    { "d", eString },  //                U+EA25
    { "D", eString },  //                U+EA26
    { "d", eString },  //                U+EA27
    { "D", eString },  //                U+EA28
    { "d", eString },  //                U+EA29
    { "D", eString },  //                U+EA2A
    { "D", eString },  //                U+EA2B
    { "Delta", eString },  //            U+EA2C
    { "delta", eString },  //            U+EA2D
    { "delta", eString },  //            U+EA2E
    { "dl", eString },  //               U+EA2F
    { "e", eString },  //                U+EA30
    { "e", eString },  //                U+EA31
    { "l", eString },  //                U+EA32
    { "l", eString },  //                U+EA33
    { "f", eString },  //                U+EA34
    { "f", eString },  //                U+EA35
    { "F", eString },  //                U+EA36
    { "f", eString },  //                U+EA37
    { "F", eString },  //                U+EA38
    { "f", eString },  //                U+EA39
    { "F", eString },  //                U+EA3A
    { "sigma", eString },  //            U+EA3B
    { "sigma", eString },  //            U+EA3C
    { "g", eString },  //                U+EA3D
    { "g", eString },  //                U+EA3E
    { "G", eString },  //                U+EA3F
    { "g", eString },  //                U+EA40
    { "G", eString },  //                U+EA41
    { "Gamma", eString },  //            U+EA42
    { "gamma", eString },  //            U+EA43
    { "gamma", eString },  //            U+EA44
    { "gamma", eString },  //            U+EA45
    { "H", eString },  //                U+EA46
    { "h", eString },  //                U+EA47
    { "h", eString },  //                U+EA48
    { "i", eString },  //                U+EA49
    { "I", eString },  //                U+EA4A
    { "j", eString },  //                U+EA4B
    { "J", eString },  //                U+EA4C
    { "J", eString },  //                U+EA4D
    { "J", eString },  //                U+EA4E
    { "k", eString },  //                U+EA4F
    { "k", eString },  //                U+EA50
    { "k", eString },  //                U+EA51
    { "K", eString },  //                U+EA52
    { "K", eString },  //                U+EA53
    { "k", eString },  //                U+EA54
    { "l", eString },  //                U+EA55
    { "L", eString },  //                U+EA56
    { "l", eString },  //                U+EA57
    { "lambda", eString },  //           U+EA58
    { "lambda", eString },  //           U+EA59
    { "Lambda", eString },  //           U+EA5A
    { "lnV", eString },  //              U+EA5B
    { "m", eString },  //                U+EA5C
    { "M", eString },  //                U+EA5D
    { "m", eString },  //                U+EA5E
    { "m", eString },  //                U+EA5F
    { "mu", eString },  //               U+EA60
    { "mu", eString },  //               U+EA61
    { "mu", eString },  //               U+EA62
    { "n", eString },  //                U+EA63
    { "n", eString },  //                U+EA64
    { "N", eString },  //                U+EA65
    { "n", eString },  //                U+EA66
    { "N", eString },  //                U+EA67
    { "n", eString },  //                U+EA68
    { "n", eString },  //                U+EA69
    { "N", eString },  //                U+EA6A
    { "nabla", eString },  //            U+EA6B
    { "nu", eString },  //               U+EA6C
    { "nv", eString },  //               U+EA6D
    { "O", eString },  //                U+EA6E
    { "O", eString },  //                U+EA6F
    { "O", eString },  //                U+EA70
    { "omega", eString },  //            U+EA71
    { "F", eString },  //                U+EA72
    { "p", eString },  //                U+EA73
    { "P", eString },  //                U+EA74
    { "p", eString },  //                U+EA75
    { "P", eString },  //                U+EA76
    { "p", eString },  //                U+EA77
    { "P", eString },  //                U+EA78
    { "p", eString },  //                U+EA79
    { "P", eString },  //                U+EA7A
    { "phi", eString },  //              U+EA7B
    { "phi", eString },  //              U+EA7C
    { "Phi", eString },  //              U+EA7D
    { "phi", eString },  //              U+EA7E
    { "pi", eString },  //               U+EA7F
    { "pi", eString },  //               U+EA80
    { "pi", eString },  //               U+EA81
    { "Pi", eString },  //               U+EA82
    { "pi", eString },  //               U+EA83
    { "pi", eString },  //               U+EA84
    { "pi", eString },  //               U+EA85
    { "pi", eString },  //               U+EA86
    { "psi", eString },  //              U+EA87
    { "psi", eString },  //              U+EA88
    { "psi", eString },  //              U+EA89
    { "Psi", eString },  //              U+EA8A
    { "psi", eString },  //              U+EA8B
    { "Psi", eString },  //              U+EA8C
    { "q", eString },  //                U+EA8D
    { "Q", eString },  //                U+EA8E
    { "q", eString },  //                U+EA8F
    { "q", eString },  //                U+EA90
    { "Q", eString },  //                U+EA91
    { "q", eString },  //                U+EA92
    { "Q", eString },  //                U+EA93
    { "q", eString },  //                U+EA94
    { "q", eString },  //                U+EA95
    { "q", eString },  //                U+EA96
    { "Q", eString },  //                U+EA97
    { "r", eString },  //                U+EA98
    { "R", eString },  //                U+EA99
    { "R", eString },  //                U+EA9A
    { "r", eString },  //                U+EA9B
    { "R", eString },  //                U+EA9C
    { "R", eString },  //                U+EA9D
    { "r", eString },  //                U+EA9E
    { "R", eString },  //                U+EA9F
    { "r", eString },  //                U+EAA0
    { "r", eString },  //                U+EAA1
    { "R", eString },  //                U+EAA2
    { "r", eString },  //                U+EAA3
    { "r1", eString },  //               U+EAA4
    { "RE", eString },  //               U+EAA5
    { "rho", eString },  //              U+EAA6
    { "rho", eString },  //              U+EAA7
    { "rho", eString },  //              U+EAA8
    { "ri", eString },  //               U+EAA9
    { "rj", eString },  //               U+EAAA
    { "rN", eString },  //               U+EAAB
    { "s", eString },  //                U+EAAC
    { "S", eString },  //                U+EAAD
    { "S", eString },  //                U+EAAE
    { "s", eString },  //                U+EAAF
    { "S", eString },  //                U+EAB0
    { "s", eString },  //                U+EAB1
    { "S", eString },  //                U+EAB2
    { "S", eString },  //                U+EAB3
    { "B", eString },  //                U+EAB4
    { "E", eString },  //                U+EAB5
    { "G", eString },  //                U+EAB6
    { "P", eString },  //                U+EAB7
    { "Q", eString },  //                U+EAB8
    { "t", eString },  //                U+EAB9
    { "T", eString },  //                U+EABA
    { "T", eString },  //                U+EABB
    { "t", eString },  //                U+EABC
    { "T", eString },  //                U+EABD
    { "t", eString },  //                U+EABE
    { "T", eString },  //                U+EABF
    { "tau", eString },  //              U+EAC0
    { "tau", eString },  //              U+EAC1
    { "theta", eString },  //            U+EAC2
    { "theta", eString },  //            U+EAC3
    { "times", eString },  //            U+EAC4
    { "TT", eString },  //               U+EAC5
    { "u", eString },  //                U+EAC6
    { "u", eString },  //                U+EAC7
    { "U", eString },  //                U+EAC8
    { "u", eString },  //                U+EAC9
    { "u", eString },  //                U+EACA
    { "upsilon", eString },  //          U+EACB
    { "V", eString },  //                U+EACC
    { "v", eString },  //                U+EACD
    { "v", eString },  //                U+EACE
    { "V", eString },  //                U+EACF
    { "v", eString },  //                U+EAD0
    { "V", eString },  //                U+EAD1
    { "v", eString },  //                U+EAD2
    { "v", eString },  //                U+EAD3
    { "V", eString },  //                U+EAD4
    { "v", eString },  //                U+EAD5
    { "epsilon", eString },  //          U+EAD6
    { "epsilon", eString },  //          U+EAD7
    { "epsilon", eString },  //          U+EAD8
    { "phi", eString },  //              U+EAD9
    { "phi", eString },  //              U+EADA
    { "theta", eString },  //            U+EADB
    { "w", eString },  //                U+EADC
    { "w", eString },  //                U+EADD
    { "w", eString },  //                U+EADE
    { "x", eString },  //                U+EADF
    { "X", eString },  //                U+EAE0
    { "x", eString },  //                U+EAE1
    { "X", eString },  //                U+EAE2
    { "x", eString },  //                U+EAE3
    { "x", eString },  //                U+EAE4
    { "X", eString },  //                U+EAE5
    { "x", eString },  //                U+EAE6
    { "xi", eString },  //               U+EAE7
    { "y", eString },  //                U+EAE8
    { "Y", eString },  //                U+EAE9
    { "y", eString },  //                U+EAEA
    { "Y", eString },  //                U+EAEB
    { "y", eString },  //                U+EAEC
    { "y", eString },  //                U+EAED
    { "z", eString },  //                U+EAEE
    { "Z", eString },  //                U+EAEF
    { "z", eString },  //                U+EAF0
    { "z", eString },  //                U+EAF1
    { "Z", eString },  //                U+EAF2
    { "z", eString },  //                U+EAF3
    { "z", eString },  //                U+EAF4
    { "zeta", eString },  //             U+EAF5
    { "zeta", eString },  //             U+EAF6
    { "xbscrsv", eString },  //          U+EAF7
    { "xbscrtv", eString },  //          U+EAF8
    { "xescr1v", eString },  //          U+EAF9
    { "xescr2v", eString },  //          U+EAFA
    { "ngrmi", eString },  //            U+EAFB
    { "B", eString },  //                U+EAFC
    { 0, eString },  //               U+EAFD
    { "sigma with tilde", eString },  // U+EAFE
    { "A", eString },  //                U+EAFF
};

/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2004/05/06 18:21:26  gouriano
 * Imported from pubmed/xmldb
 *
 * ==========================================================================
 */
