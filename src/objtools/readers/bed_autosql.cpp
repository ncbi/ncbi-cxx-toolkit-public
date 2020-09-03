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
#include "bed_autosql.hpp"

#include <functional>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ============================================================================
CBedAutoSql::CBedAutoSql(int bedFlags):
    mBedFlags(bedFlags),
    mColumnCount(0)
//  ============================================================================
{
}

//  ============================================================================
CBedAutoSql::~CBedAutoSql()
//  ============================================================================
{
}

//  ============================================================================
void
CBedAutoSql::Dump(
    ostream& ostr)
//  ============================================================================
{
    ostr << "CAutoSql:\n";
    ostr << "  Parameters:\n";
    for (auto item: mParameters) {
        ostr << "    \"" << item.first << "\" = \"" << item.second << "\"\n";
    }
    mWellKnownFields.Dump(ostr);
    mCustomFields.Dump(ostr);
}

//  ============================================================================
bool
CBedAutoSql::xParseAutoSqlColumnDef(
    const string& line,
    string& format,
    string& name,
    string& description)
//  ============================================================================
{
    string tail;
    NStr::SplitInTwo(line, " \t", format, tail, NStr::fSplit_MergeDelimiters);
    NStr::SplitInTwo(tail, " \t", name, description, NStr::fSplit_MergeDelimiters);
    NStr::ToLower(format);
    name = NStr::Replace(name, ";", "");
    description = NStr::Replace(description, "\"", "");
    return true;
}

//  ============================================================================
bool
CBedAutoSql::Load(
    CNcbiIstream& istr,
    CReaderMessageHandler& messageHandler)
//  =============================================================================
{
    bool readingTable = false;
    size_t autoSqlColCounter(0);

    while (!istr.eof()) {
        string line = xReadLine(istr);
        if (readingTable) {
            if (line == ")") {
                readingTable = false;
                continue;
            }
            string format, name, description;
            xParseAutoSqlColumnDef(line, format, name, description);
            if (!mWellKnownFields.ProcessTableRow(autoSqlColCounter, name, format)) {
                mCustomFields.Append(
                    CAutoSqlCustomField(
                        autoSqlColCounter, format, name, description));
            }
            ++autoSqlColCounter;
        }
        else {
            if (line == "(") {
                readingTable = true;
                continue;
            }
            if (line == "as") {
                continue;
            }
            string key, value;
            NStr::SplitInTwo(line, ":", key, value, NStr::fSplit_MergeDelimiters);
            key = NStr::TruncateSpaces(key);
            value = NStr::TruncateSpaces(value);
            if (key == "fieldCount") {
                mColumnCount = NStr::StringToUInt(value);
            }
            mParameters[key] = value;
        }
    }
    if (mColumnCount == 0) {
        mColumnCount = mWellKnownFields.NumFields() + mCustomFields.NumFields();
    }
    //Dump(cerr);
    return Validate(messageHandler);
}

//  =============================================================================
size_t
CBedAutoSql::ColumnCount() const
//  =============================================================================
{
    return mColumnCount;
}

//  ===============================================================================
string
CBedAutoSql::xReadLine(
    CNcbiIstream& istr)
//  ===============================================================================
{
    string line;
    while (line.empty()  &&  istr.good()) {
        std::getline(istr, line);
        line = NStr::TruncateSpaces(line);
    }
    return line;
}

//  ===============================================================================
bool
CBedAutoSql::ReadSeqFeat(
    const CBedColumnData& columnData,
    CSeq_feat& feat,
    CReaderMessageHandler& messageHandler) const
//  ===============================================================================
{
    //rules:
    // true: useful data was generated, even if incomplete
    // false: any data will be flawed, don't use
    // exception: something so bad we can't deal with it on this level.
    bool success = 
        mWellKnownFields.SetLocation(
            columnData, mBedFlags, feat, messageHandler)  &&
        mWellKnownFields.SetTitle(
            columnData, mBedFlags, feat, messageHandler)  &&
        mCustomFields.SetUserObject(
            columnData, mBedFlags, feat, messageHandler);
    if (!success) {
        return false;
    }
    return true;
}

//  ===============================================================================
bool
CBedAutoSql::Validate(
    CReaderMessageHandler& messageHandler) const
//  ===============================================================================
{
    if ( !mWellKnownFields.Validate(messageHandler)  ||  
            !mCustomFields.Validate(messageHandler)) {
        return false;
    }
    if (mColumnCount != mWellKnownFields.NumFields() + mCustomFields.NumFields()) {
        CReaderMessage fatal(
            EDiagSev::eDiag_Error,
            0,
            "AutoSql: The declared column count differs from the actual column count");
        messageHandler.Report(fatal);
        return false;
    }
    return true;
}
    
END_SCOPE(objects)
END_NCBI_SCOPE
