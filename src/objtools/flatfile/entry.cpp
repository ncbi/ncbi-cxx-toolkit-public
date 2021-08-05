/* entry.c
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
 * File Name:  entry.c
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *
 */
#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include "index.h"

#include "ftaerr.hpp"
#include "indx_err.h"
#include "utilfun.h"
#include "entry.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "entry.cpp"

BEGIN_NCBI_SCOPE

/**********************************************************/
static size_t FileReadBuf(char* to, size_t len, FileBuf& ffbuf)
{
    const char* p = nullptr;
    char* q;
    size_t   i;

    for(p = ffbuf.current, q = to, i = 0; i < len && *p != '\0'; i++)
        *q++ = *p++;

    ffbuf.current = p;
    return(i);
}

/**********************************************************
 *
 *   DataBlkPtr LoadEntry(pp, offset, len):
 *
 *      Put one entry of data to memory.
 *      Return NULL if fseek information which came from the
 *   first step, building index-entries, failed, or FileRead
 *   failed; otherwise, return entry pointer.
 *      Replace any none ASCII character to '#', and warning
 *   message.
 *
 *                                              3-22-93
 *
 **********************************************************/
DataBlkPtr LoadEntry(ParserPtr pp, size_t offset, size_t len)
{
    DataBlkPtr entry;
    char*    eptr;
    char*    q;
    size_t     i;

    if (pp->ifp == NULL)
    {
        pp->ffbuf.current = pp->ffbuf.start + offset;
        i = 0;
    }
    else
        i = (size_t)fseek(pp->ifp, static_cast<long>(offset), SEEK_SET);
    if (i != 0)                          /* hardware problem */
    {
        ErrPostEx(SEV_FATAL, ERR_INPUT_CannotReadEntry,
                  "Failed to fseek() in input file (buffer).");
        return(NULL);
    }

    entry = (DataBlkPtr)MemNew(sizeof(DataBlk));
    entry->type = ParFlat_ENTRYNODE;
    entry->next = NULL;                 /* assume no segment at this time */
    entry->offset = (char*)MemNew(len + 1); /* plus 1 for null byte */

    if (pp->ifp == NULL)
        entry->len = FileReadBuf(entry->offset, len, pp->ffbuf);
    else
        entry->len = fread(entry->offset, 1, len, pp->ifp);
    if ((size_t)entry->len != len)  /* hardware problem */
    {
        ErrPostEx(SEV_FATAL, ERR_INPUT_CannotReadEntry,
                  "FileRead failed, in LoadEntry routine.");
        MemFree(entry->offset);
        MemFree(entry);
        return(NULL);
    }

    eptr = entry->offset + entry->len;
    bool was = false;
    char* wasx = NULL;
    for (q = entry->offset; q < eptr; q++)
    {
        if (*q != '\n')
            continue;

        if (wasx != NULL)
        {
            fta_StringCpy(wasx, q);     /* remove XX lines */
            eptr -= q - wasx;
            entry->len -= q - wasx;
            q = wasx;
        }
        if (q + 3 < eptr && q[1] == 'X' && q[2] == 'X')
            wasx = q;
        else
            wasx = NULL;
    }

    for (q = entry->offset; q < eptr; q++)
    {
        if (*q == 13) {
            *q = 10;
        }
        if (*q > 126 || (*q < 32 && *q != 10))
        {
            ErrPostEx(SEV_WARNING, ERR_FORMAT_NonAsciiChar,
                      "none-ASCII char, Decimal value %d, replaced by # ",
                      (int)*q);
            *q = '#';
        }

        /* Modified to skip empty line: Tatiana - 01/21/94
        */
        if (*q != '\n')
        {
            was = false;
            continue;
        }
        for (i = 0; q > entry->offset;)
        {
            i++;
            q--;
            if (*q != ' ')
                break;
        }
        if (i > 0 &&
            (*q == '\n' || (q - 2 >= entry->offset && *(q - 2) == '\n')))
        {
            q += i;
            i = 0;
        }
        if (i > 0)
        {
            if (*q != ' ')
            {
                q++;
                i--;
            }
            if (i > 0)
            {
                fta_StringCpy(q, q + i);
                eptr -= i;
                entry->len -= i;
            }
        }

        if (q + 3 < eptr && q[3] == '.')
        {
            q[3] = ' ';
            if (pp->source != Parser::ESource::NCBI || pp->format != Parser::EFormat::EMBL)
            {
                ErrPostEx(SEV_WARNING, ERR_FORMAT_DirSubMode,
                          "The format allowed only in DirSubMode: period after the tag");
            }
        }
        if (was)
        {
            fta_StringCpy(q, q + 1);    /* requires null byte */
            q--;
            eptr--;
            entry->len--;
        }
        else
            was = true;
    }

    entry->data = CreateEntryBlk();

    return(entry);
}

END_NCBI_SCOPE
