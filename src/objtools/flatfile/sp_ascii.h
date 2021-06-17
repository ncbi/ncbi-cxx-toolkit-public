/* sp_ascii.h
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
 * File Name:  sp_ascii.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *      Build SWISS-PROT format entry block.
 *
 */

#ifndef _SPASCII_
#define _SPASCII_

#define ParFlatSPSites       1
#define ParFlatSPBonds       2
#define ParFlatSPRegions     3
#define ParFlatSPImports     4
#define ParFlatSPInitMet     5
#define ParFlatSPNonTer      6
#define ParFlatSPNonCons     7

typedef struct sprot_feat_type {
    const char *inkey;                  /* input key string */
    Uint1      type;                    /* SITES, REGIONS, BONDS, IMPORTS */
    Int4       keyint;                  /* output keyname for SITES, BONDS */
    const char *keystring;              /* output keyname for REGIONS, IMPORTS,
                                           or description string from SITES */
} SPFeatType, *SPFeatTypePtr;

/* Table of valid Comment topic (CC line), change
 * "comment-topic-key:" to "[comment-topic-key]"
 *
 *  CC   -!- SUBCELLULAR LOCATION: POSSIBLY ASSOCIATED WITH, AND ANCHORED TO,
 *  CC       THE CYTOPLASMIC SIDE OF THE MEMBRANE.
 */

/* "POLYMORPHISM" added in 29.0 release  June 1994
 */

/* "DOMAIN" added in 30.0 release
 */

/* "ALTERNATIVE SPLICING" changed to ALTERNATIVE PRODUCTS in 30.0 release
 */
BEGIN_NCBI_SCOPE

bool SprotAscii(ParserPtr pp);

END_NCBI_SCOPE
#endif
