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
#include <objmgr/gbloader.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <algo/blast/api/sseqloc.hpp>

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

/*
* ===========================================================================
*
* $Log: test_objmgr.cpp,v $
* Revision 1.13  2006/02/22 19:53:44  bealer
* - Unit testing for CBlastQueryVector.
*
* Revision 1.12  2005/11/10 14:47:07  madden
* Add CreateEmptySSeqLoc method
*
* Revision 1.11  2005/09/28 18:26:22  camacho
* Rearrangement of headers/functions to segregate object manager dependencies.
*
* Revision 1.10  2005/09/23 18:55:53  camacho
* + overloaded CTestObjMgr::CreateSSeqLoc
*
* Revision 1.9  2004/07/22 13:46:43  madden
* ObjectManager interface change
*
* Revision 1.8  2004/05/05 14:40:59  dondosha
* Added method CreateWholeSSeqLoc
*
* Revision 1.7  2004/03/23 17:55:26  camacho
* Fix destructor
*
* Revision 1.6  2004/03/23 16:10:02  camacho
* Minor changes
*
* Revision 1.5  2004/03/19 16:57:41  camacho
* Use CRefs when allocating CScopes
*
* Revision 1.4  2003/12/05 20:18:09  camacho
* Fix error object manager error message
*
* Revision 1.3  2003/12/05 17:22:15  camacho
* Correction to singleton pattern
*
* Revision 1.2  2003/10/31 22:18:28  camacho
* Minor updates
*
* Revision 1.1  2003/10/18 01:05:28  camacho
* Initial revision
*
*
* ===========================================================================
*/
