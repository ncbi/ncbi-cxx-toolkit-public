/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information *
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
 * Test application for selecting exon chains in SAM alignments
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <math.h>

USING_NCBI_SCOPE;

class CExonSelectorApplication : public CNcbiApplication
{
public:
    virtual void Init();
    virtual int  Run();

    typedef vector<string>          TSamFields;
    typedef list<TSamFields>        TSplitAligns;
    typedef list<pair<int, string>> TMDTag;
    typedef list<pair<int, char>>   TCigar;
    typedef vector<char>            TScript;
    template<typename T>
    using TMatrix = array<array<T, 2>, 2>;

    enum ExonType {eExtension, eNewAlignment, eNewCompartment, eBogus };
    struct Exon {
        int     m_qfrom = 0;
        int     m_qto = 0;
        int     m_sfrom = 0;
        int     m_sto = 0;
        int     m_matches = 0;
        int     m_mismatches = 0;
        int     m_indels = 0;
        int     m_align_len = 0;
        int     m_score = 0;

        string  m_tstrand;
        TCigar  m_cigar;
        TMDTag  m_md; 
        TScript m_script;

        void RemoveLeftIndels(int gapopen1, int gapextend1, int gapopen2, int gapextend2) {
            while(!m_script.empty() && (m_script.front() == 'D' || m_script.front() == 'I'))
                m_script.erase(m_script.begin());
            m_align_len = m_script.size();
            while(!m_md.empty() && m_md.front().first < 0)
                m_md.pop_front();
            while(!m_cigar.empty() && (m_cigar.front().second == 'D' || m_cigar.front().second == 'I')) {
                --m_indels;
                int len = m_cigar.front().first;
                m_score += min(gapopen1+gapextend1*len, gapopen2+gapextend2*len);
                if(m_cigar.front().second == 'D')
                    m_sfrom += len;
                else
                    m_qfrom += len;
                m_cigar.pop_front();
            }
        }
        void RemoveRightIndels(int gapopen1, int gapextend1, int gapopen2, int gapextend2) {
            while(!m_script.empty() && (m_script.back() == 'D' || m_script.back() == 'I'))
                m_script.pop_back();
            m_align_len = m_script.size();
            while(!m_md.empty() && m_md.back().first < 0)
                m_md.pop_back();
            while(!m_cigar.empty() && (m_cigar.back().second == 'D' || m_cigar.back().second == 'I')) {
                --m_indels;
                int len = m_cigar.back().first;
                m_score += min(gapopen1+gapextend1*len, gapopen2+gapextend2*len);
                if(m_cigar.back().second == 'D')
                    m_sto -= len;
                else
                    m_qto -= len;
                m_cigar.pop_back();
            }
        }
        
        ExonType m_type = eExtension;
        int      m_chain_score = 0;
        int      m_intron_len = 0;
        Exon*    m_prevp = nullptr;
    };
    typedef vector<Exon> TExons;

    struct AlignCompartment {
        TExons            m_exons;
        string            m_read;
        string            m_rseq;
        int               m_score = 0;
        pair<int, int>    m_range;
        string            m_cigar;
        string            m_md;
        string            m_tstrand;
        int               m_dist = 0;

        bool              m_above_thresholds = false;
        bool              m_paired_read = false;
        AlignCompartment* m_matep = nullptr;  // if other mate was found
    };
    typedef list<AlignCompartment> TAlignCompartments;

    Exon GetNextExon(string& cigar_string, string& MD_tag, int qfrom, int sfrom);
    void ExtractExonsFromSAM(const TSamFields& tags, TExons& exons);
    void ChainExons();
    void ConnectPairs();
    bool CompatiblePair(const AlignCompartment& left, const AlignCompartment& right);
    void SelectBestLocations();
    void FormatResults();
    void FormatSingle(AlignCompartment& compart, const string& contig, int mate, int strand, int count, int index);
    void FormatPaired(AlignCompartment& compart, const string& contig, int mate, int strand, int count, int index, bool left);

    int      m_matchscore = 1;
    int      m_mismatchpenalty = 2;
    int      m_gapopen1 = 2;
    int      m_gapopen2 = 32;
    int      m_gapextend1 = 1;
    int      m_gapextend2 = 0;

    double   m_min_output_idty;
    double   m_min_output_cov;
    double   m_penalty;
    int      m_min_query_len;
    int      m_max_intron;

    double   m_min_entropy = 0.65;

    bool     m_remove_repats = false;
    bool     m_read_through = false;
    map<string, TMatrix<TSplitAligns>> m_read_aligns; // [contig][strand] list of SAM fields
    map<string, TMatrix<TAlignCompartments>> m_compartments;
};

CExonSelectorApplication::Exon CExonSelectorApplication::GetNextExon(string& cigar_string, string& MD_tag, int qfrom, int sfrom) {
    Exon e;
    e.m_qfrom = qfrom;
    e.m_sfrom = sfrom;
    e.m_qto = e.m_qfrom-1;
    e.m_sto = e.m_sfrom-1;
    while(!cigar_string.empty()) {
        size_t letter;
        int len = stoi(cigar_string, &letter);
        char c = cigar_string[letter];

        // capture exon portion of cigar
        if(c != 'S' && c != 'N')
            e.m_cigar.emplace_back(len, c);
        // clip used cigar; keep intron for next exon
        if(c != 'N' || e.m_align_len == 0)
            cigar_string.erase(0, letter+1);

        if(c == 'S') {
            if(e.m_align_len == 0) { //new query start
                e.m_qfrom += len;
                e.m_qto = e.m_qfrom-1;
            }
        } else if(c == 'N') {
            if(e.m_align_len == 0) { // new subject start
                e.m_sfrom += len;
                e.m_sto = e.m_sfrom-1;
            } else {                 // end of exon
                e.m_score += e.m_matches*m_matchscore-e.m_mismatches*m_mismatchpenalty;
                return e;
            }
        } else if(c == 'M' || c == '=' || c =='X') {
            e.m_align_len += len;
            e.m_qto += len;
            e.m_sto += len;

            while(len > 0) {
                if(isdigit(MD_tag.front())) {
                    size_t idx;
                    int m = stoi(MD_tag, &idx);
                    if(e.m_md.empty() || e.m_md.back().first <= 0)
                        e.m_md.emplace_back(min(m,len), "");
                    else
                        e.m_md.back().first += min(m,len);
                    MD_tag.erase(0, idx);                 // remove matches
                    if(m > len) {                         // matches split between exons
                        MD_tag = to_string(m-len)+MD_tag; // return remaining matches
                        e.m_matches += len;
                        e.m_script.insert(e.m_script.end(), len, '=');
                        len = 0;
                    } else {
                        e.m_matches += m;
                        len -= m;
                        e.m_script.insert(e.m_script.end(), m, '=');
                    }
                } else {
                    if(e.m_md.empty() || e.m_md.back().first != 0)
                        e.m_md.emplace_back(0, MD_tag.substr(0, 1));
                    else
                        e.m_md.back().second += MD_tag.front();
                    ++e.m_mismatches;
                    e.m_script.push_back('X');
                    --len;
                    MD_tag.erase(0, 1);
                }
            }
        } else if(c == 'I') {
            e.m_align_len += len;
            e.m_script.insert(e.m_script.end(), len, 'I');
            e.m_qto += len;
            ++e.m_indels;
            e.m_score -= min(m_gapopen1+m_gapextend1*len, m_gapopen2+m_gapextend2*len);
        } else if(c == 'D') {
            e.m_align_len += len;
            e.m_script.insert(e.m_script.end(), len, 'D');
            e.m_sto += len;
            ++e.m_indels;
            string del;
            if(MD_tag.front() == '^') {
                del = MD_tag.substr(1, len);
                MD_tag.erase(0, len+1);
            } else {                         // in case deletion was split between exons
                del = MD_tag.substr(0, len);
                MD_tag.erase(0, len);
            }
            if(e.m_md.empty() || e.m_md.back().first >= 0)
                e.m_md.emplace_back(-1, del);
            else
                e.m_md.back().second += del;
            e.m_score -= min(m_gapopen1+m_gapextend1*len, m_gapopen2+m_gapextend2*len);
        } else {
            cerr << "Unexpected symbol in CIGAR" << endl;
            exit(1);
        }
    }
    e.m_score += e.m_matches*m_matchscore-e.m_mismatches*m_mismatchpenalty;
    return e;
}

double Entropy(const string& seq) {
    int length = seq.size();
    if(length == 0)
        return 0;
    double tA = 1.e-8;
    double tC = 1.e-8;
    double tG = 1.e-8;
    double tT = 1.e-8;
    ITERATE(string, i, seq) {
        switch(*i) {
        case 'A': tA += 1; break;
        case 'C': tC += 1; break;
        case 'G': tG += 1; break;
        case 'T': tT += 1; break;
        default: break;
        }
    }
    double entropy = -(tA*log(tA/length)+tC*log(tC/length)+tG*log(tG/length)+tT*log(tT/length))/(length*log(4.));
    
    return entropy;
}

void CExonSelectorApplication::ExtractExonsFromSAM(const TSamFields& tags, TExons& exons) {
    TExons align_exons;
    string MD_tag;
    string tstrand;
    for(int i = 11; i < (int)tags.size() && (MD_tag.empty() || tstrand.empty()); ++i) {
        auto& tag = tags[i]; 
        vector<string> fields;
        NStr::Split(tag, ":", fields, 0);
        if(fields.size() != 3)
            continue;
        if(fields[0] == "MD" && fields[1] == "Z")
            MD_tag = fields[2];
        if((fields[0] == "ts" || fields[0] == "TS" || fields[0] == "XS") && fields[1] == "A")
            tstrand = tag;
    }
    if(MD_tag.empty()) {
        cerr << "MD:Z tag in SAM file is expected" << endl;
        exit(1);
    }
                
    // extract exons    
    string cigar_string = tags[5];
    int qfrom = 0;
    int sfrom = std::stoi(tags[3])-1;
    while(!cigar_string.empty()) {
        align_exons.push_back(GetNextExon(cigar_string, MD_tag, qfrom, sfrom));
        align_exons.back().m_tstrand = tstrand;
        qfrom = align_exons.back().m_qto+1;
        sfrom = align_exons.back().m_sto+1;
    }

    // remove low complexity exons
    const string& seq = tags[9];
    while(!align_exons.empty() && Entropy(seq.substr(align_exons.front().m_qfrom, align_exons.front().m_qto-align_exons.front().m_qfrom+1)) < m_min_entropy)
        align_exons.erase(align_exons.begin());
    while(!align_exons.empty() && Entropy(seq.substr(align_exons.back().m_qfrom, align_exons.back().m_qto-align_exons.back().m_qfrom+1)) < m_min_entropy)
        align_exons.pop_back();

    exons.insert(exons.end(), align_exons.begin(), align_exons.end());
}

string Tag(const string& name, int value) {
    return name+":i:"+to_string(value);
}
string Tag(const string& name, const string& value) {
    return name+":Z:"+value;
}
string Tag(const string& name, char value) {
    return name+":A:"+value;
}

void CExonSelectorApplication::FormatPaired(AlignCompartment& compart, const string& contig, int mate, int strand, int count, int index, bool left) {
    TSamFields fields(11);
    fields[0] = compart.m_read;
    int flag = 1+2+strand*16+(1-strand)*32+(mate+1)*64;
    fields[1] = to_string(flag);
    fields[2] = contig;
    fields[3] = to_string(compart.m_range.first+1);
    fields[4] = "0";
    fields[5] = compart.m_cigar;
    fields[6] = "=";
    fields[7] = to_string(compart.m_matep->m_range.first+1);
    int span = max(compart.m_range.second, compart.m_matep->m_range.second)-min(compart.m_range.first, compart.m_matep->m_range.first)+1;
    if(!left)
        span = -span;
    fields[8] = to_string(span);
    fields[9] = compart.m_rseq;
    fields[10] = "*";
    fields.push_back(Tag("NM", compart.m_dist));
    fields.push_back(Tag("AS", compart.m_score+compart.m_matep->m_score));
    if(!compart.m_tstrand.empty())
        fields.push_back(compart.m_tstrand);
    fields.push_back(Tag("MD", compart.m_md));
    fields.push_back(Tag("NH", count));
    fields.push_back(Tag("HI", index));
    fields.push_back(Tag("MC", compart.m_matep->m_cigar));

    cout << fields[0];
    for(unsigned i = 1; i < fields.size(); ++i)
        cout << "\t" << fields[i];
    cout << endl;        
}

void CExonSelectorApplication::FormatSingle(AlignCompartment& compart, const string& contig, int mate, int strand, int count, int index) {
    TSamFields fields(11);
    fields[0] = compart.m_read;
    int flag = strand*16;
    if(compart.m_paired_read)                       // paired ends (mates not connected)
        flag += 1+8+(mate+1)*64;                    // if mates not connected the other mate reflected as 'not aligned' regrdless
    fields[1] = to_string(flag);
    fields[2] = contig;
    fields[3] = to_string(compart.m_range.first+1);
    fields[4] = "0";
    fields[5] = compart.m_cigar;
    fields[6] = "*";
    fields[7] = "0";
    fields[8] = "0";
    fields[9] = compart.m_rseq;
    fields[10] = "*";

    fields.push_back(Tag("NM", compart.m_dist));
    fields.push_back(Tag("AS", compart.m_score));
    if(!compart.m_tstrand.empty())
        fields.push_back(compart.m_tstrand);
    fields.push_back(Tag("MD", compart.m_md));
    fields.push_back(Tag("NH", count));
    fields.push_back(Tag("HI", index));

    cout << fields[0];
    for(unsigned i = 1; i < fields.size(); ++i)
        cout << "\t" << fields[i];
    cout << endl;        
}

void CExonSelectorApplication::FormatResults() {
    bool paired = false;                           // has mates, and they are connected
    array<int, 2> mate_count = {0, 0};
    for(auto& contig_aligns : m_compartments) {
        for(int mate = 0; mate < 2; ++mate) {
            for(int strand = 0; strand < 2; ++strand) {
                TAlignCompartments& compact_aligns = m_compartments[contig_aligns.first][mate][strand];
                for(auto& compart : compact_aligns) {
                    ++mate_count[mate];
                    if(compart.m_matep != nullptr)
                        paired = true;
                }
            }
        }
    }

    if(paired) {
        int index = 0;
        for(auto& contig_aligns : m_compartments) {
            auto& contig = contig_aligns.first;
            for(int mate = 0; mate < 2; ++mate) {
                int strand = 0;
                auto& left_aligns = contig_aligns.second[mate][strand];
                for(auto& compart :  left_aligns) {
                    FormatPaired(compart, contig, mate, strand, mate_count[0], ++index, true);
                    FormatPaired(*compart.m_matep, contig, 1-mate, 1-strand, mate_count[0], index, false);
                }
            }
        }
    } else {
        array<int, 2> mate_index = {0, 0};
        for(auto& contig_aligns : m_compartments) {
            auto& contig = contig_aligns.first;
            for(int mate = 0; mate < 2; ++mate) {
                for(int strand = 0; strand < 2; ++strand) {
                    TAlignCompartments& compact_aligns = m_compartments[contig_aligns.first][mate][strand];
                    for(auto& compart : compact_aligns)
                        FormatSingle(compart, contig, mate, strand, mate_count[mate], ++mate_index[mate]);
                }
            }
        }
    }
}

void CExonSelectorApplication::SelectBestLocations() {
    int pair_score = numeric_limits<int>::min();
    array<int, 2> mate_score = {numeric_limits<int>::min(), numeric_limits<int>::min()};
    for(auto& contig_aligns : m_compartments) {
        for(int mate = 0; mate < 2; ++mate) {
            for(int strand = 0; strand < 2; ++strand) {
                TAlignCompartments& compact_aligns = contig_aligns.second[mate][strand];
                for(auto& align : compact_aligns) {
                    mate_score[mate] = max(mate_score[mate], align.m_score);
                    if(align.m_matep != nullptr)
                        pair_score = max(pair_score, align.m_score+align.m_matep->m_score);
                }
                compact_aligns.remove_if([](const AlignCompartment& a){ return !a.m_above_thresholds; }); // remove after best score found; connected mates are above thresholds and will not be broken
            }
        }
    }

    bool paired = pair_score >= max(mate_score[0], mate_score[1]);
    for(auto& contig_aligns : m_compartments) {
        for(int mate = 0; mate < 2; ++mate) {
            for(int strand = 0; strand < 2; ++strand) {
                TAlignCompartments& compact_aligns = contig_aligns.second[mate][strand];
                if(paired)
                    compact_aligns.remove_if([&](const AlignCompartment& a){ 
                                                 if(a.m_matep == nullptr) return true;
                                                 if(a.m_score+a.m_matep->m_score == pair_score) return false;
                                                 a.m_matep->m_matep = nullptr; return true; });
                else
                    compact_aligns.remove_if([&](AlignCompartment& a){ a.m_matep = nullptr; return a.m_score != mate_score[mate]; }); 
            }
        }
    }
}

bool CExonSelectorApplication::CompatiblePair(const AlignCompartment& left, const AlignCompartment& right) {
    if(!left.m_above_thresholds || !right.m_above_thresholds)
        return false;
    if(right.m_range.first < left.m_range.first)
        return false;      // sticks out on the left
    int gap = right.m_range.first-left.m_range.second-1;
    if(gap > m_max_intron) // too far
        return false;
    if(gap >= 0)           // no overlap
        return true;

    // check if introns same in overlapping region
    vector<pair<int,int>> left_introns;
    vector<pair<int,int>> right_introns;
    // a small not spliced portion is allowed
    for(unsigned i = 1; i < left.m_exons.size(); ++i) {
        if(left.m_exons[i].m_sfrom > right.m_range.first+5)
            left_introns.emplace_back(left.m_exons[i-1].m_sto+1, left.m_exons[i].m_sfrom-1);
    }
    for(unsigned i = 1; i < right.m_exons.size(); ++i) {
        if(right.m_exons[i-1].m_sto < left.m_range.second-5)
            right_introns.emplace_back(right.m_exons[i-1].m_sto+1, right.m_exons[i].m_sfrom-1);
    }
    return left_introns == right_introns;
}

void CExonSelectorApplication::ConnectPairs() {
    for(auto& contig_aligns : m_compartments) {
        for(int left_mate = 0; left_mate < 2; ++left_mate) {
            int right_mate = 1-left_mate;
            int left_strand = 0;
            int right_strand = 1;
            auto& left_aligns =  contig_aligns.second[left_mate][left_strand];
            auto& right_aligns =  contig_aligns.second[right_mate][right_strand];

            auto iright = right_aligns.begin();
            for(auto ileft = left_aligns.begin(); ileft != left_aligns.end() && iright != right_aligns.end(); ++ileft) {
                while(iright != right_aligns.end() && iright->m_range.second < ileft->m_range.second)
                    ++iright;
                if(iright == right_aligns.end())
                    break;
                auto inext = next(ileft);
                if(inext != left_aligns.end() && inext->m_range.second < iright->m_range.second)
                    continue; 
                
                // connect compatible mates
                if(CompatiblePair(*ileft, *iright)) {
                    ileft->m_matep = &(*iright);
                    iright->m_matep = &(*ileft);
                }
                ++iright;
            }
        }
    }
}


void CExonSelectorApplication::ChainExons() {
    m_read_through = false;
    for(auto& contig_aligns : m_read_aligns) {
        auto& contig = contig_aligns.first;
        for(int mate = 0; mate < 2; ++mate) {
            for(int strand = 0; strand < 2; ++strand) {
                TSplitAligns& orig_aligns = contig_aligns.second[mate][strand];
                if(orig_aligns.empty())
                    continue;
                string& read = orig_aligns.front()[0];
                bool paired_read = stoi(orig_aligns.front()[1])&1;
                string& rseq = orig_aligns.front()[9];
                int read_len = rseq.size();

                // find exons       
                TExons exons;
                for(auto& oa : orig_aligns)
                    ExtractExonsFromSAM(oa, exons);

                if(exons.empty())
                   continue;

                sort(exons.begin(), exons.end(), [](const Exon& e1, const Exon& e2){ 
                         if(e1.m_sto != e2.m_sto)
                             return e1.m_sto > e2.m_sto;
                         if(e1.m_sfrom != e2.m_sfrom)
                             return e1.m_sfrom < e2.m_sfrom;
                         if(e1.m_qto != e2.m_qto)
                             return e1.m_qto > e2.m_qto;
                         if(e1.m_qfrom != e2.m_qfrom)
                             return e1.m_qfrom < e2.m_qfrom;
                         if(e1.m_score != e2.m_score)
                             return e1.m_score > e2.m_score;
                         return e1.m_cigar < e2.m_cigar; });

                // remove identical exons
                auto tail = unique(exons.begin(), exons.end(), [](const Exon& e1, const Exon& e2){ 
                                       return e1.m_sfrom == e2.m_sfrom && e1.m_sto == e2.m_sto && e1.m_qfrom == e2.m_qfrom && e1.m_qto == e2.m_qto; });
                exons.erase(tail, exons.end());

                // remove shorter exons with the same right end
                for(auto i = exons.begin(); i != exons.end(); ++i) {
                    if(i->m_type == eBogus || i->m_qfrom > 10)
                        continue;
                    for(auto j = next(i); j != exons.end() && j->m_sto == i->m_sto && j->m_qto == i->m_qto; ++j)
                        j->m_type = eBogus;
                }
                exons.erase(remove_if(exons.begin(), exons.end(), [](Exon& e){ return e.m_type == eBogus; }), exons.end());
                
                sort(exons.begin(), exons.end(), [](const Exon& e1, const Exon& e2){ 
                         if(e1.m_sfrom != e2.m_sfrom)
                             return e1.m_sfrom < e2.m_sfrom;
                         if(e1.m_sto != e2.m_sto)
                             return e1.m_sto > e2.m_sto;
                         if(e1.m_qfrom != e2.m_qfrom)
                             return e1.m_qfrom < e2.m_qfrom;
                         return e1.m_qto > e2.m_qto; });

                // remove shorter exons with the same left end
                for(auto i = exons.begin(); i != exons.end(); ++i) {
                    if(i->m_type == eBogus || read_len-i->m_qto-1 > 10)
                        continue;
                    for(auto j = next(i); j != exons.end() && j->m_sfrom == i->m_sfrom && j->m_qfrom == i->m_qfrom; ++j)
                        j->m_type = eBogus;
                }
                exons.erase(remove_if(exons.begin(), exons.end(), [](Exon& e){ return e.m_type == eBogus; }), exons.end());
                
                // find repeats
                if(m_remove_repats) {
                    for(auto i = exons.begin(); i != exons.end() && !m_read_through; ++i) {
                        for(auto j = next(i); j != exons.end() && j->m_sfrom <= i->m_sto && !m_read_through; ++j) {
                            int soverlap = i->m_sto-j->m_sfrom+1;
                            int slen = min(i->m_sto-i->m_sfrom+1, j->m_sto-j->m_sfrom+1);
                            int qoverlap = i->m_qto-j->m_qfrom+1;
                            int qlen = min(i->m_qto-i->m_qfrom+1, j->m_qto-j->m_qfrom+1);
                            if(soverlap > 0.5*slen && qoverlap < 0.1*qlen)
                                m_read_through = true;
                        }
                    }                
                }
                                                
                int matches = 0;
                int align_len = 0;
                int qmin = numeric_limits<int>::max();
                int qmax = 0;
                for(auto& e : exons) {
                    matches += e.m_matches;
                    align_len += e.m_align_len;
                    qmin = min(qmin, e.m_qfrom);
                    qmax = max(qmax, e.m_qto);
                }
                double ident = double(matches)/align_len;
                double cov = double(qmax-qmin+1)/read_len;

                double log2factor = 0.25;
                int compart_penalty = ident*cov*m_penalty*read_len + 0.5;
                int exon_num = exons.size();
                int best_index = exon_num-1;
                for(int i = exon_num-1; i >= 0; --i) {
                    auto& ei = exons[i];
                    int iscore = ei.m_matches;

                    ei.m_chain_score = iscore;                                            // start of everything 
                    ei.m_type = eNewAlignment;
                    for(int j = i+1; j < exon_num; ++j) {
                        auto& ej = exons[j];
                        int ijscore = ej.m_chain_score+iscore;

                        //                        if(ej.m_sfrom > ei.m_sto+m_max_intron)                            // too far    
                        //                            break;
                        if(ej.m_sfrom <= ei.m_sto)                                        // wrong genome order 
                            continue;

                        int overlap = ei.m_qto+1-ej.m_qfrom;
                        int intron = ej.m_sfrom-ei.m_sto-1;
                        if(overlap == 0 && intron > 20 && intron <= m_max_intron) {        // intron within range
                            int score = ijscore;
                            double intron_len = ej.m_intron_len+intron;
                            double deltas = score-ei.m_chain_score-log2factor*log2((intron_len+1)/(ei.m_intron_len+1));
                            if(deltas > 0) {
                                ei.m_chain_score = score;
                                ei.m_prevp = &ej;
                                ei.m_type = eExtension;
                                ei.m_intron_len = intron_len;
                            }
                        } else if(overlap <= 0 && intron <= m_max_intron) {                // no transcript overlap 
                            int overlap_penalty = 0;
                            overlap_penalty = max(2, int(ident*abs(overlap)+0.5));
                            int score = ijscore-overlap_penalty;
                            double intron_len = ej.m_intron_len;
                            double deltas = score-ei.m_chain_score-log2factor*log2((intron_len+1)/(ei.m_intron_len+1));
                            if(deltas > 0) {
                                ei.m_chain_score = score;
                                ei.m_prevp = &ej;
                                ei.m_type = eNewAlignment;
                                ei.m_intron_len = intron_len+intron;
                            }
                        } else if(overlap > 0 && overlap < 10 && intron <= m_max_intron) {  // small portion of transcript used twice 
                            int overlap_penalty = 0;
                            int s = ei.m_script.size()-1;
                            int l = overlap;
                            while(s >= 0 && l > 0) {
                                if(ei.m_script[s] == '=')
                                    ++overlap_penalty;
                                if(ei.m_script[s] != 'D')
                                    --l;
                                --s;
                            }
                            s = 0;
                            l = overlap;
                            while(s < (int)ej.m_script.size() && l > 0) {
                                if(ej.m_script[s] == '=')
                                    ++overlap_penalty;
                                if(ej.m_script[s] != 'D')
                                    --l;
                                ++s;
                            }
                            overlap_penalty = max(2, overlap_penalty);
                            int score = ijscore-overlap_penalty;
                            double intron_len = ej.m_intron_len+intron;
                            double deltas = score-ei.m_chain_score-log2factor*log2((intron_len+1)/(ei.m_intron_len+1));
                            if(deltas > 0) {
                                ei.m_chain_score = score;
                                ei.m_prevp = &ej;
                                ei.m_type = eNewAlignment;
                                ei.m_intron_len = intron_len;
                            }
                        } else {
                            int score = ijscore-compart_penalty;
                            double intron_len = ej.m_intron_len;
                            double deltas = score-ei.m_chain_score-log2factor*log2((intron_len+1)/(ei.m_intron_len+1));
                            if(deltas > 0) {          // start new compartment 
                                ei.m_chain_score = score;
                                ei.m_prevp = &ej;
                                ei.m_type = eNewCompartment;
                                ei.m_intron_len = intron_len;
                            }
                        }
                    }
                    double deltas = ei.m_chain_score-exons[best_index].m_chain_score-log2factor*log2((double)(ei.m_intron_len+1)/(exons[best_index].m_intron_len+1));
                    if(deltas > 0)
                        best_index = i;
                }

                /*                                                
                cerr << "Best index: " << best_index << " " << contig << " " << mate << " " << strand << " " << compart_penalty << endl;
                for(int i = 0; i < (int)exons.size(); ++i) {
                    auto& e = exons[i];
                    cerr << i << "\t" << e.m_matches << "\t" << e.m_chain_score << "\t" << e.m_intron_len << "\t" << e.m_qfrom+1 << "\t" << e.m_qto+1 << "\t" << e.m_sfrom+1 << "\t" << e.m_sto+1 << "\t" << e.m_type << "\t" << strand << endl;
                }
                */
                                                

                // not connected partial alignments initially are put in different compartments 
                TAlignCompartments& compartments = m_compartments[contig][mate][strand];
                for(Exon* p = &exons[best_index]; p != nullptr; p = p->m_prevp) {
                    if(compartments.empty() || compartments.back().m_exons.back().m_type != eExtension)
                        compartments.emplace_back();
                    compartments.back().m_exons.push_back(*p);
                } 

                // remove end indels
                for(auto iloop = compartments.begin(); iloop != compartments.end(); ) {
                    auto it = iloop++;
                    while(!it->m_exons.empty()) {
                        it->m_exons.front().RemoveLeftIndels(m_gapopen1, m_gapextend1, m_gapopen2, m_gapextend2);
                        if(it->m_exons.front().m_align_len == 0)
                            it->m_exons.erase(it->m_exons.begin());
                        else
                            break;
                    }
                    while(!it->m_exons.empty()) {
                        it->m_exons.back().RemoveRightIndels(m_gapopen1, m_gapextend1, m_gapopen2, m_gapextend2);
                        if(it->m_exons.back().m_align_len == 0)
                            it->m_exons.pop_back();
                        else
                            break;
                    }
                    if(it->m_exons.empty())
                        compartments.erase(it);
                }

                // init compartment
                for(auto& compartment : compartments) {
                    int matches = 0;
                    int align_len = 0;
                    string& cigar = compartment.m_cigar;
                    if(compartment.m_exons.front().m_qfrom > 0)
                        cigar += to_string(compartment.m_exons.front().m_qfrom)+"S";
                    string& tstrand = compartment.m_tstrand;
                    tstrand = compartment.m_exons.front().m_tstrand;
                    TMDTag md;
                    for(int i = 0; i < (int)compartment.m_exons.size(); ++i) {
                        auto& e = compartment.m_exons[i];
                        compartment.m_score += e.m_score;
                        matches += e.m_matches;
                        align_len += e.m_align_len;
                        if(i > 0) {
                            int intron = compartment.m_exons[i].m_sfrom-compartment.m_exons[i-1].m_sto-1;
                            cigar += to_string(intron)+"N";
                        }
                        for(auto& elem : compartment.m_exons[i].m_cigar)
                            cigar += to_string(elem.first)+elem.second;
                        compartment.m_dist += e.m_align_len-e.m_matches;
                        if(!tstrand.empty() && e.m_tstrand != tstrand)
                            tstrand.clear();
                        md.insert(md.end(), e.m_md.begin(), e.m_md.end());
                    }
                    int tail = rseq.size()-compartment.m_exons.back().m_qto-1;
                    if(tail > 0)
                        cigar += to_string(tail)+"S";

                    if(compartment.m_exons.size() == 1)
                        tstrand.clear();

                    string& md_tag = compartment.m_md;
                    for(auto i = md.begin(); i != md.end(); ++i) {
                        for(auto j = next(i); j != md.end(); j = next(i)) {
                            if(i->first > 0 && j->first > 0)
                                i->first += j->first;
                            else if(i->first == j->first)
                                i->second += j->second;
                            else
                                break;
                            md.erase(j);
                        }
                        if(i->first > 0)
                            md_tag += to_string(i->first);
                        else if(i->first == 0)
                            md_tag += i->second;
                        else
                            md_tag += "^"+i->second;
                    }
                    compartment.m_range.first = compartment.m_exons.front().m_sfrom;
                    compartment.m_range.second = compartment.m_exons.back().m_sto;
                    int read_span = compartment.m_exons.back().m_qto-compartment.m_exons.front().m_qfrom+1;
                    compartment.m_above_thresholds = double(read_span)/rseq.size() >= m_min_output_cov && double(matches)/align_len >= m_min_output_idty;
                    compartment.m_read = read;
                    compartment.m_rseq = rseq;
                    compartment.m_paired_read = paired_read;
                }

                /*                                
                for(auto& compartment : compartments) {
                    cerr << "Full path: " << compartment.m_score << endl;
                    for(int i = 0; i < (int)compartment.m_exons.size(); ++i) {
                        auto& e = compartment.m_exons[i];
                        cerr << i << "\t" << e.m_matches << "\t" << e.m_chain_score << "\t" << e.m_qfrom+1 << "\t" << e.m_qto+1 << "\t" << e.m_sfrom+1 << "\t" << e.m_sto+1 << "\t" << e.m_type << "\t" << strand << endl;
                    }
                    cerr << endl << endl;
                }
                */                                

                // remove lower score parts
                list<TAlignCompartments::iterator> erased;
                TAlignCompartments::reverse_iterator keeper = compartments.rbegin();
                for(auto i = next(keeper); i != compartments.rend(); ++i) {
                    if(i->m_exons.back().m_type == eNewCompartment) {
                        keeper = i;
                        continue;
                    }
                    if(i->m_score > keeper->m_score) {
                        erased.push_back(prev(keeper.base()));
                        keeper = i;
                    } else {
                        erased.push_back(prev(i.base()));
                    }
                }
                for(auto i : erased)
                    compartments.erase(i);


                /*                                 
                for(auto& compartment : compartments) {
                    cerr << "Score: " << compartment.m_score << endl;
                    for(int i = 0; i < (int)compartment.m_exons.size(); ++i) {
                        auto& e = compartment.m_exons[i];
                        cerr << i << "\t" << e.m_matches << "\t" << e.m_chain_score << "\t" << e.m_qfrom+1 << "\t" << e.m_qto+1 << "\t" << e.m_sfrom+1 << "\t" << e.m_sto+1 << "\t" << e.m_type << "\t" << strand << endl;
                    }
                    cerr << endl << endl;
                } 
                */                                

            }
        }
    }
}

void CExonSelectorApplication::Init()
{
    SetDiagPostLevel(eDiag_Info);

    auto_ptr<CArgDescriptions> argdescr(new CArgDescriptions);
    argdescr->SetUsageContext(GetArguments().GetProgramBasename(), "exon_selector expects SAM alignments at stdin collated by query, e.g. with 'sort -k 1,1'");
    argdescr->AddDefaultKey("penalty", "penalty", "Per-compartment penalty", CArgDescriptions::eDouble, "0.35");
    argdescr->AddDefaultKey ("max_intron", "max_intron", "Maximum intron length (in base pairs)", CArgDescriptions::eInteger, "1200000");
    argdescr->AddDefaultKey("min_query_len", "min_query_len", "Minimum length for individual cDNA sequences.", CArgDescriptions::eInteger, "50");
    argdescr->AddFlag("star","STAR aligner settings: -match 1 -mismatch 1 -gapopen1 2 -gapextend1 2 -gapopen2 2 -gapextend2 2 -min_output_identity 0.9 -min_output_coverage 0.9");
    argdescr->AddFlag("minimap2","minimap2 aligner settings: -match 1 -mismatch 2 -gapopen1 2 -gapextend1 1 -gapopen2 32 -gapextend2 0 -min_output_identity 0.8 -min_output_coverage 0.3");
    argdescr->AddOptionalKey("min_output_identity", "min_output_identity", "Minimal identity for output alignments",CArgDescriptions::eDouble);
    argdescr->AddOptionalKey("min_output_coverage", "min_output_coverage", "Minimal coverage for output alignments",CArgDescriptions::eDouble);
    argdescr->AddOptionalKey("match", "match", "Match score", CArgDescriptions::eInteger);
    argdescr->AddOptionalKey("mismatch", "mismatch", "Mismatch penalty", CArgDescriptions::eInteger);
    argdescr->AddOptionalKey("gapopen1", "gapopen1", "Gap open penalty. A gap of length k costs min{gapopen1+k*gapextend1,gapopen2+k*gapextend2}", CArgDescriptions::eInteger);
    argdescr->AddOptionalKey("gapextend1", "gapextend1", "Gap extend penalty", CArgDescriptions::eInteger);
    argdescr->AddOptionalKey("gapopen2", "gapopen2", "Gap open penalty", CArgDescriptions::eInteger);
    argdescr->AddOptionalKey("gapextend2", "gapextend2", "Gap extend penalty", CArgDescriptions::eInteger);
    argdescr->AddFlag("nocheck","Don't check if reads are collated (saves memory)");
    argdescr->AddFlag("remove_repeats","Remove alignments which have repeats in the read");

    CArgAllow* constrain01 (new CArgAllow_Doubles(0.0, 1.0));
    argdescr->SetConstraint("penalty", constrain01);
    argdescr->SetConstraint("min_output_identity", constrain01);
    argdescr->SetConstraint("min_output_coverage", constrain01);

    CArgAllow_Integers* constrain_minqlen (new CArgAllow_Integers(21,99999));
    argdescr->SetConstraint("min_query_len", constrain_minqlen);

    CArgAllow_Integers* constrain_score (new CArgAllow_Integers(0,99999));
    argdescr->SetConstraint("match", constrain_score);
    argdescr->SetConstraint("mismatch", constrain_score);
    argdescr->SetConstraint("gapopen1", constrain_score);
    argdescr->SetConstraint("gapextend1", constrain_score);
    argdescr->SetConstraint("gapopen2", constrain_score);
    argdescr->SetConstraint("gapextend2", constrain_score);

    SetupArgDescriptions(argdescr.release());
}

int CExonSelectorApplication::Run(void)
{
    const CArgs& args(GetArgs());

    m_penalty                  = args["penalty"].AsDouble();
    m_min_query_len            = args["min_query_len"].AsInteger();
    m_max_intron               = args["max_intron"].AsInteger();
    bool check                 = !args["nocheck"];
    if(args["remove_repeats"])
        m_remove_repats = true;

    if(args["star"] && args["minimap2"]) {
        cerr << "-star and -minimap2 can't be used together" << endl;
        exit(1);                                
    }

    if(args["minimap2"] || !args["star"]) {
        m_matchscore = 1;
        m_mismatchpenalty = 2;
        m_gapopen1 = 2;
        m_gapopen2 = 32;
        m_gapextend1 = 1;
        m_gapextend2 = 0;
        m_min_output_idty = 0.8;
        m_min_output_cov = 0.3;
    }

    if(args["star"]) {
        m_matchscore = 1;
        m_mismatchpenalty = 1;
        m_gapopen1 = 2;
        m_gapopen2 = 2;
        m_gapextend1 = 2;
        m_gapextend2 = 2;
        m_min_output_idty = 0.9;
        m_min_output_cov = 0.9;
    }

    if(args["min_output_identity"])
        m_min_output_idty = args["min_output_identity"].AsDouble();
    if(args["min_output_coverage"])
        m_min_output_cov = args["min_output_coverage"].AsDouble();

    if(args["match"])
        m_matchscore = args["match"].AsInteger();
    if(args["mismatch"])
        m_mismatchpenalty = args["mismatch"].AsInteger();
    if(args["gapopen1"])
       m_gapopen1  = args["gapopen1"].AsInteger();
    if(args["gapextend1"])
       m_gapextend1  = args["gapextend1"].AsInteger();
    if(args["gapopen2"])
       m_gapopen2  = args["gapopen2"].AsInteger();
    if(args["gapextend2"])
       m_gapextend2  = args["gapextend2"].AsInteger();

    string line;
    string read_old;
    bool paired_read = false;
    set<string> previous_reads;
    while(getline(cin, line)) {
        if(line.empty())
            continue;
        if(line[0] == '@') {
            cout << line << endl;
            continue;
        }
        vector<string> fields;
        NStr::Split(line, "\t", fields, 0);
        string read = fields[0];
        if(read != read_old && !m_read_aligns.empty()) {                    // select and emit alignments for read; clear storage
            if(check && !previous_reads.insert(read_old).second) {
                cerr << "Input collated by reads is expected" << endl;
                exit(1);                                
            }
            ChainExons();
            if(paired_read)
                ConnectPairs();
            SelectBestLocations();
            if(!m_read_through)
                FormatResults();
            m_read_aligns.clear();
            m_compartments.clear();
        }
        read_old = read;
        paired_read = stoi(fields[1])&1;

        if((int)fields[9].size() < m_min_query_len)
            continue;

        // separate contigs/strands
        string contig = fields[2];
        int flag = std::stoi(fields[1]);
        int strand = (flag&16) ? 1 : 0;
        int mate = (flag&128) ? 1 : 0;
        m_read_aligns[contig][mate][strand].push_back(fields);
    }
    
    // deal with the last read
    if(check && !previous_reads.insert(read_old).second) {
        cerr << "Input collated by reads is expected" << endl;
        exit(1);                                
    }
    ChainExons();
    if(paired_read)
        ConnectPairs();
    SelectBestLocations();
    if(!m_read_through)
        FormatResults();
    m_read_aligns.clear();
    m_compartments.clear();

    return 0;
}

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CExonSelectorApplication().AppMain(argc, argv, 0, eDS_ToStderr);
}
