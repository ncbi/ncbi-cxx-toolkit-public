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
 * Author:  Jian Ye
 *
 * File Description:
 *   Display blast defline
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbireg.hpp>
#include <serial/objostrasnb.hpp> 
#include <serial/objistrasnb.hpp> 
#include <connect/ncbi_conn_stream.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/blastdb/Blast_def_line.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>
#include <objects/blastdb/defline_extra.hpp>
#include <objtools/blast_format/showdefline.hpp>
#include <objtools/blast_format/blastfmtutil.hpp>

#include <stdio.h>
#include <html/htmlhelper.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(sequence);

//margins constant

static string kOneSpaceMargin = " ";
static string kTwoSpaceMargin = "  ";

//const strings
static const string kHeader = "Sequences producing significant alignments:";
static const string kScore = "Score";
static const string kE = "E";
static const string kBits = "(Bits)";
static const string kEvalue = "E value";
static const string kValue = "Value";
static const string kN = "N";
static const string kRepeatHeader = "Sequences used in model and found again:";
static const string kNewSeqHeader = "Sequences not found previously or not pr\
eviously below threshold:";
static const string kMaxScore = "Max score";
static const string kTotalScore = "Total score";
static const string kTotal = "Total";
static const string kIdentity = "Max ident";
static const string kPercent = "Percent";
static const string kHighest = "Highest";
static const string kQuery = "Query";
static const string kCoverage = "Query coverage";

//psiblast related
static const string kPsiblastNewSeqGif = "<IMG SRC=\"images/new.gif\" \
WIDTH=30 HEIGHT=15 ALT=\"New sequence mark\">";

static const string kPsiblastNewSeqBackgroundGif = "<IMG SRC=\"images/\
bg.gif\" WIDTH=30 HEIGHT=15 ALT=\" \">";

static const string kPsiblastCheckedBackgroundGif = "<IMG SRC=\"images\
/bg.gif\" WIDTH=15 HEIGHT=15 ALT=\" \">";

static const string kPsiblastCheckedGif = "<IMG SRC=\"images/checked.g\
if\" WIDTH=15 HEIGHT=15 ALT=\"Checked mark\">";

static const string kPsiblastEvalueLink = "<a name = Evalue></a>";

static const string  kPsiblastCheckboxChecked = "<INPUT TYPE=\"checkbox\" NAME\
=\"checked_GI\" VALUE=\"%d\" CHECKED>  <INPUT TYPE=\"hidden\" NAME =\"good_G\
I\" VALUE = \"%d\">";

static const string  kPsiblastCheckbox =  "<INPUT TYPE=\"checkbox\" NAME=\"ch\
ecked_GI\" VALUE=\"%d\">  ";


///Get the url for linkout
///@param linkout: the membership value
///@param gi: the actual gi or 0
///@param rid: RID
///@param cdd_rid: CDD RID
///@param entrez_term: entrez query term
///@param is_na: is this sequence nucleotide or not
///
static list<string> s_GetLinkoutString(int linkout, int gi, string& rid, 
                                       string& cdd_rid, string& entrez_term,
                                       bool is_na)
{
    list<string> linkout_list;
    char molType[8]={""};
    if(!is_na){
        sprintf(molType, "[pgi]");
    }
    else {
        sprintf(molType, "[ngi]");
    }
    
    char buf[1024];
    
    if (linkout & eUnigene) {
      string l_UnigeneUrl = CBlastFormatUtil::GetURLFromRegistry("UNIGEN");
        sprintf(buf, l_UnigeneUrl.c_str(), is_na ? "nucleotide" : "protein", 
                is_na ? "nucleotide" : "protein", gi, rid.c_str());
        linkout_list.push_back(buf);
    }
    if (linkout & eStructure){
        sprintf(buf, kStructureUrl.c_str(), rid.c_str(), gi, gi, 
                cdd_rid.c_str(), "onegroup", 
                (entrez_term == NcbiEmptyString) ? 
                "none":((char*) entrez_term.c_str()));
        linkout_list.push_back(buf);
    }
    if (linkout & eGeo){
      string l_GeoUrl = CBlastFormatUtil::GetURLFromRegistry("GEO");
        sprintf(buf, l_GeoUrl.c_str(), gi, rid.c_str());
        linkout_list.push_back(buf);
    }
    if(linkout & eGene){
      string l_GeneUrl = CBlastFormatUtil::GetURLFromRegistry("GENE");
        sprintf(buf, l_GeneUrl.c_str(), gi, !is_na ? "PUID" : "NUID", rid.c_str());
        linkout_list.push_back(buf);
    }
    return linkout_list;
}

///Return url for seqid
///@param ids: input seqid list
///@param gi: gi to use
///@param user_url: use this user specified url if it's non-empty
///@param is_db_na: is the database nucleotide or not
///@param db_name: name of the database
///@param open_new_window: click the url to open a new window?
///@param rid: RID
///@param query_number: the query number
///
static string s_GetIdUrl(const CBioseq::TId& ids, int gi, string& user_url,
                         bool is_db_na, string& db_name, bool open_new_window, 
                         string& rid, int query_number, int taxid)
{
    string url_link = NcbiEmptyString;
    CConstRef<CSeq_id> wid = FindBestChoice(ids, CSeq_id::WorstRank);
    char dopt[32], db[32];
    if(user_url == NcbiEmptyString ||
       (gi > 0 && user_url.find("dumpgnl.cgi") != string::npos)){ 
        //use entrez or dbtag specified
        if(is_db_na) {
            strcpy(dopt, "GenBank");
            strcpy(db, "Nucleotide");
        } else {
            strcpy(dopt, "GenPept");
            strcpy(db, "Protein");
        }    
        
        char url_buf[2048];
        if (gi > 0) {
	  string l_EntrezUrl = CBlastFormatUtil::GetURLFromRegistry("ENTREZ");
            sprintf(url_buf, l_EntrezUrl.c_str(), "", db, gi, dopt, rid.c_str(),
                    open_new_window ? "TARGET=\"EntrezView\"" : "");
            url_link = url_buf;
        } else {//seqid general, dbtag specified
            if(wid->Which() == CSeq_id::e_General){
                const CDbtag& dtg = wid->GetGeneral();
                const string& dbname = dtg.GetDb();
                if(NStr::CompareNocase(dbname, "TI") == 0){
                    string actual_id;
                    wid->GetLabel(&actual_id, CSeq_id::eContent);
                    sprintf(url_buf, kTraceUrl.c_str(), "", actual_id.c_str(),
                            rid.c_str());
                    url_link = url_buf;
                }
            }
        }
    } else { //need to use user_url 

        string url_with_parameters = CBlastFormatUtil::BuildUserUrl(ids, taxid, user_url,
                                                                    db_name,
                                                                    is_db_na, rid,
                                                                    query_number);
        if (url_with_parameters != NcbiEmptyString) {
            url_link += "<a href=\"";
            url_link += url_with_parameters;
            url_link += "\">";
        }
    }
    return url_link;
}


///Return url for seqid
///@param ids: input seqid list
///@param gi: gi to use
///@param user_url: use this user specified url if it's non-empty
///@param is_db_na: is the database nucleotide or not
///@param db_name: name of the database
///@param open_new_window: click the url to open a new window?
///@param rid: RID
///@param query_number: the query number
///
static string s_GetIdUrlNew(const CBioseq::TId& ids, int gi, string& user_url,
                         bool is_db_na, string& db_name, bool open_new_window, 
                         string& rid, int query_number, int taxid, int linkout)
{
    string url_link = NcbiEmptyString;
    CConstRef<CSeq_id> wid = FindBestChoice(ids, CSeq_id::WorstRank);
    char dopt[32], db[32];
 
    bool hit_not_in_mapviewer = !(linkout & eHitInMapviewer);
    
    if (user_url != NcbiEmptyString && 
        !((user_url.find("dumpgnl.cgi") != string::npos && gi > 0) || 
          (user_url.find("maps.cgi") != string::npos && hit_not_in_mapviewer))) {
        
        string url_with_parameters = 
            CBlastFormatUtil::BuildUserUrl(ids, taxid, user_url,
                                           db_name,
                                           is_db_na, rid,
                                           query_number);
        if (url_with_parameters != NcbiEmptyString) {
            url_link += "<a href=\"";
            url_link += url_with_parameters;
            url_link += "\">";
        }
    } else { 
        //use entrez or dbtag specified     
        char url_buf[2048];
        if (gi > 0) {
            if(is_db_na) {
                strcpy(dopt, "GenBank");
                strcpy(db, "Nucleotide");
            } else {
                strcpy(dopt, "GenPept");
                strcpy(db, "Protein");
            }    
	  string l_EntrezUrl = CBlastFormatUtil::GetURLFromRegistry("ENTREZ");            
            sprintf(url_buf, l_EntrezUrl.c_str(), "", db, gi, dopt, rid.c_str(),
                    open_new_window ? "TARGET=\"EntrezView\"" : "");
            url_link = url_buf;
        } else {//seqid general, dbtag specified
            if(wid->Which() == CSeq_id::e_General){
                const CDbtag& dtg = wid->GetGeneral();
                const string& dbname = dtg.GetDb();
                if(NStr::CompareNocase(dbname, "TI") == 0){
                    string actual_id;
                    wid->GetLabel(&actual_id, CSeq_id::eContent);
                    sprintf(url_buf, kTraceUrl.c_str(), "", actual_id.c_str(),
                            rid.c_str());
                    url_link = url_buf;
                }
            }
        }
    }
    
    return url_link;
}


string 
CShowBlastDefline::GetSeqIdListString(const list<CRef<objects::CSeq_id> >& id,
                                      bool show_gi)
{
    string id_str = NcbiEmptyString;

    ITERATE(list<CRef<CSeq_id> >, itr, id) {
        string id_token;
        // For local ids, make sure the "lcl|" part is not printed
        if ((*itr)->IsLocal())
            (*itr)->GetLabel(&id_token, CSeq_id::eContent, 0);
        else if (show_gi || !(*itr)->IsGi())
            id_token = (*itr)->AsFastaString();
        if (id_token != NcbiEmptyString)
            id_str += id_token + "|";
    }
    if (id_str.size() > 0)
        id_str.erase(id_str.size() - 1);

    return id_str;
}

void 
CShowBlastDefline::GetSeqIdList(const objects::CBioseq_Handle& bh,
                                list<CRef<objects::CSeq_id> >& ids)
{
    ids.clear();

    // Check for ids of type "gnl|BL_ORD_ID". These are the artificial ids
    // created in a BLAST database when it is formatted without indexing.
    // For such ids, create new fake local Seq-ids, saving the first token of 
    // the Bioseq's title, if it's available.
    ITERATE(CBioseq_Handle::TId, itr, bh.GetId()) {
        CRef<CSeq_id> next_seqid(new CSeq_id());
        string id_token = NcbiEmptyString;
        
        if (next_seqid->IsGeneral() &&
            next_seqid->AsFastaString().find("gnl|BL_ORD_ID") 
            != string::npos) {
            vector<string> title_tokens;
            id_token = 
                NStr::Tokenize(sequence::GetTitle(bh), " ", title_tokens)[0];
        }
        if (id_token != NcbiEmptyString) {
            // Create a new local id with a label containing the extracted
            // token and save it in the next_seqid instead of the original 
            // id.
            CObject_id* obj_id = new CObject_id();
            obj_id->SetStr(id_token);
            next_seqid->SetLocal(*obj_id);
        } else {
                next_seqid->Assign(*itr->GetSeqId());
        }
        ids.push_back(next_seqid);
    }
}

void
CShowBlastDefline::GetBioseqHandleDeflineAndId(const CBioseq_Handle& handle,
                                               list<int>& use_this_gi,
                                               string& seqid, string& defline, 
                                               bool show_gi /* = true */,
                                               int this_gi_first /* = -1 */)
{
    // Retrieve the CBlast_def_line_set object and save in a CRef, preventing
    // its destruction; then extract the list of CBlast_def_line objects.
    CRef<CBlast_def_line_set> bdlRef = 
        CBlastFormatUtil::GetBlastDefline(handle);
    bdlRef->PutTargetGiFirst(this_gi_first);
    const list< CRef< CBlast_def_line > >& bdl = bdlRef->Get();

    if (bdl.empty()){
        list<CRef<objects::CSeq_id> > ids;
        GetSeqIdList(handle, ids);
        seqid = GetSeqIdListString(ids, show_gi);
        defline = GetTitle(handle);
    } else { 
        bool is_first = true;
        ITERATE(list<CRef<CBlast_def_line> >, iter, bdl) {
            const CBioseq::TId& cur_id = (*iter)->GetSeqid();
            int cur_gi =  FindGi(cur_id);
            int gi_in_use_this_gi = 0;
            ITERATE(list<int>, iter_gi, use_this_gi){
                if(cur_gi == *iter_gi){
                    gi_in_use_this_gi = *iter_gi;                 
                    break;
                }
            }
            if(use_this_gi.empty() || gi_in_use_this_gi > 0) {
                if (is_first)
                    seqid = GetSeqIdListString(cur_id, show_gi);

                if((*iter)->IsSetTitle()){
                    if(is_first){
                        defline = (*iter)->GetTitle();
                    } else {
                        string concat_acc;
                        CConstRef<CSeq_id> wid = 
                            FindBestChoice(cur_id, CSeq_id::WorstRank);
                        wid->GetLabel(&concat_acc, CSeq_id::eFasta, 0);
                        if( show_gi && cur_gi > 0){
                            defline = defline + " >" + "gi|" + 
                                NStr::IntToString(cur_gi) + "|" + 
                                concat_acc + " " + (*iter)->GetTitle();
                        } else {
                            defline = defline + " >" + concat_acc + " " + 
                                (*iter)->GetTitle();
                        }
                    }
                    is_first = false;
                }
            }
        }
    }
}

void CShowBlastDefline::x_FillDeflineAndId(const CBioseq_Handle& handle,
                                           const CSeq_id& aln_id,
                                           list<int>& use_this_gi,
                                           SDeflineInfo* sdl)
{
    const CRef<CBlast_def_line_set> bdlRef = CBlastFormatUtil::GetBlastDefline(handle);
    const list< CRef< CBlast_def_line > >& bdl = bdlRef->Get();
    const CBioseq::TId* ids = &handle.GetBioseqCore()->GetId();
    CRef<CSeq_id> wid;
    sdl->defline = NcbiEmptyString;
 
    sdl->gi = 0;
    sdl->id_url = NcbiEmptyString;
    sdl->score_url = NcbiEmptyString;
    sdl->linkout = 0;
    sdl->is_new = false;
    sdl->was_checked = false;

    //get psiblast stuff
    
    if(m_SeqStatus){
        string aln_id_str;
        aln_id.GetLabel(&aln_id_str, CSeq_id::eContent);
        int seq_status = 0;
        
        map<string, int>::const_iterator iter = m_SeqStatus->find(aln_id_str);
        if ( iter != m_SeqStatus->end() ){
            seq_status = iter->second;
        }
        if((m_PsiblastStatus == eFirstPass) ||
           ((m_PsiblastStatus == eRepeatPass) && (seq_status & eRepeatSeq))
           || ((m_PsiblastStatus == eNewPass) && (seq_status & eRepeatSeq))){
            if(!(seq_status & eGoodSeq)){
                sdl->is_new = true;
            }
            if(seq_status & eCheckedSeq){
                sdl->was_checked = true;
            }   
        }
    }
        
    //get id
    if(bdl.empty()){
        wid = FindBestChoice(*ids, CSeq_id::WorstRank);
        sdl->id = wid;
        sdl->gi = FindGi(*ids);    
    } else {        
        bool found = false;
        for(list< CRef< CBlast_def_line > >::const_iterator iter = bdl.begin();
            iter != bdl.end(); iter++){
            const CBioseq::TId* cur_id = &((*iter)->GetSeqid());
            int cur_gi =  FindGi(*cur_id);
            wid = FindBestChoice(*cur_id, CSeq_id::WorstRank);
            if (!use_this_gi.empty()) {
                ITERATE(list<int>, iter_gi, use_this_gi){
                    if(cur_gi == *iter_gi){
                        found = true;
                        break;
                    }
                }
            } else {
                ITERATE(CBioseq::TId, iter_id, *cur_id) {
                    if ((*iter_id)->Match(aln_id)) {
                        found = true;
                    }
                }
            }
            if(found){
                sdl->id = wid;
                sdl->gi = cur_gi;
                ids = cur_id;
                break;
            }
        }
    }

    //get linkout
    if((m_Option & eLinkout)){
        bool linkout_not_found = true;
        for(list< CRef< CBlast_def_line > >::const_iterator iter = bdl.begin();
            iter != bdl.end(); iter++){
            const CBioseq::TId& cur_id = (*iter)->GetSeqid();
            int cur_gi =  FindGi(cur_id);            
            if(use_this_gi.empty()){
                if(sdl->gi == cur_gi){                 
					sdl->linkout = CBlastFormatUtil::GetLinkout((**iter));
                    sdl->linkout_list =
                        s_GetLinkoutString(sdl->linkout,
                                           sdl->gi, m_Rid, 
                                           m_CddRid, 
                                           m_EntrezTerm, 
                                           handle.GetBioseqCore()->IsNa());
                    break;
                }
            } else {
                ITERATE(list<int>, iter_gi, use_this_gi){
                    if(cur_gi == *iter_gi){                     
                        sdl->linkout = CBlastFormatUtil::GetLinkout((**iter));
                        sdl->linkout_list = 
                            s_GetLinkoutString(sdl->linkout, 
                                               cur_gi, m_Rid,
                                               m_CddRid,
                                               m_EntrezTerm,
                                               handle.GetBioseqCore()->IsNa());
                        linkout_not_found = false;
                        break;
                    }
                }
                if(!linkout_not_found){
                    break;
                } 
            }
            
        }
    }
    //get score and id url
    if(m_Option & eHtml){
        string accession;
        sdl->id->GetLabel(&accession, CSeq_id::eContent);
        sdl->score_url = "<a href=#";
        sdl->score_url += sdl->gi == 0 ? accession : 
            NStr::IntToString(sdl->gi);
        sdl->score_url += ">";

        string user_url = m_Reg->Get(m_BlastType, "TOOL_URL");
        string type_temp = m_BlastType;
        type_temp = NStr::TruncateSpaces(NStr::ToLower(type_temp));
        int taxid = 0;
        if (type_temp == "mapview" || type_temp == "mapview_prev" || 
            type_temp == "gsfasta") {
            taxid = 
                CBlastFormatUtil::GetTaxidForSeqid(aln_id, *m_ScopeRef);
        }
           
        sdl->id_url = s_GetIdUrl(*ids, sdl->gi, user_url,
                                 m_IsDbNa, m_Database, 
                                 (m_Option & eNewTargetWindow) ? true : false,
                                 m_Rid, m_QueryNumber, taxid);
    }

  
    sdl->defline = GetTitle(m_ScopeRef->GetBioseqHandle(*(sdl->id)));
    if (!(bdl.empty())) { 
        for(list< CRef< CBlast_def_line > >::const_iterator iter = bdl.begin();
            iter != bdl.end(); iter++){
            const CBioseq::TId& cur_id = (*iter)->GetSeqid();
            int cur_gi =  FindGi(cur_id);
            int gi_in_use_this_gi = 0;
            ITERATE(list<int>, iter_gi, use_this_gi){
                if(cur_gi == *iter_gi){
                    gi_in_use_this_gi = *iter_gi;                 
                    break;
                }
            }
            if(use_this_gi.empty() || gi_in_use_this_gi > 0) {
                
                if((*iter)->IsSetTitle()){
                    bool id_used_already = false;
                    ITERATE(CBioseq::TId, iter_id, cur_id) {
                        if ((*iter_id)->Match(*(sdl->id))) {
                            id_used_already = true;
                            break;
                        }
                    }
                    if (!id_used_already) {
                        string concat_acc;
                        wid = FindBestChoice(cur_id, CSeq_id::WorstRank);
                        wid->GetLabel(&concat_acc, CSeq_id::eFasta, 0);
                        if( (m_Option & eShowGi) && cur_gi > 0){
                            sdl->defline =  sdl->defline + " >" + "gi|" + 
                                NStr::IntToString(cur_gi) + "|" + 
                                concat_acc + " " + (*iter)->GetTitle();
                        } else {
                            sdl->defline = sdl->defline + " >" + concat_acc +
                                " " + 
                                (*iter)->GetTitle();
                        }
                    }
                }
            }
        }
    }
}


void CShowBlastDefline::x_FillDeflineAndIdNew(const CBioseq_Handle& handle,
                                           const CSeq_id& aln_id,
                                           list<int>& use_this_gi,
                                           SDeflineInfo* sdl)
{
    const CRef<CBlast_def_line_set> bdlRef = CBlastFormatUtil::GetBlastDefline(handle);
    const list< CRef< CBlast_def_line > >& bdl = bdlRef->Get();
    const CBioseq::TId* ids = &handle.GetBioseqCore()->GetId();
    CRef<CSeq_id> wid;
    sdl->defline = NcbiEmptyString;
 
    sdl->gi = 0;
    sdl->id_url = NcbiEmptyString;
    sdl->score_url = NcbiEmptyString;
    sdl->linkout = 0;
    sdl->is_new = false;
    sdl->was_checked = false;

    int taxid = 0;
    string type_temp = m_BlastType;
    type_temp = NStr::TruncateSpaces(NStr::ToLower(type_temp));
 
    if (type_temp == "mapview" || type_temp == "mapview_prev" || 
        type_temp == "gsfasta") {
        taxid = 
            CBlastFormatUtil::GetTaxidForSeqid(aln_id, *m_ScopeRef);
    }
    string user_url;
    if(m_Option & eHtml){
        user_url = m_Reg->Get(m_BlastType, "TOOL_URL");  
    }  

    //get psiblast stuff
    
    if(m_SeqStatus){
        string aln_id_str;
        aln_id.GetLabel(&aln_id_str, CSeq_id::eContent);
        int seq_status = 0;
        
        map<string, int>::const_iterator iter = m_SeqStatus->find(aln_id_str);
        if ( iter != m_SeqStatus->end() ){
            seq_status = iter->second;
        }
        if((m_PsiblastStatus == eFirstPass) ||
           ((m_PsiblastStatus == eRepeatPass) && (seq_status & eRepeatSeq))
           || ((m_PsiblastStatus == eNewPass) && (seq_status & eRepeatSeq))){
            if(!(seq_status & eGoodSeq)){
                sdl->is_new = true;
            }
            if(seq_status & eCheckedSeq){
                sdl->was_checked = true;
            }   
        }
    }
        
    //get id
    if(bdl.empty()){
        wid = FindBestChoice(*ids, CSeq_id::WorstRank);
        sdl->id = wid;
        sdl->gi = FindGi(*ids);    
    } else {        
        bool found = false;
        for(list< CRef< CBlast_def_line > >::const_iterator iter = bdl.begin();
            iter != bdl.end(); iter++){
            const CBioseq::TId* cur_id = &((*iter)->GetSeqid());
            int cur_gi =  FindGi(*cur_id);
            wid = FindBestChoice(*cur_id, CSeq_id::WorstRank);
            if (!use_this_gi.empty()) {
                ITERATE(list<int>, iter_gi, use_this_gi){
                    if(cur_gi == *iter_gi){
                        found = true;
                        break;
                    }
                }
            } else {
                ITERATE(CBioseq::TId, iter_id, *cur_id) {
                    if ((*iter_id)->Match(aln_id)) {
                        found = true;
                    }
                }
            }
            if(found){
                sdl->id = wid;
                sdl->gi = cur_gi;
                ids = cur_id;
                break;
            }
        }
    }

    //get linkout
  
    bool linkout_not_found = true;
    for(list< CRef< CBlast_def_line > >::const_iterator iter = bdl.begin();
        iter != bdl.end(); iter++){
        const CBioseq::TId& cur_id = (*iter)->GetSeqid();
        int cur_gi =  FindGi(cur_id);            
        if(use_this_gi.empty()){
            if(sdl->gi == cur_gi){                 
                sdl->linkout = CBlastFormatUtil::GetLinkout((**iter));
                if((m_Option & eLinkout)){
                    sdl->linkout_list =
                        CBlastFormatUtil::
                        GetLinkoutUrl(sdl->linkout,
                                      cur_id, m_Rid, m_Database, m_QueryNumber,
                                      taxid, m_CddRid, 
                                      m_EntrezTerm, 
                                      handle.GetBioseqCore()->IsNa(), user_url,
                                      m_IsDbNa, 0, true);
                }
                break;
            }
        } else {
            ITERATE(list<int>, iter_gi, use_this_gi){
                if(cur_gi == *iter_gi){                     
                    sdl->linkout = CBlastFormatUtil::GetLinkout((**iter));
                    if((m_Option & eLinkout)){
                        sdl->linkout_list = 
                            CBlastFormatUtil::
                            GetLinkoutUrl(sdl->linkout, 
                                          cur_id, m_Rid, m_Database, m_QueryNumber,
                                          taxid, m_CddRid, 
                                          m_EntrezTerm,
                                          handle.GetBioseqCore()->IsNa(), user_url,
                                          m_IsDbNa, 0, true);
                    }
                    linkout_not_found = false;
                    break;
                }
            }
            if(!linkout_not_found){
                break;
            } 
        }   
    }
    
    //get score and id url
    if(m_Option & eHtml){
        string accession;
        sdl->id->GetLabel(&accession, CSeq_id::eContent);
        sdl->score_url = "<a href=#";
        sdl->score_url += sdl->gi == 0 ? accession : 
            NStr::IntToString(sdl->gi);
        sdl->score_url += ">";
        string user_url = m_Reg->Get(m_BlastType, "TOOL_URL");
        string type_temp = m_BlastType;
        type_temp = NStr::TruncateSpaces(NStr::ToLower(type_temp));
        int taxid = 0;
        if (type_temp == "mapview" || type_temp == "mapview_prev" || 
            type_temp == "gsfasta") {
            taxid = 
                CBlastFormatUtil::GetTaxidForSeqid(aln_id, *m_ScopeRef);
        }
       
        sdl->id_url = s_GetIdUrlNew(*ids, sdl->gi, user_url,
                                 m_IsDbNa, m_Database, 
                                 (m_Option & eNewTargetWindow) ? true : false,
                                 m_Rid, m_QueryNumber, taxid, sdl->linkout);
    }

  
    sdl->defline = GetTitle(m_ScopeRef->GetBioseqHandle(*(sdl->id)));
    if (!(bdl.empty())) { 
        for(list< CRef< CBlast_def_line > >::const_iterator iter = bdl.begin();
            iter != bdl.end(); iter++){
            const CBioseq::TId& cur_id = (*iter)->GetSeqid();
            int cur_gi =  FindGi(cur_id);
            int gi_in_use_this_gi = 0;
            ITERATE(list<int>, iter_gi, use_this_gi){
                if(cur_gi == *iter_gi){
                    gi_in_use_this_gi = *iter_gi;                 
                    break;
                }
            }
            if(use_this_gi.empty() || gi_in_use_this_gi > 0) {
                
                if((*iter)->IsSetTitle()){
                    bool id_used_already = false;
                    ITERATE(CBioseq::TId, iter_id, cur_id) {
                        if ((*iter_id)->Match(*(sdl->id))) {
                            id_used_already = true;
                            break;
                        }
                    }
                    if (!id_used_already) {
                        string concat_acc;
                        wid = FindBestChoice(cur_id, CSeq_id::WorstRank);
                        wid->GetLabel(&concat_acc, CSeq_id::eFasta, 0);
                        if( (m_Option & eShowGi) && cur_gi > 0){
                            sdl->defline =  sdl->defline + " >" + "gi|" + 
                                NStr::IntToString(cur_gi) + "|" + 
                                concat_acc + " " + (*iter)->GetTitle();
                        } else {
                            sdl->defline = sdl->defline + " >" + concat_acc +
                                " " + 
                                (*iter)->GetTitle();
                        }
                    }
                }
            }
        }
    }
}


//Constructor
CShowBlastDefline::CShowBlastDefline(const CSeq_align_set& seqalign,
                                     CScope& scope,
                                     size_t line_length,
                                     size_t num_defline_to_show,
                                     bool translated_nuc_alignment):
    m_AlnSetRef(&seqalign), 
    m_ScopeRef(&scope),
    m_LineLen(line_length),
    m_NumToShow(num_defline_to_show),
    m_TranslatedNucAlignment(translated_nuc_alignment)
{
    
    m_Option = 0;
    m_EntrezTerm = NcbiEmptyString;
    m_QueryNumber = 0;
    m_Rid = NcbiEmptyString;
    m_CddRid = NcbiEmptyString;
    m_IsDbNa = true;
    m_BlastType = NcbiEmptyString;
    m_PsiblastStatus = eFirstPass;
    m_SeqStatus = NULL;
    m_Ctx = NULL;

}

CShowBlastDefline::~CShowBlastDefline()
{
   
    ITERATE(list<SDeflineInfo*>, iter, m_DeflineList){
        delete *iter;
    }
}

void CShowBlastDefline::DisplayBlastDefline(CNcbiOstream & out)
{
    /*Note we can't just show each alnment as we go because we will 
      need to show defline only once for all hsp's with the same id*/
    
    bool is_first_aln = true;
    size_t num_align = 0;
    CConstRef<CSeq_id> previous_id, subid;
    size_t max_score_len = kBits.size(), max_evalue_len = kValue.size();
    size_t max_sum_n_len =1;

    if(m_Option & eHtml){
        m_ConfigFile.reset(new CNcbiIfstream(".ncbirc"));
        m_Reg.reset(new CNcbiRegistry(*m_ConfigFile));  
    }
    bool master_is_na = false;
    //prepare defline
    CSeq_align_set actual_aln_list;
    CBlastFormatUtil::ExtractSeqalignSetFromDiscSegs(actual_aln_list, 
                                                     *m_AlnSetRef);

    for (CSeq_align_set::Tdata::const_iterator 
             iter = actual_aln_list.Get().begin(); 
         iter != actual_aln_list.Get().end() && num_align < m_NumToShow; 
         iter++){
        if (is_first_aln) {
            master_is_na = m_ScopeRef->GetBioseqHandle((*iter)->GetSeq_id(0)).
                GetBioseqCore()->IsNa();
        }
        subid = &((*iter)->GetSeq_id(1));
        if(is_first_aln || (!is_first_aln && !subid->Match(*previous_id))) {
            SDeflineInfo* sdl = x_GetDeflineInfo(**iter);
            if(sdl){
                m_DeflineList.push_back(sdl);
                if(max_score_len < sdl->bit_string.size()){
                    max_score_len = sdl->bit_string.size();
                }
                if(max_evalue_len < sdl->evalue_string.size()){
                    max_evalue_len = sdl->evalue_string.size();
                }

                if( max_sum_n_len < NStr::IntToString(sdl->sum_n).size()){
                    max_sum_n_len = NStr::IntToString(sdl->sum_n).size();
                }
            }
            num_align++;
        }
        is_first_aln = false;
        previous_id = subid;
      
    }
    //actual output
    if((m_Option & eLinkout) && (m_Option & eHtml) && !m_IsDbNa && !master_is_na){
        ITERATE(list<SDeflineInfo*>, iter, m_DeflineList){
            if((*iter)->linkout & eStructure){
                char buf[512];
                sprintf(buf, kStructure_Overview.c_str(), m_Rid.c_str(),
                        0, 0, m_CddRid.c_str(), "overview",
                        m_EntrezTerm == NcbiEmptyString ?
                        m_EntrezTerm.c_str() : "none");
                out << buf <<endl<<endl;
                break;
            }
        }
    }

    if(!(m_Option & eNoShowHeader)) {
        if((m_PsiblastStatus == eFirstPass) ||
           (m_PsiblastStatus == eRepeatPass)){
            CBlastFormatUtil::AddSpace(out, m_LineLen + kTwoSpaceMargin.size());
            if(m_Option & eHtml){
                if((m_Option & eShowNewSeqGif)){
                    out << kPsiblastNewSeqBackgroundGif;
                    out << kPsiblastCheckedBackgroundGif;
                }
                if (m_Option & eCheckbox) {
                    out << kPsiblastNewSeqBackgroundGif;
                    out << kPsiblastCheckedBackgroundGif;
                }
            }
            out << kScore;
            CBlastFormatUtil::AddSpace(out, max_score_len - kScore.size());
            CBlastFormatUtil::AddSpace(out, kTwoSpaceMargin.size());
            CBlastFormatUtil::AddSpace(out, 2); //E align to l of value
            out << kE;
            out << endl;
            out << kHeader;
            if(m_Option & eHtml){
                if((m_Option & eShowNewSeqGif)){
                    out << kPsiblastNewSeqBackgroundGif;
                    out << kPsiblastCheckedBackgroundGif;
                }
                if (m_Option & eCheckbox) {
                    out << kPsiblastNewSeqBackgroundGif;
                    out << kPsiblastCheckedBackgroundGif;
                }
            }
            CBlastFormatUtil::AddSpace(out, m_LineLen - kHeader.size());
            CBlastFormatUtil::AddSpace(out, kOneSpaceMargin.size());
            out << kBits;
            //in case max_score_len > kBits.size()
            CBlastFormatUtil::AddSpace(out, max_score_len - kBits.size()); 
            CBlastFormatUtil::AddSpace(out, kTwoSpaceMargin.size());
            out << kValue;
            if(m_Option & eShowSumN){
                CBlastFormatUtil::AddSpace(out, max_evalue_len - kValue.size()); 
                CBlastFormatUtil::AddSpace(out, kTwoSpaceMargin.size());
                out << kN;
            }
            out << endl<<endl;
        }
        if(m_PsiblastStatus == eRepeatPass){
            out << kRepeatHeader <<endl;
        }
        if(m_PsiblastStatus == eNewPass){
            out << kNewSeqHeader <<endl;
        }
    }
    
    bool first_new =true;
    ITERATE(list<SDeflineInfo*>, iter, m_DeflineList){
        size_t line_length = 0;
        string line_component;
        if ((m_Option & eHtml) && ((*iter)->gi > 0)){
            if((m_Option & eShowNewSeqGif)) { 
                if ((*iter)->is_new) {
                    if (first_new) {
                        first_new = false;
                        out << kPsiblastEvalueLink;
                    }
                    out << kPsiblastNewSeqGif;
                
                } else {
                    out << kPsiblastNewSeqBackgroundGif;
                }
                if ((*iter)->was_checked) {
                    out << kPsiblastCheckedGif;
                    
                } else {
                    out << kPsiblastCheckedBackgroundGif;
                }
            }
            char buf[256];
            if((m_Option & eCheckboxChecked)){
                sprintf(buf, kPsiblastCheckboxChecked.c_str(), (*iter)->gi, 
                        (*iter)->gi);
                out << buf;
            } else if (m_Option & eCheckbox) {
                sprintf(buf, kPsiblastCheckbox.c_str(), (*iter)->gi);
                out << buf;
            }
        }
        
        
        if((m_Option & eHtml) && ((*iter)->id_url != NcbiEmptyString)) {
            out << (*iter)->id_url;
        }
        if(m_Option & eShowGi){
            if((*iter)->gi > 0){
                line_component = "gi|" + NStr::IntToString((*iter)->gi) + "|";
                out << line_component;
                line_length += line_component.size();
            }
        }
        if(!(*iter)->id.Empty()){
            if(!((*iter)->id->AsFastaString().find("gnl|BL_ORD_ID") 
                 != string::npos)){
                out << (*iter)->id->AsFastaString();
                line_length += (*iter)->id->AsFastaString().size();
            }
        }
        if((m_Option & eHtml) && ((*iter)->id_url != NcbiEmptyString)) {
            out << "</a>";
        }        
        line_component = "  " + (*iter)->defline; 
        string actual_line_component;
        if(line_component.size() > m_LineLen){
            actual_line_component = line_component.substr(0, m_LineLen - 
                                                          line_length - 3);
            actual_line_component += "...";
        } else {
            actual_line_component = line_component.substr(0, m_LineLen - 
                                                          line_length);
        }
        if (m_Option & eHtml) {
            out << CHTMLHelper::HTMLEncode(actual_line_component);
        } else {
            out << actual_line_component; 
        }
        line_length += actual_line_component.size();
        //pad the short lines
        CBlastFormatUtil::AddSpace(out, m_LineLen - line_length);
        out << kTwoSpaceMargin;
       
        if((m_Option & eHtml) && ((*iter)->score_url != NcbiEmptyString)) {
            out << (*iter)->score_url;
        }
        out << (*iter)->bit_string;
        if((m_Option & eHtml) && ((*iter)->score_url != NcbiEmptyString)) {
            out << "</a>";
        }   
        CBlastFormatUtil::AddSpace(out, max_score_len - (*iter)->bit_string.size());
        out << kTwoSpaceMargin << (*iter)->evalue_string;
        CBlastFormatUtil::AddSpace(out, max_evalue_len - (*iter)->evalue_string.size());
        if(m_Option & eShowSumN){ 
            out << kTwoSpaceMargin << (*iter)->sum_n;   
            CBlastFormatUtil::AddSpace(out, max_sum_n_len - 
                     NStr::IntToString((*iter)->sum_n).size());
        }
        if((m_Option & eLinkout) && (m_Option & eHtml)){
            bool is_first = true;
            ITERATE(list<string>, iter_linkout, (*iter)->linkout_list){
                if(is_first){
                    out << kOneSpaceMargin;
                    is_first = false;
                }
                out << *iter_linkout;
            }
        }
        out <<endl;
    }
}


static void s_DisplayDescrColumnHeader(CNcbiOstream & out,
                                       int currDisplaySort,
                                       string query_buf,  
                                       int columnDisplSort,
                                       int columnHspSort,
                                       string columnText,
                                       int max_data_len,
                                       bool html)
                                       

{
    if (html) {				
        if(currDisplaySort == columnDisplSort) {
            out << "<th class=\"sel\">";		
        }
        else {
            out << "<th>";
        }	
        
        out << "<a href=\"Blast.cgi?"
            << "CMD=Get&" << query_buf 
            << "&DISPLAY_SORT=" << columnDisplSort
            << "&HSP_SORT=" << columnHspSort
            << "#sort_mark\">";
        
    }
    out << columnText;
    if (html) {        
        out << "</a></th>" << endl;
    }
    else {
        CBlastFormatUtil::AddSpace(out, max_data_len - columnText.size());
        CBlastFormatUtil::AddSpace(out, kTwoSpaceMargin.size());
    }
    
}

void CShowBlastDefline::DisplayBlastDeflineTable(CNcbiOstream & out)
{
    /*Note we can't just show each alnment as we go because we will 
      need to show defline only once for all hsp's with the same id*/
    
    bool is_first_aln = true;
    size_t num_align = 0;
    CConstRef<CSeq_id> previous_id, subid;
    size_t max_score_len = kMaxScore.size(), max_evalue_len = kValue.size();
    size_t max_sum_n_len =1;
    size_t max_total_score_len = kTotal.size();
    size_t max_percent_identity_len = kIdentity.size();
    int percent_identity = 0;
    size_t max_query_cover_len = kCoverage.size();

    if(m_Option & eHtml){
        m_ConfigFile.reset(new CNcbiIfstream(".ncbirc"));
        m_Reg.reset(new CNcbiRegistry(*m_ConfigFile));  
    }

    CSeq_align_set hit;
    int query_length = 1;
    bool master_is_na = false;
    //prepare defline
    CSeq_align_set actual_aln_list;
    CBlastFormatUtil::ExtractSeqalignSetFromDiscSegs(actual_aln_list, 
                                                     *m_AlnSetRef);

    for (CSeq_align_set::Tdata::const_iterator 
             iter = actual_aln_list.Get().begin(); 
         iter != actual_aln_list.Get().end() && num_align < m_NumToShow; 
         iter++){
        if (is_first_aln) {
            query_length = m_ScopeRef->GetBioseqHandle((*iter)->GetSeq_id(0)).
                GetBioseqLength();
            master_is_na = m_ScopeRef->GetBioseqHandle((*iter)->GetSeq_id(0)).
                GetBioseqCore()->IsNa();
        }

        subid = &((*iter)->GetSeq_id(1));
      
        if(!is_first_aln && !(subid->Match(*previous_id))) {
            SDeflineInfo* sdl = x_GetHitDeflineInfo(hit);
            if(sdl){
                m_DeflineList.push_back(sdl);
                if(max_score_len < sdl->bit_string.size()){
                    max_score_len = sdl->bit_string.size();
                }
                if(max_total_score_len < sdl->total_bit_string.size()){
                    max_total_score_len = sdl->total_bit_string.size();
                }
                percent_identity = 100*sdl->match/sdl->align_length;
                if(max_percent_identity_len < NStr::IntToString(percent_identity).size()) {
                    max_percent_identity_len = NStr::IntToString(percent_identity).size();
                }
                if(max_evalue_len < sdl->evalue_string.size()){
                    max_evalue_len = sdl->evalue_string.size();
                }
                
                if( max_sum_n_len < NStr::IntToString(sdl->sum_n).size()){
                    max_sum_n_len = NStr::IntToString(sdl->sum_n).size();
                }
                hit.Set().clear();
            }
          
            num_align++;
      
        }
        if (num_align < m_NumToShow) { //no adding if number to show already reached
            hit.Set().push_back(*iter);
        }
        is_first_aln = false;
        previous_id = subid;
    }

    //the last hit
    SDeflineInfo* sdl = x_GetHitDeflineInfo(hit);
    if(sdl){
        m_DeflineList.push_back(sdl);
        if(max_score_len < sdl->bit_string.size()){
            max_score_len = sdl->bit_string.size();
        }
        if(max_total_score_len < sdl->total_bit_string.size()){
            max_score_len = sdl->total_bit_string.size();
        }
        percent_identity = 100*sdl->match/sdl->align_length;
        if(max_percent_identity_len < NStr::IntToString(percent_identity).size()) {
            max_percent_identity_len =  NStr::IntToString(percent_identity).size();
        }
        if(max_evalue_len < sdl->evalue_string.size()){
            max_evalue_len = sdl->evalue_string.size();
        }
        
        if( max_sum_n_len < NStr::IntToString(sdl->sum_n).size()){
            max_sum_n_len = NStr::IntToString(sdl->sum_n).size();
        }
        hit.Set().clear();
    }
             
    //actual output
    if((m_Option & eLinkout) && (m_Option & eHtml) && !m_IsDbNa && !master_is_na){
        ITERATE(list<SDeflineInfo*>, iter, m_DeflineList){
            if((*iter)->linkout & eStructure){
                char buf[512];
                sprintf(buf, kStructure_Overview.c_str(), m_Rid.c_str(),
                        0, 0, m_CddRid.c_str(), "overview",
                        m_EntrezTerm == NcbiEmptyString ?
                        m_EntrezTerm.c_str() : "none");
                out << buf <<endl<<endl;
                break;
            }
        }
    }
	//This is max number of columns in the table - later should be probably put in enum DisplayOption
	int tableColNumber = 9;
    //if(!(m_Option & eNoShowHeader)) {
        if((m_PsiblastStatus == eFirstPass) ||
           (m_PsiblastStatus == eRepeatPass)){
            //CBlastFormatUtil::AddSpace(out, m_LineLen + 1);
			///???
            if(m_Option & eHtml){
                if((m_Option & eShowNewSeqGif)){
                    out << kPsiblastNewSeqBackgroundGif;
                    out << kPsiblastCheckedBackgroundGif;
                }
                if (m_Option & eCheckbox) {
                    out << kPsiblastNewSeqBackgroundGif;
                    out << kPsiblastCheckedBackgroundGif;
                }
            }
            //This is done instead of code displaying titles			
            if(!(m_Option & eNoShowHeader)) {
                
                if(m_Option & eHtml){
                
                    out << "<b>";
                }
                out << kHeader << endl;
                if(m_Option & eHtml){
                    out << "</b>";			
                    out << "(Click headers to sort columns)" << endl;
                }
            }
            if(m_Option & eHtml){
                out << "<div id=\"desctbl\">" << "<table id=\"descs\">" << endl << "<thead>" << endl;
                out << "<tr class=\"first\">" << endl << "<th>Accession</th>" << endl << "<th>Description</th>" << endl;			
            }
            /*
            if(m_Option & eHtml){
                if((m_Option & eShowNewSeqGif)){
                    out << kPsiblastNewSeqBackgroundGif;
                    out << kPsiblastCheckedBackgroundGif;
                }
                if (m_Option & eCheckbox) {
                    out << kPsiblastNewSeqBackgroundGif;
                    out << kPsiblastCheckedBackgroundGif;
                }
            }
			
			
            CBlastFormatUtil::AddSpace(out, m_LineLen - kHeader.size());
            CBlastFormatUtil::AddSpace(out, kTwoSpaceMargin.size());
			*/

            string query_buf; 
            map< string, string> parameters_to_change;
            parameters_to_change.insert(map<string, string>::
                                        value_type("DISPLAY_SORT", ""));
            parameters_to_change.insert(map<string, string>::
                                        value_type("HSP_SORT", ""));
            CBlastFormatUtil::BuildFormatQueryString(*m_Ctx, 
                                                     parameters_to_change,
                                                     query_buf);
        
            parameters_to_change.clear();

            string display_sort_value = m_Ctx->GetRequestValue("DISPLAY_SORT").
                GetValue();
            int display_sort = display_sort_value == NcbiEmptyString ? 
                CBlastFormatUtil::eEvalue : NStr::StringToInt(display_sort_value);
            
            s_DisplayDescrColumnHeader(out,display_sort,query_buf,CBlastFormatUtil::eHighestScore,CBlastFormatUtil::eScore,kMaxScore,max_score_len,m_Option & eHtml);

            s_DisplayDescrColumnHeader(out,display_sort,query_buf,CBlastFormatUtil::eTotalScore,CBlastFormatUtil::eScore,kTotalScore,max_total_score_len,m_Option & eHtml);
            s_DisplayDescrColumnHeader(out,display_sort,query_buf,CBlastFormatUtil::eQueryCoverage,CBlastFormatUtil::eHspEvalue,kCoverage,max_query_cover_len,m_Option & eHtml);
            s_DisplayDescrColumnHeader(out,display_sort,query_buf,CBlastFormatUtil::eEvalue,CBlastFormatUtil::eHspEvalue,kEvalue,max_evalue_len,m_Option & eHtml);
            if(m_Option & eShowPercentIdent){
                s_DisplayDescrColumnHeader(out,display_sort,query_buf,CBlastFormatUtil::ePercentIdentity,CBlastFormatUtil::eHspPercentIdentity,kIdentity,max_percent_identity_len,m_Option & eHtml);
            }else {
                tableColNumber--;
            }
           
            if(m_Option & eShowSumN){
                // CBlastFormatUtil::AddSpace(out, kTwoSpaceMargin.size());
                out << "<th>" << kN << "</th>" << endl;
                
            }
            //out << endl<<endl;
            if (m_Option & eLinkout) {
                out << "<th>Links</th>" << endl;
                out << "</tr>" << endl;
                out << "</thead>" << endl;
            }
        }
		//????
        /*      if(m_PsiblastStatus == eRepeatPass){
            out << kRepeatHeader <<endl;
        }
        if(m_PsiblastStatus == eNewPass){
            out << kNewSeqHeader <<endl;
            }*/
    //}
    
    bool first_new =true;
    int prev_database_type = 0, cur_database_type = 0;
    bool is_first = true;
    bool is_mixed_database = CBlastFormatUtil::
        IsMixedDatabase(*m_AlnSetRef, *m_ScopeRef);

    map< string, string> parameters_to_change;
    string query_buf;
    if (is_mixed_database && m_Option & eHtml) {
        parameters_to_change.insert(map<string, string>::
                                    value_type("DATABASE_SORT", ""));
        CBlastFormatUtil::BuildFormatQueryString(*m_Ctx, 
                                                 parameters_to_change,
                                                 query_buf);
    }
	if (m_Option & eHtml) {
		out << "<tbody>" << endl;
	}
    ITERATE(list<SDeflineInfo*>, iter, m_DeflineList){
        size_t line_length = 0;
        string line_component;
        cur_database_type = ((*iter)->linkout & eGenomicSeq);
        if (is_mixed_database) {
			if (is_first) {
                if (m_Option & eHtml) {
				    out << "<tr>" << endl << "<th colspan=\"" << tableColNumber<< "\" class=\"l sp\">";
			    }
                if (cur_database_type) {
                    out << "Genomic sequences";                    
                } else {
                    out << "Transcripts";                    
                }
				if (!(m_Option & eHtml)) {					
					out << ":" << endl;
				}
                if (m_Option & eHtml) {
				    out << "</th></tr>" << endl;
			    }
            } else if (prev_database_type != cur_database_type) {				
                if (m_Option & eHtml) {
				    out << "<tr>" << endl << "<th colspan=\"" << tableColNumber<< "\" class=\"l sp\">";
			    }
				if (cur_database_type) {
                    out << "Genomic sequences";					
				} else {
                    out << "Transcripts";
				}                
				if (m_Option & eHtml) {
					out << "<span class=\"slink\">"
						<< " [<a href=\"Blast.cgi?CMD=Get&"
                        << query_buf 
                        << "&DATABASE_SORT=";					
					if (cur_database_type) {
						out << CBlastFormatUtil::eGenomicFirst;				
					} else {
						out << CBlastFormatUtil::eNonGenomicFirst;
					}
					out << "#sort_mark\">show first</a>]</span>";
				}
				else {
					out << ":" << endl;
				}				
                if (m_Option & eHtml) {
				    out << "</th></tr>" << endl;
			    }
            }			
        }
        prev_database_type = cur_database_type;
        is_first = false;
		if (m_Option & eHtml) {
			out << "<tr>" << endl;
			out << "<td class=\"l\">" << endl;
		}
        if ((m_Option & eHtml) && ((*iter)->gi > 0)){
            if((m_Option & eShowNewSeqGif)) { 
                if ((*iter)->is_new) {
                    if (first_new) {
                        first_new = false;
                        out << kPsiblastEvalueLink;
                    }
                    out << kPsiblastNewSeqGif;
                
                } else {
                    out << kPsiblastNewSeqBackgroundGif;
                }
                if ((*iter)->was_checked) {
                    out << kPsiblastCheckedGif;
                    
                } else {
                    out << kPsiblastCheckedBackgroundGif;
                }
            }
            char buf[256];
            if((m_Option & eCheckboxChecked)){
                sprintf(buf, kPsiblastCheckboxChecked.c_str(), (*iter)->gi, 
                        (*iter)->gi);
                out << buf;
            } else if (m_Option & eCheckbox) {
                sprintf(buf, kPsiblastCheckbox.c_str(), (*iter)->gi);
                out << buf;
            }
        }
        
        
        if((m_Option & eHtml) && ((*iter)->id_url != NcbiEmptyString)) {
            out << (*iter)->id_url;
        }
        if(m_Option & eShowGi){
            if((*iter)->gi > 0){
                line_component = "gi|" + NStr::IntToString((*iter)->gi) + "|";
                out << line_component;
                line_length += line_component.size();
            }
        }
        if(!(*iter)->id.Empty()){
            if(!((*iter)->id->AsFastaString().find("gnl|BL_ORD_ID") 
                 != string::npos)){
                string id_str;
                (*iter)->id->GetLabel(&id_str, CSeq_id::eContent);
                out << id_str;
                line_length += id_str.size();
            }
        }
        if((m_Option & eHtml) && ((*iter)->id_url != NcbiEmptyString)) {
            out << "</a>";
        }
		if (m_Option & eHtml) {
			out << "</td><td class=\"lim l\"><div class=\"lim\">";
		}
        line_component = "  " + (*iter)->defline; 
        string actual_line_component;
        actual_line_component = line_component;
        /*
        if(line_component.size() > m_LineLen){
            actual_line_component = line_component.substr(0, m_LineLen - 
                                                          line_length - 3);
            actual_line_component += "...";
        } else {
            actual_line_component = line_component.substr(0, m_LineLen - 
                                                          line_length);
        }
        */
        if (m_Option & eHtml) {
            out << CHTMLHelper::HTMLEncode(actual_line_component);
			out << "</div></td><td>";
        } else {
            out << actual_line_component; 
        }
		/*
        line_length += actual_line_component.size();
        //pad the short lines
		if (!(m_Option & eHtml)) {
			CBlastFormatUtil::AddSpace(out, m_LineLen - line_length);
			out << kTwoSpaceMargin;
		}
		*/
        if((m_Option & eHtml) && ((*iter)->score_url != NcbiEmptyString)) {
            out << (*iter)->score_url;
        }
        out << (*iter)->bit_string;
        if((m_Option & eHtml) && ((*iter)->score_url != NcbiEmptyString)) {
            out << "</a>";
        }   
		if(m_Option & eHtml) {
			out << "</td>";
			out << "<td>" << (*iter)->total_bit_string << "</td>";
		}
		if (!(m_Option & eHtml)) {
			CBlastFormatUtil::AddSpace(out, max_score_len - (*iter)->bit_string.size());

			out << kTwoSpaceMargin << kOneSpaceMargin << (*iter)->total_bit_string;
			CBlastFormatUtil::AddSpace(out, max_total_score_len - 
                                   (*iter)->total_bit_string.size());
		}
		
        int percent_coverage = 100*(*iter)->master_covered_lenghth/query_length;
		if (m_Option & eHtml) {
			out << "<td>" << percent_coverage << "%</td>";
		}
		else {
			out << kTwoSpaceMargin << percent_coverage << "%";
        
			//minus one due to % sign
			CBlastFormatUtil::AddSpace(out, max_query_cover_len - 
			                           NStr::IntToString(percent_coverage).size() - 1);
		}

		if (m_Option & eHtml) {        
			out << "<td>" << (*iter)->evalue_string << "</td>";
		}
		else {
			out << kTwoSpaceMargin << (*iter)->evalue_string;
			CBlastFormatUtil::AddSpace(out, max_evalue_len - (*iter)->evalue_string.size());
		}
        if(m_Option & eShowPercentIdent){
            percent_identity = 100*(*iter)->match/(*iter)->align_length;
            if (m_Option & eHtml) {        
                out << "<td>" << percent_identity << "%</td>";
            }
            else {
                out << kTwoSpaceMargin << percent_identity <<"%";
                
                CBlastFormatUtil::AddSpace(out, max_percent_identity_len - 
                                           NStr::IntToString(percent_identity).size());
            }
        }
		//???
        if(m_Option & eShowSumN){ 
            if (m_Option & eHtml) {
                out << "<td>";
            }
            out << kTwoSpaceMargin << (*iter)->sum_n;   
            if (m_Option & eHtml) {
                out << "</td>";
            } else {
                CBlastFormatUtil::AddSpace(out, max_sum_n_len - 
                                           NStr::IntToString((*iter)->sum_n).size());
            }
        }
	
        if((m_Option & eLinkout) && (m_Option & eHtml)){
               
            out << "<td>";
            bool is_first = true;
            ITERATE(list<string>, iter_linkout, (*iter)->linkout_list){
                if(is_first){
                    out << kOneSpaceMargin;
                    is_first = false;
                }
                out << *iter_linkout;
            }
            out << "</td>";
        }
        if (m_Option & eHtml) {
            out << "</tr>";
        }
        if (!(m_Option & eHtml)) {        
            out <<endl;
        }
    }
	if (m_Option & eHtml) {
		out << "</tbody>" << endl << "</table>" << "</div>" << endl;
	}
}

CShowBlastDefline::SDeflineInfo* 
CShowBlastDefline::x_GetDeflineInfo(const CSeq_align& aln)
{
    SDeflineInfo* sdl = NULL;
    const CSeq_id& id = aln.GetSeq_id(1); 
    string evalue_buf, bit_score_buf, total_bit_score_buf;
    int score = 0;
    double bits = 0;
    double evalue = 0;
    int sum_n = 0;
    int num_ident = 0;
    list<int> use_this_gi;       
      
    try{
        const CBioseq_Handle& handle = m_ScopeRef->GetBioseqHandle(id);
      
        CBlastFormatUtil::GetAlnScores(aln, score, bits, evalue, sum_n, 
                                       num_ident, use_this_gi);
        CBlastFormatUtil::GetScoreString(evalue, bits, 0, evalue_buf, 
                                         bit_score_buf, total_bit_score_buf);
        sdl = new SDeflineInfo;

        x_FillDeflineAndId(handle, id, use_this_gi, sdl);
        sdl->sum_n = sum_n == -1 ? 1:sum_n ;
        sdl->bit_string = bit_score_buf;
        sdl->evalue_string = evalue_buf;
    } catch (CException& ){
        sdl = new SDeflineInfo;
        CBlastFormatUtil::GetAlnScores(aln, score, bits, evalue, sum_n,
                                       num_ident, use_this_gi);
        CBlastFormatUtil::GetScoreString(evalue, bits, 0, evalue_buf, 
                                         bit_score_buf, total_bit_score_buf);
        sdl->sum_n = sum_n == -1 ? 1:sum_n;
        sdl->bit_string = bit_score_buf;
        sdl->evalue_string = evalue_buf;
        sdl->defline = "Unknown";
        sdl->is_new = false;
        sdl->was_checked = false;
        sdl->linkout = 0;
        
        if(id.Which() == CSeq_id::e_Gi){
            sdl->gi = id.GetGi();
        } else {            
            sdl->id = &id;
            sdl->gi = 0;
        }
        if(m_Option & eHtml){
            string user_url= m_Reg->Get(m_BlastType, "TOOL_URL");
            CBioseq::TId ids;
            CRef<CSeq_id> id_ref;
            id_ref = &(const_cast<CSeq_id&>(id));
            ids.push_back(id_ref);
            sdl->id_url = s_GetIdUrl(ids, sdl->gi, user_url,
                                     m_IsDbNa, m_Database, 
                                     (m_Option & eNewTargetWindow) ? 
                                     true : false, m_Rid, m_QueryNumber, 0);
            sdl->score_url = NcbiEmptyString;
        }
    }
    
    return sdl;
}

CShowBlastDefline::SDeflineInfo* 
CShowBlastDefline::x_GetHitDeflineInfo(const CSeq_align_set& aln)
{
  
    SDeflineInfo* sdl = NULL;

    if(aln.Get().empty()){
        return sdl;
    }
        
    const CSeq_id& id = aln.Get().front()->GetSeq_id(1); 
    string evalue_buf, bit_score_buf, total_bit_score_buf;

    int score = 0;
    double bits = 0;
    double total_bits = 0;
    double evalue = 0;
    double highest_bits = 0;
    double lowest_evalue = 0;
    int sum_n = 0;
    int num_ident = 0;
    list<int> use_this_gi;       
    int align_length = 0;
    int highest_identity = 0;
    int highest_ident = 0;
    int highest_length = 1;
    
    int master_covered_lenghth = CBlastFormatUtil::GetMasterCoverage(aln);
    

    ITERATE(CSeq_align_set::Tdata, iter, aln.Get()) {
        align_length = CBlastFormatUtil::GetAlignmentLength(**iter, 
                                                            m_TranslatedNucAlignment);
        CBlastFormatUtil::GetAlnScores(**iter, score, bits, evalue, sum_n, 
                                       num_ident, use_this_gi);  
        
        total_bits += bits;
        
        if (100*num_ident/align_length > highest_identity) {
            highest_length = align_length;
            highest_ident = num_ident;
            highest_identity = 100*num_ident/align_length;
        }
        
        if (bits > highest_bits) {
            highest_bits = bits;
            lowest_evalue = evalue;
        }       
    }
    use_this_gi.clear();
    try{
        const CBioseq_Handle& handle = m_ScopeRef->GetBioseqHandle(id);

        CBlastFormatUtil::GetAlnScores(*(aln.Get().front()), score, bits, evalue, sum_n, 
                                       num_ident, use_this_gi);

        CBlastFormatUtil::GetScoreString(lowest_evalue, highest_bits, total_bits, 
                                         evalue_buf, bit_score_buf, total_bit_score_buf);

        sdl = new SDeflineInfo;

        x_FillDeflineAndIdNew(handle, id, use_this_gi, sdl);
        sdl->sum_n = sum_n == -1 ? 1:sum_n ;
        sdl->bit_string = bit_score_buf;
        sdl->evalue_string = evalue_buf;
        sdl->total_bit_string = total_bit_score_buf;
        
        sdl->match = highest_ident;      
        sdl->align_length = highest_length;
        sdl->master_covered_lenghth = master_covered_lenghth;
    } catch (CException& ){
        sdl = new SDeflineInfo;
        CBlastFormatUtil::GetAlnScores(*(aln.Get().front()), score, bits, evalue, sum_n,
                                       num_ident, use_this_gi);

        CBlastFormatUtil::GetScoreString(lowest_evalue, highest_bits, total_bits, 
                                         evalue_buf,
                                         bit_score_buf, total_bit_score_buf);

        sdl->sum_n = sum_n == -1 ? 1:sum_n;
        sdl->bit_string = bit_score_buf;
        sdl->evalue_string = evalue_buf;
        sdl->defline = "Unknown";
        sdl->is_new = false;
        sdl->was_checked = false;
        sdl->linkout = 0;
        sdl->total_bit_string = total_bit_score_buf;
        sdl->master_covered_lenghth = master_covered_lenghth;
        sdl->match = highest_ident;      
        sdl->align_length = highest_length; 
        if(id.Which() == CSeq_id::e_Gi){
            sdl->gi = id.GetGi();
        } else {            
            sdl->id = &id;
            sdl->gi = 0;
        }
        if(m_Option & eHtml){
            string user_url= m_Reg->Get(m_BlastType, "TOOL_URL");
            CBioseq::TId ids;
            CRef<CSeq_id> id_ref;
            id_ref = &(const_cast<CSeq_id&>(id));
            ids.push_back(id_ref);
            sdl->id_url = s_GetIdUrlNew(ids, sdl->gi, user_url,
                                     m_IsDbNa, m_Database, 
                                     (m_Option & eNewTargetWindow) ? 
                                     true : false, m_Rid, m_QueryNumber, 0, sdl->linkout);
            sdl->score_url = NcbiEmptyString;
        }
    }
    
    return sdl;
}

END_NCBI_SCOPE
