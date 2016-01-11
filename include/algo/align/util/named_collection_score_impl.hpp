#ifndef __NAMED_ALIGNMENT_COLLECTION_SCORE_IMPL__HPP
#define __NAMED_ALIGNMENT_COLLECTION_SCORE_IMPL__HPP


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
 * File Description:
 */

#include <corelib/ncbiobj.hpp>

#include <objmgr/scope.hpp>

#include <objects/seq/seq_id_handle.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

#include <algo/align/util/named_collection_score.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScoreSeqCoverage : public INamedAlignmentCollectionScore
{
    CScoreSeqCoverage() {}
    static pair<double, bool> MakeScore(CBioseq_Handle const& query_handle, vector<CSeq_align const*>::const_iterator, vector<CSeq_align const*>::const_iterator);
public:
    string GetName() const;
    vector<CScoreValue> Get(CScope& scope, CSeq_align_set const& coll) const;
    
    static const char* Name;    
    static CIRef<INamedAlignmentCollectionScore> Create();
};

class CScoreUniqSeqCoverage : public INamedAlignmentCollectionScore
{
    CScoreUniqSeqCoverage() {}
    static pair<double, bool> MakeScore(CBioseq_Handle const& query_handle, vector<CSeq_align const*>::const_iterator, vector<CSeq_align const*>::const_iterator);
public:
    string GetName() const;   
    vector<CScoreValue> Get(CScope& scope, CSeq_align_set const& coll) const;

    static const char* Name;    
    static CIRef<INamedAlignmentCollectionScore> Create();
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // __NAMED_ALIGNMENT_COLLECTION_SCORE_IMPL__HPP
