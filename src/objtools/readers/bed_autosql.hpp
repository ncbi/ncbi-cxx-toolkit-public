#ifndef _BED_AUTOSQL_HPP_
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
#include "bed_autosql_standard.hpp"
#include "bed_autosql_custom.hpp"
#include "bed_column_data.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

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
        const CBedColumnData&,
        CSeq_feat& feat,
        CReaderMessageHandler&) const;

    void
    Dump(
        ostream&);

protected:
    int mBedFlags;
    map<string, string> mParameters;
    CAutoSqlStandardFields mWellKnownFields;
    CAutoSqlCustomFields mCustomFields;
    size_t mColumnCount;
    
    string
    xReadLine(
        CNcbiIstream&);

    static void
    mParseString(
        const string&,
        CUser_field&);

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
