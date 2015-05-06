#ifndef OBJTOOLS_READERS___SOURCE_MOD_PARSER__HPP
#define OBJTOOLS_READERS___SOURCE_MOD_PARSER__HPP

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
 * Authors:  Aaron Ucko, Jonathan Kans, Vasuki Gobi, Michael Kornbluh
 *
 */

/// @file source_mod_parser.hpp
/// Parser for source modifiers, as found in (Sequin-targeted) FASTA files.

#include <corelib/ncbi_autoinit.hpp>

#include <objects/general/User_object.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Prot_ref.hpp>

#include <objtools/readers/reader_exception.hpp>

#include <set>
#include <map>

/** @addtogroup Miscellaneous
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CBioseq;
// class CSubmit_block;

/////////////////////////////////////////////////////////////////////////////
///
/// CSourceModParser
///
/// Class for parsing source modifiers, as found in (Sequin-targeted) FASTA
/// files.
///
/// Some flat interchange formats (notably FASTA) limit annotations to a
/// single definition/title line per sequence, so NCBI developed a convention
/// for allowing them to contain bracketed modifiers of the form [key=value],
/// as documented at https://www.ncbi.nlm.nih.gov/Sequin/modifiers.html .

template<class _T>
class CAutoInitDesc;

class NCBI_XOBJREAD_EXPORT CSourceModParser
{
public:
    enum EHandleBadMod {
        // regardless of choice, bad mods are still put in the bad mods set,
        // although it may be incomplete if you exit early (e.g. from throw)
        eHandleBadMod_Ignore = 1, // behavior of C toolkit is to ignore
        eHandleBadMod_Throw,
        eHandleBadMod_PrintToCerr
    };
    CSourceModParser(EHandleBadMod handleBadMod = eHandleBadMod_Ignore) 
        : m_HandleBadMod(handleBadMod) { }

    /// Extract and store bracketed modifiers from a title string, returning a
    /// stripped version (which may well be empty at that point!)
    string ParseTitle(const CTempString& title, CConstRef<CSeq_id> seqid,
        size_t iMaxModsToParse = std::numeric_limits<size_t>::max() );

    /// Apply previously extracted modifiers to the given object, marking all
    /// relevant ones as used.
    void ApplyAllMods(CBioseq& seq, CTempString organism = kEmptyStr, CConstRef<CSeq_loc> location = CConstRef<CSeq_loc>());
    void ApplyMods(CBioSource& bsrc, CTempString organism = kEmptyStr);
    void ApplyMods(CMolInfo& mi);
    void ApplyMods(CBioseq& seq);
    void ApplyMods(CGene_ref& gene);
    void ApplyMods(CProt_ref& prot);
    void ApplyMods(CGB_block& gbb);
    void ApplyMods(CSeq_hist& hist);
    // void ApplyMods(CSubmit_block& sb);
    void ApplyTPAMods(CUser_object& tpa);
    void ApplyGenomeProjectsDBMods(CUser_object& gpdb);
    void ApplyPubMods(CSeq_descr& sd);

    static int CompareKeys(const CTempString& lhs, const CTempString& rhs);

    struct PKeyCompare {
        bool operator()(const CTempString& lhs, const CTempString& rhs) const
            { return CompareKeys(lhs, rhs) < 0; }
    };

    struct PKeyEqual {
        bool operator()(const CTempString& lhs, const CTempString& rhs) const
            { return CompareKeys(lhs, rhs) == 0; }
    };

    struct SMod {

        CConstRef<CSeq_id> seqid;
        string key;
        string value;
        size_t pos;
        bool   used;

        bool operator < (const SMod& rhs) const;
        string ToString(void) const;
    };
    typedef set<SMod>              TMods;
    typedef TMods::const_iterator  TModsCI;
    typedef pair<TModsCI, TModsCI> TModsRange;

    enum EWhichMods {
        fUsedMods   = 0x1,
        fUnusedMods = 0x2,
        fAllMods    = 0x3
    };
    typedef int TWhichMods; // binary OR of EWhichMods

    // class which may be thrown
    class CBadModError : public runtime_error {
    public:
        CBadModError( 
            const SMod & badMod, 
            const std::string & sAllowedValues );
        ~CBadModError() THROWS_NONE { } // required by GCC 4.6

        const SMod & GetBadMod() const { return m_BadMod; }
        const std::string & GetAllowedValues() const { return m_sAllowedValues; }
    private:
        SMod        m_BadMod;
        std::string m_sAllowedValues;

        std::string x_CalculateErrorString(
            const SMod & badMod, 
            const string & sAllowedValues );
    };

    /// Return all modifiers matching the given criteria (if any) without
    /// affecting their status (used vs. unused).
    TMods GetMods(TWhichMods which = fAllMods) const;

    /// If a modifier with either key is present, mark it as used and
    /// return it; otherwise, return NULL.
    const SMod* FindMod(const CTempString& key,
                        CTempString alt_key = kEmptyStr);

    /// Return all modifiers with the given key (e.g., db_xref), marking them
    /// as used along the way.
    TModsRange FindAllMods(const CTempString& key);

    /// Append a representation of the specified modifiers to s, with a space
    /// in between if s is not empty and doesn't already end with one.
    void GetLabel(string* s, TWhichMods which = fAllMods) const;

    /// Allows user to get the list of bad mods found by this
    const TMods & GetBadMods(void) const { return m_BadMods; }

    class CModFilter : public CObject {
    public:
        virtual ~CModFilter(void) { }
        virtual bool operator()( const CTempString & mod_name ) = 0;
    };
    /// The filter indicates whether certain mods are okay or not.
    ///
    /// @param pModFilter
    ///   If this is an unset CRef, it turns off filtering.
    void SetModFilter( CRef<CModFilter> pModFilter ) { 
        m_pModFilter = pModFilter; }

    /// Given a mod name (e.g. "topology"), it returns the set of acceptable
    /// values (e.g. "linear", "circular", etc.).  If the
    /// mod is unknown or free-form, the returned set will be blank.
    static const set<string> & GetModAllowedValues(const string &mod);

    /// Same as GetModAllowedValues, but returns one string with all the
    /// values.  Example: For "strand" it might give "'single', 'double', 'mixed'"
    static const string & GetModAllowedValuesAsOneString(const string &mod);

private:
    static const unsigned char kKeyCanonicalizationTable[257];

    EHandleBadMod m_HandleBadMod;

    TMods m_Mods;
    TMods m_BadMods;

    CRef<CModFilter> m_pModFilter;

    void x_ApplyMods(CAutoInitDesc<CBioSource>& bsrc, CTempString organism);
    void x_ApplyMods(CAutoInitDesc<CMolInfo>& mi);
    void x_ApplyMods(CAutoInitRef<CGene_ref>& gene);
    void x_ApplyMods(CAutoInitRef<CProt_ref>& prot);
    void x_ApplyMods(CAutoInitDesc<CGB_block>& gbb);
    void x_ApplyMods(CAutoInitRef<CSeq_hist>& hist);
    // void x_ApplyMods(CAutoInitRef<CSubmit_block>& sb);
    void x_ApplyTPAMods(CAutoInitRef<CUser_object>& tpa);
    void x_ApplyGenomeProjectsDBMods(CAutoInitRef<CUser_object>& gpdb);


    // sAllowedValues, enum_values, etc. are combined to produce the final list of
    // allowed values.
    // TModMap is some kind of map whose keys are "const char *".
    void x_HandleBadModValue(
        const SMod& mod );
};


//////////////////////////////////////////////////////////////////////

inline
void CSourceModParser::ApplyMods(CGene_ref& gene)
{
    CAutoInitRef<CGene_ref> ref;
    ref.Set(&gene);
    x_ApplyMods(ref);
}


inline
void CSourceModParser::ApplyMods(CProt_ref& prot)
{
    CAutoInitRef<CProt_ref> ref;
    ref.Set(&prot);
    x_ApplyMods(ref);
}


inline
void CSourceModParser::ApplyMods(CSeq_hist& hist)
{
    CAutoInitRef<CSeq_hist> ref;
    ref.Set(&hist);
    x_ApplyMods(ref);
}


// inline void CSourceModParser::ApplyMods(CSubmit_block& sb) { ... };


inline
void CSourceModParser::ApplyTPAMods(CUser_object& tpa)
{
    CAutoInitRef<CUser_object> ref;
    ref.Set(&tpa);
    x_ApplyTPAMods(ref);
}


inline
void CSourceModParser::ApplyGenomeProjectsDBMods(CUser_object& gpdb)
{
    CAutoInitRef<CUser_object> ref;
    ref.Set(&gpdb);
    x_ApplyGenomeProjectsDBMods(ref);
}


inline
int CSourceModParser::CompareKeys(const CTempString& lhs,
                                  const CTempString& rhs)
{
    CTempString::const_iterator it = lhs.begin(), it2 = rhs.begin();
    while (it != lhs.end()  &&  it2 != rhs.end()) {
        unsigned char uc1 = kKeyCanonicalizationTable[(unsigned char)*it++];
        unsigned char uc2 = kKeyCanonicalizationTable[(unsigned char)*it2++];
        if (uc1 > uc2) {
            return 1;
        } else if (uc1 < uc2) {
            return -1;
        }
    }
    if (it == lhs.end()) {
        return (it2 == rhs.end()) ? 0 : -1;
    } else {
        return 1;
    }
}


inline
bool CSourceModParser::SMod::operator <(const SMod& rhs) const
{
    // compare the key
    int key_comp = CompareKeys(key, rhs.key);
    if( key_comp != 0 ) {
        return key_comp < 0;
    }

    // compare sequence id
    if( ! seqid.IsNull() || ! rhs.seqid.IsNull() ) {
        // (unset seqid comes first)
        if( seqid.IsNull() ) {
            return true;
        } else if( rhs.seqid.IsNull() ) {
            return false;
        }

        TIntId id_comp = seqid->CompareOrdered(*rhs.seqid);
        if( id_comp != 0 ) {
            return id_comp < 0;
        }
    }

    // compare position
    return pos < rhs.pos;
}

inline
string CSourceModParser::SMod::ToString(void) const
{
    return "[ key: (" + key + "), value: (" + value +
        "), pos: " + NStr::SizetToString(pos) +
        ", used: " + string(used ? "true" : "false") + "]";
}


END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */

#endif  /* OBJTOOLS_READERS___SOURCE_MOD_PARSER__HPP */
