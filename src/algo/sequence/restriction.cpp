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
 * Authors:  Josh Cherry
 *
 * File Description:  Classes for representing and finding restriction sites
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objmgr/seq_vector.hpp>
#include <algorithm>
#include <algo/sequence/restriction.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


bool CRSpec::operator<(const CRSpec& rhs) const
{
    if (GetSeq() != rhs.GetSeq()) {
        return GetSeq() < rhs.GetSeq();
    }
    // otherwise sequences identical
    if (GetPlusCuts() != rhs.GetPlusCuts()) {
        return GetPlusCuts() < rhs.GetPlusCuts();
    }
    if (GetMinusCuts() != rhs.GetMinusCuts()) {
        return GetMinusCuts() < rhs.GetMinusCuts();
    }
    // otherwise arguments are equal, and < is false
    return false;
}


void CREnzyme::Reset(void)
{
    m_Name.erase();
    m_Specs.clear();
}


// helper functor for sorting enzymes by specificity
struct SCompareSpecs
{
    bool operator()(const CREnzyme& lhs, const CREnzyme& rhs)
    {
        return lhs.GetSpecs() < rhs.GetSpecs();
    }
};


void CREnzyme::CombineIsoschizomers(vector<CREnzyme>& enzymes)
{
    stable_sort(enzymes.begin(), enzymes.end(), SCompareSpecs());
    vector<CREnzyme> result;
    ITERATE (vector<CREnzyme>, enzyme, enzymes) {
        if (enzyme != enzymes.begin() &&
            enzyme->GetSpecs() == result.back().GetSpecs()) {
            result.back().SetName() += "/";
            result.back().SetName() += enzyme->GetName();
        } else {
            result.push_back(*enzyme);
        }
    }
    swap(enzymes, result);
}


void CRSpec::Reset(void)
{
    m_Seq.erase();
    m_PlusCuts.clear();
    m_MinusCuts.clear();
}


ostream& operator<<(ostream& os, const CRSite& site)
{
    os << "Recog. site: " << site.GetStart() << '-'
       << site.GetEnd() << endl;
    os << "Plus strand cuts: ";
    string s;
    ITERATE (vector<int>, cut, site.GetPlusCuts()) {
        if (!s.empty()) {
            s += " ,";
        }
        s += NStr::IntToString(*cut);
    }
    os << s << endl;

    os << "Minus strand cuts: ";
    s.erase();
    ITERATE (vector<int>, cut, site.GetMinusCuts()) {
        if (!s.empty()) {
            s += " ,";
        }
        s += NStr::IntToString(*cut);
    }
    os << s << endl;
    
    return os;    
}


ostream& operator<<(ostream& os, const CREnzResult& er)
{
    os << "Enzyme: " << er.GetEnzymeName() << endl;
    os << er.GetDefiniteSites().size() << " definite sites:" << endl;
    ITERATE (vector<CRSite>, site, er.GetDefiniteSites()) {
        os << *site;
    }
    os << er.GetPossibleSites().size() << " possible sites:" << endl;
    ITERATE (vector<CRSite>, site, er.GetPossibleSites()) {
        os << *site;
    }
    return os;
}


struct SCompareLocation
{
    bool operator() (const CRSite& lhs, const CRSite& rhs) const
    {
        return lhs.GetStart() < rhs.GetStart();
    }
};


// Class for internal use by restriction site finding.
// Holds a pattern in ncbi8na, integers that specify
// which enzyme (of a sequential collection) 
// and which specifity of that
// enzyme it represents, a field indicating
// whether it represents the complement of the specificity,
// and the length of the initial subpattern that was
// put into the fsm.
class CPatternRec
{
public:
    enum EIsComplement {
        eIsNotComplement,
        eIsComplement
    };

    CPatternRec(string pattern, int enzyme_index, int spec_index,
                EIsComplement is_complement,
                unsigned int fsm_pat_size) : m_Pattern(pattern),
                                             m_EnzymeIndex(enzyme_index),
                                             m_SpecIndex(spec_index),
                                             m_IsComplement(is_complement),
                                             m_FsmPatSize(fsm_pat_size) {}
    // member access
    const string& GetPattern(void) const {return m_Pattern;}
    int GetEnzymeIndex(void) const {return m_EnzymeIndex;}
    int GetSpecIndex(void) const {return m_SpecIndex;}
    EIsComplement GetIsComlement(void) const {return m_IsComplement;}
    bool IsComplement(void) const {return m_IsComplement == eIsComplement;}
    unsigned int GetFsmPatSize(void) const {return m_FsmPatSize;}
private:
    // pattern in ncbi8na
    string m_Pattern;
    // which enzyme and specificity we represent
    int m_EnzymeIndex;
    int m_SpecIndex;
    // whether we represent the complement of the specificity
    EIsComplement m_IsComplement;
    unsigned int m_FsmPatSize;
};


void CFindRSites::x_ExpandRecursion(string& s, unsigned int pos,
                                    CTextFsm<int>& fsm, int match_value)
{
    if (pos == s.size()) {
        // this is the place
        fsm.AddWord(s, match_value);
        return;
    }

    char orig_ch = s[pos];
    for (char x = 1;  x <= 8;  x <<= 1) {
        if (orig_ch & x) {
            s[pos] = x;
            x_ExpandRecursion(s, pos + 1, fsm, match_value);
        }
    }
    // restore original character
    s[pos] = orig_ch;
}


void CFindRSites::x_AddPattern(const string& pat, CTextFsm<int>& fsm,
                               int match_value)
{
    string s = pat;
    x_ExpandRecursion(s, 0, fsm, match_value);
}


bool CFindRSites::x_IsAmbig(char nuc)
{
    static const bool ambig_table[16] = {
        0, 0, 0, 1, 0, 1, 1, 1,
        0, 1, 1, 1, 1, 1, 1, 1
    };
    return ambig_table[nuc];
}


/// Find all definite and possible sites in a sequence
/// for a vector of enzymes, using a finite state machine.
/// Templatized so that seq can be various containers
/// (e.g., string, vector<char>, CSeqVector),
/// but it must yield ncbi8na.
template<class Seq>
void x_FindRSite(const Seq& seq, const vector<CREnzyme>& enzymes,
                 vector<CRef<CREnzResult> >& results)
{

    results.clear();
    results.reserve(enzymes.size());

    // vector of patterns for internal use
    vector<CPatternRec> patterns;
    patterns.reserve(enzymes.size());  // an underestimate
    
    // the finite state machine for the search
    CTextFsm<int> fsm;

    // iterate over enzymes
    ITERATE (vector<CREnzyme>, enzyme, enzymes) {

        results.push_back(CRef<CREnzResult> 
                          (new CREnzResult(enzyme->GetName())));

        string pat;
        const vector<CRSpec>& specs = enzyme->GetSpecs();

        // iterate over specificities for this enzyme
        ITERATE (vector<CRSpec>, spec, specs) {
            // one or two patterns for this specificity
            // (one for pallindrome, two otherwise)
            
            // to avoid combinatorial explosion,
            // if there are more than two Ns only use
            // part of pattern before first N
            CSeqMatch::IupacToNcbi8na(spec->GetSeq(), pat);
            SIZE_TYPE fsm_pat_size = pat.find_first_of(0x0f);
            {{
                SIZE_TYPE pos = pat.find_first_of(0x0f, fsm_pat_size + 1);
                if (pos == NPOS
                    ||  pat.find_first_of(0x0f, pos + 1) == NPOS) {
                    fsm_pat_size = pat.size();
                }
            }}
            patterns.push_back(CPatternRec(pat, enzyme - enzymes.begin(),
                                           spec - specs.begin(), 
                                           CPatternRec::eIsNotComplement,
                                           fsm_pat_size));
            // add pattern to fsm
            // (add only fsm_pat_size of it)
            CFindRSites::x_AddPattern(pat.substr(0, fsm_pat_size), fsm,
                                      patterns.size() - 1);

            // if the pattern is not pallindromic,
            // do a search for its complement too
            string comp = pat;
            CSeqMatch::CompNcbi8na(comp);
            if (comp != pat) {
                {{
                    fsm_pat_size = comp.find_first_of(0x0f);
                    SIZE_TYPE pos = comp.find_first_of(0x0f, fsm_pat_size + 1);
                    if (pos == NPOS
                        ||  comp.find_first_of(0x0f, pos + 1) == NPOS) {
                        fsm_pat_size = comp.size();
                    }
                }}
                patterns.push_back(CPatternRec(comp, enzyme 
                                               - enzymes.begin(),
                                               spec - specs.begin(), 
                                               CPatternRec::eIsComplement,
                                               fsm_pat_size));
                // add pattern to fsm
                // (add only fsm_pat_size of it)
                CFindRSites::x_AddPattern(comp.substr(0, fsm_pat_size), fsm,
                                          patterns.size() - 1);
            }
        }
    }
    
    // The fsm is set up.
    // Now do the search.

    fsm.Prime();
    vector<int> ambig_nucs;  // for dealing with ambiguities later

    int state = fsm.GetInitialState();
    for (unsigned int i = 0;  i < seq.size();  i++) {
        if (CFindRSites::x_IsAmbig(seq[i])) {
            ambig_nucs.push_back(i);
        }
        state = fsm.GetNextState(state, seq[i]);
        if (fsm.IsMatchFound(state)) {
            const vector<int>& matches = fsm.GetMatches(state);
            ITERATE (vector<int>, match, matches) {
                const CPatternRec& pattern = patterns[*match];
                TSeqPos begin_pos = i + 1 - pattern.GetFsmPatSize();
                TSeqPos end_pos = begin_pos 
                    + pattern.GetPattern().size() - 1;
                
                // check for a full match to sequence
                if (pattern.GetFsmPatSize()
                    != pattern.GetPattern().size()) {
                    // Pattern put into fsm was less than the full pattern.
                    // Must check that the sequence really matches.
                    if (end_pos >= seq.size()) {
                        // full pattern goes off the end of sequence;
                        // not a match
                        continue;
                    }
                    // could really test match for just 
                    // right end of pattern here
                    if (CSeqMatch::MatchNcbi8na(seq, pattern.GetPattern(),
                                                begin_pos) 
                        == CSeqMatch::eYes) {
                        // There must be a site here.  However, because
                        // we'll later check all stretches containing
                        // ambiguities, we must ignore them here to keep
                        // from recording them twice.
                        bool ambig = false;
                        for (unsigned int n = begin_pos;  n <= end_pos;
                             n++) {
                            if (CFindRSites::x_IsAmbig(seq[n])) {
                                ambig = true;
                                break;
                            }
                        }
                        if (ambig) {
                            continue;  // ignore matches with ambig. seq.
                        }
                    } else {
                        continue;  // not a real match
                    }
                }
                CRSite site(begin_pos, end_pos);
                const CRSpec& spec = enzymes[pattern.GetEnzymeIndex()]
                    .GetSpecs()[pattern.GetSpecIndex()];

                const vector<int>& plus_cuts = spec.GetPlusCuts();
                ITERATE (vector<int>, cut, plus_cuts) {
                    if (pattern.IsComplement()) {
                        site.SetPlusCuts()
                            .push_back(begin_pos 
                                       + pattern.GetPattern().size()
                                       - *cut);
                    } else {
                        site.SetPlusCuts().push_back(begin_pos + *cut);
                    }
                }
                const vector<int>& minus_cuts = spec.GetMinusCuts();
                ITERATE (vector<int>, cut, minus_cuts) {
                    if (pattern.IsComplement()) {
                        site.SetMinusCuts()
                            .push_back(begin_pos
                                       + pattern.GetPattern().size()
                                       - *cut);
                    } else {
                        site.SetMinusCuts().push_back(begin_pos + *cut);
                    }
                }

                results[pattern.GetEnzymeIndex()]->SetDefiniteSites()
                    .push_back(site);
            }
        }
    }  // end iteration over the sequence

    // We've found all the matches involving unambiguous characters
    // in sequence.  Now go back and examine any ambiguous places.
    if (!ambig_nucs.empty()) {
        ITERATE (vector<CPatternRec>, pattern, patterns) {
            const string& pat = pattern->GetPattern();

            // for reordering later
            int ds_pos = results[pattern->GetEnzymeIndex()]
                ->GetDefiniteSites().size();
            int ps_pos = results[pattern->GetEnzymeIndex()]
                ->GetPossibleSites().size();

            // the next possible (starting) position to check
            int next_pos = 0;

            // iterate over ambiguous positions
            ITERATE (vector<int>, pos, ambig_nucs) {
                int begin_check = *pos - pat.size() + 1;
                // don't try to check a negative position
                begin_check = max(begin_check, 0);
                // to avoid recording a site multiple times
                // when there are two nearby ambiguities
                begin_check = max(begin_check, next_pos);
                int end_check = min(*pos, (int) (seq.size() - pat.size()));
                int i;
                for (i = begin_check;  i <= end_check;  i++) {
                    CSeqMatch::EMatch match
                        = CSeqMatch::MatchNcbi8na(seq, pat, i);
                    if (match == CSeqMatch::eNo) {
                        continue;  // no match of any sort
                    }
                    // otherwise, a definite or possible site
                    CRSite site(i, i + pat.size() -1);

                    // figure out cleavage locations
                    const vector<int>& plus_cuts 
                        = enzymes[pattern->GetEnzymeIndex()]
                        .GetSpecs()[pattern->GetSpecIndex()].GetPlusCuts();
                    ITERATE (vector<int>, cut, plus_cuts) {
                        if (pattern->IsComplement()) {
                            site.SetPlusCuts()
                                .push_back(i + pattern->GetPattern().size()
                                           - *cut);
                        } else {
                            site.SetPlusCuts().push_back(i + *cut);
                        }
                    }

                    const vector<int>& minus_cuts 
                        = enzymes[pattern->GetEnzymeIndex()]
                        .GetSpecs()[pattern->GetSpecIndex()]
                        .GetMinusCuts();
                    ITERATE (vector<int>, cut, minus_cuts) {
                        if (pattern->IsComplement()) {
                            site.SetMinusCuts()
                                .push_back(i + pattern->GetPattern().size()
                                           - *cut);
                        } else {
                            site.SetMinusCuts().push_back(i + *cut);
                        }
                    }


                    if (match == CSeqMatch::eYes) {
                        results[pattern->GetEnzymeIndex()]
                            ->SetDefiniteSites().push_back(site);
                    } else {
                        results[pattern->GetEnzymeIndex()]
                            ->SetPossibleSites().push_back(site);
                    }
                }
                next_pos = i;
            }  // end ITERATE over ambiguous positions
            
            // definite_sites and possible_sites may be out of order
            vector<CRSite>& def_sites = results[pattern->GetEnzymeIndex()]
                ->SetDefiniteSites();
            inplace_merge(def_sites.begin(),
                          def_sites.begin() + ds_pos,
                          def_sites.end(),
                          SCompareLocation());
            
            vector<CRSite>& pos_sites = results[pattern->GetEnzymeIndex()]
                ->SetPossibleSites();
            inplace_merge(pos_sites.begin(),
                          pos_sites.begin() + ps_pos,
                          pos_sites.end(),
                          SCompareLocation());
            
        }  // end ITERATE over patterns
    }  // end if (!ambig_nucs.empty())
}


// CFindRSites::Find for various sequence containers

void CFindRSites::Find(const string& seq,
                       const vector<CREnzyme>& enzymes,
                       vector<CRef<CREnzResult> >& results)
{
    x_FindRSite(seq, enzymes, results);
}


void CFindRSites::Find(const vector<char>& seq,
                       const vector<CREnzyme>& enzymes,
                       vector<CRef<CREnzResult> >& results)
{
    x_FindRSite(seq, enzymes, results);
}


void CFindRSites::Find(const CSeqVector& seq,
                       const vector<CREnzyme>& enzymes,
                       vector<CRef<CREnzResult> >& results)
{
    string seq_ncbi8na;
    CSeqVector vec(seq);
    vec.SetNcbiCoding();
    vec.GetSeqData(0, vec.size(), seq_ncbi8na);
    x_FindRSite(seq_ncbi8na, enzymes, results);
}



END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2004/05/21 21:41:04  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.10  2003/08/28 15:49:43  jcherry
 * Fixed problem with x_FindRSite; pattern was being searched before
 * it was read.
 *
 * Revision 1.9  2003/08/22 14:25:58  ucko
 * Fix for MSVC, which seems to have problems with member templates.
 *
 * Revision 1.8  2003/08/22 02:17:18  ucko
 * Fix WorkShop compilation.
 *
 * Revision 1.7  2003/08/21 20:05:59  dicuccio
 * Added USING_SCOPE(objects) for MSVC
 *
 * Revision 1.6  2003/08/21 19:22:47  jcherry
 * Moved restriction site finding to algo/sequence
 *
 * Revision 1.5  2003/08/21 18:38:31  jcherry
 * Overloaded CFindRSites::Find to take several sequence containers.
 * Added option to lump together enzymes with identical specificities.
 *
 * Revision 1.4  2003/08/21 12:03:07  dicuccio
 * Make use of new typedef in plugin_utils.hpp for argument values.
 *
 * Revision 1.3  2003/08/20 22:57:44  jcherry
 * Reimplemented restriction site finding using finite state machine
 *
 * Revision 1.2  2003/08/17 19:25:30  jcherry
 * Changed member variable names to follow convention
 *
 * Revision 1.1  2003/08/12 18:52:58  jcherry
 * Initial version
 *
 * ===========================================================================
 */

