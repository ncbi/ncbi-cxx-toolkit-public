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
 *    Plan FB
 *
 */

static TUnicodePlan s_Plan_FBh = {
    { " ff ", eString },  // old dictionary   U+FB00
    { " fi ", eString },  // old dictionary   U+FB01
    { " fl ", eString },  // old dictionary   U+FB02
    { " ffi ", eString },  // old dictionary  U+FB03
    { " ffl ", eString },  // old dictionary  U+FB04
    { 0, eString },  //                    U+FB05
    { 0, eString },  //                    U+FB06
    { 0, eString },  //                    U+FB07
    { 0, eString },  //                    U+FB08
    { 0, eString },  //                    U+FB09
    { 0, eString },  //                    U+FB0A
    { 0, eString },  //                    U+FB0B
    { 0, eString },  //                    U+FB0C
    { 0, eString },  //                    U+FB0D
    { 0, eString },  //                    U+FB0E
    { 0, eString },  //                    U+FB0F
    { 0, eString },  //                    U+FB10
    { 0, eString },  //                    U+FB11
    { 0, eString },  //                    U+FB12
    { 0, eString },  //                    U+FB13
    { 0, eString },  //                    U+FB14
    { 0, eString },  //                    U+FB15
    { 0, eString },  //                    U+FB16
    { 0, eString },  //                    U+FB17
    { 0, eString },  //                    U+FB18
    { 0, eString },  //                    U+FB19
    { 0, eString },  //                    U+FB1A
    { 0, eString },  //                    U+FB1B
    { 0, eString },  //                    U+FB1C
    { 0, eString },  //                    U+FB1D
    { 0, eString },  //                    U+FB1E
    { 0, eString },  //                    U+FB1F
    { 0, eString },  //                    U+FB20
    { 0, eString },  //                    U+FB21
    { 0, eString },  //                    U+FB22
    { 0, eString },  //                    U+FB23
    { 0, eString },  //                    U+FB24
    { 0, eString },  //                    U+FB25
    { 0, eString },  //                    U+FB26
    { 0, eString },  //                    U+FB27
    { 0, eString },  //                    U+FB28
    { 0, eString },  //                    U+FB29
    { 0, eString },  //                    U+FB2A
    { 0, eString },  //                    U+FB2B
    { 0, eString },  //                    U+FB2C
    { 0, eString },  //                    U+FB2D
    { 0, eString },  //                    U+FB2E
    { 0, eString },  //                    U+FB2F
    { 0, eString },  //                    U+FB30
    { 0, eString },  //                    U+FB31
    { 0, eString },  //                    U+FB32
    { 0, eString },  //                    U+FB33
    { 0, eString },  //                    U+FB34
    { 0, eString },  //                    U+FB35
    { 0, eString },  //                    U+FB36
    { 0, eString },  //                    U+FB37
    { 0, eString },  //                    U+FB38
    { 0, eString },  //                    U+FB39
    { 0, eString },  //                    U+FB3A
    { 0, eString },  //                    U+FB3B
    { 0, eString },  //                    U+FB3C
    { 0, eString },  //                    U+FB3D
    { 0, eString },  //                    U+FB3E
    { 0, eString },  //                    U+FB3F
    { 0, eString },  //                    U+FB40
    { 0, eString },  //                    U+FB41
    { 0, eString },  //                    U+FB42
    { 0, eString },  //                    U+FB43
    { 0, eString },  //                    U+FB44
    { 0, eString },  //                    U+FB45
    { 0, eString },  //                    U+FB46
    { 0, eString },  //                    U+FB47
    { 0, eString },  //                    U+FB48
    { 0, eString },  //                    U+FB49
    { 0, eString },  //                    U+FB4A
    { 0, eString },  //                    U+FB4B
    { 0, eString },  //                    U+FB4C
    { 0, eString },  //                    U+FB4D
    { 0, eString },  //                    U+FB4E
    { 0, eString },  //                    U+FB4F
    { 0, eString },  //                    U+FB50
    { 0, eString },  //                    U+FB51
    { 0, eString },  //                    U+FB52
    { 0, eString },  //                    U+FB53
    { 0, eString },  //                    U+FB54
    { 0, eString },  //                    U+FB55
    { 0, eString },  //                    U+FB56
    { 0, eString },  //                    U+FB57
    { 0, eString },  //                    U+FB58
    { 0, eString },  //                    U+FB59
    { 0, eString },  //                    U+FB5A
    { 0, eString },  //                    U+FB5B
    { 0, eString },  //                    U+FB5C
    { 0, eString },  //                    U+FB5D
    { 0, eString },  //                    U+FB5E
    { 0, eString },  //                    U+FB5F
    { 0, eString },  //                    U+FB60
    { 0, eString },  //                    U+FB61
    { 0, eString },  //                    U+FB62
    { 0, eString },  //                    U+FB63
    { 0, eString },  //                    U+FB64
    { 0, eString },  //                    U+FB65
    { 0, eString },  //                    U+FB66
    { 0, eString },  //                    U+FB67
    { 0, eString },  //                    U+FB68
    { 0, eString },  //                    U+FB69
    { 0, eString },  //                    U+FB6A
    { 0, eString },  //                    U+FB6B
    { 0, eString },  //                    U+FB6C
    { 0, eString },  //                    U+FB6D
    { 0, eString },  //                    U+FB6E
    { 0, eString },  //                    U+FB6F
    { 0, eString },  //                    U+FB70
    { 0, eString },  //                    U+FB71
    { 0, eString },  //                    U+FB72
    { 0, eString },  //                    U+FB73
    { 0, eString },  //                    U+FB74
    { 0, eString },  //                    U+FB75
    { 0, eString },  //                    U+FB76
    { 0, eString },  //                    U+FB77
    { 0, eString },  //                    U+FB78
    { 0, eString },  //                    U+FB79
    { 0, eString },  //                    U+FB7A
    { 0, eString },  //                    U+FB7B
    { 0, eString },  //                    U+FB7C
    { 0, eString },  //                    U+FB7D
    { 0, eString },  //                    U+FB7E
    { 0, eString },  //                    U+FB7F
    { 0, eString },  //                    U+FB80
    { 0, eString },  //                    U+FB81
    { 0, eString },  //                    U+FB82
    { 0, eString },  //                    U+FB83
    { 0, eString },  //                    U+FB84
    { 0, eString },  //                    U+FB85
    { 0, eString },  //                    U+FB86
    { 0, eString },  //                    U+FB87
    { 0, eString },  //                    U+FB88
    { 0, eString },  //                    U+FB89
    { 0, eString },  //                    U+FB8A
    { 0, eString },  //                    U+FB8B
    { 0, eString },  //                    U+FB8C
    { 0, eString },  //                    U+FB8D
    { 0, eString },  //                    U+FB8E
    { 0, eString },  //                    U+FB8F
    { 0, eString },  //                    U+FB90
    { 0, eString },  //                    U+FB91
    { 0, eString },  //                    U+FB92
    { 0, eString },  //                    U+FB93
    { 0, eString },  //                    U+FB94
    { 0, eString },  //                    U+FB95
    { 0, eString },  //                    U+FB96
    { 0, eString },  //                    U+FB97
    { 0, eString },  //                    U+FB98
    { 0, eString },  //                    U+FB99
    { 0, eString },  //                    U+FB9A
    { 0, eString },  //                    U+FB9B
    { 0, eString },  //                    U+FB9C
    { 0, eString },  //                    U+FB9D
    { 0, eString },  //                    U+FB9E
    { 0, eString },  //                    U+FB9F
    { 0, eString },  //                    U+FBA0
    { 0, eString },  //                    U+FBA1
    { 0, eString },  //                    U+FBA2
    { 0, eString },  //                    U+FBA3
    { 0, eString },  //                    U+FBA4
    { 0, eString },  //                    U+FBA5
    { 0, eString },  //                    U+FBA6
    { 0, eString },  //                    U+FBA7
    { 0, eString },  //                    U+FBA8
    { 0, eString },  //                    U+FBA9
    { 0, eString },  //                    U+FBAA
    { 0, eString },  //                    U+FBAB
    { 0, eString },  //                    U+FBAC
    { 0, eString },  //                    U+FBAD
    { 0, eString },  //                    U+FBAE
    { 0, eString },  //                    U+FBAF
    { 0, eString },  //                    U+FBB0
    { 0, eString },  //                    U+FBB1
    { 0, eString },  //                    U+FBB2
    { 0, eString },  //                    U+FBB3
    { 0, eString },  //                    U+FBB4
    { 0, eString },  //                    U+FBB5
    { 0, eString },  //                    U+FBB6
    { 0, eString },  //                    U+FBB7
    { 0, eString },  //                    U+FBB8
    { 0, eString },  //                    U+FBB9
    { 0, eString },  //                    U+FBBA
    { 0, eString },  //                    U+FBBB
    { 0, eString },  //                    U+FBBC
    { 0, eString },  //                    U+FBBD
    { 0, eString },  //                    U+FBBE
    { 0, eString },  //                    U+FBBF
    { 0, eString },  //                    U+FBC0
    { 0, eString },  //                    U+FBC1
    { 0, eString },  //                    U+FBC2
    { 0, eString },  //                    U+FBC3
    { 0, eString },  //                    U+FBC4
    { 0, eString },  //                    U+FBC5
    { 0, eString },  //                    U+FBC6
    { 0, eString },  //                    U+FBC7
    { 0, eString },  //                    U+FBC8
    { 0, eString },  //                    U+FBC9
    { 0, eString },  //                    U+FBCA
    { 0, eString },  //                    U+FBCB
    { 0, eString },  //                    U+FBCC
    { 0, eString },  //                    U+FBCD
    { 0, eString },  //                    U+FBCE
    { 0, eString },  //                    U+FBCF
    { 0, eString },  //                    U+FBD0
    { 0, eString },  //                    U+FBD1
    { 0, eString },  //                    U+FBD2
    { 0, eString },  //                    U+FBD3
    { 0, eString },  //                    U+FBD4
    { 0, eString },  //                    U+FBD5
    { 0, eString },  //                    U+FBD6
    { 0, eString },  //                    U+FBD7
    { 0, eString },  //                    U+FBD8
    { 0, eString },  //                    U+FBD9
    { 0, eString },  //                    U+FBDA
    { 0, eString },  //                    U+FBDB
    { 0, eString },  //                    U+FBDC
    { 0, eString },  //                    U+FBDD
    { 0, eString },  //                    U+FBDE
    { 0, eString },  //                    U+FBDF
    { 0, eString },  //                    U+FBE0
    { 0, eString },  //                    U+FBE1
    { 0, eString },  //                    U+FBE2
    { 0, eString },  //                    U+FBE3
    { 0, eString },  //                    U+FBE4
    { 0, eString },  //                    U+FBE5
    { 0, eString },  //                    U+FBE6
    { 0, eString },  //                    U+FBE7
    { 0, eString },  //                    U+FBE8
    { 0, eString },  //                    U+FBE9
    { 0, eString },  //                    U+FBEA
    { 0, eString },  //                    U+FBEB
    { 0, eString },  //                    U+FBEC
    { 0, eString },  //                    U+FBED
    { 0, eString },  //                    U+FBEE
    { 0, eString },  //                    U+FBEF
    { 0, eString },  //                    U+FBF0
    { 0, eString },  //                    U+FBF1
    { 0, eString },  //                    U+FBF2
    { 0, eString },  //                    U+FBF3
    { 0, eString },  //                    U+FBF4
    { 0, eString },  //                    U+FBF5
    { 0, eString },  //                    U+FBF6
    { 0, eString },  //                    U+FBF7
    { 0, eString },  //                    U+FBF8
    { 0, eString },  //                    U+FBF9
    { 0, eString },  //                    U+FBFA
    { 0, eString },  //                    U+FBFB
    { 0, eString },  //                    U+FBFC
    { 0, eString },  //                    U+FBFD
    { 0, eString },  //                    U+FBFE
    { 0, eString },  //                    U+FBFF
};

/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2004/05/06 18:21:26  gouriano
 * Imported from pubmed/xmldb
 *
 * ==========================================================================
 */
