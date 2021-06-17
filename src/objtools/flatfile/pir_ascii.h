/* pir_ascii.h
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
 * File Name:  pir_ascii.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *
 */

// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
#ifndef PIR_ASCII_H
#define PIR_ASCII_H

#define FEATUREBLK                 18

#define ORGANISM_formal_name       1
#define ORGANISM_common_name       2
#define ORGANISM_note              4

#define DATE_                      0
#define DATE_sequence_revision     1
#define DATE_text_change           2

#define REFERENCE_authors          1
#define REFERENCE_title            2
#define REFERENCE_journal          3
#define REFERENCE_book             4
#define REFERENCE_citation         5
#define REFERENCE_submission       6
#define REFERENCE_description      7
#define REFERENCE_contents         8
#define REFERENCE_note             9
#define REFERENCE_cross_references 10

#define CLASS_superfamily          1
BEGIN_NCBI_SCOPE

typedef struct pir_feat_type {
    Int4       inlen;           /* string length of the input key string */
    Uint1      choice;          /* SITES, REGIONS, BONDS */
    Int4       keyint;          /* output keyname for SITES, BONDS */
    const char *keystr;         /* output keyname for REGIONS */
} PirFeatType, *PirFeatTypePtr;

#define MAXTAG 38

bool PirAscii(ParserPtr pp);

END_NCBI_SCOPE
#endif //PIR_ASCII_H
// LCOV_EXCL_STOP
