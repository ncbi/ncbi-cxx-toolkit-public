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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Alignment merger
*
* ===========================================================================
*/

#include <objects/alnmgr/alnmix.hpp>

// Object Manager includes
#include <objects/objmgr/gbloader.hpp>
#include <objects/objmgr/object_manager.hpp>
#include <objects/objmgr/reader_id1.hpp>
#include <objects/objmgr/scope.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

CAlnMix::CAlnMix(void)
{
    x_CreateScope();
}


CAlnMix::CAlnMix(CScope& scope)
    : m_Scope(&scope)
{
}


CAlnMix::CAlnMix(CConstRef<CSeq_align> aln)
{
    x_CreateScope();
    Add(aln);
}


CAlnMix::CAlnMix(const TAlns& alns)
{
    x_CreateScope();
    Add(alns);
}


CAlnMix::CAlnMix(const TAlns& alns, CScope& scope)
    : m_Scope(&scope)
{
    Add(alns);
}


CAlnMix::CAlnMix(CConstRef<CSeq_align> aln, CScope& scope)
    : m_Scope(&scope)
{
    Add(aln);
}


CAlnMix::~CAlnMix(void)
{
}


void CAlnMix::x_CreateScope() {
    m_ObjMgr = new CObjectManager;
    
    m_ObjMgr->RegisterDataLoader(*new CGBDataLoader("ID", new CId1Reader, 2),
                                 CObjectManager::eDefault);
    
    m_Scope = new CScope(*m_ObjMgr);
    m_Scope->AddDefaults();
}


#if 0
void CAlnMix::x_DecomposeToPairwiseAndAdd(const CDense_seg& ds)
{
    int dim   = ds.GetDim();
    int nsegs = ds.GetNumseg();
    for (int row = 1;  row < dim;  row++) {
        CRef<CDense_seg> p = new CDense_seg;
        p->SetDim(2);
        p->SetNumseg(nsegs);

        //reserve space
        p->SetLens().reserve(nsegs);
        p->SetStarts().reserve(dim*nsegs);
        p->SetStrands().reserve(dim*nsegs);

        //fill the vectors
        p->SetIds().push_back(ds.GetIds()[0]);
        p->SetIds().push_back(ds.GetIds()[row]);
        for (int seg = 0;  seg < nsegs;  seg++) {
            p->SetLens().push_back(ds.GetLens()[seg]);
            p->SetStarts().push_back(ds.GetStarts()[dim*seg]);
            p->SetStarts().push_back(ds.GetStarts()[dim*seg+row]);
            p->SetStrands().push_back(ds.GetStrands()[dim*seg]);
            p->SetStrands().push_back(ds.GetStrands()[dim*seg+row]);
        }
        m_Pairs.push_back(new CAlnVec(*p, *m_Scope));
    }
}


void CAlnMix::x_ConvertToDenseSeg(const CSeq_align& aln)
{
    if (aln.GetSegs().IsDendiag()) {
        x_ConvertDendiagToDensegChain(aln);
    } else if (aln.GetSegs().IsDenseg()) {
        CDense_seg& ds = aln.GetSegs().GetDenseg();
        if ( !ds.IsSetStrands() ) {
            /* set them to plus */
        }
    }
}


void CAlnMix::x_ConvertDendiagToDenseg(const CSeq_align& aln)
{
 
}
#endif

END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2002/08/23 14:43:52  ucko
* Add the new C++ alignment manager to the public tree (thanks, Kamen!)
*
*
* ===========================================================================
*/
