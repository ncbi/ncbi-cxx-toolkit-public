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

    /*MRNA*/
    arg_desc->AddOptionalKey("mrna-data-in", "mrna_data_in",
                     "Concatenated CSeq_annots (blast results) used to run splign with mrna parameter set",
                            CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);
   
    arg_desc->AddOptionalKey("mrna-expected", "mrna_data_out",
                     "Expected output for mrna parameter set (concatenatd CSeq_align_sets).",
                            CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);
 
    arg_desc->SetDependency("mrna-expected", CArgDescriptions::eRequires, "mrna-data-in");

    arg_desc->AddOptionalKey("mrna-out", "mrna_out", "output alignment file",
                             CArgDescriptions::eOutputFile, CArgDescriptions::fPreOpen);

    arg_desc->SetDependency("mrna-out", CArgDescriptions::eRequires, "mrna-data-in");


    arg_desc->AddDefaultKey("mrna-outfmt", "mrna_output_format", "output format for file specified with -mrna-out", CArgDescriptions::eString, "splign");

    arg_desc->SetDependency("mrna-outfmt", CArgDescriptions::eRequires, "mrna-out");

    CArgAllow_Strings * cons = new CArgAllow_Strings();
    cons->Allow("splign")->Allow("asn")->Allow("text");
    arg_desc->SetConstraint("mrna-outfmt", cons);

    /*EST*/

    arg_desc->AddOptionalKey("est-data-in", "est_data_in",
                     "Concatenated CSeq_annots (blast results) used to run splign with est parameter set",
                            CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);
   
    arg_desc->AddOptionalKey("est-expected", "est_data_out",
                     "Expected output for est parameter set (concatenatd CSeq_align_sets).",
                            CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);
 
    arg_desc->SetDependency("est-expected", CArgDescriptions::eRequires, "est-data-in");

    arg_desc->AddOptionalKey("est-out", "est_out", "output alignment file",
                             CArgDescriptions::eOutputFile, CArgDescriptions::fPreOpen);

    arg_desc->SetDependency("est-out", CArgDescriptions::eRequires, "est-data-in");


    arg_desc->AddDefaultKey("est-outfmt", "est_output_format", "output format for file specified with -est-out", CArgDescriptions::eString, "splign");

    arg_desc->SetDependency("est-outfmt", CArgDescriptions::eRequires, "est-out");

    arg_desc->SetConstraint("est-outfmt", cons);

}


BOOST_AUTO_TEST_CASE(TestUsingArg)
{

    const CArgs& args = CNcbiApplication::Instance()->GetArgs();

    if(args["mrna-data-in"] || args["est-data-in"]) {
        
        CRef<CObjectManager> om = CObjectManager::GetInstance();
        CGBDataLoader::RegisterInObjectManager(*om, 0,  CObjectManager::eDefault);
        CRef<CScope> scope(new CScope(*om));
        scope->AddDefaults();
        
        //set up splign 
        
        CSplign splign;
        CRef<CSplicedAligner> aligner = CSplign::s_CreateDefaultAligner();//default is mrna 
        aligner->SetSpaceLimit(2 * 1024 * 1024);
        splign.SetAligner() = aligner;
        splign.SetScope().Reset(scope);
        splign.PreserveScope();
        
        CSplignFormatter sf (splign);
        
        /*MRNA*/
        
        if(args["mrna-data-in"]) {
            splign.SetStartModelId(1);
            splign.SetAlignerScores();
            
            //READ BLAST HITS   
            
            CNcbiIstream& istr = args["mrna-data-in"].AsInputFile();        
            auto_ptr<CObjectIStream> is (CObjectIStream::Open(eSerial_AsnText, istr));                
            CRef<CSeq_annot> blast_align(new CSeq_annot);
            while( istr ) {             
                try {
                    *is >> *blast_align;
                }
                catch (CEofException&) {
                    break;
                }
                
                
                CSplign::THitRefs hits_in;
                ITERATE(CSeq_align_set::Tdata, AlignIter, blast_align->GetData().GetAlign()) {
                    CSplign::THitRef tr(new CBlastTabular(**AlignIter));
                    hits_in.push_back(tr);
                }
                
                CConstRef<objects::CSeq_id> query_id;       
                CConstRef<objects::CSeq_id> subj_id;
                query_id = hits_in.front()->GetQueryId();
                subj_id = hits_in.front()->GetSubjId();
                sf.SetSeqIds(query_id, subj_id);
                
                splign.Run(&hits_in);//note that this call spoils hits_in data      
                
                //OUTPUT ALIGNMENTS 
                
                if(args["mrna-out"]) {//output alignments   
                    
                    CNcbiOstream& ostr = args["mrna-out"].AsOutputFile();                                       
                    string ofmt = args["mrna-outfmt"].AsString();
                    
                    if(ofmt == "asn") {
                        auto_ptr<CObjectIStream> is (CObjectIStream::Open(eSerial_AsnText, istr));
                        CRef<CSeq_align_set> sas (sf.AsSeqAlignSet(&splign.GetResult(), CSplignFormatter::eAF_SplicedSegWithParts));
                        ostr << MSerial_AsnText << *sas;
                        
                    } else if(ofmt == "text") {
                        ostr<<sf. AsAlignmentText(scope, &splign.GetResult())<<endl;
                        
                    } else {// splign format    
                        ostr<<sf.AsExonTable(&splign.GetResult())<<endl;;
                    }
                }
                
                //CHECK IF SPLIGN RESULT IS THE SAME AS EXPECTED    
                if(args["mrna-expected"]) {
                    CNcbiIstream& estr = args["mrna-expected"].AsInputFile();        
                    auto_ptr<CObjectIStream> es (CObjectIStream::Open(eSerial_AsnText, estr));                
                    CRef<CSeq_align_set> expected_out(new CSeq_align_set);
                    *es >> *expected_out;
                    
                    CRef<CSeq_align_set> sas (sf.AsSeqAlignSet(&splign.GetResult(), CSplignFormatter::eAF_SplicedSegWithParts));


                    //BOOST_CHECK ( sas->Equals(*expected_out) ); 
                    // check above fails on real number comparison, workaround:

                    CNcbiOstrstream s1;
                    s1 << MSerial_AsnText << *sas;
                    string mrna1 = CNcbiOstrstreamToString(s1);
                    
                    CNcbiOstrstream s2;
                    s2 << MSerial_AsnText << *expected_out;
                    string mrna2 = CNcbiOstrstreamToString(s2);
                    
                    BOOST_CHECK ( mrna1 == mrna2 );
                    
                }                                
            }
        }


        /*EST*/
        
        if(args["est-data-in"]) {
            splign.SetStartModelId(1);
            splign.SetScoringType(CSplign::eEstScoring);
            splign.SetAlignerScores();
            
            //READ BLAST HITS   
            
            CNcbiIstream& istr = args["est-data-in"].AsInputFile();        
            auto_ptr<CObjectIStream> is (CObjectIStream::Open(eSerial_AsnText, istr));                
            CRef<CSeq_annot> blast_align(new CSeq_annot);
            while( istr ) {             
                try {
                    *is >> *blast_align;
                }
                catch (CEofException&) {
                    break;
                }
                
                
                CSplign::THitRefs hits_in;
                ITERATE(CSeq_align_set::Tdata, AlignIter, blast_align->GetData().GetAlign()) {
                    CSplign::THitRef tr(new CBlastTabular(**AlignIter));
                    hits_in.push_back(tr);
                }
                
                CConstRef<objects::CSeq_id> query_id;       
                CConstRef<objects::CSeq_id> subj_id;
                query_id = hits_in.front()->GetQueryId();
                subj_id = hits_in.front()->GetSubjId();
                sf.SetSeqIds(query_id, subj_id);
                
                splign.Run(&hits_in);//note that this call spoils hits_in data      
                
                //OUTPUT ALIGNMENTS 
                
                if(args["est-out"]) {//output alignments   
                    
                    CNcbiOstream& ostr = args["est-out"].AsOutputFile();                                       
                    string ofmt = args["est-outfmt"].AsString();
                    
                    if(ofmt == "asn") {
                        auto_ptr<CObjectIStream> is (CObjectIStream::Open(eSerial_AsnText, istr));
                        CRef<CSeq_align_set> sas (sf.AsSeqAlignSet(&splign.GetResult(), CSplignFormatter::eAF_SplicedSegWithParts));
                        ostr << MSerial_AsnText << *sas;
                        
                    } else if(ofmt == "text") {
                        ostr<<sf. AsAlignmentText(scope, &splign.GetResult())<<endl;
                        
                    } else {// splign format    
                        ostr<<sf.AsExonTable(&splign.GetResult())<<endl;;
                    }
                }
                
                //CHECK IF SPLIGN RESULT IS THE SAME AS EXPECTED    
                if(args["est-expected"]) {
                    CNcbiIstream& estr = args["est-expected"].AsInputFile();        
                    auto_ptr<CObjectIStream> es (CObjectIStream::Open(eSerial_AsnText, estr));                
                    CRef<CSeq_align_set> expected_out(new CSeq_align_set);
                    *es >> *expected_out;
                    
                    CRef<CSeq_align_set> sas (sf.AsSeqAlignSet(&splign.GetResult(), CSplignFormatter::eAF_SplicedSegWithParts));
                    

                    //BOOST_CHECK ( sas->Equals(*expected_out) ); 
                    // check above fails on real number comparison, workaround:

                    CNcbiOstrstream s1;
                    s1 << MSerial_AsnText << *sas;
                    string est1 = CNcbiOstrstreamToString(s1);
                    
                    CNcbiOstrstream s2;
                    s2 << MSerial_AsnText << *expected_out;
                    string est2 = CNcbiOstrstreamToString(s2);
                    
                    BOOST_CHECK ( est1 == est2 );
                    
                }  
            }
        }
        /*END of EST*/
    }
}                              








