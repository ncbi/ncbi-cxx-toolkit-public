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
 * Authors: Nathan Bouk, Mike DiCuccio
 *
 * File Description:
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/scope.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objtools/readers/agp_read.hpp>
#include <objtools/readers/agp_seq_entry.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <objtools/readers/fasta.hpp>

#include <util/line_reader.hpp>
#include <objects/genomecoll/genome_collection__.hpp>

#include <algo/align/ngalign/ngalign.hpp>
#include <algo/align/ngalign/alignment_filterer.hpp>
#include <algo/align/ngalign/sequence_set.hpp>
#include <algo/align/ngalign/blast_aligner.hpp>
#include <algo/align/ngalign/banded_aligner.hpp>
#include <algo/align/ngalign/merge_aligner.hpp>
//#include <algo/align/ngalign/coverage_aligner.hpp>
#include <algo/align/ngalign/overlap_aligner.hpp>
#include <algo/align/ngalign/inversion_merge_aligner.hpp>
#include <algo/align/ngalign/alignment_scorer.hpp>
#include <algo/align/ngalign/unordered_spliter.hpp>

#include <algo/align/ngalign/merge_tree_aligner.hpp>

/*
#include <asn_cache/lib/Cache_blob.hpp>
#include <asn_cache/lib/asn_index.hpp>
#include <asn_cache/lib/chunk_file.hpp>
#include <asn_cache/lib/asn_cache_util.hpp>
#include <asn_cache/lib/seq_id_chunk_file.hpp>
#include <internal/asn_cache/lib/asn_cache_loader.hpp>
*/

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);



class CNgAlignApp : public CNcbiApplication
{
private:
    void Init();
    int Run();

   
	CRef<CScope> m_Scope;
	
	auto_ptr<CUnorderedSplitter> m_Splitter;

	list<CRef<CSeq_id> > m_LoadedIds;
	list<CRef<CSeq_id> >::const_iterator LoadedIdsIter;
    
    int BatchCounter;

	bool x_CreateNgAlignRun(CNgAligner& NgAligner, IRegistry* RunRegistry); 
	CRef<ISequenceSet> x_CreateSequenceSet(IRegistry* RunRegistry, 
										const string& Category);

	void x_LoadExternalSequences(IRegistry* RunRegistry, 
								const string& Category,
								list<CRef<CSeq_id> >& LoadedIds);

	void x_AddScorers(CNgAligner& NgAligner, IRegistry* RunRegistry);
	void x_AddFilters(CNgAligner& NgAligner, IRegistry* RunRegistry);
	void x_AddAligners(CNgAligner& NgAligner, IRegistry* RunRegistry);

	CRef<CBlastAligner> x_CreateBlastAligner(IRegistry* RunRegistry, const string& Name);
	CRef<CRemoteBlastAligner> x_CreateRemoteBlastAligner(IRegistry* RunRegistry, const string& Name);
	CRef<CMergeAligner> x_CreateMergeAligner(IRegistry* RunRegistry, const string& Name);
//	CRef<CCoverageAligner> x_CreateCoverageAligner(IRegistry* RunRegistry, const string& Name);
	CRef<CMergeTreeAligner> x_CreateMergeTreeAligner(IRegistry* RunRegistry, const string& Name);
	CRef<CInversionMergeAligner> x_CreateInversionMergeAligner(IRegistry* RunRegistry, const string& Name);
	CRef<CSplitSeqAlignMerger> x_CreateSplitSeqMergeAligner(IRegistry* RunRegistry, const string& Name);


	void x_RecurseSeqEntry(CRef<CSeq_entry> Top, list<CRef<CSeq_id> >& SeqIdList);

	CRef<CGC_Assembly> x_LoadAssembly(CNcbiIstream& In);

	void x_PrintNoHitList(CNcbiOstream& Out, const CSeq_align_set& Alignments);
};


void CNgAlignApp::Init()
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramName(),
                              "Generic app for creating an NgAlign "
                              "run, and running it.");

	arg_desc->AddFlag("info", "info level logging");

    arg_desc->AddDefaultKey("o", "OutputFile",
                            "File for results",
                            CArgDescriptions::eOutputFile,
                            "-");
    arg_desc->AddFlag("b", "Output binary ASN.1");
    
	arg_desc->AddFlag("dup", "allow dupes");

	arg_desc->AddKey("run", "InputFile", "ngalign run ini file", 
					CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey("asn_cache", "string", "comma seperated paths", 
                            CArgDescriptions::eString);
	arg_desc->AddFlag("G", "No genbank");

	arg_desc->AddOptionalKey("query", "string", "single seq-id",
					CArgDescriptions::eString);
	arg_desc->AddOptionalKey("subject", "string", "single seq-id",
					CArgDescriptions::eString);
	arg_desc->AddOptionalKey("qidlist", "file", "seq-id list file",
					CArgDescriptions::eInputFile);
	arg_desc->AddOptionalKey("sidlist", "file", "seq-id list file",
					CArgDescriptions::eInputFile);
	arg_desc->AddOptionalKey("idlist", "file", "fasta seq-id list file",
					CArgDescriptions::eInputFile);
	arg_desc->AddOptionalKey("qloclist", "file", "seq-loc list file",
					CArgDescriptions::eInputFile);	
	arg_desc->AddOptionalKey("sloclist", "file", "seq-loc list file",
					CArgDescriptions::eInputFile);	
    arg_desc->AddOptionalKey("blastdb", "blastdb", "Blastdb Name",
					CArgDescriptions::eInputFile);
	arg_desc->AddOptionalKey("softfilter", "integer", "blastdb soft filter id",
					CArgDescriptions::eInteger);
	arg_desc->AddOptionalKey("fasta", "file", "fasta file",
					CArgDescriptions::eInputFile);
	
	arg_desc->AddOptionalKey("seqentry", "file", "seq-entry file",
					CArgDescriptions::eInputFile);
	arg_desc->AddOptionalKey("agp", "file", "agp file",
					CArgDescriptions::eInputFile);

	arg_desc->AddOptionalKey("gc", "file", "gencoll asn.1 Assembly",
					CArgDescriptions::eInputFile);

	arg_desc->AddDefaultKey("batch", "int", "batch size for loaded ids", 
							CArgDescriptions::eInteger, "100");

	arg_desc->AddOptionalKey("nohit", "outfile", "List of nohit IDs", 
							CArgDescriptions::eOutputFile);
    
    arg_desc->AddOptionalKey("filters", "string", 
                            "semi-colon seperated list of filters, overrides the ini file",
                            CArgDescriptions::eString);
    arg_desc->AddOptionalKey("scorers", "string", 
                            "semi-colon seperated list of scorers, overrides the ini file",
                            CArgDescriptions::eString);


    SetupArgDescriptions(arg_desc.release());
}


int CNgAlignApp::Run()
{
    const CArgs& args = GetArgs();

	if(args["info"].HasValue())
		SetDiagPostLevel(eDiag_Info);  

    // Scope things
    CRef<CObjectManager> om(CObjectManager::GetInstance());
    
	if(!args["G"].HasValue())
		CGBDataLoader::RegisterInObjectManager(*om);

	if(args["blastdb"].HasValue()) {
		CBlastDbDataLoader::RegisterInObjectManager
        	(*om, args["blastdb"].AsString(),
	    		CBlastDbDataLoader::eNucleotide,
				true, CObjectManager::eDefault, 100);
	}
   
    /* 
    if(args["asn_cache"].HasValue()) {
	    vector<string> Tokens;
        NStr::Tokenize(args["asn_cache"].AsString(), ",", Tokens);
        int Priority = 1;
        ITERATE(vector<string>, CacheIter, Tokens) {
            CFile CacheFile( *CacheIter + "/asn_cache.idx"  );
		    if(CacheFile.Exists()) {
			    CAsnCache_DataLoader::RegisterInObjectManager(*om, *CacheIter,
													CObjectManager::eDefault, Priority);
                Priority++;
		    }
        }	
    }*/
    

	m_Scope.Reset(new CScope(*om));
	m_Scope->AddDefaults();

    // end scope

	m_Splitter.reset(new CUnorderedSplitter(*m_Scope));
	
    CNcbiRegistry RunRegistry(args["run"].AsInputFile());
	
	bool AllowDupes = false;
	if(RunRegistry.Get("ngalign", "allow_dupes") == "true")
		AllowDupes = true;
	if(args["dup"].HasValue() && args["dup"].AsBoolean())
		AllowDupes = true;
ERR_POST(Info << "Dupes: " << AllowDupes);
	
	CRef<CGC_Assembly> Assembly;
	if(args["gc"].HasValue()) {
		Assembly = x_LoadAssembly(args["gc"].AsInputFile());
	}


    BatchCounter = 0;

	x_LoadExternalSequences(&RunRegistry, "", m_LoadedIds);
	LoadedIdsIter = m_LoadedIds.begin();
	

	CRef<CSeq_align_set> Alignments(new CSeq_align_set);

	do {
		CNgAligner NgAligner(*m_Scope, Assembly, AllowDupes);

		bool Created;
		Created = x_CreateNgAlignRun(NgAligner, &RunRegistry);

		if(!Created) {
			cerr << "Failed to create Run." << endl;
			return 1;
		}

		CRef<CSeq_align_set> CurrAligns;
		try {
            CurrAligns = NgAligner.Align();
		} catch(CException& e) {
            ERR_POST(Warning << e.ReportAll());
            break;
        }

        if(CurrAligns) {
			Alignments->Set().insert(Alignments->Set().end(), 
									CurrAligns->Get().begin(), 
									CurrAligns->Get().end());
		}
        //break;
        ERR_POST(Info << "Looping" );
	} while(!m_LoadedIds.empty() && LoadedIdsIter != m_LoadedIds.end() );

	if(!Alignments.IsNull()) {
    	
		/*{{
			CRef<CSeq_annot> Old(new CSeq_annot), New(new CSeq_annot), Blast(new CSeq_annot);
			Blast->SetTitle("Blast Aligner");
			Blast->AddName("Blast Aligner");
			Old->SetTitle("Old Merge Aligner");
			Old->AddName("Old Merge Aligner");
			New->SetTitle("New Merge Aligner");
			New->AddName("New Merge Aligner");
			ITERATE(CSeq_align_set::Tdata, AlignIter, Alignments->Get()) {
				int BlastScore=0, OldScore=0, NewScore=0;
				(*AlignIter)->GetNamedScore("blast_aligner", BlastScore);
				(*AlignIter)->GetNamedScore("merge_aligner", OldScore);
				(*AlignIter)->GetNamedScore("new_merge_aligner", NewScore);
				
				if(BlastScore)
					Blast->SetData().SetAlign().push_back(*AlignIter);
				else if(OldScore)
					Old->SetData().SetAlign().push_back(*AlignIter);
				else if(NewScore)
					New->SetData().SetAlign().push_back(*AlignIter);
			}
			CNcbiOstream& ostr = args["o"].AsOutputFile();
			ostr << MSerial_AsnText << *Old;
			ostr << MSerial_AsnText << *New;
			ostr << MSerial_AsnText << *Blast;
			return 1;
		}}*/

		CNcbiOstream& ostr = args["o"].AsOutputFile();
		if(args["b"].HasValue() && args["b"].AsBoolean())
			ostr << MSerial_AsnBinary << *Alignments;
		else
			ostr << MSerial_AsnText << *Alignments;
		
		if(args["nohit"].HasValue()) {
			x_PrintNoHitList(args["nohit"].AsOutputFile(), *Alignments);
		}
	} else {
		cerr << "No alignments found." << endl;
		return 1;
	}

    return 0;
}


bool CNgAlignApp::x_CreateNgAlignRun(CNgAligner& NgAligner, IRegistry* RunRegistry)
{
	CRef<ISequenceSet> Query, Subject;
	Query = x_CreateSequenceSet(RunRegistry, "query");
	Subject = x_CreateSequenceSet(RunRegistry, "subject");

	if(Query.IsNull() || Subject.IsNull())
		return false;

	NgAligner.SetQuery(Query);
	NgAligner.SetSubject(Subject);

	x_AddScorers(NgAligner, RunRegistry);
	x_AddFilters(NgAligner, RunRegistry);
	
	x_AddAligners(NgAligner, RunRegistry);

	return true;
}


CRef<ISequenceSet> 
CNgAlignApp::x_CreateSequenceSet(IRegistry* RunRegistry, 
								const string& Category)
{
    CRef<ISequenceSet> Result;
	const string Type = RunRegistry->Get(Category, "type");

	const string Source = RunRegistry->Get(Category, "source");
    
    const string Mask = RunRegistry->Get(Category, "mask");
    CSeqMasker* Masker = NULL;
    if(!Mask.empty()) {
        string NMer = "/panfs/pan1/gpipe/ThirdParty/WindowMasker/data/" + Mask;
        Masker = new CSeqMasker(NMer,
                        0, 1, 1, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, 0, "mean", 0, false, 0, false);
    }

    const CArgs& Args = GetArgs();


	if(Type == "seqidlist") {
		CRef<CSeqIdListSet> IdList(new CSeqIdListSet);

		if(Args["query"].HasValue() && Source.empty() && Category == "query") {
			const string& Id = Args["query"].AsString();
			CRef<CSeq_id> QueryId(new CSeq_id(Id));
			IdList->SetIdList().push_back(QueryId);
		}
		if(Args["subject"].HasValue() && Source.empty() && Category == "subject") {
			const string& Id = Args["subject"].AsString();
			CRef<CSeq_id> SubjectId(new CSeq_id(Id));
			IdList->SetIdList().push_back(SubjectId);
		}	
		if(Args["qidlist"].HasValue() && Source.empty() && Category == "query") {
			CNcbiIstream& In = Args["qidlist"].AsInputFile();
			CStreamLineReader Reader(In);
			while(!Reader.AtEOF()) {
				++Reader;
				string Line = *Reader;
				if(!Line.empty() && Line[0] != '#') {
					Line = Line.substr(0, Line.find(" "));
					CRef<CSeq_id> QueryId(new CSeq_id(Line));
					IdList->SetIdList().push_back(QueryId);
				}
			}
		}
		if(Args["sidlist"].HasValue() && Source.empty() && Category == "subject") {
			CNcbiIstream& In = Args["sidlist"].AsInputFile();
			CStreamLineReader Reader(In);
			while(!Reader.AtEOF()) {
				++Reader;
				string Line = *Reader;
				if(!Line.empty() && Line[0] != '#') {
					Line = Line.substr(0, Line.find(" "));
					CRef<CSeq_id> SubjectId(new CSeq_id(Line));
					IdList->SetIdList().push_back(SubjectId);
				}
			}
		}

		if(Args["idlist"].HasValue() && Source.empty() && m_LoadedIds.empty()) {
			CNcbiIstream& In = Args["idlist"].AsInputFile();
			CStreamLineReader Reader(In);
			while(!Reader.AtEOF()) {
				++Reader;
				string Line = *Reader;
				if(!Line.empty() && Line[0] != '#') {
					Line = Line.substr(0, Line.find(" "));
					CRef<CSeq_id> QueryId(new CSeq_id(Line));
					m_LoadedIds.push_back(QueryId);
					//IdList->SetIdList().push_back(QueryId);
				}
			}
			LoadedIdsIter = m_LoadedIds.begin();
		}
	
        cerr << __LINE__ << endl;
		if(IdList->SetIdList().empty() && !m_LoadedIds.empty()) {
			int BatchSize = Args["batch"].AsInteger();
			for(int Count = 0; 
				Count < BatchSize && LoadedIdsIter != m_LoadedIds.end(); 
				Count++) {
			cerr << __LINE__ << (*LoadedIdsIter)->AsFastaString() << endl;
                IdList->SetIdList().push_back(*LoadedIdsIter);
				++LoadedIdsIter;
			}
		}
        
        if(Masker != NULL)
            IdList->SetSeqMasker(Masker);

		Result.Reset(IdList.GetPointer());
	}
    else if(Type == "seqloclist") {
        CRef<CSeqLocListSet> LocList(new CSeqLocListSet);
    
        if(Args["query"].HasValue() && Source.empty() && Category == "query" ) {
			string QueryString = Args["query"].AsString();
			//CStreamLineReader Reader(In);
			//while(!Reader.AtEOF()) {
			//	++Reader;
				string Line = QueryString;
				if(!Line.empty() && Line[0] != '#') {
					vector<string> Tokens;
                    NStr::Tokenize(Line, "\t _.", Tokens, NStr::eMergeDelims);
                    CRef<CSeq_loc> Loc(new CSeq_loc);
                    
                    Loc->SetInt().SetId().Set(Tokens[0]);
                    if(Loc->GetInt().GetId().IsGi() && Loc->GetInt().GetId().GetGi() < 50) {
                        Loc->SetInt().SetId().SetLocal().SetId() = NStr::StringToInt(Tokens[0]);
                    }

                    Loc->SetInt().SetFrom() = NStr::StringToInt(Tokens[1])-1;
                    Loc->SetInt().SetTo() = NStr::StringToInt(Tokens[2])-1;
				    LocList->SetLocList().push_back(Loc);
                }
			//}
		}
        else if(Args["qloclist"].HasValue() && Source.empty() && Category == "query" ) {
			CNcbiIstream& In = Args["qloclist"].AsInputFile();
			CStreamLineReader Reader(In);
			while(!Reader.AtEOF()) {
				++Reader;
				string Line = *Reader;
				if(!Line.empty() && Line[0] != '#') {
					vector<string> Tokens;
                    NStr::Tokenize(Line, "\t _.", Tokens, NStr::eMergeDelims);
                    CRef<CSeq_loc> Loc(new CSeq_loc);
                    
                    Loc->SetInt().SetId().Set(Tokens[0]);
                    if(Loc->GetInt().GetId().IsGi() && Loc->GetInt().GetId().GetGi() < 50) {
                        Loc->SetInt().SetId().SetLocal().SetId() = NStr::StringToInt(Tokens[0]);
                    }

                    Loc->SetInt().SetFrom() = NStr::StringToInt(Tokens[1])-1;
                    Loc->SetInt().SetTo() = NStr::StringToInt(Tokens[2])-1;
				    LocList->SetLocList().push_back(Loc);
                }
			}
		}

        else if(Args["subject"].HasValue() && Source.empty() && Category == "subject" ) {
			string SubjectString = Args["subject"].AsString();
			//CStreamLineReader Reader(In);
			//while(!Reader.AtEOF()) {
			//	++Reader;
				string Line = SubjectString;
				if(!Line.empty() && Line[0] != '#') {
					vector<string> Tokens;
                    NStr::Tokenize(Line, "\t _.", Tokens, NStr::eMergeDelims);
                    CRef<CSeq_loc> Loc(new CSeq_loc);
                    Loc->SetInt().SetId().Set(Tokens[0]);
                    if(Loc->GetInt().GetId().IsGi() && Loc->GetInt().GetId().GetGi() < 50) {
                        Loc->SetInt().SetId().SetLocal().SetId() = NStr::StringToInt(Tokens[0]);
                    }

                    Loc->SetInt().SetFrom() = NStr::StringToInt(Tokens[1])-1;
                    Loc->SetInt().SetTo() = NStr::StringToInt(Tokens[2])-1;
				    LocList->SetLocList().push_back(Loc);
                }
			//}
		}

        else if(Args["sloclist"].HasValue() && Source.empty() && Category == "subject" ) {
			CNcbiIstream& In = Args["sloclist"].AsInputFile();
			CStreamLineReader Reader(In);
			while(!Reader.AtEOF()) {
				++Reader;
				string Line = *Reader;
				if(!Line.empty() && Line[0] != '#') {
					vector<string> Tokens;
                    NStr::Tokenize(Line, "\t _.", Tokens, NStr::eMergeDelims);
                    CRef<CSeq_loc> Loc(new CSeq_loc);
                    Loc->SetInt().SetId().Set(Tokens[0]);
                    if(Loc->GetInt().GetId().IsGi() && Loc->GetInt().GetId().GetGi() < 50) {
                        Loc->SetInt().SetId().SetLocal().SetId() = NStr::StringToInt(Tokens[0]);
                    }

                    Loc->SetInt().SetFrom() = NStr::StringToInt(Tokens[1])-1;
                    Loc->SetInt().SetTo() = NStr::StringToInt(Tokens[2])-1;
				    LocList->SetLocList().push_back(Loc);
                }
			}
		}

        if(Masker != NULL)
            LocList->SetSeqMasker(Masker);

		Result.Reset(LocList.GetPointer());
    }
	else if(Type == "blastdb") {
		CRef<CBlastDbSet> BlastDb(new CBlastDbSet(Args["blastdb"].AsString()));
		if(Args["softfilter"].HasValue()) {
			BlastDb->SetSoftFiltering(Args["softfilter"].AsInteger());
		}
		Result.Reset(BlastDb.GetPointer());
	} 
	else if(Type == "fasta") {
		string FileName;
		FileName = RunRegistry->Get(Category, "fasta");
		if(Args["fasta"].HasValue())
			FileName = Args["fasta"].AsString();
        int Batch = -1;
        if(Args["batch"].HasValue())
            Batch = Args["batch"].AsInteger();
		ERR_POST(Info << "Fasta File Name: " << FileName );
		// yes, its being leaked.
		CNcbiIfstream* FastaIn = new CNcbiIfstream(FileName.c_str());
		CRef<CFastaFileSet> FastaSet;
        if(Batch != -1) {
            FastaSet.Reset(new CFastaFileSet(FastaIn, BatchCounter, Batch));
            BatchCounter += Batch;
            for(int i = 0; i < Batch; i++) {
                if(LoadedIdsIter != m_LoadedIds.end())
                    ++LoadedIdsIter;
            }
        } else { 
            FastaSet.Reset(new CFastaFileSet(FastaIn));
		}
        Result.Reset(FastaSet.GetPointer());
	}
	else if(Type == "splitseqidlist") {
		CRef<CSplitSeqIdListSet> IdList(new CSplitSeqIdListSet(m_Splitter.get()));

		if(Args["query"].HasValue()) {
			const string& Id = Args["query"].AsString();
			CRef<CSeq_id> QueryId(new CSeq_id(Id));
			IdList->AddSeqId(QueryId);
		}		
		
        /*if(Args["idlist"].HasValue()) {
			CNcbiIstream& In = Args["idlist"].AsInputFile();
			CStreamLineReader Reader(In);
			while(!Reader.AtEOF()) {
				++Reader;
				string Line = *Reader;
				if(!Line.empty() && Line[0] != '#') {
					Line = Line.substr(0, Line.find(" "));
					CRef<CSeq_id> QueryId(new CSeq_id(Line));
					IdList->AddSeqId(QueryId);
				}
			}
		}*/

        if(Args["idlist"].HasValue() && Source.empty() && m_LoadedIds.empty()) {
			CNcbiIstream& In = Args["idlist"].AsInputFile();
			CStreamLineReader Reader(In);
			while(!Reader.AtEOF()) {
				++Reader;
				string Line = *Reader;
				if(!Line.empty() && Line[0] != '#') {
					Line = Line.substr(0, Line.find(" "));
					CRef<CSeq_id> QueryId(new CSeq_id(Line));
					m_LoadedIds.push_back(QueryId);
					//IdList->SetIdList().push_back(QueryId);
				}
			}
			LoadedIdsIter = m_LoadedIds.begin();
		}
	
        cerr << __LINE__ << endl;
		if(IdList->Empty() && !m_LoadedIds.empty()) {
			int BatchSize = Args["batch"].AsInteger();
			for(int Count = 0; 
				Count < BatchSize && LoadedIdsIter != m_LoadedIds.end(); 
				Count++) {
			cerr << __LINE__ << (*LoadedIdsIter)->AsFastaString() << endl;
				IdList->AddSeqId(*LoadedIdsIter);
                //IdList->SetIdList().push_back(*LoadedIdsIter);
				++LoadedIdsIter;
			}
		}

		
        if(IdList->Empty()) {
            ERR_POST(Error << " Split Seq Id List is empty, maybe all gap? ");
        }
        
        if(Masker != NULL)
            IdList->SetSeqMasker(Masker);

		Result.Reset(IdList.GetPointer());
	}
    else if(Type == "splitseqloclist") {
		CRef<CSplitSeqLocListSet> LocList(new CSplitSeqLocListSet(m_Splitter.get()));
		
        if(Args["qloclist"].HasValue() && Source.empty() && Category == "query" ) {
			CNcbiIstream& In = Args["qloclist"].AsInputFile();
			CStreamLineReader Reader(In);
			while(!Reader.AtEOF()) {
				++Reader;
				string Line = *Reader;
				if(!Line.empty() && Line[0] != '#') {
					vector<string> Tokens;
                    NStr::Tokenize(Line, "\t ", Tokens, NStr::eMergeDelims);
                    CRef<CSeq_loc> Loc(new CSeq_loc);
                    if(Tokens.size() >= 3) {
                        Loc->SetInt().SetId().Set(Tokens[0]);
                        Loc->SetInt().SetFrom() = NStr::StringToInt(Tokens[1])-1;
                        Loc->SetInt().SetTo() = NStr::StringToInt(Tokens[2])-1;
                    } else {
                        Loc->SetWhole().Set(Tokens[0]);
                    }
                    LocList->AddSeqLoc(Loc);
                }
			}
		}

        if(Args["sloclist"].HasValue() && Source.empty() && Category == "subject" ) {
			CNcbiIstream& In = Args["sloclist"].AsInputFile();
			CStreamLineReader Reader(In);
			while(!Reader.AtEOF()) {
				++Reader;
				string Line = *Reader;
				if(!Line.empty() && Line[0] != '#') {
					vector<string> Tokens;
                    NStr::Tokenize(Line, "\t ", Tokens, NStr::eMergeDelims);
                    CRef<CSeq_loc> Loc(new CSeq_loc);
                    if(Tokens.size() >= 3) {
                        Loc->SetInt().SetId().Set(Tokens[0]);
                        Loc->SetInt().SetFrom() = NStr::StringToInt(Tokens[1])-1;
                        Loc->SetInt().SetTo() = NStr::StringToInt(Tokens[2])-1;
                    } else {
                        Loc->SetWhole().Set(Tokens[0]);
                    }
                    LocList->AddSeqLoc(Loc);
                }
			}
		}
        if(LocList->Empty()) {
            ERR_POST(Error << " Split Seq Loc List is empty, maybe all gap? ");
        }
        
        if(Masker != NULL)
            LocList->SetSeqMasker(Masker);

		Result.Reset(LocList.GetPointer());
	}

	return Result;
}


void CNgAlignApp::x_LoadExternalSequences(IRegistry* RunRegistry, 
								const string& Category,
								list<CRef<CSeq_id> >& LoadedIds)
{
    const CArgs& Args = GetArgs();
    
    if(Args["fasta"].HasValue()) {
        CNcbiIstream& In = Args["fasta"].AsInputFile();
        CFastaReader FastaReader(In);
       
        while(!FastaReader.AtEOF()) {
            CRef<CSeq_entry> Entry;
            try{
                Entry = FastaReader.ReadOneSeq();
            } catch(...) {
                break;
            }
            m_Scope->AddTopLevelSeqEntry(*Entry);
			LoadedIds.push_back( Entry->GetSeq().GetId().front() );
        }
        cerr << __LINE__ << "\t" << LoadedIds.size() << endl;
    }


	if(Args["seqentry"].HasValue()) {
		CNcbiIstream& In = Args["seqentry"].AsInputFile();

		while(!In.eof()) {
			CRef<CSeq_entry> Top(new CSeq_entry);
			
			size_t Pos = In.tellg();
			try {
				In >> MSerial_AsnText >> *Top;
			} catch(...) {
				if(In.eof())
					continue;
				In.clear();
				In.seekg(Pos);
				try {	
					In >> MSerial_AsnBinary >> *Top;
				} catch(...) {
					if(!In.eof()) 
						cerr << "Read Failure" << endl;
				}
			}
			if(In.eof())
				continue;

			x_RecurseSeqEntry(Top, LoadedIds);
		}
	}

	if(Args["agp"].HasValue()) {
		CNcbiIstream& In = Args["agp"].AsInputFile();

		vector< CRef< CSeq_entry > > SeqEntries;
	    try {
            CAgpToSeqEntry Reader(CAgpToSeqEntry::fSetSeqGap );
            Reader.SetVersion( eAgpVersion_1_1);
            Reader.ReadStream(In);
            SeqEntries.clear();
            SeqEntries = Reader.GetResult();
			//AgpRead(In, SeqEntries, eAgpRead_ParseId, true);
		} catch(CException& e) {
			cerr << "AgpRead Exception: " << e.ReportAll() << endl;
		}

		ITERATE(vector<CRef<CSeq_entry> >, SeqEntryIter, SeqEntries) {
			//cerr << "Loading AGP: " << (*SeqEntryIter)->GetSeq().GetId().front()->AsFastaString() << endl;
            m_Scope->AddTopLevelSeqEntry(**SeqEntryIter);
			LoadedIds.push_back( (*SeqEntryIter)->GetSeq().GetId().front() );
		}
	}
    
}


void CNgAlignApp::x_RecurseSeqEntry(CRef<CSeq_entry> Top, 
									list<CRef<CSeq_id> >& SeqIdList)
{
	if(Top->IsSet()) {
		int Loop = 0;
		ITERATE(CBioseq_set::TSeq_set, SeqIter, Top->GetSet().GetSeq_set()) {
			//if(Loop > 1927)
			//	break;
			//if(Loop > 1900)
				x_RecurseSeqEntry(*SeqIter, SeqIdList);
			Loop++;
		}
	} else if(Top->IsSeq()) {
		CRef<CSeq_id> Id;
		Id = Top->GetSeq().GetId().front();
		SeqIdList.push_back(Id);
		try {
			m_Scope->AddTopLevelSeqEntry(*Top);
		} catch(...) { ; }
	}
}


void CNgAlignApp::x_AddScorers(CNgAligner& NgAligner, IRegistry* RunRegistry)
{
	string ScorerNames; 
    
    if(GetArgs()["scorers"].HasValue())
        ScorerNames = GetArgs()["scorers"].AsString();
    else
        ScorerNames = RunRegistry->Get("scorers", "names");
	
    vector<string> Names;
	NStr::Tokenize(ScorerNames, " \t;", Names);
	
	ITERATE(vector<string>, NameIter, Names) {

		if(*NameIter == "blast")
			NgAligner.AddScorer(
                new CBlastScorer(CBlastScorer::eSkipUnsupportedAlignments));
		else if(*NameIter == "pctident")
			NgAligner.AddScorer(new CPctIdentScorer);
		else if(*NameIter == "pctcov")
			NgAligner.AddScorer(new CPctCoverageScorer);
		else if(*NameIter == "comcomp")
			NgAligner.AddScorer(new CCommonComponentScorer);
		else if(*NameIter == "expansion")
			NgAligner.AddScorer(new CExpansionScorer);
		else if(*NameIter == "weighted") {
            double K = 0.04;
            K = RunRegistry->GetDouble("scorers", "sw_cvg", 0.04);
			NgAligner.AddScorer(new CWeightedIdentityScorer(K));
		}
        else if(*NameIter == "hang")
			NgAligner.AddScorer(new CHangScorer);
		else if(*NameIter == "overlap")
			NgAligner.AddScorer(new COverlapScorer);
		else if(*NameIter == "clip") {
	        double K = 0.04;
            K = RunRegistry->GetDouble("scorers", "sw_cvg", 0.04);
            NgAligner.AddScorer(new CClippedScorer(K));
        }

	}
}


void CNgAlignApp::x_AddFilters(CNgAligner& NgAligner, IRegistry* RunRegistry)
{
    const CArgs& args = GetArgs();
	
    int Rank = 0;
    
    if(args["filters"].HasValue()) {        
        string OrigFilters = args["filters"].AsString();
        vector<string> SplitFilters;
        NStr::Tokenize(OrigFilters, ";", SplitFilters);
        ITERATE(vector<string>, FilterIter, SplitFilters) {
            const string FilterStr = *FilterIter;
            try {
                CRef<CQueryFilter> Filter(new CQueryFilter(Rank, FilterStr));
                Rank++;
                NgAligner.AddFilter(Filter);
            } catch( CException& e) {
                cerr << "x_AddFilters" << "  :  "  << Rank << "  :  " << FilterStr << endl;
                cerr << e.ReportAll() << endl;
            }
        }
    } else {    
        string FilterNames = RunRegistry->Get("filters", "names");
        vector<string> Names;
        NStr::Tokenize(FilterNames, " \t", Names);
        ITERATE(vector<string>, NameIter, Names) {
            string FilterStr = RunRegistry->Get("filters", *NameIter);
            try {
                CRef<CQueryFilter> Filter(new CQueryFilter(Rank, FilterStr));
                Rank++;
                NgAligner.AddFilter(Filter);
            } catch( CException& e) {
                cerr << "x_AddFilters" << "  :  "  << *NameIter << "  :  " << FilterStr << endl;
                cerr << e.ReportAll() << endl;
            }
        }
    }
}


void CNgAlignApp::x_AddAligners(CNgAligner& NgAligner, IRegistry* RunRegistry)
{
	string FilterNames = RunRegistry->Get("aligners", "names");
	vector<string> Names;
	NStr::Tokenize(FilterNames, " \t", Names);

	ITERATE(vector<string>, NameIter, Names) {
		
		string Type = RunRegistry->Get(*NameIter, "type");
	
		CRef<IAlignmentFactory> Aligner;
		if(Type == "blast")
			Aligner = x_CreateBlastAligner(RunRegistry, *NameIter);
		else if(Type == "remote_blast")
			Aligner = x_CreateRemoteBlastAligner(RunRegistry, *NameIter);
		else if(Type == "merge")
			Aligner = x_CreateMergeAligner(RunRegistry, *NameIter);
		//else if(Type == "coverage")
		//	Aligner = x_CreateCoverageAligner(RunRegistry, *NameIter);
		else if(Type == "mergetree")
			Aligner = x_CreateMergeTreeAligner(RunRegistry, *NameIter);
		else if(Type == "inversion")
			Aligner = x_CreateInversionMergeAligner(RunRegistry, *NameIter);
		else if(Type == "split")
			Aligner = x_CreateSplitSeqMergeAligner(RunRegistry, *NameIter);

		if(!Aligner.IsNull())
			NgAligner.AddAligner(Aligner);
	}
}


CRef<CBlastAligner> 
CNgAlignApp::x_CreateBlastAligner(IRegistry* RunRegistry, const string& Name)
{
	string Params = RunRegistry->Get(Name, "params");
	int Threshold = RunRegistry->GetInt(Name, "threshold", 0);
	int Filter = RunRegistry->GetInt(Name, "filter", -1);
    bool UseNegatives = RunRegistry->GetBool(Name, "useneg", true);

	const CArgs& Args = GetArgs();	
	
	
	if(Args["softfilter"].HasValue())
		Filter = Args["softfilter"].AsInteger();

	

	CRef<CBlastAligner> Blaster;
	Blaster.Reset(new CBlastAligner(Params, Threshold));

	//if(Filter != -1)
		Blaster->SetSoftFiltering(Filter);

    Blaster->SetUseNegativeGiList(UseNegatives);

	return Blaster;
}


CRef<CRemoteBlastAligner> 
CNgAlignApp::x_CreateRemoteBlastAligner(IRegistry* RunRegistry, const string& Name)
{
	string Params = RunRegistry->Get(Name, "params");
	int Threshold = RunRegistry->GetInt(Name, "threshold", 0);
	//int Filter = RunRegistry->GetInt(Name, "filter", -1);

	const CArgs& Args = GetArgs();	
	

	CRef<CRemoteBlastAligner> Blaster;
	Blaster.Reset(new CRemoteBlastAligner(Params, Threshold));

	//if(Filter != -1)
	//	Blaster->SetSoftFiltering(Filter);

	return Blaster;
}


CRef<CMergeAligner> 
CNgAlignApp::x_CreateMergeAligner(IRegistry* RunRegistry, const string& Name)
{
	int Threshold = RunRegistry->GetInt(Name, "threshold", 0);
	double Clip = RunRegistry->GetDouble(Name, "clip", 3.0);
	return CRef<CMergeAligner>(new CMergeAligner(Threshold/*, Clip*/));
}

/*CRef<CCoverageAligner> 
CNgAlignApp::x_CreateCoverageAligner(IRegistry* RunRegistry, const string& Name)
{
	int Threshold = RunRegistry->GetInt(Name, "threshold", 0);
	return CRef<CCoverageAligner>(new CCoverageAligner(Threshold));
}*/

CRef<CMergeTreeAligner> 
CNgAlignApp::x_CreateMergeTreeAligner(IRegistry* RunRegistry, const string& Name)
{
	int Threshold = RunRegistry->GetInt(Name, "threshold", 0);
	
    CMergeTree::SScoring Scoring; 
    string ScoringStr = RunRegistry->GetString(Name, "scoring", "");
    if(!ScoringStr.empty()) {
        vector<string> Tokens;
        NStr::Tokenize(ScoringStr, ",", Tokens);
        if(Tokens.size() == 4) {
            Scoring.Match     = NStr::StringToInt(Tokens[0]);
            Scoring.MisMatch  = NStr::StringToInt(Tokens[1]);
            Scoring.GapOpen   = NStr::StringToInt(Tokens[2]);
            Scoring.GapExtend = NStr::StringToInt(Tokens[3]);
        }
    }
	
    return CRef<CMergeTreeAligner>(new CMergeTreeAligner(Threshold, Scoring));
}


CRef<CInversionMergeAligner> 
CNgAlignApp::x_CreateInversionMergeAligner(IRegistry* RunRegistry, const string& Name)
{
	int Threshold = RunRegistry->GetInt(Name, "threshold", 0);
	return CRef<CInversionMergeAligner>(new CInversionMergeAligner(Threshold));
}


CRef<CSplitSeqAlignMerger> 
CNgAlignApp::x_CreateSplitSeqMergeAligner(IRegistry* RunRegistry, const string& Name)
{
	return CRef<CSplitSeqAlignMerger>(new CSplitSeqAlignMerger(m_Splitter.get()));
}



CRef<CGC_Assembly> CNgAlignApp::x_LoadAssembly(CNcbiIstream& In)
{
	CRef<CGC_Assembly> Assembly(new CGC_Assembly);

	try {
		In >> MSerial_AsnText >> *Assembly;
	} catch(...) {
		In.clear();
		In.seekg(0);
		try {	
			In >> MSerial_AsnBinary >> *Assembly;
		} catch(...) {
			Assembly.Reset(NULL);
		}
	}

	return Assembly;
}


void CNgAlignApp::x_PrintNoHitList(CNcbiOstream& Out, const CSeq_align_set& Alignments)
{
	set<CSeq_id_Handle> GivenIds;

	ITERATE(list<CRef<CSeq_id> >, IdIter, m_LoadedIds) {
		CSeq_id_Handle Handle = CSeq_id_Handle::GetHandle(**IdIter);
		GivenIds.insert(Handle);
	}

	ITERATE(CSeq_align_set::Tdata, AlignIter, Alignments.Get()) {
		CSeq_id_Handle AlignIdHandle = CSeq_id_Handle::GetHandle((*AlignIter)->GetSeq_id(0));
		set<CSeq_id_Handle>::iterator Found = GivenIds.find(AlignIdHandle);
		if(Found != GivenIds.end())
			GivenIds.erase(Found);
	}

	ITERATE(set<CSeq_id_Handle>, IdIter, GivenIds) {
		Out << IdIter->AsString() << endl;
	}

}


END_NCBI_SCOPE
USING_SCOPE(ncbi);


int main(int argc, char** argv)
{
    return CNgAlignApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
