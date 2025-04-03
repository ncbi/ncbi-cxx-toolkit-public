/* $Id$
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
 * File Name: ind.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *      Indexing functions used for references.
 *
 */

#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include "index.h"
#include "ind.hpp"
#include "utilfun.h"

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "ind.cpp"

BEGIN_NCBI_SCOPE

static const char* ref_tag_gb[] = {
    "LOCUS",      /*  0 */
    "DEFINITION", /*  1 */
    "ACCESSION",  /*  2 */
    "KEYWORDS",   /*  3 */
    "SOURCE",     /*  4 */
    "REFERENCE",  /*  5 */
    "FEATURES",   /*  6 */
    "BASE COUNT", /*  7 */
    "ORIGIN",     /*  8 */
    "END",        /*  9 */
    "",           /* 10 */
    "",           /* 11 */
    "",           /* 12 */
    "",           /* 13 */
    "",           /* 14 */
    "",           /* 15 */
    "",           /* 16 */
    "",           /* 17 */
    "",           /* 18 */
    "",           /* 19 */
    "",           /* 20 */
    "",           /* 21 */
    "",           /* 22 */
    "",           /* 23 */
    "  AUTHORS",  /* 24 */
    "  CONSRTM",  /* 25 */
    "  TITLE",    /* 26 */
    "  JOURNAL",  /* 27 */
    "  STANDARD", /* 28 */
    "",           /* 29 */
    "  MEDLINE",  /* 30 */
    "  REMARK",   /* 31 */
    "   PUBMED",  /* 32 */
    nullptr
};

static const char* ref_tag_embl[] = {
    "ID",   /*  0 = locus */
    "AC",   /*  1 = accession # */
    "NI",   /*  2 */
    "DT",   /*  3 = date */
    "DE",   /*  4 = description */
    "KW",   /*  5 = keywords */
    "OS",   /*  6 = organism species */
    "RN",   /*  7 = reference number */
    "DR",   /*  8 = database cross-reference */
    "CC",   /*  9 = comments or notes */
    "FH",   /* 10 = feature table header */
    "SQ",   /* 11 = sequence header */
    "SV",   /* 12 */
    "CO",   /* 13 */
    "AH",   /* 14 */
    "PR",   /* 15 */
    "END",  /* 16 */
    "",     /* 17 */
    "",     /* 18 */
    "",     /* 19 */
    "  OC", /* 20 = organism classification */
    "  OG", /* 21 = organelle */
    "  RC", /* 22 = reference comment */
    "  RP", /* 23 = reference positions */
    "  RX", /* 24 = Medline ID */
    "  RG", /* 25 */
    "  RA", /* 26 = reference authors */
    "  RT", /* 27 = reference title */
    "  RL", /* 28 = reference journal */
    "  FT", /* 29 = feature table data */
    nullptr
};

/**********************************************************/
void ind_subdbp(DataBlk& dbp, DataBlk* ind[], int maxkw, Parser::EFormat bank)
{
    const char** ref_tag;
    char*        s;
    char*        ss;
    char*        sx;
    int          n_rest;
    size_t       i;
    int          j;

    if (bank == Parser::EFormat::GenBank)
        ref_tag = ref_tag_gb;
    else if (bank == Parser::EFormat::EMBL)
        ref_tag = ref_tag_embl;
    else
        return;

    for (j = 20; j < maxkw; j++)
        ind[j] = nullptr;

    n_rest = 0;
    for (auto& subdbp : dbp.GetSubBlocks()) {
        if (ind[subdbp.mType]) {
            if (n_rest >= 21) {
                fprintf(stderr, "Too many rest\n");
                exit(1);
            }
            n_rest++;
        } else
            ind[subdbp.mType] = &subdbp;

        i                                = StringLen(ref_tag[subdbp.mType]);
        subdbp.mBuf.ptr[subdbp.mBuf.len - 1] = '\0';
        for (s = subdbp.mBuf.ptr + i; isspace((int)*s) != 0; s++)
            i++;
        subdbp.mBuf.ptr += i;
        subdbp.mBuf.len -= (i + 1);
        sx = nullptr;
        for (s = subdbp.mBuf.ptr; *s != '\0'; s++) {
            if (*s != '\n')
                continue;

            if (s[1] == 'X' && s[2] == 'X') {
                if (! sx)
                    sx = s;
                continue;
            }

            for (ss = s + i; isspace((int)*ss) != 0;)
                ss++;
            if (sx)
                s = sx;
            sx = nullptr;
            *s = ' ';
            fta_StringCpy(s + 1, ss);
            subdbp.mBuf.len -= (ss - s - 1);
        }
        if (sx) {
            *sx             = '\0';
            subdbp.mBuf.len = sx - subdbp.mBuf.ptr;
        }
    }
}

END_NCBI_SCOPE
