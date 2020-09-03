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
    {"double", CAutoSqlCustomField::AddDouble},
    {"int", CAutoSqlCustomField::AddInt},
    {"int[]", CAutoSqlCustomField::AddIntArray},
    {"lstring", CAutoSqlCustomField::AddString},
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
    auto handlerIt = mFormatHandlers.find(mFormat);
    if (handlerIt != mFormatHandlers.end()) {
        mHandler = handlerIt->second;
    }
    else {
        mHandler = CAutoSqlCustomField::AddString;
    } 
}

//  ============================================================================
bool CAutoSqlCustomField::AddDouble(
    const string& key,
    const string& value,
    unsigned int lineNo,
    int bedFlags,
    CUser_object& uo,
    CReaderMessageHandler& messageHandler)
//  ============================================================================
{
    double floatVal = 0;
    try {
        floatVal = NStr::StringToDouble(value);
    }
    catch (CStringException&) {
        CReaderMessage warning(
            eDiag_Warning,
            lineNo,
            string("BED: Unable to convert \"") + key + "\" value \"" + value + 
                "\" to float. Defaulting to 0.0");
        messageHandler.Report(warning);
    }
    uo.AddField(key, floatVal);
    return true;
}


//  ============================================================================
bool CAutoSqlCustomField::AddInt(
    const string& key,
    const string& value,
    unsigned int lineNo,
    int bedFlags,
    CUser_object& uo,
    CReaderMessageHandler& messageHandler)
//  ============================================================================
{
    int intVal = 0;
    try {
        intVal = NStr::StringToInt(value);
    }
    catch (CStringException&) {
        CReaderMessage warning(
            eDiag_Warning,
            lineNo,
            string("BED: Unable to convert \"") + key + "\" value \"" + value + 
                "\" to int. Defaulting to 0");
        messageHandler.Report(warning);
    }
    uo.AddField(key, intVal);
    return true;
}

//  ============================================================================
bool CAutoSqlCustomField::AddIntArray(
    const string& key,
    const string& value,
    unsigned int lineNo,
    int bedFlags,
    CUser_object& uo,
    CReaderMessageHandler& messageHandler)
//  ============================================================================
{
    vector<string> intStrs;
    NStr::Split(value, ",", intStrs);
    vector<int> realInts;
    try {
        std::transform(
            intStrs.begin(), intStrs.end(), 
            std::back_inserter(realInts), 
            [] (const string& str) -> int { return NStr::StringToInt(str);} );
    }
    catch(CStringException&) {
        CReaderMessage warning(
            eDiag_Warning,
            lineNo,
            string("BED: Unable to convert \"") + key + "\" value \"" + value + 
                "\" to int list. Defaulting to empty list");
        messageHandler.Report(warning);
        realInts.clear();
    }
    uo.AddField(key, realInts);
    return true;
}

//  ============================================================================
bool CAutoSqlCustomField::AddString(
    const string& key,
    const string& value,
    unsigned int lineNo,
    int bedFlags,
    CUser_object& uo,
    CReaderMessageHandler& messageHandler)
//  ============================================================================
{
    uo.AddField(key, value);
    return true;
}

//  ============================================================================
bool CAutoSqlCustomField::AddUint(
    const string& key,
    const string& value,
    unsigned int lineNo,
    int bedFlags,
    CUser_object& uo,
    CReaderMessageHandler& messageHandler)
//  ============================================================================
{
    return AddInt(key, value, lineNo, bedFlags, uo, messageHandler);
}

//  ============================================================================
bool CAutoSqlCustomField::AddUintArray(
    const string& key,
    const string& value,
    unsigned int lineNo,
    int bedFlags,
    CUser_object& uo,
    CReaderMessageHandler& messageHandler)
//  ============================================================================
{
    return AddIntArray(key, value, lineNo, bedFlags, uo, messageHandler);
}

//  ============================================================================
bool
CAutoSqlCustomField::xHandleSpecialCases(
    const CBedColumnData& columnData,
    int bedFlags,
    CUser_object& uo,
    CReaderMessageHandler& messageHandler) const
//  ============================================================================
{
    return xHandleSpecialCaseRgb(columnData, bedFlags, uo, messageHandler);
}

//  ============================================================================
bool
CAutoSqlCustomField::xHandleSpecialCaseRgb(
    const CBedColumnData& columnData,
    int bedFlags,
    CUser_object& uo,
    CReaderMessageHandler& messageHandler) const
//  ============================================================================
{
    //if it's column 9 or the key and format suggests it's an RGB value then accept
    // (r,g,b) or #RRGGBB and convert to int.
    //
    if (mFormat != "int"  &&  mFormat != "uint") {
        return false;
    }

    vector<string> knownRgbKeys = {"itemrgb", "color", "colour"};
    string lowerName(mName);
    NStr::ToLower(lowerName);
    bool isRgbKey = (find(knownRgbKeys.begin(), knownRgbKeys.end(), lowerName) !=
        knownRgbKeys.end());
    if (mColIndex != 8  &&  !isRgbKey) {
        return false;
    }
    string valueStr = columnData[mColIndex];

    if (NStr::StartsWith(valueStr, "#")) {
        int intVal = 0;
        try {
            intVal = NStr::StringToInt(valueStr.substr(1), 0, 16);
        }
        catch (CStringException&) {
            CReaderMessage warning(
                eDiag_Warning,
                columnData.LineNo(),
                string("BED: Unable to convert \"") + mName + "\" value \"" + 
                    valueStr +  "\" to int. Defaulting to 0");
            messageHandler.Report(warning);
        }
        uo.AddField(mName, intVal);
        return true;
    }

    vector<string> rgb;
    NStr::Split(valueStr, ",", rgb);
    int rgbInt(0);
    if (rgb.size() == 3) {
        try {
            rgbInt = 256*256*NStr::StringToInt(rgb[0]) + 
                256*NStr::StringToInt(rgb[1]) + NStr::StringToInt(rgb[2]);
        }
        catch (CStringException&) {
            CReaderMessage warning(
                eDiag_Warning,
                columnData.LineNo(),
                string("BED: Unable to convert \"") + mName + "\" value \"" + 
                    valueStr +  "\" to int. Defaulting to 0");
            messageHandler.Report(warning);
        }
        uo.AddField(mName, rgbInt);
        return true;
    }
    // no special case after all, use regular processing
    return false;
}


//  ============================================================================
bool
CAutoSqlCustomField::SetUserField(
    const CBedColumnData& columnData,
    int bedFlags,
    CUser_object& uo,
    CReaderMessageHandler& messageHandler) const
//  ============================================================================
{
    if (xHandleSpecialCases(columnData, bedFlags, uo, messageHandler)) {
        return true;
    }
    string valueStr = columnData[mColIndex];
    if (NStr::EndsWith(mFormat, "[]")) {
        // deal with trailing comma in list
        NStr::TrimSuffixInPlace(valueStr, ",");
    }

    //note:
    // we need some extra policy decisions on error handling in custom field.
    // until then: avoid throwing at all costs,
    // return false like never.
    return mHandler(
        mName, valueStr, columnData.LineNo(), bedFlags, uo, messageHandler);
}

//  ============================================================================
bool
CAutoSqlCustomField::Validate(
    CReaderMessageHandler& messageHandler) const
//  ============================================================================
{
    if (mFormatHandlers.find(mFormat) == mFormatHandlers.end()) {
        CReaderMessage warning(
            EDiagSev::eDiag_Warning,
            static_cast<int>(mColIndex),
            string("AutoSql: Format \"") + mFormat +
                "\" for \"" + mName +
                "\" not recognized, processing as string");
        messageHandler.Report(warning);
    }
    return true;
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
    const CBedColumnData& columnData,
    int bedFlags,
    CSeq_feat& feat,
    CReaderMessageHandler& messageHandler) const
//  ============================================================================
{
    CRef<CUser_object> pAutoSqlCustomData(new CUser_object);
    pAutoSqlCustomData->SetType().SetStr( "AutoSqlCustomData" );

    CRef<CUser_field> pDummy(new CUser_field);
    for (const auto& fieldInfo: mFields) {
        if (! fieldInfo.SetUserField(
                columnData, bedFlags, *pAutoSqlCustomData, messageHandler)) {
            return false;
        }
    }

    feat.SetData().SetUser(*pAutoSqlCustomData);
    return true;
}

//  ============================================================================
bool
CAutoSqlCustomFields::Validate(
    CReaderMessageHandler& messageHandler) const
//  ============================================================================
{
    for (const auto& field: mFields) {
        if (!field.Validate(messageHandler)) {
            return false;
        }
    }
    return true;
}

END_SCOPE(objects)
END_NCBI_SCOPE
