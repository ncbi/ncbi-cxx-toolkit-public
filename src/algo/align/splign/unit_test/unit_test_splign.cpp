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
//#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/seqalign__.hpp>

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


void s_RoundScores(CRef<CSeq_align_set> align) {
    if(align->IsSet()) {
                    NON_CONST_ITERATE(CSeq_align_set::Tdata, sait, align->Set()) {
                        BOOST_CHECK( (*sait)->GetSegs().IsSpliced() );
                        if( (*sait)->SetSegs().SetSpliced().IsSetExons() ) {
                            NON_CONST_ITERATE(CSpliced_seg::TExons, exot, (*sait)->SetSegs().SetSpliced().SetExons()) {
                                if( (*exot)->IsSetScores() && (*exot)->SetScores().IsSet() ) {
                                    NON_CONST_ITERATE(	CScore_set::Tdata, sit, (*exot)->SetScores().Set() ) {
                                        if( (*sit)->IsSetValue() && (*sit)->SetValue().IsReal() ) {
                                            CScore_Base::C_Value::TReal re = (*sit)->SetValue().SetReal();
                                            CNcbiOstrstream ots1;		
                                            ots1 << setprecision(7) << scientific << re;
                                            string tstr1 = CNcbiOstrstreamToString(ots1);
                                            CNcbiIstrstream its1(tstr1); 
                                            its1 >> re;
                                            (*sit)->SetValue().SetReal(re);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
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
        splign.SetAligner() = aligner;
        splign.SetScope().Reset(scope);
        splign.PreserveScope();

        aligner->SetSpaceLimit(numeric_limits<Uint4>::max());//That's what you get when 4096 is passed as "max_space" parameter as of today.
        
        splign.SetMinExonIdentity(0.75);
        splign.SetMinCompartmentIdentity(0.5);
        splign.SetCompartmentPenalty(0.55);
        splign.SetMinSingletonIdentity(0.2);
        splign.SetMinSingletonIdentityBps(250);
        splign.SetMaxIntron(1200000);
        splign.SetPolyaExtIdentity(1.0);
        splign.SetMinPolyaLen(1);
        

        CSplignFormatter sf (splign);
        
        /*MRNA*/
        
        if(args["mrna-data-in"]) {

            splign.SetStartModelId(1);
            splign.SetAlignerScores();
            
            CNcbiIstream& istr = args["mrna-data-in"].AsInputFile();        
            unique_ptr<CObjectIStream> is (CObjectIStream::Open(eSerial_AsnText, istr));  
            string ofmt;
            if(args["mrna-out"]) {                    
                ofmt = args["mrna-outfmt"].AsString();
            }

            //READ BLAST HITS; GROUP BY QUERY/SUBJECT PAIR

            CRef<CSeq_annot> blast_align(new CSeq_annot);
            while( istr ) {     // one Seq-annot -> one Seq-align-set
                try {
                    *is >> *blast_align;
                }
                catch (CEofException&) {
                    break;
                }

                typedef map<pair<CSeq_id_Handle, CSeq_id_Handle>, CSplign::THitRefs> TAllHits;
                TAllHits one_seq_annot_hits_in;            
                 
                CRef<CSeq_align_set> splign_out(new CSeq_align_set);
                
                ITERATE(CSeq_align_set::Tdata, AlignIter, blast_align->GetData().GetAlign()) {
                    CSplign::THitRef tr(new CBlastTabular(**AlignIter));
                    CConstRef<objects::CSeq_id> query_id =  tr->GetQueryId();      
                    CConstRef<objects::CSeq_id> subj_id = tr->GetSubjId();
                    CSeq_id_Handle idh1 = CSeq_id_Handle::GetHandle(*query_id);
                    CSeq_id_Handle idh2 = CSeq_id_Handle::GetHandle(*subj_id);
                    one_seq_annot_hits_in[make_pair(idh1, idh2)].push_back(tr);
                }

                //iterate by query/subject pair

           /* DEBUG OUTPUT 
                NON_CONST_ITERATE(TAllHits, qsit, one_seq_annot_hits_in) {
                    cout<<"*****************"<<endl;
                    cout<<"PAIR: "<<qsit->first.first.AsString()<<"\t"<<qsit->first.second.AsString()<<endl;
                    cout<<"*****************"<<endl;
                    ITERATE( CSplign::THitRefs, it, qsit->second) {
                        cout<<**it<<endl;
                    }
                }

            END OF DEBUG OUTPUT   */

                NON_CONST_ITERATE(TAllHits, qsit, one_seq_annot_hits_in) {
                    sf.SetSeqIds(qsit->first.first.GetSeqId(), qsit->first.second.GetSeqId());
                    splign.Run(&(qsit->second));//note that this call spoils hits_in data      
                    CRef<CSeq_align_set> sas (sf.AsSeqAlignSet(&splign.GetResult(), CSplignFormatter::eAF_SplicedSegWithParts));
                    splign_out->Set().insert(splign_out->Set().end(), sas->Get().begin(), sas->Get().end());
                    if(args["mrna-out"]) {//output alignments   
                        if(ofmt == "text") {
                            CNcbiOstream& ostr = args["mrna-out"].AsOutputFile();                                       
                            ostr<<sf. AsAlignmentText(scope, &splign.GetResult())<<endl;
                        } else if(ofmt == "splign") {
                            CNcbiOstream& ostr = args["mrna-out"].AsOutputFile();                                       
                            ostr<<sf.AsExonTable(&splign.GetResult())<<endl;;
                        }
                    }
                }
                if(args["mrna-out"] && ofmt == "asn") {//output asn   
                    CNcbiOstream& ostr = args["mrna-out"].AsOutputFile();                                       
                    ostr << MSerial_AsnText << *splign_out;                        
                }
                
                //CHECK IF SPLIGN RESULT IS THE SAME AS EXPECTED    

                if(args["mrna-expected"]) {
                    CNcbiIstream& estr = args["mrna-expected"].AsInputFile();        
                    unique_ptr<CObjectIStream> es (CObjectIStream::Open(eSerial_AsnText, estr));                
                    CRef<CSeq_align_set> expected_out(new CSeq_align_set);
                    *es >> *expected_out;

                    /* solution is not working, some real numbers produced on CentOS and Windows stay different after serialization
                    CNcbiOstrstream s1;
                    s1 << MSerial_AsnText << *splign_out;
                    string mrna1 = CNcbiOstrstreamToString(s1);
                    
                    CNcbiOstrstream s2;
                    s2 << MSerial_AsnText << *expected_out;
                    string mrna2 = CNcbiOstrstreamToString(s2);

					BOOST_CHECK ( mrna1 == mrna2 );
                    */

                    s_RoundScores(expected_out);
                    s_RoundScores(splign_out);
                    BOOST_CHECK ( splign_out->Equals(*expected_out) ); 

/* DEBUG OUTPUT
if(! splign_out->Equals(*expected_out) ) {
	 CNcbiOfstream os1("tmp1.asn");	
	 os1 << MSerial_AsnText << *splign_out;
	 CNcbiOfstream os2("tmp2.asn");	
	 os2 << MSerial_AsnText << *expected_out;
	 exit(1);
}
*/
                    
                }                                
            }
        }


        /*EST*/
        
        if(args["est-data-in"]) {

            splign.SetStartModelId(1);
            splign.SetScoringType(CSplign::eEstScoring);
            splign.SetAlignerScores();
            
            CNcbiIstream& istr = args["est-data-in"].AsInputFile();        
            unique_ptr<CObjectIStream> is (CObjectIStream::Open(eSerial_AsnText, istr));   
            string ofmt;
            if(args["est-out"]) {                    
                ofmt = args["est-outfmt"].AsString();
            }

            //READ BLAST HITS; GROUP BY QUERY/SUBJECT PAIR
             
            CRef<CSeq_annot> blast_align(new CSeq_annot);
            while( istr ) {      // one Seq-annot -> one Seq-align-set           
                try {
                    *is >> *blast_align;
                }
                catch (CEofException&) {
                    break;
                }
                
                typedef map<pair<CSeq_id_Handle, CSeq_id_Handle>, CSplign::THitRefs> TAllHits;
                TAllHits one_seq_annot_hits_in;            
                 
                CRef<CSeq_align_set> splign_out(new CSeq_align_set);
                
                ITERATE(CSeq_align_set::Tdata, AlignIter, blast_align->GetData().GetAlign()) {
                    CSplign::THitRef tr(new CBlastTabular(**AlignIter));
                    CConstRef<objects::CSeq_id> query_id =  tr->GetQueryId();      
                    CConstRef<objects::CSeq_id> subj_id = tr->GetSubjId();
                    CSeq_id_Handle idh1 = CSeq_id_Handle::GetHandle(*query_id);
                    CSeq_id_Handle idh2 = CSeq_id_Handle::GetHandle(*subj_id);
                    one_seq_annot_hits_in[make_pair(idh1, idh2)].push_back(tr);
                }

                //iterate by query/subject pair                
                
                NON_CONST_ITERATE(TAllHits, qsit, one_seq_annot_hits_in) {
                    sf.SetSeqIds(qsit->first.first.GetSeqId(), qsit->first.second.GetSeqId());
                    splign.Run(&(qsit->second));//note that this call spoils hits_in data      
                    CRef<CSeq_align_set> sas (sf.AsSeqAlignSet(&splign.GetResult(), CSplignFormatter::eAF_SplicedSegWithParts));
                    splign_out->Set().insert(splign_out->Set().end(), sas->Get().begin(), sas->Get().end());
                    if(args["est-out"]) {//output alignments   
                        if(ofmt == "text") {
                            CNcbiOstream& ostr = args["est-out"].AsOutputFile();                                       
                            ostr<<sf. AsAlignmentText(scope, &splign.GetResult())<<endl;
                        } else if(ofmt == "splign") {
                            CNcbiOstream& ostr = args["est-out"].AsOutputFile();                                       
                            ostr<<sf.AsExonTable(&splign.GetResult())<<endl;;
                        }
                    }
                }
                if(args["est-out"] && ofmt == "asn") {//output asn   
                    CNcbiOstream& ostr = args["est-out"].AsOutputFile();                                       
                    ostr << MSerial_AsnText << *splign_out;                        
                }

                //CHECK IF SPLIGN RESULT IS THE SAME AS EXPECTED    

                if(args["est-expected"]) {
                    CNcbiIstream& estr = args["est-expected"].AsInputFile();        
                    unique_ptr<CObjectIStream> es (CObjectIStream::Open(eSerial_AsnText, estr));                
                    CRef<CSeq_align_set> expected_out(new CSeq_align_set);
                    *es >> *expected_out;

                    s_RoundScores(expected_out);
                    s_RoundScores(splign_out);
                    BOOST_CHECK ( splign_out->Equals(*expected_out) );                     

                }  
            }
        }
        /*END of EST*/
    }
}                              








