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
* Author:  Justin Foley
*
* File Description:
*   Unit tests for code processing secondary accessions
*
*/

#include <ncbi_pch.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/flatfile/flatfile_parse_info.hpp>
#include "../index.h"
#include <objtools/flatfile/flatdefn.h>
#include "../indx_blk.h"
#include "../add.h"
#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;
USING_SCOPE(objects);


static CRef<CSeq_entry> s_BuildRawBioseq(const list<CRef<CSeq_id>>& ids)
{
    auto pEntry = Ref(new CSeq_entry());
    pEntry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_dna);
    pEntry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_raw);
    string seqdata { "AATTGGGGCCAAAATTGGCCAAATTGGCCATGC" };
    pEntry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set(seqdata);
    pEntry->SetSeq().SetInst().SetLength(seqdata.size());
 
    for (auto pId : ids) {
        pEntry->SetSeq().SetId().push_back(pId);
    }   
    return pEntry;
}



static CRef<CDelta_seq> s_BuildDeltaSeq(const CSeq_id& id)
{
    auto pDeltaSeq = Ref(new CDelta_seq());
    pDeltaSeq->SetLoc().SetWhole().Assign(id);
    return pDeltaSeq;
}



BOOST_AUTO_TEST_CASE(CheckDoesNotReferencePrimary) 
{
    auto pPrimaryAcc = Ref(new CSeq_id("AF123456.1"));
    auto pPrimaryGi = Ref(new CSeq_id());
    pPrimaryGi->SetGi(GI_CONST(1234));

    list<CRef<CSeq_id>> ids;
    ids.push_back(pPrimaryAcc);
    ids.push_back(pPrimaryGi);
    auto pEntry = s_BuildRawBioseq(ids);

    auto pScope = Ref(new CScope(*CObjectManager::GetInstance()));
    pScope->AddTopLevelSeqEntry(*pEntry);

    auto giHandle = CSeq_id_Handle::GetHandle(pPrimaryGi->GetGi());
    auto accHandle = pScope->GetAccVer(giHandle);

    auto pOtherAcc = Ref(new CSeq_id("AF123456.2"));
    auto pDeltaSeq = s_BuildDeltaSeq(*pOtherAcc);
    auto pDeltaExt = Ref(new CDelta_ext());
    pDeltaExt->Set().push_back(pDeltaSeq);

    BOOST_CHECK(!g_DoesNotReferencePrimary(*pDeltaExt, *pPrimaryAcc, *pScope));
    BOOST_CHECK(!g_DoesNotReferencePrimary(*pDeltaExt, *pPrimaryGi, *pScope));

    pDeltaSeq = s_BuildDeltaSeq(*pPrimaryGi);
    pDeltaExt->Set().clear();
    pDeltaExt->Set().push_back(pDeltaSeq);
    BOOST_CHECK(!g_DoesNotReferencePrimary(*pDeltaExt, *pPrimaryAcc, *pScope));
    BOOST_CHECK(!g_DoesNotReferencePrimary(*pDeltaExt, *pPrimaryGi, *pScope));
    
    BOOST_CHECK(!g_DoesNotReferencePrimary(*pDeltaExt, *pOtherAcc, *pScope));

    auto pOtherGi = Ref(new CSeq_id());
    pOtherGi->SetGi(GI_CONST(5678));
    BOOST_CHECK(g_DoesNotReferencePrimary(*pDeltaExt, *pOtherGi, *pScope));
}


BOOST_AUTO_TEST_CASE(IdentifyWGSAccessions)
{
    BOOST_CHECK_EQUAL(fta_if_wgs_acc("BACF01000000"), 0); // WGS project 
    
    BOOST_CHECK_EQUAL(fta_if_wgs_acc("BBXK01007945"), 1); // WGS contig
    
    BOOST_CHECK_EQUAL(fta_if_wgs_acc("DF977004"), 2); // WGS scaffold

    BOOST_CHECK_EQUAL(fta_if_wgs_acc("BACF00000000"), 3); // Non-project WGS master

    BOOST_CHECK_EQUAL(fta_if_wgs_acc("ABCD23S000001"), 7); // 4+2+S+6 VDB scaffold
    
    BOOST_CHECK_EQUAL(fta_if_wgs_acc("ABCD23S0000001"), 7); // 4+2+S+7 VDB scaffold
    
    BOOST_CHECK_EQUAL(fta_if_wgs_acc("ABCD23S00000001"), 7); // 4+2+S+8 VDB scaffold
    
    BOOST_CHECK_EQUAL(fta_if_wgs_acc("ABCD23S00000000"), -1); // Not a valid VDB scaffold
   
    BOOST_CHECK_EQUAL(fta_if_wgs_acc("ABCD23S000000001"), -1); // Invalid

    BOOST_CHECK_EQUAL(fta_if_wgs_acc("JAAEAK010000000"), 0); // WGS project

    BOOST_CHECK_EQUAL(fta_if_wgs_acc("JAAEAK010000001"), 1); // WGS contig

    BOOST_CHECK_EQUAL(fta_if_wgs_acc("GACF01000000"), 4); // TSA WGS project 

    BOOST_CHECK_EQUAL(fta_if_wgs_acc("GBXK01007945"), 5); // TSA WGS contig
    
    BOOST_CHECK_EQUAL(fta_if_wgs_acc("GACF00000000"), 6); // TSA WGS master

    BOOST_CHECK_EQUAL(fta_if_wgs_acc("IBXK01007945"), 8); // TSA WGS DDBJ contig

    BOOST_CHECK_EQUAL(fta_if_wgs_acc("HBXK01007945"), 9); // TSA WGS EMBL contig
    
    BOOST_CHECK_EQUAL(fta_if_wgs_acc("KACF01000000"), 10); // TLS WGS project
    
    BOOST_CHECK_EQUAL(fta_if_wgs_acc("ATCF01000000"), 10); // TLS WGS project
    
    BOOST_CHECK_EQUAL(fta_if_wgs_acc("KACF01000001"), 11); // TLS WGS contig
    
    BOOST_CHECK_EQUAL(fta_if_wgs_acc("ATCF01000001"), 11); // TLS WGS contig
    
    BOOST_CHECK_EQUAL(fta_if_wgs_acc("KACF00000000"), 12); // TLS WGS master
    
    BOOST_CHECK_EQUAL(fta_if_wgs_acc("ATCF00000000"), 12); // TLS WGS master
}

