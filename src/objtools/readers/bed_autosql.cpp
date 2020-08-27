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
#include <objtools/readers/reader_error_codes.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/reader_error_codes.hpp>
#include "bed_autosql.hpp"

#include <functional>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ============================================================================
bool
CAutoSqlKnownFields::SetLocation(
    const vector<string>& fields,
    int bedFlags,
    CSeq_feat& feat) const
//  ============================================================================
{
    _ASSERT(Validate());
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
CAutoSqlKnownFields::SetTitle(
    const vector<string>& fields,
    int bedFlags,
    CSeq_feat& feat) const
//  ============================================================================
{
    if (mColChrom == -1) {
        return true;
    }
    feat.SetTitle(fields[mColChrom]);
    return true;
}

//  ============================================================================
void
CAutoSqlCustomFields::Append(
    const SAutoSqlColumnInfo& columnInfo)
//  ============================================================================
{
    mFields.push_back(columnInfo);
}

//  ============================================================================
void
CAutoSqlCustomFields::Dump(
    ostream& ostr)
//  ============================================================================
{
    ostr << "  Custom Fields:\n";
    for (auto colInfo: mFields) {
        ostr << "    column=\"" << colInfo.mColIndex << "\"" 
             << "    name=\"" << colInfo.mName << "\""
             << "    format=\"" << colInfo.mFormat << "\""
             << "    description=\"" << colInfo.mDescription << "\"\n";
    }
}

//  ============================================================================
bool
CAutoSqlCustomFields::SetUserObject(
    const vector<string>& fields,
    int bedFlags,
    CSeq_feat& feat) const
//  ============================================================================
{
    CRef<CUser_object> pAutoSqlCustomData(new CUser_object);
    pAutoSqlCustomData->SetType().SetStr( "AutoSqlCustomData" );

    CRef<CUser_field> pDummy(new CUser_field);
    for (const auto& fieldInfo: mFields) {
        if (! fieldInfo.SetUserField(fields, bedFlags, *pAutoSqlCustomData)) {
            return false;
        }
    }

    feat.SetData().SetUser(*pAutoSqlCustomData);
    return true;
}

//  ============================================================================
SAutoSqlColumnInfo::SAutoSqlColumnInfo(
    size_t colIndex, string format, string name, string description):
    mColIndex(colIndex),
    mFormat(format),
    mName(name),
    mDescription(description)
{
    if (NStr::EndsWith(format, "]")) {
        auto openBracket = format.find('[');
        if (openBracket != string::npos) {
            mFormat = format.substr(0, openBracket + 1) + "]";
        }
    }
}

//  ============================================================================
bool SAutoSqlColumnInfo::AddInt(
    const string& key,
    const string& value,
    int bedFlags,
    CUser_object& uo)
//  ============================================================================
{
    uo.AddField(key, NStr::StringToNonNegativeInt(value));
    return true;
}


//  ============================================================================
bool SAutoSqlColumnInfo::AddIntArray(
    const string& key,
    const string& value,
    int bedFlags,
    CUser_object& uo)
//  ============================================================================
{
    vector<string> intStrs;
    NStr::Split(value, ",", intStrs);
    vector<int> realInts;
    std::transform(
        intStrs.begin(), intStrs.end(), 
        std::back_inserter(realInts), 
        [] (const string& str) -> int { return NStr::StringToInt(str);} );
    uo.AddField(key, realInts);
    return true;
}



//  ============================================================================
bool SAutoSqlColumnInfo::AddString(
    const string& key,
    const string& value,
    int bedFlags,
    CUser_object& uo)
//  ============================================================================
{
    uo.AddField(key, value);
    return true;
}


//  ============================================================================
bool SAutoSqlColumnInfo::AddUint(
    const string& key,
    const string& value,
    int bedFlags,
    CUser_object& uo)
//  ============================================================================
{
    uo.AddField(key, NStr::StringToNonNegativeInt(value));
    return true;
}


SAutoSqlColumnInfo::FormatHandlers  SAutoSqlColumnInfo::mFormatHandlers = {
    {"int", SAutoSqlColumnInfo::AddInt},
    {"int[]", SAutoSqlColumnInfo::AddIntArray},
    {"string", SAutoSqlColumnInfo::AddString},
    {"uint", SAutoSqlColumnInfo::AddUint},
    {"uint[]", SAutoSqlColumnInfo::AddIntArray},
};

//  ============================================================================
bool
SAutoSqlColumnInfo::SetUserField(
    const vector<string>& fields,
    int bedFlags,
    CUser_object& uo) const
//  ============================================================================
{
    string valueStr = fields[mColIndex];
    if (NStr::EndsWith(mFormat, "[]")) {
        // deal with trailing comma in list
        NStr::TrimSuffixInPlace(valueStr, ",");
    }

    auto handlerIt = mFormatHandlers.find(mFormat);
    if (handlerIt != mFormatHandlers.end()) {
        return handlerIt->second(mName, valueStr, bedFlags, uo);
    }
    cerr << "Unknown format specifier \"" << mFormat << "\".";
    cerr << "Treating as string.\n";
    return AddString(mName, valueStr, bedFlags, uo);
}

//  ============================================================================
CBedAutoSql::CBedAutoSql(int bedFlags):
    mBedFlags(bedFlags)
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
    if (mWellKnownFields.ContainsInfo()) {
        ostr << "  Well known fields:\n";
        if (mWellKnownFields.mColChrom != -1) {
            ostr << "    colChrom=\"" << mWellKnownFields.mColChrom << "\"\n";
        }
        if (mWellKnownFields.mColSeqStart != -1) {
            ostr << "    colSeqStart=\"" << mWellKnownFields.mColSeqStart << "\"\n";
        }
        if (mWellKnownFields.mColSeqStop != -1) {
            ostr << "    colSeqStop=\"" << mWellKnownFields.mColSeqStop << "\"\n";
        }
        if (mWellKnownFields.mColStrand != -1) {
            ostr << "    colStrand=\"" << mWellKnownFields.mColStrand << "\"\n";
        }
        if (mWellKnownFields.mColName != -1) {
            ostr << "    colName=\"" << mWellKnownFields.mColName << "\"\n";
        }
        if (mWellKnownFields.mColScore != -1) {
            ostr << "    colScore=\"" << mWellKnownFields.mColScore << "\"\n";
        }
    }
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
    name = NStr::Replace(name, ";", "");
    description = NStr::Replace(description, "\"", "");
    return true;
}

//  ============================================================================
bool
CBedAutoSql::xStoreAsLocationInfo(
    size_t colIndex,
    const string& colName,
    const string& colFormat)
//  ============================================================================
{
    if (colName == "chrom"  &&  colFormat == "string") {
        mWellKnownFields.mColChrom = colIndex;
        return true;
    }
    if (colName == "chromStart"  &&  colFormat == "uint") {
        mWellKnownFields.mColSeqStart = colIndex;
        return true;
    }
    if (colName == "chromEnd"  &&  colFormat == "uint") {
        mWellKnownFields.mColSeqStop = colIndex;
        return true;
    }
    if (colName == "strand"  &&  colFormat == "char[1]") {
        mWellKnownFields.mColStrand = colIndex;
        return true;
    }
    if (colName == "name"  &&  colFormat == "string") {
        mWellKnownFields.mColName = colIndex;
        return true;
    }
    if (colName == "score"  &&  colFormat == "uint") {
        mWellKnownFields.mColScore = colIndex;
        return true;
    }
    return false;
}

//  ============================================================================
bool
CBedAutoSql::Load(
    CNcbiIstream& istr)
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
            if (!xStoreAsLocationInfo(autoSqlColCounter, name, format)) {
                mCustomFields.Append(
                    SAutoSqlColumnInfo(
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
            if (key == "fieldcount") {
                mColumnCount = NStr::StringToUInt(value);
            }
            mParameters[key] = value;
        }
    }
    Dump(cerr);
    return Validate();
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
    const vector<string>& fields,
    CSeq_feat& feat)
//  ===============================================================================
{
    bool success = 
        mWellKnownFields.SetLocation(fields, mBedFlags, feat)  &&
        mWellKnownFields.SetTitle(fields, mBedFlags, feat)  &&
        mCustomFields.SetUserObject(fields, mBedFlags, feat);
    if (!success) {
        return false;
    }
    return true;
}

//  ===============================================================================
bool
CBedAutoSql::Validate() const
//  ===============================================================================
{
    // check internal consistency
    return true;
}
    
END_SCOPE(objects)
END_NCBI_SCOPE
