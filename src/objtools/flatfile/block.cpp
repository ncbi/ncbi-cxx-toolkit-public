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
 * File Name: block.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *      Parsing flatfile to blocks in memory.
 *
 */

#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include "ftaerr.hpp"
#include "ftablock.h"
#include "indx_blk.h"
#include "loadfeat.h"
#include "indx_def.h"
#include "utilfun.h"

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "block.cpp"
BEGIN_NCBI_SCOPE

struct QSStruct {
    string    accession;
    Int2      version = 0;
    size_t    offset  = 0;
    size_t    length  = 0;
};
using QSStructList = std::forward_list<QSStruct>;

/**********************************************************/
DataBlk::~DataBlk()
{
    mpQscore.clear();
    deleteData();
    if (mType == ParFlat_ENTRYNODE) {
        MemFree(mBuf.ptr);
    }
}

void DataBlk::SetEntryData(EntryBlk* p)
{
    mData = p;
}
EntryBlk* DataBlk::GetEntryData() const
{
    return std::get<EntryBlk*>(mData);
}

/**********************************************************
 *
 *   void FreeEntry(entry):
 *
 *      Only free entry itself and ebp->chain data because
 *   ebp->sep has to be used to write out ASN.1 output then
 *   free ebp->sep and ebp itself together.
 *
 *                                              5-12-93
 *
 **********************************************************/

void DataBlk::deleteData()
{
    if (holds_alternative<monostate>(mData))
        return;
    if (holds_alternative<TDataBlkList>(mData)) {
        auto& subblocks = std::get<TDataBlkList>(mData);
        subblocks.clear();
    } else if (holds_alternative<EntryBlk*>(mData)) {
        auto& p = std::get<EntryBlk*>(mData);
        delete p;
        p = nullptr;
    } else if (holds_alternative<FeatBlk*>(mData)) {
        auto& p = std::get<FeatBlk*>(mData);
        delete p;
        p = nullptr;
    } else if (holds_alternative<const TXmlIndexList*>(mData)) {
        auto& p = std::get<const TXmlIndexList*>(mData);
        p = nullptr;
    }
    mData.emplace<monostate>();
}

/**********************************************************/
static bool AccsCmp(const Indexblk* ibp1, const Indexblk* ibp2)
{
    int i = StringCmp(ibp1->acnum, ibp2->acnum);
    if (i != 0)
        return i < 0;

    if (ibp1->vernum != ibp2->vernum)
        return ibp1->vernum < ibp2->vernum;

    return ibp2->offset < ibp1->offset;
}

/**********************************************************/
static bool QSCmp(QSStructList::const_iterator qs1, QSStructList::const_iterator qs2)
{
    int i = StringCmp(qs1->accession.c_str(), qs2->accession.c_str());
    if (i != 0)
        return i < 0;

    return qs1->version < qs2->version;
}

/**********************************************************/
static bool QSNoSequenceRecordErr(bool accver, const QSStruct& qssp)
{
    if (accver)
        FtaErrPost(SEV_FATAL, ERR_QSCORE_NoSequenceRecord, "Encountered Quality Score data for a record \"{}.{}\" that does not exist in the file of sequence records being parsed.", qssp.accession, qssp.version);
    else
        FtaErrPost(SEV_FATAL, ERR_QSCORE_NoSequenceRecord, "Encountered Quality Score data for a record \"{}\" that does not exist in the file of sequence records being parsed.", qssp.accession);
    return false;
}

/**********************************************************/
bool QSIndex(ParserPtr pp, const TIndBlkList& ibl, unsigned ibl_size)
{
    char*  p;
    char*  q;
    bool   ret;
    size_t i;
    Int4   count;
    Int4   j;
    Uint4  k;
    Int4   l;
    Int2   m;
    Char   buf[1024];

    if (! pp->qsfd)
        return true;

    QSStructList qssp;
    QSStructList::iterator tqssp = qssp.before_begin();
    QSStructList::iterator tqsspprev;

    count = 0;
    while (fgets(buf, 1023, pp->qsfd)) {
        if (buf[0] != '>')
            continue;

        p = StringChr(buf, ' ');
        if (! p)
            continue;

        i  = (size_t)StringLen(buf);
        *p = '\0';

        q = StringChr(buf, '.');
        if (q)
            *q++ = '\0';

        tqssp            = qssp.emplace_after(tqssp);
        tqssp->accession = string(buf + 1);
        tqssp->version   = q ? atoi(q) : 0;
        tqssp->offset    = (size_t)ftell(pp->qsfd) - i;
        if (count > 0)
            tqsspprev->length = tqssp->offset - tqsspprev->offset;

        tqsspprev = tqssp;
        count++;
    }
    tqssp->length = (size_t)ftell(pp->qsfd) - tqssp->offset;

    if (qssp.empty()) {
        FtaErrPost(SEV_FATAL, ERR_QSCORE_NoScoreDataFound, "No correctly formatted records containing quality score data were found within file \"{}\".", pp->qsfile);
        return false;
    }

    vector<QSStructList::iterator> qsspp(count);
    tqssp = qssp.begin();
    for (j = 0; j < count && tqssp != qssp.end(); j++, ++tqssp)
        qsspp[j] = tqssp;

    if (count > 1) {
        std::sort(qsspp.begin(), qsspp.end(), QSCmp);

        for (j = 0, count--; j < count; j++)
            if (qsspp[j]->accession == qsspp[j + 1]->accession)
                if (pp->accver == false ||
                    qsspp[j]->version == qsspp[j + 1]->version)
                    break;

        if (j < count) {
            if (pp->accver)
                FtaErrPost(SEV_FATAL, ERR_QSCORE_RedundantScores, "Found more than one set of Quality Score for accession \"{}.{}\".", qsspp[j]->accession, qsspp[j]->version);
            else
                FtaErrPost(SEV_FATAL, ERR_QSCORE_RedundantScores, "Found more than one set of Quality Score for accession \"{}\".", qsspp[j]->accession);

            qssp.clear();
            return false;
        }
        count++;
    }

    vector<IndexblkPtr> ibpp;
    ibpp.reserve(ibl_size);
    for (const auto& it : ibl)
        ibpp.push_back(it.get());

    if (ibl_size > 1)
        std::sort(ibpp.begin(), ibpp.end(), AccsCmp);

    for (ret = true, j = 0, k = 0; j < count; j++) {
        if (k == ibl_size) {
            ret = QSNoSequenceRecordErr(pp->accver, *qsspp[j]);
            continue;
        }
        for (; k < ibl_size; k++) {
            l = StringCmp(qsspp[j]->accession.c_str(), ibpp[k]->acnum);
            if (l < 0) {
                ret = QSNoSequenceRecordErr(pp->accver, *qsspp[j]);
                break;
            }
            if (l > 0)
                continue;
            m = qsspp[j]->version - ibpp[k]->vernum;
            if (m < 0) {
                ret = QSNoSequenceRecordErr(pp->accver, *qsspp[j]);
                break;
            }
            if (m > 0)
                continue;
            ibpp[k]->qsoffset = qsspp[j]->offset;
            ibpp[k]->qslength = qsspp[j]->length;
            k++;
            break;
        }
    }

    qssp.clear();

    return (ret);
}

END_NCBI_SCOPE
