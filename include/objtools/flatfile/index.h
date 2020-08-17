/* index.h
 *
 * ===========================================================================
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
 * ===========================================================================
 *
 * File Name:  index.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *      Main parser defines.
 *
 */

#ifndef _INDEX_
#define _INDEX_

#include <objtools/flatfile/ftablock.h>

/* flat file format & source
 */
#define ParFlat_EMBL                1
#define ParFlat_GENBANK             2
#define ParFlat_PIR                 3
#define ParFlat_SPROT               4
#define ParFlat_NCBI                5
#define ParFlat_LANL                6
#define ParFlat_DDBJ                7
#define ParFlat_FLYBASE             8
#define ParFlat_REFSEQ              9
#define ParFlat_PRF                 10
#define ParFlat_XML                 11
#define ParFlat_ALL                 12

#define ParFlat_EMBL_AC             "AFVXYZ"    /* patent is "A"
                                                   dbEST = "F" */
#define ParFlat_LANL_AC             "JKLM"
#define ParFlat_PIR_AC              NULL
#define ParFlat_PRF_AC              NULL
#define ParFlat_SPROT_AC            NULL
#define ParFlat_DDBJ_AC             "CDE"
#define ParFlat_NCBI_AC             "BGHIJKLMRSTUWN"    /* backbone = "S",
                                                           patent = "I",
                                                           dirsub = "U",
                                                           dbSTS = "G" */
                                                        /* dbEST = "T" and
                                                           "HR" added 02/01/95
                                                           BNW reserved
                                                           for ESTs */
                                                        /* "N" reserved
                                                           for NCBI, ilegal
                                                           for now */

/* genbank format old style LOCUS line column position
 */
#define ParFlat_COL_BASES           22
#define ParFlat_COL_BP              30
#define ParFlat_COL_STRAND          33
#define ParFlat_COL_MOLECULE        36
#define ParFlat_COL_TOPOLOGY        42
#define ParFlat_COL_DIV             52
#define ParFlat_COL_DATE            62

/* genbank format new style LOCUS line column position
 */
#define ParFlat_COL_BASES_NEW       29
#define ParFlat_COL_BP_NEW          41
#define ParFlat_COL_STRAND_NEW      44
#define ParFlat_COL_MOLECULE_NEW    47
#define ParFlat_COL_TOPOLOGY_NEW    55
#define ParFlat_COL_DIV_NEW         64
#define ParFlat_COL_DATE_NEW        68

#define ParFlat_REF_END             26
#define ParFlat_REF_BTW             27
#define ParFlat_REF_SITES           28
#define ParFlat_REF_NO_TARGET       29

#define ParFlat_COL_FEATKEY         5
#define ParFlat_COL_FEATDAT         21

const Char **GetAccArray(Int2 whose);
Uint1        GetNucAccOwner(const char* acc, short* source, bool is_tpa);
Uint1        GetProtAccOwner(const char* acc);
void         FreeParser(ParserPtr pp);
void         ind_subdbp(DataBlkPtr dbp, DataBlkPtr ind[], int maxkw, int bank);

#endif
