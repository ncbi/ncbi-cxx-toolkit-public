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
 *    Plan FE
 *
 */

static TUnicodePlan s_Plan_FEh = {
    { 0, eString },  // U+FE00
    { 0, eString },  // U+FE01
    { 0, eString },  // U+FE02
    { 0, eString },  // U+FE03
    { 0, eString },  // U+FE04
    { 0, eString },  // U+FE05
    { 0, eString },  // U+FE06
    { 0, eString },  // U+FE07
    { 0, eString },  // U+FE08
    { 0, eString },  // U+FE09
    { 0, eString },  // U+FE0A
    { 0, eString },  // U+FE0B
    { 0, eString },  // U+FE0C
    { 0, eString },  // U+FE0D
    { 0, eString },  // U+FE0E
    { 0, eString },  // U+FE0F
    { 0, eString },  // U+FE10
    { 0, eString },  // U+FE11
    { 0, eString },  // U+FE12
    { 0, eString },  // U+FE13
    { 0, eString },  // U+FE14
    { 0, eString },  // U+FE15
    { 0, eString },  // U+FE16
    { 0, eString },  // U+FE17
    { 0, eString },  // U+FE18
    { 0, eString },  // U+FE19
    { 0, eString },  // U+FE1A
    { 0, eString },  // U+FE1B
    { 0, eString },  // U+FE1C
    { 0, eString },  // U+FE1D
    { 0, eString },  // U+FE1E
    { 0, eString },  // U+FE1F
    { "", eString },  //   U+FE20
    { "", eString },  //   U+FE21
    { "", eString },  //   U+FE22
    { "", eString },  //   U+FE23
    { "", eString },  //   U+FE24
    { "", eString },  //   U+FE25
    { "", eString },  //   U+FE26
    { "", eString },  //   U+FE27
    { "", eString },  //   U+FE28
    { "", eString },  //   U+FE29
    { "", eString },  //   U+FE2A
    { "", eString },  //   U+FE2B
    { "", eString },  //   U+FE2C
    { "", eString },  //   U+FE2D
    { "", eString },  //   U+FE2E
    { "", eString },  //   U+FE2F
    { 0, eString },  // U+FE30
    { 0, eString },  // U+FE31
    { 0, eString },  // U+FE32
    { 0, eString },  // U+FE33
    { 0, eString },  // U+FE34
    { 0, eString },  // U+FE35
    { 0, eString },  // U+FE36
    { 0, eString },  // U+FE37
    { 0, eString },  // U+FE38
    { 0, eString },  // U+FE39
    { 0, eString },  // U+FE3A
    { 0, eString },  // U+FE3B
    { 0, eString },  // U+FE3C
    { 0, eString },  // U+FE3D
    { 0, eString },  // U+FE3E
    { 0, eString },  // U+FE3F
    { 0, eString },  // U+FE40
    { 0, eString },  // U+FE41
    { 0, eString },  // U+FE42
    { 0, eString },  // U+FE43
    { 0, eString },  // U+FE44
    { 0, eString },  // U+FE45
    { 0, eString },  // U+FE46
    { 0, eString },  // U+FE47
    { 0, eString },  // U+FE48
    { 0, eString },  // U+FE49
    { 0, eString },  // U+FE4A
    { 0, eString },  // U+FE4B
    { 0, eString },  // U+FE4C
    { 0, eString },  // U+FE4D
    { 0, eString },  // U+FE4E
    { 0, eString },  // U+FE4F
    { 0, eString },  // U+FE50
    { 0, eString },  // U+FE51
    { 0, eString },  // U+FE52
    { 0, eString },  // U+FE53
    { 0, eString },  // U+FE54
    { 0, eString },  // U+FE55
    { 0, eString },  // U+FE56
    { 0, eString },  // U+FE57
    { 0, eString },  // U+FE58
    { 0, eString },  // U+FE59
    { 0, eString },  // U+FE5A
    { 0, eString },  // U+FE5B
    { 0, eString },  // U+FE5C
    { 0, eString },  // U+FE5D
    { 0, eString },  // U+FE5E
    { 0, eString },  // U+FE5F
    { 0, eString },  // U+FE60
    { 0, eString },  // U+FE61
    { 0, eString },  // U+FE62
    { 0, eString },  // U+FE63
    { 0, eString },  // U+FE64
    { 0, eString },  // U+FE65
    { 0, eString },  // U+FE66
    { 0, eString },  // U+FE67
    { 0, eString },  // U+FE68
    { 0, eString },  // U+FE69
    { 0, eString },  // U+FE6A
    { 0, eString },  // U+FE6B
    { 0, eString },  // U+FE6C
    { 0, eString },  // U+FE6D
    { 0, eString },  // U+FE6E
    { 0, eString },  // U+FE6F
    { 0, eString },  // U+FE70
    { 0, eString },  // U+FE71
    { 0, eString },  // U+FE72
    { 0, eString },  // U+FE73
    { 0, eString },  // U+FE74
    { 0, eString },  // U+FE75
    { 0, eString },  // U+FE76
    { 0, eString },  // U+FE77
    { 0, eString },  // U+FE78
    { 0, eString },  // U+FE79
    { 0, eString },  // U+FE7A
    { 0, eString },  // U+FE7B
    { 0, eString },  // U+FE7C
    { 0, eString },  // U+FE7D
    { 0, eString },  // U+FE7E
    { 0, eString },  // U+FE7F
    { 0, eString },  // U+FE80
    { 0, eString },  // U+FE81
    { 0, eString },  // U+FE82
    { 0, eString },  // U+FE83
    { 0, eString },  // U+FE84
    { 0, eString },  // U+FE85
    { 0, eString },  // U+FE86
    { 0, eString },  // U+FE87
    { 0, eString },  // U+FE88
    { 0, eString },  // U+FE89
    { 0, eString },  // U+FE8A
    { 0, eString },  // U+FE8B
    { 0, eString },  // U+FE8C
    { 0, eString },  // U+FE8D
    { 0, eString },  // U+FE8E
    { 0, eString },  // U+FE8F
    { 0, eString },  // U+FE90
    { 0, eString },  // U+FE91
    { 0, eString },  // U+FE92
    { 0, eString },  // U+FE93
    { 0, eString },  // U+FE94
    { 0, eString },  // U+FE95
    { 0, eString },  // U+FE96
    { 0, eString },  // U+FE97
    { 0, eString },  // U+FE98
    { 0, eString },  // U+FE99
    { 0, eString },  // U+FE9A
    { 0, eString },  // U+FE9B
    { 0, eString },  // U+FE9C
    { 0, eString },  // U+FE9D
    { 0, eString },  // U+FE9E
    { 0, eString },  // U+FE9F
    { 0, eString },  // U+FEA0
    { 0, eString },  // U+FEA1
    { 0, eString },  // U+FEA2
    { 0, eString },  // U+FEA3
    { 0, eString },  // U+FEA4
    { 0, eString },  // U+FEA5
    { 0, eString },  // U+FEA6
    { 0, eString },  // U+FEA7
    { 0, eString },  // U+FEA8
    { 0, eString },  // U+FEA9
    { 0, eString },  // U+FEAA
    { 0, eString },  // U+FEAB
    { 0, eString },  // U+FEAC
    { 0, eString },  // U+FEAD
    { 0, eString },  // U+FEAE
    { 0, eString },  // U+FEAF
    { 0, eString },  // U+FEB0
    { 0, eString },  // U+FEB1
    { 0, eString },  // U+FEB2
    { 0, eString },  // U+FEB3
    { 0, eString },  // U+FEB4
    { 0, eString },  // U+FEB5
    { 0, eString },  // U+FEB6
    { 0, eString },  // U+FEB7
    { 0, eString },  // U+FEB8
    { 0, eString },  // U+FEB9
    { 0, eString },  // U+FEBA
    { 0, eString },  // U+FEBB
    { 0, eString },  // U+FEBC
    { 0, eString },  // U+FEBD
    { 0, eString },  // U+FEBE
    { 0, eString },  // U+FEBF
    { 0, eString },  // U+FEC0
    { 0, eString },  // U+FEC1
    { 0, eString },  // U+FEC2
    { 0, eString },  // U+FEC3
    { 0, eString },  // U+FEC4
    { 0, eString },  // U+FEC5
    { 0, eString },  // U+FEC6
    { 0, eString },  // U+FEC7
    { 0, eString },  // U+FEC8
    { 0, eString },  // U+FEC9
    { 0, eString },  // U+FECA
    { 0, eString },  // U+FECB
    { 0, eString },  // U+FECC
    { 0, eString },  // U+FECD
    { 0, eString },  // U+FECE
    { 0, eString },  // U+FECF
    { 0, eString },  // U+FED0
    { 0, eString },  // U+FED1
    { 0, eString },  // U+FED2
    { 0, eString },  // U+FED3
    { 0, eString },  // U+FED4
    { 0, eString },  // U+FED5
    { 0, eString },  // U+FED6
    { 0, eString },  // U+FED7
    { 0, eString },  // U+FED8
    { 0, eString },  // U+FED9
    { 0, eString },  // U+FEDA
    { 0, eString },  // U+FEDB
    { 0, eString },  // U+FEDC
    { 0, eString },  // U+FEDD
    { 0, eString },  // U+FEDE
    { 0, eString },  // U+FEDF
    { 0, eString },  // U+FEE0
    { 0, eString },  // U+FEE1
    { 0, eString },  // U+FEE2
    { 0, eString },  // U+FEE3
    { 0, eString },  // U+FEE4
    { 0, eString },  // U+FEE5
    { 0, eString },  // U+FEE6
    { 0, eString },  // U+FEE7
    { 0, eString },  // U+FEE8
    { 0, eString },  // U+FEE9
    { 0, eString },  // U+FEEA
    { 0, eString },  // U+FEEB
    { 0, eString },  // U+FEEC
    { 0, eString },  // U+FEED
    { 0, eString },  // U+FEEE
    { 0, eString },  // U+FEEF
    { 0, eString },  // U+FEF0
    { 0, eString },  // U+FEF1
    { 0, eString },  // U+FEF2
    { 0, eString },  // U+FEF3
    { 0, eString },  // U+FEF4
    { 0, eString },  // U+FEF5
    { 0, eString },  // U+FEF6
    { 0, eString },  // U+FEF7
    { 0, eString },  // U+FEF8
    { 0, eString },  // U+FEF9
    { 0, eString },  // U+FEFA
    { 0, eString },  // U+FEFB
    { 0, eString },  // U+FEFC
    { 0, eString },  // U+FEFD
    { 0, eString },  // U+FEFE
    { 0, eString },  // U+FEFF
};

/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2004/05/06 18:21:26  gouriano
 * Imported from pubmed/xmldb
 *
 * ==========================================================================
 */
