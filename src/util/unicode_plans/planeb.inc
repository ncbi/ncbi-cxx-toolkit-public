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
 *    Plan EB
 *
 */

static TUnicodePlan s_Plan_EBh = {
    { "Lambda", eString },  //                   U+EB00
    { "Lambda with umlaut", eString },  //       U+EB01
    { "down triangle with a dot", eString },  // U+EB02
    { "tau with tilde", eString },  //           U+EB03
    { "w", eString },  //                        U+EB04
    { "m", eString },  //                        U+EB05
    { "M", eString },  //                        U+EB06
    { "E", eString },  //                        U+EB07
    { "W", eString },  //                        U+EB08
    { "kappa", eString },  //                    U+EB09
    { "h", eString },  //                        U+EB0A
    { "n", eString },  //                        U+EB0B
    { "underlined kappa", eString },  //         U+EB0C
    { "H", eString },  //                        U+EB0D
    { "Theta", eString },  //                    U+EB0E
    { "M", eString },  //                        U+EB0F
    { "m", eString },  //                        U+EB10
    { "Z", eString },  //                        U+EB11
    { "g", eString },  //                        U+EB12
    { "partial", eString },  //                  U+EB13
    { "C", eString },  //                        U+EB14
    { "E", eString },  //                        U+EB15
    { "sfgrvect", eString },  //                 U+EB16
    { "bgrvect", eString },  //                  U+EB17
    { "xogrvect", eString },  //                 U+EB18
    { "W", eString },  //                        U+EB19
    { "a", eString },  //                        U+EB1A
    { "b", eString },  //                        U+EB1B
    { "S", eString },  //                        U+EB1C
    { "otimesmac", eString },  //                U+EB1D
    { "J", eString },  //                        U+EB1E
    { "b", eString },  //                        U+EB1F
    { "Z", eString },  //                        U+EB20
    { "L", eString },  //                        U+EB21
    { "g", eString },  //                        U+EB22
    { "Delta with circumflex", eString },  //    U+EB23
    { "omega with tilde", eString },  //         U+EB24
    { "Lambda with circumflex", eString },  //   U+EB25
    { "var phiv with circumflex", eString },  // U+EB26
    { "Delta with tilde", eString },  //         U+EB27
    { "lambda with macron", eString },  //       U+EB28
    { "pi with macron", eString },  //           U+EB29
    { "Q", eString },  //                        U+EB2A
    { "1", eString },  //                        U+EB2B
    { "3", eString },  //                        U+EB2C
    { "E", eString },  //                        U+EB2D
    { "e", eString },  //                        U+EB2E
    { "g", eString },  //                        U+EB2F
    { "j", eString },  //                        U+EB30
    { "raised chempoint", eString },  //         U+EB31
    { "epsilon-vector", eString },  //           U+EB32
    { "l", eString },  //                        U+EB33
    { "M", eString },  //                        U+EB34
    { "l", eString },  //                        U+EB35
    { "A", eString },  //                        U+EB36
    { "phi", eString },  //                      U+EB37
    { "pupsil", eString },  //                   U+EB38
    { "h with umlaut", eString },  //            U+EB39
    { "- - - -", eString },  //                  U+EB3A
    { "F with hat", eString },  //               U+EB3B
    { "rectangle", eString },  //                U+EB3C
    { "bold dash", eString },  //                U+EB3D
    { "diamond", eString },  //                  U+EB3E
    { "R", eString },  //                        U+EB3F
    { "omega", eString },  //                    U+EB40
    { "r", eString },  //                        U+EB41
    { "circle w/criscross", eString },  //       U+EB42
    { "n", eString },  //                        U+EB43
    { "j", eString },  //                        U+EB44
    { "d", eString },  //                        U+EB45
    { "g", eString },  //                        U+EB46
    { "o", eString },  //                        U+EB47
    { "c", eString },  //                        U+EB48
    { "dotted_times", eString },  //             U+EB49
    { "large asterisk", eString },  //           U+EB4A
    { 0, eString },  //                       U+EB4B
    { 0, eString },  //                       U+EB4C
    { 0, eString },  //                       U+EB4D
    { 0, eString },  //                       U+EB4E
    { 0, eString },  //                       U+EB4F
    { 0, eString },  //                       U+EB50
    { 0, eString },  //                       U+EB51
    { 0, eString },  //                       U+EB52
    { 0, eString },  //                       U+EB53
    { 0, eString },  //                       U+EB54
    { 0, eString },  //                       U+EB55
    { 0, eString },  //                       U+EB56
    { 0, eString },  //                       U+EB57
    { 0, eString },  //                       U+EB58
    { 0, eString },  //                       U+EB59
    { 0, eString },  //                       U+EB5A
    { 0, eString },  //                       U+EB5B
    { 0, eString },  //                       U+EB5C
    { 0, eString },  //                       U+EB5D
    { 0, eString },  //                       U+EB5E
    { 0, eString },  //                       U+EB5F
    { 0, eString },  //                       U+EB60
    { 0, eString },  //                       U+EB61
    { 0, eString },  //                       U+EB62
    { 0, eString },  //                       U+EB63
    { 0, eString },  //                       U+EB64
    { 0, eString },  //                       U+EB65
    { 0, eString },  //                       U+EB66
    { 0, eString },  //                       U+EB67
    { 0, eString },  //                       U+EB68
    { 0, eString },  //                       U+EB69
    { 0, eString },  //                       U+EB6A
    { 0, eString },  //                       U+EB6B
    { 0, eString },  //                       U+EB6C
    { 0, eString },  //                       U+EB6D
    { 0, eString },  //                       U+EB6E
    { 0, eString },  //                       U+EB6F
    { 0, eString },  //                       U+EB70
    { 0, eString },  //                       U+EB71
    { 0, eString },  //                       U+EB72
    { 0, eString },  //                       U+EB73
    { 0, eString },  //                       U+EB74
    { 0, eString },  //                       U+EB75
    { 0, eString },  //                       U+EB76
    { 0, eString },  //                       U+EB77
    { 0, eString },  //                       U+EB78
    { 0, eString },  //                       U+EB79
    { 0, eString },  //                       U+EB7A
    { 0, eString },  //                       U+EB7B
    { 0, eString },  //                       U+EB7C
    { 0, eString },  //                       U+EB7D
    { 0, eString },  //                       U+EB7E
    { 0, eString },  //                       U+EB7F
    { 0, eString },  //                       U+EB80
    { 0, eString },  //                       U+EB81
    { 0, eString },  //                       U+EB82
    { 0, eString },  //                       U+EB83
    { 0, eString },  //                       U+EB84
    { 0, eString },  //                       U+EB85
    { 0, eString },  //                       U+EB86
    { 0, eString },  //                       U+EB87
    { 0, eString },  //                       U+EB88
    { 0, eString },  //                       U+EB89
    { 0, eString },  //                       U+EB8A
    { 0, eString },  //                       U+EB8B
    { 0, eString },  //                       U+EB8C
    { 0, eString },  //                       U+EB8D
    { 0, eString },  //                       U+EB8E
    { 0, eString },  //                       U+EB8F
    { 0, eString },  //                       U+EB90
    { 0, eString },  //                       U+EB91
    { 0, eString },  //                       U+EB92
    { 0, eString },  //                       U+EB93
    { 0, eString },  //                       U+EB94
    { 0, eString },  //                       U+EB95
    { 0, eString },  //                       U+EB96
    { 0, eString },  //                       U+EB97
    { 0, eString },  //                       U+EB98
    { 0, eString },  //                       U+EB99
    { 0, eString },  //                       U+EB9A
    { 0, eString },  //                       U+EB9B
    { 0, eString },  //                       U+EB9C
    { 0, eString },  //                       U+EB9D
    { 0, eString },  //                       U+EB9E
    { 0, eString },  //                       U+EB9F
    { 0, eString },  //                       U+EBA0
    { 0, eString },  //                       U+EBA1
    { 0, eString },  //                       U+EBA2
    { 0, eString },  //                       U+EBA3
    { 0, eString },  //                       U+EBA4
    { 0, eString },  //                       U+EBA5
    { 0, eString },  //                       U+EBA6
    { 0, eString },  //                       U+EBA7
    { 0, eString },  //                       U+EBA8
    { 0, eString },  //                       U+EBA9
    { 0, eString },  //                       U+EBAA
    { 0, eString },  //                       U+EBAB
    { 0, eString },  //                       U+EBAC
    { 0, eString },  //                       U+EBAD
    { 0, eString },  //                       U+EBAE
    { 0, eString },  //                       U+EBAF
    { 0, eString },  //                       U+EBB0
    { 0, eString },  //                       U+EBB1
    { 0, eString },  //                       U+EBB2
    { 0, eString },  //                       U+EBB3
    { 0, eString },  //                       U+EBB4
    { 0, eString },  //                       U+EBB5
    { 0, eString },  //                       U+EBB6
    { 0, eString },  //                       U+EBB7
    { 0, eString },  //                       U+EBB8
    { 0, eString },  //                       U+EBB9
    { 0, eString },  //                       U+EBBA
    { 0, eString },  //                       U+EBBB
    { 0, eString },  //                       U+EBBC
    { 0, eString },  //                       U+EBBD
    { 0, eString },  //                       U+EBBE
    { 0, eString },  //                       U+EBBF
    { 0, eString },  //                       U+EBC0
    { 0, eString },  //                       U+EBC1
    { 0, eString },  //                       U+EBC2
    { 0, eString },  //                       U+EBC3
    { 0, eString },  //                       U+EBC4
    { 0, eString },  //                       U+EBC5
    { 0, eString },  //                       U+EBC6
    { 0, eString },  //                       U+EBC7
    { 0, eString },  //                       U+EBC8
    { 0, eString },  //                       U+EBC9
    { 0, eString },  //                       U+EBCA
    { 0, eString },  //                       U+EBCB
    { 0, eString },  //                       U+EBCC
    { 0, eString },  //                       U+EBCD
    { 0, eString },  //                       U+EBCE
    { 0, eString },  //                       U+EBCF
    { 0, eString },  //                       U+EBD0
    { 0, eString },  //                       U+EBD1
    { 0, eString },  //                       U+EBD2
    { 0, eString },  //                       U+EBD3
    { 0, eString },  //                       U+EBD4
    { 0, eString },  //                       U+EBD5
    { 0, eString },  //                       U+EBD6
    { 0, eString },  //                       U+EBD7
    { 0, eString },  //                       U+EBD8
    { 0, eString },  //                       U+EBD9
    { 0, eString },  //                       U+EBDA
    { 0, eString },  //                       U+EBDB
    { 0, eString },  //                       U+EBDC
    { 0, eString },  //                       U+EBDD
    { 0, eString },  //                       U+EBDE
    { 0, eString },  //                       U+EBDF
    { 0, eString },  //                       U+EBE0
    { 0, eString },  //                       U+EBE1
    { 0, eString },  //                       U+EBE2
    { 0, eString },  //                       U+EBE3
    { 0, eString },  //                       U+EBE4
    { 0, eString },  //                       U+EBE5
    { 0, eString },  //                       U+EBE6
    { 0, eString },  //                       U+EBE7
    { 0, eString },  //                       U+EBE8
    { 0, eString },  //                       U+EBE9
    { 0, eString },  //                       U+EBEA
    { 0, eString },  //                       U+EBEB
    { 0, eString },  //                       U+EBEC
    { 0, eString },  //                       U+EBED
    { 0, eString },  //                       U+EBEE
    { 0, eString },  //                       U+EBEF
    { 0, eString },  //                       U+EBF0
    { 0, eString },  //                       U+EBF1
    { 0, eString },  //                       U+EBF2
    { 0, eString },  //                       U+EBF3
    { 0, eString },  //                       U+EBF4
    { 0, eString },  //                       U+EBF5
    { 0, eString },  //                       U+EBF6
    { 0, eString },  //                       U+EBF7
    { 0, eString },  //                       U+EBF8
    { 0, eString },  //                       U+EBF9
    { 0, eString },  //                       U+EBFA
    { 0, eString },  //                       U+EBFB
    { 0, eString },  //                       U+EBFC
    { 0, eString },  //                       U+EBFD
    { 0, eString },  //                       U+EBFE
    { 0, eString },  //                       U+EBFF
};

/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2004/05/06 18:21:25  gouriano
 * Imported from pubmed/xmldb
 *
 * ==========================================================================
 */
