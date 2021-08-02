#ifndef _BED_AUTOSQL_STANDARD_HPP_
#define _BED_AUTOSQL_STANDARD_HPP_

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
#include <objects/seqfeat/Seq_feat.hpp>
#include "reader_message_handler.hpp"
#include "bed_column_data.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ============================================================================
struct CAutoSqlStandardFields
//  ============================================================================
{
public:
    CAutoSqlStandardFields();

    bool
    SetLocation(
        const CBedColumnData&,
        int bedFlags,
        CSeq_feat&,
        CReaderMessageHandler&) const;

    bool
    SetTitle(
        const CBedColumnData&,
        int bedFlags,
        CSeq_feat&,
        CReaderMessageHandler&) const;

    bool
    ProcessTableRow(
        size_t,
        const string&,
        const string&);

    bool Validate(
        CReaderMessageHandler&) const;

    size_t
    NumFields() const { return mNumFields; };

private:
    size_t mColChrom;
    size_t mColSeqStart;
    size_t mColSeqStop;
    size_t mColStrand;
    size_t mColName;
    size_t mColScore;
    size_t mNumFields;

    bool ContainsInfo() const
    {
        return (mColChrom + mColSeqStart + mColSeqStop + mColStrand +
            mColName + mColScore != -6);
    }
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _BED_AUTOSQL_STANDARD_HPP_
