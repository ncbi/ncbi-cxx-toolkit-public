#include <ncbi_pch.hpp>
#include <algo/gnomon/variations.hpp>
#include <algo/gnomon/id_handler.hpp>
#include <algo/gnomon/glb_align.hpp>

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <sstream>
#include <numeric>      // std::accumulate


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

CLiteAlign::CLiteAlign(TSignedSeqRange range, const TLiteInDels& indels, set<CLiteIndel>& indel_holder, double weight, double ident, const string& notalignedl, const string& notalignedr) :
         m_weight(weight), m_ident(ident), m_range(range),m_notalignedl(notalignedl), m_notalignedr(notalignedr) {

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
            if(seq_pos == 0)
                m_notalignedl = seq.substr(0,len);
            else
                m_notalignedr = seq.substr(seq_pos);
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
    m_reads_notalignedl.clear();
    m_reads_notalignedr.clear();
    m_starts.clear();
    m_alignsp.clear();
    m_contig_to_align.clear();
    m_align_to_contig.clear();
    m_counts.clear();
    m_coverage.clear();
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
    
    cout << "Get contig" << endl;
    system("ps -u souvorov u | egrep 'USER|variations'");
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

    cout << "All aligns" << endl;
    system("ps -u souvorov u | egrep 'USER|variations'");

    SMatrix delta(1,1);
    TAlignModelList aligns;

    map<TSignedSeqRange,TSIMap> variations;
    list<TSignedSeqRange> confirmed_ranges;
    //    Variations(variations, confirmed_ranges, correctionsonly);
    Variations(variations, confirmed_ranges, false);

    cout << "After variations" << endl;
    system("ps -u souvorov u | egrep 'USER|variations'");

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
#define COVERAGE_LIMIT 100.
#define RELATIVE_COVERAGE_LIMIT 0.1

void CMultAlign::SelectAligns(vector<const CLiteAlign*>& all_alignsp, bool use_limits) {

    if(use_limits) {
        int contig_len = m_base.size();
        vector<int> desired_base_coverage(contig_len);
        vector<vector<int> > desired_notalignedl_coverage(contig_len);
        vector<vector<int> > desired_notalignedr_coverage(contig_len);
        vector<CLiteAlign*> raw_alignsp;
        raw_alignsp.reserve(m_aligns.size());
        NON_CONST_ITERATE(TLiteAlignList, al, m_aligns) {
            string read = al->TranscriptSeq(m_contigt);
            string base = m_contigt.substr(al->Limits().GetFrom(),al->Limits().GetLength());
            if(min(Entropy(read),Entropy(base)) < ENTROPY_LEVEL_FOR_ALIGNS)
                continue;

            raw_alignsp.push_back(&(*al));
            int w = al->Weight()+0.5;
            for(int p = al->Limits().GetFrom(); p <= al->Limits().GetTo(); ++p) {
                desired_base_coverage[p] += w;
            }
            int notalignedl = al->NotAlignedL().size();
            desired_notalignedl_coverage[al->Limits().GetFrom()].resize(notalignedl);
            for(int p = 0; p < notalignedl; ++p) {
                desired_notalignedl_coverage[al->Limits().GetFrom()][p] += w;
            }
            int notalignedr = al->NotAlignedR().size();
            desired_notalignedr_coverage[al->Limits().GetTo()].resize(notalignedr);
            for(int p = 0; p < notalignedr; ++p) {
                desired_notalignedr_coverage[al->Limits().GetTo()][p] += w;
            }
        }
        for(int i = 0; i < contig_len; ++i) {
            desired_base_coverage[i] = max(COVERAGE_LIMIT,RELATIVE_COVERAGE_LIMIT*desired_base_coverage[i])+0.5;
            NON_CONST_ITERATE(vector<int>, j, desired_notalignedl_coverage[i])
                *j = max(COVERAGE_LIMIT,*j*RELATIVE_COVERAGE_LIMIT)+0.5;
            NON_CONST_ITERATE(vector<int>, j, desired_notalignedr_coverage[i])
                *j = max(COVERAGE_LIMIT,*j*RELATIVE_COVERAGE_LIMIT)+0.5;
        }

        sort(raw_alignsp.begin(),raw_alignsp.end(),AlignsHighIdentFirst());

        for(int ir = 0; ir < (int)raw_alignsp.size(); ++ir) {
            CLiteAlign& align = *raw_alignsp[ir];
            int notalignedl = align.NotAlignedL().size();
            int notalignedr = align.NotAlignedR().size();

            int needed_weight = 0;
            for(int p = align.Limits().GetFrom(); p <= align.Limits().GetTo(); ++p) {
                needed_weight = max(needed_weight,desired_base_coverage[p]);
            }
            for(int p = 0; p < notalignedl; ++p) {
                needed_weight = max(needed_weight,desired_notalignedl_coverage[align.Limits().GetFrom()][p]);
            }
            for(int p = 0; p < notalignedr; ++p) {
                needed_weight = max(needed_weight,desired_notalignedr_coverage[align.Limits().GetTo()][p]);
            }

            if(needed_weight == 0)
                continue;
            else if(needed_weight < align.Weight())
                align.SetWeight(needed_weight);

            int w = align.Weight()+0.5;

            for(int p = align.Limits().GetFrom(); p <= align.Limits().GetTo(); ++p) {            
                desired_base_coverage[p] -= w;
            }
            for(int p = 0; p < notalignedl; ++p) {
                desired_notalignedl_coverage[align.Limits().GetFrom()][p] -= w;
            }
            for(int p = 0; p < notalignedr; ++p) {
                desired_notalignedr_coverage[align.Limits().GetTo()][p] -= w;
            }

            all_alignsp.push_back(raw_alignsp[ir]);
        }
    } else {
        ITERATE(TLiteAlignList, al, m_aligns) 
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
        m_reads_notalignedl.push_back(&al->NotAlignedL());
        m_reads_notalignedr.push_back(&al->NotAlignedR());
        m_starts.push_back(al->Limits().GetFrom());
    }
}

void CMultAlign::InsertDashesInBase() {
    int reads_num = m_reads.size();
    int contig_len = m_contigt.size();

    vector<int> deletion_len(contig_len,0);
    for(int ir = 0; ir < reads_num; ++ir) {
        TLiteInDelsP indels = m_alignsp[ir]->GetInDels();
        ITERATE(TLiteInDelsP, indl, indels) {
            if((*indl)->IsDeletion())
                deletion_len[(*indl)->Loc()] = max((*indl)->Len(), deletion_len[(*indl)->Loc()]);
        }
    }
    int total_deletion_len = accumulate(deletion_len.begin(),deletion_len.end(),0);

    m_base.clear();
    m_base.reserve(contig_len+total_deletion_len);
    m_align_to_contig.reserve(contig_len+total_deletion_len);
    m_contig_to_align.reserve(contig_len);

    for(int p = 0; p < contig_len; ++p) {
        int n = deletion_len[p];
        if(n > 0) {
            m_base.insert(m_base.size(), n, '-');
            m_align_to_contig.insert(m_align_to_contig.end(), n, -1);
        }
        m_contig_to_align.push_back(m_base.size());
        m_align_to_contig.push_back(p);
        m_base.push_back(m_contigt[p]);
    }

    /*
    m_contig_to_align.resize(contig_len,0);
    m_align_to_contig.resize(contig_len,0);
    for(int i = 0; i < contig_len; ++i) {
        m_contig_to_align[i] = i;
        m_align_to_contig[i] = i;
    }

    for(int ir = 0; ir < reads_num; ++ir) {
        TLiteInDelsP indels = m_alignsp[ir]->GetInDels();
        ITERATE(TLiteInDelsP, indl, indels) {
            if((*indl)->IsDeletion()) {
                int align_pos = m_contig_to_align[(*indl)->Loc()];
                int existing_deletions = 0;
                while(align_pos-existing_deletions-1 >= 0 && m_base[align_pos-existing_deletions-1] == '-')
                    ++existing_deletions;
                if(existing_deletions < (*indl)->Len()) {
                    int n = (*indl)->Len()-existing_deletions;
                    m_base.insert(align_pos, n, '-');
                    m_align_to_contig.insert(m_align_to_contig.begin()+align_pos, n, -1);
                    for(int p = (*indl)->Loc(); p < contig_len; ++p)
                        m_contig_to_align[p] += n;
                }
            }
        }
    }
    _ASSERT(m_contig_to_align.back() == (int)m_base.size()-1);
    */
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
        TLiteInDelsP indels = m_alignsp[ir]->GetInDels();
        string& read = m_reads[ir];
        int start = m_starts[ir];
        TLiteInDelsP::iterator indl = indels.begin();
        for(int p = start+1; p < base_length && p < start+(int)read.size(); ) {
            if(m_base[p] == '-') {
                int len = 1;
                while(p+len < base_length && m_base[p+len] == '-')
                    ++len;
                int insertp = p-start;
                p += len;
                if(indl != indels.end() && (*indl)->IsDeletion() &&  m_contig_to_align[(*indl)->Loc()] == p) {
                    _ASSERT(len >= (*indl)->Len());
                    len -= (*indl)->Len();
                    ++indl;
                }
                if(len > 0)
                    read.insert(insertp,len,'-');
            } else if(indl != indels.end() && (*indl)->IsInsertion() && m_contig_to_align[(*indl)->Loc()] == p) {  // left of insertion
                int n = m_contig_to_align[(*indl)->Loc()+(*indl)->Len()-1]-p+1;
                read.insert(p-start,n,'-');
                p += n;
                ++indl;
            } else {
                ++p;
            }
        }
        _ASSERT(indl == indels.end());
    }
}

void CMultAlign::GetCounts() {
    int reads_num = m_reads.size();
    m_counts.resize(m_base.size());
    m_coverage.resize(m_base.size());
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
        for(int p = start; p < (int)(start+read.size()); ++p) {
            m_coverage[p] += w;
        }
    }
}

void CMultAlign::PrepareStats(bool use_limits) {

    CStopWatch timer;
    timer.Start();

    vector<const CLiteAlign*> all_alignsp;
    SelectAligns(all_alignsp, use_limits);

    cout << "SelectAligns" << endl;
    system("ps -u souvorov u | egrep 'USER|variations'");

    LOG_POST("SelectAligns in " << timer.Elapsed());
    timer.Restart();

    int aligns_size = all_alignsp.size();
    m_reads.reserve(aligns_size);
    m_reads_notalignedl.reserve(aligns_size);
    m_reads_notalignedr.reserve(aligns_size);
    m_starts.reserve(aligns_size);
    m_alignsp.reserve(aligns_size);

    PrepareReads(all_alignsp);

    cout << "Prepare reads" << endl;
    system("ps -u souvorov u | egrep 'USER|variations'");
    LOG_POST("Prepare reads in " << timer.Elapsed());
    timer.Restart();

    InsertDashesInBase();


    cout << "Insert dashes in base" << endl;
    system("ps -u souvorov u | egrep 'USER|variations'");
    LOG_POST("InsertDashesInBase in " << timer.Elapsed());
    timer.Restart();

    InsertDashesInReads();

    cout << "Insert dashes in reads" << endl;
    system("ps -u souvorov u | egrep 'USER|variations'");
    LOG_POST("InsertDashesInReads in " << timer.Elapsed());
    timer.Restart();

    GetCounts();

    cout << "Get counts" << endl;
    system("ps -u souvorov u | egrep 'USER|variations'");
    LOG_POST("GetCounts in " << timer.Elapsed());
    timer.Restart();
}

pair<string,bool> CMultAlign::SelectVariant(TSIMap& seq_counts, const TSignedSeqRange weak_range, int total_cross, int accepted_cross, int chunk, int pos, TSIMap* additional_tsp) {
    string max_seq;
    bool found = false;

    string base;
    for(int p = weak_range.GetFrom(); p <= weak_range.GetTo(); ++p) {
        if(m_base[p] != '-')
            base.push_back(m_base[p]);
    }

    if(!seq_counts.empty()) {
        TSIMap::const_iterator most_frequent_variant = seq_counts.begin();
        for(TSIMap::const_iterator i = seq_counts.begin(); i != seq_counts.end(); ++i) {
            if(i->second > most_frequent_variant->second)
                most_frequent_variant = i;
        }

        TSIMap::const_iterator selected_variant = most_frequent_variant;
        int sdist = EditDistance(base, selected_variant->first);
        for(TSIMap::iterator iloop = seq_counts.begin(); iloop != seq_counts.end(); ) {
            TSIMap::iterator i = iloop++;

            if(i->second < max(m_min_abs_support_for_variant,(int)(m_min_rel_support_for_variant*most_frequent_variant->second+0.5))) {
                seq_counts.erase(i);
                continue;
            }

            int dist = EditDistance(base, i->first);
            if(dist < sdist || (dist == sdist && i->second > selected_variant->second)) {
                selected_variant = i;
                sdist = dist;
            }
        }

        if(seq_counts.empty())
            return make_pair(max_seq,found);                        

        multimap<int,string,greater<int> > other_variants;
        for(TSIMap::const_iterator i = seq_counts.begin(); i != seq_counts.end(); ++i) {
            if(i != selected_variant)
                other_variants.insert(multimap<int,string,greater<int> >::value_type(i->second,i->first));
        }

        int bp = weak_range.GetFrom();
        while(bp < (int)m_align_to_contig.size() && m_align_to_contig[bp] < 0)
            ++bp;
        int base_posl = m_align_to_contig[bp];
        bp = weak_range.GetTo();
        while(bp >0 && m_align_to_contig[bp] < 0)
            --bp;
        int base_posr = m_align_to_contig[bp];

        if(additional_tsp != 0)
            pos += (*additional_tsp)[selected_variant->first];

        int variant_num = 0;
        cerr << "Variant_" << ++variant_num << "\t" << base_posl+1 << '\t' << base_posr+1 << '\t' << chunk << '\t' << pos+1 << '\t' << total_cross << ' ' << accepted_cross << "\t";
        cerr << selected_variant->second << '\t' << sdist << '\t' << selected_variant->first << '\t' << base << endl;
        for(multimap<int,string,greater<int> >::const_iterator i = other_variants.begin(); i != other_variants.end(); ++i) {
            int dist = EditDistance(base, i->second);
            cerr << "Variant_" << ++variant_num << "\t" << base_posl+1 << '\t' << base_posr+1 << "\t" << chunk << '\t' << pos+1 << '\t' << total_cross << ' ' << accepted_cross << "\t";
            cerr << i->first << '\t' << dist << '\t' << i->second << '\t' << base << endl;
        }

        max_seq = selected_variant->first;
        found = true;
    }
    
    return make_pair(max_seq,found);                        
}

#define MIN_POLYA 7
#define MIN_FRAC_OF_A 0.8

pair<string,bool> CMultAlign::SupportedSeqBetweenStrongWordAndPolya(const TSignedSeqRange& strong_word_range, const string& strong_word, int chunk, int pos) {
    TSIMap seq_counts;
    TSIMap additional_as;
    int total_cross = 0;
    int accepted_cross = 0;
    TSignedSeqRange weak_range(strong_word_range.GetTo()+1,m_base.size()-1);
    int ir_first = lower_bound(m_starts.begin(),m_starts.end(),weak_range.GetFrom()-m_max_len)-m_starts.begin();
    int reads_num = m_alignsp.size();
    for(int ir = ir_first; ir < reads_num && m_starts[ir] <= strong_word_range.GetFrom(); ++ir) {
        int w = m_alignsp[ir]->Weight()+0.5;
        int start = m_starts[ir];
        TSignedSeqRange read_range(m_starts[ir],start+m_reads[ir].size()-1);
        if(read_range.GetTo() >= weak_range.GetFrom()+MIN_POLYA-1) {
            total_cross += w;

            if(strong_word != EmitSequenceFromRead(ir, strong_word_range))
                continue;

            string read_seq = EmitSequenceFromRead(ir, weak_range);
            string polya(MIN_POLYA,'A');
            size_t polya_pos =  read_seq.find(polya);
            if(polya_pos == string::npos)
                continue;

            int additional = read_seq.size()-polya_pos-MIN_POLYA;
            size_t not_a = read_seq.find_first_not_of('A',polya_pos+MIN_POLYA);
            if(not_a != string::npos)
                additional = not_a-polya_pos-MIN_POLYA;

            read_seq = read_seq.substr(0,polya_pos+MIN_POLYA);
            additional_as[read_seq] = max(additional,additional_as[read_seq]);

            accepted_cross += w;
            seq_counts[read_seq] += w;
        }
    }

    pair<string,bool> selected_variant = SelectVariant(seq_counts, weak_range, total_cross, accepted_cross, chunk, pos, 0);
    int additional = additional_as[selected_variant.first];
    if(additional > 0)
        selected_variant.first += string(additional,'A');

    return selected_variant;
}

pair<string,bool>  CMultAlign::SupportedSeqBetweenPolytAndStrongWord(const TSignedSeqRange& strong_word_range, const string& strong_word, int chunk, int pos) {
    TSIMap seq_counts;
    TSIMap additional_ts;
    int total_cross = 0;
    int accepted_cross = 0;
    TSignedSeqRange weak_range(0,strong_word_range.GetFrom()-1);
    int ir_first = lower_bound(m_starts.begin(),m_starts.end(),strong_word_range.GetTo()-m_max_len)-m_starts.begin();
    int reads_num = m_alignsp.size();
    for(int ir = ir_first; ir < reads_num && m_starts[ir] <= weak_range.GetTo()-MIN_POLYA+1; ++ir) {
        int w = m_alignsp[ir]->Weight()+0.5;
        int start = m_starts[ir];
        TSignedSeqRange read_range(start,m_starts[ir]+m_reads[ir].size()-1);
        if(read_range.GetTo() >= strong_word_range.GetTo()) {
            total_cross += w;

            if(strong_word != EmitSequenceFromRead(ir, strong_word_range))
                continue;

            string read_seq = EmitSequenceFromRead(ir, weak_range);
            string polyt(MIN_POLYA,'T');
            size_t polyt_pos =  read_seq.rfind(polyt);
            if(polyt_pos == string::npos)
                continue;

            int additional = polyt_pos;
            size_t not_t = read_seq.find_last_not_of('T',polyt_pos);
            if(not_t != string::npos)
                additional = polyt_pos-not_t-1;

            read_seq = read_seq.substr(polyt_pos);
            additional_ts[read_seq] = max(additional,additional_ts[read_seq]);

            accepted_cross += w;
            seq_counts[read_seq] += w;
        }
    }

    pair<string,bool> selected_variant = SelectVariant(seq_counts, weak_range, total_cross, accepted_cross, chunk, pos, &additional_ts);
    int additional = additional_ts[selected_variant.first];
    if(additional > 0)
        selected_variant.first = string(additional,'T')+selected_variant.first;

    return selected_variant;
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

pair<string,bool>  CMultAlign::SupportedSeqBetweenTwoStrongWords(const TSignedSeqRange& prev_strong_word_range, const string& prev_strong_word, const TSignedSeqRange& strong_word_range, const string& strong_word, int chunk, int pos) {

    TSIMap seq_counts;
    int total_cross = 0;
    int accepted_cross = 0;
    SeqCountsBetweenTwoStrongWords(prev_strong_word_range, prev_strong_word, strong_word_range, strong_word, seq_counts, total_cross, accepted_cross);
    TSignedSeqRange weak_range(prev_strong_word_range.GetTo()+1,strong_word_range.GetFrom()-1);

    return SelectVariant(seq_counts, weak_range, total_cross, accepted_cross, chunk, pos, 0);
}

#define MIN_OVERLAP 15
#define MIN_CONNECTORS 1
#define MAX_DIST 0.3

typedef multimap<string, int> TSuffixIndex;

pair<string,bool>  CMultAlign::CombinedSeqBetweenTwoStrongWords(const TSignedSeqRange& prev_strong_word_range, const string& prev_strong_word, const TSignedSeqRange& strong_word_range, const string& strong_word, int chunk, int pos) {

    TSignedSeqRange weak_range(prev_strong_word_range.GetTo()+1,strong_word_range.GetFrom()-1);
    int wsize = strong_word.size();
    string base;
    for(int p = weak_range.GetFrom(); p <= weak_range.GetTo(); ++p) {
        if(m_base[p] != '-')
            base.push_back(m_base[p]);
    }

    vector<string> ireads;
    vector<int> iweights;
    TSuffixIndex isuffixind;
    int ir_first = lower_bound(m_starts.begin(),m_starts.end(),prev_strong_word_range.GetFrom()-m_max_len)-m_starts.begin();
    for(int ir = ir_first; ir < (int)m_alignsp.size() && m_starts[ir] <= prev_strong_word_range.GetFrom(); ++ir) {
        int iw = m_alignsp[ir]->Weight()+0.5;
        TSignedSeqRange iread_range(m_starts[ir],m_starts[ir]+m_reads[ir].size()-1);
        if(Include(iread_range,prev_strong_word_range) && !Include(iread_range,strong_word_range.GetTo()) && prev_strong_word == EmitSequenceFromRead(ir, prev_strong_word_range)) {
            string iseq = EmitSequenceFromRead(ir, TSignedSeqRange(prev_strong_word_range.GetFrom(),numeric_limits<int>::max()))+*m_reads_notalignedr[ir];
            if(iseq.size() >= MIN_OVERLAP) {
                string iend = iseq.substr(iseq.size()-MIN_OVERLAP);
                isuffixind.insert(TSuffixIndex::value_type(iend,(int)ireads.size()));
                ireads.push_back(iseq);
                iweights.push_back(iw);
            }
        }
    }
    int inum = ireads.size();

    vector<string> jreads;
    vector<int> jweights;
    TSuffixIndex jsuffixind;
    int jr_first = lower_bound(m_starts.begin(),m_starts.end(),strong_word_range.GetFrom()-m_max_len)-m_starts.begin();
    for(int jr = jr_first; jr < (int)m_alignsp.size() && m_starts[jr] <= strong_word_range.GetFrom(); ++jr) {
        int jw = m_alignsp[jr]->Weight()+0.5;
        TSignedSeqRange jread_range(m_starts[jr],m_starts[jr]+m_reads[jr].size()-1);
        if(Include(jread_range,strong_word_range) && !Include(jread_range,prev_strong_word_range.GetFrom()) && strong_word == EmitSequenceFromRead(jr, strong_word_range)) {
            string jseq = *m_reads_notalignedl[jr]+EmitSequenceFromRead(jr, TSignedSeqRange(0,strong_word_range.GetTo()));
            if(jseq.size() >= MIN_OVERLAP) {
                string jend = jseq.substr(jseq.size()-MIN_OVERLAP);
                jsuffixind.insert(TSuffixIndex::value_type(jend,(int)jreads.size()));
                jreads.push_back(jseq);
                jweights.push_back(jw);
            }
        }
    }
    int jnum = jreads.size();

    
    vector<set<string> > iconnectors(inum);
    vector<set<string> > jconnectors(jnum);
    map<string,pair<int,int> > counts;

    if(!jsuffixind.empty()) {
        for(int i = 0; i < inum; ++i) {   // finds connectors when right side of jseq matches iseq    
            const string& iseq = ireads[i];
            int iw = iweights[i];
            for(int n = 0; n < (int)iseq.size()-MIN_OVERLAP; ++n) {  // don't match the last position of iseq   
                string pattern = iseq.substr(n,MIN_OVERLAP);
                pair<TSuffixIndex::const_iterator,TSuffixIndex::const_iterator> er = jsuffixind.equal_range(pattern);  // matching jsuffixes    
                for(TSuffixIndex::const_iterator it = er.first; it != er.second; ++it) {
                    int j = it->second;
                    const string& jseq = jreads[j];
                    int jw = jweights[j];

                    int jmin = jseq.size()-MIN_OVERLAP;
                    int imin = n;
                    while(imin > 0 && jmin > 0 && iseq[imin-1] == jseq[jmin-1]) {
                        --imin;
                        --jmin;
                    }
                    if(imin == 0 || jmin == 0) {   // left end of iseq matched or ALL jseq matched          
                        int imax = n+MIN_OVERLAP-1-wsize;
                        if(imax >= wsize-1) {      // didn't clip prev_strong_word and were not connected before        
                            string connector = iseq.substr(wsize,imax-wsize+1);
                            if(iconnectors[i].insert(connector).second)
                                counts[connector].first += iw;
                            if(jconnectors[j].insert(connector).second)
                                counts[connector].second += jw;
                        }
                    }
                }
            }
        }
    }


    if(!isuffixind.empty()) {
        for(int j = 0; j < jnum; ++j) {   // finds connectors when right side of iseq matches jseq    
            const string& jseq = jreads[j];
            int jw = jweights[j];
            for(int n = jseq.size()-MIN_OVERLAP+1; n >= 0; --n) {
                string pattern = jseq.substr(n,MIN_OVERLAP);
                pair<TSuffixIndex::const_iterator,TSuffixIndex::const_iterator> er = isuffixind.equal_range(pattern);  // matching isuffixes    
                for(TSuffixIndex::const_iterator it = er.first; it != er.second; ++it) {
                    int i = it->second;
                    const string& iseq = ireads[i];
                    int iw = iweights[i];

                    int imin = iseq.size()-MIN_OVERLAP;
                    int jmin = n;
                    while(imin > 0 && jmin > 0 && iseq[imin-1] == jseq[jmin-1]) {
                        --imin;
                        --jmin;
                    }
                    if(imin == 0 || jmin == 0) {   // left end of jseq matched or ALL iseq matched              
                        int imax = iseq.size()-1;
                        int jmax = n+MIN_OVERLAP-1;
                        if(jmax > (int)jseq.size()-wsize-1) {  // match overlaps with strong_word           
                            int del = jmax-(int)jseq.size()+wsize+1; 
                            imax -= del;
                            jmax -= del;
                        }
                        if(imax >= wsize-1) {      // didn't clip prev_strong_word  and were not connected before         
                            string connector = iseq.substr(wsize,imax-wsize+1)+jseq.substr(jmax+1,(int)jseq.size()-wsize-1-jmax);
                            if(iconnectors[i].insert(connector).second)
                                counts[connector].first += iw;
                            if(jconnectors[j].insert(connector).second)
                                counts[connector].second += jw;
                        }
                    }
                }
            }
        }
    }

    TSIMap seq_counts;
    int total_cross = 0;
    int accepted_cross = 0;
    for(map<string,pair<int,int> >::iterator it = counts.begin(); it != counts.end(); ++it) {
        const string& connector = it->first;
        int w = min(it->second.first,it->second.second);
        if(w >= MIN_CONNECTORS && EditDistance(base, connector) < (int)(max(base.size(),connector.size())*MAX_DIST+0.5)) {
            seq_counts[connector] += w;
            accepted_cross +=w;
        }
    }

    pair<string,bool> variant = SelectVariant(seq_counts, weak_range, total_cross, accepted_cross, chunk, pos, 0);
    variant.first = NStr::ToLower(variant.first);

    return variant;
}

TSignedSeqRange CMultAlign::LegitRange(int ir) {
    const string& read = m_reads[ir];
    int start = m_starts[ir];
    int end = start+read.size()-1;
    _ASSERT(m_align_to_contig[start] >= 0);
    _ASSERT(m_align_to_contig[end] >= 0);
    int first_legit_match = m_contig_to_align[m_align_to_contig[start]+m_min_edge];
    while(first_legit_match <= end && (read[first_legit_match-start] == '-' || read[first_legit_match-start] != m_base[first_legit_match]))
        ++first_legit_match;
    int last_legit_match = m_contig_to_align[m_align_to_contig[end]-m_min_edge];
    while(last_legit_match >= start && (read[last_legit_match-start] == '-' || read[last_legit_match-start] != m_base[last_legit_match]))
        --last_legit_match;

    return TSignedSeqRange(first_legit_match, last_legit_match);
}


#define MAX_POLYA_DIST 50
#define MIN_LC_INTERVAL 3
#define LC_FRACTION 0.3
#define MIN_SUPPORT_FOR_SIMPL_RUN 3
#define MIN_FRAC_FOR_SIMPL_RUN 0.8

void CMultAlign::AddSimpleRunToConsensus(int curp, int nextp, const TSignedSeqRange& polya_range, const TSignedSeqRange& polyt_range, list<string>& consensus, const string& maximal_bases) {

    string simple_run;
    for(int p = curp; p < nextp; ++p) {
        char c = maximal_bases[p];
        if(c != '-')
            simple_run.push_back(c);
    }
    int total = simple_run.size();

    bool all_supported = true;
    int left = -1;
    int right = total;
    int ll = -1;
    for(int p = curp; p < nextp; ++p) {
        map<char,int>& count = m_counts[p];
        int t = 0;
        for(map<char,int>::const_iterator i = count.begin(); i != count.end(); ++i)
            t += i->second;

        char c = maximal_bases[p];
        if(c != '-')
            ++ll;

        int m = count[toupper(c)];
        if(t < MIN_SUPPORT_FOR_SIMPL_RUN || m < int(MIN_FRAC_FOR_SIMPL_RUN*t+0.5)) {
            all_supported = false;
            right = ll+1;
        }

        if(all_supported)
            left = ll;
    }

    /*
    int lc = 0;
    for(int i = 0; i < total; ++i) {
        if(islower(simple_run[i])) {
            ++lc;
            if(left >= i)
                left = i-1;
            if(right <= i)
                right = i+1;
        }
    }

    if(all_supported && (total <= MIN_LC_INTERVAL || lc < LC_FRACTION*total)) {
    */
    if(all_supported) {
        cerr << "Simple run: " << simple_run << endl;
        consensus.back() += NStr::ToUpper(simple_run);
    } else {
        if(left >= 0) {
            string left_part = simple_run.substr(0,left+1);
            cerr << "Simple run: " << left_part << endl;
            consensus.back() += NStr::ToUpper(left_part);
        }

        string skipped_part = simple_run.substr(left+1,right-left-1);
        if(!all_supported) 
            cerr << "Low support run: " << skipped_part << endl;
        else
            cerr << "Low complexity run: " << skipped_part << endl;

        if(!consensus.back().empty())
            consensus.push_back(string()); 

        if(right < total) {
            string right_part = simple_run.substr(right);
            cerr << "Simple run: " << right_part << endl;
            consensus.back() += NStr::ToUpper(right_part);
        }
    }
}

list<string> CMultAlign::Consensus() {
    if(m_aligns.empty())
        return list<string>();

    PrepareStats(true);

    string maximal_bases(m_base.size(),'A');
    for(int p = 0; p < (int)m_base.size(); ++p) {
        const map<char,int>& count = m_counts[p];
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

    string simple_consensus;
    vector<int> sconsensus_to_align;
    for(int i = 0; i < (int)maximal_bases.size(); ++i) {
        char c = maximal_bases[i];
        if(c != '-') {
            simple_consensus.push_back(c);
            sconsensus_to_align.push_back(i);
        }
    }

    //mask low complexity

    /*
    for(int p = 0; p < (int)simple_consensus.size()-m_word; ++p) {
        string word = simple_consensus.substr(p,m_word);
        size_t match = simple_consensus.find(word,p+1);
        if(match != string::npos && (int)match < p+m_word) {
            for(int l = 0; l < m_word; ++l) {
                int pos = sconsensus_to_align[p+l];
                maximal_bases[pos] = tolower(maximal_bases[pos]);
                pos = sconsensus_to_align[match+l];
                maximal_bases[pos] = tolower(maximal_bases[pos]);
            }
        }
    }
    */

    //#define ENTROPY_LEVEL 0.75
    if((int)simple_consensus.size() >= m_entropy_window) {
        size_t reg_left = simple_consensus.find_first_not_of('#');
        while(reg_left <= simple_consensus.size()-m_entropy_window) {
            size_t reg_right = min(simple_consensus.size(),simple_consensus.find_first_of('#',reg_left+1));
            if(reg_right >= reg_left+m_entropy_window) {
                for(size_t l = reg_left+m_entropy_window/2; l < reg_right-m_entropy_window/2; ++l) {
                    double entropy = Entropy(simple_consensus.substr(l-m_entropy_window/2,m_entropy_window));
                    if(entropy < m_entropy_level) {
                        int pos = sconsensus_to_align[l];
                        maximal_bases[pos] = tolower(maximal_bases[pos]); 
                    }
                }
            }
            reg_left = simple_consensus.find_first_not_of('#',reg_right+1);
        }
    }


    /*
    //mask sharp transitions
#define SHARP_TRANSITION 10
#define TRANSITION_SIZE 15
#define SAMPLE 1
    if(simple_consensus.size() > 2*TRANSITION_SIZE) {
        vector<int> sconsensus_coverage(simple_consensus.size());
        for(int l = SAMPLE; l < (int)simple_consensus.size()-SAMPLE; ++l) {
            int cov = 0;
            for(int s = -SAMPLE; s <= SAMPLE; ++s) {
                int pos = sconsensus_to_align[l+s];
                cov += m_coverage[pos];
            }
            sconsensus_coverage[l] = cov;
        }
        for(int l = TRANSITION_SIZE; l < (int)sconsensus_coverage.size(); ++l) {
            if(SHARP_TRANSITION*sconsensus_coverage[l] < sconsensus_coverage[l-TRANSITION_SIZE]) {
                for(int s = 0; s < TRANSITION_SIZE && l+s < (int)sconsensus_coverage.size(); ++s) {
                    if(simple_consensus[l+s] != '#') {
                        int pos = sconsensus_to_align[l+s];
                        maximal_bases[pos] = tolower(maximal_bases[pos]); 
                    }
                }
            }
        }
        for(int l = 0; l < (int)sconsensus_coverage.size()-TRANSITION_SIZE; ++l) {
            if(SHARP_TRANSITION*sconsensus_coverage[l] < sconsensus_coverage[l+TRANSITION_SIZE]) {
                for(int s = 0; s < TRANSITION_SIZE && l-s >= 0; ++s) {
                    if(simple_consensus[l-s] != '#') {
                        int pos = sconsensus_to_align[l-s];
                        maximal_bases[pos] = tolower(maximal_bases[pos]); 
                    }
                }
            }
        }
    }
    */

    //clip to polyt
    TSignedSeqRange polyt_range;
    size_t first_base = simple_consensus.find_first_not_of('#');
    if(first_base != string::npos) {
        string polyt(MIN_POLYA,'T');
        size_t polyt_pos =  simple_consensus.find(polyt,first_base);
        if(polyt_pos != string::npos && polyt_pos > first_base && polyt_pos <  first_base+MAX_POLYA_DIST) {
            size_t first_after = simple_consensus.find_first_not_of('T',polyt_pos);
            while(first_after != string::npos && simple_consensus.substr(first_after+1,MIN_POLYA) == polyt) {
                first_after = simple_consensus.find_first_not_of('T',first_after+1);
            }
            if(first_after != string::npos) {
                int b = sconsensus_to_align[first_after]-1;
                if(simple_consensus[first_after] == '#') {
                    for(int p = 0; p <= b; ++p)
                        maximal_bases[p] = '#';
                    for(int p = 0; p < (int)first_after; ++p)
                        simple_consensus[p] = '#';
                } else {
                    int a = sconsensus_to_align[polyt_pos];

                    TSignedSeqRange strong_word_before_range;
                    string strong_word_before;
                    FindPrevStrongWord(a-1, maximal_bases, strong_word_before, strong_word_before_range);

                    TSignedSeqRange strong_word_after_range;
                    string strong_word_after;
                    int first_gap;
                    FindNextStrongWord(b+1, maximal_bases, strong_word_after, strong_word_after_range, first_gap);

                    if(first_gap >= 0 || strong_word_before_range.Empty() || strong_word_after_range.Empty() ||
                       !SupportedSeqBetweenTwoStrongWords(strong_word_before_range, strong_word_before, strong_word_after_range, strong_word_after, -1, -1).second) {

                        polyt_range.SetFrom(a);
                        polyt_range.SetTo(b);
                        for(int p = 0; p < a; ++p)
                            maximal_bases[p] = '#';
                        for(int p = 0; p < (int)polyt_pos; ++p)
                            simple_consensus[p] = '#';
                    }
                }
            }
        }
    }
    //clip to polya 
    TSignedSeqRange polya_range;
    size_t last_base = simple_consensus.find_last_not_of('#');
    if(last_base != string::npos) {
        string polya(MIN_POLYA,'A');
        size_t polya_pos = simple_consensus.rfind(polya);
        if(polya_pos != string::npos) {
            size_t polya_end = polya_pos+MIN_POLYA-1;
            if(polya_end < last_base && polya_end > last_base-MAX_POLYA_DIST) {
                size_t first_before = simple_consensus.find_last_not_of('A',polya_pos);
                while(first_before != string::npos && first_before >= MIN_POLYA && simple_consensus.substr(first_before-MIN_POLYA,MIN_POLYA) == polya) {
                    first_before = simple_consensus.find_last_not_of('A',first_before-MIN_POLYA);
                }
                if(first_before != string::npos) {
                    int a = sconsensus_to_align[first_before]+1;
                    if(simple_consensus[first_before] == '#') {
                        for(int p = a; p < (int)maximal_bases.size(); ++p)
                            maximal_bases[p] = '#';
                        for(int p = first_before+1; p < (int)simple_consensus.size(); ++p)
                            simple_consensus[p] = '#';
                    } else {
                        int b = sconsensus_to_align[polya_end];

                        TSignedSeqRange strong_word_before_range;
                        string strong_word_before;
                        FindPrevStrongWord(a-1, maximal_bases, strong_word_before, strong_word_before_range);

                        TSignedSeqRange strong_word_after_range;
                        string strong_word_after;
                        int first_gap;
                        FindNextStrongWord(b+1, maximal_bases, strong_word_after, strong_word_after_range, first_gap);

                        if(first_gap >= 0 || strong_word_before_range.Empty() || strong_word_after_range.Empty() ||
                           !SupportedSeqBetweenTwoStrongWords(strong_word_before_range, strong_word_before, strong_word_after_range, strong_word_after, -1, -1).second) {

                            polya_range.SetFrom(a);
                            polya_range.SetTo(b);
                            for(int p = b+1; p < (int)maximal_bases.size(); ++p)
                                maximal_bases[p] = '#';
                            for(int p = polya_end+1; p < (int)simple_consensus.size(); ++p)
                                simple_consensus[p] = '#';
                        }
                    }
                }
            }
        }
    }
        
    list<string> consensus;
    consensus.push_back(string());

    int prev_emitted_pos = -1;
    TSignedSeqRange prev_strong_word_range;
    for(int p = 0; p < (int)maximal_bases.size(); ) {
        if(maximal_bases[p] == '#') {
            if(consensus.empty() || !consensus.back().empty()) {
                consensus.push_back(string());
            }
            prev_emitted_pos = p;
            prev_strong_word_range = TSignedSeqRange::GetEmpty();
            ++p;
        } else {
            string strong_word;
            TSignedSeqRange strong_word_range;
            int first_gap;
            p = FindNextStrongWord(p, maximal_bases, strong_word, strong_word_range, first_gap);

            if(!strong_word_range.Empty()) {
                if(prev_emitted_pos+1 != strong_word_range.GetFrom()) {  // there is a weak range   
                    pair<string,bool> max_seq(string(),false);
                                
                    if(first_gap < 0) { // there is no gap
                        if(prev_strong_word_range.NotEmpty()) {    // weak consensus between two strong words
                            string prev_strong_word = consensus.back().substr(consensus.back().size()-m_word,m_word);
                            max_seq = SupportedSeqBetweenTwoStrongWords(prev_strong_word_range, prev_strong_word, strong_word_range, strong_word, consensus.size(), consensus.back().size());
                        } else if(consensus.back().empty() && polyt_range.NotEmpty() && strong_word_range.GetFrom() > polyt_range.GetTo()+1) {
                            max_seq = SupportedSeqBetweenPolytAndStrongWord(strong_word_range, strong_word, consensus.size(), consensus.back().size());
                        }
                    }

                    if(!max_seq.second && prev_strong_word_range.NotEmpty()) {  // there was a gap or no one-read connection
                        string prev_strong_word = consensus.back().substr(consensus.back().size()-m_word,m_word);
                        max_seq = CombinedSeqBetweenTwoStrongWords(prev_strong_word_range, prev_strong_word, strong_word_range, strong_word, consensus.size(), consensus.back().size());
                    }

                    if(max_seq.second || first_gap < 0) {
                        if(max_seq.second) {
                            consensus.back() += max_seq.first;
                        } else {                      
                            TSignedSeqRange weak_range(prev_emitted_pos+1,strong_word_range.GetFrom()-1);
                            AddSimpleRunToConsensus(weak_range.GetFrom(), weak_range.GetTo()+1, polya_range, polyt_range, consensus, maximal_bases);
                        }
                    } else {
                        strong_word_range = TSignedSeqRange::GetEmpty();
                    }
                }

                if(!strong_word_range.Empty()) {
                    //cerr << "Strong word: " << strong_word << " " << m_align_to_contig[strong_word_range.GetFrom()]+1 << " " << m_align_to_contig[strong_word_range.GetTo()]+1 << endl;
                    consensus.back() += strong_word;
                    prev_strong_word_range = strong_word_range;
                    prev_emitted_pos = strong_word_range.GetTo();
                }
            }
            
            if(strong_word_range.Empty()) {
                if(first_gap >= 0) p = first_gap;
                pair<string,bool> max_seq;
                if(prev_strong_word_range.NotEmpty() && polya_range.NotEmpty() && p == polya_range.GetTo()+1 && prev_emitted_pos < polya_range.GetFrom()) {
                    string prev_strong_word = consensus.back().substr(consensus.back().size()-m_word,m_word);
                    max_seq = SupportedSeqBetweenStrongWordAndPolya(prev_strong_word_range, prev_strong_word, consensus.size(), consensus.back().size());
                } 

                if(max_seq.second) {
                    consensus.back() += max_seq.first;
                } else {
                    AddSimpleRunToConsensus(prev_emitted_pos+1, p, polya_range, polyt_range, consensus, maximal_bases);
                }

                prev_strong_word_range = TSignedSeqRange::GetEmpty();
                prev_emitted_pos = p-1;
            }
        }
    }

    if(consensus.back().empty()) {
        consensus.pop_back();
    }

    return consensus;
}

void CMultAlign::Variations(map<TSignedSeqRange,TSIMap>& variations, list<TSignedSeqRange>& confirmed_ranges, bool use_limits) {

    if(m_aligns.empty())
        return;

    PrepareStats(use_limits);

    string maximal_bases(m_base.size(),'A');
    for(int p = 0; p < (int)m_base.size(); ++p) {
        const map<char,int>& count = m_counts[p];
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

    string simple_consensus;
    vector<int> sconsensus_to_align;
    for(int i = 0; i < (int)maximal_bases.size(); ++i) {
        char c = maximal_bases[i];
        if(c != '-') {
            simple_consensus.push_back(c);
            sconsensus_to_align.push_back(i);
        }
    }

    //mask low complexity
    if((int)simple_consensus.size() >= m_entropy_window) {
        size_t reg_left = simple_consensus.find_first_not_of('#');
        while(reg_left <= simple_consensus.size()-m_entropy_window) {
            size_t reg_right = min(simple_consensus.size(),simple_consensus.find_first_of('#',reg_left+1));
            if(reg_right >= reg_left+m_entropy_window) {
                for(size_t l = reg_left+m_entropy_window/2; l < reg_right-m_entropy_window/2; ++l) {
                    double entropy = Entropy(simple_consensus.substr(l-m_entropy_window/2,m_entropy_window));
                    if(entropy < m_entropy_level) {
                        int pos = sconsensus_to_align[l];
                        maximal_bases[pos] = tolower(maximal_bases[pos]); 
                    }
                }
            }
            reg_left = simple_consensus.find_first_not_of('#',reg_right+1);
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

            
            /*
            if(!strong_word_range.Empty()) {
                cout << m_align_to_contig[strong_word_range.GetFrom()] << " " <<  m_align_to_contig[strong_word_range.GetTo()] << " ";
                if(!prev_strong_word_range.Empty()) {
                    cout << m_align_to_contig[prev_strong_word_range.GetFrom()] << " " <<  m_align_to_contig[prev_strong_word_range.GetTo()] << " ";
                } else {
                    cout << "Empty";
                }
                cout << " ";
                for(int pos = strong_word_range.GetFrom(); pos <= strong_word_range.GetTo(); ++pos)
                    cout << maximal_bases[pos];
                cout << " ";
                for(int pos = strong_word_range.GetFrom(); pos <= strong_word_range.GetTo(); ++pos)
                    cout << m_base[pos];
                cout << " " << same_as_contig << " " << first_gap << endl;
            }
            */
           

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

int CMultAlign::FindPrevStrongWord(int prevp, const string& maximal_bases, string& strong_word, TSignedSeqRange& strong_word_range) {
    while(prevp >= 0) {
        string word;
        int word_start = prevp;
        int word_stop = prevp;
        bool low_complexity = false;
        for( ; word_start >= 0 && (int)word.size() < m_word && maximal_bases[word_start] != '#'; --word_start) {
            if(maximal_bases[word_start] != '-')
                word.insert(0,1,toupper(maximal_bases[word_start]));
            if(islower(maximal_bases[word_start]))
                low_complexity = true; 
        }

        if((int)word.size() < m_word)
            return word_start;

        TSignedSeqRange word_range(word_start+1,word_stop);
        if(!low_complexity && CheckWord(word_range,word)) {
            strong_word = word;
            strong_word_range = word_range;
            return word_start;
        }
        --prevp;
    }

    return prevp;
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
    arg_desc->AddDefaultKey("entropy_window", "entropy_window", "Window for entropy calculation", CArgDescriptions::eInteger, "11");
    arg_desc->AddDefaultKey("entropy", "entropy", "Entropy level for masking (0 - no masking)", CArgDescriptions::eDouble, "0");

}

void CMultAlign::ProcessArgs(const CArgs& args) {

    m_word = args["word"].AsInteger();
    m_min_edge = args["min_edge"].AsInteger();
    m_min_coverage = args["min_coverage"].AsInteger();   
    m_maxNs = args["maxNs"].AsInteger();
    m_min_rel_support_for_variant = args["min_rel_support"].AsDouble();
    m_min_abs_support_for_variant = args["min_abs_support"].AsInteger();
    m_strong_consensus = args["strong_consensus"].AsDouble();
    m_entropy_window = (args["entropy_window"].AsInteger()/2)*2+1;
    m_entropy_level = args["entropy"].AsDouble();
}

void CMultAlign::SetDefaultParams() {
    m_word = 8;
    m_min_edge = 5;
    m_min_coverage = 3;   
    m_maxNs = 2;
    m_min_rel_support_for_variant = 0.075;
    m_min_abs_support_for_variant = 3;
    m_strong_consensus = 0.85;
    m_entropy_window = 11;
    m_entropy_level = 0;
}


END_SCOPE(gnomon)
END_NCBI_SCOPE
