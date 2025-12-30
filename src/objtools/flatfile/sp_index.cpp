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
 * File Name: sp_index.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *      Build SWISS-PROT format index block. Parsing SP to memory blocks.
 *
 */

#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include "index.h"
#include "sprot.h"
#include "ftaerr.hpp"
#include "indx_blk.h"
#include "indx_def.h"
#include "utilfun.h"
#include "entry.h"

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "sp_index.cpp"

BEGIN_NCBI_SCOPE

vector<string> swissProtKeywords = {
    "ID",
    "AC",
    "DT",
    "DE",
    "GN",
    "OS",
    "RN",
    "CC",
    "PE",
    "DR",
    "KW",
    "FT",
    "SQ",
    "//",
};

/**********************************************************/
static bool sp_err_field(const char* name)
{
    FtaErrPost(SEV_ERROR, ERR_FORMAT_MissingField, "Missing {} line, entry dropped", name);
    return true;
}

/**********************************************************/
static void SPGetVerNum(string_view str, IndexblkPtr ibp)
{
    const char* p;
    const char* q;

    if (str.empty() || ! ibp)
        return;

    p = StringIStr(str.data(), "sequence version");
    if (! p)
        return;
    p += 16;

    const char* end = str.data() + str.size();
    while (p < end && *p == ' ')
        p++;
    q = p;
    while (p < end && *p >= '0' && *p <= '9')
        p++;
    if (p < end && *p == '.') {
        if (p + 1 >= end || *(p + 1) == '\n') {
            ibp->vernum = fta_atoi(string_view(q, p));
        }
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
    bool after_AC;
    bool after_OS;
    bool after_OC;
    bool after_RN;
    bool after_SQ;
    bool end_of_file;

    IndexblkPtr   entry;
    Int4          indx = 0;

    bool reviewed;

    FinfoBlk finfo;

    end_of_file = SkipTitleBuf(pp->ffbuf, finfo, swissProtKeywords[ParFlatSP_ID]);
    if (end_of_file) {
        MsgSkipTitleFail("Swiss-Prot", finfo);
        return false;
    }

    TIndBlkList ibl;
    auto tibnp = ibl.before_begin();

    while (! end_of_file) {
        entry = InitialEntry(pp, finfo);
        if (entry) {
            pp->curindx = indx;
            tibnp       = ibl.emplace_after(tibnp, entry);

            indx++;

            after_AC = false;
            after_OS = false;
            after_OC = false;
            after_RN = false;
            after_SQ = false;

            char* p = finfo.str + ParFlat_COL_DATA_SP;
            PointToNextToken(p);
            reviewed = NStr::StartsWith(p, "reviewed"sv, NStr::eNocase);

            while (! end_of_file &&
                   ! fta_StartsWith(finfo.str, swissProtKeywords[ParFlatSP_END])) {
                const string_view line = finfo.str;
                if (line.starts_with("RM"sv)) {
                    FtaErrPost(SEV_ERROR, ERR_ENTRY_InvalidLineType, "RM line type has been replaced by RX, skipped {}", line);
                }
                if (after_SQ && isalpha(line.front())) {
                    FtaErrPost(SEV_ERROR, ERR_FORMAT_MissingEnd, "Missing end of the entry, entry dropped");
                    entry->drop = true;
                    break;
                }
                if (line.starts_with(swissProtKeywords[ParFlatSP_SQ]))
                    after_SQ = true;

                if (line.starts_with(swissProtKeywords[ParFlatSP_OS]))
                    after_OS = true;

                if (line.starts_with("OC"sv))
                    after_OC = true;

                if (line.starts_with(swissProtKeywords[ParFlatSP_RN]))
                    after_RN = true;

                if (line.starts_with(swissProtKeywords[ParFlatSP_AC])) {
                    if (after_AC == false) {
                        after_AC = true;
                        if (! GetAccession(pp, line, entry, 2))
                            pp->num_drop++;
                    } else if (! entry->drop && ! GetAccession(pp, line, entry, 1))
                        pp->num_drop++;
                } else if (line.starts_with(swissProtKeywords[ParFlatSP_DT])) {
                    if (reviewed && pp->sp_dt_seq_ver && entry->vernum < 1)
                        SPGetVerNum(line, entry);
                    auto tokens = TokenString(line, ' ');
                    if (tokens.num > 2) {
                        entry->date = GetUpdateDate(*next(tokens.list.begin()),
                                                    pp->source);
                    }
                }

                end_of_file = XReadFileBuf(pp->ffbuf, finfo);

            } /* while, end of one entry */

            if (! entry->drop) {
                if (after_AC == false) {
                    FtaErrPost(SEV_ERROR, ERR_ACCESSION_NoAccessNum, "Missing AC (accession #) line, entry dropped");
                    entry->drop = true;
                }

                if (after_OS == false)
                    entry->drop = sp_err_field("OS (organism)");

                if (after_OC == false)
                    entry->drop = sp_err_field("OC (organism classification)");

                if (after_RN == false)
                    entry->drop = sp_err_field("RN (reference data)");

                if (after_SQ == false)
                    entry->drop = sp_err_field("SQ (sequence data)");
            }

            entry->len = pp->ffbuf.get_offs() - entry->offset;

            if (fun) {
                auto data(LoadEntry(pp, entry));
                (*fun)(entry, data->mBuf.ptr, static_cast<Int4>(data->mBuf.len));
            }
        } /* if, entry */
        else {
            end_of_file = FindNextEntryBuf(
                end_of_file, pp->ffbuf, finfo, swissProtKeywords[ParFlatSP_END]);
        }
        end_of_file = FindNextEntryBuf(
            end_of_file, pp->ffbuf, finfo, swissProtKeywords[ParFlatSP_ID]);

    } /* while, end_of_file */

    pp->indx = indx;

    pp->entrylist.reserve(indx);
    for (auto& it : ibl) {
        pp->entrylist.push_back(it.release());
    }

    return end_of_file;
}

END_NCBI_SCOPE
