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
#include <algorithm>
#include <corelib/ncbistd.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/Rsite_ref.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>

#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>

#include <algo/sequence/restriction.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

NCBI_PARAM_DECL(string, RESTRICTION_SITES, REBASE);
NCBI_PARAM_DEF (string, RESTRICTION_SITES, REBASE, "");
typedef NCBI_PARAM_TYPE(RESTRICTION_SITES, REBASE) TRebaseData;


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


void CREnzyme::CombineIsoschizomers(TEnzymes& enzymes)
{
    stable_sort(enzymes.begin(), enzymes.end(), SCompareSpecs());
    TEnzymes result;
    ITERATE (TEnzymes, enzyme, enzymes) {
        if (enzyme != enzymes.begin() &&
            enzyme->GetSpecs() == result.back().GetSpecs()) {
            result.back().SetName() += "/";
            result.back().SetName() += enzyme->GetName();
        } else {
            result.push_back(*enzyme);
            result.back().SetPrototype();
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


CRSpec CRebase::MakeRSpec(const string& site)
{
    // site contains a string such as
    // GACGTC, GACGT^C, GCAGC(8/12), or (8/13)GAGNNNNNCTC(13/8)

    CRSpec spec;

    int plus_cut, minus_cut;

    string s = site;
    if (s[0] == '(') {
        string::size_type idx = s.find_first_of(")");
        if (idx == std::string::npos) {
            throw runtime_error(string("Error parsing site ")
                                + site);
        }
        x_ParseCutPair(s.substr(0, idx + 1), plus_cut, minus_cut);
        s.erase(0, idx + 1);
        spec.SetPlusCuts().push_back(-plus_cut);
        spec.SetMinusCuts().push_back(-minus_cut);
    }
    if (s[s.length() - 1] == ')') {
        string::size_type idx = s.find_last_of("(");
        if (idx == std::string::npos) {
            throw runtime_error(string("Error parsing site ")
                                + site);
        }
        x_ParseCutPair(s.substr(idx), plus_cut, minus_cut);
        s.erase(idx);
        spec.SetPlusCuts().push_back(plus_cut + s.length());
        spec.SetMinusCuts().push_back(minus_cut + s.length());
    }
    for (unsigned int i = 0;  i < s.length();  i++) {
        if (s[i] == '^') {

            // This should be simple.  If we have
            // G^GATCC, the cut on the plus strand is
            // at 1, and on the minus it's at the
            // symmetric position, 5.
            // The complication is that TspRI
            // is given as CASTGNN^, not NNCASTGNN^.
            // Someone someday might write NNCASTGNN^,
            // and something like ^NNGATC might arise,
            // so code defensively.

            s.erase(i, 1);
            int plus_cut = i;

            // this is the slightly complicated bit
            // trim any leading and trailing N's;
            // in case leading N's removed, adjust plus_cut accordingly
            string::size_type idx = s.find_first_not_of("N");
            if (idx == string::npos) {
                // bizarre situation; all N's (possibly empty)
                s.erase();
                plus_cut = 0;
            } else {
                s.erase(0, idx);
                plus_cut -= idx;
            }
            idx = s.find_last_not_of("N");
            s.erase(idx + 1);

            // plus strand cut
            spec.SetPlusCuts().push_back(plus_cut);
            // symmetric cut on minus strand
            spec.SetMinusCuts().push_back(s.length() - plus_cut);
            break;  // there better be just one '^'
        }
    }
    spec.SetSeq(s);
    return spec;
}


CREnzyme CRebase::MakeREnzyme(const string& name, const string& sites)
{

    CREnzyme re;

    re.SetName(name);

    vector<string> site_vec;
    NStr::Tokenize(sites, ",", site_vec);
    ITERATE (vector<string>, iter, site_vec) {
        CRSpec spec = CRebase::MakeRSpec(*iter);
        re.SetSpecs().push_back(spec);
    }

    return re;
}


string CRebase::GetDefaultDataPath()
{
    return TRebaseData::GetDefault();
}


void CRebase::x_ParseCutPair(const string& s, int& plus_cut, int& minus_cut)
{
    // s should look like "(8/13)"; plus_cut gets set to 8 and minus_cut to 13
    list<string> l;
    NStr::Split(s.substr(1, s.length() - 2), "/", l, NStr::fSplit_Tokenize);
    if (l.size() != 2) {
        throw runtime_error(string("Couldn't parse cut locations ")
                            + s);
    }
    plus_cut = NStr::StringToInt(l.front());
    minus_cut = NStr::StringToInt(l.back());
}


void CRebase::ReadNARFormat(istream& input,
                            TEnzymes& enzymes,
                            enum EEnzymesToLoad which)
{
    string line;
    CREnzyme enzyme;
    string name;
    TEnzymes::size_type prototype_idx(0);

    while (getline(input, line)) {
        vector<string> fields;
        NStr::Tokenize(line, "\t", fields);
        // the lines we're interested in have more than one field
        if (fields.size() < 2) {
            continue;
        }
        // name of enzyme is in field one or two (field zero is empty)
        name = fields[1];
        bool is_prototype(true);
        if (name == " ") {
            if (which == ePrototype) {
                continue;  // skip this non-protype
            }
            is_prototype = false;
            name = fields[2];
        }
        if (which == eCommercial && fields[5].empty()) {
            continue;  // skip this commercially unavailable enzyme
        }
        string sites = fields[3];  // this contains a comma-separted list of sites,
                            // usually just one (but TaqII has two)
        enzyme = MakeREnzyme(name, sites);
        enzymes.push_back(enzyme);

        // Link isoschizomers with their prototypes.
        if (is_prototype) {
            prototype_idx = enzymes.size();
        } else if (prototype_idx) {
            CREnzyme& prototype = enzymes[prototype_idx - 1];
            enzyme.SetPrototype(prototype.GetName());
            prototype.SetIsoschizomers().push_back(name);
        }
    }
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
    CPatternRec(string pattern, int enzyme_index, int spec_index,
                ENa_strand strand,
                unsigned int fsm_pat_size) : m_Pattern(pattern),
                                             m_EnzymeIndex(enzyme_index),
                                             m_SpecIndex(spec_index),
                                             m_Strand(strand),
                                             m_FsmPatSize(fsm_pat_size) {}
    // member access
    const string& GetPattern(void) const {return m_Pattern;}
    int GetEnzymeIndex(void) const {return m_EnzymeIndex;}
    int GetSpecIndex(void) const {return m_SpecIndex;}
    ENa_strand GetStrand(void) const {return m_Strand;}
    unsigned int GetFsmPatSize(void) const {return m_FsmPatSize;}
private:
    // pattern in ncbi8na
    string m_Pattern;
    // which enzyme and specificity we represent
    int m_EnzymeIndex;
    int m_SpecIndex;
    // whether we represent the complement of the specificity
    ENa_strand m_Strand;
    unsigned int m_FsmPatSize;
};


// helper functor for sorting CRef<CREnzResult> by the enzyme name
struct SEnzymeNameCompare
{
    bool operator()
        (const CRef<CREnzResult>& lhs, const CRef<CREnzResult>& rhs) const
    {
        return lhs->GetEnzymeName() < rhs->GetEnzymeName();
    }
};


// helper functor for sorting CREnzyme by the enzyme name
struct SNameCompare
{
    bool operator()
        (const CREnzyme& lhs, const CREnzyme& rhs) const
    {
        return lhs.GetName() < rhs.GetName();
    }
};


// helper functor for sorting by the number of definite sites
struct SLessDefSites
{
    bool operator()
        (const CRef<CREnzResult>& lhs, const CRef<CREnzResult>& rhs) const
    {
        return lhs->GetDefiniteSites().size() < rhs->GetDefiniteSites().size();
    }
};


// helper functor for sorting CRef<CSeq_loc>s by location
struct SLessSeq_loc
{
    bool operator()
        (const CRef<CSeq_loc>& lhs, const CRef<CSeq_loc>& rhs) const
    {
        return (lhs->Compare(*rhs) < 0);
    }
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


CFindRSites::CFindRSites(const string& refile,
                         CRebase::EEnzymesToLoad which_enzymes,
                         TFlags flags)
    : m_Flags(flags)
{
    x_LoadREnzymeData(refile.empty()
                          ? CRebase::GetDefaultDataPath()
                          : refile,
                      which_enzymes);
}


const CFindRSites::TEnzymes& CFindRSites::GetEnzymes()
{
    return m_Enzymes;
}


CFindRSites::TEnzymes& CFindRSites::SetEnzymes()
{
    return m_Enzymes;
}


void CFindRSites::x_LoadREnzymeData(const string& refile,
                                    CRebase::EEnzymesToLoad which_enzymes)
{
    if (! refile.empty()) {
        ifstream istr(refile.c_str());
        CRebase::ReadNARFormat(istr, m_Enzymes, which_enzymes);

        if (m_Flags & fCombineIsoschizomers) {
            // first sort alphabetically by enzyme name
            sort(m_Enzymes.begin(), m_Enzymes.end(), SNameCompare());
            // now combine isoschizomers
            CREnzyme::CombineIsoschizomers(m_Enzymes);
        }
    }
}


// remap a child location to a parent
static CRef<CSeq_loc> s_RemapChildToParent(const CSeq_loc& parent,
                                           const CSeq_loc& child,
                                           CScope* scope)
{
    CSeq_loc dummy_parent;
    dummy_parent.SetWhole(const_cast<CSeq_id&>(sequence::GetId(parent, 0)));
    SRelLoc converter(dummy_parent, child, scope, SRelLoc::fNoMerge);
    converter.m_ParentLoc = &parent;
    return converter.Resolve(scope);
}

static
void s_AddSitesToAnnot(const vector<CRSite>& sites,
                       const CREnzResult&    result,
                       CSeq_annot&           annot,
                       CScope&               scope,
                       const CSeq_loc&       parent_loc,
                       bool                  definite = true)
{
    const CSeq_id& id = sequence::GetId(parent_loc, 0);

    ITERATE (vector<CRSite>, site, sites) {
        // create feature
        CRef<CSeq_feat> feat(new CSeq_feat());

        // start to set up Rsite
        feat->SetData().SetRsite().SetDb().SetDb("REBASE");
        feat->SetData().SetRsite().SetDb()
            .SetTag().SetStr("REBASE");

        string str(result.GetEnzymeName());
        if ( !definite ) {
            str = "Possible " + str;
        }
        feat->SetData().SetRsite().SetStr(str);

        //
        // build our location
        //

        vector< CRef<CSeq_loc> > locs;

        // a loc for the recognition site
        CRef<CSeq_loc> recog_site(new CSeq_loc);
        recog_site->SetInt().SetFrom(site->GetStart());
        recog_site->SetInt().SetTo  (site->GetEnd());
        recog_site->SetInt().SetStrand(site->GetStrand());
        recog_site->SetId(id);
        locs.push_back(recog_site);

        ENa_strand cut_strand =
            IsReverse(site->GetStrand()) ? eNa_strand_minus
                                         : eNa_strand_plus;

        // locs for the cleavage sites
        int negative_cut_locs = 0;  // count these exceptions
        ITERATE (vector<int>, cut, site->GetPlusCuts()) {
            if (*cut >= 0 ) {
                CRef<CSeq_loc> cut_site(new CSeq_loc);
                cut_site->SetPnt().SetPoint(*cut);
                // indicate that the cut is to the "left"
                cut_site->SetPnt()
                    .SetFuzz().SetLim(CInt_fuzz::eLim_tl);
                cut_site->SetPnt().SetStrand(cut_strand);
                cut_site->SetId(id);
                locs.push_back(cut_site);
            } else {
                negative_cut_locs++;
            }
        }

        ITERATE (vector<int>, cut, site->GetMinusCuts()) {
            if (*cut >= 0 ) {
                CRef<CSeq_loc> cut_site(new CSeq_loc);
                cut_site->SetPnt().SetPoint(*cut);
                // indicate that the cut is to the "left"
                cut_site->SetPnt()
                    .SetFuzz().SetLim(CInt_fuzz::eLim_tl);
                cut_site->SetPnt().SetStrand(Reverse(cut_strand));
                cut_site->SetId(id);
                locs.push_back(cut_site);
            } else {
                negative_cut_locs++;
            }
        }

        // comment for those few cases where there are
        // cuts before the sequence begins
        if (negative_cut_locs > 0) {
            string a_comm = NStr::IntToString(negative_cut_locs)
                + " cleavage sites are located before the"
                " beginning of the sequence and are not reported";
            feat->SetComment(a_comm);
        }

        sort(locs.begin(), locs.end(), SLessSeq_loc());
        copy(locs.begin(), locs.end(),
             back_inserter(feat->SetLocation().SetMix().Set()));

        CRef<CSeq_loc> mapped =
            s_RemapChildToParent(parent_loc, feat->GetLocation(), &scope);
        feat->SetLocation(*mapped);

        // save in annot
        annot.SetData().SetFtable().push_back(feat);
    }
}


CFindRSites::TAnnot
CFindRSites::GetAnnot(CScope& scope, const CSeq_loc& loc) const
{
    CTime now(CTime::eCurrent);
    // get sequence in binary (8na) form
    CSeqVector vec(loc, scope, CBioseq_Handle::eCoding_Ncbi);

    // a place to store results (one per enzyme)
    typedef vector<CRef<CREnzResult> > TResults;
    TResults results;

    // do the big search
    Find(vec, m_Enzymes, results, m_Flags);

    // deal with stored enzyme results

    sort(results.begin(), results.end(), SEnzymeNameCompare());

    if (m_Flags & fSortByNumberOfSites) {
        // do a stablesort so alphabetical order preserved
        // within sets with the same number of sites
        stable_sort(results.begin(), results.end(), SLessDefSites());
    }

    TAnnot annot;
    int total_definite_sites = 0, total_possible_sites = 0;
    int total_non_cutters = 0;

    ITERATE (TResults, result, results) {
        const vector<CRSite>& definite_sites =
            (*result)->GetDefiniteSites();
        const vector<CRSite>& possible_sites =
            (*result)->GetPossibleSites();

        int count_definite_sites = definite_sites.size();
        int count_possible_sites = possible_sites.size();

        if (count_definite_sites  ||  count_possible_sites) {
            total_definite_sites += count_definite_sites;
            total_possible_sites += count_possible_sites;

            if (m_Flags & fSplitAnnotsByEnzyme  ||  annot.empty()) {
                CRef<CSeq_annot> new_annot(new CSeq_annot);
                // add description to annot
                const string title("Restriction sites");
                new_annot->SetNameDesc(title);
                new_annot->SetTitleDesc(title);
                new_annot->SetCreateDate(now);
                CRef<CAnnotdesc> region(new CAnnotdesc);
                region->SetRegion().Assign(loc);
                new_annot->SetDesc().Set().push_back(region);
                annot.push_back(new_annot);
            }
            CSeq_annot& curr_annot(*annot.back());

            //
            // add features to annot
            //
            s_AddSitesToAnnot(definite_sites,
                              **result, curr_annot, scope, loc);
            s_AddSitesToAnnot(possible_sites,
                              **result, curr_annot, scope, loc, false);
        } else {
            ++total_non_cutters;
        }
    }

    /**
    // in order to build dialog efficiently,
    // pre-allocate rows
    int total_rows = total_definite_sites + total_possible_sites
        + total_non_cutters;
        **/

    _TRACE("Found " << total_definite_sites << " definite and "
           << total_possible_sites << " possible sites");

    return annot;
}


CFindRSites::TAnnot
CFindRSites::GetAnnot(CBioseq_Handle bsh) const
{
    CSeq_id_Handle idh = sequence::GetId(bsh, sequence::eGetId_Canonical);
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetWhole().Assign(*idh.GetSeqId());
    return GetAnnot(bsh.GetScope(), *loc);
}


void CFindRSites::x_AddPattern(const string& pat, CTextFsm<int>& fsm,
                               int match_value)
{
    string s = pat;
    x_ExpandRecursion(s, 0, fsm, match_value);
}


bool CFindRSites::x_IsAmbig (char nuc)
{
    static const bool ambig_table[16] = {
        0, 0, 0, 1, 0, 1, 1, 1,
        0, 1, 1, 1, 1, 1, 1, 1
    };
    return ambig_table[(size_t) nuc];
}


/// Find all definite and possible sites in a sequence
/// for a vector of enzymes, using a finite state machine.
/// Templatized so that seq can be various containers
/// (e.g., string, vector<char>, CSeqVector),
/// but it must yield ncbi8na.
template<class Seq>
void x_FindRSite(const Seq& seq, const CFindRSites::TEnzymes& enzymes,
                 vector<CRef<CREnzResult> >& results,
                 CFindRSites::TFlags flags)
{

    results.clear();
    results.reserve(enzymes.size());

    // vector of patterns for internal use
    vector<CPatternRec> patterns;
    patterns.reserve(enzymes.size());  // an underestimate
    
    // the finite state machine for the search
    CTextFsm<int> fsm;

    // iterate over enzymes
    ITERATE (CFindRSites::TEnzymes, enzyme, enzymes) {
        // Unless isoschizomers are requested, skip them.
        if (! (flags & CFindRSites::fFindIsoschizomers  ||
               enzyme->IsPrototype())) {
            continue;
        }

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
            // Check if the pattern is pallindromic.
            string comp = pat;
            CSeqMatch::CompNcbi8na(comp);
            ENa_strand strand =
                (comp == pat) ? eNa_strand_both : eNa_strand_plus;

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
                                           strand,
                                           fsm_pat_size));
            // add pattern to fsm
            // (add only fsm_pat_size of it)
            CFindRSites::x_AddPattern(pat.substr(0, fsm_pat_size), fsm,
                                      patterns.size() - 1);

            // If not palidromic do a search for its complement too
            if (strand != eNa_strand_both) {
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
                                               Reverse(strand),
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
                const CREnzyme& enzyme =
                    enzymes[pattern.GetEnzymeIndex()];
                const CRSpec& spec =
                    enzyme.GetSpecs()[pattern.GetSpecIndex()];

                CRSite site(begin_pos, end_pos);
                site.SetStrand(pattern.GetStrand());
                const vector<int>& plus_cuts = spec.GetPlusCuts();
                ITERATE (vector<int>, cut, plus_cuts) {
                    if (IsReverse(pattern.GetStrand())) {
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
                    if (IsReverse(pattern.GetStrand())) {
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
                    site.SetStrand(pattern->GetStrand());

                    // figure out cleavage locations
                    const vector<int>& plus_cuts 
                        = enzymes[pattern->GetEnzymeIndex()]
                        .GetSpecs()[pattern->GetSpecIndex()].GetPlusCuts();
                    ITERATE (vector<int>, cut, plus_cuts) {
                        if (IsReverse(pattern->GetStrand())) {
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
                        if (IsReverse(pattern->GetStrand())) {
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
                    } else if (flags & CFindRSites::fFindPossibleSites) {
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
                       const TEnzymes& enzymes,
                       vector<CRef<CREnzResult> >& results,
                       TFlags flags)
{
    x_FindRSite(seq, enzymes, results, flags);
}


void CFindRSites::Find(const vector<char>& seq,
                       const TEnzymes& enzymes,
                       vector<CRef<CREnzResult> >& results,
                       TFlags flags)
{
    x_FindRSite(seq, enzymes, results, flags);
}


void CFindRSites::Find(const CSeqVector& seq,
                       const TEnzymes& enzymes,
                       vector<CRef<CREnzResult> >& results,
                       TFlags flags)
{
    string seq_ncbi8na;
    CSeqVector vec(seq);
    vec.SetNcbiCoding();
    vec.GetSeqData(0, vec.size(), seq_ncbi8na);
    x_FindRSite(seq_ncbi8na, enzymes, results, flags);
}



END_NCBI_SCOPE
