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
 * Test application for selecting compact SAM alignments
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <util/sequtil/sequtil_manip.hpp>

#include <algo/align/util/compartment_finder.hpp>
#include <algo/align/util/blast_tabular.hpp>
#include <algo/align/util/hit_filter.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

class CCompactSAMApplication : public CNcbiApplication
{
public:
    virtual void Init();
    virtual int  Run();

    typedef CBlastTabular          THit;
    typedef CRef<THit>             THitRef;
    typedef vector<THitRef>        THitRefs;
    typedef vector<string>         TSamFields;
    typedef list<TSamFields>       TSplitAligns;
    template<typename T>
    using TMatrix = array<array<T, 2>, 2>;

    struct Exon {
        int m_qfrom = 0;
        int m_qto = 0;
        int m_sfrom = 0;
        int m_sto = 0;
        int m_matches = 0;
        int m_mismatches = 0;
        int m_indels = 0;
        int m_align_len = 0;
        int m_score = 0;
    };

    struct AlignInfo {
        pair<int, int> m_range;
        int            m_score = 0;
        bool           m_above_thresholds = false;
        TSamFields     m_fields;
        AlignInfo*     m_matep = nullptr;  // if other mate was found
        vector<Exon>   m_exons;
    };
    typedef list<AlignInfo> TAlignInfoList;

    void FindCompactAligns();
    void ConnectPairs();
    void SelectBestLocations();
    bool CompatiblePair(const AlignInfo& left, const AlignInfo& right);
    void FormatResults();

    double                 m_min_output_idty;
    double                 m_min_output_cov;
    double                 m_penalty;
    double                 m_min_idty;
    double                 m_min_singleton_idty;
    int                    m_min_singleton_idty_bps;
    int                    m_min_query_len;
    int                    m_max_intron;

    map<string, TMatrix<TAlignInfoList>> m_compact_aligns;  // [contig][mate][strand] list of AlignInfo
};

CCompactSAMApplication::Exon GetNextExon(string& cigar_string, string& MD_tag, int qfrom, int sfrom) {
    CCompactSAMApplication::Exon e;
    e.m_qfrom = qfrom;
    e.m_sfrom = sfrom;
        
    e.m_qto = e.m_qfrom-1;
    e.m_sto = e.m_sfrom-1;
    istringstream icigar(cigar_string);
    int len;
    char c;
    int clip_pos = 0;
    while(icigar >> len >> c) {
        if(c == 'S') {
            if(e.m_align_len == 0) { //new query start
                e.m_qfrom += len;
                e.m_qto = e.m_qfrom-1;
            }
        } else if(c == 'N') {
            if(e.m_align_len == 0) { // new subject start
                e.m_sfrom += len;
                e.m_sto = e.m_sfrom-1;
            } else {
                cigar_string = cigar_string.substr(clip_pos); // put back intron for next exon cigar    
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
                    MD_tag.erase(0, idx);                 // remove matches
                    if(m > len) {                         // matches split between exons
                        MD_tag = to_string(m-len)+MD_tag; // return remaining matches
                        e.m_matches += len;
                        len = 0;
                    } else {
                        e.m_matches += m;
                        len -= m;
                    }
                } else {
                    ++e.m_mismatches;
                    --len;
                    MD_tag.erase(0, 1);
                }
            }
        } else if(c == 'I') {
            e.m_align_len += len;
            e.m_qto += len;
            ++e.m_indels;
        } else if(c == 'D') {
            e.m_align_len += len;
            e.m_sto += len;
            ++e.m_indels;
            if(MD_tag.front() == '^')
                MD_tag.erase(0, len+1);
            else                         // in case deletion was split between exons (was not seen in STAR output yet)
                MD_tag.erase(0, len);
        } else {
            cerr << "Unexpected symbol in CIGAR" << endl;
            exit(1);
        }
        clip_pos = icigar.tellg();
    }
    cigar_string.clear();

    return e;
}

int ProcessSAM(const vector<string>& tags, vector<CCompactSAMApplication::Exon>& exons) {

    // should be parameters if changed from defaults for STAR run   
    int matchscore = 1;
    int mismatchpenalty = 1;
    int gapopen = 2;
    int gapextend = 2;
    double log2factor = 0.25;
    int intron_penalty = 0;
    int noncanonical_penalty = 8;
    int gcag_penalty = 4;
    int atac_penalty = 8;

    string MD_tag;
    string jM_tag;
    for(int i = 11; i < (int)tags.size() && (MD_tag.empty() || jM_tag.empty()); ++i) {
        auto& tag = tags[i]; 
        vector<string> fields;
        NStr::Split(tag, ":", fields, 0);
        if(fields.size() != 3)
            continue;
        if(fields[0] == "MD" && fields[1] == "Z")
            MD_tag = fields[2];
        if(fields[0] == "jM" && fields[1] == "B")
            jM_tag = fields[2];
    }
    if(MD_tag.empty()) {
        cerr << "MD:Z tag in SAM file is expected" << endl;
        exit(1);
    }
    if(jM_tag.empty()) {
        cerr << "jM:B tag in SAM file is expected" << endl;
        exit(1);
    }
                
    // extract exons    
    string cigar_string = tags[5];
    int qfrom = 0;
    int sfrom = std::stoi(tags[3])-1;
    while(!cigar_string.empty()) {
        exons.push_back(GetNextExon(cigar_string, MD_tag, qfrom, sfrom));
        qfrom = exons.back().m_qto+1;
        sfrom = exons.back().m_sto+1;
    }
    pair<int, int> range(exons.front().m_sfrom, exons.back().m_sto);

    // score exons and whole alignmment 
    int align_score = -int(log2factor*log2(double(range.second-range.first+1))+0.5);
    for(auto& e : exons) {
        int gap_len = e.m_align_len-e.m_matches-e.m_mismatches;
        e.m_score = e.m_matches*matchscore-e.m_mismatches*mismatchpenalty-e.m_indels*gapopen-gap_len*gapextend;
        align_score += e.m_score;
    }
    if(next(exons.begin()) != exons.end()) { // > 1
        vector<string> splices;
        NStr::Split(jM_tag, ",", splices, 0);
        for(auto& spl : splices) {
            if(spl != "c")
                align_score -= intron_penalty;
            if(spl == "0")
                align_score -= noncanonical_penalty;
            else if(spl == "3" || spl == "4")
                align_score -= gcag_penalty;
            else if(spl == "5" || spl == "6")
                align_score -= atac_penalty;
        }
    }

    return align_score;
}

void Print(const CCompactSAMApplication::AlignInfo& ai) {
    cout << ai.m_fields[0];
    for(unsigned i = 1; i < ai.m_fields.size(); ++i)
        cout << "\t" << ai.m_fields[i];
    cout << endl;        
}

string Tag(const string& name, int value) {
    return name+":i:"+to_string(value);
}
string Tag(const string& name, const string& value) {
    return name+":Z:"+value;
}
template<typename T>
void ReplaceTag(vector<string>& fields, const string& name, const T& value) {
    auto itag = find_if(fields.begin()+11, fields.end(), [&](const string& tag){ return tag.substr(0,2) == name; });
    if(itag != fields.end())
        *itag = Tag(name, value);
    else
        fields.push_back(Tag(name, value));
}
void RemoveTag(vector<string>& fields, const string& name) {
    auto itag = find_if(fields.begin()+11, fields.end(), [&](const string& tag){ return tag.substr(0,2) == name; });
    if(itag != fields.end())
        fields.erase(itag);
}

// mates were connected
void FormatPaired(CCompactSAMApplication::AlignInfo& ai, int mate, int strand, int count, int index, bool left) {
    auto& fields = ai.m_fields;

    //flag
    int flag = 1+2+strand*16+(1-strand)*32+(mate+1)*64;
    fields[1] = to_string(flag);

    // fields   
    fields[6] = "=";
    fields[7] = ai.m_matep->m_fields[3];
    int span = max(ai.m_range.second, ai.m_matep->m_range.second)-min(ai.m_range.first, ai.m_matep->m_range.first)+1;
    if(!left)
        span = -span;
    fields[8] = to_string(span);

    //tags  
    ReplaceTag(fields, "NH", count); 
    ReplaceTag(fields, "HI", index); 
    ReplaceTag(fields, "AS", ai.m_score+ai.m_matep->m_score);
    int mismatches = 0;
    for(auto& e : ai.m_exons)
        mismatches += e.m_mismatches;
    for(auto& e : ai.m_matep->m_exons)
        mismatches += e.m_mismatches;
    ReplaceTag(fields, "nM", mismatches);
    ReplaceTag(fields, "MC", ai.m_matep->m_fields[5]);

    Print(ai);
}

// single end read or mates not connected
void FormatSingle(CCompactSAMApplication::AlignInfo& ai, int mate, int strand, int count, int index, bool all_aligned) {
    auto& fields = ai.m_fields;

    //flag
    int flag = stoi(fields[1])&1;      // capture if read is paired ends
    if(flag) {                         // paired ends (mates not connected)
        flag += (mate+1)*64;
        flag += 8;                     // if mates not connected the other mate reflected as 'not aligned' regrdless
        /*
        if(!all_aligned)
            flag += 8;
        */
    }
    flag += strand*16;
    fields[1] = to_string(flag);

    // fields   
    fields[6] = "*";
    fields[7] = "0";
    fields[8] = "0";

    //tags  
    ReplaceTag(fields, "NH", count); 
    ReplaceTag(fields, "HI", index); 
    ReplaceTag(fields, "AS", ai.m_score);
    int mismatches = 0;
    for(auto& e : ai.m_exons)
        mismatches += e.m_mismatches;
    ReplaceTag(fields, "nM", mismatches);
    RemoveTag(fields, "MC");

    Print(ai);
}

void CCompactSAMApplication::FormatResults() {
    bool paired = false;
    array<int, 2> mate_count = {0, 0};
    for(auto& contig_aligns : m_compact_aligns) {
        for(int mate = 0; mate < 2; ++mate) {
            for(int strand = 0; strand < 2; ++strand) {
                TAlignInfoList& compact_aligns = m_compact_aligns[contig_aligns.first][mate][strand];
                for(auto& ai : compact_aligns) {
                    ++mate_count[mate];
                    if(ai.m_matep != nullptr)
                        paired = true;
                }
            }
        }
    }
                    
    if(paired) {
        int index = 0;
        for(auto& contig_aligns : m_compact_aligns) {
            for(int mate = 0; mate < 2; ++mate) {
                int strand = 0;
                auto& left_aligns = contig_aligns.second[mate][strand];
                for(auto& ai :  left_aligns) {
                    FormatPaired(ai, mate, strand, mate_count[0], ++index, true);
                    FormatPaired(*ai.m_matep, 1-mate, 1-strand, mate_count[0], index, false);
                }
            }
        }
    } else {
        array<int, 2> mate_index = {0, 0};
        bool all_aligned = mate_count[0] > 0 && mate_count[1] > 0;

        for(auto& contig_aligns : m_compact_aligns) {
            for(int mate = 0; mate < 2; ++mate) {
                for(int strand = 0; strand < 2; ++strand) {
                    TAlignInfoList& compact_aligns = m_compact_aligns[contig_aligns.first][mate][strand];
                    for(auto& ai : compact_aligns)
                        FormatSingle(ai, mate, strand, mate_count[mate], ++mate_index[mate], all_aligned);
                }
            }
        }
    }
}

void CCompactSAMApplication::SelectBestLocations() {
    int pair_score = numeric_limits<int>::min();
    array<int, 2> mate_score = {numeric_limits<int>::min(), numeric_limits<int>::min()};
    for(auto& contig_aligns : m_compact_aligns) {
        for(int mate = 0; mate < 2; ++mate) {
            for(int strand = 0; strand < 2; ++strand) {
                TAlignInfoList& compact_aligns = m_compact_aligns[contig_aligns.first][mate][strand];
                for(auto& align : compact_aligns) {
                    mate_score[mate] = max(mate_score[mate], align.m_score);
                    if(align.m_matep != nullptr)
                        pair_score = max(pair_score, align.m_score+align.m_matep->m_score);
                }
                compact_aligns.remove_if([](const AlignInfo& a){ return !a.m_above_thresholds; }); // remove after best score found; connected mates are above thresholds and will not be broken
            }
        }
    }

    bool paired = pair_score >= max(mate_score[0], mate_score[1]);
    for(auto& contig_aligns : m_compact_aligns) {
        for(int mate = 0; mate < 2; ++mate) {
            for(int strand = 0; strand < 2; ++strand) {
                TAlignInfoList& compact_aligns = m_compact_aligns[contig_aligns.first][mate][strand];
                if(paired)
                    compact_aligns.remove_if([&](const AlignInfo& a){ 
                                                 if(a.m_matep == nullptr) return true;
                                                 if(a.m_score+a.m_matep->m_score == pair_score) return false;
                                                 a.m_matep->m_matep = nullptr; return true; });
                else
                    compact_aligns.remove_if([&](AlignInfo& a){ a.m_matep = nullptr; return a.m_score != mate_score[mate]; });                
            }
        }
    }
}

bool CCompactSAMApplication::CompatiblePair(const AlignInfo& left, const AlignInfo& right) {
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

void CCompactSAMApplication::ConnectPairs() {
    for(auto& contig_aligns : m_compact_aligns) {
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

void CCompactSAMApplication::FindCompactAligns() {
    for(auto& contig_aligns : m_compact_aligns) {
        for(int mate = 0; mate < 2; ++mate) {
            for(int strand = 0; strand < 2; ++strand) {
                TAlignInfoList& compact_aligns = contig_aligns.second[mate][strand];
                if(compact_aligns.empty())
                    continue;
                vector<Exon> exons;
                size_t read_len = compact_aligns.front().m_fields[9].size();

                // find exons, scores and ranges for alignments
                for(auto& ai : compact_aligns) {
                    auto& fields = ai.m_fields;
                    vector<Exon> align_exons;
                    int align_score = ProcessSAM(fields, align_exons);
                    pair<int, int> align_range(align_exons.front().m_sfrom, align_exons.back().m_sto);
                    int read_span = align_exons.back().m_qto-align_exons.front().m_qfrom+1;
                    exons.insert(exons.end(), align_exons.begin(), align_exons.end());

                    int matches = 0;
                    int align_len = 0;
                    for(auto& e : align_exons) {
                        matches += e.m_matches;
                        align_len += e.m_align_len;
                    }
                    bool above_hresholds = double(read_span)/read_len >= m_min_output_cov && double(matches)/align_len >= m_min_output_idty;
                    ai.m_range = align_range;
                    ai.m_score = align_score;
                    ai.m_above_thresholds = above_hresholds;
                    swap(align_exons, ai.m_exons);
                }

                // remove redundant exons sharing ends
                {
                    // high score first 
                    sort(exons.begin(), exons.end(), [](const Exon& e1, const Exon& e2){ 
                                                        if(e1.m_score == e2.m_score)
                                                            return e1.m_matches > e2.m_matches;
                                                        else
                                                            return e1.m_score > e2.m_score; });
                    set<int> lefts;
                    set<int> rights;
                    auto tail = remove_if(exons.begin(), exons.end(), [&](const Exon& e){
                                              bool r1 = !lefts.insert(e.m_sfrom).second; bool r2 = !rights.insert(e.m_sto).second; 
                                              return r1 || r2; });
                    exons.erase(tail, exons.end());        
                }

                // find compartments (access algoalignsplign)
                // here we don't care about actual ids 
                CRef<CSeq_id> readid(new CSeq_id("lcl|read"));
                CRef<CSeq_id> contigid(new CSeq_id("lcl|contig"));    
                THitRefs hitrefs;
                for(auto& e : exons) {
                    THitRef hit (new THit());
                    hit->SetQueryId(readid);
                    hit->SetSubjId(contigid);
                    hit->SetQueryStart(e.m_qfrom);
                    hit->SetQueryStop(e.m_qto);
                    hit->SetSubjStart(e.m_sfrom);
                    hit->SetSubjStop(e.m_sto);
                    hit->SetLength(e.m_align_len);
                    hit->SetMismatches(e.m_mismatches);
                    hit->SetGaps(e.m_indels);
                    hit->SetEValue(0);
                    hit->SetScore(e.m_score);
                    hit->SetRawScore(e.m_score);
                    hit->SetIdentity(float(e.m_matches)/e.m_align_len);
                    hitrefs.push_back(hit);
                }

                typedef CCompartmentAccessor<THit> TAccessor;
                typedef TAccessor::TCoord          TCoord;

                const TCoord penalty_bps (TCoord(m_penalty*read_len + 0.5));
                const TCoord min_matches (TCoord(m_min_idty*read_len + 0.5));
                const TCoord msm1        (TCoord(m_min_singleton_idty*read_len + 0.5));
                const TCoord msm2        (m_min_singleton_idty_bps);
                const TCoord min_singleton_matches (min(msm1, msm2));

                TAccessor ca (penalty_bps, min_matches, min_singleton_matches, false);
                ca.SetMaxIntron(m_max_intron);
                ca.Run(hitrefs.begin(), hitrefs.end());
        
                // find ranges  
                set<pair<int, int>> selected_ranges;
                THitRefs comp;
                for(bool b0 (ca.GetFirst(comp)); b0 ; b0 = ca.GetNext(comp)) {
                    TSeqPos span[4];
                    CHitFilter<THit>::s_GetSpan(comp, span);
                    pair<int, int> range(span[2], span[3]);
                    selected_ranges.insert(range);
                }

                // select alignmnets by ranges
                compact_aligns.remove_if([&](const AlignInfo& a){ return selected_ranges.count(a.m_range) == 0; });

                // remove redundant alignmnets
                compact_aligns.sort([](const AlignInfo& a1, const AlignInfo& a2) {
                                        if(a1.m_range == a2.m_range)
                                            return a1.m_score > a2.m_score;
                                        else
                                            return a1.m_range < a2.m_range; }); 
                for(auto it = compact_aligns.begin(); it != compact_aligns.end(); ++it) {
                    for(auto inext = next(it); inext != compact_aligns.end() && inext->m_range == it->m_range; )  // delete duplicate with the same interval
                        inext = compact_aligns.erase(inext);
                }                                
            }
        }
    }
}

void CCompactSAMApplication::Init()
{
    SetDiagPostLevel(eDiag_Info);

    unique_ptr<CArgDescriptions> argdescr(new CArgDescriptions);
    argdescr->SetUsageContext(GetArguments().GetProgramBasename(),
                              "compact_sam expects SAM alignments at stdin collated by query, e.g. with 'sort -k 1,1'");

    argdescr->AddDefaultKey("min_output_identity", "min_output_identity", "Minimal identity for output alignments",
                            CArgDescriptions::eDouble, "0.9");

    argdescr->AddDefaultKey("min_output_coverage", "min_output_coverage", "Minimal coverage for output alignments",
                            CArgDescriptions::eDouble, "0.9");

    argdescr->AddDefaultKey("penalty", "penalty", "Per-compartment penalty",
                            CArgDescriptions::eDouble, "0.55");
    
    argdescr->AddDefaultKey ("max_intron", "max_intron", 
                             "Maximum intron length (in base pairs)",
                             CArgDescriptions::eInteger,
                             "1200000");

    argdescr->AddDefaultKey("min_query_len", "min_query_len", 
                            "Minimum length for individual cDNA sequences.",
                            CArgDescriptions::eInteger, "50");

    argdescr->AddFlag("nocheck","Don't check if reads are collated (saves memory)");

    CArgAllow* constrain01 (new CArgAllow_Doubles(0.0, 1.0));
    argdescr->SetConstraint("penalty", constrain01);
    argdescr->SetConstraint("min_output_identity", constrain01);
    argdescr->SetConstraint("min_output_coverage", constrain01);

    CArgAllow_Integers* constrain_minqlen (new CArgAllow_Integers(21,99999));
    argdescr->SetConstraint("min_query_len", constrain_minqlen);

    SetupArgDescriptions(argdescr.release());
}

int CCompactSAMApplication::Run(void)
{
    const CArgs& args(GetArgs());

    m_min_output_idty          = args["min_output_identity"].AsDouble();
    m_min_output_cov           = args["min_output_coverage"].AsDouble();
    m_penalty                  = args["penalty"].AsDouble();
    m_min_idty                 = 0.7;
    m_min_singleton_idty       = m_min_idty;
    m_min_singleton_idty_bps   = 9999999;
    m_min_query_len            = args["min_query_len"].AsInteger();
    m_max_intron               = args["max_intron"].AsInteger();
    bool check                 = !args["nocheck"];

    string line;
    string read_old;
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
        if(read != read_old) {                                             // select and emit alignments for read; clear storage
            if(check && !previous_reads.insert(read_old).second) {
                cerr << "Input collated by reads is expected" << endl;
                exit(1);                                
            }
            FindCompactAligns();
            ConnectPairs();
            SelectBestLocations();
            FormatResults();
            m_compact_aligns.clear();
        }
        read_old = read;

        if((int)fields[9].size() < m_min_query_len)
            continue;

        // separate contigs/mates/strands
        string contig = fields[2];
        int flag = std::stoi(fields[1]);
        int strand = (flag&16) ? 1 : 0;
        int mate = (flag&128) ? 1 : 0;
        auto& ais = m_compact_aligns[contig][mate][strand];
        ais.emplace_back();
        swap(ais.back().m_fields, fields);
    }
    
    // deal with the last read
    if(check && !previous_reads.insert(read_old).second) {
        cerr << "Input collated by reads is expected" << endl;
        exit(1);                                
    }
    FindCompactAligns();
    ConnectPairs();
    SelectBestLocations();
    FormatResults();
    m_compact_aligns.clear();

    return 0;
}

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CCompactSAMApplication().AppMain(argc, argv, 0, eDS_ToStderr);
}
