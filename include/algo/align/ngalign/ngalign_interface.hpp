#ifndef NGALIGN_INTERFACE__HPP
#define NGALIGN_INTERFACE__HPP

/*  $Id$
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
 * Authors:  Nathan Bouk
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqloc/Na_strand.hpp>

#include <algo/align/ngalign/result_set.hpp>


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CScope;
    class CSeq_align;
    class CSeq_align_set;
    class CSeq_id;
END_SCOPE(objects)

BEGIN_SCOPE(blast)
    class IQueryFactory;
    class CLocalDbAdapter;
    class CBlastOptionsHandle;
    class CSearchResultSet;
    class CSearchResults;
END_SCOPE(blast)

class CSeqMasker;


typedef CRef<objects::CSeq_align_set> TAlignSetRef;
typedef CRef<blast::CSearchResultSet> TResultsSetRef;
typedef CRef<CAlignResultsSet>  TAlignResultsRef;


class ISequenceSet : public CObject
{
public:
    virtual CRef<blast::IQueryFactory> CreateQueryFactory(
            objects::CScope& Scope, const blast::CBlastOptionsHandle& BlastOpts) = 0;
    virtual CRef<blast::IQueryFactory> CreateQueryFactory(
            objects::CScope& Scope, const blast::CBlastOptionsHandle& BlastOpts,
            const CAlignResultsSet& Alignments, int Threshold) = 0;
    virtual CRef<blast::CLocalDbAdapter> CreateLocalDbAdapter(
            objects::CScope& Scope, const blast::CBlastOptionsHandle& BlastOpts) = 0;
};



class IAlignmentFactory : public CObject
{
public:
    virtual string GetName() const = 0;
    virtual TAlignResultsRef GenerateAlignments(objects::CScope& Scope,
                                                ISequenceSet* Querys,
                                                ISequenceSet* Subjects,
                                                TAlignResultsRef AccumResults) = 0;
};


// Low Ranks are better.
class IAlignmentFilter : public CObject
{
public:
    virtual void FilterAlignments(TAlignResultsRef In,
                                  TAlignResultsRef Out) = 0;
    virtual unsigned int GetFilterRank() const = 0;

    static const string KFILTER_SCORE;
};



class IAlignmentScorer : public CObject
{
public:
    virtual void ScoreAlignments(TAlignResultsRef Alignments,
                                 objects::CScope& Scope) = 0;
};




END_NCBI_SCOPE

#endif
