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

#include <algorithm>
#include <iterator>
#include <iostream>

#include <corelib/ncbistd.hpp>
#include <algo/align/nw_spliced_aligner16.hpp>
#include <algo/align/nw_spliced_aligner32.hpp>
#include "splign.hpp"
#include "splign_app.hpp"
#include "splign_app_exception.hpp"
#include "seq_loader.hpp"
#include "subjmixer.hpp"
#include "util.hpp"

#include "hf_hitparser.hpp"

BEGIN_NCBI_SCOPE


const size_t g_max_intron = 10000;
const double g_min_mrna_coverage = 0.5;
const double g_min_hit_idty = 0.95;


bool CSplignApp::x_GetNextQuery(ifstream* ifs, vector<CHit>* hits,
				string* first_line)
{
  hits->clear();
  if(!ifs || !*ifs) {
    return false;
  }

  string query;
  if(first_line->size()) {
    CHit hit (first_line->c_str());
    query = hit.m_Query;
    hits->push_back(hit);
  }
  char buf [1024];
  while(ifs) {
    size_t pos0 = ifs->tellg();
    ifs->getline(buf, sizeof buf, '\n');
    size_t pos1 = ifs->tellg();
    if(pos1 == pos0) break; // GCC hack
    if(buf[0] == '#') continue; // skip comments
    const char* p = buf; // skip leading spaces
    while(*p == ' ' || *p == '\t') ++p;
    if(*p == 0) continue; // skip empty lines
    CHit hit (p);
    if(query.size() == 0) {
      query = hit.m_Query;
    }
    if(hit.m_Query != query) {
      *first_line = p;
      break;
    }
    hits->push_back(hit);
  }
  return hits->size()? true: false;
}


void CSplignApp::x_LogStatus(const string& query, const string& subj,
			     bool error, const string& msg)
{
  string error_tag (error? "Error:\t": "");
  m_logstream << query << '\t' << subj << '\t' << error_tag << msg << endl;
}


void CSplignApp::Init()
{
  HideStdArgs( fHideLogfile | fHideConffile | fHideVersion);
  
  auto_ptr<CArgDescriptions> argdescr(new CArgDescriptions);
  argdescr->SetUsageContext(GetArguments().GetProgramName(),
			    "Splign v.0.0.1");
  
  // principal input arguments
  argdescr->AddKey
    ("data_mrna", "data_mrna",
     "Multi-FastA file with mRna sequences.",
     CArgDescriptions::eString);
  
  argdescr->AddOptionalKey
    ("mrna_idx", "mrna_idx",
     "Index file for data_mrna.",
     CArgDescriptions::eString);
  
  argdescr->AddKey
    ("data_ctg", "data_ctg",
     "Multi-FastA file with genomic sequences.",
     CArgDescriptions::eString);
  
  argdescr->AddOptionalKey
    ("ctg_idx", "ctg_idx",
     "Index file to data_ctg.",
     CArgDescriptions::eString);
  
  argdescr->AddOptionalKey
    ("hits", "hits",
     "Input hit file, sorted by query. "
     "If not specified, hits will be generated internally.",
     CArgDescriptions::eString);
  
  argdescr->AddDefaultKey
    ("min_idty", "identity",
     "Minimal exon identity. Lower identity segments "
     "will be marked as gaps.",
     CArgDescriptions::eDouble,
     "0.8");
  
  argdescr->AddFlag ("endgaps", "Always detect unaligning regions at the ends,"
		     " regardless of exon identities.",
		     true);

  argdescr->AddDefaultKey
    ("err_level", "err_level", "Sequences' error level.",
     CArgDescriptions::eString, "low");
      
  argdescr->AddFlag ("rle", "Encode alignment transcripts.", true);

  argdescr->AddFlag ("mkidx", "Make MultiFastA index file and quit.", true);

  argdescr->AddDefaultKey
    ("logfile", "logfile", "Splign log file",
     CArgDescriptions::eString, "splign.log");
  
  // restrictions
  CArgAllow_Strings* constrain_errlevel = new CArgAllow_Strings;
  constrain_errlevel->Allow("low")->Allow("high");
  argdescr->SetConstraint("err_level", constrain_errlevel);

  CArgAllow* constrain_idty = new CArgAllow_Doubles(0,1);
  argdescr->SetConstraint("min_idty", constrain_idty);
    
  SetupArgDescriptions(argdescr.release());
}


int CSplignApp::Run()
{ 
  const CArgs& args = GetArgs();

  m_SeqQuality = args["err_level"].AsString() == "low";
  m_rle = args["rle"];
  m_minidty = args["min_idty"].AsDouble();
  m_endgaps = args["endgaps"];

  m_logstream.open( args["logfile"].AsString().c_str() );

  // initialize sequence loaders

  const string& filename_mrna = args["data_mrna"].AsString();
  const string& filename_mrna_idx = args["mrna_idx"]?
    args["mrna_idx"].AsString(): kEmptyStr;  
  m_loader_mrna.Open( filename_mrna, filename_mrna_idx );

  const string& filename_ctg = args["data_ctg"].AsString();
  const string& filename_ctg_idx = args["ctg_idx"]?
    args["ctg_idx"].AsString(): kEmptyStr;
  m_loader_ctg.Open( filename_ctg, filename_ctg_idx );

  // prepare input hit stream

  auto_ptr<fstream> hit_stream;
  if(args["hits"]) {
    const string filename (args["hits"].AsString());
    hit_stream.reset((fstream*)new ifstream (filename.c_str()));
  }
  else {
    hit_stream.reset(CFile::CreateTmpFile(kEmptyStr,
					  CFile::eText, CFile::eAllowRead ));
  }

  if(!args["hits"]) {

    vector<CHit> hits_new;
    // x_BlastAll(loader_mrna, loader_ctg, &hits_new);
    // sort(hits_new.begin(), hits_new.end(), CHit::PQuerySubj);
    // copy(hits_new.begin(), hits_new.end(),
    // ostream_iterator<CHit>(*hit_stream));
    NCBI_THROW(
	       CSplignAppException,
	       eInternal,
	       "Internal hit calculation not yet supported. "
	       "Please specify external hit file.");
  }

  // parse input stream by query

  vector<CHit> hits;
  ifstream* ifs_hits = (ifstream*) (hit_stream.get());
  string first_line;
  while(x_GetNextQuery(ifs_hits, &hits, &first_line)) {

    // select the best groups among all subjects
    CSubjMixer mixer (&hits); // mix subjects
    DoFilter(&hits);          // filter and select the best group
    mixer.UnMix(&hits);

    string query (hits[0].m_Query);
    string subj (hits[0].m_Subj);

    bool error = false;
    try {
      cout << x_RunOnPair(&hits);
    }
    catch(CException& e) {
      x_LogStatus(query, subj, true, e.GetMsg());
      error = true;
    }
    if(!error) {
      x_LogStatus(query, subj, false, "Ok");
    }
  }

  return 0;
}


string CSplignApp::x_RunOnPair( vector<CHit>* hits )
{
  if(hits->size() == 0) {
    NCBI_THROW( CSplignAppException, eGeneral, "No hits left after filtering");
  }

  const string query ((*hits)[0].m_Query);

  vector<char> mrna;
  m_loader_mrna.Load(query, &mrna, 0, kMax_UInt);
  const size_t mrna_size = mrna.size();
  
  size_t polya_start = TestPolyA(mrna);
  if(polya_start < kMax_UInt) {
    CleaveOffByTail(hits, polya_start + 1); // cleave off hits beyond cds
    if(hits->size() == 0) {
      NCBI_THROW( CSplignAppException, eGeneral, "All hits behind CDS");
    }
  }  
  
  // find regions of interest on mRna (query)
  // and contig (subj)
  size_t qmin, qmax, smin, smax;
  GetHitsMinMax(*hits, &qmin, &qmax, &smin, &smax);
  --qmin; --qmax; --smin; --smax;

  qmin = 0;
  qmax = polya_start < kMax_UInt? polya_start - 1: mrna_size - 1;
  smin = max(0, int(smin - g_max_intron));
  smax += g_max_intron;
  
  bool minus_strand = ! (*hits)[0].IsStraight();

  const string subj ((*hits)[0].m_Subj);
  vector<char> contig;
  m_loader_ctg.Load(subj, &contig, smin, smax);
  
  const size_t ctg_end = smin + contig.size();
  if(ctg_end - 1 < smax) { // perhabs adjust smax
    smax = ctg_end - 1;
  }

  if(minus_strand) {
    // make reverse complementary
    // for the contig's area of interest
    reverse (contig.begin(), contig.end());
    transform(contig.begin(), contig.end(), contig.begin(),
	      SCompliment());
    // flip the hits
    for(size_t i = 0, n = hits->size(); i < n; ++i) {
      CHit& h = (*hits)[i];
      size_t a2 = smax - (h.m_ai[3] - smin) + 2;
      size_t a3 = smax - (h.m_ai[2] - smin) + 2;
      h.m_an[2] = h.m_ai[2] = a2;
      h.m_an[3] = h.m_ai[3] = a3;
    }
  }

  // shift hits so that they originate from qmin, smin;
  // also make them zero-based
  for(size_t i = 0, n = hits->size(); i < n; ++i) {
    CHit& h = (*hits)[i];
    h.m_an[0] = h.m_ai[0] -= qmin + 1;
    h.m_an[1] = h.m_ai[1] -= qmin + 1;
    h.m_an[2] = h.m_ai[2] -= smin + 1;
    h.m_an[3] = h.m_ai[3] -= smin + 1;
  }  
  
  // skip low-coverage sequences
  {{
  CHitParser hp;
  int hit_coverage = hp.CalcCoverage(hits->begin(), hits->end(), 'q');
  size_t max_right = qmax + 1;
  if(double(hit_coverage)/max_right < g_min_mrna_coverage) {
    
    CNcbiOstrstream oss;
    oss << "mRna not covered at least " <<  g_min_mrna_coverage*100 << '%';
    string msg = CNcbiOstrstreamToString(oss);
    NCBI_THROW( CSplignAppException, eGeneral, msg.c_str());
  }
  }}
  
  // delete low-identity hits
  {{
  size_t hit_dim = hits->size(), j = 0;
  for(size_t k = 0; k < hit_dim; ++k) {
    if((*hits)[k].m_Idnty >= g_min_hit_idty) {
      if(j < k) (*hits)[j] = (*hits)[k];
      ++j;
    }                }
  hits->resize(hit_dim = j);
  }}
  
  if(hits->size() == 0) {
    NCBI_THROW(CSplignAppException, eGeneral, "Hits are too weak");
  }

  // select aligner based on sequence quality
  auto_ptr<CSplicedAligner> aligner ( m_SeqQuality > 0?
    static_cast<CSplicedAligner*> (new CSplicedAligner16):
    static_cast<CSplicedAligner*> (new CSplicedAligner32) );

  // setup splign
  CSplign splign (aligner.get());
  splign.SetMinExonIdentity(m_minidty);
  splign.EnforceEndGapsDetection(m_endgaps);
  splign.SetSequences(&mrna[qmin], qmax - qmin + 1, &contig[0], contig.size());
  SetPatternFromHits(splign, hits);

  // run and parse
  const vector<CSplign::SSegment>* segments = splign.Run();
  const size_t seg_dim = segments->size();

  // try to extend the last segment into PolyA area  
  if(polya_start < kMax_UInt && seg_dim && (*segments)[seg_dim-1].m_exon) {
    CSplign::SSegment& s = const_cast<CSplign::SSegment&>(
				       (*segments)[seg_dim-1]);
    const char* p0 = &mrna[0] + s.m_box[1] + 1, *p = p0;
    const char* pe = p0 + mrna_size;
    const char* q = &contig[0] + s.m_box[3] + 1;
    for(; p < pe; ++p, ++q) {
      if(*p != 'A') break;
      if(*p != *q) break;
    }
    size_t sh = p - p0;
    if(sh) {
      // resize
      s.m_box[1] += sh;
      s.m_box[3] += sh;
      vector<char> v (sh, 'M');
      copy(v.begin(), v.end(), back_inserter(s.m_details));
      s.RestoreIdentity();

      // correct annotation
      const size_t ann_dim = s.m_annot.size();
      if(ann_dim > 2 && s.m_annot[ann_dim - 3] == '>') {
          s.m_annot[ann_dim-2] = *q++;
      }

      polya_start += sh;
    }
  }

  // look for PolyA in trailing segments:
  // if a segment is mostly 'A's then
  // we add it to Poly(A)
  int j = seg_dim - 1;
  for(; j >= 0; --j) {
    const char* p0 = &mrna[qmin] + (*segments)[j].m_box[0];
    const char* p1 = &mrna[qmin] + (*segments)[j].m_box[1] + 1;
    size_t count = 0;
    for(const char* pc = p0; pc != p1; ++pc) {
        if(*pc == 'A') ++count;
    }
    double ac = double(count) / double(p1 - p0);
    if(ac < 0.9) break;
  }
  if(j >= 0 && j < int(seg_dim - 1)) {
    polya_start = (*segments)[j].m_box[1] + 1;
  }

  CNcbiOstrstream oss;
  oss.precision(3);
  for(size_t i = 0, n = j + 1; i < n; ++i ) {
    const CSplign::SSegment seg = (*segments)[i];
    oss << query << '\t' << subj << '\t';
    if(seg.m_exon) {
      oss << seg.m_idty << '\t';
    }
    else {
      oss << "-\t";
    }
    
    oss << seg.m_len << '\t';

    // the box
    oss << qmin + seg.m_box[0] + 1 << '\t'
	<< qmin + seg.m_box[1] + 1 << '\t';

    if(seg.m_exon) {
      if(minus_strand) {
	oss << smax - seg.m_box[2] + 1 << '\t' 
	    << smax - seg.m_box[3] + 1 << '\t';
      }
      else {
	oss << smin + seg.m_box[2] + 1 << '\t' 
	    << smin + seg.m_box[3] + 1 << '\t';
      }
    } else {
      oss << "-\t-\t";
    }

    if(seg.m_exon) {
      oss << seg.m_annot << '\t' << (m_rle? RLE(seg.m_details): seg.m_details);
    }
    else {
      if(i == 0) {
	oss << "<L-Gap>\t";
      }
      else if(i == n - 1) {
	oss << "<R-Gap>\t";
      }
      else {
	oss << "<M-Gap>\t";
      }
      oss << seg.m_details;
    }
    oss << endl;
  }

  // report Poly(A)
  if(polya_start < kMax_UInt && polya_start < mrna_size) {
    oss << query << '\t' << subj << "\t-\t"
  	<< mrna_size - polya_start << '\t'
  	<< polya_start + 1 << '\t' << mrna_size << "\t-\t-\t<Poly(A)-Tail>\t";
    copy(mrna.begin() + polya_start, mrna.end(),
	 ostream_iterator<char>(oss));
    oss << endl;
  }

  return CNcbiOstrstreamToString(oss);
}



END_NCBI_SCOPE
                     

USING_NCBI_SCOPE;

int main(int argc, const char* argv[]) 
{
  // pre-scan for mkidx
  for(int i = 1; i < argc; ++i) {
    if(0 == strcmp(argv[i], "-mkidx")) {
      MakeIDX();
      return 0;
    }
  }

  return CSplignApp().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/10/31 19:43:15  kapustin
 * Format and compatibility update
 *
 * Revision 1.1  2003/10/30 19:37:20  kapustin
 * Initial toolkit revision
 *
 * ===========================================================================
 */
