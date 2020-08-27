#ifndef _BED_SUTOSQL_HPP_
#define _BED_AUTOSQL_HPP_

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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ============================================================================
struct CAutoSqlKnownFields
//  ============================================================================
{
    size_t mColChrom;
    size_t mColSeqStart;
    size_t mColSeqStop;
    size_t mColStrand;
    size_t mColName;
    size_t mColScore;

    CAutoSqlKnownFields():
        mColChrom(-1), mColSeqStart(-1), mColSeqStop(-1), mColStrand(-1),
        mColName(-1), mColScore(-1)
    {};

    bool ContainsInfo() const 
    {
        return (mColChrom + mColSeqStart + mColSeqStop + mColStrand + 
            mColName + mColScore != -6);
    }

    bool Validate() const
    {
        return (mColChrom != -1  &&  mColSeqStart != -1  &&  mColSeqStop != -1);
    }

    bool
    SetLocation(
        const vector<string>& fields,
        int bedFlags,
        CSeq_feat&) const;

    bool
    SetTitle(
        const vector<string>& fields,
        int bedFlags,
        CSeq_feat&) const;
};

//  ============================================================================
struct SAutoSqlColumnInfo
//  ============================================================================
{
    size_t mColIndex;
    string mFormat;
    string mName;
    string mDescription;

    SAutoSqlColumnInfo(
            size_t colIndex, string format, string name, string description);

    bool
    SetUserField(
        const vector<string>& fields,
        int bedFlags,
        CUser_object&) const;

    using FormatHandler = bool (*)(const string&, const string&, int, CUser_object&);
    using FormatHandlers =  map<string, FormatHandler>;
    static FormatHandlers mFormatHandlers;

    static bool
    AddInt(const string&, const string&, int, CUser_object&);
    static bool
    AddIntArray(const string&, const string&, int, CUser_object&);
    static bool
    AddString(const string&, const string&, int, CUser_object&);
    static bool
    AddUint(const string&, const string&, int, CUser_object&);
};

//  ============================================================================
class CAutoSqlCustomFields
//  ============================================================================
{
public:
    void
    Append(
        const SAutoSqlColumnInfo&);

    void
    Dump(
        ostream&);

    bool
    SetUserObject(
        const vector<string>& fields,
        int bedFlags,
        CSeq_feat&) const;

private:
    vector<SAutoSqlColumnInfo> mFields;
};

//  ============================================================================
class CBedAutoSql
//  ============================================================================
{
public:
    using ValueParser = void (*)(const string&, CUser_object&);

public:
    CBedAutoSql(int);
    ~CBedAutoSql();

    bool Load(
        CNcbiIstream&,
        CReaderMessageHandler&); 

    bool Validate(
        CReaderMessageHandler&) const;

    size_t
    ColumnCount() const;

    bool
    ReadSeqFeat(
        const vector<string>& fields,
        CSeq_feat& feat);

    void
    Dump(
        ostream&);

protected:
    int mBedFlags;
    map<string, string> mParameters;
    CAutoSqlKnownFields mWellKnownFields;
    CAutoSqlCustomFields mCustomFields;
    size_t mColumnCount;
    
    string
    xReadLine(
        CNcbiIstream&);

    static void
    mParseString(
        const string&,
        CUser_field&);

    bool
    xStoreAsLocationInfo(
        size_t colIndex,
        const string&,
        const string&);

    static bool
    xParseAutoSqlColumnDef(
        const string&,
        string&,
        string&,
        string&);
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _BED_AUTOSQL_HPP_
