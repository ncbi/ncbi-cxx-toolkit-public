/* prf_index.c
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
 * File Name:  prf_index.c
 *
 * Author: Sergey Bazhin
 *
 * File Description:
 * -----------------
 *      Build PRF format index block. Parses PRF to memory blocks.
 *
 */

// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include "index.h"
#include "prf_index.h"

#include "ftaerr.hpp"
#include "indx_blk.h"
#include "indx_def.h"
#include "utilfun.h"
#include "entry.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "prf_index.cpp"

BEGIN_NCBI_SCOPE

KwordBlk prfkwl[] = {
    {">ref;", 5},   {"NAME", 4},    {"SOURCE", 6},   {"JOURNAL", 7},
    {"KEYWORD", 7}, {"COMMENT", 7}, {"CROSSREF", 8}, {"SEQUENCE", 8},
    {"END", 3},     {NULL, 0}
};

/**********************************************************/
static Uint1 prf_err_field(const char *name)
{
    ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingField,
              "Missing %s line. Entry dropped.", name);
    return(1);
}

/**********************************************************/
static Uint1 prf_err_order(const char *name1, const char *name2)
{
    ErrPostEx(SEV_ERROR, ERR_FORMAT_LineTypeOrder,
              "Line %s precedes %s. Entry dropped.", name1, name2);
    return(1);
}

/**********************************************************
 *
 *   bool PRFIndex(pp, (*fun)()):
 *
 *                                              3-26-93
 *
 **********************************************************/
bool PrfIndex(ParserPtr pp, void (*fun)(IndexblkPtr entry, char* offset, Int4 len))
{
    FinfoBlkPtr   finfo;
    Int4          got_NAME;
    Int4          got_SOURCE;
    Int4          got_JOURNAL;
    Int4          got_KEYWORD;
    Int4          got_COMMENT;
    Int4          got_CROSSREF;
    Int4          got_SEQUENCE;
    bool          end_of_file;
    IndexblkPtr   entry;
    DataBlkPtr    data;
    Int4          i;
    Int4          indx = 0;
    IndBlkNextPtr ibnp;
    IndBlkNextPtr tibnp;
    const char    *p;

    finfo = (FinfoBlkPtr) MemNew(sizeof(FinfoBlk));

    end_of_file = SkipTitleBuf(pp->ffbuf, finfo, prfkwl[ParFlatPRF_ref].str, prfkwl[ParFlatPRF_ref].len);

    if(end_of_file)
    {
        MsgSkipTitleFail("PRF", finfo);
        return false;
    }

    ibnp = (IndBlkNextPtr) MemNew(sizeof(IndBlkNext));
    ibnp->next = NULL;
    tibnp = ibnp;

    while(!end_of_file)
    {
        entry = InitialEntry(pp, finfo);

        if(entry != NULL)
        {
            pp->curindx = indx;
            tibnp->next = (IndBlkNextPtr) MemNew(sizeof(IndBlkNext));
            tibnp = tibnp->next;
            tibnp->ibp = entry;
            tibnp->next = NULL;

            indx++;

            got_NAME = -1;
            got_SOURCE = -1;
            got_JOURNAL = -1;
            got_KEYWORD = -1;
            got_COMMENT = -1;
            got_CROSSREF = -1;
            got_SEQUENCE = -1;

            p = NULL;
            for(i = 0; !end_of_file &&
                       StringNCmp(finfo->str, prfkwl[ParFlatPRF_END].str,
                                  prfkwl[ParFlatPRF_END].len) != 0; i++)
            {
                if(got_SEQUENCE > -1 && isalpha(finfo->str[0]) != 0)
                {
                    ErrPostStr(SEV_ERROR, ERR_FORMAT_MissingEnd,
                               "Missing end of the entry. Entry dropped.");
                    entry->drop = 1;
                    break;
                }
                if(StringNCmp(finfo->str, prfkwl[ParFlatPRF_NAME].str,
                              prfkwl[ParFlatPRF_NAME].len) == 0)
                {
                    if(got_NAME > -1)
                    {
                        p = prfkwl[ParFlatPRF_NAME].str;
                        break;
                    }
                    got_NAME = i;
                }
                else if(StringNCmp(finfo->str, prfkwl[ParFlatPRF_SOURCE].str,
                                   prfkwl[ParFlatPRF_SOURCE].len) == 0)
                {
                    if(got_SOURCE > -1)
                    {
                        p = prfkwl[ParFlatPRF_SOURCE].str;
                        break;
                    }
                    got_SOURCE = i;
                }
                else if(StringNCmp(finfo->str, prfkwl[ParFlatPRF_JOURNAL].str,
                                   prfkwl[ParFlatPRF_JOURNAL].len) == 0)
                    got_JOURNAL = i;
                else if(StringNCmp(finfo->str, prfkwl[ParFlatPRF_KEYWORD].str,
                                   prfkwl[ParFlatPRF_KEYWORD].len) == 0)
                {
                    if(got_KEYWORD > -1)
                    {
                        p = prfkwl[ParFlatPRF_KEYWORD].str;
                        break;
                    }
                    got_KEYWORD = i;
                }
                else if(StringNCmp(finfo->str, prfkwl[ParFlatPRF_COMMENT].str,
                                   prfkwl[ParFlatPRF_COMMENT].len) == 0)
                {
                    if(got_COMMENT > -1)
                    {
                        p = prfkwl[ParFlatPRF_COMMENT].str;
                        break;
                    }
                    got_COMMENT = i;
                }
                else if(StringNCmp(finfo->str, prfkwl[ParFlatPRF_CROSSREF].str,
                                   prfkwl[ParFlatPRF_CROSSREF].len) == 0)
                {
                    if(got_CROSSREF > -1)
                    {
                        p = prfkwl[ParFlatPRF_CROSSREF].str;
                        break;
                    }
                    got_CROSSREF = i;
                }
                else if(StringNCmp(finfo->str, prfkwl[ParFlatPRF_SEQUENCE].str,
                                   prfkwl[ParFlatPRF_SEQUENCE].len) == 0)
                {
                    if(got_SEQUENCE > -1)
                    {
                        p = prfkwl[ParFlatPRF_SEQUENCE].str;
                        break;
                    }
                    got_SEQUENCE = i;
                }

                end_of_file = XReadFileBuf(pp->ffbuf, finfo);
            }

            if(entry->drop != 1)
            {
                if(p != NULL)
                {
                    ErrPostEx(SEV_ERROR, ERR_FORMAT_LineTypeOrder,
                              "Multiple %s lines in one entry.", p);
                    entry->drop = 1;
                }
                if(got_NAME < 0)
                    entry->drop = prf_err_field("NAME");

                if(got_SOURCE < 0)
                    entry->drop = prf_err_field("SOURCE");

                if(got_SEQUENCE < 0)
                    entry->drop = prf_err_field("SEQUENCE");
            }

            if(entry->drop != 1)
            {
                if(got_SEQUENCE > -1 && got_SEQUENCE < got_CROSSREF)
                    entry->drop = prf_err_order("SEQUENCE", "CROSSREF");
                if(got_CROSSREF > -1 && got_CROSSREF < got_COMMENT)
                    entry->drop = prf_err_order("CROSSREF", "COMMENT");
                if(got_COMMENT > -1 && got_COMMENT < got_KEYWORD)
                    entry->drop = prf_err_order("COMMENT", "KEYWORD");
                if(got_KEYWORD > -1 && got_KEYWORD < got_JOURNAL)
                    entry->drop = prf_err_order("KEYWORD", "JOURNAL");
                if(got_JOURNAL > -1 && got_JOURNAL < got_SOURCE)
                    entry->drop = prf_err_order("JOURNAL", "SOURCE");
                if(got_SOURCE > -1 && got_SOURCE < got_NAME)
                    entry->drop = prf_err_order("SOURCE", "NAME");
            }
            
            entry->len = (size_t) (pp->ffbuf.current - pp->ffbuf.start) - entry->offset;

            if(fun != NULL)
            {
                data = LoadEntry(pp, entry->offset, entry->len);
                (*fun)(entry, data->mOffset, static_cast<Int4>(data->len));
                FreeEntry(data);
            }
        } /* if, entry */
        else
        {
            end_of_file = FindNextEntryBuf(end_of_file, pp->ffbuf, finfo,
                                           prfkwl[ParFlatPRF_END].str,
                                           prfkwl[ParFlatPRF_END].len);
        }
        end_of_file = FindNextEntryBuf(end_of_file, pp->ffbuf, finfo,
                                       prfkwl[ParFlatPRF_ref].str,
                                       prfkwl[ParFlatPRF_ref].len);

    } /* while, end_of_file */

    pp->indx = indx;

    pp->entrylist = (IndexblkPtr*) MemNew(indx * sizeof(IndexblkPtr));
    tibnp = ibnp->next;
    MemFree(ibnp);
    for(i = 0; i < indx && tibnp != NULL; i++, tibnp = ibnp)
    {
        pp->entrylist[i] = tibnp->ibp;
        ibnp = tibnp->next;
        MemFree(tibnp);
    }
    MemFree(finfo);

    return(end_of_file);
}

END_NCBI_SCOPE
// LCOV_EXCL_STOP
