#ifndef NGALIGN__HPP
#define NGALIGN__HPP

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

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>

#include <algo/align/ngalign/ngalign_interface.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CScope;
    class CSeq_align;
    class CSeq_align_set;
    class CSeq_id;
    class CDense_seg;
END_SCOPE(objects)



class CNgAligner
{
public:

    CNgAligner(objects::CScope& Scope);
    virtual ~CNgAligner();

    void SetQuery(ISequenceSet* Set);
    void SetSubject(ISequenceSet* Set);

    void AddFilter(IAlignmentFilter* Filter);
    void AddAligner(IAlignmentFactory* Aligner);
    void AddScorer(IAlignmentScorer* Scorer);

    TAlignSetRef Align();

protected:

    virtual TAlignSetRef x_Align_Impl();


private:

    CRef<objects::CScope> m_Scope;

    CIRef<ISequenceSet> m_Query;
    CIRef<ISequenceSet> m_Subject;

    typedef list<CIRef<IAlignmentFilter> > TFilters;
    typedef list<CIRef<IAlignmentFactory> > TFactories;
    typedef list<CIRef<IAlignmentScorer> > TScorers;
    TFilters m_Filters;
    TFactories m_Aligners;
    TScorers m_Scorers;

};


END_NCBI_SCOPE

#endif
