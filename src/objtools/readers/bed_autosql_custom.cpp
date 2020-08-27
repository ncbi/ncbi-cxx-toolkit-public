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
CAutoSqlCustomField::FormatHandlers  CAutoSqlCustomField::mFormatHandlers = {
//  ============================================================================
    {"int", CAutoSqlCustomField::AddInt},
    {"int[]", CAutoSqlCustomField::AddIntArray},
    {"string", CAutoSqlCustomField::AddString},
    {"uint", CAutoSqlCustomField::AddUint},
    {"uint[]", CAutoSqlCustomField::AddIntArray},
};


//  ============================================================================
void
CAutoSqlCustomField::Dump(
    ostream& ostr)
//  ============================================================================
{
    ostr << "    column=\"" << mColIndex << "\"" 
            << "    name=\"" << mName << "\""
            << "    format=\"" << mFormat << "\""
            << "    description=\"" << mDescription << "\"\n";
}


//  ============================================================================
CAutoSqlCustomField::CAutoSqlCustomField(
    size_t colIndex, string format, string name, string description):
//  ============================================================================
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
bool CAutoSqlCustomField::AddInt(
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
bool CAutoSqlCustomField::AddIntArray(
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
bool CAutoSqlCustomField::AddString(
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
bool CAutoSqlCustomField::AddUint(
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
bool
CAutoSqlCustomField::SetUserField(
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
void
CAutoSqlCustomFields::Append(
    const CAutoSqlCustomField& columnInfo)
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
        colInfo.Dump(ostr);
    }
}

//  ============================================================================
bool
CAutoSqlCustomFields::SetUserObject(
    const vector<string>& fields,
    int bedFlags,
    CSeq_feat& feat,
    CReaderMessageHandler&) const
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
bool
CAutoSqlCustomFields::Validate(
    CReaderMessageHandler&) const
//  ============================================================================
{
    return true;
}

END_SCOPE(objects)
END_NCBI_SCOPE
