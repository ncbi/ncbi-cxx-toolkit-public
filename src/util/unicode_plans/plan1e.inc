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
 *    Plan 1E
 *
 */

static TUnicodePlan s_Plan_1Eh = {
    { "A", eString },  //  U+1E00
    { "a", eString },  //  U+1E01
    { "B", eString },  //  U+1E02
    { "b", eString },  //  U+1E03
    { "B", eString },  //  U+1E04
    { "b", eString },  //  U+1E05
    { "B", eString },  //  U+1E06
    { "b", eString },  //  U+1E07
    { "C", eString },  //  U+1E08
    { "c", eString },  //  U+1E09
    { "D", eString },  //  U+1E0A
    { "d", eString },  //  U+1E0B
    { "D", eString },  //  U+1E0C
    { "d", eString },  //  U+1E0D
    { "D", eString },  //  U+1E0E
    { "d", eString },  //  U+1E0F
    { "D", eString },  //  U+1E10
    { "d", eString },  //  U+1E11
    { "D", eString },  //  U+1E12
    { "d", eString },  //  U+1E13
    { "E", eString },  //  U+1E14
    { "e", eString },  //  U+1E15
    { "E", eString },  //  U+1E16
    { "e", eString },  //  U+1E17
    { "E", eString },  //  U+1E18
    { "e", eString },  //  U+1E19
    { "E", eString },  //  U+1E1A
    { "e", eString },  //  U+1E1B
    { "E", eString },  //  U+1E1C
    { "e", eString },  //  U+1E1D
    { "F", eString },  //  U+1E1E
    { "f", eString },  //  U+1E1F
    { "G", eString },  //  U+1E20
    { "g", eString },  //  U+1E21
    { "H", eString },  //  U+1E22
    { "h", eString },  //  U+1E23
    { "H", eString },  //  U+1E24
    { "h", eString },  //  U+1E25
    { "H", eString },  //  U+1E26
    { "h", eString },  //  U+1E27
    { "H", eString },  //  U+1E28
    { "h", eString },  //  U+1E29
    { "H", eString },  //  U+1E2A
    { "h", eString },  //  U+1E2B
    { "I", eString },  //  U+1E2C
    { "i", eString },  //  U+1E2D
    { "I", eString },  //  U+1E2E
    { "i", eString },  //  U+1E2F
    { "K", eString },  //  U+1E30
    { "k", eString },  //  U+1E31
    { "K", eString },  //  U+1E32
    { "k", eString },  //  U+1E33
    { "K", eString },  //  U+1E34
    { "k", eString },  //  U+1E35
    { "L", eString },  //  U+1E36
    { "l", eString },  //  U+1E37
    { "L", eString },  //  U+1E38
    { "l", eString },  //  U+1E39
    { "L", eString },  //  U+1E3A
    { "l", eString },  //  U+1E3B
    { "L", eString },  //  U+1E3C
    { "l", eString },  //  U+1E3D
    { "M", eString },  //  U+1E3E
    { "m", eString },  //  U+1E3F
    { "M", eString },  //  U+1E40
    { "m", eString },  //  U+1E41
    { "M", eString },  //  U+1E42
    { "m", eString },  //  U+1E43
    { "N", eString },  //  U+1E44
    { "n", eString },  //  U+1E45
    { "N", eString },  //  U+1E46
    { "n", eString },  //  U+1E47
    { "N", eString },  //  U+1E48
    { "n", eString },  //  U+1E49
    { "N", eString },  //  U+1E4A
    { "n", eString },  //  U+1E4B
    { "O", eString },  //  U+1E4C
    { "o", eString },  //  U+1E4D
    { "O", eString },  //  U+1E4E
    { "o", eString },  //  U+1E4F
    { "O", eString },  //  U+1E50
    { "o", eString },  //  U+1E51
    { "O", eString },  //  U+1E52
    { "o", eString },  //  U+1E53
    { "P", eString },  //  U+1E54
    { "p", eString },  //  U+1E55
    { "P", eString },  //  U+1E56
    { "p", eString },  //  U+1E57
    { "R", eString },  //  U+1E58
    { "r", eString },  //  U+1E59
    { "R", eString },  //  U+1E5A
    { "r", eString },  //  U+1E5B
    { "R", eString },  //  U+1E5C
    { "r", eString },  //  U+1E5D
    { "R", eString },  //  U+1E5E
    { "r", eString },  //  U+1E5F
    { "S", eString },  //  U+1E60
    { "s", eString },  //  U+1E61
    { "S", eString },  //  U+1E62
    { "s", eString },  //  U+1E63
    { "S", eString },  //  U+1E64
    { "s", eString },  //  U+1E65
    { "S", eString },  //  U+1E66
    { "s", eString },  //  U+1E67
    { "S", eString },  //  U+1E68
    { "s", eString },  //  U+1E69
    { "T", eString },  //  U+1E6A
    { "t", eString },  //  U+1E6B
    { "T", eString },  //  U+1E6C
    { "t", eString },  //  U+1E6D
    { "T", eString },  //  U+1E6E
    { "t", eString },  //  U+1E6F
    { "T", eString },  //  U+1E70
    { "t", eString },  //  U+1E71
    { "U", eString },  //  U+1E72
    { "u", eString },  //  U+1E73
    { "U", eString },  //  U+1E74
    { "u", eString },  //  U+1E75
    { "U", eString },  //  U+1E76
    { "u", eString },  //  U+1E77
    { "U", eString },  //  U+1E78
    { "u", eString },  //  U+1E79
    { "U", eString },  //  U+1E7A
    { "u", eString },  //  U+1E7B
    { "V", eString },  //  U+1E7C
    { "v", eString },  //  U+1E7D
    { "V", eString },  //  U+1E7E
    { "v", eString },  //  U+1E7F
    { "W", eString },  //  U+1E80
    { "w", eString },  //  U+1E81
    { "W", eString },  //  U+1E82
    { "w", eString },  //  U+1E83
    { "W", eString },  //  U+1E84
    { "w", eString },  //  U+1E85
    { "W", eString },  //  U+1E86
    { "w", eString },  //  U+1E87
    { "W", eString },  //  U+1E88
    { "w", eString },  //  U+1E89
    { "X", eString },  //  U+1E8A
    { "x", eString },  //  U+1E8B
    { "X", eString },  //  U+1E8C
    { "x", eString },  //  U+1E8D
    { "Y", eString },  //  U+1E8E
    { "y", eString },  //  U+1E8F
    { "Z", eString },  //  U+1E90
    { "z", eString },  //  U+1E91
    { "Z", eString },  //  U+1E92
    { "z", eString },  //  U+1E93
    { "Z", eString },  //  U+1E94
    { "z", eString },  //  U+1E95
    { "h", eString },  //  U+1E96
    { "t", eString },  //  U+1E97
    { "w", eString },  //  U+1E98
    { "y", eString },  //  U+1E99
    { "a", eString },  //  U+1E9A
    { "f", eString },  //  U+1E9B
    { 0, eString },  // U+1E9C
    { 0, eString },  // U+1E9D
    { 0, eString },  // U+1E9E
    { 0, eString },  // U+1E9F
    { "A", eString },  //  U+1EA0
    { "a", eString },  //  U+1EA1
    { "A", eString },  //  U+1EA2
    { "a", eString },  //  U+1EA3
    { "A", eString },  //  U+1EA4
    { "a", eString },  //  U+1EA5
    { "A", eString },  //  U+1EA6
    { "a", eString },  //  U+1EA7
    { "A", eString },  //  U+1EA8
    { "a", eString },  //  U+1EA9
    { "A", eString },  //  U+1EAA
    { "a", eString },  //  U+1EAB
    { "A", eString },  //  U+1EAC
    { "a", eString },  //  U+1EAD
    { "A", eString },  //  U+1EAE
    { "a", eString },  //  U+1EAF
    { "A", eString },  //  U+1EB0
    { "a", eString },  //  U+1EB1
    { "A", eString },  //  U+1EB2
    { "a", eString },  //  U+1EB3
    { "A", eString },  //  U+1EB4
    { "a", eString },  //  U+1EB5
    { "A", eString },  //  U+1EB6
    { "a", eString },  //  U+1EB7
    { "E", eString },  //  U+1EB8
    { "e", eString },  //  U+1EB9
    { "E", eString },  //  U+1EBA
    { "e", eString },  //  U+1EBB
    { "E", eString },  //  U+1EBC
    { "e", eString },  //  U+1EBD
    { "E", eString },  //  U+1EBE
    { "e", eString },  //  U+1EBF
    { "E", eString },  //  U+1EC0
    { "e", eString },  //  U+1EC1
    { "E", eString },  //  U+1EC2
    { "e", eString },  //  U+1EC3
    { "E", eString },  //  U+1EC4
    { "e", eString },  //  U+1EC5
    { "E", eString },  //  U+1EC6
    { "e", eString },  //  U+1EC7
    { "I", eString },  //  U+1EC8
    { "i", eString },  //  U+1EC9
    { "I", eString },  //  U+1ECA
    { "i", eString },  //  U+1ECB
    { "O", eString },  //  U+1ECC
    { "o", eString },  //  U+1ECD
    { "O", eString },  //  U+1ECE
    { "o", eString },  //  U+1ECF
    { "O", eString },  //  U+1ED0
    { "o", eString },  //  U+1ED1
    { "O", eString },  //  U+1ED2
    { "o", eString },  //  U+1ED3
    { "O", eString },  //  U+1ED4
    { "o", eString },  //  U+1ED5
    { "O", eString },  //  U+1ED6
    { "o", eString },  //  U+1ED7
    { "O", eString },  //  U+1ED8
    { "o", eString },  //  U+1ED9
    { "O", eString },  //  U+1EDA
    { "o", eString },  //  U+1EDB
    { "O", eString },  //  U+1EDC
    { "o", eString },  //  U+1EDD
    { "O", eString },  //  U+1EDE
    { "o", eString },  //  U+1EDF
    { "O", eString },  //  U+1EE0
    { "o", eString },  //  U+1EE1
    { "O", eString },  //  U+1EE2
    { "o", eString },  //  U+1EE3
    { "U", eString },  //  U+1EE4
    { "u", eString },  //  U+1EE5
    { "U", eString },  //  U+1EE6
    { "u", eString },  //  U+1EE7
    { "U", eString },  //  U+1EE8
    { "u", eString },  //  U+1EE9
    { "U", eString },  //  U+1EEA
    { "u", eString },  //  U+1EEB
    { "U", eString },  //  U+1EEC
    { "u", eString },  //  U+1EED
    { "U", eString },  //  U+1EEE
    { "u", eString },  //  U+1EEF
    { "U", eString },  //  U+1EF0
    { "u", eString },  //  U+1EF1
    { "Y", eString },  //  U+1EF2
    { "y", eString },  //  U+1EF3
    { "Y", eString },  //  U+1EF4
    { "y", eString },  //  U+1EF5
    { "Y", eString },  //  U+1EF6
    { "y", eString },  //  U+1EF7
    { "Y", eString },  //  U+1EF8
    { "y", eString },  //  U+1EF9
    { 0, eString },  // U+1EFA
    { 0, eString },  // U+1EFB
    { 0, eString },  // U+1EFC
    { 0, eString },  // U+1EFD
    { 0, eString },  // U+1EFE
    { 0, eString },  // U+1EFF
};

/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2004/05/06 18:21:26  gouriano
 * Imported from pubmed/xmldb
 *
 * ==========================================================================
 */
