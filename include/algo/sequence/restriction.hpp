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


#ifndef GUI_CORE_ALGO_BASIC___RESTRICTION__HPP
#define GUI_CORE_ALGO_BASIC___RESTRICTION__HPP

#include <corelib/ncbistd.hpp>
#include "seq_match.hpp"


BEGIN_NCBI_SCOPE


///
/// This class represents a particular occurrence of a restriction
/// site on a sequence (not to be confused with a CRSpec, which
/// represents a *type* of restriction site).
/// Contains the locations of beginning and end of recognition site,
/// and vectors of cut sites on plus and minus strands.
///

class CRSite
{
public:
    CRSite(int start, int end);
    // location of recognition sequence
    void SetStart(const int pos);
    int GetStart(void) const;
    void SetEnd(const int pos);
    int GetEnd(void) const;
    // cleavage locations
    // 0 is the bond just before the recognition sequence
    vector<int>& SetPlusCuts(void);
    const vector<int>& GetPlusCuts(void) const;
    vector<int>& SetMinusCuts(void);
    const vector<int>& GetMinusCuts(void) const;
private:
    int m_start;
    int m_end;
    vector<int> m_plus_cuts;
    vector<int> m_minus_cuts;
};


///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////

inline
CRSite::CRSite(int start, int end)
{
    m_start = start;
    m_end = end;
}


inline
vector<int>& CRSite::SetPlusCuts(void)
{
    return m_plus_cuts;
}


inline
const vector<int>& CRSite::GetPlusCuts(void) const
{
    return m_plus_cuts;
}


inline
vector<int>& CRSite::SetMinusCuts(void)
{
    return m_minus_cuts;
}


inline
const vector<int>& CRSite::GetMinusCuts(void) const
{
    return m_minus_cuts;
}


inline
void CRSite::SetStart(int pos)
{
    m_start = pos;
}


inline
int CRSite::GetStart(void) const
{
    return m_start;
}


inline
void CRSite::SetEnd(int pos)
{
    m_end = pos;
}


inline
int CRSite::GetEnd(void) const
{
    return m_end;
}



///
/// This class represents a restriction enzyme specificity,
/// i.e., a sequence recognition pattern and vectors of cleavage
/// sites on the two strands.
/// Some known enzymes (e.g., BaeI) have two cleavage sites on each
/// strand.  Some will be represented as having zero because the
/// cut locations are unknown.
/// An enzyme may have more than one specificity (TaqII).
///

class CRSpec
{
public:
    // recognition sequence
    void SetSeq(const string& s);
    string& SetSeq(void);
    const string& GetSeq(void) const;
    // cleavage locations
    // 0 is the bond just before the recognition sequence
    vector<int>& SetPlusCuts(void);
    const vector<int>& GetPlusCuts(void) const;
    vector<int>& SetMinusCuts(void);
    const vector<int>& GetMinusCuts(void) const;
    // reset everything
    void Reset(void);
private:
    string m_seq;
    vector<int> m_plus_cuts;
    vector<int> m_minus_cuts;
};


///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////


inline
void CRSpec::SetSeq(const string& s)
{
    m_seq = s;
}


inline
string& CRSpec::SetSeq(void)
{
    return m_seq;
}


inline
const string& CRSpec::GetSeq(void) const
{
    return m_seq;
}


inline
vector<int>& CRSpec::SetPlusCuts(void)
{
    return m_plus_cuts;
}


inline
const vector<int>& CRSpec::GetPlusCuts(void) const
{
    return m_plus_cuts;
}


inline
vector<int>& CRSpec::SetMinusCuts(void)
{
    return m_minus_cuts;
}


inline
const vector<int>& CRSpec::GetMinusCuts(void) const
{
    return m_minus_cuts;
}


///
/// This class represents a restriction enzyme
/// (an enzyme name and a vector of cleavage specificities)
///

class CREnzyme
{
public:
    // name of enzyme
    void SetName(const string& s);
    string& SetName(void);
    const string& GetName(void) const;
    // cleavage specificities
    // (usually just one, but TaqII has two)
    vector<CRSpec>& SetSpecs(void);
    const vector<CRSpec>& GetSpecs(void) const;
    // reset everything
    void Reset(void);
private:
    string m_name;
    vector<CRSpec> m_specs;
};


///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////

inline
void CREnzyme::SetName(const string& s)
{
    m_name = s;
}


inline
string& CREnzyme::SetName(void)
{
    return m_name;
}


inline
const string& CREnzyme::GetName(void) const
{
    return m_name;
}


inline
vector<CRSpec>& CREnzyme::SetSpecs(void)
{
    return m_specs;
}


inline
const vector<CRSpec>& CREnzyme::GetSpecs(void) const
{
    return m_specs;
}


///
/// This class represents the results of a search for sites
/// of a particular enzyme.
/// It merely packages an enzyme name, a vector of
/// definite sites, and a vector of possible sites
///

class CREnzResult
{
public:
    CREnzResult(const string& enzyme_name,
                const vector<CRSite> definite_sites,
                const vector<CRSite> possible_sites);
    // member access functions
    const string& GetEnzymeName(void) const {return m_enzyme_name;}
    const vector<CRSite>& GetDefiniteSites(void) const {return m_definite_sites;}
    const vector<CRSite>& GetPossibleSites(void) const {return m_possible_sites;}

private:
    string m_enzyme_name;
    vector<CRSite> m_definite_sites;
    vector<CRSite> m_possible_sites;
};


///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////

inline
CREnzResult::CREnzResult(const string& enzyme_name,
                         const vector<CRSite> definite_sites,
                         const vector<CRSite> possible_sites)
{
    m_enzyme_name = enzyme_name;
    m_definite_sites = definite_sites;
    m_possible_sites = possible_sites;
}


struct SCompareLocation
{
    bool operator() (const CRSite& lhs, const CRSite& rhs) const
    {
        return lhs.GetStart() < rhs.GetStart();
    }
};


/// this class contains the static member function Find,
/// which finds restriction sites in a sequence
class CFindRSites
{
public:
    /// Find all definite and possible sites in a sequence
    /// for a vector of enzymes.
    /// Templatized so that both string and vector<char>
    /// will work for seq.
    template<class T>
    static void Find(const T& seq,
                     const vector<CREnzyme>& enzymes,
                     vector<CREnzResult>& results)
    {
        // iterate over enzymes
        ITERATE (vector<CREnzyme>, enzyme, enzymes) {
            vector<CRSite> definite_sites;
            vector<CRSite> possible_sites;
            const vector<CRSpec>& specs = enzyme->GetSpecs();
            // iterate over specificities for this enzyme
            ITERATE (vector<CRSpec>, spec, specs) {
                // do a search
                Find(seq, *spec, definite_sites, possible_sites);
            }

            // In the rare case that there's more than one
            // spec, the sites will be out of order.
            // Do a sort to fix this (an inplace_merge
            // after each iteration would probably be
            // more efficient)
            if (specs.size() > 1) {
                sort(definite_sites.begin(),
                     definite_sites.end(),
                     SCompareLocation());
                sort(possible_sites.begin(),
                     possible_sites.end(),
                     SCompareLocation());
            }

            // store the results for this enzyme
            results.push_back(CREnzResult(enzyme->GetName(),
                                          definite_sites,
                                          possible_sites));
        }
    }

    /// Find definite and possible sites for a single specificity
    template<class T>
    static void Find(const T& seq8na,
                     const CRSpec& spec,
                     vector<CRSite>& definite_sites,
                     vector<CRSite>& possible_sites)
    {
        string pattern;
        CSeqMatch::IupacToNcbi8na(spec.GetSeq(), pattern);
        vector<TSeqPos> definite_matches;
        vector<TSeqPos> possible_matches;
        CSeqMatch::FindMatchesNcbi8na(seq8na, pattern,
                               definite_matches, possible_matches);
        ITERATE (vector<TSeqPos>, match, definite_matches) {
            CRSite site(*match, *match + pattern.size() - 1);

            // figure out cleavage locations
            const vector<int>& plus_cuts = spec.GetPlusCuts();
            ITERATE (vector<int>, cut, plus_cuts) {
                site.SetPlusCuts().push_back(*match + *cut);
            }
            const vector<int>& minus_cuts = spec.GetMinusCuts();
            ITERATE (vector<int>, cut, minus_cuts) {
                site.SetMinusCuts().push_back(*match + *cut);
            }

            definite_sites.push_back(site);
        }
        ITERATE (vector<TSeqPos>, match, possible_matches) {
            CRSite site(*match, *match + pattern.size() - 1);

            // figure out cleavage locations
            const vector<int>& plus_cuts = spec.GetPlusCuts();
            ITERATE (vector<int>, cut, plus_cuts) {
                site.SetPlusCuts().push_back(*match + *cut);
            }
            const vector<int>& minus_cuts = spec.GetMinusCuts();
            ITERATE (vector<int>, cut, minus_cuts) {
                site.SetMinusCuts().push_back(*match + *cut);
            }

            possible_sites.push_back(site);
        }
    

        // if the pattern is not pallindromic,
        // do a search with its complement
        // FIX CUT SITE CALCULATION
        string comp = pattern;
        CSeqMatch::CompNcbi8na(comp);
        if (comp != pattern) {
            definite_matches.clear();
            possible_matches.clear();

            // for reordering later
            int ds_pos = definite_sites.size();
            int ps_pos = possible_sites.size();

            CSeqMatch::FindMatchesNcbi8na(seq8na, comp,
                                          definite_matches, possible_matches);
            ITERATE (vector<TSeqPos>, match, definite_matches) {
                CRSite site(*match, *match + pattern.size() - 1);

                // figure out cleavage locations
                const vector<int>& plus_cuts = spec.GetPlusCuts();
                ITERATE (vector<int>, cut, plus_cuts) {
                    site.SetPlusCuts().push_back(*match + comp.length() - *cut);
                }
                const vector<int>& minus_cuts = spec.GetMinusCuts();
                ITERATE (vector<int>, cut, minus_cuts) {
                    site.SetMinusCuts().push_back(*match + comp.length() - *cut);
                }

                definite_sites.push_back(site);
            }
            ITERATE (vector<TSeqPos>, match, possible_matches) {
                CRSite site(*match, *match + pattern.size() - 1);

                // figure out cleavage locations
                const vector<int>& plus_cuts = spec.GetPlusCuts();
                ITERATE (vector<int>, cut, plus_cuts) {
                    site.SetPlusCuts().push_back(*match + comp.length() - *cut);
                }
                const vector<int>& minus_cuts = spec.GetMinusCuts();
                ITERATE (vector<int>, cut, minus_cuts) {
                    site.SetMinusCuts().push_back(*match + comp.length() - *cut);
                }

                possible_sites.push_back(site);
            }

            // definite_sites and possible_sites are not correctly
            // ordered: all complementary matches come after all
            // 'forward' matches.  Do an in-place merge to
            // fix this

            inplace_merge(definite_sites.begin(),
                          definite_sites.begin() + ds_pos,
                          definite_sites.end(),
                          CompareLocation);

            inplace_merge(possible_sites.begin(),
                          possible_sites.begin() + ps_pos,
                          possible_sites.end(),
                          CompareLocation);
        }
    }
};



END_NCBI_SCOPE

#endif   // GUI_CORE_ALGO_BASIC___RESTRICTION__HPP


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/08/13 12:37:58  dicuccio
 * Partial compilation fixes for Windows
 *
 * Revision 1.1  2003/08/12 18:52:58  jcherry
 * Initial version
 *
 * ===========================================================================
 */
