#ifndef __NAMED_COLLECTION_SCORE__HPP
#define __NAMED_COLLECTION_SCORE__HPP


/* 
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
 * Author:  Alex Kotliarov
 *
 * File Description: named collection score abstract base class.
 */

#include <corelib/ncbiobj.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScoreValue
{
    CSeq_id_Handle m_Query;
    CSeq_id_Handle m_Subject;
    double m_Value;

    CScoreValue();
public:
    CScoreValue(CSeq_id_Handle query, CSeq_id_Handle subject, double value)
    : m_Query(query)
    , m_Subject(subject)
    , m_Value(value)
    {
    }

    CSeq_id_Handle const& GetQueryId() const { return m_Query; }
    CSeq_id_Handle const& GetSubjectId() const { return m_Subject; }
    double const& GetValue() const { return m_Value; }
};

// INamedAlignmentCollectionScore : abstract base class.
//  Defines interface for a concrete class that provides 
//  implementation for a score.
class INamedAlignmentCollectionScore : public CObject
{
public:
    virtual ~INamedAlignmentCollectionScore() {}
    virtual string GetName() const = 0;
    virtual vector<CScoreValue> Get(CScope&, CSeq_align_set const&) const = 0;
};

typedef CIRef<INamedAlignmentCollectionScore> (* TNamedScoreFactory)();

END_SCOPE(objects)
END_NCBI_SCOPE


#endif

