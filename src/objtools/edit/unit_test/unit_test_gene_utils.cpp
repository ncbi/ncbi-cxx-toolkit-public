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
* Author:  J. Chen, NCBI
*
* File Description:
*   unit tests file for the gene_utils.
*
* This file represents basic most common usage of Ncbi.Test framework (which
* is based on Boost.Test framework. For more advanced techniques look into
* another sample - unit_test_alt_sample.cpp.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqres/Int_graph.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objtools/unit_test_util/unit_test_util.hpp>
#include <objtools/edit/gene_utils.hpp>
#include "unit_test_gene_utils.hpp"


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */



USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(unit_test_util);

NCBITEST_AUTO_INIT()
{
    // Your application initialization code here (optional)
    cout << "Initialization function executed" << endl;
}


NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Describe command line parameters that we are going to use
    arg_desc->AddFlag
        ("enable_TestTimeout",
         "Run TestTimeout test, which is artificially disabled by default in"
         "order to avoid unwanted failure during the daily automated builds.");
}


NCBITEST_AUTO_FINI()
{
    // Your application finalization code here (optional)
    cout << "Finalization function executed" << endl;
}

BOOST_AUTO_TEST_CASE(Test_GetGeneForFeature)
{
   CRef <CSeq_entry> entry = BuildGoodNucProtSet();

   // add 1st gene
   CRef <CSeq_feat> feat = GetCDSFromGoodNucProtSet(entry);
   CRef <CSeq_feat> gene = MakeGeneForFeature(feat);
   gene->SetId().SetLocal().SetId(1);
   AddFeat(gene, entry);

   // 2nd cd and its xref gene
   feat.Reset(MakeCDSForGoodNucProtSet("nuc", "prot2"));
   feat->SetGeneXref();
   feat->SetXref().front()->SetId().SetLocal().SetId(1);
   feat->SetXref().front()->SetData().SetGene().SetLocus("gene locus");
   AddFeat(feat, entry);

   STANDARD_SETUP

   ITERATE (list <CRef <CSeq_feat> >, it, 
           entry->SetSet().SetAnnot().front()->SetData().SetFtable()) {
      CConstRef <CSeq_feat> gene4cds = edit::GetGeneForFeature(**it, scope);
      NCBITEST_CHECK_MESSAGE(gene == gene4cds, "Didn't get right gene");
   }
};
