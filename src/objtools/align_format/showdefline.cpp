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
 * Author:  Jian Ye
 *
 * File Description:
 *   Display blast defline
 *
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbireg.hpp>

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

#include <objtools/align_format/showdefline.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>    // for CSeqDB::ExtractBlastDefline

#include <stdio.h>
#include <html/htmlhelper.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(sequence);
BEGIN_SCOPE(align_format)

//margins constant

static string kOneSpaceMargin = " ";
static string kTwoSpaceMargin = "  ";

//const strings
static const string kHeader = "Sequences producing significant alignments:";
static const string kScore = "Score";
static const string kE = "E";
static const string kBits = 
    (getenv("CTOOLKIT_COMPATIBLE") ? "(bits)" : "(Bits)");
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
static const string kEllipsis = "...";

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



string 
CShowBlastDefline::GetSeqIdListString(const list<CRef<objects::CSeq_id> >& id,
                                      bool show_gi)
{
    string id_string = NcbiEmptyString;
    bool found_gi = false;

    CRef<CSeq_id> best_id = FindBestChoice(id, CSeq_id::Score);

    if (show_gi) {
        ITERATE(list<CRef<CSeq_id> >, itr, id) {
             if ((*itr)->IsGi()) {
                id_string += (*itr)->AsFastaString();
                found_gi = true;
                break;
             }
        }
    }

    if (best_id.NotEmpty()  &&  !best_id->IsGi() ) {
         if (found_gi)
             id_string += "|";
        
         if (best_id->IsLocal()) {
             string id_token;
             best_id->GetLabel(&id_token, CSeq_id::eContent, 0);
             id_string += id_token;
         }
         else 
             id_string += best_id->AsFastaString();
    }

    return id_string;
}

void 
CShowBlastDefline::GetSeqIdList(const objects::CBioseq_Handle& bh,
                                list<CRef<objects::CSeq_id> >& ids)
{
    ids.clear();

    
    vector< CConstRef<CSeq_id> > original_seqids;
    
    ITERATE(CBioseq_Handle::TId, itr, bh.GetId()) {
      original_seqids.push_back(itr->GetSeqId());    
    } 

    // Check for ids of type "gnl|BL_ORD_ID". These are the artificial ids
    // created in a BLAST database when it is formatted without indexing.
    // For such ids, create new fake local Seq-ids, saving the first token of 
    // the Bioseq's title, if it's available.
    GetSeqIdList(bh,original_seqids,ids);
}



void 
CShowBlastDefline::GetSeqIdList(const objects::CBioseq_Handle& bh,
                                vector< CConstRef<CSeq_id> > &original_seqids,
                                list<CRef<objects::CSeq_id> >& ids)
{
    ids.clear();
    ITERATE(vector< CConstRef<CSeq_id> >, itr, original_seqids) {
        CRef<CSeq_id> next_seqid(new CSeq_id());
        string id_token = NcbiEmptyString;
        
        if ((*itr)->IsGeneral() &&
            (*itr)->AsFastaString().find("gnl|BL_ORD_ID") 
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
            next_seqid->Assign(**itr);
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
        CSeqDB::ExtractBlastDefline(handle);       

    if(bdlRef.Empty()){    
        list<CRef<objects::CSeq_id> > ids;
        GetSeqIdList(handle, ids);
        seqid = GetSeqIdListString(ids, show_gi);
        defline = GetTitle(handle);
    } else { 
        bdlRef->PutTargetGiFirst(this_gi_first);
        const list< CRef< CBlast_def_line > >& bdl = bdlRef->Get();
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
                                           SDeflineInfo* sdl,
                                           int blast_rank)
{

    const CRef<CBlast_def_line_set> bdlRef = CSeqDB::ExtractBlastDefline(handle);
    const list< CRef< CBlast_def_line > > &bdl = (bdlRef.Empty()) ? list< CRef< CBlast_def_line > >() : bdlRef->Get();
       
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
        PsiblastSeqStatus seq_status = eUnknown;
        
        TIdString2SeqStatus::const_iterator itr = m_SeqStatus->find(aln_id_str);
        if ( itr != m_SeqStatus->end() ){
            seq_status = itr->second;
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
    //get id (sdl->id, sdl-gi)
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
                    if ((*iter_id)->Match(aln_id) 
                      || (aln_id.IsGeneral() && aln_id.GetGeneral().CanGetDb() && 
                         (*iter_id)->IsGeneral() && (*iter_id)->GetGeneral().CanGetDb() &&
                         aln_id.GetGeneral().GetDb() == (*iter_id)->GetGeneral().GetDb())) {
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
                    sdl->linkout = m_LinkoutDB
                        ? m_LinkoutDB->GetLinkout(cur_gi,m_MapViewerBuildName)
                        : 0;
                    if (m_DeflineTemplates == NULL || !m_DeflineTemplates->advancedView)            
                        sdl->linkout_list =
                            CAlignFormatUtil::GetLinkoutUrl(sdl->linkout,
                                           cur_id, m_Rid, 
                                           m_CddRid, 
                                           m_EntrezTerm, 
                                           handle.GetBioseqCore()->IsNa(),
                                           0, true, false,
                                           blast_rank,m_PreComputedResID);                    
                    
                    break;
                }
            } else {
                ITERATE(list<int>, iter_gi, use_this_gi){
                    if(cur_gi == *iter_gi){                     
                        sdl->linkout = m_LinkoutDB
                            ?
                            m_LinkoutDB->GetLinkout(cur_gi,m_MapViewerBuildName)
                            : 0;
                        if (m_DeflineTemplates == NULL || !m_DeflineTemplates->advancedView)
                            sdl->linkout_list = 
                                CAlignFormatUtil::GetLinkoutUrl(sdl->linkout,
                                           cur_id, m_Rid, 
                                           m_CddRid, 
                                           m_EntrezTerm, 
                                           handle.GetBioseqCore()->IsNa(),
                                           0, true, false,
                                           blast_rank,m_PreComputedResID);                        
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
        bool useTemplates = m_DeflineTemplates != NULL;
        bool advancedView = (m_DeflineTemplates != NULL) ? m_DeflineTemplates->advancedView : false;
        string accession;
        sdl->id->GetLabel(&accession, CSeq_id::eContent);
        sdl->score_url = !useTemplates ? "<a href=#" : "";
        sdl->score_url += sdl->gi == 0 ? accession : 
            NStr::IntToString(sdl->gi);
        sdl->score_url += !useTemplates ? ">" : "";

		string user_url = m_Reg.get() ? m_Reg->Get(m_BlastType, "TOOL_URL") : kEmptyStr;        
		//blast_rank = num_align + 1
		CRange<TSeqPos> seqRange = ((int)m_ScoreList.size() >= blast_rank)? m_ScoreList[blast_rank - 1]->subjRange : CRange<TSeqPos>(0,0);
        bool flip = ((int)m_ScoreList.size() >= blast_rank) ? m_ScoreList[blast_rank - 1]->flip : false;        
        CAlignFormatUtil::SSeqURLInfo seqUrlInfo(user_url,m_BlastType,m_IsDbNa,m_Database,m_Rid,
                                                 m_QueryNumber,sdl->gi, accession, sdl->linkout,
                                                 blast_rank,false,(m_Option & eNewTargetWindow) ? true : false,seqRange,flip); 
        seqUrlInfo.resourcesUrl = m_Reg.get() ? m_Reg->Get(m_BlastType, "RESOURCES_URL") : kEmptyStr;
        seqUrlInfo.useTemplates = useTemplates;        
        seqUrlInfo.advancedView = advancedView;
        sdl->id_url = CAlignFormatUtil::GetIDUrl(&seqUrlInfo,aln_id,*m_ScopeRef);        
        if (advancedView){
            if (m_Option & eLinkout && (sdl->gi > 0) ) {                    
                    sdl->linkout_list = CAlignFormatUtil::GetFullLinkoutUrl(bdl,
                                           m_Rid, 
                                           m_CddRid, 
                                           m_EntrezTerm, 
                                           handle.GetBioseqCore()->IsNa(),
                                           sdl->gi,
                                           true, false,
                                           blast_rank,
                                           m_LinkoutOrder,
                                           seqUrlInfo.taxid,
                                           m_Database,
                                           m_QueryNumber,                                                 
                                           user_url,
                                           m_PreComputedResID, m_LinkoutDB,
                                           m_MapViewerBuildName);
            }
            //Links to GenBank,FASTA,Graphics
            list <string> customLinksList = 
            CAlignFormatUtil::GetCustomLinksList(&seqUrlInfo,
                           aln_id,
                           *m_ScopeRef);                           
            sdl->linkout_list.splice(sdl->linkout_list.begin(),customLinksList);
        }
    }

    //get defline
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
                                     bool translated_nuc_alignment,
                                     CRange<TSeqPos>* master_range):
                                     
    m_AlnSetRef(&seqalign), 
    m_ScopeRef(&scope),
    m_LineLen(line_length),
    m_NumToShow(num_defline_to_show),
    m_TranslatedNucAlignment(translated_nuc_alignment),
    m_SkipFrom(-1),
    m_SkipTo(-1),
    m_MasterRange(master_range),
    m_LinkoutDB(NULL)
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
    m_StructureLinkout = false;
    if(m_MasterRange) {
        if(m_MasterRange->GetFrom() >= m_MasterRange->GetTo()) {
            m_MasterRange = NULL;
        }
    }
    m_DeflineTemplates = NULL;
}

CShowBlastDefline::~CShowBlastDefline()
{   
    ITERATE(vector<SScoreInfo*>, iter, m_ScoreList){
        delete *iter;
    }
}


void CShowBlastDefline::Init(void)
{
	if (m_DeflineTemplates != NULL) {
        x_InitDeflineTable();        
    }
    else {        
        x_InitDefline();
    }
}


void CShowBlastDefline::Display(CNcbiOstream & out)
{
    if (m_DeflineTemplates != NULL) {        
        x_DisplayDeflineTableTemplate(out);        
    }
    else {        
        x_DisplayDefline(out);
    }
}

bool CShowBlastDefline::x_CheckForStructureLink()
{
      bool struct_linkout = false;
      int count = 0;
      const int k_CountMax = 200; // Max sequences to check.

      ITERATE(vector<SScoreInfo*>, iter, m_ScoreList) {
          const CBioseq_Handle& handle = m_ScopeRef->GetBioseqHandle(*(*iter)->id);
          const CRef<CBlast_def_line_set> bdlRef = CSeqDB::ExtractBlastDefline(handle);          
          const list< CRef< CBlast_def_line > > &bdl = (bdlRef.Empty()) ? list< CRef< CBlast_def_line > >() : bdlRef->Get();
          for(list< CRef< CBlast_def_line > >::const_iterator bdl_iter = bdl.begin();
              bdl_iter != bdl.end() && struct_linkout == false; bdl_iter++){
              if ((*bdl_iter)->IsSetLinks())
              {
                 for (list< int >::const_iterator link_iter = (*bdl_iter)->GetLinks().begin();
                      link_iter != (*bdl_iter)->GetLinks().end(); link_iter++)
                 {
                      if (*link_iter & eStructure) {
                         struct_linkout = true;
                         break;
                      }
                 }
              }
          }
          if (struct_linkout == true || count > k_CountMax)
            break;
          count++;
      }
      return struct_linkout;
}

//size_t max_score_len = kBits.size(), max_evalue_len = kValue.size();
//size_t max_sum_n_len =1;
//size_t m_MaxScoreLen , m_MaxEvalueLen,m_MaxSumNLen;
//bool m_StructureLinkout
void CShowBlastDefline::x_InitDefline(void)
{
    /*Note we can't just show each alnment as we go because we will 
      need to show defline only once for all hsp's with the same id*/
    
    bool is_first_aln = true;
    size_t num_align = 0;
    CConstRef<CSeq_id> previous_id, subid;

    m_MaxScoreLen = kBits.size();
    m_MaxEvalueLen = kValue.size();
    m_MaxSumNLen =1;
    

    if(m_Option & eHtml){
        m_ConfigFile.reset(new CNcbiIfstream(".ncbirc"));
        m_Reg.reset(new CNcbiRegistry(*m_ConfigFile));  
    }
    bool master_is_na = false;
    //prepare defline

    int ialn = 0;
    for (CSeq_align_set::Tdata::const_iterator 
             iter = m_AlnSetRef->Get().begin();
         iter != m_AlnSetRef->Get().end() && num_align < m_NumToShow; 
         iter++, ialn++){
        if (ialn < m_SkipTo && ialn >= m_SkipFrom) continue;

        if (is_first_aln) {
            _ASSERT(m_ScopeRef);
            CBioseq_Handle bh = m_ScopeRef->GetBioseqHandle((*iter)->GetSeq_id(0));
            _ASSERT(bh);
            master_is_na = bh.GetBioseqCore()->IsNa();
        }
        subid = &((*iter)->GetSeq_id(1));
        if(is_first_aln || (!is_first_aln && !subid->Match(*previous_id))) {
            SScoreInfo* sci = x_GetScoreInfo(**iter, num_align);
            if(sci){
                m_ScoreList.push_back(sci);
                if(m_MaxScoreLen < sci->bit_string.size()){
                    m_MaxScoreLen = sci->bit_string.size();
                }
                if(m_MaxEvalueLen < sci->evalue_string.size()){
                    m_MaxEvalueLen = sci->evalue_string.size();
                }

                if( m_MaxSumNLen < NStr::IntToString(sci->sum_n).size()){
                    m_MaxSumNLen = NStr::IntToString(sci->sum_n).size();
                }
            }
            num_align++;
        }
        is_first_aln = false;
        previous_id = subid;
      
    }

    
    if((m_Option & eLinkout) && (m_Option & eHtml) && !m_IsDbNa && !master_is_na)
        m_StructureLinkout = x_CheckForStructureLink();
}



void CShowBlastDefline::x_DisplayDefline(CNcbiOstream & out)
{
    if(!(m_Option & eNoShowHeader)) {
        if((m_PsiblastStatus == eFirstPass) ||
           (m_PsiblastStatus == eRepeatPass)){
            CAlignFormatUtil::AddSpace(out, m_LineLen + kTwoSpaceMargin.size());
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
            CAlignFormatUtil::AddSpace(out, m_MaxScoreLen - kScore.size());
            CAlignFormatUtil::AddSpace(out, kTwoSpaceMargin.size());
            CAlignFormatUtil::AddSpace(out, 2); //E align to l of value
            out << kE;
            out << "\n";
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
            CAlignFormatUtil::AddSpace(out, m_LineLen - kHeader.size());
            CAlignFormatUtil::AddSpace(out, kOneSpaceMargin.size());
            out << kBits;
            //in case m_MaxScoreLen > kBits.size()
            CAlignFormatUtil::AddSpace(out, m_MaxScoreLen - kBits.size()); 
            CAlignFormatUtil::AddSpace(out, kTwoSpaceMargin.size());
            out << kValue;
            if(m_Option & eShowSumN){
                CAlignFormatUtil::AddSpace(out, m_MaxEvalueLen - kValue.size()); 
                CAlignFormatUtil::AddSpace(out, kTwoSpaceMargin.size());
                out << kN;
            }
            out << "\n";
        }
        if(m_PsiblastStatus == eRepeatPass){
            out << kRepeatHeader << "\n";
        }
        if(m_PsiblastStatus == eNewPass){
            out << kNewSeqHeader << "\n";
        }
        out << "\n";
    }
    
    bool first_new =true;
    ITERATE(vector<SScoreInfo*>, iter, m_ScoreList){
        SDeflineInfo* sdl = x_GetDeflineInfo((*iter)->id, (*iter)->use_this_gi, (*iter)->blast_rank);
        size_t line_length = 0;
        string line_component;
        if ((m_Option & eHtml) && (sdl->gi > 0)){
            if((m_Option & eShowNewSeqGif)) { 
                if (sdl->is_new) {
                    if (first_new) {
                        first_new = false;
                        out << kPsiblastEvalueLink;
                    }
                    out << kPsiblastNewSeqGif;
                
                } else {
                    out << kPsiblastNewSeqBackgroundGif;
                }
                if (sdl->was_checked) {
                    out << kPsiblastCheckedGif;
                    
                } else {
                    out << kPsiblastCheckedBackgroundGif;
                }
            }
            char buf[256];
            if((m_Option & eCheckboxChecked)){
                sprintf(buf, kPsiblastCheckboxChecked.c_str(), sdl->gi, 
                        sdl->gi);
                out << buf;
            } else if (m_Option & eCheckbox) {
                sprintf(buf, kPsiblastCheckbox.c_str(), sdl->gi);
                out << buf;
            }
        }
        
        
        if((m_Option & eHtml) && (sdl->id_url != NcbiEmptyString)) {
            out << sdl->id_url;
        }
        if(m_Option & eShowGi){
            if(sdl->gi > 0){
                line_component = "gi|" + NStr::IntToString(sdl->gi) + "|";
                out << line_component;
                line_length += line_component.size();
            }
        }
        if(!sdl->id.Empty()){
            if(!(sdl->id->AsFastaString().find("gnl|BL_ORD_ID") 
                 != string::npos)){
                out << sdl->id->AsFastaString();
                line_length += sdl->id->AsFastaString().size();
            }
        }
        if((m_Option & eHtml) && (sdl->id_url != NcbiEmptyString)) {
            out << "</a>";
        }        
        line_component = "  " + sdl->defline; 
        string actual_line_component;
        if(line_component.size()+line_length > m_LineLen){
            actual_line_component = line_component.substr(0, m_LineLen - 
                                                          line_length - 3);
            actual_line_component += kEllipsis;
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
        CAlignFormatUtil::AddSpace(out, m_LineLen - line_length);
        out << kTwoSpaceMargin;
       
        if((m_Option & eHtml) && (sdl->score_url != NcbiEmptyString)) {
            out << sdl->score_url;
        }
        out << (*iter)->bit_string;
        if((m_Option & eHtml) && (sdl->score_url != NcbiEmptyString)) {
            out << "</a>";
        }   
        CAlignFormatUtil::AddSpace(out, m_MaxScoreLen - (*iter)->bit_string.size());
        out << kTwoSpaceMargin << (*iter)->evalue_string;
        CAlignFormatUtil::AddSpace(out, m_MaxEvalueLen - (*iter)->evalue_string.size());
        if(m_Option & eShowSumN){ 
            out << kTwoSpaceMargin << (*iter)->sum_n;   
            CAlignFormatUtil::AddSpace(out, m_MaxSumNLen - 
                     NStr::IntToString((*iter)->sum_n).size());
        }
        if((m_Option & eLinkout) && (m_Option & eHtml)){
            bool is_first = true;
            ITERATE(list<string>, iter_linkout, sdl->linkout_list){
                if(is_first){
                    out << kOneSpaceMargin;
                    is_first = false;
                }
                out << *iter_linkout;
            }
        }
        out <<"\n";
        delete sdl;
    }
}

void CShowBlastDefline::DisplayBlastDefline(CNcbiOstream & out)
{
    x_InitDefline();
    if(m_StructureLinkout){        
        char buf[512];
        string  mapCDDParams = (NStr::Find(m_CddRid,"data_cache") != NPOS) ? "" : "blast_CD_RID=" + m_CddRid;        
        sprintf(buf, kStructure_Overview.c_str(), m_Rid.c_str(),
                        0, 0, mapCDDParams.c_str(), "overview",
                        m_EntrezTerm == NcbiEmptyString ?
                        m_EntrezTerm.c_str() : "none");
        out << buf <<"\n\n";  
    }
    x_DisplayDefline(out);
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
        out << "</a></th>\n";
    }
    else {
        CAlignFormatUtil::AddSpace(out, max_data_len - columnText.size());
        CAlignFormatUtil::AddSpace(out, kTwoSpaceMargin.size());
    }
    
}

void CShowBlastDefline::x_InitDeflineTable(void)
{
    /*Note we can't just show each alnment as we go because we will 
      need to show defline only once for all hsp's with the same id*/
    
    bool is_first_aln = true;
    size_t num_align = 0;
    CConstRef<CSeq_id> previous_id, subid;
    m_MaxScoreLen = kMaxScore.size();
    m_MaxEvalueLen = kValue.size();
    m_MaxSumNLen =1;
    m_MaxTotalScoreLen = kTotal.size();
    m_MaxPercentIdentityLen = kIdentity.size();
    int percent_identity = 0;
    m_MaxQueryCoverLen = kCoverage.size();

    if(m_Option & eHtml){
        m_ConfigFile.reset(new CNcbiIfstream(".ncbirc"));
        m_Reg.reset(new CNcbiRegistry(*m_ConfigFile));        
        if(!m_BlastType.empty()) m_LinkoutOrder = m_Reg->Get(m_BlastType,"LINKOUT_ORDER");
        m_LinkoutOrder = (!m_LinkoutOrder.empty()) ? m_LinkoutOrder : kLinkoutOrderStr;
    }

    CSeq_align_set hit;
    m_QueryLength = 1;
    bool master_is_na = false;
    //prepare defline

    int ialn = 0;
    for (CSeq_align_set::Tdata::const_iterator 
             iter = m_AlnSetRef->Get().begin();
         iter != m_AlnSetRef->Get().end() && num_align < m_NumToShow; 
         iter++, ialn++){

        if (ialn < m_SkipTo && ialn >= m_SkipFrom) continue;
        if (is_first_aln) {
            m_QueryLength = m_MasterRange ? 
                m_MasterRange->GetLength() :
                m_ScopeRef->GetBioseqHandle((*iter)->GetSeq_id(0)).GetBioseqLength();
            master_is_na = m_ScopeRef->GetBioseqHandle((*iter)->GetSeq_id(0)).
                GetBioseqCore()->IsNa();
        }

        subid = &((*iter)->GetSeq_id(1));
      
        // This if statement is working on the last CSeq_align_set, stored in "hit"
        // This is confusing and the loop should probably be restructured at some point.
        if(!is_first_aln && !(subid->Match(*previous_id))) {
            SScoreInfo* sci = x_GetScoreInfoForTable(hit, num_align);
            if(sci){
                m_ScoreList.push_back(sci);
                if(m_MaxScoreLen < sci->bit_string.size()){
                    m_MaxScoreLen = sci->bit_string.size();
                }
                if(m_MaxTotalScoreLen < sci->total_bit_string.size()){
                    m_MaxTotalScoreLen = sci->total_bit_string.size();
                }
                percent_identity = CAlignFormatUtil::GetPercentMatch(sci->match,sci->align_length);
                if(m_MaxPercentIdentityLen < NStr::IntToString(percent_identity).size()) {
                    m_MaxPercentIdentityLen = NStr::IntToString(percent_identity).size();
                }
                if(m_MaxEvalueLen < sci->evalue_string.size()){
                    m_MaxEvalueLen = sci->evalue_string.size();
                }
                
                if( m_MaxSumNLen < NStr::IntToString(sci->sum_n).size()){
                    m_MaxSumNLen = NStr::IntToString(sci->sum_n).size();
                }
                hit.Set().clear();
            }
          
            num_align++; // Only increment if new subject ID found.
        }
        if (num_align < m_NumToShow) { //no adding if number to show already reached
            hit.Set().push_back(*iter);
        }
        is_first_aln = false;
        previous_id = subid;
    }

    //the last hit
    SScoreInfo* sci = x_GetScoreInfoForTable(hit, num_align);
    if(sci){
         m_ScoreList.push_back(sci);
        if(m_MaxScoreLen < sci->bit_string.size()){
            m_MaxScoreLen = sci->bit_string.size();
        }
        if(m_MaxTotalScoreLen < sci->total_bit_string.size()){
            m_MaxScoreLen = sci->total_bit_string.size();
        }
        percent_identity = CAlignFormatUtil::GetPercentMatch(sci->match,sci->align_length);
        if(m_MaxPercentIdentityLen < NStr::IntToString(percent_identity).size()) {
            m_MaxPercentIdentityLen =  NStr::IntToString(percent_identity).size();
        }
        if(m_MaxEvalueLen < sci->evalue_string.size()){
            m_MaxEvalueLen = sci->evalue_string.size();
        }
        
        if( m_MaxSumNLen < NStr::IntToString(sci->sum_n).size()){
            m_MaxSumNLen = NStr::IntToString(sci->sum_n).size();
        }
        hit.Set().clear();
    }

    if((m_Option & eLinkout) && (m_Option & eHtml) && !m_IsDbNa && !master_is_na)
        m_StructureLinkout = x_CheckForStructureLink();
 
}

void CShowBlastDefline::x_DisplayDeflineTable(CNcbiOstream & out)
{
    //This is max number of columns in the table - later should be probably put in enum DisplayOption        
        if((m_PsiblastStatus == eFirstPass) ||
           (m_PsiblastStatus == eRepeatPass)){

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
                out << kHeader << "\n";
                if(m_Option & eHtml){
                    out << "</b>";            
                    out << "(Click headers to sort columns)\n";
                }
            }
            if(m_Option & eHtml){
                out << "<div id=\"desctbl\">" << "<table id=\"descs\">" << "\n" << "<thead>" << "\n";
                out << "<tr class=\"first\">" << "\n" << "<th>Accession</th>" << "\n" << "<th>Description</th>" << "\n";            
            }
            
            string query_buf; 
            map< string, string> parameters_to_change;
            parameters_to_change.insert(map<string, string>::
                                        value_type("DISPLAY_SORT", ""));
            parameters_to_change.insert(map<string, string>::
                                        value_type("HSP_SORT", ""));
            CAlignFormatUtil::BuildFormatQueryString(*m_Ctx, 
                                                     parameters_to_change,
                                                     query_buf);
        
            parameters_to_change.clear();

            string display_sort_value = m_Ctx->GetRequestValue("DISPLAY_SORT").
                GetValue();
            int display_sort = display_sort_value == NcbiEmptyString ? 
                CAlignFormatUtil::eEvalue : NStr::StringToInt(display_sort_value);
            
            s_DisplayDescrColumnHeader(out,display_sort,query_buf,CAlignFormatUtil::eHighestScore,CAlignFormatUtil::eScore,kMaxScore,m_MaxScoreLen,m_Option & eHtml);

            s_DisplayDescrColumnHeader(out,display_sort,query_buf,CAlignFormatUtil::eTotalScore,CAlignFormatUtil::eScore,kTotalScore,m_MaxTotalScoreLen,m_Option & eHtml);
            s_DisplayDescrColumnHeader(out,display_sort,query_buf,CAlignFormatUtil::eQueryCoverage,CAlignFormatUtil::eHspEvalue,kCoverage,m_MaxQueryCoverLen,m_Option & eHtml);
            s_DisplayDescrColumnHeader(out,display_sort,query_buf,CAlignFormatUtil::eEvalue,CAlignFormatUtil::eHspEvalue,kEvalue,m_MaxEvalueLen,m_Option & eHtml);
            if(m_Option & eShowPercentIdent){
                s_DisplayDescrColumnHeader(out,display_sort,query_buf,CAlignFormatUtil::ePercentIdentity,CAlignFormatUtil::eHspPercentIdentity,kIdentity,m_MaxPercentIdentityLen,m_Option & eHtml);
            }else {             
            }
           
            if(m_Option & eShowSumN){    
                out << "<th>" << kN << "</th>" << "\n";
                
            }            
            if (m_Option & eLinkout) {
                out << "<th>Links</th>\n";
                out << "</tr>\n";
                out << "</thead>\n";
            }
        }        
    
    if (m_Option & eHtml) {
        out << "<tbody>\n";
    }
    
    x_DisplayDeflineTableBody(out);
    
    if (m_Option & eHtml) {
    out << "</tbody>\n</table></div>\n";
    }
}

void CShowBlastDefline::x_DisplayDeflineTableBody(CNcbiOstream & out)
{
    int percent_identity = 0;
    int tableColNumber = (m_Option & eShowPercentIdent) ? 9 : 8;    
    bool first_new =true;
    int prev_database_type = 0, cur_database_type = 0;
    bool is_first = true;
    // Mixed db is genomic + transcript and this does not apply to proteins.
    bool is_mixed_database = false;
    if (m_IsDbNa == true)       
       is_mixed_database = CAlignFormatUtil::IsMixedDatabase(*m_Ctx);

    map< string, string> parameters_to_change;
    string query_buf;
    if (is_mixed_database && m_Option & eHtml) {
        parameters_to_change.insert(map<string, string>::
                                    value_type("DATABASE_SORT", ""));
        CAlignFormatUtil::BuildFormatQueryString(*m_Ctx, 
                                                 parameters_to_change,
                                                 query_buf);
    }
    ITERATE(vector<SScoreInfo*>, iter, m_ScoreList){
        SDeflineInfo* sdl = x_GetDeflineInfo((*iter)->id, (*iter)->use_this_gi, (*iter)->blast_rank);
        size_t line_length = 0;
        string line_component;
        cur_database_type = (sdl->linkout & eGenomicSeq);
        if (is_mixed_database) {
            if (is_first) {
                if (m_Option & eHtml) {
                    out << "<tr>\n<th colspan=\"" << tableColNumber<< "\" class=\"l sp\">";
                }
                if (cur_database_type) {
                    out << "Genomic sequences";                    
                } else {
                    out << "Transcripts";                    
                }
                if (!(m_Option & eHtml)) {                    
                    out << ":\n";
                }
                if (m_Option & eHtml) {
                    out << "</th></tr>\n";
                }
            } else if (prev_database_type != cur_database_type) {                
                if (m_Option & eHtml) {
                    out << "<tr>\n<th colspan=\"" << tableColNumber<< "\" class=\"l sp\">";
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
                        out << CAlignFormatUtil::eGenomicFirst;                
                    } else {
                        out << CAlignFormatUtil::eNonGenomicFirst;
                    }
                    out << "#sort_mark\">show first</a>]</span>";
                }
                else {
                    out << ":\n";
                }                
                if (m_Option & eHtml) {
                    out << "</th></tr>\n";
                }
            }            
        }
        prev_database_type = cur_database_type;
        is_first = false;
        if (m_Option & eHtml) {
            out << "<tr>\n";
            out << "<td class=\"l\">\n";
        }
        if ((m_Option & eHtml) && (sdl->gi > 0)){
            if((m_Option & eShowNewSeqGif)) { 
                if (sdl->is_new) {
                    if (first_new) {
                        first_new = false;
                        out << kPsiblastEvalueLink;
                    }
                    out << kPsiblastNewSeqGif;
                
                } else {
                    out << kPsiblastNewSeqBackgroundGif;
                }
                if (sdl->was_checked) {
                    out << kPsiblastCheckedGif;
                    
                } else {
                    out << kPsiblastCheckedBackgroundGif;
                }
            }
            char buf[256];
            if((m_Option & eCheckboxChecked)){
                sprintf(buf, kPsiblastCheckboxChecked.c_str(), sdl->gi, 
                        sdl->gi);
                out << buf;
            } else if (m_Option & eCheckbox) {
                sprintf(buf, kPsiblastCheckbox.c_str(), sdl->gi);
                out << buf;
            }
        }
        
        
        if((m_Option & eHtml) && (sdl->id_url != NcbiEmptyString)) {
            out << sdl->id_url;
        }
        if(m_Option & eShowGi){
            if(sdl->gi > 0){
                line_component = "gi|" + NStr::IntToString(sdl->gi) + "|";
                out << line_component;
                line_length += line_component.size();
            }
        }
        if(!sdl->id.Empty()){
            if(!(sdl->id->AsFastaString().find("gnl|BL_ORD_ID") 
                 != string::npos)){
                string id_str;
                sdl->id->GetLabel(&id_str, CSeq_id::eContent);
                out << id_str;
                line_length += id_str.size();
            }
        }
        if((m_Option & eHtml) && (sdl->id_url != NcbiEmptyString)) {
            out << "</a>";
        }
        if (m_Option & eHtml) {
            out << "</td><td class=\"lim l\"><div class=\"lim\">";
        }
        line_component = "  " + sdl->defline; 
        string actual_line_component;
        actual_line_component = line_component;
        
        if (m_Option & eHtml) {
            out << CHTMLHelper::HTMLEncode(actual_line_component);
            out << "</div></td><td>";
        } else {
            out << actual_line_component; 
        }
        
        if((m_Option & eHtml) && (sdl->score_url != NcbiEmptyString)) {
            out << sdl->score_url;
        }
        out << (*iter)->bit_string;
        if((m_Option & eHtml) && (sdl->score_url != NcbiEmptyString)) {
            out << "</a>";
        }   
        if(m_Option & eHtml) {
            out << "</td>";
            out << "<td>" << (*iter)->total_bit_string << "</td>";
        }
        if (!(m_Option & eHtml)) {
            CAlignFormatUtil::AddSpace(out, m_MaxScoreLen - (*iter)->bit_string.size());

            out << kTwoSpaceMargin << kOneSpaceMargin << (*iter)->total_bit_string;
            CAlignFormatUtil::AddSpace(out, m_MaxTotalScoreLen - 
                                   (*iter)->total_bit_string.size());
        }
        
                int percent_coverage = 100*(*iter)->master_covered_length/m_QueryLength;
        if (m_Option & eHtml) {
            out << "<td>" << percent_coverage << "%</td>";
        }
        else {
            out << kTwoSpaceMargin << percent_coverage << "%";
        
            //minus one due to % sign
            CAlignFormatUtil::AddSpace(out, m_MaxQueryCoverLen - 
                                       NStr::IntToString(percent_coverage).size() - 1);
        }

        if (m_Option & eHtml) {        
            out << "<td>" << (*iter)->evalue_string << "</td>";
        }
        else {
            out << kTwoSpaceMargin << (*iter)->evalue_string;
            CAlignFormatUtil::AddSpace(out, m_MaxEvalueLen - (*iter)->evalue_string.size());
        }
        if(m_Option & eShowPercentIdent){            
            percent_identity = CAlignFormatUtil::GetPercentMatch((*iter)->match,(*iter)->align_length);
            if (m_Option & eHtml) {        
                out << "<td>" << percent_identity << "%</td>";
            }
            else {
                out << kTwoSpaceMargin << percent_identity <<"%";
                
                CAlignFormatUtil::AddSpace(out, m_MaxPercentIdentityLen - 
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
                CAlignFormatUtil::AddSpace(out, m_MaxSumNLen - 
                                           NStr::IntToString((*iter)->sum_n).size());
            }
        }
    
        if((m_Option & eLinkout) && (m_Option & eHtml)){
               
            out << "<td>";
            bool first_time = true;
            ITERATE(list<string>, iter_linkout, sdl->linkout_list){
                if(first_time){
                    out << kOneSpaceMargin;
                    first_time = false;
                }
                out << *iter_linkout;
            }
            out << "</td>";
        }
        if (m_Option & eHtml) {
            out << "</tr>";
        }
        if (!(m_Option & eHtml)) {        
            out <<"\n";
        }
        delete sdl;
    }
}


void CShowBlastDefline::DisplayBlastDeflineTable(CNcbiOstream & out)
{
    x_InitDeflineTable();
    if(m_StructureLinkout){        
        char buf[512];
        sprintf(buf, kStructure_Overview.c_str(), m_Rid.c_str(),
                        0, 0, m_CddRid.c_str(), "overview",
                        m_EntrezTerm == NcbiEmptyString ?
                        m_EntrezTerm.c_str() : "none");
        out << buf <<"\n\n";        
    }    
    x_DisplayDeflineTable(out);
}

CShowBlastDefline::SScoreInfo* 
CShowBlastDefline::x_GetScoreInfo(const CSeq_align& aln, int blast_rank)
{
    string evalue_buf, bit_score_buf, total_bit_score_buf, raw_score_buf;
    int score = 0;
    double bits = 0;
    double evalue = 0;
    int sum_n = 0;
    int num_ident = 0;
    list<int> use_this_gi; 

    use_this_gi.clear();

    CAlignFormatUtil::GetAlnScores(aln, score, bits, evalue, sum_n, 
                                       num_ident, use_this_gi);

    CAlignFormatUtil::GetScoreString(evalue, bits, 0, score,
                              evalue_buf, bit_score_buf, total_bit_score_buf,
                              raw_score_buf);

    auto_ptr<SScoreInfo> score_info(new SScoreInfo);
    score_info->sum_n = sum_n == -1 ? 1:sum_n ;
    score_info->id = &(aln.GetSeq_id(1));

    score_info->use_this_gi = use_this_gi;

    score_info->bit_string = bit_score_buf;
    score_info->raw_score_string = raw_score_buf;
    score_info->evalue_string = evalue_buf;
    score_info->id = &(aln.GetSeq_id(1));
    score_info->blast_rank = blast_rank+1;		
    score_info->subjRange = CRange<TSeqPos>(0,0);	
    score_info->flip = false;
    return score_info.release();
}

CShowBlastDefline::SScoreInfo* 
CShowBlastDefline::x_GetScoreInfoForTable(const CSeq_align_set& aln, int blast_rank)
{
    string evalue_buf, bit_score_buf, total_bit_score_buf, raw_score_buf;
    int score = 0;
    double bits = 0;
    double evalue = 0;
    int sum_n = 0;
    int num_ident = 0;
    SScoreInfo* score_info = NULL;

    if(aln.Get().empty())
        return score_info;

    score_info = x_GetScoreInfo(*(aln.Get().front()), blast_rank); 

    double total_bits = 0;
    double highest_bits = 0;
    double lowest_evalue = 0;
    int highest_length = 1;
    int highest_ident = 0;
    int highest_identity = 0;
    list<int> use_this_gi;   // Not used here, but needed for GetAlnScores.    
    score_info->subjRange = CAlignFormatUtil::GetSeqAlignCoverageParams(aln,&score_info->master_covered_length,&score_info->flip);
												
    ITERATE(CSeq_align_set::Tdata, iter, aln.Get()) {
        int align_length = CAlignFormatUtil::GetAlignmentLength(**iter, 
                                                        m_TranslatedNucAlignment);
        CAlignFormatUtil::GetAlnScores(**iter, score, bits, evalue, sum_n, 
                                   num_ident, use_this_gi);  
        use_this_gi.clear();
    
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
    score_info->match = highest_ident;      
    score_info->align_length = highest_length;

    // Really need to call this twice??
    CAlignFormatUtil::GetScoreString(lowest_evalue, highest_bits, total_bits, score,
                                     evalue_buf, bit_score_buf, total_bit_score_buf, raw_score_buf);

    score_info->total_bit_string = total_bit_score_buf; 
    score_info->bit_string = bit_score_buf;
    score_info->evalue_string = evalue_buf;
    score_info->hspNum = aln.Size();	

    return score_info;
}

vector <CShowBlastDefline::SDeflineInfo*> 
CShowBlastDefline::GetDeflineInfo(vector< CConstRef<CSeq_id> > &seqIds)
{
    vector <CShowBlastDefline::SDeflineInfo*>  sdlVec;
    for(size_t i = 0; i < seqIds.size(); i++) {
        list<int> use_this_gi;
        CShowBlastDefline::SDeflineInfo* sdl = x_GetDeflineInfo(seqIds[i], use_this_gi, i + 1 );
        sdlVec.push_back(sdl);        
    }
    return sdlVec;
}



CShowBlastDefline::SDeflineInfo* 
CShowBlastDefline::x_GetDeflineInfo(CConstRef<CSeq_id> id, list<int>& use_this_gi, int blast_rank)
{
    SDeflineInfo* sdl = NULL;
      
    try{
        const CBioseq_Handle& handle = m_ScopeRef->GetBioseqHandle(*id);
        sdl = new SDeflineInfo;
        x_FillDeflineAndId(handle, *id, use_this_gi, sdl, blast_rank);
    } catch (const CException&){
        sdl = new SDeflineInfo;
        sdl->defline = "Unknown";
        sdl->is_new = false;
        sdl->was_checked = false;
        sdl->linkout = 0;
        
        if((*id).Which() == CSeq_id::e_Gi){
            sdl->gi = (*id).GetGi();
        } else {                        
            sdl->gi = 0;
        }
        sdl->id = id;
        if(m_Option & eHtml){
            _ASSERT(m_Reg.get());

            string user_url= m_Reg->Get(m_BlastType, "TOOL_URL");
            string accession;
            sdl->id->GetLabel(&accession, CSeq_id::eContent);
            CRange<TSeqPos> seqRange(0,0);
            CAlignFormatUtil::SSeqURLInfo seqUrlInfo(user_url,m_BlastType,m_IsDbNa,m_Database,m_Rid,
                                                     m_QueryNumber,sdl->gi,accession,0,blast_rank,false,(m_Option & eNewTargetWindow) ? true : false,seqRange,false,0);
            sdl->id_url = CAlignFormatUtil::GetIDUrl(&seqUrlInfo,*id,*m_ScopeRef);			
            sdl->score_url = NcbiEmptyString;
        }
    }
    
    return sdl;
}

void CShowBlastDefline::x_DisplayDeflineTableTemplate(CNcbiOstream & out)                                                    
{
    bool first_new =true;
    int prev_database_type = 0, cur_database_type = 0;
    bool is_first = true;

    // Mixed db is genomic + transcript and this does not apply to proteins.        
    bool is_mixed_database = (m_IsDbNa == true)? CAlignFormatUtil::IsMixedDatabase(*m_Ctx): false;    
    string rowType = "odd";
    string subHeaderID;  
    ITERATE(vector<SScoreInfo*>, iter, m_ScoreList){
        SDeflineInfo* sdl = x_GetDeflineInfo((*iter)->id, (*iter)->use_this_gi, (*iter)->blast_rank);
        cur_database_type = (sdl->linkout & eGenomicSeq);
        string subHeader;             
        bool formatHeaderSort = !is_first && (prev_database_type != cur_database_type);
        if (is_mixed_database && (is_first || formatHeaderSort)) {            
            subHeader = x_FormatSeqSetHeaders(cur_database_type, formatHeaderSort);
            subHeaderID = cur_database_type ? "GnmSeq" : "Transcr";
            //This is done for 508 complience
            subHeader = CAlignFormatUtil::MapTemplate(subHeader,"defl_header_id",subHeaderID); 
        }                
        prev_database_type = cur_database_type;
                            
        string defLine = x_FormatDeflineTableLine(sdl,*iter,first_new);

        //This is done for 508 complience
        defLine = CAlignFormatUtil::MapTemplate(defLine,"defl_header_id",subHeaderID); 

        string firstSeq = (is_first) ? "firstSeq" : "";
        defLine = CAlignFormatUtil::MapTemplate(defLine,"firstSeq",firstSeq); 
        defLine = CAlignFormatUtil::MapTemplate(defLine,"trtp",rowType); 

        rowType = (rowType == "odd") ? "even" : "odd";
        
        if(!subHeader.empty()) {
            defLine = subHeader + defLine;
        }
        is_first = false;
        out << defLine;
        
        delete sdl;
    }
}

string CShowBlastDefline::x_FormatSeqSetHeaders(int isGenomicSeq, bool formatHeaderSort)
{
    string seqSetType = isGenomicSeq ? "Genomic sequences" : "Transcripts";    
    string subHeader = CAlignFormatUtil::MapTemplate(m_DeflineTemplates->subHeaderTmpl,"defl_seqset_type",seqSetType);
    if (formatHeaderSort) {
        int database_sort = isGenomicSeq ? CAlignFormatUtil::eGenomicFirst : CAlignFormatUtil::eNonGenomicFirst;
        string deflnSubHeaderSort = CAlignFormatUtil::MapTemplate(m_DeflineTemplates->subHeaderSort,"database_sort",database_sort);
        subHeader = CAlignFormatUtil::MapTemplate(subHeader,"defl_header_sort",deflnSubHeaderSort);        
    }
    else {
        subHeader = CAlignFormatUtil::MapTemplate(subHeader,"defl_header_sort","");
    }
    return subHeader;            
}


string CShowBlastDefline::x_FormatDeflineTableLine(SDeflineInfo* sdl,SScoreInfo* iter,bool &first_new)
{
    string defLine = ((sdl->gi > 0) && ((m_Option & eCheckboxChecked) || (m_Option & eCheckbox))) ? x_FormatPsi(sdl, first_new) : m_DeflineTemplates->defLineTmpl;   
    string dflGi = (m_Option & eShowGi) && (sdl->gi > 0) ? "gi|" + NStr::IntToString(sdl->gi) + "|" : "";
    string seqid;
    if(!sdl->id.Empty()){
        if(!(sdl->id->AsFastaString().find("gnl|BL_ORD_ID") != string::npos)){
            sdl->id->GetLabel(&seqid, CSeq_id::eContent);            
        }
    }	
    
    if(sdl->id_url != NcbiEmptyString) {        
		string seqInfo  = CAlignFormatUtil::MapTemplate(m_DeflineTemplates->seqInfoTmpl,"dfln_url",sdl->id_url);
        string trgt = (m_Option & eNewTargetWindow) ? "TARGET=\"EntrezView\"" : "";
        seqInfo = CAlignFormatUtil::MapTemplate(seqInfo,"dfln_target",trgt);
        defLine = CAlignFormatUtil::MapTemplate(defLine,"seq_info",seqInfo);        		
        defLine = CAlignFormatUtil::MapTemplate(defLine,"dfln_gi",dflGi);    
        defLine = CAlignFormatUtil::MapTemplate(defLine,"dfln_seqid",seqid);        		
    }
    else {        
        defLine = CAlignFormatUtil::MapTemplate(defLine,"seq_info",dflGi + seqid);        
    }
    
    defLine = CAlignFormatUtil::MapTemplate(defLine,"dfln_defline",CHTMLHelper::HTMLEncode(sdl->defline));

    if(sdl->score_url != NcbiEmptyString) {        
        string scoreInfo  = CAlignFormatUtil::MapTemplate(m_DeflineTemplates->scoreInfoTmpl,"score_url",sdl->score_url);
        scoreInfo = CAlignFormatUtil::MapTemplate(scoreInfo,"bit_string",iter->bit_string);
        scoreInfo = CAlignFormatUtil::MapTemplate(scoreInfo,"score_seqid",seqid);
        defLine = CAlignFormatUtil::MapTemplate(defLine,"score_info",scoreInfo);        
    }
    else {           
        defLine = CAlignFormatUtil::MapTemplate(defLine,"score_info",iter->bit_string);        
    }
    /*****************This block of code is for future use with AJAX begin***************************/ 
    string deflId; 
    if(sdl->gi == 0) {
        string accession;
        sdl->id->GetLabel(& deflId, CSeq_id::eContent);
    }
    else {        
      deflId = NStr::IntToString(sdl->gi);
    }
    defLine = CAlignFormatUtil::MapTemplate(defLine,"dfln_id",deflId);
    defLine = CAlignFormatUtil::MapTemplate(defLine,"dfln_rid",m_Rid);    
    defLine = CAlignFormatUtil::MapTemplate(defLine,"dfln_hspnum",iter->hspNum); 
    defLine = CAlignFormatUtil::MapTemplate(defLine,"dfln_blast_rank",iter->blast_rank); 
	/*****************This block of code is for future use with AJAX end***************************/ 

    defLine = CAlignFormatUtil::MapTemplate(defLine,"total_bit_string",iter->total_bit_string);
        
    int percent_coverage = 100*iter->master_covered_length/m_QueryLength;

    defLine = CAlignFormatUtil::MapTemplate(defLine,"percent_coverage",NStr::IntToString(percent_coverage));

    defLine = CAlignFormatUtil::MapTemplate(defLine,"evalue_string",iter->evalue_string);

    if(m_Option & eShowPercentIdent){
        int percent_identity = CAlignFormatUtil::GetPercentMatch(iter->match, iter->align_length);
        defLine = CAlignFormatUtil::MapTemplate(defLine,"percent_identity",NStr::IntToString(percent_identity));
    }
        
    if(m_Option & eShowSumN){     
        defLine = CAlignFormatUtil::MapTemplate(defLine,"sum_n",NStr::IntToString(iter->sum_n));
    }

	string links;
    //sdl->linkout_list may contain linkouts + mapview link + seqview link
    ITERATE(list<string>, iter_linkout, sdl->linkout_list){        
        links += *iter_linkout;
    }                          
    defLine = CAlignFormatUtil::MapTemplate(defLine,"linkout",links);        
    
    return defLine;
}

string CShowBlastDefline::x_FormatPsi(SDeflineInfo* sdl, bool &first_new)
{
    string defline = m_DeflineTemplates->defLineTmpl;
    string show_new,show_checked,replaceBy;    
    if((m_Option & eShowNewSeqGif)) {         
        replaceBy = (sdl->is_new && first_new) ? m_DeflineTemplates->psiFirstNewAnchorTmpl : "";
        first_new = (sdl->is_new && first_new) ? false : first_new;
        if (!sdl->is_new) {                 
            show_new = "hidden";            
        }
        
        if(!sdl->was_checked) {
            show_checked = "hidden";        
        }

        defline = CAlignFormatUtil::MapTemplate(defline,"first_new",replaceBy);            
        defline = CAlignFormatUtil::MapTemplate(defline,"psi_new_gi",show_new);
        defline = CAlignFormatUtil::MapTemplate(defline,"psi_checked_gi",show_checked);            
    }

    replaceBy = (m_Option & eCheckboxChecked) ? m_DeflineTemplates->psiGoodGiHiddenTmpl : "";//<@psi_good_gi@>        
    defline = CAlignFormatUtil::MapTemplate(defline,"psi_good_gi",replaceBy);

    replaceBy = (m_Option & eCheckboxChecked) ? "checked=\"checked\"" : "";
    defline = CAlignFormatUtil::MapTemplate(defline,"gi_checked",replaceBy);    
    defline = CAlignFormatUtil::MapTemplate(defline,"psiGi",NStr::IntToString(sdl->gi));
    
    return defline;
}

END_SCOPE(align_format)
END_NCBI_SCOPE
