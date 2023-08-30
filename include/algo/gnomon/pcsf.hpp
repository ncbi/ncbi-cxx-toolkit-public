#ifndef PCSF__HPP
#define PCSF__HPP

/* 
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
 * Authors:  Alexandre Souvorov
 *
 * File Description:
 *
 */

#include <algo/gnomon/gnomon_model.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

typedef float TPhyloCSFScore;
typedef vector<TPhyloCSFScore> TFVec;
struct SPhyloCSFCompactScore {
    typedef pair<TSignedSeqPos, TPhyloCSFScore> TElement;
    void Read(CNcbiIstream& from, size_t len) {
        m_scores.resize(len);
        if(!from.read(reinterpret_cast<char*>(m_scores.data()), len*sizeof(TElement))) {
            cerr << "Error in PhyloCSF compact read\n";
            exit(1);
        }
    }
    void Write(CNcbiOstream& out) const {
        if(!out.write(reinterpret_cast<const char*>(m_scores.data()), Size()*sizeof(TElement))) {
            cerr << "Error in PhyloCSF compact write\n";
            exit(1);
        }                
    }
    TPhyloCSFScore Score(TSignedSeqPos p) const {
        auto rslt = lower_bound(m_scores.begin(), m_scores.end(), p, [](const TElement& e, TSignedSeqPos i) { return e.first < i; });
        if(rslt == m_scores.end() || rslt->first != p)
            return 0;
        else
            return rslt->second;
    }
    size_t LowerBound(TSignedSeqPos p) const { return lower_bound(m_scores.begin(), m_scores.end(), p, [](const TElement& e, TSignedSeqPos i) { return e.first < i; })-m_scores.begin(); }
    size_t UpperBound(TSignedSeqPos p) const { return upper_bound(m_scores.begin(), m_scores.end(), p, [](TSignedSeqPos i, const TElement& e) { return i < e.first; })-m_scores.begin(); }
    size_t Size() const { return m_scores.size(); }

    vector<TElement> m_scores;
};

struct SPhyloCSFSlice {
    TPhyloCSFScore Score(int s, TSignedSeqPos codon_left) const {
        if(m_map != nullptr) {
            codon_left = m_map->MapEditedToOrig(codon_left);
            if(codon_left < 0)
                return 0;
        }
        return (*m_scoresp)[s].Score(codon_left+m_shift); 
    }
    TSignedSeqRange CompactRange(int s, TSignedSeqRange edited_range) const {    // returns range of compact indices included in edited_range
        TSignedSeqRange orig_range = edited_range;
        if(m_map != nullptr) {
            edited_range = m_map->ShrinkToRealPointsOnEdited(edited_range);
            if(edited_range.Empty())
                return TSignedSeqRange::GetEmpty();
            orig_range = m_map->MapRangeEditedToOrig(edited_range, false);
        }
        if(orig_range.Empty())
            return TSignedSeqRange::GetEmpty();
        size_t left = (*m_scoresp)[s].LowerBound(orig_range.GetFrom()+m_shift);  // first >= position
        if(left == (*m_scoresp)[s].Size())
            return TSignedSeqRange::GetEmpty();
        // result is not empty
        size_t right = (*m_scoresp)[s].UpperBound(orig_range.GetTo()+m_shift)-1; // last <= position

        return TSignedSeqRange((TSignedSeqPos)left, (TSignedSeqPos)right);
    }

    const array<SPhyloCSFCompactScore, 2>* m_scoresp = nullptr;
    const CAlignMap* m_map = nullptr;
    double m_factor = 0;
    TSignedSeqPos m_shift = 0;   
};

class CPhyloCSFData {
public:
    void Read(CNcbiIstream& from) {
        m_contig_scores.clear();
        //read index
        map<string, tuple<size_t, TSignedSeqPos, TSignedSeqPos>> index; // shift in file, number of + elements, number of - elements
        size_t data_length;
        if(!from.read(reinterpret_cast<char*>(&data_length), sizeof data_length)) {
            cerr << "Error in PhyloCSF read\n";
            exit(1);
        }

        from.seekg(data_length, ios_base::cur); // skip to index
        int slen;
        while(from.read(reinterpret_cast<char*>(&slen), sizeof slen)) { // acc string size
            vector<char> buf(slen);
            from.read(buf.data(), slen);
            string contig_acc(buf.begin(), buf.end());
            size_t shift;
            from.read(reinterpret_cast<char*>(&shift), sizeof(size_t));      // data shift in file
            TSignedSeqPos plus_len;
            from.read(reinterpret_cast<char*>(&plus_len), sizeof(TSignedSeqPos));  // + strand number of elements
            TSignedSeqPos minus_len;
            from.read(reinterpret_cast<char*>(&minus_len), sizeof(TSignedSeqPos)); // - strand number of elements
            if(!from) {
                cerr << "Error in PhyloCSF index read\n";
                exit(1);
            }
            index[contig_acc] = make_tuple(shift, plus_len, minus_len);
        }

        //read data
        from.clear();
        for(auto& ind : index) {
            auto& contig_acc = ind.first;
            auto shift = get<0>(ind.second); 
            auto plus_len = get<1>(ind.second);
            auto minus_len = get<2>(ind.second); 
            from.seekg(shift,ios_base::beg);
            m_contig_scores[contig_acc][0].Read(from, plus_len);
            m_contig_scores[contig_acc][1].Read(from, minus_len);
        }
    }
    void Write(CNcbiOstream& out) const {
        size_t data_length = 0;
        for(auto& scr : m_contig_scores)
            data_length += (scr.second[0].Size()+scr.second[1].Size());
        data_length *= sizeof(SPhyloCSFCompactScore::TElement);
        //write data length 
        out.write(reinterpret_cast<const char*>(&data_length), sizeof data_length);         
        //write scores  
        for(auto& scr : m_contig_scores) {
            for(int strand = 0; strand < 2; ++strand)
                scr.second[strand].Write(out);
        }
        //write index
        size_t shift = sizeof data_length;
        for(auto& scr : m_contig_scores) {
            auto& contig_acc = scr.first;
            //write contig name as length+string    
            int slen = (int)contig_acc.size();
            out.write(reinterpret_cast<const char*>(&slen), sizeof slen);
            out.write(contig_acc.c_str(), slen);
            //write shift    
            out.write(reinterpret_cast<const char*>(&shift), sizeof shift);
            for(int strand = 0; strand < 2; ++strand) {
                TSignedSeqPos len = (TSignedSeqPos)scr.second[strand].Size();
                out.write(reinterpret_cast<const char*>(&len), sizeof len);
                shift += len*sizeof(SPhyloCSFCompactScore::TElement);
            }
        }
        if(!out) {
            cerr << "Error in PhyloCSF write\n";
            exit(1);
        }
    }
    SPhyloCSFSlice* CreateSliceForContig(const string& contig_acc) const {
        SPhyloCSFSlice* p = nullptr;
        auto rslt = m_contig_scores.find(contig_acc);
        if(rslt != m_contig_scores.end()) {
            p = new SPhyloCSFSlice;
            p->m_scoresp = &rslt->second;
        }
        return p;
    }
    void CompactFullScores(const map<string, array<TFVec, 2>>& scores) {
        m_contig_scores.clear();
        for(auto& cs : scores) {
            auto& contig_acc = cs.first;
            for(int strand = 0; strand < 2; ++strand) {
                for(TSignedSeqPos i = 0; i < (TSignedSeqPos)cs.second[strand].size(); ++i) {
                    auto scr = cs.second[strand][i];
                    if(scr > 0)
                        m_contig_scores[contig_acc][strand].m_scores.emplace_back(i, scr);
                }
            }
        }
    }

private:
    map<string, array<SPhyloCSFCompactScore, 2>> m_contig_scores;
};

END_SCOPE(gnomon)
END_NCBI_SCOPE

#endif  // PCSF__HPP
