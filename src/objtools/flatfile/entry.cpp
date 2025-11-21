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
 * File Name: entry.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <objects/seqcode/Seq_code_type.hpp>

#include "ftacpp.hpp"
#include "index.h"
#include "ftaerr.hpp"
#include "indx_err.h"
#include "utilfun.h"
#include "ftablock.h"
#include "entry.h"
#include "indx_blk.h"
#include "genbank.h"

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "entry.cpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
void Section::xBuildSubBlock(int subtype, string_view subKw)
//  ----------------------------------------------------------------------------
{
    auto subBegin = mTextLines.end(), subEnd = mTextLines.end();

    bool found(false);
    for (auto lineIt = mTextLines.begin(); lineIt != mTextLines.end(); lineIt++) {
        const string& line         = *lineIt;
        auto          firstCharPos = line.find_first_not_of(" ");
        if (found) {
            if (firstCharPos >= 12) {
                subEnd++;
                continue;
            } else
                break;
        }
        if (line.starts_with(subKw)) {
            subBegin = lineIt;
            subEnd   = subBegin + 1;
            found    = true;
            continue;
        }
    }
    if (subBegin != mTextLines.end()) {
        mSubSections.push_back(new Section(subtype, vector<string>(subBegin, subEnd)));
        mTextLines.erase(subBegin, subEnd);
    }
}

//  ----------------------------------------------------------------------------
void Section::xBuildFeatureBlocks()
//  ----------------------------------------------------------------------------
{
    const int COL_FEATKEY = 5;

    auto subBegin = mTextLines.end(), subEnd = mTextLines.end();
    bool found(false);
    auto lineIt = mTextLines.begin();
    while (lineIt != mTextLines.end()) {
        auto line         = *lineIt;
        auto firstCharPos = line.find_first_not_of(" ");
        if (found) {
            if (firstCharPos > COL_FEATKEY) {
                subEnd++;
                lineIt++;
                continue;
            } else {
                if (subBegin != mTextLines.end()) {
                    mSubSections.push_back(
                        new Section(ParFlat_FEATBLOCK, vector<string>(subBegin, subEnd)));
                    mTextLines.erase(subBegin, subEnd);
                }
                subBegin = subEnd = mTextLines.end();
                found             = false;
                lineIt            = mTextLines.begin();
                continue;
            }
        }
        if (firstCharPos == COL_FEATKEY) {
            subBegin = lineIt;
            subEnd   = subBegin + 1;
            found    = true;
            lineIt++;
            continue;
        }
        lineIt++;
    }
    if (subBegin != mTextLines.end()) {
        mSubSections.push_back(
            new Section(ParFlat_FEATBLOCK, vector<string>(subBegin, subEnd)));
        mTextLines.erase(subBegin, subEnd);
    }
}

//  ----------------------------------------------------------------------------
bool Entry::xInitNidSeqId(
    CBioseq& bioseq, int type, int dataOffset, Parser::ESource)
//  -----------------------------------------------------------------------------
{
    SectionPtr secPtr = this->GetFirstSectionOfType(type);
    if (! secPtr) {
        return true;
    }
    throw std::runtime_error("xInitNidSeqId: Details not yet implemented");
    return false;
}


//  -----------------------------------------------------------------------------
bool Entry::IsAA() const
//  -----------------------------------------------------------------------------
{
    IndexblkPtr ibp = mPp->entrylist[mPp->curindx];
    string      mol = mBaseData.substr(ibp->lc.bp, 2);
    return (mol == "aa");
}


//  -----------------------------------------------------------------------------
bool Entry::xInitSeqInst(const unsigned char* pConvert)
//  -----------------------------------------------------------------------------
{
    IndexblkPtr  ibp = mPp->entrylist[mPp->curindx];
    LocusContPtr lcp = &ibp->lc;

    auto& bioseq  = mSeqEntry->SetSeq();
    auto& seqInst = bioseq.SetInst();
    seqInst.SetRepr(ibp->is_mga ? CSeq_inst::eRepr_virtual : CSeq_inst::eRepr_raw);
    // Int2         topology;
    // Int2         strand;
    // char*      strandstr;

    string topologyStr = mBaseData.substr(lcp->topology, 16);
    int    topology    = CheckTPG(topologyStr);
    if (topology > 1)
        seqInst.SetTopology(static_cast<CSeq_inst::ETopology>(topology));

    string strandStr = mBaseData.substr(lcp->strand, 16);
    int    strand    = CheckSTRAND((lcp->strand >= 0) ? strandStr : "   ");
    if (strand > 0)
        seqInst.SetStrand(static_cast<CSeq_inst::EStrand>(strand));

    // auto codeType = (ibp->is_prot ? eSeq_code_type_iupacaa : eSeq_code_type_iupacna);
    // if (!GetSeqData(pp, entry, bioseq, ParFlat_ORIGIN, dnaconv, codeType))
    //     return false;

    // if (ibp->is_contig && !GetGenBankInstContig(entry, bioseq, pp))
    //     return false;

    return true;
}


/**********************************************************/
static size_t FileReadBuf(char* to, size_t len, FileBuf& ffbuf)
{
    const char* p = nullptr;
    char*       q;
    size_t      i;

    for (p = ffbuf.current, q = to, i = 0; i < len && *p != '\0'; i++)
        *q++ = *p++;

    ffbuf.current = p;
    return (i);
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
DataBlk* LoadEntry(ParserPtr pp, size_t offset, size_t len)
{
    pp->ffbuf.set_offs(offset);

    char*  bptr = StringNew(len); /* includes nul byte */
    size_t blen = FileReadBuf(bptr, len, pp->ffbuf);

    if (blen != len) /* hardware problem */
    {
        FtaErrPost(SEV_FATAL, ERR_INPUT_CannotReadEntry, "FileRead failed, in LoadEntry routine.");
        MemFree(bptr);
        return nullptr;
    }

    char* eptr = bptr + blen;
    bool  was  = false;
    char* wasx = nullptr;
    for (char* q = bptr; q < eptr; q++) {
        if (*q != '\n')
            continue;

        if (wasx) {
            fta_StringCpy(wasx, q); /* remove XX lines */
            eptr -= q - wasx;
            blen -= q - wasx;
            q = wasx;
        }
        if (q + 3 < eptr && q[1] == 'X' && q[2] == 'X')
            wasx = q;
        else
            wasx = nullptr;
    }

    for (char* q = bptr; q < eptr; q++) {
        if (*q == 13) {
            *q = 10;
        }
        if (*q > 126 || (*q < 32 && *q != 10)) {
            FtaErrPost(SEV_WARNING, ERR_FORMAT_NonAsciiChar, "none-ASCII char, Decimal value {}, replaced by # ", (int)*q);
            *q = '#';
        }

        /* Modified to skip empty line: Tatiana - 01/21/94 */
        if (*q != '\n') {
            was = false;
            continue;
        }
        size_t i;
        for (i = 0; q > bptr;) {
            i++;
            q--;
            if (*q != ' ')
                break;
        }
        if (i > 0 &&
            (*q == '\n' || (q - 2 >= bptr && *(q - 2) == '\n'))) {
            q += i;
            i = 0;
        }
        if (i > 0) {
            if (*q != ' ') {
                q++;
                i--;
            }
            if (i > 0) {
                fta_StringCpy(q, q + i);
                eptr -= i;
                blen -= i;
            }
        }

        if (q + 3 < eptr && q[3] == '.') {
            q[3] = ' ';
            if (pp->source != Parser::ESource::NCBI || pp->format != Parser::EFormat::EMBL) {
                FtaErrPost(SEV_WARNING, ERR_FORMAT_DirSubMode, "The format allowed only in DirSubMode: period after the tag");
            }
        }
        if (was) {
            fta_StringCpy(q, q + 1); /* requires null byte */
            q--;
            eptr--;
            blen--;
        } else
            was = true;
    }

    DataBlk* entry = new DataBlk(ParFlat_ENTRYNODE, bptr, blen);
    entry->SetEntryData(new EntryBlk());
    return entry;
}

END_NCBI_SCOPE
