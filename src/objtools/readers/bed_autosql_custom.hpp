#ifndef _BED_AUTOSQL_CUSTOM_HPP_
#define _BED_AUTOSQL_CUSTOM_HPP_

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
 * Authors: Frank Ludwig
 *
 */

#include <corelib/ncbistd.hpp>
#include <objects/general/User_field.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include "reader_message_handler.hpp"
#include "bed_column_data.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ============================================================================
class CAutoSqlCustomField
//  ============================================================================
{
public:
    CAutoSqlCustomField(
            size_t colIndex, string format, string name, string description);

    bool
    SetUserField(
        const CBedColumnData&,
        int bedFlags,
        CUser_object&,
        CReaderMessageHandler&) const;

    bool Validate(
        CReaderMessageHandler&) const;

private:
    bool
    xHandleSpecialCases(
        const CBedColumnData&,
        int bedFlags,
        CUser_object&,
        CReaderMessageHandler&) const;

    bool
    xHandleSpecialCaseRgb(
        const CBedColumnData&,
        int bedFlags,
        CUser_object&,
        CReaderMessageHandler&) const;

    using FormatHandler =
        bool (*)(const string&, const string&, unsigned int, int, CUser_object&, CReaderMessageHandler&);
    using FormatHandlers =  map<string, FormatHandler>;
    static FormatHandlers mFormatHandlers;

    size_t mColIndex;
    string mFormat;
    FormatHandler mHandler;
    string mName;
    string mDescription;

    static bool
    AddDouble(
        const string&, const string&, unsigned int, int, CUser_object&, CReaderMessageHandler&);
    static bool
    AddInt(
        const string&, const string&, unsigned int, int, CUser_object&, CReaderMessageHandler&);
    static bool
    AddIntArray(
        const string&, const string&, unsigned int, int, CUser_object&, CReaderMessageHandler&);
    static bool
    AddString(
        const string&, const string&, unsigned int, int, CUser_object&, CReaderMessageHandler&);
    static bool
    AddUint(
        const string&, const string&, unsigned int, int, CUser_object&, CReaderMessageHandler&);
    static bool
    AddUintArray(
        const string&, const string&, unsigned int, int, CUser_object&, CReaderMessageHandler&);

};

//  ============================================================================
class CAutoSqlCustomFields
//  ============================================================================
{
public:
    void
    Append(
        const CAutoSqlCustomField&);

    void
    Dump(
        ostream&);

    bool
    SetUserObject(
        const CBedColumnData&,
        int bedFlags,
        CSeq_feat&,
        CReaderMessageHandler&) const;

    bool Validate(
        CReaderMessageHandler&) const;

    size_t NumFields() const { return mFields.size(); };

private:
    vector<CAutoSqlCustomField> mFields;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _BED_AUTOSQL_CUSTOM_HPP_
