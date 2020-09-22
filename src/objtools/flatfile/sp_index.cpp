/* sp_index.c
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
 * File Name:  sp_index.c
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *      Build SWISS-PROT format index block. Parsing SP to memory blocks.
 *
 */
#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include <objtools/flatfile/index.h>
#include <objtools/flatfile/sprot.h>

#include "ftaerr.hpp"
#include "indx_blk.h"
#include "indx_def.h"
#include "utilfun.h"
#include "entry.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "sp_index.cpp"

BEGIN_NCBI_SCOPE

KwordBlk spkwl[] = {
    {"ID", 2}, {"AC", 2}, {"DT", 2}, {"DE", 2}, {"GN", 2}, {"OS", 2},
    {"RN", 2}, {"CC", 2}, {"PE", 2}, {"DR", 2}, {"KW", 2}, {"FT", 2},
    {"SQ", 2}, {"//", 2}, {NULL, 0} };

/**********************************************************/
static Uint1 sp_err_field(const char *name)
{
    ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingField,
              "Missing %s line, entry dropped", name);
    return(1);
}

/**********************************************************/
static void SPGetVerNum(char* str, IndexblkPtr ibp)
{
    char* p;
    char* q;

    if(str == NULL || ibp == NULL)
        return;

    p = StringIStr(str, "sequence version");
    if(p == NULL)
        return;

    for(p += 16; *p == ' ';)
        p++;
    for(q = p; *p >= '0' && *p <= '9';)
        p++;
    if(*p == '.' && (p[1] == '\0' || p[1] == '\n'))
    {
        *p = '\0';
        ibp->vernum = atoi(q);
        *p = '.';
    }
}

/**********************************************************
 *
 *   bool SprotIndex(pp, (*fun)()):
 *
 *                                              3-26-93
 *
 **********************************************************/
bool SprotIndex(ParserPtr pp, void (*fun)(IndexblkPtr entry, char* offset, Int4 len))
{
    TokenStatBlkPtr stoken;
    FinfoBlkPtr     finfo;

    bool            after_AC;
    bool            after_OS;
    bool            after_OC;
    bool            after_RN;
    bool            after_SQ;
    bool            end_of_file;

    IndexblkPtr     entry;
    DataBlkPtr      data;
    Int4            i;
    Int4            indx = 0;
    IndBlkNextPtr   ibnp;
    IndBlkNextPtr   tibnp;
    char*         p;

    bool            reviewed;

    finfo = (FinfoBlkPtr) MemNew(sizeof(FinfoBlk));

    if(pp->ifp == NULL)
        end_of_file = SkipTitleBuf(pp->ffbuf, finfo, spkwl[ParFlatSP_ID].str,
                                   spkwl[ParFlatSP_ID].len);
    else
        end_of_file = SkipTitle(pp->ifp, finfo, spkwl[ParFlatSP_ID].str,
                                spkwl[ParFlatSP_ID].len);
    if(end_of_file)
    {
        MsgSkipTitleFail("Swiss-Prot", finfo);
        return false;
    }

    ibnp = (IndBlkNextPtr) MemNew(sizeof(IndBlkNext));
    ibnp->next = NULL;
    tibnp = ibnp;

    while (!end_of_file)
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

            after_AC = false;
            after_OS = false;
            after_OC = false;
            after_RN = false;
            after_SQ = false;

            p = PointToNextToken(finfo->str + ParFlat_COL_DATA_SP);
            reviewed = (StringNICmp(p, "reviewed", 8) == 0);

            while(!end_of_file &&
                  StringNCmp(finfo->str, spkwl[ParFlatSP_END].str,
                             spkwl[ParFlatSP_END].len) != 0)
            {
                if(StringNCmp(finfo->str, "RM", 2) == 0)
                {
                    ErrPostEx(SEV_ERROR, ERR_ENTRY_InvalidLineType,
                              "RM line type has been replaced by RX, skipped %s",
                              finfo->str);
                }
                if(after_SQ && IS_ALPHA(finfo->str[0]) != 0)
                {
                    ErrPostStr(SEV_ERROR, ERR_FORMAT_MissingEnd,
                               "Missing end of the entry, entry dropped");
                    entry->drop = 1;
                    break;
                }
                if(StringNCmp(finfo->str, spkwl[ParFlatSP_SQ].str,
                              spkwl[ParFlatSP_SQ].len) == 0)
                    after_SQ = true;

                if(StringNCmp(finfo->str, spkwl[ParFlatSP_OS].str,
                              spkwl[ParFlatSP_OS].len) == 0)
                    after_OS = true;

                if(StringNCmp(finfo->str, "OC", 2) == 0)
                    after_OC = true;

                if(StringNCmp(finfo->str, spkwl[ParFlatSP_RN].str,
                              spkwl[ParFlatSP_RN].len) == 0)
                    after_RN = true;

                if(StringNCmp(finfo->str, spkwl[ParFlatSP_AC].str,
                              spkwl[ParFlatSP_AC].len) == 0)
                {
                    if(after_AC == false)
                    {
                        after_AC = true;
                        if(!GetAccession(pp, finfo->str, entry, 2))
                            pp->num_drop++;
                    }
                    else if(entry->drop == 0 && !GetAccession(pp, finfo->str, entry, 1))
                        pp->num_drop++;
                }
                else if(StringNCmp(finfo->str, spkwl[ParFlatSP_DT].str,
                                   spkwl[ParFlatSP_DT].len) == 0)
                {
                    if(reviewed && pp->sp_dt_seq_ver && entry->vernum < 1)
                        SPGetVerNum(finfo->str, entry);
                    stoken = TokenString(finfo->str, ' ');
                    if(stoken->num > 2)
                    {
                        entry->date = GetUpdateDate(stoken->list->next->str,
                                                    pp->source);
                    }
                    FreeTokenstatblk(stoken);
                }

                if(pp->ifp == NULL)
                    end_of_file = XReadFileBuf(pp->ffbuf, finfo);
                else
                    end_of_file = XReadFile(pp->ifp, finfo);

            } /* while, end of one entry */

            if(entry->drop != 1)
            {
                if(after_AC == false)
                {
                    ErrPostStr(SEV_ERROR, ERR_ACCESSION_NoAccessNum,
                               "Missing AC (accession #) line, entry dropped");
                    entry->drop = 1;
                }

                if(after_OS == false)
                    entry->drop = sp_err_field("OS (organism)");

                if(after_OC == false)
                    entry->drop = sp_err_field("OC (organism classification)");

                if(after_RN == false)
                    entry->drop = sp_err_field("RN (reference data)");

                if(after_SQ == false)
                    entry->drop = sp_err_field("SQ (sequence data)");
            }

            if(pp->ifp == NULL)
                entry->len = (size_t) (pp->ffbuf->current - pp->ffbuf->start) -
                             entry->offset;
            else
                entry->len = (size_t) ftell(pp->ifp) - entry->offset;

            if(fun != NULL)
            {
                data = LoadEntry(pp, entry->offset, entry->len);
                (*fun)(entry, data->offset, static_cast<Int4>(data->len));
                FreeEntry(data);
            }
        } /* if, entry */
        else
        {
            if(pp->ifp == NULL)
                end_of_file = FindNextEntryBuf(end_of_file, pp->ffbuf, finfo,
                                               spkwl[ParFlatSP_END].str,
                                               spkwl[ParFlatSP_END].len);
            else
                end_of_file = FindNextEntry(end_of_file, pp->ifp, finfo,
                                            spkwl[ParFlatSP_END].str,
                                            spkwl[ParFlatSP_END].len);
        }
        if(pp->ifp == NULL)
            end_of_file = FindNextEntryBuf(end_of_file, pp->ffbuf, finfo,
                                           spkwl[ParFlatSP_ID].str,
                                           spkwl[ParFlatSP_ID].len);
        else
            end_of_file = FindNextEntry(end_of_file, pp->ifp, finfo,
                                        spkwl[ParFlatSP_ID].str,
                                        spkwl[ParFlatSP_ID].len);

    } /* while, end_of_file */

    pp->indx = indx;

    pp->entrylist = (IndexblkPtr*) MemNew(indx* sizeof(IndexblkPtr));
    tibnp = ibnp->next;
    MemFree(ibnp);
    for(i = 0; i < indx && tibnp != NULL; i++, tibnp = ibnp)
    {
        pp->entrylist[i] = tibnp->ibp;
        ibnp = tibnp->next;
        MemFree(tibnp);
    }
    MemFree(finfo);

    return end_of_file;
}

END_NCBI_SCOPE
