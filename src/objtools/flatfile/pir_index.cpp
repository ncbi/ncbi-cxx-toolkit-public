/* pir_index.c
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
 * File Name:  pir_index.c
 *
 * Author:  Sergey Bazhin
 *
 * File Description:
 * -----------------
 *      Build PIR format index block.
 *
 */

// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
#include <ncbi_pch.hpp>
#include "ftacpp.hpp"

#include <objtools/flatfile/index.h>
#include <objtools/flatfile/pir_index.h>

#include "ftaerr.hpp"
#include "indx_blk.h"
#include "indx_def.h"
#include "utilfun.h"
#include "entry.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "pir_index.cpp"

BEGIN_NCBI_SCOPE


KwordBlk pirkwl[] = {
    {"ENTRY", 5},           {"TITLE", 5},           {"ALTERNATE_NAMES", 15},
    {"CONTAINS", 8},        {"ORGANISM", 8},        {"DATE", 4},
    {"ACCESSIONS", 10},     {"REFERENCE", 9},       {"COMMENT", 7},
    {"COMPLEX", 7},         {"GENETICS", 8},        {"FUNCTION", 8},
    {"CLASSIFICATION", 14}, {"KEYWORDS", 8},        {"FEATURE", 7},
    {"SUMMARY", 7},         {"SEQUENCE", 8},        {"///", 3},
    {NULL, 0}
};

/**********************************************************/
static Uint1 pir_err_field(const char *name)
{
    ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingField,
              "Missing %s line, entry dropped", name);
    return(1);
}

/**********************************************************/
static CRef<objects::CDate_std> GetDatePtr(char* line)
{
    const char *mon[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL",
        "AUG", "SEP", "OCT", "NOV", "DEC", NULL };
    const char **b;
    char*    p;
    Int2       day;
    Int2       month;
    Int2       year;

    p = line;
    if (p == NULL || StringLen(p) != 11 || p[2] != '-' || p[6] != '-' ||
        p[0] < '0' || p[0] > '3' || p[1] < '0' || p[1] > '9' ||
        p[7] < '1' || p[7] > '2' || p[8] < '0' || p[8] > '9' ||
        p[9] < '0' || p[9] > '9' || p[10] < '0' || p[10] > '9')
        return CRef<objects::CDate_std>();

    line[6] = '\0';

    for (month = 1, b = mon; *b != NULL; b++, month++)
        if (StringICmp(*b, &p[3]) == 0)
            break;

    line[6] = '-';
    if (*b == NULL)
        return CRef<objects::CDate_std>();

    year = atoi(&p[7]);

    line[2] = '\0';
    if (*p == '0')
        p++;
    day = atoi(p);
    line[2] = '-';

    if (day < 1 || day > 31)
        return CRef<objects::CDate_std>();

    CRef<objects::CDate_std> res(new objects::CDate_std);
    res->SetYear(year);
    res->SetMonth(month);
    res->SetDay(day);

    return res;
}

/**********************************************************/
static CRef<objects::CDate_std> GetPirDate(char* line)
{
    CRef<objects::CDate_std> first;
    CRef<objects::CDate_std> second;
    char* p;
    char* q;
    Char    ch;

    for (p = line; *p != '\0'; p++)
        if (*p == '\t' || *p == '\n')
            *p = ' ';

    q = StringStr(line, "#sequence_revision");
    if (q != NULL)
    {
        for (p = q + 18; *p == ' ';)
            p++;
        for (q = p; *p != '\0' && *p != ' ';)
            p++;
        ch = *p;
        *p = '\0';
        first = GetDatePtr(q);
        *p = ch;
    }

    q = StringStr(line, "#text_change");
    if (q != NULL)
    {
        for (p = q + 12; *p == ' ';)
            p++;
        for (q = p; *p != '\0' && *p != ' ';)
            p++;
        ch = *p;
        *p = '\0';
        second = GetDatePtr(q);
        *p = ch;
    }

    if (first.Empty())
        return(second);
    if (second.Empty())
        return(first);

    if (second->Compare(*first) == objects::CDate::eCompare_before)
    {
        return(second);
    }

    return(first);
}

/**********************************************************
 *
 *   bool PirIndex(pp, (*fun)()):
 *
 *                                              12-JAN-2001
 *
 **********************************************************/
bool PirIndex(ParserPtr pp, void (*fun)(IndexblkPtr entry, char* offset, Int4 len))
{
    FinfoBlkPtr   finfo;
    bool          after_ORGANISM;
    bool          after_REFERENCE;
    bool          after_SEQUENCE;
    bool          end_of_file;
    IndexblkPtr   entry;
    DataBlkPtr    data;
    Int4          i;
    Int4          indx = 0;
    IndBlkNextPtr ibnp;
    IndBlkNextPtr tibnp;
    char*       p;
    char*       q;

    finfo = (FinfoBlkPtr) MemNew(sizeof(FinfoBlk));

    if(pp->ifp == NULL)
        end_of_file = SkipTitleBuf(pp->ffbuf, finfo,
                                   pirkwl[ParFlatPIR_ENTRY].str,
                                   pirkwl[ParFlatPIR_ENTRY].len);
    else
        end_of_file = SkipTitle(pp->ifp, finfo, pirkwl[ParFlatPIR_ENTRY].str,
                                pirkwl[ParFlatPIR_ENTRY].len);
    if(end_of_file)
    {
        MsgSkipTitleFail("PIR", finfo);
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
            entry->EST = false;
            entry->STS = false;
            entry->vernum = 0;
            entry->segnum = 0;
            entry->segtotal = 0;

            pp->curindx = indx;
            tibnp->next = (IndBlkNextPtr) MemNew(sizeof(IndBlkNext));
            tibnp = tibnp->next;
            tibnp->ibp = entry;
            tibnp->next = NULL;

            indx++;

            after_ORGANISM = false;
            after_REFERENCE = false;
            after_SEQUENCE = false;

            while(!end_of_file &&
                  StringNCmp(finfo->str, pirkwl[ParFlatPIR_END].str,
                             pirkwl[ParFlatPIR_END].len) != 0)
            {
                if(after_SEQUENCE && IS_ALPHA(finfo->str[0]) != 0)
                {
                    ErrPostStr(SEV_ERROR, ERR_FORMAT_MissingEnd,
                               "Missing end of the entry, entry dropped");
                    entry->drop = 1;
                    break;
                }
                if(StringNCmp(finfo->str, pirkwl[ParFlatPIR_SEQUENCE].str,
                              pirkwl[ParFlatPIR_SEQUENCE].len) == 0)
                    after_SEQUENCE = true;

                else if(StringNCmp(finfo->str, pirkwl[ParFlatPIR_ORGANISM].str,
                                   pirkwl[ParFlatPIR_ORGANISM].len) == 0)
                    after_ORGANISM = true;

                else if(StringNCmp(finfo->str,
                                   pirkwl[ParFlatPIR_REFERENCE].str,
                                   pirkwl[ParFlatPIR_REFERENCE].len) == 0)
                    after_REFERENCE = true;

                else if(StringNCmp(finfo->str, pirkwl[ParFlatPIR_DATE].str,
                                   pirkwl[ParFlatPIR_DATE].len) == 0)
                {
                    q = StringSave(finfo->str);
                    for(;;)
                    {
                        if(pp->ifp == NULL)
                            end_of_file = XReadFileBuf(pp->ffbuf, finfo);
                        else
                            end_of_file = XReadFile(pp->ifp, finfo);
                        if(end_of_file)
                            break;
                        if(finfo->str[0] != ' ' && finfo->str[0] != '\t')
                            break;

                        p = (char*) MemNew(StringLen(finfo->str) +
                                             StringLen(q) + 1);
                        StringCpy(p, q);
                        StringCat(p, finfo->str);
                        MemFree(q);
                        q = p;
                    }

                    entry->date = GetPirDate(q);
                    MemFree(q);
                    continue;
                }

                if(pp->ifp == NULL)
                    end_of_file = XReadFileBuf(pp->ffbuf, finfo);
                else
                    end_of_file = XReadFile(pp->ifp, finfo);
            }

            if(entry->drop != 1)
            {
                if(after_ORGANISM == false)
                    entry->drop = pir_err_field("ORGANISM");

                if(after_REFERENCE == false)
                    entry->drop = pir_err_field("REFERENCE");

                if(after_SEQUENCE == false)
                    entry->drop = pir_err_field("SEQUENCE");
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
        }
        else
        {
            if(pp->ifp == NULL)
                end_of_file = FindNextEntryBuf(end_of_file, pp->ffbuf, finfo,
                                               pirkwl[ParFlatPIR_END].str,
                                               pirkwl[ParFlatPIR_END].len);
            else
                end_of_file = FindNextEntry(end_of_file, pp->ifp, finfo,
                                            pirkwl[ParFlatPIR_END].str,
                                            pirkwl[ParFlatPIR_END].len);
        }
        if(pp->ifp == NULL)
            end_of_file = FindNextEntryBuf(end_of_file, pp->ffbuf, finfo,
                                           pirkwl[ParFlatPIR_ENTRY].str,
                                           pirkwl[ParFlatPIR_ENTRY].len);
        else
            end_of_file = FindNextEntry(end_of_file, pp->ifp, finfo,
                                        pirkwl[ParFlatPIR_ENTRY].str,
                                        pirkwl[ParFlatPIR_ENTRY].len);
    }

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
