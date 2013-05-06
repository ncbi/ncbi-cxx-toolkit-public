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
 * Author: Boris Kiryutin
 *
 * File Description:
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/test_boost.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
//#include <objmgr/seq_vector.hpp>
//#include <objmgr/feat_ci.hpp>
#include<objtools/data_loaders/genbank/gbloader.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

//#include <objects/seq/Bioseq.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

#include <algo/align/splign/splign.hpp>
#include <algo/align/splign/splign_formatter.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Here we make descriptions of command line parameters that we are
    // going to use.

    arg_desc->AddKey("data-in", "InputData",
                     "Concatenated Seq-aligns used to generate gene models",
                     CArgDescriptions::eInputFile);
}


BOOST_AUTO_TEST_CASE(TestUsingArg)
{

    CRef<CObjectManager> om = CObjectManager::GetInstance();
       CGBDataLoader::RegisterInObjectManager(*om,
                                              0,
                                              CObjectManager::eDefault
                                            );

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    CNcbiIstream& istr = args["data-in"].AsInputFile();

    auto_ptr<CObjectIStream> is(CObjectIStream::Open(eSerial_AsnText, istr));

    CRef<CSeq_annot> blast_align(new CSeq_annot());
    *is >> *blast_align;


    //set up splign

    CSplign splign;
    CRef<CSplicedAligner> aligner = CSplign::s_CreateDefaultAligner();//default is mrna
    aligner->SetSpaceLimit(1024);
    splign.SetAligner() = aligner;
    splign.SetScope().Reset(scope);
    splign.PreserveScope();
    splign.SetStartModelId(1);
    splign.SetAlignerScores();


    CConstRef<objects::CSeq_id> query_id;
    CConstRef<objects::CSeq_id> subj_id;

    CSplign::THitRefs hits_in;
    ITERATE(CSeq_align_set::Tdata, AlignIter, blast_align->GetData().GetAlign()) {
        CSplign::THitRef tr(new CBlastTabular(**AlignIter));
        hits_in.push_back(tr);
    }
    query_id = hits_in.front()->GetQueryId();
    subj_id = hits_in.front()->GetSubjId();

    splign.Run(&hits_in);

        CSplignFormatter sf (splign);
        sf.SetSeqIds(query_id, subj_id);
        CRef<CSeq_align_set> sas (sf.AsSeqAlignSet(NULL, CSplignFormatter::eAF_SplicedSegWithParts));

        cout<<sf. AsAlignmentText(scope, &splign.GetResult())<<endl;


        /*

    CSplign::SAlignedCompartment comp_out;
    CSplign::TResults splign_results;

    vector<CSplign::THitRefs> comp_in;

    
    if(!comp_in.empty()) {

        query_id = comp_in.front()->GetQueryId();
        subj_id = comp_in.front()->GetSubjId();

        m_Splign->AlignSingleCompartment(comp_in,
                                     comp_ranges[i].first,
                                     comp_ranges[i].second,
                                     &comp_out);

        if (this_comp.m_Status == CSplign::SAlignedCompartment::eStatus_Ok) {
            splign_results.push_back(comp_out);
        }
        
        CSplignFormatter sf (splign);
        sf.SetSeqIds(query_id, subj_id);
        //CRef<CSeq_align_set> sas (sf.AsSeqAlignSet(splign_results, CSplignFormatter::eAF_SplicedSegWithParts));
        CRef<CSeq_align_set> sas (sf.AsSeqAlignSet(NULL, CSplignFormatter::eAF_SplicedSegWithParts));

        cout<<sf. AsAlignmentText()<<endl;
        
    }

        */


    BOOST_CHECK_EQUAL( 1, 1 );
        
}


