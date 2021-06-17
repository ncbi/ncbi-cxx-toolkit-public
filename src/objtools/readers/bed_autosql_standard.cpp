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
    mColName(-1), mColScore(-1), mNumFields(0)
{};

//  ============================================================================
bool
CAutoSqlStandardFields::ProcessTableRow(
    size_t colIndex,
    const string& colName,
    const string& colFormat)
//  ============================================================================
{
    ++mNumFields;
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
    --mNumFields;
    return false;
}

//  ============================================================================
bool
CAutoSqlStandardFields::SetLocation(
    const CBedColumnData& columnData,
    int bedFlags,
    CSeq_feat& feat,
    CReaderMessageHandler& messageHandler) const
//  ============================================================================
{
    CRef<CSeq_id> pId = CReadUtil::AsSeqId(columnData[mColChrom], bedFlags, false);

    auto& location = feat.SetLocation().SetInt();
    location.SetId(*pId);
    try {
        location.SetFrom(NStr::StringToUInt(columnData[mColSeqStart]));
    }
    catch (CStringException&) {
        CReaderMessage error(
            eDiag_Error,
            columnData.LineNo(),
            "BED: Invalid data for column \"chromStart\". Feature omitted");
        messageHandler.Report(error);
        return false;
    }

    try {
        location.SetTo(NStr::StringToUInt(columnData[mColSeqStop])-1);
    }
    catch (CStringException&) {
        CReaderMessage error(
            eDiag_Error,
            columnData.LineNo(),
            "BED: Invalid data for column \"chromEnd\". Feature omitted");
        messageHandler.Report(error);
        return false;
    }

    if (mColStrand == -1) {
        return true;
    }

    CReaderMessage warning(
        eDiag_Warning,
        columnData.LineNo(),
        "BED: Invalid data for column \"strand\". Defaulting to \"+\"");

    location.SetStrand(eNa_strand_plus);
    auto strandStr = columnData[mColStrand];
    if (strandStr.size() != 1) {
        messageHandler.Report(warning);
    }
    else {
        auto strandChar = strandStr[0];
        if (string("+-.").find(strandChar) == string::npos) {
            messageHandler.Report(warning);
        }
        else if (strandChar == '-') {
           location.SetStrand(eNa_strand_minus);
        }
    }
    return true;
}

//  ============================================================================
bool
CAutoSqlStandardFields::SetTitle(
    const CBedColumnData& columnData,
    int bedFlags,
    CSeq_feat& feat,
    CReaderMessageHandler& messageHandler) const
//  ============================================================================
{
    if (mColChrom == -1) {
        return true;
    }
    feat.SetTitle(columnData[mColChrom]);
    return true;
}

//  ============================================================================
bool
CAutoSqlStandardFields::Validate(
    CReaderMessageHandler& messageHandler) const
//  ============================================================================
{
    //at issue: do we have enough information to make a Seq-loc
    if (mColChrom == -1  ||  mColSeqStart == -1  ||  mColSeqStop == -1) {
        CReaderMessage fatal(
            EDiagSev::eDiag_Error,
            0,
            "AutoSql: Table does not contain enough information to set a feature location.");
        messageHandler.Report(fatal);
        return false;
    }
    return true;
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
