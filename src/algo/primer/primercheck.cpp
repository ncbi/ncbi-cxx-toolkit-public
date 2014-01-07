 
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
 */

/** @file primercheck.hpp
 *  primer specificity checking tool
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistre.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objects/seqalign/Seq_align_set.hpp>



#include <objtools/alnmgr/alnvec.hpp>
#include <algo/align/nw/nw_aligner.hpp>
#include <algo/align/nw/nw_formatter.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <algo/primer/primercheck.hpp>


BEGIN_NCBI_SCOPE
USING_NCBI_SCOPE;
USING_SCOPE(objects);

USING_SCOPE (sequence);

static const double k_MinOverlapLenFactor = 0.45;
static const double k_Min_Percent_Identity = 0.64999;
static const int k_MaxReliableGapNum = 3;

COligoSpecificityTemplate::COligoSpecificityTemplate(const CBioseq_Handle& template_handle,
                                               const CSeq_align_set& input_seqalign,
                                               CScope &scope,
                                               int word_size,
                                               TSeqPos allowed_total_mismatch,
                                               TSeqPos allowed_3end_mismatch,
                                               TSeqPos max_mismatch,
                                               bool allow_transcript_variants)
    : m_TemplateHandle(template_handle),
      m_Id(template_handle.GetSeqId()),
      m_WordSize(word_size),
      m_AllowedTotalMismatch(allowed_total_mismatch),
      m_Allowed3EndMismatch(allowed_3end_mismatch),
      m_MaxMismatch(max_mismatch),
      m_AllowTranscriptVariants(allow_transcript_variants),
      m_UseITree(false),
      m_MismatchRegionLength3End(10),
      m_MaxHSPSize(0),
      m_NumNonSpecificTarget(20)
{
    x_SortHit(input_seqalign);
    if(!input_seqalign.Get().empty()){
        m_TemplateRange.Set(0,
                            scope.GetBioseqHandle(input_seqalign.Get().front()
                                                  ->GetSeq_id(0)).GetBioseqLength());
    }
    const CRef<CSeq_id> wid
        = FindBestChoice(template_handle.GetBioseqCore()->GetId(),
                         CSeq_id::WorstRank);
    m_TemplateType = wid->IdentifyAccession();
}

COligoSpecificityTemplate::~COligoSpecificityTemplate()
{
    for (TSeqPos i = 0; i < m_SortHit.size(); i ++) {
        for (TSeqPos j = 0; j < m_SortHit[i].first.size(); j ++) {
            delete m_SortHit[i].first[j];
        }
        for (TSeqPos j = 0; j < m_SortHit[i].second.size(); j ++) {
            delete m_SortHit[i].second[j];
        }
    }

    for (int i = 0; i < (int)m_RangeTreeListPlusStrand.size(); i ++) {
        delete m_RangeTreeListPlusStrand[i];
    };
    
    for (int i = 0; i < (int)m_RangeTreeListMinusStrand.size(); i ++) {
        delete m_RangeTreeListMinusStrand[i];
    };
    
}
   
COligoSpecificityCheck::COligoSpecificityCheck(const COligoSpecificityTemplate* hits,
                                               CScope & scope)
    : m_Hits(hits),
      m_Scope(&scope),
      m_FeatureScope(NULL)
{
    m_SlaveRangeCache.resize(m_Hits->m_SortHit.size());
    if (m_Hits->m_MaxHSPSize > 0) {
        m_HspOverlappingWithLeftPrimer = new SHspIndexInfo[m_Hits->m_MaxHSPSize];
        m_HspOverlappingWithRightPrimer = new SHspIndexInfo[m_Hits->m_MaxHSPSize];
        m_HspOverlappingWithLeftPrimerMinusStrand = new SHspIndexInfo[m_Hits->m_MaxHSPSize];
        m_HspOverlappingWithRightPrimerMinusStrand = new SHspIndexInfo[m_Hits->m_MaxHSPSize];
    }
}

COligoSpecificityCheck::~COligoSpecificityCheck()
{
    if (m_Hits->m_MaxHSPSize > 0) {
        delete [] m_HspOverlappingWithLeftPrimer;
        delete []  m_HspOverlappingWithRightPrimer;
        delete []  m_HspOverlappingWithLeftPrimerMinusStrand;
        delete []  m_HspOverlappingWithRightPrimerMinusStrand;
    };
}

static void s_CountGaps(const string& xcript, 
                        TSeqPos& master_start_gap,
                        TSeqPos& master_end_gap,
                        TSeqPos& slave_start_gap,
                        TSeqPos& slave_end_gap,
                        char master_gap_char,
                        char slave_gap_char) {
    
     for(int i = 0; i < (int)xcript.size(); i ++){
        if (xcript[i] == master_gap_char) {
            master_start_gap ++;
        } else {
            break;
        }
     }

     for(int i = xcript.size() - 1; i >= 0; i--){
        if (xcript[i] == master_gap_char) {
            master_end_gap ++;
        } else {
            break;
        }
     }
     
     if (master_start_gap == 0) {//can only have gap on either master or slave, not both
         for(int i = 0; i < (int)xcript.size(); i ++){
             if (xcript[i] == slave_gap_char) {
                 slave_start_gap ++;
             } else {
                 break;
             }
         }
     }
     if (master_end_gap == 0){
         for(int i = xcript.size() - 1; i >= 0; i--){
             if (xcript[i] == slave_gap_char) {
                 slave_end_gap ++;
             } else {
                 break;
             }
         }
     }
}

void COligoSpecificityCheck::x_FindMatchInfoForAlignment(CDense_seg& primer_denseg,
                                                         bool& end_gap,
                                                         TSeqPos& num_total_mismatch, 
                                                         TSeqPos& num_3end_mismatch, 
                                                         TSeqPos& num_total_gap,
                                                         TSeqPos& num_3end_gap,
                                                         bool is_left_primer,
                                                         int& max_num_continuous_match,
                                                         CRange<TSignedSeqPos>& aln_range) {
    

    int num_continuous_match = 0;
    num_total_mismatch = 0;
    num_3end_mismatch = 0;
    num_total_gap = 0;
    num_3end_gap = 0;
    max_num_continuous_match = 0;

    CRef<CAlnVec> av(new CAlnVec(primer_denseg, *m_Scope));
    //need to calculate the mismatch and gaps.
    aln_range.SetFrom (0);
    aln_range.SetTo(av->GetAlnStop());
    av->SetGapChar('-');
    av->SetEndChar('-');
    string master_string;
    av->GetAlnSeqString(master_string, 0, aln_range);
    string slave_string;
    av->GetAlnSeqString(slave_string, 1, aln_range);
    char gap_char = av->GetGapChar(0);
    int num_bp = 0;
    int master_letter_len = 0;
    for(int i=0; i< (int)master_string.size(); i++){
        if (master_string[i] != gap_char) {
            master_letter_len ++;
        }
    }
    for(int i=0; i < (int)master_string.size(); i++){
        
        if (master_string[i] == gap_char) {
            if (is_left_primer) {
                if (num_bp > (int)(master_letter_len - m_Hits->m_MismatchRegionLength3End)){
                    num_3end_gap ++;
                }
            } else {
                if (num_bp < (int)(m_Hits->m_MismatchRegionLength3End - 1)){
                    num_3end_gap ++;
                } 
            }
            num_total_gap ++;
        } else if (slave_string[i] == gap_char) {
            if (is_left_primer) {
                if (num_bp > (int)(master_letter_len - m_Hits->m_MismatchRegionLength3End)){
                    num_3end_gap ++;
                }
            } else {
                if (num_bp < (int)(m_Hits->m_MismatchRegionLength3End - 1)){
                    num_3end_gap ++; 
                }
            }
            num_bp ++;
            num_total_gap ++;
        } else if(master_string[i]!=slave_string[i]){
            if (is_left_primer) {
                if (num_bp >= (int)(master_letter_len - m_Hits->m_MismatchRegionLength3End)){
                    num_3end_mismatch ++;
                }
            } else {
                if (num_bp < (int)m_Hits->m_MismatchRegionLength3End){
                    num_3end_mismatch ++;
                }
            }
            num_total_mismatch ++;
            num_bp ++;
        } else {
            num_bp ++;
        }
        if(master_string[i]== slave_string[i]){
            num_continuous_match ++;
            if (max_num_continuous_match < num_continuous_match) {
                max_num_continuous_match = num_continuous_match;
            }
        } else {
            //reset
            num_continuous_match = 0;
        }
    }
    //      cerr<< "master = " << master_string << endl;
    //    cerr<< "slave = " << slave_string << endl;
    if (master_string[0] == gap_char ||
        master_string[(int)master_string.size() - 1] == gap_char || 
        slave_string[0] == gap_char ||
        slave_string[(int)slave_string.size() - 1] == gap_char || 
        (int)num_total_gap >= k_MaxReliableGapNum) {
        //blast local alignment ends with gaps may not be an accurate alignment
        end_gap = true;
    }
}

CRef<CDense_seg> s_DoNWalign (const CRange<TSeqPos>& desired_align_range,
                              string& master_seq, 
                              const CAlnVec& av,
                              TSeqPos hit_full_start, 
                              TSeqPos hit_full_stop,
                              ENa_strand hit_strand, 
                              string& xcript,
                              bool& nw_align_modified) {
    nw_align_modified = false;
    CRef<CDense_seg> den_ref (NULL);
    string hit_seq;
    const CBioseq_Handle& hit_handle = av.GetBioseqHandle(1);
    if (hit_strand == eNa_strand_minus) {
        hit_handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac,
                                eNa_strand_minus).
            GetSeqData((int)av.GetBioseqHandle(1).GetBioseqLength() -
                       hit_full_stop - 1, 
                       (int)av.GetBioseqHandle(1).GetBioseqLength() -
                       hit_full_start, hit_seq);
        //  cerr << "strand minus" << endl;
    } else {
        hit_handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac,
                                eNa_strand_plus).
            GetSeqData(hit_full_start, hit_full_stop + 1, hit_seq);
    }
    //cerr << "global master=" << master_seq << " hit=" << hit_seq << endl;
    
    CRef<CNWAligner> aligner (new CNWAligner(master_seq, hit_seq));
    aligner->SetWm(1);
    aligner->SetWms(-1);
    aligner->SetWg(-1);
    aligner->SetWs(-4);
    aligner->SetScoreMatrix(NULL);
    aligner->Run();
    xcript = aligner->GetTranscriptString();
    //cerr << "original script=" << xcript << endl;
    den_ref = aligner->GetDense_seg(desired_align_range.GetFrom(),
                                    eNa_strand_plus,
                                    av.GetSeqId(0),
                                    (hit_strand == 
                                     eNa_strand_minus ? hit_full_stop :
                                     hit_full_start), 
                                    hit_strand, 
                                    av.GetSeqId(1));
    
    /* auto_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, cerr)); 
    cerr << "original denseg:" << endl;
    *out << *den_ref; 
    cerr << endl;*/
    TSeqPos master_start_gap = 0;
    TSeqPos master_end_gap = 0;
    TSeqPos slave_start_gap = 0;
    TSeqPos slave_end_gap = 0;
    s_CountGaps(xcript, master_start_gap, master_end_gap, slave_start_gap, slave_end_gap, 'I', 'D');
    if (slave_start_gap > 0 || slave_end_gap > 0) {
        
        //extending slave row
        TSeqPos new_hit_full_start;
        TSeqPos new_hit_full_stop;
        if (av.IsPositiveStrand(1)) {
            new_hit_full_start = max((int)(hit_full_start - slave_start_gap), 0); 
            new_hit_full_stop = min(hit_full_stop + slave_end_gap, av.GetBioseqHandle(1).
                                    GetBioseqLength() - 1);
        } else {
            new_hit_full_start = max((int)(hit_full_start - slave_end_gap), 0); 
            new_hit_full_stop = min(hit_full_stop + slave_start_gap, av.GetBioseqHandle(1).
                                    GetBioseqLength() - 1);
        }
        
        /* cerr << "in hit_full_start =" << hit_full_start << endl;
        cerr << "in hit_full_stop =" << hit_full_stop << endl;
        cerr << "in new_hit_full_start =" << new_hit_full_start << endl;
        cerr << "in new_hit_full_stop =" << new_hit_full_stop << endl;*/
        //realign again only if hit seq can extend (i.e., not at seq end or start)
        if (!(new_hit_full_start == hit_full_start &&
              new_hit_full_stop == hit_full_stop)) {
            if (av.IsPositiveStrand(1)) {
                for(int i = hit_full_start - new_hit_full_start - 1; i >= 0 ; i --){
                    xcript[i] = 'M';
                }
                for(int i = (int)xcript.size() - 1 - (new_hit_full_stop - hit_full_stop); i < (int)xcript.size(); i ++) {
                    xcript[i] = 'M';
                }
            } else {
                for(int i = new_hit_full_stop - hit_full_stop - 1; i >= 0 ; i --){
                    xcript[i] = 'M';
                }
                for(int i = (int)xcript.size() - 1 - (hit_full_start - new_hit_full_start); i < (int)xcript.size(); i ++) {
                    xcript[i] = 'M';
                }

            }
            hit_full_start = new_hit_full_start;
            hit_full_stop = new_hit_full_stop;
            den_ref = new CDense_seg;
            den_ref->FromTranscript(desired_align_range.GetFrom(),
                                     eNa_strand_plus,
                                    (hit_strand == 
                                     eNa_strand_minus ? new_hit_full_stop :
                                     new_hit_full_start), 
                                    hit_strand,
                                    xcript);
            CRef<CSeq_id> master_id (new CSeq_id);
            CRef<CSeq_id> slave_id (new CSeq_id);
            master_id->Assign(av.GetSeqId(0));
            slave_id->Assign(av.GetSeqId(1));
            den_ref->SetIds().push_back(master_id);
            den_ref->SetIds().push_back(slave_id);
            nw_align_modified = true;
        }
     }

     
     if (master_start_gap > 0 || master_end_gap > 0) {
         //deleting the master gaps
         xcript = xcript.substr(master_start_gap);
         xcript = xcript.substr(0, xcript.size() - master_end_gap);
         
         TSeqPos new_hit_full_start;
         TSeqPos new_hit_full_stop;
         if (av.IsPositiveStrand(1)) {
             new_hit_full_start = hit_full_start + master_start_gap; 
             new_hit_full_stop = hit_full_stop - master_end_gap;
        } else {
             new_hit_full_start = hit_full_start + master_end_gap; 
             new_hit_full_stop = hit_full_stop - master_start_gap;
        }
         /*
        cerr << "in hit_full_start =" << hit_full_start << endl;
        cerr << "in hit_full_stop =" << hit_full_stop << endl;
        cerr << "in new_hit_full_start =" << new_hit_full_start << endl;
        cerr << "in new_hit_full_stop =" << new_hit_full_stop << endl;*/
         den_ref = new CDense_seg;
         den_ref->FromTranscript(desired_align_range.GetFrom(),
                                 eNa_strand_plus,
                                 (hit_strand == 
                                  eNa_strand_minus ? new_hit_full_stop :
                                  new_hit_full_start), 
                                 hit_strand,
                                 xcript);
         CRef<CSeq_id> master_id (new CSeq_id);
         CRef<CSeq_id> slave_id (new CSeq_id);
         master_id->Assign(av.GetSeqId(0));
         slave_id->Assign(av.GetSeqId(1));
         den_ref->SetIds().push_back(master_id);
         den_ref->SetIds().push_back(slave_id);
         nw_align_modified = true;
     }

     
     /* CNWFormatter fmt (*aligner);
      
    string text;
    fmt.AsText(&text, CNWFormatter::eFormatType2);
    cerr << text << endl;

    auto_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, cerr)); 
    cerr << "final denseg:" << endl;
    *out << *den_ref; 
    cerr << endl;*/
    return den_ref;
}

 
CRef<CDense_seg> COligoSpecificityCheck::x_NW_alignment(const CRange<TSeqPos>& desired_align_range,
                                                        const CSeq_align& input_hit,
                                                        TSeqPos& num_total_mismatch, 
                                                        TSeqPos& num_3end_mismatch, 
                                                        TSeqPos& num_total_gap,
                                                        TSeqPos& num_3end_gap,
                                                        bool is_left_primer,
                                                        int& max_num_continuous_match,
                                                        int& align_length,
                                                        TSeqPos master_local_start,
                                                        TSeqPos master_local_stop,
                                                        ENa_strand hit_strand,
                                                        bool& nw_align_modified) {
    /*   auto_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, cerr)); 
    cerr << endl << "input_hit:" << endl;       
    *out << input_hit;
    cerr << endl;*/

    string master_seq;
    m_Scope->GetBioseqHandle(input_hit.GetSeq_id(0)).GetSeqVector(CBioseq_Handle::eCoding_Iupac, eNa_strand_plus).
        GetSeqData(desired_align_range.GetFrom(),
                   desired_align_range.GetTo() + 1, master_seq);
    //   cerr << "desired_align_range.GetFrom=" << desired_align_range.GetFrom() << endl;
    //cerr << "desired_align_range.Getto=" << desired_align_range.GetTo() << endl;
    string hit_seq;
    //global hit start 
    TSeqPos hit_full_start; 
    TSeqPos hit_full_stop; 
    
    
    TSeqPos full_master_start = max(desired_align_range.GetFrom(), 
                                    master_local_start);
    
    TSeqPos full_master_stop = min(desired_align_range.GetTo(), master_local_stop);
    
    CRef<CAlnVec> av(new CAlnVec(input_hit.GetSegs().GetDenseg(), *m_Scope));
                    
    CRange<int> full_master_range(full_master_start, full_master_stop);
    
    
    CRef<CAlnMap::CAlnChunkVec> master_chunk = 
        av->GetSeqChunks(0, full_master_range,
                                    CAlnMap::fChunkSameAsSeg);
    
    CConstRef<CAlnMap::CAlnChunk> chunk_ref;
    int longest_chunk_index = 0;
    int longest_chunk_size = 0;
    //find the chunk that is the longest aligned region.  We will use that as
    //basis for finding the sujbect region for global alignment
    for (int chunk_index = 0; chunk_index < master_chunk->size(); chunk_index ++) {
        
        chunk_ref = (*master_chunk)[chunk_index];
        if (!chunk_ref->IsGap()) {
            if (chunk_ref->GetAlnRange().GetLength() > longest_chunk_size) {
                longest_chunk_size = chunk_ref->GetAlnRange().GetLength();
                longest_chunk_index = chunk_index;
                
            }
                    }
        /*  cerr << "longest size=" << longest_chunk_size
             << " longest index =" << longest_chunk_index 
             << " start = " << chunk_ref->GetAlnRange().GetFrom() 
             << " stop =" <<  chunk_ref->GetAlnRange().GetTo() << endl;*/
    }
    CRange<int> longest_chunk_range = (*master_chunk)[longest_chunk_index]->GetRange();
                
    int hit_start_adjust = longest_chunk_range.GetFrom() - 
        desired_align_range.GetFrom();
    int hit_stop_adjust = desired_align_range.GetTo() - 
        longest_chunk_range.GetTo();
    
    if(av->IsPositiveStrand(1)) {
        hit_full_start = 
            max((int)(av->GetSeqPosFromSeqPos(1, 0, longest_chunk_range.GetFrom(),
                                              CAlnMap::eBackwards, true) - 
                      hit_start_adjust), 0);
        
        hit_full_stop = 
            min(int(av->GetSeqPosFromSeqPos(1, 0, longest_chunk_range.GetTo(),
                                            CAlnMap::eBackwards, true) + 
                    hit_stop_adjust), (int)av->GetBioseqHandle(1).
                GetBioseqLength() - 1); 
    } else {
        hit_full_start = 
            max((int)(av->GetSeqPosFromSeqPos(1, 0, longest_chunk_range.GetTo(),
                                              CAlnMap::eBackwards, true) - 
                                  hit_stop_adjust), 0);
        
        hit_full_stop = 
            min(int(av->GetSeqPosFromSeqPos(1, 0, longest_chunk_range.GetFrom(),
                                            CAlnMap::eBackwards, true) + 
                    hit_start_adjust),
                (int)av->GetBioseqHandle(1).GetBioseqLength() - 1); 
        
    }
    /*cerr << "longest_chunk_range.GetFrom=" << longest_chunk_range.GetFrom() << endl;
    cerr << "longest_chunk_range.GetTo=" << longest_chunk_range.GetTo() << endl;
    cerr << "hit_start_adjust="<< hit_start_adjust << endl;
    cerr << "hit_stop_adjust="<< hit_stop_adjust << endl;
    cerr << "full_master_start =" << full_master_start << endl;
    cerr << "full_master_stop =" << full_master_stop << endl;
    cerr << "hit_full_start =" << hit_full_start << endl;
    cerr << "hit_full_stop =" << hit_full_stop << endl;
    */
    string xcript;
    nw_align_modified = false;
    CRef<objects::CDense_seg> den_ref = s_DoNWalign(desired_align_range,
                                                    master_seq, *av, hit_full_start,
                                                    hit_full_stop,
                                                    hit_strand, xcript, nw_align_modified);
   

    //sometimes master seq end aligns to an gap which should be deleted on master row
    //but should be extended on slave row
       
     TSeqPos match = 0;
     TSeqPos total_mismatch = 0;
     TSeqPos total_insertion = 0;
     TSeqPos total_deletion = 0;
     TSeqPos mismatch_3end = 0;
     TSeqPos insertion_3end = 0;
     TSeqPos deletion_3end = 0;
     TSeqPos num_master_gap = 0;
     int num_continuous_match = 0;
     max_num_continuous_match = 0;
     
     if (!nw_align_modified) {
       

         align_length = xcript.size();
         
         ITERATE(string, iter, xcript) {
             switch(*iter) {
             case 'I':
                 ++ num_master_gap;
                 break;
             default:
                 break;
             }
         }
         
         TSeqPos master_letter_len = xcript.size() - num_master_gap;
         int num_bp = 0;
         ITERATE(string, the_iter, xcript) {
             switch(*the_iter) {
                 
             case 'R':
                 if (is_left_primer) {
                     if (num_bp >= (int)(master_letter_len - m_Hits->m_MismatchRegionLength3End)){
                         mismatch_3end ++;
                     }
                 } else {
                     if (num_bp < (int)m_Hits->m_MismatchRegionLength3End){
                         mismatch_3end ++;
                     }
                 }
                 total_mismatch ++;
                 num_bp ++;
                 num_continuous_match = 0;
                 break;
                 
             case 'M': 
                 if (is_left_primer) {
                     if (num_bp >=(int)(master_letter_len - m_Hits->m_MismatchRegionLength3End)){
                         match ++;
                     }
                 } else {
                     if (num_bp < (int)m_Hits->m_MismatchRegionLength3End){
                         match ++;
                     }
                 }
                 num_bp ++;
                 num_continuous_match ++;
                 if (max_num_continuous_match < num_continuous_match) {
                     max_num_continuous_match = num_continuous_match;
                 }
                 break;
                 
             case 'I':
                 if (is_left_primer) {
                     if (num_bp > (int)(master_letter_len - m_Hits->m_MismatchRegionLength3End)){
                         insertion_3end ++; 
                     }
                 } else {
                     if (num_bp < (int)(m_Hits->m_MismatchRegionLength3End - 1)){
                         insertion_3end ++; 
                     }
                 }
                 total_insertion ++;
                 num_bp ++;
                 num_continuous_match = 0;
                 break;
                 
             case 'D':
                 if (is_left_primer) {
                     if (num_bp > (int)(master_letter_len - m_Hits->m_MismatchRegionLength3End)){
                         deletion_3end ++; 
                     }
                 } else {
                     if (num_bp < (int)(m_Hits->m_MismatchRegionLength3End - 1)){
                         deletion_3end ++; 
                     }
                 }
                 total_deletion ++;
                 num_continuous_match = 0;
                 break;
             }
         }
         
     }
     /*    cout << "length =" << xcript.size() << endl;
           cout << "mis = " << mismatch << endl;
           cout << "match = " << match << endl;
                       cout << "insert = " << insertion << endl;
                       cout << "delete = " << deletion << endl;*/
     num_total_mismatch = total_mismatch;
     num_total_gap = total_insertion + total_deletion;
     num_3end_mismatch = mismatch_3end;
     num_3end_gap = insertion_3end + deletion_3end;               
     
     return den_ref;
}


/*is_left_primer controls whether mismatch is at the 5' (pass false) or 3' end (pass true).   
pass true for left or false for right primer if both primers are on the same strand.
For self primers...for left primer window, pass true for both left and right primers.
For right primers, pass false for both 
*/
CRef<CSeq_align> COligoSpecificityCheck::
x_FillGlobalAlignInfo(const CRange<TSeqPos>& desired_align_range,
                      SHspInfo* input_hsp_info,
                      TSeqPos& num_total_mismatch, 
                      TSeqPos& num_3end_mismatch, 
                      TSeqPos& num_total_gap,
                      TSeqPos& num_3end_gap,
                      bool is_left_primer,
                      TSeqPos index,
                      ENa_strand hit_strand)
{
   
    num_total_mismatch = 0;
    num_3end_mismatch = 0;
    num_total_gap = 0;
    num_3end_gap = 0;
    int max_num_continuous_match = 0;
    CRef<CSeq_align> global_align(NULL);
    CConstRef<CSeq_align> input_hit = input_hsp_info->hsp;
    CRange<TSeqPos> master_range = input_hsp_info->master_range;
    CRange<TSeqPos> hit_range = input_hsp_info->slave_range;
   
    CRange<TSeqPos> primer_master_overlap = desired_align_range.
        IntersectionWith(master_range);
    //check if primer loc overlaps with hits
    if (primer_master_overlap.GetLength() >= 
        k_MinOverlapLenFactor*desired_align_range.GetLength()) {
        SAlnCache cache_id;

        cache_id.hit_id = index;
        cache_id.primer_start = desired_align_range.GetFrom();
        cache_id.primer_stop = desired_align_range.GetTo();
        cache_id.master_start= master_range.GetFrom();
        cache_id.hit_start = hit_range.GetFrom();
        if (hit_strand == eNa_strand_minus) {
            cache_id.is_positive_strand = false;
        } else {
            cache_id.is_positive_strand = true;
        }
        
        if(is_left_primer) {
            cache_id.is_left_primer = true;
        } else {
            cache_id.is_left_primer = false;
        }
        
        map <SAlnCache, SPrimerMatch, sort_order>::iterator ii = m_Cache.find(cache_id); 
        
        if(ii != m_Cache.end() ) {
            //already cached
            SPrimerMatch tmp = (*ii).second;
            global_align = tmp.aln; 
            num_total_mismatch = tmp.num_total_mismatch;
            num_3end_mismatch = tmp.num_3end_mismatch;
            num_total_gap = tmp.num_total_gap;
            num_3end_gap = tmp.num_3end_gap;
        } else {//new range and hit 
            TSeqPos master_local_start = input_hsp_info->master_range.GetFrom();
            TSeqPos master_local_stop = input_hsp_info->master_range.GetTo();
            bool do_global_alignment = true;
            if (desired_align_range.GetFrom() >= master_local_start && 
                desired_align_range.GetTo() <= master_local_stop) {
                do_global_alignment = false;
                
                CRef<CDense_seg> primer_denseg = 
                    input_hit->GetSegs().GetDenseg().ExtractSlice(0, desired_align_range.GetFrom(),
                                                                 desired_align_range.GetTo());
                CRange<TSignedSeqPos> aln_range;
                x_FindMatchInfoForAlignment(*primer_denseg, do_global_alignment, num_total_mismatch, 
                                            num_3end_mismatch, num_total_gap,num_3end_gap,
                                            is_left_primer,
                                            max_num_continuous_match, aln_range);
                
                if (!do_global_alignment) {
                    double percent_ident = 1 - ((double)(num_total_mismatch + num_total_gap))/aln_range.GetLength();
                    if (max_num_continuous_match >= m_Hits->m_WordSize && 
                        percent_ident > k_Min_Percent_Identity && 
                        num_total_mismatch + num_total_gap < m_Hits->m_MaxMismatch &&
                        (num_total_mismatch + num_total_gap <= m_Hits->m_AllowedTotalMismatch || 
                         num_3end_mismatch + num_3end_gap <= m_Hits->m_Allowed3EndMismatch)) {
                        CRef<CSeq_align> aln_ref(new CSeq_align());
                        aln_ref->SetType(CSeq_align::eType_partial);
                        
                        aln_ref->SetSegs().SetDenseg(*primer_denseg);
                        
                        global_align = aln_ref;
                    }
                } 
            } 
            //only extend if the hit alignment does not completely covers the primer window
            if (do_global_alignment) {
                int align_length = 1;
                bool nw_align_modified = false;
                num_total_mismatch = 0;
                num_3end_mismatch = 0;
                num_total_gap = 0;
                num_3end_gap = 0;
                max_num_continuous_match = 0;
                double percent_ident;
                CRef<CDense_seg> den_ref = x_NW_alignment(desired_align_range, *input_hit, 
                                                          num_total_mismatch, num_3end_mismatch, 
                                                          num_total_gap, num_3end_gap, 
                                                          is_left_primer, max_num_continuous_match,
                                                          align_length,
                                                          master_local_start,
                                                          master_local_stop, hit_strand,
                                                          nw_align_modified);
                if (nw_align_modified) {
                    align_length = 1;
                    num_total_mismatch = 0;
                    num_3end_mismatch = 0;
                    num_total_gap = 0;
                    num_3end_gap = 0;
                    max_num_continuous_match = 0;
                    CRange<TSignedSeqPos> aln_range;
                    x_FindMatchInfoForAlignment(*den_ref, do_global_alignment, 
                                                num_total_mismatch, 
                                                num_3end_mismatch, num_total_gap,num_3end_gap,
                                                is_left_primer,
                                                max_num_continuous_match, aln_range);
              
                    percent_ident = 1 - ((double)(num_total_mismatch + num_total_gap))/aln_range.GetLength();
                    
                } else {
                    percent_ident = 1 - ((double)(num_total_mismatch + num_total_gap))/align_length;
                }
                if (max_num_continuous_match >= m_Hits->m_WordSize &&
                    percent_ident > k_Min_Percent_Identity && 
                    num_total_mismatch + num_total_gap < m_Hits->m_MaxMismatch &&
                    (num_total_mismatch + num_total_gap <= m_Hits->m_AllowedTotalMismatch || 
                     num_3end_mismatch + num_3end_gap <= m_Hits->m_Allowed3EndMismatch)) {
                    
                    CRef<CSeq_align> aln_ref(new CSeq_align());
                    aln_ref->SetType(CSeq_align::eType_partial);
                        
                    aln_ref->SetSegs().SetDenseg(*den_ref);
                    
                    // auto_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText, cout));                                                       
                    //     *out << *aln_ref;
                        
                    global_align = aln_ref;
                }
            }
        }
        SPrimerMatch temp_match;
        temp_match.num_total_mismatch = num_total_mismatch;
        temp_match.num_3end_mismatch = num_3end_mismatch;
        temp_match.num_total_gap = num_total_gap;
        temp_match.num_3end_gap = num_3end_gap;
        temp_match.aln = global_align;
        
        m_Cache[cache_id] = temp_match;
        /*  m_Cache.insert(map<string, SPrimerMatch >::
            value_type(cache_id, temp_match));
        */
        //m_AlignCache[cache_id.c_str()] = temp_match;    
        
    }
    
    return global_align;
}

//determine if template and the hits map to the same chromosome location.  Note this only
//does the check if template or hits are chromosome
bool COligoSpecificityCheck::x_SequencesMappedToSameTarget(CSeq_id::EAccessionInfo hit_type,
                                                           const CSeq_align& left_align,
                                                           const CSeq_align& right_align) {
    bool same_target = false;
    if(!m_FeatureScope) {
        m_FeatureOM = CObjectManager::GetInstance();
        CGBDataLoader::RegisterInObjectManager(*m_FeatureOM);
        m_FeatureScope = new CScope(*m_FeatureOM); 
        string name = CGBDataLoader::GetLoaderNameFromArgs();
        m_FeatureScope->AddDataLoader(name);
        cerr << "mapping triggered" << endl;
    }
    
    //the backbone such as chromosome
    CRef<CSeq_loc> backbone_loc (0);
    CRef<CSeq_loc> component_loc (0);
    //try backbone and component on template or hit as we don't know which is which
    //at least hit or template needs to be chr or contig
    if ((m_Hits->m_TemplateType & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_chromosome) {
        //template as backbone
        backbone_loc = new CSeq_loc((CSeq_loc::TId &) *(m_Hits->m_Id),
                                    (CSeq_loc::TPoint) m_Hits->m_TemplateRange.GetFrom(),
                                    (CSeq_loc::TPoint) m_Hits->m_TemplateRange.GetTo()); 
        component_loc = new CSeq_loc((CSeq_loc::TId &) left_align.GetSeq_id(1),
                                     (CSeq_loc::TPoint) min(left_align.GetSeqRange(1).GetFrom(), 
                                                            right_align.GetSeqRange(1).GetFrom()),
                                     (CSeq_loc::TPoint) max(left_align.GetSeqRange(1).GetTo(), 
                                                            right_align.GetSeqRange(1).GetTo())); 
    } else if ((hit_type & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_chromosome) {
        //hit as backbone
        backbone_loc =  new CSeq_loc((CSeq_loc::TId &)left_align.GetSeq_id(1),
                                     (CSeq_loc::TPoint) min(left_align.GetSeqRange(1).GetFrom(), 
                                                            right_align.GetSeqRange(1).GetFrom()),
                                     (CSeq_loc::TPoint) max(left_align.GetSeqRange(1).GetTo(), 
                                                            right_align.GetSeqRange(1).GetTo())); 
        component_loc = new CSeq_loc((CSeq_loc::TId &) *(m_Hits->m_Id),
                                     (CSeq_loc::TPoint) m_Hits->m_TemplateRange.GetFrom(),
                                     (CSeq_loc::TPoint) m_Hits->m_TemplateRange.GetTo()); 
        

    } else if ((hit_type & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_con) {
        //hit as backbone
        backbone_loc =  new CSeq_loc((CSeq_loc::TId &)left_align.GetSeq_id(1),
                                     (CSeq_loc::TPoint) min(left_align.GetSeqRange(1).GetFrom(), 
                                                            right_align.GetSeqRange(1).GetFrom()),
                                     (CSeq_loc::TPoint) max(left_align.GetSeqRange(1).GetTo(), 
                                                            right_align.GetSeqRange(1).GetTo())); 
        component_loc = new CSeq_loc((CSeq_loc::TId &) *(m_Hits->m_Id),
                                     (CSeq_loc::TPoint) m_Hits->m_TemplateRange.GetFrom(),
                                     (CSeq_loc::TPoint) m_Hits->m_TemplateRange.GetTo()); 
        
        
    } else if ((m_Hits->m_TemplateType & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_con) {
        //template as backbone
        backbone_loc = new CSeq_loc((CSeq_loc::TId &) *(m_Hits->m_Id),
                                    (CSeq_loc::TPoint) m_Hits->m_TemplateRange.GetFrom(),
                                    (CSeq_loc::TPoint) m_Hits->m_TemplateRange.GetTo()); 
        component_loc = new CSeq_loc((CSeq_loc::TId &) left_align.GetSeq_id(1),
                                     (CSeq_loc::TPoint) min(left_align.GetSeqRange(1).GetFrom(), 
                                                            right_align.GetSeqRange(1).GetFrom()),
                                     (CSeq_loc::TPoint) max(left_align.GetSeqRange(1).GetTo(), 
                                                            right_align.GetSeqRange(1).GetTo())); 
    }
    
    if (backbone_loc && component_loc) {
        CSeq_id_Handle backbone_idh = sequence::GetIdHandle(*backbone_loc, m_FeatureScope);
        CBioseq_Handle backbone_handle = m_FeatureScope->GetBioseqHandle(backbone_idh);
        CSeq_loc_Mapper mapper(1, backbone_handle, CSeq_loc_Mapper::eSeqMap_Down);
        mapper.KeepNonmappingRanges(); 
        CRef<CSeq_loc> backbone_component = mapper.Map(*backbone_loc);
        
        if (backbone_component) {
            sequence::ECompare compare_result = 
                sequence::Compare(*backbone_component, *component_loc, m_FeatureScope);
            if ( compare_result == sequence::eContains ||
                 compare_result ==  sequence::eContained) {
                
                same_target = true;
            }
        }
    }
   
    return same_target;
}

static bool ContainsId(const CBioseq_Handle& bh, const CSeq_id_Handle& id) {

    return (find(bh.GetId().begin(), bh.GetId().end(), id) != bh.GetId().end());
}

void COligoSpecificityCheck::x_SavePrimerInfo(CSeq_align& left_align,
                                              CSeq_align& right_align,
                                              TSeqPos left_total_mismatch,
                                              TSeqPos left_3end_mismatch,
                                              TSeqPos left_total_gap,
                                              TSeqPos left_3end_gap,
                                              TSeqPos right_total_mismatch,
                                              TSeqPos right_3end_mismatch,
                                              TSeqPos right_total_gap,
                                              TSeqPos right_3end_gap,
                                              int product_len,
                                              TSeqPos index,
                                              bool is_self_forward_primer,
                                              bool is_self_reverse_primer) 
{
   
    SPrimerHitInfo info;
    CConstRef<CSeq_align> left;
    CConstRef<CSeq_align> right;
   
    info.product_len = product_len;
    info.left_total_mismatch = left_total_mismatch;
    info.left_total_gap = left_total_gap;
    info.left_3end_mismatch = left_3end_mismatch;
    info.left_3end_gap = left_3end_gap;

    info.right_total_mismatch = right_total_mismatch;
    info.right_total_gap = right_total_gap;
    info.right_3end_mismatch = right_3end_mismatch;
    info.right_3end_gap = right_3end_gap;
    info.aln.first = &left_align;
    info.aln.second = &right_align;
    info.index = index;
    info.self_forward_primer = is_self_forward_primer;
    info.self_reverse_primer = is_self_reverse_primer;
    TGi master_gi = ZERO_GI;
    TGi subj_gi = ZERO_GI;
    
    if (left_align.GetSeq_id(0).Which() == CSeq_id::e_Gi) {
        master_gi = left_align.GetSeq_id(0).GetGi();
    } else {
        master_gi = 
            FindGi(m_Scope->GetBioseqHandle(left_align.GetSeq_id(0)).
                   GetBioseqCore()->GetId());
        
    }
       
       
    if (left_align.GetSeq_id(1).Which() == CSeq_id::e_Gi) {
        subj_gi = left_align.GetSeq_id(1).GetGi();
    } else {
        subj_gi = 
            FindGi(m_Scope->GetBioseqHandle(left_align.GetSeq_id(1)).
                   GetBioseqCore()->GetId());
    }
    //    cout << "master_gi = " << master_gi  << endl;
    //   cout << "subj_gi = " << subj_gi  << endl;
    
    bool left_template_aln_overlap = m_Hits->m_TemplateRange.IntersectingWith(left_align.GetSeqRange(1));
    bool right_template_aln_overlap = m_Hits->m_TemplateRange.IntersectingWith(right_align.GetSeqRange(1));
    bool template_hit_same_id = ContainsId(m_Hits->m_TemplateHandle,
                                           CSeq_id_Handle::
                                           GetHandle(left_align.GetSeq_id(1)));

    const CRef<CSeq_id> hit_wid
        = FindBestChoice(m_Scope->GetBioseqHandle(left_align.GetSeq_id(1)).
                   GetBioseqCore()->GetId(),
                         CSeq_id::WorstRank);
    CSeq_id::EAccessionInfo hit_type = hit_wid->IdentifyAccession();
    //self hits
    if (template_hit_same_id && left_template_aln_overlap && right_template_aln_overlap) {
        
        m_SelfHit[m_CurrentPrimerIndex].push_back(info);
    } else if (m_Hits->m_Id->Which() != CSeq_id::e_Local && 
               ((m_Hits->m_TemplateType & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_chromosome ||
                (m_Hits->m_TemplateType & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_htgs ||
                (m_Hits->m_TemplateType & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_con || 
                (m_Hits->m_TemplateType & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_wgs ||
                m_Hits->m_TemplateType == CSeq_id::eAcc_refseq_genomic) &&
               ((hit_type & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_chromosome ||
                (hit_type & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_htgs ||
                (hit_type & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_con || 
                (hit_type & CSeq_id::eAcc_division_mask) == CSeq_id::eAcc_wgs) &&
               hit_type != m_Hits->m_TemplateType &&
               !template_hit_same_id &&
               x_SequencesMappedToSameTarget(hit_type, left_align, right_align)) {
        //try mapping the template
        m_SelfHit[m_CurrentPrimerIndex].push_back(info);
        cerr << "self hit by mapping" << endl;
    } else {
        bool hit_assigned = false;
        //allowed hits
        ITERATE(vector<TSeqPos>, iter, m_Hits->m_AllowedSeqidIndex) {
            if (index == *iter) {
                m_AllowedHit[m_CurrentPrimerIndex].push_back(info);
                hit_assigned  = true;
                break;
            }
        }
        
        //transcript variants
        
        if (m_Hits->m_AllowTranscriptVariants && !hit_assigned && master_gi != ZERO_GI && subj_gi != ZERO_GI) {
            IGeneInfoInput::TGeneIdList master_gene_id_list;
            IGeneInfoInput::TGeneIdList subj_gene_id_list;
            if (m_FileReader.GetGeneIdsForGi(GI_TO(int, master_gi), master_gene_id_list) 
                && m_FileReader.GetGeneIdsForGi(GI_TO(int, subj_gi), subj_gene_id_list)){
                
                ITERATE(IGeneInfoInput::TGeneIdList, iter1, master_gene_id_list) {
                    ITERATE(IGeneInfoInput::TGeneIdList, iter2, subj_gene_id_list) {
                        if (*iter1 == *iter2){
                            m_VariantHit[m_CurrentPrimerIndex].push_back(info);
                            hit_assigned = true;
                            break;
                        }
                    }
                    if (hit_assigned) {
                        break;
                    }
                }
            } 
        }
        //non specific hit
        if (!hit_assigned) {
             
            m_PrimerHit[m_CurrentPrimerIndex].push_back(info);
        } 
    }
    
}

static bool 
SortHspByMasterStartAscending(const SHspInfo* info1,
                              const SHspInfo* info2) 
{
    int start1 = 0, start2  = 0;
   
    start1 = info1->master_range.GetFrom();
    start2 = info2->master_range.GetFrom();
   
    return start1 <= start2;  
     
}



static bool 
SortIndexListByScoreDescending(const COligoSpecificityCheck::SHspIndexInfo& info1,
                               const COligoSpecificityCheck::SHspIndexInfo& info2) 
{
   
    return info1.bit_score > info2.bit_score;  
     
}


void COligoSpecificityCheck::
x_FindOverlappingHSP(SHspIndexInfo* left_window_index_list,
                     int& left_window_index_list_size,
                     SHspIndexInfo* right_window_index_list,
                     int& right_window_index_list_size,
                     const CRange<TSeqPos>& left_window_desired_range,
                     const CRange<TSeqPos>& right_window_desired_range,
                     ENa_strand hit_strand,
                     TSeqPos hit_index,
                     const vector<SHspInfo*>& hsp_list) {
    left_window_index_list_size = 0;
    right_window_index_list_size = 0;
    if (m_Hits->m_UseITree) {

        CRange<int> left_window_desired_range_int;
        
        left_window_desired_range_int.SetFrom(left_window_desired_range.GetFrom());
        left_window_desired_range_int.SetTo(left_window_desired_range.GetTo());
        CRange<int> right_window_desired_range_int;
        
        right_window_desired_range_int.SetFrom(right_window_desired_range.GetFrom());
        right_window_desired_range_int.SetTo(right_window_desired_range.GetTo());
        CIntervalTree::const_iterator left_window_tree_it;
        CIntervalTree::const_iterator right_window_tree_it;
        if (hit_strand == eNa_strand_minus) {
            left_window_tree_it = m_Hits->m_RangeTreeListMinusStrand[hit_index]->IntervalsOverlapping(left_window_desired_range_int);
            right_window_tree_it = m_Hits->m_RangeTreeListMinusStrand[hit_index]->IntervalsOverlapping(right_window_desired_range_int);
            
        } else {
            left_window_tree_it = m_Hits->m_RangeTreeListPlusStrand[hit_index]->IntervalsOverlapping(left_window_desired_range_int); 
            right_window_tree_it = m_Hits->m_RangeTreeListPlusStrand[hit_index]->IntervalsOverlapping(right_window_desired_range_int); 
        }
        
        for (; left_window_tree_it; ++ left_window_tree_it) {
            CConstRef<SHspIndex>  temp (static_cast<const SHspIndex*> (&*left_window_tree_it.GetValue()));
            if (hsp_list[temp->index]->master_range.IntersectionWith(left_window_desired_range).GetLength() >= 
                k_MinOverlapLenFactor*left_window_desired_range.GetLength()) {
                left_window_index_list[left_window_index_list_size].index = temp->index;
                left_window_index_list[left_window_index_list_size].bit_score = hsp_list[temp->index]->bit_score;
                left_window_index_list_size ++;
            }
        }
        for (; right_window_tree_it; ++ right_window_tree_it) {
            CConstRef<SHspIndex>  temp (static_cast<const SHspIndex*> (&*right_window_tree_it.GetValue()));
            if (hsp_list[temp->index]->master_range.IntersectionWith(right_window_desired_range).GetLength() >= 
                k_MinOverlapLenFactor*right_window_desired_range.GetLength()) {
                right_window_index_list[right_window_index_list_size].index = temp->index;
                right_window_index_list[right_window_index_list_size].bit_score = hsp_list[temp->index]->bit_score;
                right_window_index_list_size ++;
            }
        }
      
    } else {

        for (int i = 0; i <(int) hsp_list.size(); i ++ ) {
            //quit if master range is beyond the desired range already as 
            //hsp is already sorted according to master start.
            if (hsp_list[i]->master_range.GetFrom() >= right_window_desired_range.GetTo()) {
                break;
            } 
            if (hsp_list[i]->master_range.IntersectionWith(left_window_desired_range).
                GetLength() >= k_MinOverlapLenFactor*left_window_desired_range.GetLength()) {
                left_window_index_list[left_window_index_list_size].index = i;
                left_window_index_list[left_window_index_list_size].bit_score = hsp_list[i]->bit_score;
                left_window_index_list_size ++;
            }
            
            if (hsp_list[i]->master_range.IntersectionWith(right_window_desired_range).
                GetLength() >= k_MinOverlapLenFactor*right_window_desired_range.GetLength()) {
                right_window_index_list[right_window_index_list_size].index = i;
                right_window_index_list[right_window_index_list_size].bit_score = hsp_list[i]->bit_score;
                right_window_index_list_size ++;
            }
        }
    }

    //sort the index within the list such that hsp with higher score comes first
    //to avoid potential loss of more significant matches
   
    if (left_window_index_list_size > 0) {
        stable_sort(left_window_index_list, left_window_index_list + left_window_index_list_size, SortIndexListByScoreDescending);
    }
    
    if (right_window_index_list_size > 0) {
        stable_sort(right_window_index_list, right_window_index_list + right_window_index_list_size, SortIndexListByScoreDescending);
    }
        
}

void COligoSpecificityCheck::x_AnalyzeTwoPrimers(const TSortedHsp& sorted_hsp,
                                                 TSeqPos hit_index) 
{
    //our primer input uses locations on the same strand notation so a valid pcr product
    //can only have primers on the same strand
    //  bool hsp_on_minus_strand = (hit_strand == eNa_strand_minus ? true : false);
  
    int HspOverlappingWithLeftPrimer_size;
    int HspOverlappingWithRightPrimer_size;
    int HspOverlappingWithLeftPrimerMinusStrand_size;
    int HspOverlappingWithRightPrimerMinusStrand_size;

  
    x_FindOverlappingHSP(m_HspOverlappingWithLeftPrimer,
                         HspOverlappingWithLeftPrimer_size,
                         m_HspOverlappingWithRightPrimer,
                         HspOverlappingWithRightPrimer_size,
                         m_PrimerInfo->left,
                         m_PrimerInfo->right,
                         eNa_strand_plus, hit_index, sorted_hsp.first); 


    x_FindOverlappingHSP(m_HspOverlappingWithLeftPrimerMinusStrand,
                         HspOverlappingWithLeftPrimerMinusStrand_size,
                         m_HspOverlappingWithRightPrimerMinusStrand, 
                         HspOverlappingWithRightPrimerMinusStrand_size,
                         m_PrimerInfo->left,
                         m_PrimerInfo->right,
                         eNa_strand_minus, hit_index,
                         sorted_hsp.second); 


    bool analyze_plus_strand_first = true;
    //analyze the strand that have better hits first or we may miss more significant primer matches
    //because we limit the total number of non-specific matches
    
    //-1 because comparision with double problem
    
    if (sorted_hsp.first.size() > 0) {
        if (sorted_hsp.second.size() > 0 &&
            sorted_hsp.first[0]->bit_score < sorted_hsp.second[0]->bit_score - 1) {
            analyze_plus_strand_first = false;
        }
    } else {
        analyze_plus_strand_first = false;
    }
      

    if (analyze_plus_strand_first) {
        //analyze plus strand
        x_AnalyzeLeftAndRightPrimer(sorted_hsp.first,
                                    eNa_strand_plus,
                                    HspOverlappingWithLeftPrimer_size,
                                    HspOverlappingWithRightPrimer_size,
                                    hit_index);
        
        //analyze minus strand
        x_AnalyzeLeftAndRightPrimer(sorted_hsp.second,
                                    eNa_strand_minus,
                                    HspOverlappingWithLeftPrimerMinusStrand_size,
                                    HspOverlappingWithRightPrimerMinusStrand_size,
                                    hit_index);
        
    } else {
        
        //analyze minus strand
        x_AnalyzeLeftAndRightPrimer(sorted_hsp.second,
                                    eNa_strand_minus,
                                    HspOverlappingWithLeftPrimerMinusStrand_size,
                                    HspOverlappingWithRightPrimerMinusStrand_size,
                                    hit_index);
        //analyze plus strand
        x_AnalyzeLeftAndRightPrimer(sorted_hsp.first,
                                    eNa_strand_plus,
                                    HspOverlappingWithLeftPrimer_size,
                                    HspOverlappingWithRightPrimer_size,
                                    hit_index);
    }

    //only need to check the plus strand case as one primer cases are palindrom 
    x_AnalyzeOnePrimer(sorted_hsp.first, sorted_hsp.second,
                       HspOverlappingWithLeftPrimer_size,
                       HspOverlappingWithRightPrimer_size,
                       HspOverlappingWithLeftPrimerMinusStrand_size,
                       HspOverlappingWithRightPrimerMinusStrand_size,
                       hit_index); 
  
}


CRange<TSeqPos> COligoSpecificityCheck::
x_GetSlaveRangeGivenMasterRange(const CSeq_align& input_align,
                                CRange<TSeqPos>& master_range,
                                int index) {
    
    CRange<TSeqPos> slave_range = CRange<TSeqPos>::GetEmpty();
    SSlaveRange cache_id;
    cache_id.align_index = (uintptr_t) &input_align;
    cache_id.master_start = master_range.GetFrom();
    cache_id.master_stop = master_range.GetTo();
    map <SSlaveRange, CRange<TSeqPos>, slave_range_sort_order>::iterator ij = m_SlaveRangeCache[index].find(cache_id); 
        
    if(ij != m_SlaveRangeCache[index].end() ){
        // cache hit
        slave_range = (*ij).second;  
        //cerr << "cached range" << endl;
    } else {
        // cerr << "no cached range" << endl;
        CRef<CDense_seg> denseg (NULL);
        try {
            denseg = input_align.GetSegs().GetDenseg().
                ExtractSlice(0, master_range.GetFrom(), master_range.GetTo());
            slave_range = denseg->GetSeqRange(1);
           
        } catch (CSeqalignException& e) {
            //    if (e.GetErrCode() == CSeqalignException::eInvalidAlignment) {
            cerr << "ExtractSlice error = " << e.what() << endl;
            
            //  }
        }
        m_SlaveRangeCache[index][cache_id] = slave_range;
    }

    return slave_range;
}

//check if the left primer has a valid right primer on hits based on the distance and orietation  
void COligoSpecificityCheck::
x_AnalyzeLeftAndRightPrimer(const vector<SHspInfo*>& hsp_list, 
                            ENa_strand hit_strand,
                            int HspOverlappingWithLeftPrimer_size,
                            int HspOverlappingWithRightPrimer_size,
                            TSeqPos hit_index)
{
    //save right primer slave range to avoid repeated map lookup which is slow
    
    vector<CRange<TSeqPos> > right_slave_range_array(HspOverlappingWithRightPrimer_size);
    for (int j = 0; HspOverlappingWithLeftPrimer_size > 0 && j < HspOverlappingWithRightPrimer_size; j ++) {
        int right_hsp_index = m_HspOverlappingWithRightPrimer[j].index;
        if (hit_strand == eNa_strand_minus) {
            right_hsp_index = m_HspOverlappingWithRightPrimerMinusStrand[j].index;  
        }
        CRange<TSeqPos> right_primer_master_overlap = 
            hsp_list[right_hsp_index]->master_range.IntersectionWith(m_PrimerInfo->right);
        
        right_slave_range_array[j] = 
            x_GetSlaveRangeGivenMasterRange(*(hsp_list[right_hsp_index]->hsp),
                                            right_primer_master_overlap,
                                            hit_index);
    }
    
    int left_primer_hsp_index = 0;
    for (int i = 0; i < HspOverlappingWithLeftPrimer_size; i ++) {
        //each left window
        left_primer_hsp_index = m_HspOverlappingWithLeftPrimer[i].index;
        if (hit_strand == eNa_strand_minus) {
            left_primer_hsp_index = m_HspOverlappingWithLeftPrimerMinusStrand[i].index; 
        }
        if ((int)(m_PrimerHit[m_CurrentPrimerIndex].size()) >= m_Hits->m_NumNonSpecificTarget){
            break;
        }
        CRange<TSeqPos> left_primer_master_overlap = 
            hsp_list[left_primer_hsp_index]->master_range.IntersectionWith(m_PrimerInfo->left);
   
        //check right primer
        bool left_slave_range_filled = false;
        bool left_global_align_filled = false;
        CRange<TSeqPos> left_primer_hit_range;
        CRef<CSeq_align> left_primer_hit_global_align(NULL);
           
        TSeqPos left_total_mismatch = 0;
        TSeqPos left_total_gap = 0;
        TSeqPos left_3end_mismatch = 0;
        TSeqPos left_3end_gap = 0;
        //check the current left window against all right windows
        for (int j = 0; j < HspOverlappingWithRightPrimer_size; j ++) {
        
            int right_hsp_index = m_HspOverlappingWithRightPrimer[j].index;
            if (hit_strand == eNa_strand_minus) {
                right_hsp_index = m_HspOverlappingWithRightPrimerMinusStrand[j].index;  
            }
            
            if (!left_slave_range_filled) { // only check left primer once
                left_primer_hit_range = 
                    x_GetSlaveRangeGivenMasterRange(*hsp_list[left_primer_hsp_index]->hsp,
                                                    left_primer_master_overlap,
                                                    hit_index);
                left_slave_range_filled = true;
                if (left_primer_hit_range.Empty()) {
                    break;
                }
            }
            
            TSeqPos left_primer_hit_stop = left_primer_hit_range.GetTo();
            TSeqPos left_primer_hit_start = left_primer_hit_range.GetFrom();
            
            
            CRange<TSeqPos> right_primer_hit_range = right_slave_range_array[j];
            if (right_primer_hit_range.Empty()) {
                continue;
            }
            TSeqPos right_primer_hit_stop = right_primer_hit_range.GetTo();
            TSeqPos right_primer_hit_start = right_primer_hit_range.GetFrom();
            
            int product_len;
            if (hit_strand == eNa_strand_minus) {
                
                product_len = 
                    (left_primer_hit_start - right_primer_hit_stop + 1) + 
                    (m_PrimerInfo->right.GetLength() -1) + 
                    (m_PrimerInfo->left.GetLength() -1); 
            } else {
                product_len = 
                    (right_primer_hit_start - left_primer_hit_stop + 1) +
                    (m_PrimerInfo->right.GetLength() -1) + 
                    (m_PrimerInfo->left.GetLength() -1); 
            }
           
            if (product_len > 0 && 
                product_len <= 
                (int)(m_SpecifiedProductLen + m_Hits->m_ProductLenMargin) && 
                (int)product_len >= 
                (int)m_SpecifiedProductLen - (int)(m_Hits->m_ProductLenMargin)){
             
                if (!left_global_align_filled) {
                    
                    left_primer_hit_global_align 
                        = x_FillGlobalAlignInfo(m_PrimerInfo->left, 
                                                hsp_list[left_primer_hsp_index], 
                                                left_total_mismatch, 
                                                left_3end_mismatch, 
                                                left_total_gap,
                                                left_3end_gap,
                                                true, hit_index, hit_strand);
                    left_global_align_filled = true;
                    if (!left_primer_hit_global_align) {
                        break;
                    }
                }
                
                TSeqPos right_total_mismatch = 0;
                TSeqPos right_total_gap = 0;
                TSeqPos right_3end_mismatch = 0;
                TSeqPos right_3end_gap = 0;
                CRef<CSeq_align> right_primer_hit_global_align = 
                    x_FillGlobalAlignInfo(m_PrimerInfo->right,
                                          hsp_list[right_hsp_index], 
                                          right_total_mismatch, right_3end_mismatch, 
                                          right_total_gap, right_3end_gap, false, 
                                          hit_index, hit_strand);
                if (right_primer_hit_global_align) {
                    
                    int pcr_product_len = 0;
                    if (x_IsPcrLengthInRange(*left_primer_hit_global_align,
                                             *right_primer_hit_global_align,
                                             false,
                                             hit_strand,
                                             pcr_product_len)){
                        
                        x_SavePrimerInfo(*left_primer_hit_global_align, 
                                         *right_primer_hit_global_align,
                                         left_total_mismatch,
                                         left_3end_mismatch, left_total_gap, left_3end_gap,
                                         right_total_mismatch, right_3end_mismatch, 
                                         right_total_gap,
                                         right_3end_gap, pcr_product_len,
                                         hit_index, false, false);                
                    }
                }
                
            }
        }        
    }
} 


bool COligoSpecificityCheck::
x_IsPcrLengthInRange(const CSeq_align& left_primer_hit_align, 
                     const CSeq_align& right_primer_hit_align,
                     bool primers_on_different_strand,
                     ENa_strand hit_strand,
                     int& product_len)
{
    bool result = false;
    TSeqPos left_primer_hit_stop = left_primer_hit_align.GetSeqStop(1);
    TSeqPos left_primer_hit_start = left_primer_hit_align.GetSeqStart(1);
    TSeqPos right_primer_hit_start = right_primer_hit_align.GetSeqStart(1);  
    TSeqPos right_primer_hit_stop = right_primer_hit_align.GetSeqStop(1); 
    product_len = 0;

    //self primer case
    if (primers_on_different_strand) {
        
        product_len = right_primer_hit_stop  - left_primer_hit_start + 1; 
        
    } else if (hit_strand == eNa_strand_minus) {

        product_len = (left_primer_hit_stop - right_primer_hit_start + 1);
    } else {
        product_len = (right_primer_hit_start - left_primer_hit_stop + 1) +
            (m_PrimerInfo->right.GetLength() -1) + 
            (m_PrimerInfo->left.GetLength() -1); 
    }
                
    if (product_len >= min((int)left_primer_hit_align.GetSeqRange(0).GetLength(),
                           (int)right_primer_hit_align.GetSeqRange(0).GetLength()) && 
        product_len <= (int)(m_SpecifiedProductLen + m_Hits->m_ProductLenMargin) && 
        (int)product_len >= (int)m_SpecifiedProductLen - (int)(m_Hits->m_ProductLenMargin)) { 
        result = true;
    }


    return result;
}


void COligoSpecificityCheck::
x_AnalyzeOnePrimer(const vector<SHspInfo*>& plus_strand_hsp_list,
                   const vector<SHspInfo*>& minus_strand_hsp_list,
                   int HspOverlappingWithLeftPrimer_size,
                   int HspOverlappingWithRightPrimer_size,
                   int HspOverlappingWithLeftPrimerMinusStrand_size,
                   int HspOverlappingWithRightPrimerMinusStrand_size,
                   TSeqPos hit_index)  {
    //save right slave range to avoid repeated tree query
    vector<CRange<TSeqPos> >  right_primer_hit_range_array(HspOverlappingWithLeftPrimerMinusStrand_size);
    for (int j = 0; HspOverlappingWithLeftPrimer_size > 0 && j < HspOverlappingWithLeftPrimerMinusStrand_size; j ++) {
        int right_hsp_index = m_HspOverlappingWithLeftPrimerMinusStrand[j].index;
        CRange<TSeqPos>  right_master_range = minus_strand_hsp_list[right_hsp_index]->master_range;
        
        CRange<TSeqPos> left_primer_window_right_align_overlap = 
            m_PrimerInfo->left.IntersectionWith(right_master_range);
            
        right_primer_hit_range_array[j] = 
            x_GetSlaveRangeGivenMasterRange(*(minus_strand_hsp_list[right_hsp_index]->hsp),
                                            left_primer_window_right_align_overlap,
                                            hit_index);
    }

    for (int i = 0; i < HspOverlappingWithLeftPrimer_size; i ++) {
       
        int left_hsp_index = m_HspOverlappingWithLeftPrimer[i].index;
        if ((int)(m_PrimerHit[m_CurrentPrimerIndex].size()) >= m_Hits->m_NumNonSpecificTarget){
            break;
        }
        
        CRange<TSeqPos> left_primer_master_overlap = 
            plus_strand_hsp_list[left_hsp_index]->master_range.IntersectionWith(m_PrimerInfo->left);
        
     
        CRange<TSeqPos> left_primer_hit_range;
        bool left_slave_range_filled = false;
        bool left_global_align_filled = false;
        TSeqPos left_total_mismatch = 0;
        TSeqPos left_3end_mismatch = 0; 
        TSeqPos left_total_gap = 0;
        TSeqPos left_3end_gap = 0;
        
        CRef<CSeq_align> left_primer_hit_global_align(NULL);
        for (int j = 0; j < HspOverlappingWithLeftPrimerMinusStrand_size; j ++) {
            int right_hsp_index = m_HspOverlappingWithLeftPrimerMinusStrand[j].index;
            
            if (!left_slave_range_filled) {
                left_primer_hit_range = 
                    x_GetSlaveRangeGivenMasterRange(*(plus_strand_hsp_list[left_hsp_index]->hsp),
                                                    left_primer_master_overlap,
                                                    hit_index);
                left_slave_range_filled = true;
                if (left_primer_hit_range.Empty()) {
                    break;
                }
            }
            
            TSeqPos left_primer_hit_stop = left_primer_hit_range.GetTo();
            
          
            CRange<TSeqPos> right_primer_hit_range = right_primer_hit_range_array[j];
            if (right_primer_hit_range.Empty()) {
                continue;
            }
            TSeqPos right_primer_hit_start = right_primer_hit_range.GetFrom();
            
            //now check the distance of two primer windows
            
            int product_len = right_primer_hit_start  - left_primer_hit_stop + 1 + 
                + (m_PrimerInfo->right.GetLength() -1) + 
                (m_PrimerInfo->left.GetLength() -1); 
            
            if (!(product_len > 0 && 
                  product_len <= 
                  (int)(m_SpecifiedProductLen + m_Hits->m_ProductLenMargin) && 
                  (int)product_len >= 
                  (int)m_SpecifiedProductLen - (int)(m_Hits->m_ProductLenMargin))) {
                continue;
            }
           
            TSeqPos right_total_mismatch = 0;
            TSeqPos right_total_gap = 0;
            TSeqPos right_3end_mismatch = 0;
            TSeqPos right_3end_gap = 0;
            
            if (!left_global_align_filled) {
                left_primer_hit_global_align = 
                    x_FillGlobalAlignInfo(m_PrimerInfo->left, 
                                          plus_strand_hsp_list[left_hsp_index], 
                                          left_total_mismatch,
                                          left_3end_mismatch, 
                                          left_total_gap, left_3end_gap,
                                          true,
                                          hit_index, eNa_strand_plus);
                left_global_align_filled = true;
                if(!left_primer_hit_global_align) {
                    break;
                }
            }
            CRef<CSeq_align> right_primer_hit_global_align =
                x_FillGlobalAlignInfo(m_PrimerInfo->left,
                                      minus_strand_hsp_list[right_hsp_index], 
                                      right_total_mismatch,
                                      right_3end_mismatch, 
                                      right_total_gap, right_3end_gap,
                                      true,
                                      hit_index, eNa_strand_minus);
            if (right_primer_hit_global_align) {
                
                //primer overlaps on hit should be within pcr product range
                int pcr_product_len = 0;
                bool valid_pcr_length;
                
                valid_pcr_length = 
                    x_IsPcrLengthInRange(*left_primer_hit_global_align,
                                         *right_primer_hit_global_align,
                                         true, 
                                         eNa_strand_minus,
                                         pcr_product_len);
                
                if (valid_pcr_length) {
                    CRef<CSeq_align> new_align_left(new CSeq_align);
                    CRef<CSeq_align> new_align_right(new CSeq_align);
                    
                    new_align_left = left_primer_hit_global_align;
                    new_align_right = right_primer_hit_global_align;
                    
                    
                    x_SavePrimerInfo(*new_align_left, *new_align_right,
                                     left_total_mismatch,
                                     left_3end_mismatch, left_total_gap, left_3end_gap,
                                     right_total_mismatch, right_3end_mismatch,
                                     right_total_gap,
                                     right_3end_gap, pcr_product_len, hit_index, true, false);  
                    
                }
            }
        }    
    }
    
    //check right primer
    
    // save right slave range once to avoid repeated slow tree query
    vector<CRange<TSeqPos> >  right_primer_hit_range_array2(HspOverlappingWithRightPrimerMinusStrand_size);
    for (int j = 0; HspOverlappingWithRightPrimer_size > 0 && j < HspOverlappingWithRightPrimerMinusStrand_size; j ++) {
        int right_hsp_index = m_HspOverlappingWithRightPrimerMinusStrand[j].index;
            
        CRange<TSeqPos>  right_master_range = minus_strand_hsp_list[right_hsp_index]->master_range;
            
        CRange<TSeqPos> right_primer_as_3_master_overlap = 
            m_PrimerInfo->right.IntersectionWith(right_master_range);
           
        right_primer_hit_range_array2[j] = 
            x_GetSlaveRangeGivenMasterRange(*(minus_strand_hsp_list[right_hsp_index]->hsp),
                                            right_primer_as_3_master_overlap,
                                            hit_index);
    }

    for (int i = 0; i < HspOverlappingWithRightPrimer_size; i ++) {
        int left_hsp_index = m_HspOverlappingWithRightPrimer[i].index;
        if ((int)(m_PrimerHit[m_CurrentPrimerIndex].size()) >= m_Hits->m_NumNonSpecificTarget){
            break;
        }
       
        CRange<TSeqPos> right_primer_as_5_master_overlap = 
            plus_strand_hsp_list[left_hsp_index]->master_range.IntersectionWith(m_PrimerInfo->right);
        //so right primer has alignment in plus strand alignment
        //This means the right primer has to be on
        //minus strand of this alignment to work as any pcr can only go 5'->3'
        //so this is actually left primer
        
     
        CRange<TSeqPos> right_primer_as_5_hit_range;  
        bool right_primer_as_5_slave_range_filled = false;
        bool left_global_align_filled = false;
        
        CRef<CSeq_align> left_primer_hit_global_align(NULL);
        TSeqPos left_total_mismatch = 0;
        TSeqPos left_3end_mismatch = 0; 
        TSeqPos left_total_gap = 0;
        TSeqPos left_3end_gap = 0;
            
        //test if right window also have alignment on plus strand of any minus strand 
        //alignment.  
        
        for (int j = 0; j < HspOverlappingWithRightPrimerMinusStrand_size; j ++) {
            int right_hsp_index = m_HspOverlappingWithRightPrimerMinusStrand[j].index;
           
            if (!right_primer_as_5_slave_range_filled ) {
                right_primer_as_5_hit_range = 
                    x_GetSlaveRangeGivenMasterRange(*plus_strand_hsp_list[left_hsp_index]->hsp,
                                                    right_primer_as_5_master_overlap,
                                                    hit_index);
                right_primer_as_5_slave_range_filled = true;
                if (right_primer_as_5_hit_range.Empty()) {
                    break;
                }
                
            }
            
            
            TSeqPos right_primer_as_5_hit_start = right_primer_as_5_hit_range.GetFrom();
            
            CRange<TSeqPos> right_primer_as_3_hit_range = right_primer_hit_range_array2[j];
            
            if (right_primer_as_3_hit_range.Empty()) {
                continue;
            }
            TSeqPos right_primer_as_3_hit_stop =  right_primer_as_3_hit_range.GetTo();
            
            
            //now check the distance of two primer windows
            
            int product_len = right_primer_as_5_hit_start
                - right_primer_as_3_hit_stop + 1 + 
                + (m_PrimerInfo->right.GetLength() -1) + 
                (m_PrimerInfo->left.GetLength() -1); 
            
            if (!(product_len > 0 && 
                  product_len <= 
                  (int)(m_SpecifiedProductLen + m_Hits->m_ProductLenMargin) && 
                  (int)product_len >= 
                  (int)m_SpecifiedProductLen - (int)(m_Hits->m_ProductLenMargin))) {
                continue;
            }
          
            TSeqPos right_total_mismatch = 0;
            TSeqPos right_total_gap = 0;
            TSeqPos right_3end_mismatch = 0;
            TSeqPos right_3end_gap = 0;
            
            if (!left_global_align_filled) {
                left_primer_hit_global_align = 
                    x_FillGlobalAlignInfo(m_PrimerInfo->right, 
                                          plus_strand_hsp_list[left_hsp_index], 
                                          left_total_mismatch,
                                          left_3end_mismatch, 
                                          left_total_gap, left_3end_gap,
                                          false, hit_index, eNa_strand_plus);
                left_global_align_filled = true;
                if(!left_primer_hit_global_align) {
                    break;
                }
            }
            CRef<CSeq_align> right_primer_hit_global_align =
                x_FillGlobalAlignInfo(m_PrimerInfo->right, minus_strand_hsp_list[right_hsp_index], 
                                      right_total_mismatch,
                                      right_3end_mismatch, 
                                      right_total_gap, right_3end_gap,
                                      false,
                                      hit_index, eNa_strand_minus);
            if (right_primer_hit_global_align) {
                
                //primer overlaps on hit should be within pcr product range
                int pcr_product_len = 0;
                bool valid_pcr_length;
                
                valid_pcr_length = 
                    x_IsPcrLengthInRange(*right_primer_hit_global_align,
                                         *left_primer_hit_global_align,
                                         true,
                                         eNa_strand_minus,
                                         pcr_product_len);
                        
                if (valid_pcr_length) {
                    CRef<CSeq_align> new_align_left(new CSeq_align);
                    CRef<CSeq_align> new_align_right(new CSeq_align);
                    
                    new_align_left->Assign(*left_primer_hit_global_align);
                    //we deal with plus strand on master all the time
                    new_align_left->Reverse();
                            
                    new_align_right->Assign(*right_primer_hit_global_align);
                    //we deal with plus strand on master all the time
                    new_align_right->Reverse();
                    
                    
                    x_SavePrimerInfo(*new_align_left, *new_align_right,
                                     left_total_mismatch,
                                     left_3end_mismatch, left_total_gap, left_3end_gap,
                                     right_total_mismatch, right_3end_mismatch,
                                     right_total_gap,
                                     right_3end_gap, pcr_product_len, hit_index, false, true);  
                    
                }
            }
            
        }    
    }
}

    


void COligoSpecificityCheck::x_AnalyzePrimerSpecificity()
{   
       
    for (TSeqPos i = 0; i < m_Hits->m_SortHit.size(); i ++) {
     
        for (int j = 0; j < (int)m_PrimerInfoList.size(); j ++) {
           
            m_CurrentPrimerIndex = j;
            
            m_PrimerInfo = m_PrimerInfoList[j];
            m_SpecifiedProductLen = m_PrimerInfo->right.GetTo() - m_PrimerInfo->left.GetFrom() + 1;
            x_AnalyzeTwoPrimers((m_Hits->m_SortHit[i]), i);
            
        }
    }
}

static bool SortPrimerHitInGroupByMismatchAscending(const COligoSpecificityCheck::
                                                    SPrimerHitInfo* info1,
                                                    const COligoSpecificityCheck::
                                                    SPrimerHitInfo* info2) {
    int mismatch1 = 20;
    int mismatch2 = 20;
    
    mismatch1 = min(mismatch1,
                    (int)info1->right_total_mismatch + 
                    (int)info1->right_total_gap +
                    (int)info1->left_total_mismatch + 
                    (int)info1->left_total_gap);
   
    mismatch2 = min(mismatch2,
                    (int)info2->right_total_mismatch + 
                    (int)info2->right_total_gap +
                    (int)info2->left_total_mismatch + 
                    (int)info2->left_total_gap);
   
    return mismatch1 < mismatch2;
}

static bool SortPrimerHitByMismatchAscending(const vector<COligoSpecificityCheck::
                                             SPrimerHitInfo*>* info1,
                                             const vector<COligoSpecificityCheck::
                                             SPrimerHitInfo*>* info2) {
    int mismatch1 = 20;
    int mismatch2 = 20;
    ITERATE(vector<COligoSpecificityCheck::SPrimerHitInfo*>, iter, *info1) {
      
        mismatch1 = min(mismatch1,
                        (int)(*iter)->right_total_mismatch + 
                        (int)(*iter)->right_total_gap +
                        (int)(*iter)->left_total_mismatch + 
                        (int)(*iter)->left_total_gap);
    }


    ITERATE(vector<COligoSpecificityCheck::SPrimerHitInfo*>, iter, *info2) {

        mismatch2 = min(mismatch2,
                        (int)(*iter)->right_total_mismatch + 
                        (int)(*iter)->right_total_gap +
                        (int)(*iter)->left_total_mismatch + 
                        (int)(*iter)->left_total_gap);
    }
    return mismatch1 < mismatch2;
}

void COligoSpecificityCheck::x_SortPrimerHit(vector<vector<SPrimerHitInfo> >& primer_hit_list_list){
    for (int i = 0; i < (int) primer_hit_list_list.size(); i ++){
        vector<COligoSpecificityCheck::SPrimerHitInfo>* primer_hit_list = &primer_hit_list_list[i];
    //group hit with the same subject sequence together
    vector<vector<SPrimerHitInfo*>* > result;
    CConstRef<CSeq_id> previous_id;
    vector<SPrimerHitInfo*>* temp;

    NON_CONST_ITERATE(vector<COligoSpecificityCheck::SPrimerHitInfo>, iter, *primer_hit_list) {
        const CSeq_id& cur_id = iter->aln.first->GetSeq_id(1);
       
        if(previous_id.Empty()) {
            temp = new vector<SPrimerHitInfo*>; 
            temp->push_back(&(*iter));
            result.push_back(temp);
        } else if (cur_id.Match(*previous_id)){
            temp->push_back(&(*iter));
            
        } else {
             temp = new vector<SPrimerHitInfo*>; 
             temp->push_back(&(*iter));
             result.push_back(temp);
        }
        previous_id = &cur_id;
    }
    //sort the group based on lowest mismatches in a group
    stable_sort(result.begin(), result.end(), SortPrimerHitByMismatchAscending);
    
    //restore the hit structure to a plain list
    vector<SPrimerHitInfo> temp2;
    NON_CONST_ITERATE(vector<vector<SPrimerHitInfo*>* >, iter, result) {
        //sort within the group based on lowest mismatches

        stable_sort((**iter).begin(), (**iter).end(), SortPrimerHitInGroupByMismatchAscending);
        ITERATE(vector<SPrimerHitInfo*>, iter2, **iter){
            temp2.push_back(**iter2);
        }
        (**iter).clear();
        delete *iter;
    }
    primer_hit_list->clear();
    *primer_hit_list = temp2;
    }
}

void COligoSpecificityCheck::CheckSpecificity(const vector<SPrimerInfo>& primer_info_list, 
                                              int from, int to)
{
    int end = primer_info_list.size(); 
    if (from >= end) return;
    if (to > end || to < 0) to = end;
    for (int i=from; i<to; ++i) {
        m_PrimerInfoList.push_back(&(primer_info_list[i]));
        vector<SPrimerHitInfo> temp;
        m_PrimerHit.push_back(temp);
        m_SelfHit.push_back(temp);
        m_VariantHit.push_back(temp);
        m_AllowedHit.push_back(temp);
    }
   
    x_AnalyzePrimerSpecificity();
    x_SortPrimerHit(m_PrimerHit);
}



///Place alignment from the same id into one holder.  Split the alignment in each holder
/// into plus or minus strand and sort them by alignment start in ascending order
void COligoSpecificityTemplate::x_SortHit(const CSeq_align_set& input_hits)
{
    CConstRef<CSeq_id> previous_id, subid;
    bool is_first_aln = true;
    TSortedHsp each_hit; //first element for plus strand, second element for minus strand
   
    ITERATE(CSeq_align_set::Tdata, iter, input_hits.Get()) {
        subid = &((*iter)->GetSeq_id(1));
        if (!is_first_aln && !subid->Match(*previous_id)) {
            //this aln is a new id, save  for the same seqid
           
            m_SortHit.push_back(each_hit);
           
            //reset 
           
            each_hit.first.clear();
            each_hit.second.clear();
           
        }
        SHspInfo* temp = new SHspInfo;
        if ((*iter)->GetSeqStrand(0) == eNa_strand_minus) {
            CRef<CSeq_align> new_align(new CSeq_align);
            new_align->Assign(**iter);
            //we deal with plus strand on master all the time
            new_align->Reverse();
          
            temp ->hsp = new_align;
            each_hit.second.push_back(temp);
        } else {
            
            temp ->hsp = *iter;
            each_hit.first.push_back(temp);
        }
        temp->master_range = temp->hsp->GetSeqRange(0);
        temp->slave_range = temp->hsp->GetSeqRange(1);
        temp->bit_score = 0;
        temp ->hsp->GetNamedScore(CSeq_align::eScore_BitScore, temp->bit_score);

        is_first_aln = false;
        previous_id = subid;
       
    }

    //save the last ones with the same id
    if(!(each_hit.first.empty() && each_hit.second.empty())) {
       
        m_SortHit.push_back(each_hit);
        
    }
    int num_hits = m_SortHit.size();
    int num_hsp = input_hits.Get().size();
    int hsp_hit_ratio = 0;
  
    if (num_hits > 0) {
        hsp_hit_ratio = num_hsp/num_hits;
        
    }
    cerr << "hit = " << num_hits << " hsp = " << num_hsp
         << " hsp/hit ratio = " << hsp_hit_ratio << endl;
    
   
    if (hsp_hit_ratio > 100) {//use itree for many hsp case
       
        m_UseITree = true;
        for (int i = 0; i < (int)m_SortHit.size(); i ++) {
             
            CIntervalTree* RangeTreeForEachHitPlusStrand = new CIntervalTree;
            if ((int)m_SortHit[i].first.size() > m_MaxHSPSize) {
                m_MaxHSPSize = (int)m_SortHit[i].first.size();
            }
            for (int j = 0; j < (int)m_SortHit[i].first.size(); j ++) {
                CRef<SHspIndex> index_holder(new SHspIndex);
                index_holder->index = j;
                CRange<int> temp_master_range(m_SortHit[i].first[j]->master_range.GetFrom(),
                                              m_SortHit[i].first[j]->master_range.GetTo());
                RangeTreeForEachHitPlusStrand->Insert(temp_master_range, 
                                                      static_cast<CConstRef<CObject> > (index_holder));
                
            }
            m_RangeTreeListPlusStrand.push_back(RangeTreeForEachHitPlusStrand);
            
            CIntervalTree* RangeTreeForEachHitMinusStrand = new CIntervalTree;
            
            if ((int)m_SortHit[i].second.size() > m_MaxHSPSize) {
                m_MaxHSPSize = (int)m_SortHit[i].second.size();
            }
            for (int j = 0; j < (int)m_SortHit[i].second.size(); j ++) {
                CRef<SHspIndex> index_holder(new SHspIndex);
                index_holder->index = j;
                CRange<int> temp_master_range(m_SortHit[i].second[j]->master_range.GetFrom(),
                                              m_SortHit[i].second[j]->master_range.GetTo());
                RangeTreeForEachHitMinusStrand->Insert(temp_master_range, 
                                                       static_cast<CConstRef<CObject> > (index_holder));
                
            }
            m_RangeTreeListMinusStrand.push_back(RangeTreeForEachHitMinusStrand);
        }
    } else {
        //sort hsp to speed up comparison later
        for (int i = 0; i < (int)m_SortHit.size(); i ++) {
            stable_sort(m_SortHit[i].first.begin(), m_SortHit[i].first.end(),
                        SortHspByMasterStartAscending);
            stable_sort(m_SortHit[i].second.begin(), m_SortHit[i].second.end(), 
                        SortHspByMasterStartAscending);
            if ((int)m_SortHit[i].first.size() > m_MaxHSPSize) {
                m_MaxHSPSize = (int)m_SortHit[i].first.size();
            }
            
            if ((int)m_SortHit[i].second.size() > m_MaxHSPSize) {
                m_MaxHSPSize = (int)m_SortHit[i].second.size();
            }
            
        }
    }
}



END_NCBI_SCOPE
