/* $Id$
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
 * Author:  Yuri Kapustin
 *
 * File Description: Splign application
 *                   
*/

#include <ncbi_pch.hpp>
#include "splign_app.hpp"
#include "splign_app_exception.hpp"

#include <corelib/ncbistd.hpp>
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>

#include <algo/align/nw/nw_spliced_aligner16.hpp>
#include <algo/align/nw/nw_spliced_aligner32.hpp>
#include <algo/align/splign/splign.hpp>
#include <algo/align/splign/splign_simple.hpp>
#include <algo/align/splign/splign_formatter.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <algo/blast/api/bl2seq.hpp>

#include <iostream>
#include <memory>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


const char kQuality_high[] = "high";
const char kQuality_low[] = "low";

const char kStrandPlus[] = "plus";
const char kStrandMinus[] = "minus";
const char kStrandBoth[] = "both";

void CSplignApp::Init()
{
  HideStdArgs( fHideLogfile | fHideConffile | fHideVersion);
  
  auto_ptr<CArgDescriptions> argdescr(new CArgDescriptions);

  string program_name ("Splign v.1.09");
#ifdef GENOME_PIPELINE
  program_name += 'p';
#endif
  argdescr->SetUsageContext(GetArguments().GetProgramName(), program_name);

  argdescr->AddOptionalKey
    ("index", "index",
     "Batch mode index file (use -mkidx to generate).",
     CArgDescriptions::eString);

  argdescr->AddFlag ("mkidx", "Generate batch mode index and quit.", true);
  
  argdescr->AddOptionalKey
    ("hits", "hits",
     "Batch mode hit file. "
     "This file defines the set of sequences to align and "
     "is also used to guide alignments.",
     CArgDescriptions::eString);

  argdescr->AddOptionalKey
    ("query", "query",
     "FastA file with the spliced sequence. Use in pairwise mode only.",
     CArgDescriptions::eString);

  argdescr->AddOptionalKey
    ("subj", "subj",
     "FastA file with the genomic sequence. Use in pairwise mode only.",
     CArgDescriptions::eString);

  argdescr->AddDefaultKey
    ("log", "log", "Splign log file",
     CArgDescriptions::eString, "splign.log");

  argdescr->AddOptionalKey
    ("asn", "asn", "ASN.1 output file name", CArgDescriptions::eString);

  argdescr->AddDefaultKey
    ("strand", "strand", "Spliced sequence's strand.",
     CArgDescriptions::eString, kStrandPlus);

  argdescr->AddFlag ("noendgaps",
		     "Skip detection of unaligning regions at the ends.",
                      true);

  argdescr->AddFlag ("nopolya", "Assume no Poly(A) tail.",  true);

  argdescr->AddDefaultKey
    ("compartment_penalty", "compartment_penalty",
     "Penalty to open a new compartment.",
     CArgDescriptions::eDouble,
     "0.75");
 
 argdescr->AddDefaultKey
    ("min_query_cov", "min_query_cov",
     "Min query coverage by initial Blast hits. "
     "Queries with less coverage will not be aligned.",
     CArgDescriptions::eDouble, "0.25");

  argdescr->AddDefaultKey
    ("min_idty", "identity",
     "Minimal exon identity. Lower identity segments "
     "will be marked as gaps.",
     CArgDescriptions::eDouble, "0.75");

#ifdef GENOME_PIPELINE

  argdescr->AddFlag ("mt", "Use multiple threads (up to CPU count)", true);

  argdescr->AddDefaultKey
    ("quality", "quality", "Genomic sequence quality.",
     CArgDescriptions::eString, kQuality_high);
  
  argdescr->AddDefaultKey
    ("Wm", "match", "match score",
     CArgDescriptions::eInteger,
     NStr::IntToString(CNWAligner::GetDefaultWm()).c_str());
  
  argdescr->AddDefaultKey
    ("Wms", "mismatch", "mismatch score",
     CArgDescriptions::eInteger,
     NStr::IntToString(CNWAligner::GetDefaultWms()).c_str());
  
  argdescr->AddDefaultKey
    ("Wg", "gap", "gap opening score",
     CArgDescriptions::eInteger,
     NStr::IntToString(CNWAligner::GetDefaultWg()).c_str());
  
  argdescr->AddDefaultKey
    ("Ws", "space", "gap extension (space) score",
     CArgDescriptions::eInteger,
     NStr::IntToString(CNWAligner::GetDefaultWs()).c_str());
  
  argdescr->AddDefaultKey
    ("Wi0", "Wi0", "Conventional intron (GT/AG) score",
     CArgDescriptions::eInteger,
     NStr::IntToString(CSplicedAligner16::GetDefaultWi(0)).c_str());
  
  argdescr->AddDefaultKey
    ("Wi1", "Wi1", "Conventional intron (GC/AG) score",
     CArgDescriptions::eInteger,
     NStr::IntToString(CSplicedAligner16::GetDefaultWi(1)).c_str());
  
  argdescr->AddDefaultKey
    ("Wi2", "Wi2", "Conventional intron (AT/AC) score",
     CArgDescriptions::eInteger,
     NStr::IntToString(CSplicedAligner16::GetDefaultWi(2)).c_str());
  
  argdescr->AddDefaultKey
    ("Wi3", "Wi3", "Arbitrary intron score",
     CArgDescriptions::eInteger,
     NStr::IntToString(CSplicedAligner16::GetDefaultWi(3)).c_str());
  
  // restrictions

  CArgAllow_Strings* constrain_errlevel = new CArgAllow_Strings;
  constrain_errlevel->Allow(kQuality_low)->Allow(kQuality_high);
  argdescr->SetConstraint("quality", constrain_errlevel);
  
#endif
    
  CArgAllow_Strings* constrain_strand = new CArgAllow_Strings;
  constrain_strand->Allow(kStrandPlus)->Allow(kStrandMinus)
      ->Allow(kStrandBoth);
  argdescr->SetConstraint("strand", constrain_strand);

  CArgAllow* constrain01 = new CArgAllow_Doubles(0,1);
  argdescr->SetConstraint("min_idty", constrain01);
  argdescr->SetConstraint("compartment_penalty", constrain01);
  argdescr->SetConstraint("min_query_cov", constrain01);
    
  SetupArgDescriptions(argdescr.release());
}


bool CSplignApp::x_GetNextPair(istream* ifs, vector<CHit>* hits)
{
  hits->clear();
  if(!m_pending.size() && (!ifs || !*ifs) ) {
    return false;
  }

  if(!m_pending.size()) {

    string query, subj;
    if(m_firstline.size()) {
      CHit hit (m_firstline.c_str());
      query = hit.m_Query;
      subj  = hit.m_Subj;
      m_pending.push_back(hit);
    }

    char buf [1024];
    while(ifs) {
      buf[0] = 0;
      CT_POS_TYPE pos0 = ifs->tellg();
      ifs->getline(buf, sizeof buf, '\n');
      CT_POS_TYPE pos1 = ifs->tellg();
      if(pos1 == pos0) break; // GCC hack
      if(buf[0] == '#') continue; // skip comments
      const char* p = buf; // skip leading spaces
      while(*p == ' ' || *p == '\t') ++p;
      if(*p == 0) continue; // skip empty lines

      CHit hit (p);
      if(query.size() == 0) {
	query = hit.m_Query;
      }
      if(subj.size() == 0) {
	subj = hit.m_Subj;
      }
      if(hit.m_ai[0] > hit.m_ai[1]) {
        NCBI_THROW(CSplignAppException,
                   eBadData,
                   "Hit with reversed query coordinates detected." );
      }
      if(hit.m_ai[2] == hit.m_ai[3]) { // skip single bases
        continue;
      }

      if(hit.m_Query != query || hit.m_Subj != subj) {
	m_firstline = p;
	break;
      }

      m_pending.push_back(hit);
    }
  }

  const size_t pending_size = m_pending.size();
  if(pending_size) {
    const string& query = m_pending[0].m_Query;
    const string& subj  = m_pending[0].m_Subj;
    size_t i = 1;
    for(; i < pending_size; ++i) {
      const CHit& h = m_pending[i];
      if(h.m_Query != query || h.m_Subj != subj) {
        break;
      }
    }
    hits->resize(i);
    copy(m_pending.begin(), m_pending.begin() + i, hits->begin());
    m_pending.erase(m_pending.begin(), m_pending.begin() + i);
  }

  return hits->size() > 0;
}


void CSplignApp::x_LogStatus(size_t model_id, bool query_strand,
                             const string& query, const string& subj,
			     bool error, const string& msg)
{
    string error_tag (error? "Error: ": "");
    if(model_id == 0) {
        m_logstream << '-';
    }
    else {
        m_logstream << (query_strand? '+': '-') << model_id;
    }
    
    m_logstream << '\t' << query << '\t' << subj 
                << '\t' << error_tag << msg << endl;
}


istream* CSplignApp::x_GetPairwiseHitStream(
                         CSeqLoaderPairwise& seq_loader) const
{
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CRef<CSeq_loc> seqloc_query;
    CRef<CScope> scope(new CScope(*objmgr));
    scope->AddDefaults();
    {{
    CRef<CSeq_entry> se (seq_loader.GetQuerySeqEntry());
    scope->AddTopLevelSeqEntry(*se);
    seqloc_query.Reset(new CSeq_loc);
    seqloc_query->SetWhole().Assign(*(se->GetSeq().GetId().front()));
    }}

    CRef<CSeq_loc> seqloc_subj;
    {{
    CRef<CSeq_entry> se (seq_loader.GetSubjSeqEntry());
    scope->AddTopLevelSeqEntry(*se);
    seqloc_subj.Reset(new CSeq_loc);
    seqloc_subj->SetWhole().Assign(*(se->GetSeq().GetId().front()));
    }}

    blast::CBl2Seq Blast(blast::SSeqLoc(seqloc_query.GetNonNullPointer(),
                                        scope.GetNonNullPointer()),
                         blast::SSeqLoc(seqloc_subj.GetNonNullPointer(),
                                        scope.GetNonNullPointer()),
                         blast::eMegablast);

    blast::TSeqAlignVector blast_output (Blast.Run());

    if (!blast_output.empty() && blast_output.front().NotEmpty() &&
        !blast_output.front()->Get().empty() &&
        !blast_output.front()->Get().front()->
          GetSegs().GetDisc().Get().empty()) {
        
        CNcbiOstrstream oss;

        const CSeq_align_set::Tdata &sas =
            blast_output.front()->Get().front()->GetSegs().GetDisc().Get();
        ITERATE(CSeq_align_set::Tdata, sa_iter, sas) {
            CHit hit (**sa_iter);
            oss << hit << endl;
        }
        string str = CNcbiOstrstreamToString(oss);
        CNcbiIstrstream* iss = new CNcbiIstrstream (str.c_str());
        return iss;        
    }
    else {
        return 0;
    }
}


int CSplignApp::Run()
{ 
  const CArgs& args = GetArgs();    

  // check that modes aren't mixed
  bool is_query = args["query"];
  bool is_subj = args["subj"];
  bool is_index = args["index"];
  bool is_hits = args["hits"];

  if(is_query ^ is_subj) {
      NCBI_THROW(CSplignAppException,
                 eBadParameter,
                 "Both query and subj must be specified in pairwise mode." );
  }
  
  if(is_query && (is_hits || is_index)) {
      NCBI_THROW(CSplignAppException,
                 eBadParameter,
                 "Do not use -hits or -index in pairwise mode "
                 "(i.e. with -query and -subj)." );
  }

  if(!is_query && !is_hits) {
      NCBI_THROW(CSplignAppException,
                 eBadParameter,
                 "Either -query or -hits must be specified (but not both)");
  }


  if(is_hits ^ is_index) {
      NCBI_THROW(CSplignAppException,
                 eBadParameter,
                 "When in batch mode, specify both -hits and -index");
  }
  
  bool mode_pairwise = is_query;

  // open log stream
  m_logstream.open( args["log"].AsString().c_str() );

  // open asn output stream, if any
  auto_ptr<ofstream> asn_ofs (0);
  if(args["asn"]) {
      const string filename = args["asn"].AsString();
      asn_ofs.reset(new ofstream (filename.c_str()));
      if(!*asn_ofs) {
          NCBI_THROW(CSplignAppException,
                     eCannotOpenFile,
                     "Cannot open output asn file." );
      }
  }

  // aligner setup

  string quality;

#ifndef GENOME_PIPELINE
  quality = kQuality_high;
#else
  quality = args["quality"].AsString();
#endif

  CRef<CSplicedAligner> aligner (  quality == kQuality_high ?
    static_cast<CSplicedAligner*> (new CSplicedAligner16):
    static_cast<CSplicedAligner*> (new CSplicedAligner32) );

#if GENOME_PIPELINE
  aligner->SetWm(args["Wm"].AsInteger());
  aligner->SetWms(args["Wms"].AsInteger());
  aligner->SetWg(args["Wg"].AsInteger());
  aligner->SetWs(args["Ws"].AsInteger());

  for(size_t i = 0, n = aligner->GetSpliceTypeCount(); i < n; ++i) {
      string arg_name ("Wi");
      arg_name += NStr::IntToString(i);
      aligner->SetWi(i, args[arg_name.c_str()].AsInteger());
  }

  aligner->EnableMultipleThreads(args["mt"]);
#endif

  // setup sequence loader

  CRef<CSplignSeqAccessor> seq_loader;
  if(mode_pairwise) {
      const string query_filename (args["query"].AsString());
      const string subj_filename  (args["subj"].AsString());
      CSeqLoaderPairwise* pseqloader = 
          new CSeqLoaderPairwise(query_filename, subj_filename);
      seq_loader.Reset(pseqloader);
  }
  else {
      CSeqLoader* pseqloader = new CSeqLoader;
      pseqloader->Open(args["index"].AsString());
      seq_loader.Reset(pseqloader);
  }

  // splign object setup

  CSplign splign;
  splign.SetPolyaDetection(!args["nopolya"]);
  splign.SetMinExonIdentity(args["min_idty"].AsDouble());
  splign.SetCompartmentPenalty(args["compartment_penalty"].AsDouble());
  splign.SetMinQueryCoverage(args["min_query_cov"].AsDouble());
  splign.SetEndGapDetection(!(args["noendgaps"]));
  splign.SetMaxGenomicExtension(75000);

  splign.SetAligner() = aligner;
  splign.SetSeqAccessor() = seq_loader;
  splign.SetStartModelId(1);

  // splign formatter object

  CSplignFormatter formatter (splign);

  // prepare input hit stream

  auto_ptr<istream> hit_stream;
  if(mode_pairwise) {
      CSeqLoaderPairwise* pseq_loader_pw = 
          static_cast<CSeqLoaderPairwise*>(seq_loader.GetNonNullPointer());
      hit_stream.reset(x_GetPairwiseHitStream(*pseq_loader_pw));
      if(hit_stream.get() == 0) {
          NCBI_THROW(CSplignAppException,
                     eNoHits,
                     "No hits returned from Blast with the default "
                     "set of parameters. "
                     "Try running Blast externally and run the batch mode.");
      }
  }
  else {
      const string filename (args["hits"].AsString());
      hit_stream.reset(new ifstream (filename.c_str()));
  }

  // iterate over input hits

  CSplign::THits hits;
  while(x_GetNextPair(hit_stream.get(), &hits) ) {

    if(hits.size() == 0) {
        continue;
    }

    const string query (hits[0].m_Query);
    CRef<CSeq_id> seqid_query (new CSeq_id (query));
    if (seqid_query->Which() == CSeq_id::e_not_set) {
        seqid_query->SetLocal().SetStr(query);
    }
    const string subj (hits[0].m_Subj);
    CRef<CSeq_id> seqid_subj (new CSeq_id (subj));
    if (seqid_subj->Which() == CSeq_id::e_not_set) {
        seqid_subj->SetLocal().SetStr(subj);
    }
    formatter.SetSeqIds(seqid_query, seqid_subj);

    const string strand = args["strand"].AsString();
    CSplign::TResults splign_results;
    if(strand == kStrandPlus) {

        splign.SetStrand(true);
        splign.Run(&hits);
        const CSplign::TResults& results = splign.GetResult();
        copy(results.begin(), results.end(), back_inserter(splign_results));
    }
    else if(strand == kStrandMinus) {

        splign.SetStrand(false);
        splign.Run(&hits);
        const CSplign::TResults& results = splign.GetResult();
        copy(results.begin(), results.end(), back_inserter(splign_results));
    }
    else { // kStrandBoth

        CSplign::THits hits0 (hits.begin(), hits.end());
        static size_t mid = 1;
        size_t mid_plus, mid_minus;
        {{
        splign.SetStrand(true);
        splign.SetStartModelId(mid);
        splign.Run(&hits);
        const CSplign::TResults& results = splign.GetResult();
        copy(results.begin(), results.end(), back_inserter(splign_results));
        mid_plus = splign.GetNextModelId();
        }}
        {{
        splign.SetStrand(false);
        splign.SetStartModelId(mid);
        splign.Run(&hits0);
        const CSplign::TResults& results = splign.GetResult();
        copy(results.begin(), results.end(), back_inserter(splign_results));
        mid_minus = splign.GetNextModelId();
        }}
        mid = max(mid_plus, mid_minus);
    }

    cout << formatter.AsText(&splign_results) << flush;

    if(asn_ofs.get()) {

        CRef<CSeq_align_set> sa_set = formatter.AsSeqAlignSet(&splign_results);
        auto_ptr<CObjectOStream> os(CObjectOStream::Open(eSerial_AsnText,
                                                         *asn_ofs));
        *os << *sa_set;
    }

    ITERATE(CSplign::TResults, ii, splign_results) {
      x_LogStatus(ii->m_id, ii->m_QueryStrand, query, subj,
                  ii->m_error, ii->m_msg);
    }

    if(splign_results.size() == 0) {
      x_LogStatus(0, true, query, subj, true, "No compartment found");
    }
  }

  return 0;
}


END_NCBI_SCOPE
                     

USING_NCBI_SCOPE;


// make Splign index file
void MakeIDX( istream* inp_istr, const size_t file_index, ostream* out_ostr )
{
    istream * inp = inp_istr? inp_istr: &cin;
    ostream * out = out_ostr? out_ostr: &cout;
    inp->unsetf(IOS_BASE::skipws);
    char c0 = '\n', c;
    while(inp->good()) {
        c = inp->get();
        if(c0 == '\n' && c == '>') {
            CT_OFF_TYPE pos = inp->tellg() - CT_POS_TYPE(1);
            string s;
            *inp >> s;
            *out << s << '\t' << file_index << '\t' << pos << endl;
        }
        c0 = c;
    }
}



int main(int argc, const char* argv[]) 
{
    // pre-scan for mkidx
    for(int i = 1; i < argc; ++i) {
        if(0 == strcmp(argv[i], "-mkidx")) {
            
            if(i + 1 == argc) {
                char err_msg [] = 
                    "ERROR: No FastA files specified to index. "
                    "Please specify one or more FastA files after -mkidx. "
                    "Your system may support wildcards "
                    "to specify multiple files.";
                cerr << err_msg << endl;
                return 1;
            }
            else {
                ++i;
            }
            vector<string> fasta_filenames;
            for(; i < argc; ++i) {
                fasta_filenames.push_back(argv[i]);
                // test
                ifstream ifs (argv[i]);
                if(ifs.fail()) {
                    cerr << "ERROR: Unable to open " << argv[i] << endl;
                    return 1;
                }
            }
            
            // write the list of files
            const size_t files_count = fasta_filenames.size();
            cout << "# This file was generated by Splign." << endl;
            cout << "$$$FI" << endl;
            for(size_t k = 0; k < files_count; ++k) {
                cout << fasta_filenames[k] << '\t' << k << endl;
            }
            cout << "$$$SI" << endl;
            for(size_t k = 0; k < files_count; ++k) {
                ifstream ifs (fasta_filenames[k].c_str());
                MakeIDX(&ifs, k, &cout);
            }
            
            return 0;
        }
    }
    
    return CSplignApp().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.35  2005/02/23 22:14:18  kapustin
 * Remove outdated code. Shae scope object
 *
 * Revision 1.34  2005/01/31 13:45:19  kapustin
 * Enforce local seq-id if type not recognized
 *
 * Revision 1.33  2005/01/03 22:47:35  kapustin
 * Implement seq-ids with CSeq_id instead of generic strings
 *
 * Revision 1.32  2004/12/16 23:12:26  kapustin
 * algo/align rearrangement
 *
 * Revision 1.31  2004/07/21 15:51:24  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.30  2004/06/29 20:51:21  kapustin
 * Support simultaneous segment computing
 *
 * Revision 1.29  2004/06/23 19:30:44  kapustin
 * Tweak model id substitution when doing both strands
 *
 * Revision 1.28  2004/06/23 19:24:59  ucko
 * GetLastModelID() --> GetNextModelID()
 *
 * Revision 1.27  2004/06/21 18:16:45  kapustin
 * Support computation on both query strands
 *
 * Revision 1.26  2004/06/07 13:47:37  kapustin
 * Throw when no hits returned from Blast
 *
 * Revision 1.25  2004/06/03 19:30:22  kapustin
 * Specify max genomic extension ta the app level
 *
 * Revision 1.24  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.23  2004/05/18 21:43:40  kapustin
 * Code cleanup
 *
 * Revision 1.22  2004/05/10 16:40:12  kapustin
 * Support a pairwise mode
 *
 * Revision 1.21  2004/05/04 15:23:45  ucko
 * Split splign code out of xalgoalign into new xalgosplign.
 *
 * Revision 1.20  2004/04/30 15:00:47  kapustin
 * Support ASN formatting
 *
 * Revision 1.19  2004/04/26 19:26:22  kapustin
 * Count models from one
 *
 * Revision 1.18  2004/04/26 15:38:46  kapustin
 * Add model_id as a CSplign member
 *
 * Revision 1.17  2004/04/23 14:33:32  kapustin
 * *** empty log message ***
 *
 * Revision 1.15  2004/02/19 22:57:55  ucko
 * Accommodate stricter implementations of CT_POS_TYPE.
 *
 * Revision 1.14  2004/02/18 16:04:53  kapustin
 * Do not print PolyA and gap contents
 *
 * Revision 1.13  2003/12/15 20:16:58  kapustin
 * GetNextQuery() ->GetNextPair()
 *
 * Revision 1.12  2003/12/09 13:21:47  ucko
 * +<memory> for auto_ptr
 *
 * Revision 1.11  2003/12/04 20:08:22  kapustin
 * Remove endgaps argument
 *
 * Revision 1.10  2003/12/04 19:26:37  kapustin
 * Account for zero-length segment vector
 *
 * Revision 1.9  2003/12/03 19:47:02  kapustin
 * Increase exon search scope at the ends
 *
 * Revision 1.8  2003/11/26 16:13:21  kapustin
 * Determine subject after filtering for more accurate log reporting
 *
 * Revision 1.7  2003/11/21 16:04:18  kapustin
 * Enable RLE for Poly(A) tail.
 *
 * Revision 1.6  2003/11/20 17:58:20  kapustin
 * Make the code msvc6.0-friendly
 *
 * Revision 1.5  2003/11/20 14:38:10  kapustin
 * Add -nopolya flag to suppress Poly(A) detection.
 *
 * Revision 1.4  2003/11/10 19:20:26  kapustin
 * Generate encoded alignment transcript by default
 *
 * Revision 1.3  2003/11/05 20:32:11  kapustin
 * Include source information into the index
 *
 * Revision 1.2  2003/10/31 19:43:15  kapustin
 * Format and compatibility update
 *
 * Revision 1.1  2003/10/30 19:37:20  kapustin
 * Initial toolkit revision
 *
 * ===========================================================================
 */
