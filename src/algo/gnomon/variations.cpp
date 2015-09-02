#include <ncbi_pch.hpp>
#include <algo/gnomon/variations.hpp>
#include <algo/gnomon/id_handler.hpp>
#include <algo/gnomon/glb_align.hpp>

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <sstream>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

TLiteInDels GroupInDels(const TLiteInDels& sam_indels) {
    TLiteInDels indels;
    indels.reserve(sam_indels.size());
    int loc = -1;
    int insertlen = 0;
    string deletion;
    ITERATE(TLiteInDels, indl, sam_indels) {
        if(loc < 0) {
            loc = indl->Loc();
            if(indl->GetInDelV().empty())
                insertlen = indl->Len();
            else
                deletion = indl->GetInDelV();
        } else if(loc+insertlen == indl->Loc()) {
            if(indl->GetInDelV().empty())
                insertlen += indl->Len();
            else
                deletion += indl->GetInDelV();
        } else {
            if(insertlen > 0)
                indels.push_back(CLiteIndel(loc, insertlen));
            if(!deletion.empty())
                indels.push_back(CLiteIndel(loc+insertlen,deletion.size(),deletion));

            loc = indl->Loc();
            if(indl->GetInDelV().empty()) {
                insertlen = indl->Len();
                deletion.clear();
            } else {
                deletion = indl->GetInDelV();
                insertlen = 0;
            }
        }
    }
    if(loc >= 0) {
        if(insertlen > 0)
            indels.push_back(CLiteIndel(loc, insertlen));
        if(!deletion.empty())
            indels.push_back(CLiteIndel(loc+insertlen,deletion.size(),deletion));
    }

    return indels;
}

CLiteAlign::CLiteAlign(TSignedSeqRange range, const TLiteInDels& indels, set<CLiteIndel>& indel_holder, double weight, double ident) :
         m_weight(weight), m_ident(ident), m_range(range) {

    m_indels.reserve(indels.size());
    ITERATE( TLiteInDels, indl, indels) {
        const CLiteIndel* indelp = &(*indel_holder.insert(*indl).first);
        m_indels.push_back(indelp);
    }        
}

CLiteAlign::CLiteAlign(const SSamData& ad, const string& contig, set<CLiteIndel>& indel_holder) : m_weight(ad.m_weight) {
    string cigar = ad.m_cigar;
    size_t first_element = cigar.find_first_not_of("0123456789");
    if(first_element != string::npos && cigar[first_element] == 'I')
        cigar[first_element] = 'S';
    if(cigar[cigar.size()-1] == 'I')
        cigar[cigar.size()-1] = 'S';
 
    TLiteInDels sam_indels;
    int matches = 0; 
    int align_len = 0;
    int seq_pos = 0;   //position on seq    
    int gstart = ad.m_contigp-1; //initial position on contig 
    int gstop = gstart;          //current position on contig

    istringstream istr_cigar(cigar);
    int len;
    char c;
    const string& seq = ad.m_seq;
    while(istr_cigar >> len >> c) {
        switch(c) {
        case 'S':
            seq_pos =+ len; 
        case 'H':
            break;

        case 'M':
            for(int l = 0; l < len; ++l) {
                if(seq[seq_pos] != contig[gstop]) { // mismatch
                    sam_indels.push_back(CLiteIndel(gstop,1));
                    sam_indels.push_back(CLiteIndel(gstop+1,1,seq.substr(seq_pos,1)));                        
                } else {
                    ++matches;
                }
                ++seq_pos;
                ++gstop;
                ++align_len;
            }
            break;
        case '=':
            matches += len; 
            seq_pos += len; align_len += len; gstop += len;
            break;

        case 'X':
        case 'R':
            sam_indels.push_back(CLiteIndel(gstop,len));
            sam_indels.push_back(CLiteIndel(gstop+len,len,seq.substr(seq_pos,len)));
            seq_pos += len; align_len += len; gstop += len;
            break;

        case 'I':
            sam_indels.push_back(CLiteIndel(gstop,len,seq.substr(seq_pos,len)));
            seq_pos += len; align_len += len;
            break;

        case 'D':
            sam_indels.push_back(CLiteIndel(gstop,len));
            align_len += len; gstop += len; 
            break;

        case 'N':
            throw runtime_error("Alignments can't have introns");

        default:
            break;
        }
    }

    m_range = TSignedSeqRange(gstart,gstop-1);
    m_ident = (double)matches/align_len;

    TLiteInDels indels = GroupInDels(sam_indels);

    m_indels.reserve(indels.size());
    ITERATE(TLiteInDels, indl, indels) {
        const CLiteIndel* indelp = &(*indel_holder.insert(*indl).first);
        m_indels.push_back(indelp);
    }
}

string CLiteAlign::TranscriptSeq(const string& contig) const {
    int l = m_range.GetFrom();
    int r = m_range.GetTo()+1;
    if(!m_indels.empty())
        r = m_indels.front()->Loc();
    string seq = contig.substr(l,r-l);
    for(int i = 0; i < (int)m_indels.size(); ++i) {
        if(m_indels[i]->IsInsertion()) {
            l = r+m_indels[i]->Len();
        } else {
            seq += m_indels[i]->GetInDelV();
            l = r;
        }
        r = m_range.GetTo()+1;
        if(i < (int)m_indels.size()-1)
            r = m_indels[i+1]->Loc();
        seq += contig.substr(l,r-l);
    }

    return seq;
}

void CMultAlign::SetGenomic(const CConstRef<CSeq_id>& seqid, CScope& scope) {
    m_max_len = 0;
    m_reads.clear();
    m_starts.clear();
    m_alignsp.clear();
    m_contig_to_align.clear();
    m_align_to_contig.clear();
    m_counts.clear();
    m_aligns.clear();
    m_indel_holder.clear();
    
    if(m_contig_id != seqid->GetSeqIdString(true)) {
        m_contig_id = seqid->GetSeqIdString(true);
        CBioseq_Handle bh(scope.GetBioseqHandle(*seqid));
        CSeqVector sv (bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac));
        int length (sv.size());
        sv.GetSeqData(0, length, m_contigt);
    }
    m_base = m_contigt;
}

void CMultAlign::AddAlignment(const SSamData& alignd) {
    if(count(alignd.m_seq.begin(),alignd.m_seq.end(),'N') <= m_maxNs) {
        m_aligns.push_back(CLiteAlign(alignd, m_contigt, m_indel_holder));
    }
}

void CMultAlign::AddAlignment(const CAlignModel& align) {
    TInDels all_corrections = align.FrameShifts();
    for(int i = 0; i < (int)align.Exons().size(); ++i) {
        TSignedSeqRange exon_lim = align.Exons()[i].Limits();
        TInDels corrections;
        ITERATE(TInDels, i, all_corrections) {
            if(i->IntersectingWith(exon_lim.GetFrom(), exon_lim.GetTo()))
                corrections.push_back(*i);
        }
        while(!corrections.empty() && corrections.front().Loc() == exon_lim.GetFrom()) {
            exon_lim.SetFrom(corrections.front().InDelEnd());
            corrections.erase(corrections.begin());
        }
        while(!corrections.empty() && corrections.back().InDelEnd() == exon_lim.GetTo()+1) {
            exon_lim.SetTo(corrections.back().Loc()-1);
            corrections.pop_back();
        }
        if(exon_lim.GetLength() < 3*m_min_edge)
            continue;

        int ns = 0;
        double errors = 0;
        int align_len = exon_lim.GetLength();
        ITERATE(TInDels, indl, corrections) {
            errors += indl->Len();
            if(indl->IsDeletion()) 
                align_len += indl->Len();
            if(!indl->IsInsertion()) {
                string s = indl->GetInDelV();
                ns += count(s.begin(), s.end(), 'N');
            }
        }
        if(ns > m_maxNs)
            continue;

        TLiteInDels indels;
        ITERATE(TInDels, indl, corrections) {
            if(indl->IsMismatch()) {
                indels.push_back(CLiteIndel(indl->Loc(), indl->Len()));
                indels.push_back(CLiteIndel(indl->InDelEnd(), indl->Len(), indl->GetInDelV()));                
            } else if(indl->IsInsertion()) {
                indels.push_back(CLiteIndel(indl->Loc(), indl->Len()));
            } else {
                indels.push_back(CLiteIndel(indl->Loc(), indl->Len(), indl->GetInDelV()));
            }
        }

        m_aligns.push_back(CLiteAlign(exon_lim, GroupInDels(indels), m_indel_holder, align.Weight(), errors/align_len));        
    }
}

TAlignModelList CMultAlign::GetVariationAlignList(bool correctionsonly) {

    SMatrix delta(1,1);
    TAlignModelList aligns;

    map<TSignedSeqRange,TSIMap> variations;
    list<TSignedSeqRange> confirmed_ranges;
    Variations(variations, confirmed_ranges);

    string contig_acc = m_contig_id;

    ITERATE(list<TSignedSeqRange>, i, confirmed_ranges) {
        TSignedSeqRange range = *i;
        string acc = (correctionsonly ? "CorrectionData:" : "Confirmed:")+contig_acc+":"+NStr::IntToString(range.GetFrom()+1)+":"+NStr::IntToString(range.GetTo()+1);
        CGeneModel a(ePlus, 0, CGeneModel::eSR);
        a.AddExon(range);
        aligns.push_back(CAlignModel(a, a.GetAlignMap()));
        CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Local, acc));
        aligns.back().SetTargetId(*id);
    }

    for(map<TSignedSeqRange,TSIMap>::iterator i = variations.begin(); i != variations.end(); ++i) {
        TSignedSeqRange range = i->first;
        TSIMap& seq_counts = i->second;

        if(!correctionsonly) {
            int num = 0;
            ITERATE(TSIMap, j, seq_counts) {
                const string& seq = j->first;
                int count = j->second;
                string acc = "Variant:"+contig_acc+":"+NStr::IntToString(range.GetFrom()+1)+":"+NStr::IntToString(range.GetTo()+1)+":"+NStr::IntToString(++num)+":"+NStr::IntToString(count);

                CCigar cigar = GlbAlign(seq.c_str()+m_word, seq.size()-2*m_word, m_contigt.c_str()+range.GetFrom()+m_word, range.GetLength()-2*m_word, 1, 1, delta.matrix); // don't align anchors
                CGeneModel a(ePlus, 0, CGeneModel::eSR);
                a.AddExon(range);
                a.FrameShifts() = cigar.GetInDels(range.GetFrom()+m_word, seq.c_str()+m_word, m_contigt.c_str()+range.GetFrom()+m_word);
                a.SetWeight(count);
                aligns.push_back(CAlignModel(a, a.GetAlignMap()));
                CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Local, acc));
                aligns.back().SetTargetId(*id);
            }
        } else {
            CCigar selected_cigar;
            string selected_seq;
            int selected_dist = numeric_limits<int>::max();
            double selected_weight = 0; 
            ITERATE(TSIMap, j, seq_counts) {
                const string& seq = j->first;
                int count = j->second;
                CCigar cigar = GlbAlign(seq.c_str()+m_word, seq.size()-2*m_word, m_contigt.c_str()+range.GetFrom()+m_word, range.GetLength()-2*m_word, 1, 1, delta.matrix); // don't align anchors
                int dist = cigar.Distance(seq.c_str()+m_word, m_contigt.c_str()+range.GetFrom()+m_word);
                if(dist < selected_dist || (dist == selected_dist && count > selected_weight)) {
                    selected_cigar = cigar;
                    selected_seq = seq;
                    selected_dist = dist;
                    selected_weight = count;
                }
            }
            string acc = "CorrectionData:" + contig_acc+":"+NStr::IntToString(range.GetFrom()+1)+":"+NStr::IntToString(range.GetTo()+1);
            CGeneModel a(ePlus, 0, CGeneModel::eSR);
            a.AddExon(range);
            a.FrameShifts() = selected_cigar.GetInDels(range.GetFrom()+m_word, selected_seq.c_str()+m_word, m_contigt.c_str()+range.GetFrom()+m_word);
            a.SetWeight(selected_weight);
            aligns.push_back(CAlignModel(a, a.GetAlignMap()));
            CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Local, acc));
            aligns.back().SetTargetId(*id);
        }
    }

    return aligns;
}


#define ENTROPY_LEVEL_FOR_ALIGNS 0.51

void CMultAlign::SelectAligns(vector<const CLiteAlign*>& all_alignsp) {

    ITERATE(TLiteAlignList, al, m_aligns) {
        string read = al->TranscriptSeq(m_contigt);
        string base = m_contigt.substr(al->Limits().GetFrom(),al->Limits().GetLength());
        if(min(Entropy(read),Entropy(base)) < ENTROPY_LEVEL_FOR_ALIGNS)
            continue;

        all_alignsp.push_back(&(*al));
    }

    sort(all_alignsp.begin(),all_alignsp.end(),AlignsLeftEndFirst());
}

void CMultAlign::PrepareReads(const vector<const CLiteAlign*>& all_alignsp) {
    for(int ir = 0; ir < (int)all_alignsp.size(); ++ir) {
        const CLiteAlign* al = all_alignsp[ir];
        string read = al->TranscriptSeq(m_contigt);
        m_alignsp.push_back(al);
        m_reads.push_back(read);
        m_starts.push_back(al->Limits().GetFrom());
    }
}

void CMultAlign::InsertDashesInBase() {
    int reads_num = m_reads.size();
    int contig_len = m_contigt.size();

    TIntMap deletion_len;
    for(int ir = 0; ir < reads_num; ++ir) {
        for(int p = m_alignsp[ir]->Limits().GetFrom(); p <= m_alignsp[ir]->Limits().GetTo(); ++p)
            m_contig_to_align[p] = p;
        TLiteInDelsP indels = m_alignsp[ir]->GetInDels();
        ITERATE(TLiteInDelsP, indl, indels) {
            if((*indl)->IsDeletion()) {
                int len = (*indl)->Len();
                if(indl != indels.begin()) {
                    TLiteInDelsP::const_iterator prev = indl-1;
                    if((*prev)->IsInsertion() && (*prev)->Loc()+(*prev)->Len() == (*indl)->Loc())  // mismatch
                        len -= (*prev)->Len();
                }
                if(len > 0)
                    deletion_len[(*indl)->Loc()] = max(len, deletion_len[(*indl)->Loc()]);
            }
        }
    }
    int shift = 0;
    TIntMap::iterator del = deletion_len.begin();
    NON_CONST_ITERATE(TIntMap, i, m_contig_to_align) {
        int contigp = i->first;
        if(del != deletion_len.end() && contigp == del->first) {
            int del_len = (del++)->second; 
            for(int l = 0; l < del_len; ++l) 
                m_align_to_contig[contigp+shift++] = - 1; 
        }
        int alignp = contigp+shift;
        i->second = alignp;
        m_align_to_contig[alignp] = contigp;
    }

    int total_deletion_len = 0;
    ITERATE(TIntMap, i, deletion_len)
        total_deletion_len += i->second;

    m_base.reserve(contig_len+total_deletion_len);
    m_base.clear();
    for(int p = 0; p < contig_len; ++p) {
        TIntMap::iterator rslt = deletion_len.find(p);
        if(rslt != deletion_len.end()) {
            int n = rslt->second;
            m_base.insert(m_base.size(), n, '-');
        }
        m_base.push_back(m_contigt[p]);
    }
}

void CMultAlign::InsertDashesInReads() {
    int reads_num = m_reads.size();

    int dashes = 0;
    int r = 0;
    for(int p = 0; p < (int)m_base.length(); ++p) {
        if(m_base[p] == '-') {
            ++dashes;
        } else {
            while(r < reads_num && m_starts[r]+dashes == p)
                m_starts[r++] = p;
        }
    }
    
    int base_length = m_base.length();
    for(int ir = 0; ir < reads_num; ++ir) {
        string& read = m_reads[ir];
        int start = m_starts[ir];
        if(m_contig_to_align[m_alignsp[ir]->Limits().GetTo()]-start+1 == (int)read.size())
            continue;

        TLiteInDelsP indels = m_alignsp[ir]->GetInDels();
        list<pair<int,int> > indel_pos_length;  // deletion positive length
        ITERATE(TLiteInDelsP, indl, indels) {
            if((*indl)->IsDeletion()) {
                if(!indel_pos_length.empty() && indel_pos_length.back().first-indel_pos_length.back().second == (*indl)->Loc()) {  // mismatch
                    _ASSERT(indel_pos_length.back().second < 0);
                    int new_len = indel_pos_length.back().second+(*indl)->Len();
                    if(new_len < 0) {         // still insertion
                        indel_pos_length.back().second = new_len; 
                    } else if(new_len > 0) {  // becomes deletion
                        indel_pos_length.back().first = (*indl)->Loc();
                        indel_pos_length.back().second = new_len;
                    } else {                  // pure mismatch
                        indel_pos_length.pop_back();
                    }
                } else {
                    indel_pos_length.push_back(make_pair((*indl)->Loc(), (*indl)->Len()));
                }
            } else {
                indel_pos_length.push_back(make_pair((*indl)->Loc(), -(*indl)->Len()));
            }
        }

        list<pair<int,int> >::iterator indl = indel_pos_length.begin();
        for(int p = start+1; p < base_length && p < start+(int)read.size(); ) {
            if(m_base[p] == '-') {
                int len = 1;
                while(p+len < base_length && m_base[p+len] == '-')
                    ++len;
                int insertp = p-start;
                p += len;
                if(indl != indel_pos_length.end() && indl->second > 0 &&  m_contig_to_align[indl->first] == p) {
                    len -= indl->second;
                    ++indl;
                }
                if(len > 0)
                    read.insert(insertp,len,'-');
            } else if(indl != indel_pos_length.end() && indl->second < 0 && m_contig_to_align[indl->first] == p) {  // left of insertion
                int n = m_contig_to_align[indl->first-indl->second-1]-p+1;
                read.insert(p-start,n,'-');
                p += n;
                ++indl;
            } else {
                ++p;
            }
        }
    }
}

void CMultAlign::GetCounts() {
    int reads_num = m_reads.size();
    //    m_counts.resize(m_base.size());
    for(int ir = 0; ir < reads_num; ++ir) {
        int w = m_alignsp[ir]->Weight()+0.5;
        const string& read = m_reads[ir];
        m_max_len = max(m_max_len,(int)read.size());
        int start = m_starts[ir];
        TSignedSeqRange legit_range = LegitRange(ir);
        for(int p = legit_range.GetFrom(); p <= legit_range.GetTo(); ++p) {
            char c = read[p-start];
            m_counts[p][c] += w;
        }
    }
}

void CMultAlign::PrepareStats() {
    vector<const CLiteAlign*> all_alignsp;
    SelectAligns(all_alignsp);

    int aligns_size = all_alignsp.size();
    m_reads.reserve(aligns_size);
    m_starts.reserve(aligns_size);
    m_alignsp.reserve(aligns_size);

    PrepareReads(all_alignsp);
    InsertDashesInBase();
    InsertDashesInReads();
    GetCounts();
}

void CMultAlign::SeqCountsBetweenTwoStrongWords(const TSignedSeqRange& prev_strong_word_range, const string& prev_strong_word, const TSignedSeqRange& strong_word_range, const string& strong_word,  TSIMap& seq_counts, int& total_cross, int& accepted_cross) {

    total_cross = 0;
    accepted_cross = 0;
    TSignedSeqRange two_word_range(prev_strong_word_range.GetFrom(),strong_word_range.GetTo());
    TSignedSeqRange weak_range(prev_strong_word_range.GetTo()+1,strong_word_range.GetFrom()-1);
    int ir_first = lower_bound(m_starts.begin(),m_starts.end(),two_word_range.GetFrom()-m_max_len)-m_starts.begin();
    for(int ir = ir_first; ir < (int)m_alignsp.size() && m_starts[ir] <= two_word_range.GetFrom(); ++ir) {
        int w = m_alignsp[ir]->Weight()+0.5;
        TSignedSeqRange read_range(m_starts[ir],m_starts[ir]+m_reads[ir].size()-1);
        if(Include(read_range,two_word_range)) {
            total_cross += w;
            if(prev_strong_word == EmitSequenceFromRead(ir, prev_strong_word_range)
               && strong_word == EmitSequenceFromRead(ir, strong_word_range)) {
                accepted_cross += w;
                string read_seq = EmitSequenceFromRead(ir, weak_range);
                seq_counts[read_seq] += w;
            }
        }
    }
}

TSignedSeqRange CMultAlign::LegitRange(int ir) {
    const string& read = m_reads[ir];
    int start = m_starts[ir];
    int end = start+read.size()-1;

    int first_legit_match = start;
    int shift = 0;
    while(shift < m_min_edge || (first_legit_match <= end && (read[first_legit_match-start] == '-' || read[first_legit_match-start] != m_base[first_legit_match]))) {
        if(m_base[first_legit_match] != '-')
            ++shift;
        ++first_legit_match;
    }
    int last_legit_match = end;
    shift = 0;
    while(shift < m_min_edge || (last_legit_match >= start && (read[last_legit_match-start] == '-' || read[last_legit_match-start] != m_base[last_legit_match]))) {
        if(m_base[last_legit_match] != '-')
            ++shift;
        --last_legit_match;
    }

    return TSignedSeqRange(first_legit_match, last_legit_match);
}


void CMultAlign::Variations(map<TSignedSeqRange,TSIMap>& variations, list<TSignedSeqRange>& confirmed_ranges) {

    if(m_aligns.empty())
        return;

    PrepareStats();

    string maximal_bases(m_base.size(),'A');
    for(int p = 0; p < (int)m_base.size(); ++p) {
        map<int,TCharIntMap>::const_iterator rslt = m_counts.find(p);
        if(rslt == m_counts.end()) {
            maximal_bases[p] = '#';
            continue;
        }

        const map<char,int>& count = rslt->second;
        char c = 0;
        int w = 0;
        int t = 0;
        for(map<char,int>::const_iterator i = count.begin(); i != count.end(); ++i) {
            if(i->second > w) {
                c = i->first;
                w = i->second;
            }
            t += i->second;
        }
        if(t < m_min_coverage) {
            maximal_bases[p] = '#';
        } else {
            _ASSERT(c != 0);
            maximal_bases[p] = c;
        }
    }

    TSignedSeqRange prev_strong_word_range;
    string prev_strong_word;
    for(int p = 0; p < (int)maximal_bases.size(); ) {
        if(maximal_bases[p] == '#') {
            prev_strong_word_range = TSignedSeqRange::GetEmpty();
            ++p;
        } else {
            string strong_word;
            TSignedSeqRange strong_word_range;
            int first_gap;
            FindNextStrongWord(p, maximal_bases, strong_word, strong_word_range, first_gap);

            if(strong_word_range.Empty()) // end of contig
                break;

            p = strong_word_range.GetFrom()+1;

            bool same_as_contig = true;
            for(int pos = strong_word_range.GetFrom(); pos <= strong_word_range.GetTo() && same_as_contig; ++pos)
                same_as_contig = (maximal_bases[pos] == m_base[pos]);

            if(!same_as_contig) {        // genomic error
                continue;
            } else if(first_gap >= 0) {                           // there is a strong word after a gap
                prev_strong_word_range = TSignedSeqRange::GetEmpty();
            }

            bool there_is_weak_range = prev_strong_word_range.NotEmpty() && prev_strong_word_range.GetTo()+1 < strong_word_range.GetFrom();

            while(m_align_to_contig[strong_word_range.GetFrom()] < 0)  // shift from possible dashes
                strong_word_range.SetFrom(strong_word_range.GetFrom()+1);

            int swordl = m_align_to_contig[strong_word_range.GetFrom()];
            int swordr = m_align_to_contig[strong_word_range.GetTo()];

            if(there_is_weak_range) {  // there is a weak range 
                TSIMap seq_counts;
                int total_cross = 0;
                int accepted_cross = 0;
                SeqCountsBetweenTwoStrongWords(prev_strong_word_range, prev_strong_word, strong_word_range, strong_word, seq_counts, total_cross, accepted_cross);

                //                cout << "SeqCounts:" << endl;

                if(!seq_counts.empty()) {
                    TSIMap::const_iterator most_frequent_variant = seq_counts.begin();
                    for(TSIMap::const_iterator i = seq_counts.begin(); i != seq_counts.end(); ++i) {
                        
                        //                        cout << i->first << " " << i->second << endl;

                        if(i->second > most_frequent_variant->second)
                            most_frequent_variant = i;
                    }

                    TSIMap var_counts;
                    ITERATE(TSIMap, it, seq_counts) {
                        string var_seq = prev_strong_word+it->first+strong_word;
                        if(it->second >= max(m_min_abs_support_for_variant,(int)(m_min_rel_support_for_variant*most_frequent_variant->second+0.5)))
                            var_counts[var_seq] = it->second;
                    }

                    if(!var_counts.empty()) {
                        int base_posl = m_align_to_contig[prev_strong_word_range.GetFrom()];
                        int base_posr = swordr;
                        TSignedSeqRange var_range(base_posl,base_posr);

                        if(var_counts.size() == 1 && var_counts.begin()->first == m_contigt.substr(base_posl,base_posr-base_posl+1)) {  // confirmed weak range
                            confirmed_ranges.back().SetTo(base_posr);
                            there_is_weak_range = false;
                        } else {                        
                            map<TSignedSeqRange,TSIMap>::value_type var(var_range,var_counts);
                            variations.insert(var);
                        }
                    }
                }
            }  


            prev_strong_word_range = strong_word_range;
            prev_strong_word = strong_word;

            if(confirmed_ranges.empty() || there_is_weak_range || confirmed_ranges.back().GetTo()+1 < swordl)
                confirmed_ranges.push_back(TSignedSeqRange(swordl,swordr));
            else
                confirmed_ranges.back().SetTo(swordr);
        }
    }
}

string CMultAlign::EmitSequenceFromRead(int ir, const TSignedSeqRange& word_range) {
    const string& read = m_reads[ir];
    int start = m_starts[ir];
    int stop = start+read.size()-1;
    string read_word;
    for(int r = max(start,word_range.GetFrom()); r <= min(stop,word_range.GetTo()); ++r) {
        if(read[r-start] != '-')
            read_word.push_back(read[r-start]);
    }
    return read_word;
}

string CMultAlign::EmitSequenceFromBase(const TSignedSeqRange& word_range) {
    string read_word;
    for(int r = word_range.GetFrom(); r <= word_range.GetTo(); ++r) {
        if(m_base[r] != '-')
            read_word.push_back(m_base[r]);
    }
    return read_word;
}

bool CMultAlign::CheckWord(const TSignedSeqRange& word_range, const string& word) {
    int total = 0;
    int match = 0;
    int ir_first = lower_bound(m_starts.begin(),m_starts.end(),word_range.GetFrom()-m_max_len)-m_starts.begin();
    for(int ir = ir_first; ir < (int)m_alignsp.size() && m_starts[ir] <= word_range.GetFrom(); ++ir) {
        int w = m_alignsp[ir]->Weight()+0.5;
        TSignedSeqRange legit_range = LegitRange(ir);
        if(Include(legit_range,word_range)) {
            total += w;
            string read_word = EmitSequenceFromRead(ir, word_range);
            if(word == read_word)
                match += w;
        }
    }

    if(match > m_strong_consensus*total) {
        return true;
    } else {
        return false;
    }
}

int CMultAlign::FindNextStrongWord(int nextp, const string& maximal_bases, string& strong_word, TSignedSeqRange& strong_word_range, int& first_gap) {
    first_gap = -1;
    while(nextp < (int)maximal_bases.size()) {
        string word;
        int word_start = nextp;
        int word_end = nextp;
        bool low_complexity = false;
        for( ; word_end < (int)maximal_bases.size() && (int)word.size() < m_word && maximal_bases[word_end] != '#'; ++word_end) {
            if(maximal_bases[word_end] != '-') {
                word.push_back(toupper(maximal_bases[word_end]));
                if(islower(maximal_bases[word_end]))
                    low_complexity = true; 
            }
        }

        if((int)word.size() < m_word) {
            if(maximal_bases[word_end] == '#') { // we reached a gap
                if(first_gap < 0) first_gap = word_end;
                nextp = word_end+1;
                continue;
            } else {                             // we reached the end
                return word_end;
            }
        }

        TSignedSeqRange word_range(word_start,word_end-1);
        if(!low_complexity && CheckWord(word_range,word)) {
            strong_word = word;
            strong_word_range = word_range;
            return word_end;
        }
        ++nextp;
    }

    return nextp;
}

void CMultAlign::SetupArgDescriptions(CArgDescriptions* arg_desc) {
    arg_desc->AddDefaultKey("word", "word", "Elementary size for anchor regions", CArgDescriptions::eInteger, "8");
    arg_desc->AddDefaultKey("min_edge", "min_edge", "This many bases are ignored on both side of alignment", CArgDescriptions::eInteger, "5");
    arg_desc->AddDefaultKey("min_coverage", "min_coverage", "Minimal coverage on contig", CArgDescriptions::eInteger, "3");
    arg_desc->AddDefaultKey("maxNs", "maxNs", "Maximal number of Ns allowed per alignment", CArgDescriptions::eInteger, "2");
    arg_desc->AddDefaultKey("min_abs_support", "min_abs_support", "Minimal number of reads to support a variant", CArgDescriptions::eInteger, "3");
    arg_desc->AddDefaultKey("min_rel_support", "min_rel_support", "Minimal fraction of reads to support a variant", CArgDescriptions::eDouble, "0.075");
    arg_desc->AddDefaultKey("strong_consensus", "strong_consensus", "Minimal fraction of alignmnets to support a strong word", CArgDescriptions::eDouble, "0.85");
}

void CMultAlign::ProcessArgs(const CArgs& args) {

    m_word = args["word"].AsInteger();
    m_min_edge = args["min_edge"].AsInteger();
    m_min_coverage = args["min_coverage"].AsInteger();   
    m_maxNs = args["maxNs"].AsInteger();
    m_min_rel_support_for_variant = args["min_rel_support"].AsDouble();
    m_min_abs_support_for_variant = args["min_abs_support"].AsInteger();
    m_strong_consensus = args["strong_consensus"].AsDouble();
}

void CMultAlign::SetDefaultParams() {
    m_word = 8;
    m_min_edge = 5;
    m_min_coverage = 3;   
    m_maxNs = 2;
    m_min_rel_support_for_variant = 0.075;
    m_min_abs_support_for_variant = 3;
    m_strong_consensus = 0.85;
}


END_SCOPE(gnomon)
END_NCBI_SCOPE
