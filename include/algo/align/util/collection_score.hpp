#ifndef ALIGNMENT_COLLECTION_SCORE__HPP
#define ALIGNMENT_COLLECTION_SCORE__HPP
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
 *  - API for scores defined on a collection of alignments. 
 */

#include <corelib/ncbiobj.hpp>
#include <algo/align/util/named_collection_score.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class NCBI_XALGOALIGN_EXPORT IAlignmentCollectionScore : public CObject
{
public:
    virtual ~IAlignmentCollectionScore() {}
    virtual vector<CScoreValue> Get(string const& name, CSeq_align_set const& ) const = 0;
    virtual void Set(string const & name, CSeq_align_set& ) const = 0;

    virtual vector<CScoreValue> Get(string const& group_name, vector<string> const& scores, CSeq_align_set const& ) const = 0;
    virtual void Set(string const& group_name, vector<string> const& scores, CSeq_align_set& ) const = 0;



    static CRef<IAlignmentCollectionScore> GetInstance(objects::CScope&);
    static CRef<IAlignmentCollectionScore> GetInstance();
};

END_SCOPE(objects)

END_NCBI_SCOPE

#endif // ALIGNMENT_COLLECTION_SCORE__HPP




