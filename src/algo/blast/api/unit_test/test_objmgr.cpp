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
* Author:  Christiam Camacho
*
* File Description:
*   Singleton class to facilitate the creation of SSeqLocs 
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include "test_objmgr.hpp"
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <algo/blast/api/sseqloc.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

USING_SCOPE(objects);
USING_SCOPE(blast);

CTestObjMgr*         CTestObjMgr::m_Instance = NULL;
CRef<CObjectManager> CTestObjMgr::m_ObjMgr;

CTestObjMgr::CTestObjMgr()
{

    m_ObjMgr = CObjectManager::GetInstance();
    if (!m_ObjMgr) {
         throw std::runtime_error("Could not initialize object manager");
    }
    CGBDataLoader::RegisterInObjectManager(*m_ObjMgr);
}

CTestObjMgr::~CTestObjMgr()
{
    m_ObjMgr.Reset(NULL);   // all scopes should be gone by now
}

CTestObjMgr&
CTestObjMgr::Instance() 
{
    if (m_Instance == NULL) {
        m_Instance = new CTestObjMgr();
    }
    return *m_Instance;
}

CObjectManager&
CTestObjMgr::GetObjMgr() const
{
    return *m_ObjMgr;
}

SSeqLoc*
CTestObjMgr::CreateSSeqLoc(CSeq_id& id, ENa_strand strand)
{
    CRef<CSeq_loc> seqloc(new CSeq_loc());
    CRef<CScope> scope(new CScope(GetObjMgr()));
    scope->AddDefaults();

    seqloc->SetInt().SetFrom(0);
    seqloc->SetInt().SetTo(sequence::GetLength(id, scope)-1);
    seqloc->SetInt().SetStrand(strand);
    seqloc->SetInt().SetId().Assign(id);

    return new SSeqLoc(seqloc, scope);
}

SSeqLoc*
CTestObjMgr::CreateSSeqLoc(CSeq_id& id, 
                           pair<TSeqPos, TSeqPos> range,
                           ENa_strand strand)
{
    CRef<CSeq_loc> seqloc(new CSeq_loc());
    CRef<CScope> scope(new CScope(GetObjMgr()));
    scope->AddDefaults();

    seqloc->SetInt().SetFrom(range.first);
    seqloc->SetInt().SetTo(range.second);
    seqloc->SetInt().SetStrand(strand);
    seqloc->SetInt().SetId().Assign(id);

    return new SSeqLoc(seqloc, scope);
}

SSeqLoc*
CTestObjMgr::CreateSSeqLoc(CSeq_id& id, 
                           TSeqRange const & sr,
                           ENa_strand strand)
{
    return CreateSSeqLoc(id, make_pair(sr.GetFrom(), sr.GetTo()), strand);
}

SSeqLoc* 
CTestObjMgr::CreateWholeSSeqLoc(CSeq_id& id)
{
    CRef<CSeq_loc> seqloc(new CSeq_loc());
    CRef<CScope> scope(new CScope(GetObjMgr()));
    scope->AddDefaults();

    seqloc->SetWhole(id);

    return new SSeqLoc(seqloc, scope);
}

SSeqLoc* 
CTestObjMgr::CreateEmptySSeqLoc(CSeq_id& id)
{
    CRef<CSeq_loc> seqloc(new CSeq_loc());
    CRef<CScope> scope(new CScope(GetObjMgr()));
    scope->AddDefaults();

    seqloc->SetEmpty(id);

    return new SSeqLoc(seqloc, scope);
}

CRef<ncbi::blast::CBlastSearchQuery>
CTestObjMgr::CreateBlastSearchQuery(CSeq_id& id, ENa_strand strand)
{
    CRef<CSeq_loc> seqloc(new CSeq_loc());
    CRef<CScope> scope(new CScope(GetObjMgr()));
    scope->AddDefaults();
    
    seqloc->SetInt().SetFrom(0);
    seqloc->SetInt().SetTo(sequence::GetLength(id, scope)-1);
    seqloc->SetInt().SetStrand(strand);
    seqloc->SetInt().SetId().Assign(id);
    
    TMaskedQueryRegions mqr;
    
    CRef<CBlastSearchQuery>
        bsq(new CBlastSearchQuery(*seqloc, *scope, mqr));
    
    return bsq;
}

#endif /* SKIP_DOXYGEN_PROCESSING */
