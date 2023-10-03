/*
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

Author: Tom Madden

******************************************************************************/

/** @file vecscreen_run.cpp
 * Run vecscreen, produce output
*/

#include <ncbi_pch.hpp>
#include <algo/blast/format/blast_format.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objmgr/util/seq_loc_util.hpp>

#include <objmgr/util/sequence.hpp>
#include <objtools/align_format/showdefline.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objtools/readers/fasta.hpp>

#include <algo/blast/format/vecscreen_run.hpp>
#include <misc/jsonwrapp/jsonwrapp.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
USING_SCOPE(sequence);
#endif

map<string, int> match_type_ints = { {"Strong",0}, {"Moderate",1}, {"Weak",2}, {"Suspect",3},{"Absent",4} };
map<int, string> match_type_strs = { {0,"Strong"}, {1,"Moderate"}, {2,"Weak"}, {3,"Suspect"},{4,"Absent"} };


string s_PopIdPart(const string& in_str) 
{
    auto bar_p = in_str.find("|");
    auto colon_p = in_str.find(":");

    if(bar_p == NPOS && colon_p == NPOS)
        return "";

    auto min_p = min(bar_p, colon_p);

    return in_str.substr(min_p+1, NPOS);
}

void s_MakeFastaSubStrs(const string& in_str, list<string>& out_strs) 
{
    string new_str = in_str;
    while(!new_str.empty()) {
        out_strs.push_back(new_str);
        new_str = s_PopIdPart(new_str);
    }
}

void s_MakeFastaSubIds(const CSeq_id& in_id, list<CRef<CSeq_id> >& out_ids)
{
    list<string> fasta_sub_strs;
    s_MakeFastaSubStrs(in_id.GetSeqIdString(true), fasta_sub_strs);
    ITERATE(list<string>, sub_str_iter, fasta_sub_strs) {
        try {
            CRef<CSeq_id> sub_id(new CSeq_id);
            sub_id->Set(*sub_str_iter);
            out_ids.push_back(sub_id);
        } catch(...) { ; } // those that dont work with Set() just get ignored
    }
}

CRef<CSeq_id> s_ReplaceLocalId(const CBioseq_Handle& bh, CConstRef<CSeq_id> sid_in, bool parse_local)
{
    CRef<CSeq_id> retval(new CSeq_id());

    // Local ids are usually fake. If a title exists, use the first token
    // of the title instead of the local id. If no title or if the local id
    // should be parsed, use the local id, but without the "lcl|" prefix.
    if (sid_in->IsLocal()) {
        string id_token;
        vector<string> title_tokens;
        title_tokens =
            NStr::Split(CAlignFormatUtil::GetTitle(bh), " ", title_tokens);
        if(title_tokens.empty()){
            id_token = NcbiEmptyString;
        } else {
            id_token = title_tokens[0];
        }

        if (id_token == NcbiEmptyString || parse_local) {
            const CObject_id& obj_id = sid_in->GetLocal();
            if (obj_id.IsStr())
                id_token = obj_id.GetStr();
            else
                id_token = NStr::IntToString(obj_id.GetId());
        }
        CObject_id* obj_id = new CObject_id();
        obj_id->SetStr(id_token);
        retval->SetLocal(*obj_id);
    } else {
        retval->Assign(*sid_in);
    }

    return retval;
}

// This, and its above helpers, are largely derived from algo/blast/format/tabular.cpp
// but with a little extra digging to find the accession with no fasta 'extras'
// And the title, because its in the defline with the IDs anyway. 
list<CRef<CSeq_id> > s_SetIdList(const CBioseq_Handle& bh, string& title)
{
	list<CRef<CSeq_id> > out_list; 
   	
    // this is a blast_db specific object put on the bioseq, 
    // if the bioseq handle comes from a scope connected to a blastdb 
    // query-side ids will just have a null bdlRef. 
    CRef<CBlast_def_line_set> bdlRef =
                          CSeqDB::ExtractBlastDefline(bh);
    if(!bdlRef.IsNull()) {
        ITERATE(CBlast_def_line_set::Tdata, itr, bdlRef->Get()) {
            ITERATE(CBlast_def_line::TSeqid, id, (*itr)->GetSeqid()) {
                
    	        CRef<CSeq_id> next_id = s_ReplaceLocalId(bh, *id, true);
                out_list.push_back(next_id);
            }
            
            vector<CConstRef<CSeq_id> > idl_copy;
            NON_CONST_ITERATE(list<CRef<CSeq_id> >, ic, out_list) {
                CConstRef<CSeq_id> cc(&(**ic));
                idl_copy.push_back( cc);
            }
            list<CRef<objects::CSeq_id> > next_seqid_list;
            
            // Next call replaces BL_ORD_ID if found.
            CShowBlastDefline::GetSeqIdList(bh,/*original_seqids*/ idl_copy,next_seqid_list);
            ITERATE(list<CRef<CSeq_id> >, idi, next_seqid_list) {
    	        CRef<CSeq_id> next_id = s_ReplaceLocalId(bh, *idi, true);
                out_list.push_back(next_id);
                s_MakeFastaSubIds(**idi, out_list);   
            }
            list<TGi> ignore_gi;
            string other_seq_id_str;
            CShowBlastDefline::GetBioseqHandleDeflineAndId(bh, ignore_gi, other_seq_id_str, title, false);
            if(title.find("|") != NPOS) {
                size_t pos = title.find(" ");
                title = title.substr(pos+1, title.length()-(pos+1));
            }
        }
    }

    // And this is to just loop over whatever other seq-ids exist in the bioseq. 
    // substitute any local ids by new fake local ids, with label set to the first token of this Bioseq's title.
    ITERATE(CBioseq_Handle::TId, itr, bh.GetId()) {
    	CRef<CSeq_id> next_id = s_ReplaceLocalId(bh, itr->GetSeqId(), true);
    	out_list.push_back(next_id);
    }
	return out_list;
}



CVecscreenRun::CVecscreenRun(CRef<CSeq_loc> seq_loc, CRef<CScope> scope, 
                             const string & db, TSeqPos terminal_flexibility)
 : m_SeqLoc(seq_loc), m_Scope(scope), m_DB(db), 
   m_TerminalFlexibility(terminal_flexibility), m_Vecscreen(0)
{
    m_Queries.Reset(new CBlastQueryVector());
    CRef<CBlastSearchQuery> q(new CBlastSearchQuery(*seq_loc, *scope));
    m_Queries->push_back(q);
    x_RunBlast();
}

void CVecscreenRun::x_RunBlast()
{
   //_ASSERT(m_Queries.NotEmpty());
   //_ASSERT(m_Queries->Size() != 0);

   // Load blast queries
   CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(*m_Queries));

   // BLAST optiosn needed for vecscreen.
   CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::CreateTask("vecscreen"));

   // Sets Vecscreen database.
   const CSearchDatabase target_db(m_DB, CSearchDatabase::eBlastDbIsNucleotide);

   // Constructor for blast run.
   CLocalBlast blaster(query_factory, opts, target_db);
 
   // BLAST run.
   m_RawBlastResults = blaster.Run();
   _ASSERT(m_RawBlastResults->size() == 1);
   CRef<CBlastAncillaryData> ancillary_data((*m_RawBlastResults)[0].GetAncillaryData());
   
   // The vecscreen stuff follows.
   CRef<CSeq_align_set>  results_copy(new CSeq_align_set);
   results_copy->Assign( *((*m_RawBlastResults)[0].GetSeqAlign()) );
   m_Vecscreen = new CVecscreen(*results_copy, GetLength(*m_SeqLoc, m_Scope), m_TerminalFlexibility);

   // This actually does the vecscreen work.
   m_Seqalign_set = m_Vecscreen->ProcessSeqAlign();

   CConstRef<CSeq_id> id(m_SeqLoc->GetId());
   CRef<CSearchResults> results(new CSearchResults(id, m_Seqalign_set,
                                                   TQueryMessages(),
                                                   ancillary_data));
   // this part wrote over the original hits with the post-processed results. 
   // I cant see why this was here
   //m_RawBlastResults->clear();
   //m_RawBlastResults->push_back(results);
}

CRef<blast::CSearchResultSet> 
CVecscreenRun::GetSearchResultSet() const
{
    return m_RawBlastResults;
}

struct SVecscreenMatchFinder
{
    SVecscreenMatchFinder(const string& match_type)
        : m_MatchType(match_type) {}

    bool operator()(const CVecscreenRun::SVecscreenSummary& rhs) {
        return (rhs.match_type == m_MatchType);
    }

private:
    string m_MatchType;
};


void CVecscreenRun::CFormatter::x_GetIdsAndTitlesForSeqAlign(const CSeq_align& align, 
                                                            string& qid, string& qtitle, 
                                                            string& sid, string& stitle)
{

    const CBioseq_Handle& query_bh =
        m_Scope.GetBioseqHandle(align.GetSeq_id(0));
    const CBioseq_Handle& subjt_bh =
        m_Scope.GetBioseqHandle(align.GetSeq_id(1));
  
//cerr <<__LINE__<<" : Q BH  " << MSerial_AsnText << *query_bh.GetCompleteBioseq() << endl;

    // these alignments have weird ids. 
    // Query_01 for the inputs and BL_ORD_01 for the subjects. 
    // just from the positions in the fasta reader and blast database. 
    // But we want nice ids, or at least the original ids, for the tabular report
    // Digging through the blast metadata to find the original ids is inside s_SetIdList.
    list<CRef<CSeq_id> > qidl, sidl;
    qidl = s_SetIdList(query_bh, qtitle);   
    sidl = s_SetIdList(subjt_bh, stitle);    

    CConstRef<CSeq_id> qaccid = FindBestChoice(qidl, CSeq_id::Score);
    qaccid->GetLabel(&qid, CSeq_id::eContent, CSeq_id::fLabel_Version);
    CConstRef<CSeq_id> saccid = FindBestChoice(sidl, CSeq_id::Score);
    saccid->GetLabel(&sid, CSeq_id::eContent, CSeq_id::fLabel_Version);
}


void
CVecscreenRun::CFormatter::FormatResults(CNcbiOstream& out,
                                         CRef<CBlastOptionsHandle> vs_opts)
{
    const bool kPrintAlignments = static_cast<bool>(m_Outfmt == eShowAlignments);
    const bool kPrintBlastTab = static_cast<bool>(m_Outfmt == eBlastTab);
    const bool kPrintJson = static_cast<bool>(m_Outfmt == eJson);
    const bool kPrintAsnText = static_cast<bool>(m_Outfmt == eAsnText || m_Outfmt == eAsnTextNoProcess);
    const CFormattingArgs::EOutputFormat fmt = (kPrintBlastTab ? CFormattingArgs::eTabular : (kPrintAsnText?CFormattingArgs::eAsnText : CFormattingArgs::ePairwise));
    const string custom_output = ( kPrintBlastTab ? "qaccver qstart qend saccver salltitles "  : "" );
    const bool kBelieveQuery(false);
    const bool kShowGi(false);
    const CSearchDatabase dbinfo(m_Screener.m_DB,
                                 CSearchDatabase::eBlastDbIsNucleotide);
    CLocalDbAdapter dbadapter(dbinfo);
    const int kNumDescriptions(0);
    const int kNumAlignments(50);   // FIXME: find this out
    const bool kIsTabular(false);

    CBlastFormat blast_formatter(vs_opts->GetOptions(), dbadapter, fmt,
                                 kBelieveQuery, out, kNumDescriptions,
                                 kNumAlignments, m_Scope, BLAST_DEFAULT_MATRIX,
                                 kShowGi, m_HtmlOutput,  
                                 BLAST_GENETIC_CODE,  BLAST_GENETIC_CODE,  false, false, -1,
                                 custom_output);
    if(!kPrintBlastTab && !kPrintJson && !kPrintAsnText ) {
        blast_formatter.PrintProlog();
    }
    
    list<SVecscreenSummary> match_list = m_Screener.GetList();
    
    if(kPrintBlastTab || kPrintJson || kPrintAsnText) {
        CRef<CSearchResultSet> result_set = m_Screener.GetSearchResultSet();
        _ASSERT(result_set->size() == 1);

       // CSeq_align_set* alignments = * ((*result_set)[0]).SetSeqAlign();
        CSeq_align_set* alignments =  m_Screener.GetSeqalignSet();
        if(m_Outfmt == eAsnTextNoProcess) {
            CRef<CSearchResultSet> result_set = m_Screener.GetSearchResultSet();
            alignments =  ((*result_set)[0]).SetSeqAlign().GetPointer();
        }

        if(m_Outfmt != eAsnTextNoProcess) {
            ITERATE(list<SVecscreenSummary>, mi, match_list) {
                NON_CONST_ITERATE(list<CRef<CSeq_align> >, align_iter, alignments->Set()) {
                    if(mi->range.IntersectionWith((*align_iter)->GetSeqRange(0))==mi->range ||
                       mi->range.IntersectionWith((*align_iter)->GetSeqRange(0))==(*align_iter)->GetSeqRange(0)) {
                        if(mi->seqid->Equals( (*align_iter)->GetSeq_id(0))) {
                            (*align_iter)->SetNamedScore("match_type", match_type_ints[mi->match_type]);
                        }
                    }    
                }
            }
        }
        
        if(kPrintAsnText)  {
            
            CSeq_align_set fixed_ids;
            ITERATE(list<CRef<CSeq_align> >, align_iter, alignments->Set()) {
                string qid, qtitle, sid, stitle;
                x_GetIdsAndTitlesForSeqAlign(**align_iter, qid, qtitle, sid, stitle);
                CRef<CSeq_align> fixed(new CSeq_align);
                fixed->Assign(**align_iter);
                try {
                    fixed->SetSegs().SetDenseg().SetIds()[0]->Set(qid);
                } catch(...) {
                    fixed->SetSegs().SetDenseg().SetIds()[0]->Set("lcl|"+qid);
                }
                try {
                    fixed->SetSegs().SetDenseg().SetIds()[1]->Set(sid);
                } catch(...) {
                    fixed->SetSegs().SetDenseg().SetIds()[1]->Set("lcl|"+sid);
                }
                fixed_ids.Set().push_back(fixed);
            }
            (*result_set)[0].SetSeqAlign()->Assign(fixed_ids);
            blast_formatter.PrintOneResultSet((*result_set)[0],
                                              m_Screener.m_Queries);
        }
        else if(kPrintBlastTab) {
            if(!match_list.empty()) {
            out << "#qid\tqstart\tqend\tmatch_strength\tdrop_count\tsid\tstitle" << endl;
            ITERATE(list<SVecscreenSummary>, mi, match_list) { 
                string qtitle="", stitle="";
                string qid="", sid="";
               
                if(!mi->aligns.empty()) {
                    x_GetIdsAndTitlesForSeqAlign(*mi->aligns.front(), qid, qtitle, sid, stitle);
                } else {
                    qid = mi->seqid->GetSeqIdString(true);
                }

                ITERATE(TAlignList, align_iter, mi->aligns) {
                    const CSeq_align& align = **align_iter;
                    qid=sid=qtitle=stitle="";

                    int match_score=0;
                    align.GetNamedScore("vs_match_type", match_score);
                    
                    x_GetIdsAndTitlesForSeqAlign(align, qid, qtitle, sid, stitle);
                    
                    out << qid << "\t"
                        << align.GetSeqStart(0) + 1 << "\t"
                        << align.GetSeqStop(0) + 1 << "\t"
                        << match_type_strs[match_score] << "\t"
                        << mi->drops.size() << "\t"
                        << sid << "\t"
                        << stitle 
                        << endl;
                }
                /*
                ITERATE(TAlignList, align_iter, mi->drops) {
                    const CSeq_align& align = **align_iter;
                    qid=sid=qtitle=stitle="";
                    
                    int match_score=0;
                    align.GetNamedScore("vs_match_type", match_score);

                    x_GetIdsAndTitlesForSeqAlign(align, qid, qtitle, sid, stitle);
                    
                    out << "%%\t" 
                    << qid << "\t"
                    << align.GetSeqStart(0) + 1 << "\t"
                    << align.GetSeqStop(0) + 1 << "\t"
                    << match_type_strs[match_score] << "\t"
                    << sid << "\t"
                    << stitle 
                    << endl;
                }*/
            }
            }    
        } else if(kPrintJson) {
            CJson_Document doc;
            CJson_Object top_obj = doc.SetObject();
            CJson_Array hits_array = top_obj.insert_array("hits");
            
            string qtitle="", stitle="";
            string qid="", sid="";

            string last_qid = "";
            int align_count=0;
            ITERATE(list<SVecscreenSummary>, mi, match_list) { 
                ITERATE(TAlignList, align_iter, mi->aligns) {
                    const CSeq_align& align = **align_iter;
                    qid=sid=qtitle=stitle="";

                    int match_score=0;
                    align.GetNamedScore("vs_match_type", match_score);
                    
                    x_GetIdsAndTitlesForSeqAlign(align, qid, qtitle, sid, stitle);

                    CJson_Object jobj = hits_array.push_back_object();

                    jobj.insert("query_id", qid);
                    jobj.insert("query_start", align.GetSeqStart(0)+1);
                    jobj.insert("query_end", align.GetSeqStop(0)+1);
                    jobj.insert("query_strand", (align.GetSeqStrand(0)==eNa_strand_plus?"+":"-"));
                    jobj.insert("match_strength", match_type_strs[match_score]);
                    jobj.insert("drop_count", mi->drops.size());
                    jobj.insert("subject_id", sid);
                    jobj.insert("subject_title", stitle);
                
                    last_qid = qid;
                    align_count++;
                } // align loop                
            } // range loop
            if(align_count > 0) { // leave out queries with no output
                top_obj.insert("query_id", last_qid);
                doc.Write(out);
            }  
        } // end kPrintJson
        return;
    } // end or or


    // Acknowledge the query if the alignments section won't be printed (this
    // does the acknowledgement)
    if (kPrintAlignments == false) {
        CBioseq_Handle bhandle =
            m_Scope.GetBioseqHandle(*m_Screener.m_SeqLoc->GetId(),
                                    CScope::eGetBioseq_All);
        if( !bhandle  ){
            string message = "Failed to resolve SeqId: "+m_Screener.m_SeqLoc->GetId()->AsFastaString();
            ERR_POST(message);
            NCBI_THROW(CException, eUnknown, message);
        }
        CConstRef<CBioseq> bioseq = bhandle.GetBioseqCore();
        CBlastFormatUtil::AcknowledgeBlastQuery(*bioseq, 
                                                CBlastFormat::kFormatLineLength,
                                                out, kBelieveQuery,
                                                m_HtmlOutput, kIsTabular);
    }
    if (m_HtmlOutput) {
        m_Screener.m_Vecscreen->VecscreenPrint(out);
        if (match_list.empty() && !kPrintAlignments) {
            out << "<b>***** No hits found *****</b><br>\n";
        }
    } else {
        if (match_list.empty() && !kPrintAlignments) {
            out << "No hits found\n";
        } else {

            typedef pair<string, string> TLabels;
            vector<TLabels> match_labels;
            match_labels.push_back(TLabels("Strong", "Strong match"));
            match_labels.push_back(TLabels("Moderate", "Moderate match"));
            match_labels.push_back(TLabels("Weak", "Weak match"));
            match_labels.push_back(TLabels("Suspect", "Suspect origin"));

            ITERATE(vector<TLabels>, label, match_labels) {
                list<SVecscreenSummary>::iterator boundary, itr;
                boundary = stable_partition(match_list.begin(), match_list.end(),
                                            SVecscreenMatchFinder(label->first));
                if (boundary != match_list.begin()) {
                    out << label->second << "\n";
                    for (itr = match_list.begin(); itr != boundary; ++itr) {
                        out << itr->range.GetFrom()+1 << "\t" 
                            << itr->range.GetTo()+1 << "\n";
                    }
                    match_list.erase(match_list.begin(), boundary);
                }
            }

        }
    }

    if (kPrintAlignments) {
        CRef<CSearchResultSet> result_set = m_Screener.GetSearchResultSet();
        _ASSERT(result_set->size() == 1);
        blast_formatter.PrintOneResultSet((*result_set)[0],
                                          m_Screener.m_Queries);
        blast_formatter.PrintEpilog(vs_opts->GetOptions());
    }
}

list<CVecscreenRun::SVecscreenSummary>
CVecscreenRun::GetList() const
{
    _ASSERT(m_Vecscreen != NULL);
    list<CVecscreenRun::SVecscreenSummary> retval;

    const list<CVecscreen::AlnInfo*>* aln_info_ptr = m_Vecscreen->GetAlnInfoList();
    list<CVecscreen::AlnInfo> aln_info;
    ITERATE(list<CVecscreen::AlnInfo*>, ai, *aln_info_ptr) {
        if ((*ai)->type == CVecscreen::eNoMatch) 
            continue;
        CVecscreen::AlnInfo align_info((*ai)->range, (*ai)->type, (*ai)->get_aligns());
        align_info.align_drops = (*ai)->align_drops;
        aln_info.push_back(align_info);
    }
    aln_info.sort();

    ITERATE(list<CVecscreen::AlnInfo>, ai, aln_info) {
       SVecscreenSummary summary;
       summary.seqid = m_SeqLoc->GetId();
       summary.range = ai->range;
       summary.match_type = CVecscreen::GetStrengthString(ai->type);
       summary.aligns = ai->get_aligns();
       summary.drops = ai->align_drops;
       retval.push_back(summary);
    }
    return retval;
}

CRef<objects::CSeq_align_set>
CVecscreenRun::GetSeqalignSet() const
{
    return m_Seqalign_set;
}
