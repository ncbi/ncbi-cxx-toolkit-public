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
 *  @file vectorscreen.cpp
 *   vector screen graphic (using HTML table) 
 *
 */
#include <ncbi_pch.hpp>
#include <objtools/align_format/vectorscreen.hpp>
#include <util/range.hpp>
#include <serial/iterator.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/seqalign_exception.hpp>
#include <objects/general/Object_id.hpp>
#include <html/html.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(align_format)

  
static const int kNumSeqalignMatchTypes = 3;
    
//Note these arrays are in the order strong, moderate, weak
static const int kTerminalMatchScore[kNumSeqalignMatchTypes] = {24, 19, 16}; 
static const int kInternalMatchScore[kNumSeqalignMatchTypes] = {30, 25, 23};
// + 1 for suspected match
static const char* kGif[kNumSeqalignMatchTypes + 2] =
    {"red.gif", "purple.gif", "green.gif", "yellow.gif", "white.gif"};
static const char* kGifLegend[] =
    {"Strong", "Moderate", "Weak", "Suspect"};
static const char* kMatchUrlLegend[] =
    {"Strong match", "Moderate match", "Weak match", "Suspect origin"};

static const TSeqPos kSupectLength = 50;

static const TSeqPos kMasterPixel = 600;
static const TSeqPos kBarHeight = 20;
static const TSeqPos kNumScales = 5;


///Returns a string concerning the strength of the match for a given enum value
string
CVecscreen::GetStrengthString(CVecscreen::MatchType match_type)
{
    if (match_type == CVecscreen::eNoMatch)
        return NcbiEmptyString;
    return string(kGifLegend[match_type]);
}


///group hsp with same id togather
///@param seqalign: the seqalign
///
static void s_RestoreHspPos(CSeq_align_set& seqalign){
    CSeq_align_set::Tdata::iterator next_iter; 
    CSeq_align_set::Tdata::iterator cur_iter = seqalign.Set().begin();
    
    while(cur_iter != seqalign.Set().end()){ 
        bool is_first = true;
        next_iter = cur_iter;
        next_iter ++;
        const CSeq_id& cur_id  = (*cur_iter)->GetSeq_id(1);
        while(next_iter != seqalign.Set().end()){
            //only care the ones starting from the next next one
            //because we don't need to do anything for the next one
            if(is_first){
                next_iter ++;
                is_first = false;
            }
            if(next_iter != seqalign.Set().end()){
                const CSeq_id& next_id  = (*next_iter)->GetSeq_id(1);
                if (cur_id.Match(next_id)){
                    CSeq_align_set::Tdata::iterator temp_iter = next_iter;
                    next_iter ++;
                    //insert after cur_iter
                    cur_iter ++;
                    seqalign.Set().insert(cur_iter, *temp_iter);
                    //move back to the newly inserted one
                    cur_iter --; 
                    seqalign.Set().erase(temp_iter); 
                } else {
                    next_iter ++;
                }
            }
        }
        cur_iter ++;
    } 
   
}
  
///Sort on seqalign range from
///@param info1: the first seqalign
///@param info2: the second seqalign
///
static bool AlnFromRangeAscendingSort(CRef<CSeq_align> const& info1,
                                      CRef<CSeq_align> const& info2)
{
    int score1,  score2, sum_n, num_ident;
    double bits, evalue;
    list<TGi> use_this_gi;
    TSeqPos from1, from2;
    
    CAlignFormatUtil::GetAlnScores(*info1, score1, bits, evalue,
                                   sum_n, num_ident, use_this_gi);
    CAlignFormatUtil::GetAlnScores(*info2, score2, bits, evalue,
                                   sum_n, num_ident, use_this_gi);
    from1 = info1->GetSeqRange(0).GetFrom();
    from2 = info2->GetSeqRange(0).GetFrom();
    if(from1 == from2) {
        return score2 > score1;
    } else {
        return from1 < from2;
    }
}


///Sort on sealign score
///@param info1: the first seqalign
///@param info2: the second seqalign
///
static bool AlnScoreDescendingSort(CRef<CSeq_align> const& info1,
                                  CRef<CSeq_align> const& info2)
{
    int score1,  score2, sum_n, num_ident;
    double bits, evalue;
    list<TGi> use_this_gi;
    
    CAlignFormatUtil::GetAlnScores(*info1, score1, bits, evalue,
                                   sum_n, num_ident, use_this_gi);
    CAlignFormatUtil::GetAlnScores(*info2, score2, bits, evalue,
                                   sum_n, num_ident, use_this_gi);
    
    return (score1 > score2);
}


void CVecscreen::x_GetEdgeRanges(const objects::CSeq_align& seqalign,
                                 TSeqPos master_len,
                                 TSeqPos& start_edge,
                                 TSeqPos& end_edge)
{
    int score, sum_n, num_ident;
    TSeqPos aln_start, aln_stop;
    double bits, evalue;
    list<TGi> use_this_gi;
    
    aln_start = min(seqalign.GetSeqRange(0).GetTo(), 
                    seqalign.GetSeqRange(0).GetFrom());
    aln_stop = max(seqalign.GetSeqRange(0).GetTo(), 
                   seqalign.GetSeqRange(0).GetFrom());
    CAlignFormatUtil::GetAlnScores(seqalign, score, bits, evalue,
                                   sum_n, num_ident,use_this_gi);
    if(aln_start < m_TerminalFlexibility ){
        if (aln_stop > start_edge) {
            if(score >= kTerminalMatchScore[eStrong]){
                start_edge=aln_stop;
            } else if (score >= kTerminalMatchScore[eModerate]){
                start_edge=aln_stop;
            } else if (score >= kTerminalMatchScore[eWeak] && m_ShowWeakMatch){
                start_edge=aln_stop;
            }
        }
    } else if (aln_stop > master_len - 1 - m_TerminalFlexibility){
        if (aln_start < end_edge) {
            if(score >= kTerminalMatchScore[eStrong]){
                end_edge = aln_start;
            } else if (score >= kTerminalMatchScore[eModerate]){
                end_edge = aln_start;
            } else if (score >= kTerminalMatchScore[eWeak] && m_ShowWeakMatch){
                end_edge = aln_start;
            }
            
        }
    }
}

CVecscreen::MatchType CVecscreen::x_GetMatchType(const CSeq_align& seqalign,
                                                 TSeqPos master_len,
                                                 TSeqPos start_edge,
                                                 TSeqPos end_edge)
{
    int score, sum_n, num_ident;
    TSeqPos aln_start, aln_stop;
    double bits, evalue;
    list<TGi> use_this_gi;
    
    aln_start = min(seqalign.GetSeqRange(0).GetTo(), 
                    seqalign.GetSeqRange(0).GetFrom());
    aln_stop = max(seqalign.GetSeqRange(0).GetTo(), 
                   seqalign.GetSeqRange(0).GetFrom());
    CAlignFormatUtil::GetAlnScores(seqalign, score, bits, evalue,
                                   sum_n, num_ident,use_this_gi);
    
    if(aln_start < m_TerminalFlexibility || 
       aln_stop > master_len - 1 - m_TerminalFlexibility){
        //terminal match       
        if(score >= kTerminalMatchScore[eStrong]){
            return eStrong;
        } else if (score >= kTerminalMatchScore[eModerate]){
            return eModerate;
        } else if (score >= kTerminalMatchScore[eWeak] && m_ShowWeakMatch){
            return eWeak;
        }
    } else if ((int)aln_start - (int)start_edge <= 1 || 
               (int)aln_stop + 1 >= (int) end_edge){
        //terminal match if abutting or within another terminal hit       
        if(score >= kTerminalMatchScore[eStrong]){
            return eStrong;
        } else if (score >= kTerminalMatchScore[eModerate]){
            return eModerate;
        } else if (score >= kTerminalMatchScore[eWeak] && m_ShowWeakMatch){
            return eWeak;
        }
    } else {
        //internal match
        if(score >= kInternalMatchScore[eStrong]){
            return eStrong;
        } else if (score >= kInternalMatchScore[eModerate]){
            return eModerate;
        } else if (score >= kInternalMatchScore[eWeak] && m_ShowWeakMatch){
            return eWeak;
        }
    }
    return eNoMatch;
}

void CVecscreen::x_MergeLowerRankSeqalign(CSeq_align_set& seqalign_higher,
                                          CSeq_align_set& seqalign_lower)
{
    //get merged range for higher seqalign
    map<TSeqRange, int> range_to_id; // id is whichever came first, not perfect
    list<CRange<TSeqPos> > range_list;
    CRange<TSeqPos> prev_range, cur_range;
    int j = 0;
    ITERATE(CSeq_align_set::Tdata, iter, seqalign_higher.Get()){ 
        cur_range.Set((*iter)->GetSeqRange(0).GetFrom(), 
                      (*iter)->GetSeqRange(0).GetTo());
        //merge if previous range intersect with current range
        if(j > 0){ 
            prev_range = range_list.back();
            if(prev_range.IntersectingWith(cur_range)){
                range_list.back() = 
                    range_list.back().CombinationWith(cur_range);
                range_to_id[range_list.back()] = range_to_id[prev_range];
            } else {
                range_to_id[cur_range]=x_GetId(**iter);
                range_list.push_back(cur_range);
            }
        } else {
            range_to_id[cur_range]=x_GetId(**iter);
            range_list.push_back(cur_range); //store current range
        }
        j ++;
    }
    
    //merge lower rank seqalign if it's contained in higher rank seqalign
    //or if it's contained in the new range formed by higher and lower
    //seqalign with a higher score 
    seqalign_lower.Set().sort(AlnScoreDescendingSort);
    
    NON_CONST_ITERATE(list<CRange<TSeqPos> >, iter_higher,  range_list){
        CSeq_align_set::Tdata::iterator iter_lower =
            seqalign_lower.Set().begin();
        while(iter_lower != seqalign_lower.Set().end()){
            if((*iter_lower)->GetSeqRange(0).GetFrom() >=
               iter_higher->GetFrom() &&
               (*iter_lower)->GetSeqRange(0).GetTo() <=
               iter_higher->GetTo()){
                
                x_DropToKeepMap[x_GetId(**iter_lower)] = range_to_id[*iter_higher];
                x_IdToDropIdMap[range_to_id[*iter_higher]].push_back(x_GetId(**iter_lower));

                CSeq_align_set::Tdata::iterator temp_iter = iter_lower;
                iter_lower ++;
                seqalign_lower.Set().erase(temp_iter);
            } else if ((*iter_lower)->GetSeqRange(0).
                       IntersectingWith(*iter_higher)){
                int id= range_to_id[*iter_higher];
                CRange<TSeqPos> lower_range = (*iter_lower)->GetSeqRange(0);
                *iter_higher = 
                    iter_higher->CombinationWith(lower_range);
                iter_lower ++;
                range_to_id[*iter_higher]=id;
            }else {
                iter_lower ++;
            }
        }
    }
   
}

CVecscreen::CVecscreen(const CSeq_align_set& seqalign, TSeqPos master_length, TSeqPos terminal_flexibility){
    m_SeqalignSetRef = &seqalign;
    m_ImagePath = "./";
    m_MasterLen = master_length;
    m_FinalSeqalign = new CSeq_align_set;
    m_HelpDocsUrl = "//www.ncbi.nlm.nih.gov/tools/vecscreen/about/";    
    m_ShowWeakMatch = true;
    m_TerminalFlexibility = terminal_flexibility;
}

CVecscreen::~CVecscreen()
{
    ITERATE(list<AlnInfo*>, iter, m_AlnInfoList){
        delete (*iter);
    }
}


void CVecscreen::x_MergeInclusiveSeqalign(CSeq_align_set& seqalign)
{   
    //seqalign is presorted by score already.  Delete ones that are contained
    //in seqaligns with higher scores
    CSeq_align_set::Tdata::iterator next_iter; 
    CSeq_align_set::Tdata::iterator cur_iter = seqalign.Set().begin();
    
    while(cur_iter != seqalign.Set().end()){ 
        next_iter = cur_iter;
        next_iter ++;
        
        CRange<TSeqPos> cur_range = (*cur_iter)->GetSeqRange(0);
        while(next_iter != seqalign.Set().end()){
            CRange<TSeqPos> next_range = (*next_iter)->GetSeqRange(0);
            if (cur_range.GetFrom() <= next_range.GetFrom() &&
                cur_range.GetTo() >= next_range.GetTo()){
                //if cur_range contains next_range
                x_DropToKeepMap[x_GetId(**next_iter)] = x_GetId(**cur_iter);
                x_IdToDropIdMap[x_GetId(**cur_iter)].push_back(x_GetId(**next_iter));
                CSeq_align_set::Tdata::iterator temp_iter = next_iter;
                next_iter ++;
                seqalign.Set().erase(temp_iter); 
            } else if (cur_range.IntersectingWith(next_range)){
                cur_range = 
                    cur_range.CombinationWith(next_range);
                next_iter ++;
            } else {
                next_iter ++;
            }
        }
        cur_iter ++;
    } 
}

void CVecscreen::x_MergeSeqalign(CSeq_align_set& seqalign)
{
   
    //different match types, no eSuspect or eNoMatch
    //as they are not contained in seqalign
    vector<CRef<CSeq_align_set> > catagorized_seqalign(kNumSeqalignMatchTypes);
    for(unsigned int i = 0; i < catagorized_seqalign.size(); i ++){
        catagorized_seqalign[i] = new CSeq_align_set;
    }

    //find edges of terminal hits  

    TSeqPos start_edge = 0, end_dege = m_MasterLen - 1 ;
    ITERATE(CSeq_align_set::Tdata, iter, seqalign.Get()){
        x_GetEdgeRanges(**iter, m_MasterLen, start_edge, end_dege);
    }

    //seperate seqalign with different catagory
    int next_id=1;
    x_OrigAlignsById.push_back(CRef<CSeq_align>()); // zero is not being used
    ITERATE(CSeq_align_set::Tdata, iter, seqalign.Get()){
        MatchType type = x_GetMatchType(**iter, m_MasterLen, start_edge, end_dege);
        if(type != eNoMatch){
            CRef<CSeq_align> new_align(new CSeq_align);
            new_align->Assign(**iter);
            if(new_align->GetSeqStrand(0) == eNa_strand_minus){
                new_align->Reverse();
            }
            new_align->SetNamedScore("vs_match_type", type);
            new_align->SetNamedScore("vs_id", next_id); next_id++;
            x_OrigAlignsById.push_back(new_align);
            catagorized_seqalign[type]->Set().push_back(new_align);
        } 
    }
    
    for(unsigned int i = 0; i < catagorized_seqalign.size(); i ++){
        //sort for x_MergeInclusiveSeqalign
        catagorized_seqalign[i]->Set().sort(AlnScoreDescendingSort);
        x_MergeInclusiveSeqalign(*(catagorized_seqalign[i]));
        //restore alnrangesort
        catagorized_seqalign[i]->Set().sort(AlnFromRangeAscendingSort);
    }
    
    
    for(int i = eStrong; i < kNumSeqalignMatchTypes - 1 ; i ++){
        for(int j = i + 1; j < kNumSeqalignMatchTypes; j ++){
            x_MergeLowerRankSeqalign(*(catagorized_seqalign[i]), 
                                 *(catagorized_seqalign[j]));
        }
    }
    //set final seqalign
    for(unsigned int i = 0; i < catagorized_seqalign.size(); i ++){
        //restore alnrangesort
        catagorized_seqalign[i]->Set().sort(AlnFromRangeAscendingSort);
        ITERATE(CSeq_align_set::Tdata, iter, catagorized_seqalign[i]->Get()){
            m_FinalSeqalign->Set().push_back(*iter);
            
        }
    }
    
    x_BuildNonOverlappingRange(catagorized_seqalign);


    NON_CONST_ITERATE(list<AlnInfo*>, aln_info_iter, m_AlnInfoList) {
        AlnInfo::TAlignList aligns_to_add;
        ITERATE(AlnInfo::TAlignList, aln_iter, (*aln_info_iter)->get_aligns()) {
            set<int> drop_ids;
            x_GetAllDropIdsForKeepId(x_GetId(**aln_iter), drop_ids);
            ITERATE(set<int>, di, drop_ids) {
                const CSeq_align& da = *x_OrigAlignsById[*di];
                aligns_to_add.push_back(x_OrigAlignsById[*di]);
            }
        }
        (*aln_info_iter)->add_drops(aligns_to_add);
    }
}

void CVecscreen::x_BuildHtmlBar(CNcbiOstream& out){
    CRef<CHTML_table> tbl; 
    CRef<CHTML_img> image;
    CHTML_tc* tc;
    double pixel_factor = ((double)kMasterPixel)/m_MasterLen;
    int column = 0;
    
    if(m_AlnInfoList.empty()){
        return;
    }

    //title
    CRef<CHTML_b> b(new CHTML_b);
    b->AppendPlainText("Distribution of Vector Matches on the Query Sequence");
    b->Print(out, CNCBINode::eXHTML);
    out << "\n\n";
 
    tbl = new CHTML_table;
    tbl->SetCellSpacing(0)->SetCellPadding(0)->SetAttribute("border", "0");
    tbl->SetAttribute("width", kNumScales*kMasterPixel/(kNumScales - 1));
    
    //scale bar
    double scale = ((double)m_MasterLen)/(kNumScales - 1);
    for(TSeqPos i = 0; i < kNumScales; i ++){
        CNodeRef font(new CHTML_font(1, true, NStr::IntToString((int)(scale*i == 0?
                                                                1 : scale*i))));
        tc = tbl->InsertAt(0, column, font);
        tc->SetAttribute("align", "LEFT");
        tc->SetAttribute("valign", "CENTER");
        tc->SetAttribute("width", kMasterPixel/(kNumScales - 1));
        column ++;
    }
    tbl->Print(out, CNCBINode::eXHTML);
    //the actual bar
    
    column = 0;
    tbl = new CHTML_table;
    tbl->SetCellSpacing(0)->SetCellPadding(0)->SetAttribute("border", "0");

    int width_adjust = 1;
    ITERATE(list<AlnInfo*>, iter, m_AlnInfoList){     
        double width = (*iter)->range.GetLength()*pixel_factor;
        //rounding to int this way as round() is not portable
        width = width + (width < 0.0 ? -0.5 : 0.5);
        if(((int)width) > 1){ 
            //no show for less than one pixel as the border already
            //looks like one pixel
            //width_adjust to compensate for the border width
           
            image = new CHTML_img(m_ImagePath + kGif[(*iter)->type], 
                                  (int)width - width_adjust, kBarHeight);
            image->SetAttribute("border", 1);
            tc = tbl->InsertAt(0, column, image);
            tc->SetAttribute("align", "LEFT");
            tc->SetAttribute("valign", "CENTER");
            column ++;
        }
    }  
    tbl->Print(out, CNCBINode::eXHTML);
    out << "\n\n";
    
    //legend
    b = new CHTML_b;
    b->AppendPlainText("Match to Vector: ");
    b->Print(out, CNCBINode::eXHTML);
    for(int i = 0; i < kNumSeqalignMatchTypes; i++){
        image = new CHTML_img(m_ImagePath + kGif[i], kBarHeight, kBarHeight);
        image->SetAttribute("border", "1");
        image->Print(out, CNCBINode::eXHTML);
        b = new CHTML_b;
        b->AppendPlainText(string(" ") + kGifLegend[i] + "  ");
        b->Print(out, CNCBINode::eXHTML);
    }
    out << "\n";
    //suspected origin
    b = new CHTML_b;
    b->AppendPlainText("Segment of suspect origin: ");
    b->Print(out, CNCBINode::eXHTML);
    image = new CHTML_img(m_ImagePath + kGif[eSuspect], kBarHeight, kBarHeight);
    image->SetAttribute("border", "1");
    image->Print(out, CNCBINode::eXHTML);
   
    //footnote
    out << "\n\n";
    b = new CHTML_b;
    b->AppendPlainText("Segments matching vector:  ");
    b->Print(out, CNCBINode::eXHTML);
    CRef<CHTML_a> a;
    
    for (int i = 0; i < kNumSeqalignMatchTypes + 1; i ++){
        bool is_first = true;
        ITERATE(list<AlnInfo*>, iter, m_AlnInfoList){ 
            if((*iter)->type == i){
                if(is_first){
                    out << "\n";
                    a = new CHTML_a(m_HelpDocsUrl + "#" + 
                                    kGifLegend[(*iter)->type]);
                    a->SetAttribute("TARGET", "VecScreenInfo");
                    a->AppendPlainText(string(kMatchUrlLegend[(*iter)->type]) + ":");
                    a->Print(out, CNCBINode::eXHTML);
                    is_first = false;
                } else {
                    out << ",";
                }
                if((*iter)->range.GetFrom() == (*iter)->range.GetTo()){
                    out << " " << (*iter)->range.GetFrom() + 1;
                } else {
                    out << " " << (*iter)->range.GetFrom() + 1 << "-"
                        << (*iter)->range.GetTo() + 1;
                }
                
            }
        }
    }
  
    out << "\n\n";
}


CRef<CSeq_align_set> CVecscreen::ProcessSeqAlign(void){
    CSeq_align_set actual_aln_list;
    CAlignFormatUtil::ExtractSeqalignSetFromDiscSegs(actual_aln_list, 
                                                     *m_SeqalignSetRef);
    x_MergeSeqalign(actual_aln_list);  
    //x_BuildHtmlBar(out);
    m_FinalSeqalign->Set().sort(AlnScoreDescendingSort);
    s_RestoreHspPos(*m_FinalSeqalign);
    return m_FinalSeqalign;
}

void CVecscreen::VecscreenPrint(CNcbiOstream& out){
    x_BuildHtmlBar(out);
}

CVecscreen::AlnInfo* CVecscreen::x_GetAlnInfo(TSeqPos from, TSeqPos to, MatchType type, 
        const AlnInfo::TAlignList aligns){
    AlnInfo* aln_info = new AlnInfo;
    aln_info->range.Set(from, to);
    aln_info->type = type;
    aln_info->add_aligns(aligns);
    return aln_info;
}

void CVecscreen::x_BuildNonOverlappingRange(vector<CRef<CSeq_align_set> > 
                                            seqalign_vec){
    vector< list<AlnInfo*> > aln_info_vec(seqalign_vec.size());
    CRange<TSeqPos>* prev_range;
    
    //merge overlaps within the same type
    for(unsigned int i = 0; i < seqalign_vec.size(); i ++){
        int j = 0;
        ITERATE(CSeq_align_set::Tdata, iter, seqalign_vec[i]->Get()){ 
            AlnInfo* cur_aln_info = new AlnInfo;
            cur_aln_info->range.Set((*iter)->GetSeqRange(0).GetFrom(), 
                                    (*iter)->GetSeqRange(0).GetTo());
            cur_aln_info->type = (MatchType)i;
            cur_aln_info->add_align(*iter);
            //merge if previous range intersect with current range
            if(j > 0){ 
                prev_range = &(aln_info_vec[i].back()->range);
                if(prev_range->IntersectingWith(cur_aln_info->range) ||
                   prev_range->AbuttingWith(cur_aln_info->range)){
                    aln_info_vec[i].back()->range = 
                        aln_info_vec[i].back()->range.CombinationWith(cur_aln_info->range);
                    delete cur_aln_info;
                    aln_info_vec[i].back()->add_align(*iter);
                } else {
                    aln_info_vec[i].push_back(cur_aln_info);
                }
            } else {
                aln_info_vec[i].push_back(cur_aln_info); //store current range
            }
            j ++;
        }
    }
    
    //merge overlapping range of lower ranks to higher rank range    
    for(unsigned int i = 0; i < aln_info_vec.size(); i ++){
        ITERATE(list<AlnInfo*>, iter_higher, aln_info_vec[i]){
            for(unsigned int j = i + 1; j < aln_info_vec.size(); j ++){
                list<AlnInfo*>::iterator iter_temp;
                list<AlnInfo*>::iterator iter_lower = aln_info_vec[j].begin();
                while(iter_lower != aln_info_vec[j].end()){
                    CRange<TSeqPos> higher_range, lower_range;
                    higher_range = (*iter_higher)->range;
                    lower_range = (*iter_lower)->range;
                    if((*iter_higher)->range.IntersectingWith((*iter_lower)->range)){
                        //overlaps.  Need to handle
                        if((*iter_higher)->range.GetFrom() <=
                           (*iter_lower)->range.GetFrom()){
                            //higher from comes first
                            if((*iter_higher)->range.GetTo() >=
                               (*iter_lower)->range.GetTo()){
                                //higher include lower. delete lower.
                                (*iter_higher)->add_aligns((*iter_lower)->get_aligns());
                                iter_temp = iter_lower;
                                iter_lower ++;
                                aln_info_vec[j].erase(iter_temp);
                            } else {
                                //partially overlaps
                                //reduce the first part of the lower one
                                (*iter_lower)->range.
                                    Set((*iter_higher)->range.GetTo() + 1,
                                        (*iter_lower)->range.GetTo());
                                iter_lower ++;  
                            }
                        } else {
                            //lower from comes first
                            if((*iter_higher)->range.GetTo() <=
                               (*iter_lower)->range.GetTo()){
                                //lower includes higher. need to break up lower
                                //to 3 parts and delete the middle one(included
                                //in higher one)
                              
                                aln_info_vec[j].
                                    insert(iter_lower, 
                                           x_GetAlnInfo((*iter_lower)->range.
                                                        GetFrom(),
                                                        (*iter_higher)->range.
                                                        GetFrom() - 1 ,
                                                        (MatchType)j,
                                                        (*iter_lower)->get_aligns() ));

                                if ((*iter_higher)->range.GetTo() <
                                    (*iter_lower)->range.GetTo()) {
                                    //insert another piece only if lower has extra piece
                                    aln_info_vec[j].
                                        insert(iter_lower, 
                                               x_GetAlnInfo((*iter_higher)->range.
                                                            GetTo() + 1,
                                                            (*iter_lower)->range.GetTo() ,
                                                            (MatchType)j,
                                                            (*iter_lower)->get_aligns() ));
                                }
                                
                                iter_temp = iter_lower;
                                iter_lower ++;
                                aln_info_vec[j].erase(iter_temp);

                            } else {
                                //partially overlap
                                //reduce latter part of lower
                                
                                (*iter_lower)->range.
                                    Set((*iter_lower)->range.GetFrom(),
                                        (*iter_higher)->range.GetFrom() - 1);
                                iter_lower ++;  
                            }
                        }
                    }  else {
                        //no overlaps, do nothing
                        if ((*iter_lower)->range.GetFrom() > (*iter_higher)->range.GetFrom()) {
                            //no comparing again as it's already sorted
                            break; 
                        }
                        iter_lower ++; 
                    }
                }
            }
        }
    }
    
    //Set final list
    for(unsigned int i = 0; i < aln_info_vec.size(); i++){
        ITERATE(list<AlnInfo*>, iter, aln_info_vec[i]){
            m_AlnInfoList.push_back(*iter);
        }
        
    }
    m_AlnInfoList.sort(FromRangeAscendingSort);

  
    //adding range for suspected match and no match
    list<AlnInfo*>::iterator prev_iter = m_AlnInfoList.end();
    list<AlnInfo*>::iterator cur_iter = m_AlnInfoList.begin();
    list<AlnInfo*>::iterator temp_iter;
    int count = 0;
    
    while(cur_iter != m_AlnInfoList.end()){
        if(count > 0){
            CRange<TSeqPos> prev_range, cur_range;
            prev_range = (*prev_iter)->range;
            cur_range = (*cur_iter)->range;
            int diff =  cur_range.GetFrom() - prev_range.GetTo();
            if(diff >= 2){
                //no overlaps, insert the range in between
                
                MatchType type = ((*cur_iter)->range.GetFrom() - 1) -
                    ((*prev_iter)->range.GetTo() + 1) + 1 > kSupectLength ?
                    eNoMatch : eSuspect;
                
                m_AlnInfoList.
                    insert(cur_iter,
                           x_GetAlnInfo(prev_range.GetTo() + 1, 
                                        cur_range.GetFrom() - 1,
                                        type));
            }
            
        } else {
            if((*cur_iter)->range.GetFrom() > 0){
                //insert the range infront of first align range
                MatchType type = ((*cur_iter)->range.GetFrom() - 1) + 1 > 
                    kSupectLength ?
                    eNoMatch : eSuspect;
                m_AlnInfoList.
                    insert(cur_iter,
                           x_GetAlnInfo(0, (*cur_iter)->range.GetFrom() - 1,
                                        type)); 
            }     
        }
        prev_iter = cur_iter;
        cur_iter ++;
        count ++;
    }
    
    //add the last possible no match range
    if(prev_iter != m_AlnInfoList.end()){
        if(m_MasterLen -1 > (*prev_iter)->range.GetTo()){
            MatchType type = m_MasterLen - 
                ((*prev_iter)->range.GetTo() + 1) +1 > 
                kSupectLength ?
                eNoMatch : eSuspect;
            m_AlnInfoList.
                push_back(x_GetAlnInfo((*prev_iter)->range.GetTo() + 1, 
                                       m_MasterLen - 1,
                                       type));
        }
    }    
}

END_SCOPE(align_format)
END_NCBI_SCOPE
