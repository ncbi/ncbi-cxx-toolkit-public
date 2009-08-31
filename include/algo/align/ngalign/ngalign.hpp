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
#include <hash_map.h>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>

#include <algo/blast/api/sseqloc.hpp>

#include <algo/align/ngalign/ngalign_interface.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CScope;
    class CSeq_align;
    class CSeq_align_set;
    class CSeq_id;
    class CDense_seg;
END_SCOPE(objects)

BEGIN_SCOPE(blast)
    class SSeqLoc;
    class CBlastOptionsHandle;
END_SCOPE(blast)




class CUberAlign
{
public:


    CUberAlign(objects::CScope& Scope)
            : m_Scope(&Scope) { ; }
    virtual ~CUberAlign() { ; }

    void SetQuery(CRef<ISequenceSet> Set) { m_Query = Set; }
    void SetSubject(CRef<ISequenceSet> Set) { m_Subject = Set; }

    void AddFilter(CRef<IAlignmentFilter> Filter) { m_Filters.push_back(Filter); }
    void AddAligner(CRef<IAlignmentFactory> Aligner) { m_Aligners.push_back(Aligner); }
    void AddScorer(CRef<IAlignmentScorer> Scorer) { m_Scorers.push_back(Scorer); }

    TAlignSetRef Align();

protected:

    virtual TAlignSetRef x_Align_Impl();


private:

    CRef<objects::CScope> m_Scope;

    CRef<ISequenceSet> m_Query, m_Subject;
    list<CRef<IAlignmentFilter> > m_Filters;
    list<CRef<IAlignmentFactory> > m_Aligners;
    list<CRef<IAlignmentScorer> > m_Scorers;



};



END_NCBI_SCOPE

#endif
