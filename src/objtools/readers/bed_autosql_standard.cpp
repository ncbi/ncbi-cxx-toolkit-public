/*
 * $Id$
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
 * Authors:  Frank Ludwig
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objtools/readers/read_util.hpp>
#include <objtools/readers/reader_message.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/reader_error_codes.hpp>
#include "bed_autosql_standard.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ============================================================================
CAutoSqlStandardFields::CAutoSqlStandardFields():
//  ============================================================================
    mColChrom(-1), mColSeqStart(-1), mColSeqStop(-1), mColStrand(-1),
    mColName(-1), mColScore(-1)
{};


//  ============================================================================
bool
CAutoSqlStandardFields::ProcessTableRow(
    size_t colIndex,
    const string& colName,
    const string& colFormat)
//  ============================================================================
{
    if (colName == "chrom"  &&  colFormat == "string") {
        mColChrom = colIndex;
        return true;
    }
    if (colName == "chromStart"  &&  colFormat == "uint") {
        mColSeqStart = colIndex;
        return true;
    }
    if (colName == "chromEnd"  &&  colFormat == "uint") {
        mColSeqStop = colIndex;
        return true;
    }
    if (colName == "strand"  &&  colFormat == "char[1]") {
        mColStrand = colIndex;
        return true;
    }
    if (colName == "name"  &&  colFormat == "string") {
        mColName = colIndex;
        return true;
    }
    if (colName == "score"  &&  colFormat == "uint") {
        mColScore = colIndex;
        return true;
    }
    return false;
}

//  ============================================================================
bool
CAutoSqlStandardFields::SetLocation(
    const vector<string>& fields,
    int bedFlags,
    CSeq_feat& feat,
    CReaderMessageHandler& messageHandler) const
//  ============================================================================
{
    CRef<CSeq_id> pId = CReadUtil::AsSeqId(fields[mColChrom], bedFlags, false);

    auto& location = feat.SetLocation().SetInt();
    location.SetId(*pId);
    location.SetFrom(NStr::StringToUInt(fields[mColSeqStart]));
    location.SetTo(NStr::StringToUInt(fields[mColSeqStop])-1);
    bool onNegativeStrand = (mColStrand != -1  &&  fields[mColStrand][0] == '-');
    location.SetStrand(onNegativeStrand? eNa_strand_minus : eNa_strand_plus);

    return true;
}

//  ============================================================================
bool
CAutoSqlStandardFields::SetTitle(
    const vector<string>& fields,
    int bedFlags,
    CSeq_feat& feat,
    CReaderMessageHandler& messageHandler) const
//  ============================================================================
{
    if (mColChrom == -1) {
        return true;
    }
    feat.SetTitle(fields[mColChrom]);
    return true;
}

//  ============================================================================
bool
CAutoSqlStandardFields::Validate(
    CReaderMessageHandler& messageHandler) const
//  ============================================================================
{
    return (mColChrom != -1  &&  mColSeqStart != -1  &&  mColSeqStop != -1);
}

//  ============================================================================
void
CAutoSqlStandardFields::Dump(
    ostream& ostr) const
//  ============================================================================
{
    ostr << "  Well known fields:\n";
    if (mColChrom != -1) {
        ostr << "    colChrom=\"" << mColChrom << "\"\n";
    }
    if (mColSeqStart != -1) {
        ostr << "    colSeqStart=\"" << mColSeqStart << "\"\n";
    }
    if (mColSeqStop != -1) {
        ostr << "    colSeqStop=\"" << mColSeqStop << "\"\n";
    }
    if (mColStrand != -1) {
        ostr << "    colStrand=\"" << mColStrand << "\"\n";
    }
    if (mColName != -1) {
        ostr << "    colName=\"" << mColName << "\"\n";
    }
    if (mColScore != -1) {
        ostr << "    colScore=\"" << mColScore << "\"\n";
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
