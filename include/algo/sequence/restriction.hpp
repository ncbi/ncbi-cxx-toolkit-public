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


#ifndef ALGO_SEQUENCE___RESTRICTION__HPP
#define ALGO_SEQUENCE___RESTRICTION__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <algo/sequence/seq_match.hpp>
#include <util/strsearch.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

///
/// This class represents a particular occurrence of a restriction
/// site on a sequence (not to be confused with a CRSpec, which
/// represents a *type* of restriction site).
/// Contains the locations of beginning and end of recognition site,
/// and vectors of cut sites on plus and minus strands.
///

class NCBI_XALGOSEQ_EXPORT CRSite
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
    vector<int>&       SetPlusCuts(void);
    const vector<int>& GetPlusCuts(void) const;

    vector<int>&       SetMinusCuts(void);
    const vector<int>& GetMinusCuts(void) const;
private:
    int m_Start;
    int m_End;
    vector<int> m_PlusCuts;
    vector<int> m_MinusCuts;
};

NCBI_XALGOSEQ_EXPORT ostream& operator<<(ostream& os, const CRSite& site);


///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////

inline
CRSite::CRSite(int start, int end)
{
    m_Start = start;
    m_End = end;
}


inline
vector<int>& CRSite::SetPlusCuts(void)
{
    return m_PlusCuts;
}


inline
const vector<int>& CRSite::GetPlusCuts(void) const
{
    return m_PlusCuts;
}


inline
vector<int>& CRSite::SetMinusCuts(void)
{
    return m_MinusCuts;
}


inline
const vector<int>& CRSite::GetMinusCuts(void) const
{
    return m_MinusCuts;
}


inline
void CRSite::SetStart(int pos)
{
    m_Start = pos;
}


inline
int CRSite::GetStart(void) const
{
    return m_Start;
}


inline
void CRSite::SetEnd(int pos)
{
    m_End = pos;
}


inline
int CRSite::GetEnd(void) const
{
    return m_End;
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

class NCBI_XALGOSEQ_EXPORT CRSpec
{
public:
    // recognition sequence
    void SetSeq(const string& s);
    string& SetSeq(void);
    const string& GetSeq(void) const;

    // cleavage locations
    // 0 is the bond just before the recognition sequence
    vector<int>&       SetPlusCuts(void);
    const vector<int>& GetPlusCuts(void) const;
    vector<int>&       SetMinusCuts(void);
    const vector<int>& GetMinusCuts(void) const;

    // compare
    bool operator==(const CRSpec& rhs) const {
        return m_Seq == rhs.m_Seq 
            && m_PlusCuts == rhs.m_PlusCuts
            && m_MinusCuts == rhs.m_MinusCuts;
    }
    bool operator!=(const CRSpec& rhs) const {
        return !(*this == rhs);
    }
    bool operator<(const CRSpec& rhs) const;

    // reset everything
    void Reset(void);
private:
    string m_Seq;
    vector<int> m_PlusCuts;
    vector<int> m_MinusCuts;
};


///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////


inline
void CRSpec::SetSeq(const string& s)
{
    m_Seq = s;
}


inline
string& CRSpec::SetSeq(void)
{
    return m_Seq;
}


inline
const string& CRSpec::GetSeq(void) const
{
    return m_Seq;
}


inline
vector<int>& CRSpec::SetPlusCuts(void)
{
    return m_PlusCuts;
}


inline
const vector<int>& CRSpec::GetPlusCuts(void) const
{
    return m_PlusCuts;
}


inline
vector<int>& CRSpec::SetMinusCuts(void)
{
    return m_MinusCuts;
}


inline
const vector<int>& CRSpec::GetMinusCuts(void) const
{
    return m_MinusCuts;
}


///
/// This class represents a restriction enzyme
/// (an enzyme name and a vector of cleavage specificities)
///

class NCBI_XALGOSEQ_EXPORT CREnzyme
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

    // Given a vector of CREnzyme, lump together all
    // enzymes with identical specificities.
    // The cleavage sites must be the same for specificities
    // to be considered indentical (in addition to the
    // recognition sequenence).
    static void CombineIsoschizomers(vector<CREnzyme>& enzymes);

private:
    string m_Name;
    vector<CRSpec> m_Specs;
};


///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////

inline
void CREnzyme::SetName(const string& s)
{
    m_Name = s;
}


inline
string& CREnzyme::SetName(void)
{
    return m_Name;
}


inline
const string& CREnzyme::GetName(void) const
{
    return m_Name;
}


inline
vector<CRSpec>& CREnzyme::SetSpecs(void)
{
    return m_Specs;
}


inline
const vector<CRSpec>& CREnzyme::GetSpecs(void) const
{
    return m_Specs;
}


///
/// This class represents the results of a search for sites
/// of a particular enzyme.
/// It merely packages an enzyme name, a vector of
/// definite sites, and a vector of possible sites
///

class CREnzResult : public CObject
{
public:
    CREnzResult(const string& enzyme_name) : m_EnzymeName(enzyme_name) {}
    CREnzResult(const string& enzyme_name,
                const vector<CRSite>& definite_sites,
                const vector<CRSite>& possible_sites);
    // member access functions
    const string& GetEnzymeName(void) const {return m_EnzymeName;}
    vector<CRSite>& SetDefiniteSites(void) {return m_DefiniteSites;}
    const vector<CRSite>& GetDefiniteSites(void) const
    {
        return m_DefiniteSites;
    }
    vector<CRSite>& SetPossibleSites(void) {return m_PossibleSites;}
    const vector<CRSite>& GetPossibleSites(void) const
    {
        return m_PossibleSites;
    }

private:
    string m_EnzymeName;
    vector<CRSite> m_DefiniteSites;
    vector<CRSite> m_PossibleSites;
};

NCBI_XALGOSEQ_EXPORT ostream& operator<<(ostream& os, const CREnzResult& er);


///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////

inline
CREnzResult::CREnzResult(const string& enzyme_name,
                         const vector<CRSite>& definite_sites,
                         const vector<CRSite>& possible_sites)
{
    m_EnzymeName = enzyme_name;
    m_DefiniteSites = definite_sites;
    m_PossibleSites = possible_sites;
}


/// this class contains the static member functions Find,
/// which find restriction sites in a sequence
class NCBI_XALGOSEQ_EXPORT CFindRSites
{
public:
    static void Find(const string& seq,
                     const vector<CREnzyme>& enzymes,
                     vector<CRef<CREnzResult> >& results);
    static void Find(const vector<char>& seq,
                     const vector<CREnzyme>& enzymes,
                     vector<CRef<CREnzResult> >& results);
    static void Find(const CSeqVector& seq,
                     const vector<CREnzyme>& enzymes,
                     vector<CRef<CREnzResult> >& results);

private:
    static void x_ExpandRecursion(string& s, unsigned int pos,
                                  CTextFsm<int>& fsm, int match_value);
    static void x_AddPattern(const string& pat, CTextFsm<int>& fsm,
                             int match_value);
    static bool x_IsAmbig(char nuc);

    template<class Seq>
    friend void x_FindRSite(const Seq& seq, const vector<CREnzyme>& enzymes,
                            vector<CRef<CREnzResult> >& results);
};



END_NCBI_SCOPE

#endif   // ALGO_SEQUENCE___RESTRICTION__HPP
