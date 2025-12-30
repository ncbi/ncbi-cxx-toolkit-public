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
static string FileReadBuf(FileBuf& ffbuf, size_t offset, size_t len)
{
    ffbuf.set_offs(offset);
    const char* p = ffbuf.current;

    string s;
    s.reserve(len);
    size_t i;
    for (i = 0; i < len && *p != '\0'; i++)
        s.push_back(*p++);

    ffbuf.current = p;
    if (i != len)
        s.clear();
    return s;
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
unique_ptr<DataBlk> LoadEntry(ParserPtr pp, Indexblk* ibp)
{
    string buf = FileReadBuf(pp->ffbuf, ibp->offset, ibp->len);
    if (buf.empty()) /* hardware problem */
    {
        FtaErrPost(SEV_FATAL, ERR_INPUT_CannotReadEntry, "FileRead failed, in LoadEntry routine.");
        return {};
    }

    size_t wasx = string::npos;
    for (size_t q = 0; q < buf.size(); q++) {
        if (buf[q] != '\n')
            continue;

        if (wasx != string::npos) {
            buf.erase(wasx, q - wasx); /* remove XX lines */
            q = wasx;
        }
        if (q + 3 < buf.size() && buf[q + 1] == 'X' && buf[q + 2] == 'X')
            wasx = q;
        else
            wasx = string::npos;
    }

    bool was = false;
    for (size_t q = 0; q < buf.size(); q++) {
        char& c = buf[q];
        if (c == 13) {
            c = 10;
        }
        if (c > 126 || (c < 32 && c != 10)) {
            FtaErrPost(SEV_WARNING, ERR_FORMAT_NonAsciiChar, "non-ASCII char, Decimal value {}, replaced by # ", int(c));
            c = '#';
        } else if (q + 2 < buf.size() && buf[q] == '&' && buf[q + 1] == '#') {
            size_t i = q + 2;
            while (i < buf.size() && buf[i] >= '0' && buf[i] <= '9')
                i++;
            if (i > q + 2 && i < q + 7 && i < buf.size() && buf[i] == ';') {
                string_view s   = string_view(buf).substr(q, i + 1 - q);
                IndexblkPtr ibp = pp->entrylist[pp->curindx];
                FtaErrPost(SEV_REJECT, ERR_FORMAT_NonAsciiChar,
                           "Non-ASCII Unicode characters are not allowed: \"{}\". Entry skipped: \"{}|{}\".",
                           s, ibp->locusname, ibp->acnum);
                ibp->drop = true;
            }
        }

        /* Modified to skip empty line: Tatiana - 01/21/94 */
        if (c != '\n') {
            was = false;
            continue;
        }
        size_t i = 0;
        while (q > 0) {
            i++;
            q--;
            if (buf[q] != ' ')
                break;
        }
        if (i > 0 &&
            (buf[q] == '\n' || (q >= 2 && buf[q - 2] == '\n'))) {
            q += i;
            i = 0;
        }
        if (i > 0) {
            if (buf[q] != ' ') {
                q++;
                i--;
            }
            if (i > 0) {
                buf.erase(q, i);
            }
        }

        if (q + 3 < buf.size() && buf[q + 3] == '.') {
            buf[q + 3] = ' ';
            if (pp->source != Parser::ESource::NCBI || pp->format != Parser::EFormat::EMBL) {
                FtaErrPost(SEV_WARNING, ERR_FORMAT_DirSubMode, "The format allowed only in DirSubMode: period after the tag");
            }
        }
        if (was) {
            buf.erase(q, 1);
            q--;
        } else
            was = true;
    }

    return MakeEntry(buf);
}

unique_ptr<DataBlk> MakeEntry(string buf)
{
    if (buf.empty())
        return {};
    auto entry = make_unique<DataBlk>(ParFlat_ENTRYNODE, StringSave(buf), buf.size());
    entry->SetEntryData(new EntryBlk());
    return entry;
}

END_NCBI_SCOPE
