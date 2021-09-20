/* em_index.c
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
 * File Name:  em_index.c
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
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

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "em_index.cpp"

BEGIN_NCBI_SCOPE

vector<string> emblKeywords = {
    "ID", "AC", "NI", "DT", "DE", "KW", "OS", "RN", "DR", "CC", "FH", "SQ", "SV",
    "CO", "AH", "PR", "//",
};

vector<string> checkedEmblKeywords = {
    "ID", "AC", "NI", "DT", "DE", "KW", 
    "OS", "OC", "OG", "RN", "RP", "RX", 
    "RC", "RG", "RA", "RT", "RL", "DR", 
    "FH", "FT", "SQ", "CC", "SV", "CO", 
    "XX", "AH", "AS", "PR", "//"
};


// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
/**********************************************************
 *
 *   static void EmblSegment(pp):
 *
 *                                              2-24-93
 *
 **********************************************************/
static void EmblSegment(ParserPtr pp)
{
    size_t      i = 0;
    int         j;
    IndexblkPtr ibp;
    char*     locus;

    locus = StringSave(pp->entrylist[0]->locusname);

    for(i = StringLen(locus); isdigit(locus[i-1]) != 0 && i > 0; i--)
        locus[i-1] = '\0';

    for(j = 0; j < pp->indx; j++)
    {
        ibp = pp->entrylist[j];
        ibp->segnum = static_cast<Uint2>(j + 1);
        ibp->segtotal = pp->indx;

        StringCpy(ibp->blocusname, locus);
    }

    MemFree(locus);
}
// LCOV_EXCL_STOP

/**********************************************************/
static Uint1 em_err_field(char* str)
{
    ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingField,
              "No %s in Embl format file, entry dropped", str);
    return(1);
}

/**********************************************************/
static void ParseEmblVersion(IndexblkPtr entry, char* line)
{
    char* p;
    char* q;

    p = StringRChr(line, '.');
    if(p == NULL)
    {
        ErrPostEx(SEV_FATAL, ERR_VERSION_MissingVerNum,
                  "Missing VERSION number in SV line.");
        entry->drop = 1;
        return;
    }
    *p++ = '\0';
    for(q = p; *q >= '0' && *q <= '9';)
        q++;
    if(*q != '\0')
    {
        ErrPostEx(SEV_FATAL, ERR_VERSION_NonDigitVerNum,
                  "Incorrect VERSION number in SV line: \"%s\".", p);
        entry->drop = 1;
        return;
    }
    if(entry->acnum == NULL || StringCmp(entry->acnum, line) != 0)
    {
        ErrPostEx(SEV_FATAL, ERR_VERSION_AccessionsDontMatch,
                  "Accessions in SV and AC lines don't match: \"%s\" vs \"%s\".",
                  line, (entry->acnum == NULL) ? "NULL" : entry->acnum);
        entry->drop = 1;
        return;
    }
    entry->vernum = atoi(p);
    if(entry->vernum < 1)
    {
        ErrPostEx(SEV_FATAL, ERR_VERSION_InvalidVersion,
                  "Version number \"%d\" from Accession.Version value \"%s.%d\" is not a positive integer.",
                  entry->vernum, entry->acnum, entry->vernum);
        entry->drop = 1;
    }
}

/**********************************************************/
static char* EmblGetNewIDVersion(char* locus, char* str)
{
    char* res;
    char* p;
    char* q;

    if(locus == NULL || str == NULL)
        return(NULL);
    p = StringChr(str, ';');
    if(p == NULL)
        return(NULL);
    for(p++; *p == ' ';)
        p++;
    if(p[0] != 'S' || p[1] != 'V')
        return(NULL);
    for(p += 2; *p == ' ';)
        p++;
    q = StringChr(p, ';');
    if(q == NULL)
        return(NULL);
    *q = '\0';

    res = (char*) MemNew(StringLen(locus) + StringLen(p) + 2);
    StringCpy(res, locus);
    StringCat(res, ".");
    StringCat(res, p);

    *q = ';';
    return(res);
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
    TokenStatBlkPtr stoken;
    FinfoBlkPtr     finfo;

    bool            after_AC;
    bool            after_NI;
    bool            after_ID;
    bool            after_OS;
    bool            after_OC;
    bool            after_RN;
    bool            after_SQ;
    bool            after_SV;
    bool            after_DT;

    bool            end_of_file;

    IndexblkPtr     entry;
    DataBlkPtr      data;
    Int4            indx = 0;
    IndBlkNextPtr   ibnp;
    IndBlkNextPtr   tibnp;
    size_t          i;
    int             j;
    char*         line_sv;
    char*         p;
    char*         q;
    ValNodePtr      kwds;
    ValNodePtr      tkwds;

    finfo = new FinfoBlk();


    end_of_file = SkipTitleBuf(pp->ffbuf, finfo, emblKeywords[ParFlat_ID]);
    if(end_of_file)
    {
        MsgSkipTitleFail((char*) "Embl", finfo);
        return false;
    }

    bool tpa_check = (pp->source == Parser::ESource::EMBL);

    ibnp = (IndBlkNextPtr) MemNew(sizeof(IndBlkNext));
    ibnp->next = NULL;
    tibnp = ibnp;

    kwds = NULL;
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

            entry->is_contig = false;
            entry->origin = false;
            after_AC = false;
            after_ID = false;
            after_OS = false;
            after_OC = false;
            after_RN = false;
            after_SQ = false;
            after_NI = false;
            after_SV = false;
            after_DT = false;

            line_sv = NULL;
            if(kwds != NULL)
                kwds = ValNodeFreeData(kwds);
            tkwds = NULL;
            size_t kwds_len = 0;
            auto keywordEnd = emblKeywords[ParFlatEM_END];
            auto keywordId = emblKeywords[ParFlat_ID];
            auto keywordNi = emblKeywords[ParFlat_NI];
            auto keywordAh = emblKeywords[ParFlat_AH];
            auto keywordSq = emblKeywords[ParFlat_SQ];
            auto keywordOs = emblKeywords[ParFlat_OS];
            auto keywordSv = emblKeywords[ParFlat_SV];

            while (!end_of_file &&
                  StringNCmp(finfo->str, keywordEnd.c_str(), keywordEnd.size()) != 0)
            {
                if(StringNCmp(finfo->str, keywordEnd.c_str(), 2) == 0)
                {
                    if(pp->source == Parser::ESource::EMBL ||
                       pp->source == Parser::ESource::DDBJ)
                    {
                        if(kwds == NULL)
                        {
                            kwds = ValNodeNew(NULL);
                            tkwds = kwds;
                        }
                        else
                        {
                            tkwds->next = ValNodeNew(NULL);
                            tkwds = tkwds->next;
                        }
                        tkwds->data.ptrvalue = StringSave(finfo->str + 2);
                        kwds_len += StringLen(finfo->str) - 2;
                    }
                }
                else if(StringNCmp(finfo->str, keywordId.c_str(), keywordId.size()) == 0)
                {
                    if(after_ID)
                    {
                        ErrPostStr(SEV_ERROR, ERR_FORMAT_MissingEnd,
                                   "Missing end of the entry, entry dropped");
                        entry->drop = 1;
                        break;
                    }
                    after_ID = true;
                    if(entry->embl_new_ID)
                        line_sv = EmblGetNewIDVersion(entry->locusname,
                                                      finfo->str);
                }
                else if(StringNCmp(finfo->str, keywordAh.c_str(), keywordAh.size()) == 0)
                {
                    if(entry->is_tpa == false && entry->tsa_allowed == false)
                    {
                        ErrPostEx(SEV_ERROR, ERR_ENTRY_InvalidLineType,
                                  "Line type \"AH\" is allowed for TPA or TSA records only. Continue anyway.");
                    }
                }
                if(after_SQ && isalpha(finfo->str[0]) != 0)
                {
                    ErrPostStr(SEV_ERROR, ERR_FORMAT_MissingEnd,
                               "Missing end of the entry, entry dropped");
                    entry->drop = 1;
                    break;
                }
                if(StringNCmp(finfo->str, keywordNi.c_str(), 2) == 0)
                {
                    if(after_NI)
                    {
                        ErrPostStr(SEV_ERROR, ERR_FORMAT_Multiple_NI,
                                   "Multiple NI lines in the entry, entry dropped");
                        entry->drop = 1;
                        break;
                    }
                    after_NI = true;
                }
                else if(StringNCmp(finfo->str, keywordSq.c_str(), keywordSq.size()) == 0)
                {
                    after_SQ = true;
                    entry->origin = true;
                }
                else if(StringNCmp(finfo->str, keywordOs.c_str(), keywordOs.size()) == 0)
                {
                    if(after_OS && pp->source != Parser::ESource::EMBL)
                    {
                        ErrPostStr(SEV_INFO, ERR_ORGANISM_Multiple,
                                   "Multiple OS lines in the entry");
                    }
                    after_OS = true;
                }
                if(pp->accver &&
                   StringNCmp(finfo->str, keywordSv.c_str(), keywordSv.size()) == 0)
                {
                    if(entry->embl_new_ID)
                    {
                        ErrPostEx(SEV_ERROR, ERR_ENTRY_InvalidLineType,
                                  "Line type \"SV\" is not allowed in conjunction with the new format of \"ID\" line. Entry dropped.");
                        entry->drop = 1;
                    }
                    else
                    {
                        if(after_SV)
                        {
                            ErrPostStr(SEV_FATAL, ERR_FORMAT_Multiple_SV,
                                       "Multiple SV lines in the entry");
                            entry->drop = 1;
                            break;
                        }
                        after_SV = true;
                        p = finfo->str + ParFlat_COL_DATA_EMBL;
                        while(*p == ' ' || *p == '\t')
                            p++;
                        for(q = p; *q != '\0' && *q != ' ' && *q != '\t' &&
                                   *q != '\n';)
                            q++;
                        i = q - p;
                        line_sv = (char*) MemNew(i + 1);
                        StringNCpy(line_sv, p, i);
                        line_sv[i] = '\0';
                    }
                }
                if(StringNCmp(finfo->str, "OC", 2) == 0)
                    after_OC = true;

                auto keywordRn = emblKeywords[ParFlat_RN];
                if(StringNCmp(finfo->str, keywordRn.c_str(), keywordRn.size()) == 0)
                    after_RN = true;

                auto keywordCo = emblKeywords[ParFlat_CO];
                if(StringNCmp(finfo->str, keywordCo.c_str(),
                    keywordCo.size()) == 0)
                    entry->is_contig = true;

                auto keywordAc = emblKeywords[ParFlat_AC];
                auto keywordDt = emblKeywords[ParFlat_DT];
                if(StringNCmp(finfo->str, keywordAc.c_str(), keywordAc.size()) == 0)
                {
                    if(after_AC == false)
                    {
                        after_AC = true;
                        if(GetAccession(pp, finfo->str, entry, 2) == false)
                            pp->num_drop++;
                    }
                    else if(entry->drop == 0 &&
                            GetAccession(pp, finfo->str, entry, 1) == false)
                        pp->num_drop++;
                }
                else if(StringNCmp(finfo->str, keywordDt.c_str(), keywordDt.size()) == 0)
                {
                    stoken = TokenString(finfo->str, ' ');
                    if(stoken->num > 2)
                    {
                        after_DT = true;
                        entry->date = GetUpdateDate(stoken->list->next->str,
                                                    pp->source);
                    }

                    FreeTokenstatblk(stoken);
                }

                end_of_file = XReadFileBuf(pp->ffbuf, finfo);

                if(finfo->str[0] != ' ' && finfo->str[0] != '\t')
                {
                    if(CheckLineType(finfo->str, finfo->line,
                                     checkedEmblKeywords, false) == false)
                        entry->drop = 1;
                }
            } /* while, end of one entry */

            if(kwds != NULL)
            {
                check_est_sts_gss_tpa_kwds(kwds, kwds_len, entry, tpa_check,
                                           entry->specialist_db,
                                           entry->inferential,
                                           entry->experimental,
                                           entry->assembly);
                kwds = ValNodeFreeData(kwds);
                kwds_len = 0;
            }

            entry->is_tpa_wgs_con = (entry->is_contig && entry->is_wgs && entry->is_tpa);

            if(entry->drop != 1)
            {
                if(after_AC == false)
                {
                    ErrPostStr(SEV_ERROR, ERR_ACCESSION_NoAccessNum,
                               "No AC in Embl format file, entry dropped");
                    entry->drop = 1;
                }

                if(after_ID == false)
                    entry->drop = em_err_field((char *) "ID");

                if(after_SV == false && pp->accver &&
                   entry->embl_new_ID == false)
                    entry->drop = em_err_field((char *) "Version number (SV)");

                if(after_OS == false)
                    entry->drop = em_err_field((char *) "Organism data (OS)");

                if(after_OC == false)
                    entry->drop = em_err_field((char *) "Organism data (OC)");

                if(after_RN == false)
                    entry->drop = em_err_field((char *) "Reference data");

                if(after_DT == false)
                    entry->drop = em_err_field((char *) "Update and Create dates");

                if(after_SQ == false && entry->is_contig == false)
                    entry->drop = em_err_field((char *) "Sequence data");
            }
            if(entry->drop != 1 && pp->accver)
            {
                ParseEmblVersion(entry, line_sv);
            }
            if(line_sv != NULL)
            {
                MemFree(line_sv);
                line_sv = NULL;
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
                                           emblKeywords[ParFlatEM_END].c_str(),
                                           emblKeywords[ParFlatEM_END].size());
        }

        end_of_file = FindNextEntryBuf(end_of_file, pp->ffbuf, finfo,
            emblKeywords[ParFlat_ID].c_str(),
            emblKeywords[ParFlat_ID].size());

    } /* while, end_of_file */

    pp->indx = indx;

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

    if(pp->qsfd != NULL && QSIndex(pp, ibnp->next) == false)
        return false;

    pp->entrylist = (IndexblkPtr*) MemNew(indx * sizeof(IndexblkPtr));
    tibnp = ibnp->next;
    MemFree(ibnp);
    for(j = 0; j < indx && tibnp != NULL; j++, tibnp = ibnp)
    {
        pp->entrylist[j] = tibnp->ibp;
        ibnp = tibnp->next;
        MemFree(tibnp);
    }

    delete finfo;

    if(pp->segment)
// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
        EmblSegment(pp);
// LCOV_EXCL_STOP

    return(end_of_file);
}

END_NCBI_SCOPE
