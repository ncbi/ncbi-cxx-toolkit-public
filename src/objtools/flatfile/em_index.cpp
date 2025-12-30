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
 * File Name: em_index.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *      Parsing embl to blocks. Build Embl format index block.
 *
 */

#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include "index.h"
#include "embl.h"

#include "ftaerr.hpp"
#include "indx_blk.h"
#include "indx_def.h"
#include "utilfun.h"
#include "entry.h"
#include "keyword_parse.hpp"

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "em_index.cpp"

BEGIN_NCBI_SCOPE

vector<string> emblKeywords = {
    "ID",
    "AC",
    "NI",
    "DT",
    "DE",
    "KW",
    "OS",
    "RN",
    "DR",
    "CC",
    "FH",
    "SQ",
    "SV",
    "CO",
    "AH",
    "PR",
    "//",
};

vector<string> checkedEmblKeywords = {
    "ID", "AC", "NI", "DT", "DE", "KW", "OS", "OC", "OG", "RN", "RP", "RX", "RC", "RG", "RA", "RT", "RL", "DR", "FH", "FT", "SQ", "CC", "SV", "CO", "XX", "AH", "AS", "PR", "//"
};


/**********************************************************/
static bool em_err_field(const char* str)
{
    FtaErrPost(SEV_ERROR, ERR_FORMAT_MissingField, "No {} in Embl format file, entry dropped", str);
    return true;
}

/**********************************************************/
static void ParseEmblVersion(IndexblkPtr entry, string_view line)
{
    size_t p = line.rfind('.');
    if (p == string_view::npos) {
        FtaErrPost(SEV_FATAL, ERR_VERSION_MissingVerNum, "Missing VERSION number in SV line.");
        entry->drop = true;
        return;
    }
    string acc(line.substr(0, p));
    ++p;
    string ver(line.substr(p));
    for (size_t q = p; q < line.size(); ++q) {
        if (line[q] < '0' || line[q] > '9') {
            FtaErrPost(SEV_FATAL, ERR_VERSION_NonDigitVerNum, "Incorrect VERSION number in SV line: \"{}\".", ver);
            entry->drop = true;
            return;
        }
    }
    if (entry->acnum != acc) {
        FtaErrPost(SEV_FATAL, ERR_VERSION_AccessionsDontMatch, "Accessions in SV and AC lines don't match: \"{}\" vs \"{}\".", acc, entry->acnum);
        entry->drop = true;
        return;
    }
    entry->vernum = fta_atoi(ver.c_str());
    if (entry->vernum < 1) {
        FtaErrPost(SEV_FATAL, ERR_VERSION_InvalidVersion, "Version number \"{}\" from Accession.Version value \"{}.{}\" is not a positive integer.", entry->vernum, entry->acnum, entry->vernum);
        entry->drop = true;
    }
}

/**********************************************************/
static optional<string> EmblGetNewIDVersion(string_view locus, string_view str)
{
    if (locus.empty() || str.empty())
        return {};
    auto p = str.find(';');
    if (p == string_view::npos)
        return {};
    ++p;
    while (p < str.size() && str[p] == ' ')
        p++;
    if (p + 2 >= str.size() || str[p] != 'S' || str[p + 1] != 'V')
        return {};
    p += 2;
    while (p < str.size() && str[p] == ' ')
        p++;
    auto q = str.find(';', p);
    if (q == string_view::npos)
        return {};

    string res(locus);
    res += '.';
    res += str.substr(p, q - p);
    return res;
}

/**********************************************************
 *
 *   bool EmblIndex(pp, (*fun)()):
 *
 *                                              3-25-93
 *
 **********************************************************/
bool EmblIndex(ParserPtr pp, void (*fun)(IndexblkPtr entry, char* offset, Int4 len))
{
    FinfoBlk finfo;

    bool after_AC;
    bool after_NI;
    bool after_ID;
    bool after_OS;
    bool after_OC;
    bool after_RN;
    bool after_SQ;
    bool after_SV;
    bool after_DT;

    bool end_of_file;

    IndexblkPtr entry;
    Int4        indx = 0;

    end_of_file = SkipTitleBuf(pp->ffbuf, finfo, emblKeywords[ParFlat_ID]);
    if (end_of_file) {
        MsgSkipTitleFail("Embl", finfo);
        return false;
    }

    bool tpa_check = (pp->source == Parser::ESource::EMBL);

    TIndBlkList ibl;
    auto tibnp = ibl.before_begin();

    while (! end_of_file) {
        entry = InitialEntry(pp, finfo);

        if (entry) {
            pp->curindx = indx;
            tibnp       = ibl.emplace_after(tibnp, entry);

            indx++;

            entry->is_contig = false;
            entry->origin    = false;
            after_AC         = false;
            after_ID         = false;
            after_OS         = false;
            after_OC         = false;
            after_RN         = false;
            after_SQ         = false;
            after_NI         = false;
            after_SV         = false;
            after_DT         = false;

            optional<string> line_sv;

            const auto keywordEnd = emblKeywords[ParFlatEM_END];
            const auto keywordId  = emblKeywords[ParFlat_ID];
            const auto keywordNi  = emblKeywords[ParFlat_NI];
            const auto keywordAh  = emblKeywords[ParFlat_AH];
            const auto keywordSq  = emblKeywords[ParFlat_SQ];
            const auto keywordOs  = emblKeywords[ParFlat_OS];
            const auto keywordSv  = emblKeywords[ParFlat_SV];
            const auto keywordKw  = emblKeywords[ParFlat_KW];

            while (! end_of_file &&
                   ! fta_StartsWith(finfo.str, keywordEnd)) {
                const string_view line = finfo.str;
                if (line.starts_with(keywordKw)) {
                    if (pp->source == Parser::ESource::EMBL ||
                        pp->source == Parser::ESource::DDBJ) {
                        pp->KeywordParser().AddDataLine(string(line));
                    }
                } else if (line.starts_with(keywordId)) {
                    if (after_ID) {
                        FtaErrPost(SEV_ERROR, ERR_FORMAT_MissingEnd, "Missing end of the entry, entry dropped");
                        entry->drop = true;
                        break;
                    }
                    after_ID = true;
                    if (entry->embl_new_ID)
                        line_sv = EmblGetNewIDVersion(entry->locusname, line);
                } else if (line.starts_with(keywordAh)) {
                    if (entry->is_tpa == false && entry->tsa_allowed == false) {
                        FtaErrPost(SEV_ERROR, ERR_ENTRY_InvalidLineType, "Line type \"AH\" is allowed for TPA or TSA records only. Continue anyway.");
                    }
                }
                if (after_SQ && isalpha(line.front())) {
                    FtaErrPost(SEV_ERROR, ERR_FORMAT_MissingEnd, "Missing end of the entry, entry dropped");
                    entry->drop = true;
                    break;
                }
                if (line.starts_with(keywordNi)) {
                    if (after_NI) {
                        FtaErrPost(SEV_ERROR, ERR_FORMAT_Multiple_NI, "Multiple NI lines in the entry, entry dropped");
                        entry->drop = true;
                        break;
                    }
                    after_NI = true;
                } else if (line.starts_with(keywordSq)) {
                    after_SQ      = true;
                    entry->origin = true;
                } else if (line.starts_with(keywordOs)) {
                    if (after_OS && pp->source != Parser::ESource::EMBL) {
                        FtaErrPost(SEV_INFO, ERR_ORGANISM_Multiple, "Multiple OS lines in the entry");
                    }
                    after_OS = true;
                }
                if (pp->accver &&
                    line.starts_with(keywordSv)) {
                    if (entry->embl_new_ID) {
                        FtaErrPost(SEV_ERROR, ERR_ENTRY_InvalidLineType, "Line type \"SV\" is not allowed in conjunction with the new format of \"ID\" line. Entry dropped.");
                        entry->drop = true;
                    } else {
                        if (after_SV) {
                            FtaErrPost(SEV_FATAL, ERR_FORMAT_Multiple_SV, "Multiple SV lines in the entry");
                            entry->drop = true;
                            break;
                        }
                        after_SV = true;
                        const char* p = line.data() + ParFlat_COL_DATA_EMBL;
                        while (*p == ' ' || *p == '\t')
                            p++;
                        const char* q = p;
                        for (; *q != '\0' && *q != ' ' && *q != '\t' &&
                                    *q != '\n';)
                            q++;
                        line_sv = string(p, q);
                    }
                }
                if (line.starts_with("OC"sv))
                    after_OC = true;

                const auto keywordRn = emblKeywords[ParFlat_RN];
                if (line.starts_with(keywordRn))
                    after_RN = true;

                const auto keywordCo = emblKeywords[ParFlat_CO];
                if (line.starts_with(keywordCo))
                    entry->is_contig = true;

                const auto keywordAc = emblKeywords[ParFlat_AC];
                const auto keywordDt = emblKeywords[ParFlat_DT];
                if (line.starts_with(keywordAc)) {
                    if (after_AC == false) {
                        after_AC = true;
                        if (! GetAccession(pp, line, entry, 2))
                            pp->num_drop++;
                    } else if (! entry->drop &&
                               ! GetAccession(pp, line, entry, 1))
                        pp->num_drop++;
                } else if (line.starts_with(keywordDt)) {
                    auto tokens = TokenString(line, ' ');
                    if (tokens.num > 2) {
                        after_DT    = true;
                        entry->date = GetUpdateDate(*next(tokens.list.begin()),
                                                    pp->source);
                    }
                }

                end_of_file = XReadFileBuf(pp->ffbuf, finfo);

                if (finfo.str[0] != ' ' && finfo.str[0] != '\t') {
                    if (CheckLineType(finfo.str, finfo.line, checkedEmblKeywords, false) == false)
                        entry->drop = true;
                }
            } /* while, end of one entry */

            xCheckEstStsGssTpaKeywords(
                pp->KeywordParser().KeywordList(),
                tpa_check,
                entry);

            entry->is_tpa_wgs_con = (entry->is_contig && entry->is_wgs && entry->is_tpa);

            if (! entry->drop) {
                if (after_AC == false) {
                    FtaErrPost(SEV_ERROR, ERR_ACCESSION_NoAccessNum, "No AC in Embl format file, entry dropped");
                    entry->drop = true;
                }

                if (after_ID == false)
                    entry->drop = em_err_field("ID");

                if (after_SV == false && pp->accver &&
                    entry->embl_new_ID == false)
                    entry->drop = em_err_field("Version number (SV)");

                if (after_OS == false)
                    entry->drop = em_err_field("Organism data (OS)");

                if (after_OC == false)
                    entry->drop = em_err_field("Organism data (OC)");

                if (after_RN == false)
                    entry->drop = em_err_field("Reference data");

                if (after_DT == false)
                    entry->drop = em_err_field("Update and Create dates");

                if (after_SQ == false && entry->is_contig == false)
                    entry->drop = em_err_field("Sequence data");
            }
            if (! entry->drop && pp->accver) {
                ParseEmblVersion(entry, line_sv ? *line_sv : string_view());
            }
            line_sv.reset();

            entry->len = pp->ffbuf.get_offs() - entry->offset;

            if (fun) {
                auto data(LoadEntry(pp, entry));
                (*fun)(entry, data->mBuf.ptr, static_cast<Int4>(data->mBuf.len));
            }
        } /* if, entry */
        else {
            end_of_file = FindNextEntryBuf(
                end_of_file, pp->ffbuf, finfo, emblKeywords[ParFlatEM_END]);
        }

        end_of_file = FindNextEntryBuf(
            end_of_file, pp->ffbuf, finfo, emblKeywords[ParFlat_ID]);

    } /* while, end_of_file */

    pp->indx = indx;

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

    if (pp->qsfd && ! QSIndex(pp, ibl, indx))
        return false;

    pp->entrylist.reserve(indx);
    for (auto& it : ibl) {
        pp->entrylist.push_back(it.release());
    }

    return (end_of_file);
}

END_NCBI_SCOPE
