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
 * Author:  Nathan Bouk
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/rwstream.hpp>
#include <dbapi/driver/drivers.hpp>
#include <objtools/simple/simple_om.hpp>
#include <connect/email_diag_handler.hpp>
#include <util/compress/zlib.hpp>
#include <serial/serialbase.hpp>
#include <serial/serial.hpp>
#include <serial/objostrjson.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistr.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/seq_align_util.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/alnmgr/alnmix.hpp>
#include <objtools/format/ostream_text_ostream.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/general/Dbtag.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/feat_ci.hpp>
#include <objects/seqset/gb_release_file.hpp>

#include <objects/genomecoll/genome_collection__.hpp>
#include <objects/genomecoll/genomic_collections_cli.hpp>

#include <algo/id_mapper/id_mapper.hpp>

//#include "feat_loader.hpp"


using namespace std;
using namespace ncbi;
using namespace objects;



typedef map<string, CStopWatch> TTimerMap;
 TTimerMap TimerMap;


class CIdMapperTestApp : public  CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run();

private:

    CRef<CScope> m_Scope;
    CBioseq::TAnnot Annots; // list of crefs to annots


	int x_HandleFeatures(CGencollIdMapper& Mapper, 
						CGencollIdMapper::SIdSpec& DestSpec,
                        CNcbiOstream& Out);
	int x_HandleAlignments(CGencollIdMapper& Mapper, 
						CGencollIdMapper::SIdSpec& DestSpec,
                        CNcbiOstream& Out);
    int x_HandleEntries(CGencollIdMapper& Mapper, 
						CGencollIdMapper::SIdSpec& DestSpec,
                        CNcbiOstream& Out);
	
    void x_RecurseMapSeqAligns(CSeq_align& Align, int Row, 
							   CGencollIdMapper& Mapper, 
							   CGencollIdMapper::SIdSpec& DestSpec);

};


void CIdMapperTestApp::Init()
{
    // Create CGI argument descriptions class
    //  (For CGI applications only keys can be used)
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Annot Fetching Tool");

    // Required arguments:

    // default argument
    arg_desc->AddOptionalKey("gc", "string", "gencoll accession", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("gcf", "string", "gencoll asnt file", CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey("annot", "infile", "input annot file",
                            CArgDescriptions::eInputFile);
	arg_desc->AddOptionalKey("align", "infile", "input align file",
                            CArgDescriptions::eInputFile);
	arg_desc->AddOptionalKey("entry", "infile", "input entry file",
                            CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("row", "int", "only update this row in the alignment",
							CArgDescriptions::eInteger);
	

    arg_desc->AddOptionalKey("typed", "string", "CGC_TypedSeqId", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("alias", "string", "CGC_SeqIdAlias", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("external", "string", "UCSC?", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("pattern", "string", "<prefix>%s<postfix>", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("role", "string", "CGC_SequenceRole", CArgDescriptions::eString);
    arg_desc->AddOptionalKey("primary", "string", "anything", CArgDescriptions::eString);
    
    arg_desc->AddKey("out", "outfile", "mapped file", CArgDescriptions::eOutputFile);

    arg_desc->AddOptionalKey("informat", "string", "accepted input formats", CArgDescriptions::eString);
    
    SetupArgDescriptions(arg_desc.release());
}


int CIdMapperTestApp::Run()
{
    DBAPI_RegisterDriver_FTDS();

    const CArgs& args = GetArgs();

    /*{{
        string T = "2LHet";
        CSeq_id TId;
        try {
        TId.Set(T);
        cout << MSerial_AsnText << TId << endl;
        }catch(...) {;}

        try {
        TId.Set("030294");
        cout << MSerial_AsnText << TId << endl;
        }catch(...) {;}
        
        try {
        TId.Set("68C5");
        cout << MSerial_AsnText << TId << endl;
        }catch(...) {;}
        
        try {
        TId.Set("1234");
        cout << MSerial_AsnText << TId << endl;
        }catch(...) {;}

        try {
        TId.Set("1");
        cout << MSerial_AsnText << TId << endl;
        }catch(...) {;}
        
        try {
        TId.Set("11");
        cout << MSerial_AsnText << TId << endl;
        }catch(...) {;}
        
        try {
        TId.Set("111");
        cout << MSerial_AsnText << TId << endl;
        }catch(...) {;}

        try {
        TId.Set("111111111");
        cout << MSerial_AsnText << TId << endl;
        }catch(...) {;}
        
        try {
        TId.Set("1111111111");
        cout << MSerial_AsnText << TId << endl;
        }catch(...) {;}
        
        try {
        TId.Set("2111111111");
        cout << MSerial_AsnText << TId << endl;
        }catch(...) {;}
        
        try {
        TId.Set("11111111111");
        cout << MSerial_AsnText << TId << endl;
        }catch(...) {;}
        
        try {
        TId.Set("1111111111111");
        cout << MSerial_AsnText << TId << endl;
        }catch(...) {;}

    }}*/

    CRef<CGencollIdMapper> Mapper;

    
    if(args["gc"].HasValue()) {
        string GCAsm = args["gc"].AsString();
        CGenomicCollectionsService GcService;
        CRef<CGC_Assembly> Asm;
        Asm = GcService.GetAssembly(GCAsm, CGCClient_GetAssemblyRequest::eLevel_component);
        {{
            CNcbiOfstream Out("debug_gc.asnt");
            Out << MSerial_AsnText << *Asm;
            Out.close();
        }}
        Mapper.Reset(new CGencollIdMapper(Asm));
    } else if(args["gcf"].HasValue()) {
        CRef<CGC_Assembly> Asm(new CGC_Assembly);
       args["gcf"].AsInputFile() >> MSerial_AsnText >> *Asm;
        Mapper.Reset(new CGencollIdMapper(Asm));
	} else {
        cerr << "Rquires either a -gc or a -gcf" << endl;
        return -1;
    }


	CRef<CObjectManager> om(CObjectManager::GetInstance());
    CGBDataLoader::RegisterInObjectManager(*om, 0, CObjectManager::eDefault, 200);
	m_Scope.Reset(new CScope(*om));
	m_Scope->AddDefaults();


	//cerr << MSerial_AsnText << *Mapper.GetInternalGencoll() << endl;

    CGencollIdMapper::SIdSpec DestSpec;
    DestSpec.TypedChoice = CGC_TypedSeqId::e_Refseq;
    DestSpec.Alias = CGC_SeqIdAlias::e_Gi;
    DestSpec.Role = CGencollIdMapper::SIdSpec::e_Role_NotSet;
	DestSpec.External.clear();
    DestSpec.Pattern.clear();

    if(args["typed"].HasValue()) {
        if(args["typed"].AsString() == "genbank")
            DestSpec.TypedChoice = CGC_TypedSeqId::e_Genbank;
        if(args["typed"].AsString() == "refseq")
            DestSpec.TypedChoice = CGC_TypedSeqId::e_Refseq;
        if(args["typed"].AsString() == "external")
            DestSpec.TypedChoice = CGC_TypedSeqId::e_External;
        if(args["typed"].AsString() == "private")
            DestSpec.TypedChoice = CGC_TypedSeqId::e_Private;
    }

    if(args["alias"].HasValue()) {
        if(args["alias"].AsString() == "public")
            DestSpec.Alias = CGC_SeqIdAlias::e_Public;
        if(args["alias"].AsString() == "gpipe")
            DestSpec.Alias = CGC_SeqIdAlias::e_Gpipe;
        if(args["alias"].AsString() == "gi")
            DestSpec.Alias = CGC_SeqIdAlias::e_Gi;
        if(args["alias"].AsString() == "notset")
            DestSpec.Alias = CGC_SeqIdAlias::e_None;
    }
 
 	if(args["role"].HasValue()) {
        if(args["role"].AsString() == "top") {
            DestSpec.Role = eGC_SequenceRole_top_level;
		}
		if(args["role"].AsString() == "gpipetop") {
            DestSpec.Role = eGC_SequenceRole_top_level;
		}

        if(args["role"].AsString() == "chro")
            DestSpec.Role = eGC_SequenceRole_chromosome;
        if(args["role"].AsString() == "scaf")
            DestSpec.Role = eGC_SequenceRole_scaffold;
        if(args["role"].AsString() == "comp")
            DestSpec.Role = eGC_SequenceRole_component;

        if(args["role"].AsString() == "notset") {
            DestSpec.Role = CGencollIdMapper::SIdSpec::e_Role_NotSet;
        }
    }

    if(args["external"].HasValue())
        DestSpec.External = args["external"].AsString();

    if(args["pattern"].HasValue())
        DestSpec.Pattern = args["pattern"].AsString();

    if(args["primary"].HasValue())
        DestSpec.Primary = true;


	cerr << DestSpec.ToString() << endl;

    CNcbiOstream& Out = args["out"].AsOutputFile();

	if(args["annot"].HasValue())
		return x_HandleFeatures(*Mapper, DestSpec, Out);
	else if(args["align"].HasValue())
		return x_HandleAlignments(*Mapper, DestSpec, Out);
    else if(args["entry"].HasValue())
		return x_HandleEntries(*Mapper, DestSpec, Out);

    return 1;
}


int CIdMapperTestApp::x_HandleFeatures(CGencollIdMapper& Mapper, 
					  					CGencollIdMapper::SIdSpec& DestSpec,
                                        CNcbiOstream& Out)
{
    const CArgs& args = GetArgs();

    list<CRef<CSeq_annot> > Annots;
    /*
	CFeatLoader FeatLoader(m_Scope);
    if(args["informat"].HasValue()) {
        FeatLoader.SetReadFormat(args["informat"].AsString());
    }
	FeatLoader.LoadAnnots(args["annot"].AsInputFile(), Annots);
    */
    {{
        CNcbiIstream& AnnotIn = args["annot"].AsInputFile();
        while(AnnotIn) {
            try {
                CRef<CSeq_annot> Annot(new CSeq_annot);
                AnnotIn >> MSerial_AsnText >> *Annot;
                Annots.push_back(Annot);
            } catch(...) { ; }
        }
    }}

    if(Annots.empty())
        return 1;

	NON_CONST_ITERATE(list<CRef<CSeq_annot> >, AnnotIter, Annots) {
    	CRef<CSeq_annot> Annot = *AnnotIter;

		NON_CONST_ITERATE(CSeq_annot::C_Data::TFtable, featiter, Annot->SetData().SetFtable()) {
			CRef<CSeq_feat> Feat = *featiter;

			CGencollIdMapper::SIdSpec Spec;
			Spec.TypedChoice = CGC_TypedSeqId::e_Refseq;
			//Mapper.Guess(Feat->GetLocation(), Spec);
			//DestSpec.External = Spec.External;
			//DestSpec.Pattern = Spec.Pattern;

			CGencollIdMapper::SIdSpec GuessSpec;

			CRef<CSeq_loc> Loc;
			try {
				Mapper.Guess(Feat->GetLocation(), GuessSpec);
				cerr << "Guessed: " << GuessSpec.ToString() << endl;
				//cerr << "CanMeetSpec: " << (Mapper.CanMeetSpec(Feat->GetLocation(), DestSpec) ? "TRUE" : "FALSE" ) << endl;
				//if(!GuessSpec.Pattern.empty() && DestSpec.Pattern.empty())
				//    DestSpec.Pattern = GuessSpec.Pattern;
				Loc = Mapper.Map(Feat->GetLocation(), DestSpec);
				//cerr << "Mapped: " << MSerial_AsnText << *Feat->GetLocation().GetId()
				//	 << " to " << MSerial_AsnText << *Loc->GetId() << endl;
            } catch(CException& e) {
				cerr << "Map Exception: " << e.ReportAll() << endl;
			}

			if(Loc.IsNull()) {
			//	cerr << "Map Fail" << endl;
			//	cerr << "Dest: " << DestSpec.ToString() << endl;
            //  cerr << "Guess: " << GuessSpec.ToString() << endl;
			//	cerr << MSerial_AsnText << Feat->GetLocation();
				continue;
			}

			Feat->SetLocation().Assign(*Loc);
		}

	}

    //FeatLoader.WriteAnnots(Out, Annots, Mapper.GetInternalGencoll());
    
    ITERATE(list<CRef<CSeq_annot> >, AnnotIter, Annots) {
        cout << MSerial_AsnText << **AnnotIter;
    }
	
    return 0;
}
	

int CIdMapperTestApp::x_HandleAlignments(CGencollIdMapper& Mapper, 
					  					CGencollIdMapper::SIdSpec& DestSpec,
                                        CNcbiOstream& Out)
{
    const CArgs& args = GetArgs();

    CRef<CSeq_align_set> Alignments(new CSeq_align_set);

	int Row = -1;
	if(args["row"].HasValue())
		Row = args["row"].AsInteger();

	CNcbiIstream& In = args["align"].AsInputFile();
	while(!In.eof()) {
		CRef<CSeq_align_set> AlignSet(new CSeq_align_set);
		CRef<CSeq_annot> Annot(new CSeq_annot);
        CRef<CSeq_align> Align(new CSeq_align);

		off_t Pos = In.tellg();
        if(Pos == -1)
            break;
        
        try {
			In.seekg(Pos);
            In >> MSerial_AsnBinary >> *AlignSet;
			//cerr << "Success Binary Seq-align-set" << endl;
        	Alignments->Set().insert(Alignments->Set().end(),
									AlignSet->Set().begin(),
									AlignSet->Set().end());
			continue;
		} catch(...) { ; }

		try {
			In.seekg(Pos);
			In >> MSerial_AsnBinary >> *Annot;
			//cerr << "Success Binary Seq-annot" << endl;
			Alignments->Set().insert(Alignments->Set().end(), 
									Annot->SetData().SetAlign().begin(),
									Annot->SetData().SetAlign().end());
			continue;
		} catch(...) { ; }
		
		try {
			In.seekg(Pos);
			In >> MSerial_AsnBinary >> *Align;
			//cerr << "Success Binary Seq-align" << endl;
			Alignments->Set().push_back(Align);
			continue;
		} catch(...) { ; }

        try {
			In.seekg(Pos);
            //Pos = In.tellg();
            In >> MSerial_AsnText >> *AlignSet;
			//cerr << "Success Text Seq-align-set" << endl;
			//cerr << Alignments->Set().size() << "  " 
			//	<< AlignSet->Set().size() << endl;
        	Alignments->Set().insert(Alignments->Set().end(),
									AlignSet->Set().begin(),
									AlignSet->Set().end());
			//cerr << Alignments->Set().size() << endl;
			continue;
		} catch(...) { ; }

		try {
			In.seekg(Pos);
			In >> MSerial_AsnText >> *Annot;
			//cerr << "Success Text Seq-annot" << endl;
			Alignments->Set().insert(Alignments->Set().end(), 
									Annot->SetData().SetAlign().begin(),
									Annot->SetData().SetAlign().end());
			continue;
		} catch(...) { ; }
		
		try {
			In.seekg(Pos);
			In >> MSerial_AsnText >> *Align;
			//cerr << "Success Text Seq-align" << endl;
			Alignments->Set().push_back(Align);
			continue;
		} catch(...) { ; }

		if(!In.eof()) {
            if(Alignments->Get().empty()) {
			    cerr << "File reader failure";
			    return 1;
            }
		}
		//cerr << "Non-eof End of Filereader!" << endl;
	}

    if(Alignments->Get().empty())
		return 1;


	NON_CONST_ITERATE(CSeq_align_set::Tdata, AlignIter, 
						Alignments->Set()) {
		x_RecurseMapSeqAligns(**AlignIter, Row, Mapper, DestSpec);
	}
	
	//cout << MSerial_AsnText << *Alignments;
    ITERATE(CSeq_align_set::Tdata, AlignIter, Alignments->Get()) {
        Out << MSerial_AsnText << **AlignIter;
    }

	return 0;
}


int CIdMapperTestApp::x_HandleEntries(CGencollIdMapper& Mapper, 
					  					CGencollIdMapper::SIdSpec& DestSpec,
                                        CNcbiOstream& Out)
{
    const CArgs& args = GetArgs();

    CRef<CSeq_entry> Entry(new CSeq_entry);
    

	int Row = -1;
	if(args["row"].HasValue())
		Row = args["row"].AsInteger();

	CNcbiIstream& In = args["entry"].AsInputFile();
	//while(!In.eof()) {
            In >> MSerial_AsnText >> *Entry;
	    
    //}

    
    CTypeIterator<CSeq_align> AlignIter(*Entry);
    while(AlignIter) {
        
        x_RecurseMapSeqAligns(*AlignIter, Row, Mapper, DestSpec);
        ++AlignIter;
    }


    //ITERATE(CSeq_align_set::Tdata, AlignIter, Alignments->Get()) {
        Out << MSerial_AsnText << *Entry;
    //}

	return 0;
}


void CIdMapperTestApp::x_RecurseMapSeqAligns(CSeq_align& Align, int Row, 
							   CGencollIdMapper& Mapper, 
							   CGencollIdMapper::SIdSpec& DestSpec)
{
	if(Align.GetSegs().IsDisc()) {
		NON_CONST_ITERATE(CSeq_align_set::Tdata, AlignIter, 
							Align.SetSegs().SetDisc().Set()) {
			x_RecurseMapSeqAligns(**AlignIter, Row, Mapper, DestSpec);
		}
		return;
	}

	else if(Align.GetSegs().IsDenseg()) {

		CDense_seg& Denseg = Align.SetSegs().SetDenseg();
		int Loop = 0;	
		NON_CONST_ITERATE(CDense_seg::TIds, IdIter, Denseg.SetIds()) {
    //cerr << __LINE__ << " : " << Loop << " of " << Row << endl;			
			if(Row == -1 || Row == Loop) {
				CSeq_id& Id = **IdIter;

				CSeq_loc Whole;
				//Whole.SetWhole(Id);
                CRef<CSeq_loc> T = Align.CreateRowSeq_loc(Row);
                Whole.Assign(*T);


				CRef<CSeq_loc> Mapped;
				Mapped = Mapper.Map(Whole, DestSpec);
                

				if(!Mapped.IsNull() && !Mapped->IsNull() && !Mapped->IsEmpty() ) {
					//cerr << "Repping " << MSerial_AsnText << Whole 
					//	<< " with " << MSerial_AsnText << *Mapped << endl;
					if(Whole.Equals(*Mapped)) {
                        //cerr << __LINE__ << " SKIP " << endl;
                        Loop++;
                        continue;
                    }
                    if(Mapped->IsWhole()) {
						Id.Assign(Mapped->GetWhole());
					} else if(Mapped->IsInt()) {
						if(Whole.GetInt().GetLength() != Mapped->GetInt().GetLength()) {
                            cerr << __LINE__ << " : " << "Can not perfectly map from " 
                                << Whole.GetInt().GetId().AsFastaString() << " to "
                                << Mapped->GetInt().GetId().AsFastaString() << " , "
                                << "The first is not entirely represented in the second." << endl;
                        }
                        CRef<CSeq_align> MapAlign;
                        try { 
                            CSeq_loc_Mapper Mapper(Whole, *Mapped, NULL);
                            MapAlign = Mapper.Map(Align, Row);
                        } catch(CException& e) {
                            cerr <<__LINE__<< " : " << "CSeq_loc_Mapper ctor or Map exception" << endl;
                            ERR_POST(e.ReportAll());
                        }
                        //MapAlign = sequence::RemapAlignToLoc(Align, Row, *Mapped,  NULL); // scope)
                        if(MapAlign) {
                            //cerr << __LINE__ << " ASSIGN_MAP " << endl;
                            Align.Assign(*MapAlign);
                            //cerr << MSerial_AsnText << Align;
                            //Denseg = Align.SetSegs().SetDenseg();
                            break;
                        } else {
                            cerr << __LINE__ << " FAIL_MAP " << endl;
                        }
                        //Denseg.RemapToLoc(Row, *Mapped, false);
                       /* Id.Assign(Mapped->GetInt().GetId());
						TSignedSeqPos Offset = ((TSignedSeqPos)Mapped->GetStart(eExtreme_Positional))
                                             - ((TSignedSeqPos)Whole.GetStart(eExtreme_Positional));
                        Denseg.OffsetRow(Row, Offset);
                        if(T->GetStrand() != Mapped->GetStrand()) {
                            Denseg.Reverse();
                            for(size_t I=0; I < Denseg.GetStrands().size(); I++) {
                                if( (I % Denseg.GetDim()) != Row ) {
                                    if(Denseg.GetStrands()[I] == eNa_strand_plus)
                                        Denseg.SetStrands()[I] = eNa_strand_minus;
                                    else
                                        Denseg.SetStrands()[I] = eNa_strand_plus;
                                }
                            }
                        }*/
                    }
				} else {
					cerr << "Align: Failed on " << MSerial_AsnText << Id ;
                    if(!Mapped.IsNull())
                        cerr << MSerial_AsnText << *Mapped << endl;
				}
			}

			Loop++;
		}

		return;
	}

    else if(Align.GetSegs().IsSpliced()) {
        CSpliced_seg& Spliced = Align.SetSegs().SetSpliced();

        NON_CONST_ITERATE( list< CRef<CSpliced_exon> >, ExonIter, Spliced.SetExons()) {
            
            //
        }

    }

}



/////////////////////////////////////////////////////////////////////////////
//  MAIN
//

int main(int argc, const char* argv[])
{
    SetSplitLogFile(false);
    GetDiagContext().SetOldPostFormat(true);
    SetDiagPostLevel(eDiag_Info);
    int result = 1;
    try {
        result = CIdMapperTestApp().AppMain(argc, argv, 0, eDS_Default, 0);//, "../ctgoverlap_web.ini");
    //_TRACE("back to normal diags");
    } catch (CArgHelpException& e) {
        cout << e.ReportAll() << endl;
    }
    return result;
}
