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
* Author:  Justin Foley, NCBI
*
* File Description:
*   Sample unit tests file for cleanup.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbistd.hpp>

#include <corelib/test_boost.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/general/Object_id.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Gb_qual.hpp>

#include "../newcleanupp.hpp"
#include "../gbqual_cleanup.hpp"


USING_NCBI_SCOPE;
USING_SCOPE(objects);



BOOST_AUTO_TEST_CASE(Test_SatelliteQualifier) 
{
    auto feat = Ref(new CSeq_feat());
    auto& rna = feat->SetData().SetRna();
    rna.SetType(CRNA_ref::eType_rRNA);


    auto& int_loc = feat->SetLocation().SetInt();
    int_loc.SetId().SetLocal().SetStr("dummy_id");
    int_loc.SetFrom(0);
    int_loc.SetTo(19);

    feat->AddQualifier("satellite", "satellite  value");

    auto changes = Ref(new CCleanupChange());
    auto pScope = Ref(new CScope(*CObjectManager::GetInstance()));
    CNewCleanup_imp cleanup_imp(changes, *pScope);

    CGBQualCleanup gbqual_cleanup(*pScope, cleanup_imp);
    
    auto& satellite_qual = feat->SetQual().front();

    gbqual_cleanup.Run(*satellite_qual, *feat);
    BOOST_CHECK_EQUAL(satellite_qual->GetVal(), "satellite:value");
}



