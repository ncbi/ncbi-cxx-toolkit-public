#ifndef OBJTOOLS_FORMAT_ITEMS___QUALIFIERS__HPP
#define OBJTOOLS_FORMAT_ITEMS___QUALIFIERS__HPP

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
* Author:  Aaron Ucko, Mati Shomrat
*
* File Description:
*   new (early 2003) flat-file generator -- qualifier types
*   (mainly of interest to implementors)
*
*/
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/pub/Pub_set.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objtools/format/items/flat_seqloc.hpp>
#include <objtools/format/items/flat_qual_slots.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CBioseqContext;


/////////////////////////////////////////////////////////////////////////////
// low-level formatted qualifier

class CFormatQual : public CObject
{
public:
    enum EStyle {
        eEmpty,   // /name [value ignored]
        eQuoted,  // /name="value"
        eUnquoted // /name=value
    };
    typedef EStyle  TStyle;

    CFormatQual(const string& name,
              const string& value, 
              const string& prefix,
              const string& suffix,
              TStyle style = eQuoted);
    CFormatQual(const string& name,
              const string& value,
              TStyle style = eQuoted);

    const string& GetName  (void) const { return m_Name;   }
    const string& GetValue (void) const { return m_Value;  }
    TStyle        GetStyle (void) const { return m_Style;  }
    const string& GetPrefix(void) const { return m_Prefix; }
    const string& GetSuffix(void) const { return m_Suffix; }

    void SetAddPeriod(bool add = true) { m_AddPeriod = add; }
    bool GetAddPeriod(void) const { return m_AddPeriod; }

private:
    string m_Name, m_Value, m_Prefix, m_Suffix;
    TStyle m_Style;
    bool m_AddPeriod;
};

typedef CRef<CFormatQual>    TFlatQual;
typedef vector<TFlatQual>    TFlatQuals;


/////////////////////////////////////////////////////////////////////////////
// abstract qualifier value

class IFlatQVal : public CObject
{
public:
    enum EFlags {
        fIsNote      = 0x1,
        fIsSource    = 0x2,
        fAddPeriod   = 0x4
    };
    typedef int TFlags; // binary OR of EFlags

    static const string kSemicolon;  // ";"
    static const string kComma;      // ","
    static const string kEOL;        // "\n" - end of line
    static const string kSpace;      // " "

    virtual void Format(TFlatQuals& quals, const string& name,
        CBioseqContext& ctx, TFlags flags = 0) const = 0;

protected:
    typedef CFormatQual::TStyle   TStyle;

    IFlatQVal(const string* pfx = &kSpace, const string* sfx = &kEmptyStr)
        : m_Prefix(pfx), m_Suffix(sfx)
    { }
    TFlatQual x_AddFQ(TFlatQuals& q, const string& n, const string& v,
                      TStyle st = CFormatQual::eQuoted) const {
        TFlatQual res(new CFormatQual(n, v, *m_Prefix, *m_Suffix, st));
        q.push_back(res); 
        return res;
    }

    mutable const string* m_Prefix;
    mutable const string* m_Suffix;
};


/////////////////////////////////////////////////////////////////////////////
// qualifiers container

template<typename Key>
class CQualContainer : public CObject
{
public:
    // typedef
    typedef multimap<Key, CConstRef<IFlatQVal> > TQualMMap;
    typedef typename TQualMMap::const_iterator   const_iterator;
    typedef typename TQualMMap::iterator         iterator;
    typedef typename TQualMMap::size_type        size_type;

    // constructor
    CQualContainer(void) {}
    
    iterator begin(void) { return m_Quals.begin(); }
    const_iterator begin(void) const { return m_Quals.begin(); }
    iterator end(void) { return m_Quals.end(); }
    const_iterator end(void) const { return m_Quals.end(); }
    
    void AddQual(const Key& key, const IFlatQVal* value) {
        typedef typename TQualMMap::value_type TMapPair;
        m_Quals.insert(TMapPair(key, CConstRef<IFlatQVal>(value)));
    }
    
    bool HasQual(const Key& key) const {
        return Find(key) != m_Quals.end();
    }
    iterator LowerBound(Key& key) {
        typename TQualMMap::iterator it = m_Quals.lower_bound(key);
        return it->first == key ? it : m_Quals.end();
    }
    const_iterator LowerBound(const Key& key) const {
        typename TQualMMap::const_iterator it = m_Quals.lower_bound(key);
        return it->first == key ? it : m_Quals.end();
    }
    iterator Erase(iterator it) {
        iterator next = it;
        if ( next != end() ) {
            ++next;
            m_Quals.erase(it);
        }
        return next;
    }
    void RemoveQuals(const Key& key) {
        m_Quals.erase(key);
    }
    iterator Find(const Key& key) {
        return m_Quals.find(key);
    }
    const_iterator Find(const Key& key) const {
        return m_Quals.find(key);
    }
    size_type Size() const {
        return m_Quals.size();
    }

private:
    TQualMMap m_Quals;
};


/////////////////////////////////////////////////////////////////////////////
// concrete qualifiers

class CFlatBoolQVal : public IFlatQVal
{
public:
    CFlatBoolQVal(bool value) : m_Value(value) { }
    void Format(TFlatQuals& q, const string& n, CBioseqContext&, TFlags) const
        { if (m_Value) { x_AddFQ(q, n, kEmptyStr, CFormatQual::eEmpty); } }
private:
    bool m_Value;
};


class CFlatIntQVal : public IFlatQVal
{
public:
    CFlatIntQVal(int value) : m_Value(value) { }
    void Format(TFlatQuals& q, const string& n, CBioseqContext&, TFlags) const
        { x_AddFQ(q, n, NStr::IntToString(m_Value), CFormatQual::eUnquoted); }
private:
    int m_Value;
};


// potential flags:
//  tilde mode?
//  expand SGML entities?
// (handle via subclasses?)
class CFlatStringQVal : public IFlatQVal
{
public:
    CFlatStringQVal(const string& value, TStyle style = CFormatQual::eQuoted);
    CFlatStringQVal(const string& value, const string& pfx, const string& sfx,
        TStyle style = CFormatQual::eQuoted);
        
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

    const string& GetValue(void) const { return m_Value; }
    void SetAddPeriod(void) { m_AddPeriod = IFlatQVal::fAddPeriod; }

protected:
    mutable string    m_Value;
    TStyle            m_Style;
    IFlatQVal::TFlags m_AddPeriod;
};


class CFlatNumberQVal : public CFlatStringQVal
{
public:
    CFlatNumberQVal(const string& value) :
        CFlatStringQVal(value, CFormatQual::eUnquoted)
    {}
        
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;
};


class CFlatBondQVal : public CFlatStringQVal
{
public:
    CFlatBondQVal(const string& value) : CFlatStringQVal(value)
    {}
        
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;
};


class CFlatGeneQVal : public CFlatStringQVal
{
public:
    CFlatGeneQVal(const string& value) : CFlatStringQVal(value)
    {}
        
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;
};


class CFlatSiteQVal : public CFlatStringQVal
{
public:
    CFlatSiteQVal(const string& value) : CFlatStringQVal(value)
    {}
        
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;
};


class CFlatStringListQVal : public IFlatQVal
{
public:
    typedef list<string>    TValue;

    CFlatStringListQVal(const list<string>& value,
        TStyle style = CFormatQual::eQuoted)
        :   m_Value(value), m_Style(style) { }
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

    const TValue& GetValue(void) const { return m_Value; }
    TValue& SetValue(void) { return m_Value; }

protected:
    TValue   m_Value;
    TStyle   m_Style;
};


class CFlatGeneSynonymsQVal : public CFlatStringListQVal
{
public:
    CFlatGeneSynonymsQVal(const CGene_ref::TSyn& syns) :
        CFlatStringListQVal(syns)
    {
        m_Suffix = &kSemicolon;
    }

    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;
};


class CFlatCodeBreakQVal : public IFlatQVal
{
public:
    CFlatCodeBreakQVal(const CCdregion::TCode_break& value) : m_Value(value) { }
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

private:
    CCdregion::TCode_break m_Value;
};


class CFlatCodonQVal : public IFlatQVal
{
public:
    CFlatCodonQVal(unsigned int codon, unsigned char aa, bool is_ascii = true);
    CFlatCodonQVal(const string& value); // for imports
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

private:
    string m_Codon, m_AA;
    bool   m_Checked;
};


class CFlatExpEvQVal : public IFlatQVal
{
public:
    CFlatExpEvQVal(CSeq_feat::TExp_ev value) : m_Value(value) { }
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

private:
    CSeq_feat::TExp_ev m_Value;
};


class CFlatIllegalQVal : public IFlatQVal
{
public:
    CFlatIllegalQVal(const CGb_qual& value) : m_Value(&value) { }
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

private:
    CConstRef<CGb_qual> m_Value;
};


class CFlatLabelQVal : public CFlatStringQVal
{
public:
    CFlatLabelQVal(const string& value)
        : CFlatStringQVal(value, CFormatQual::eUnquoted) { }
    // XXX - should override Format to check syntax
};


class CFlatMolTypeQVal : public IFlatQVal
{
public:
    typedef CMolInfo::TBiomol TBiomol;
    typedef CSeq_inst::TMol   TMol;

    CFlatMolTypeQVal(TBiomol biomol, TMol mol) : m_Biomol(biomol), m_Mol(mol) { }
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

private:
    TBiomol m_Biomol;
    TMol    m_Mol;
};


class CFlatOrgModQVal : public IFlatQVal
{
public:
    CFlatOrgModQVal(const COrgMod& value) :
      IFlatQVal(&kSpace, &kSemicolon), m_Value(&value) { }

    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

private:
    CConstRef<COrgMod> m_Value;
};


class CFlatOrganelleQVal : public IFlatQVal
{
public:
    CFlatOrganelleQVal(CBioSource::TGenome value) : m_Value(value) { }
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

private:
    CBioSource::TGenome m_Value;
};


class CFlatPubSetQVal : public IFlatQVal
{
public:
    CFlatPubSetQVal(const CPub_set& value) : m_Value(&value) { }
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

private:
    CConstRef<CPub_set> m_Value;
};


class CFlatSeqIdQVal : public IFlatQVal
{
public:
    CFlatSeqIdQVal(const CSeq_id& value, bool add_gi_prefix = false) 
        : m_Value(&value), m_GiPrefix(add_gi_prefix) { }
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

private:
    CConstRef<CSeq_id> m_Value;
    bool               m_GiPrefix;
};


class CFlatSeqLocQVal : public IFlatQVal
{
public:
    CFlatSeqLocQVal(const CSeq_loc& value) : m_Value(&value) { }
    void Format(TFlatQuals& q, const string& n, CBioseqContext& ctx,
                TFlags) const
        { x_AddFQ(q, n, CFlatSeqLoc(*m_Value, ctx).GetString()); }

private:
    CConstRef<CSeq_loc> m_Value;
};


class CFlatSubSourceQVal : public IFlatQVal
{
public:
    CFlatSubSourceQVal(const CSubSource& value) :
        IFlatQVal(&kSpace, &kSemicolon), m_Value(&value)
    { }

    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

private:
    CConstRef<CSubSource> m_Value;
};


class CFlatXrefQVal : public IFlatQVal
{
public:
    typedef CSeq_feat::TDbxref                TXref;
    typedef CQualContainer<EFeatureQualifier> TQuals;

    CFlatXrefQVal(const TXref& value, const TQuals* quals = 0) 
        :   m_Value(value), m_Quals(quals) { }
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

private:
    bool x_XrefInGeneXref(const CDbtag& dbtag) const;

    TXref             m_Value;
    CConstRef<TQuals> m_Quals;
};


class CFlatModelEvQVal : public IFlatQVal
{
public:
    CFlatModelEvQVal(const CUser_object& value) : m_Value(&value) { }
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

private:
    CConstRef<CUser_object> m_Value;
};


class CFlatGoQVal : public IFlatQVal
{
public:
    CFlatGoQVal(const CUser_field& value) : m_Value(&value) { }
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

private:
    CConstRef<CUser_field> m_Value;
};


class CFlatAnticodonQVal : public IFlatQVal
{
public:
    CFlatAnticodonQVal(const CSeq_loc& ac, const string& aa) :
        m_Anticodon(&ac), m_Aa(aa) { }
    void Format(TFlatQuals& q, const string& n, CBioseqContext& ctx,
                TFlags) const;

private:
    CConstRef<CSeq_loc> m_Anticodon;
    string              m_Aa;
};


class CFlatTrnaCodonsQVal : public IFlatQVal
{
public:
    CFlatTrnaCodonsQVal(const CTrna_ext& trna) : 
      IFlatQVal(&kEmptyStr, &kSemicolon), m_Value(&trna)
    {}
    void Format(TFlatQuals& q, const string& n, CBioseqContext& ctx,
                TFlags) const;

private:
    CConstRef<CTrna_ext> m_Value;
};


class CFlatProductQVal : public CFlatStringQVal
{
public:
    typedef CSeqFeatData::ESubtype  TSubtype;
    CFlatProductQVal(const string& value, TSubtype subtype) :
        CFlatStringQVal(value), m_Subtype(subtype)
    {}
    void Format(TFlatQuals& q, const string& n, CBioseqContext& ctx,
                TFlags) const;
private:
    TSubtype m_Subtype;
};

// ...

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.20  2005/03/28 17:15:16  shomrat
* Use lower_bound instead of equal_range
*
* Revision 1.19  2005/01/21 13:10:09  dicuccio
* Pass by reference, not by value
*
* Revision 1.18  2004/12/09 14:46:51  shomrat
* Added CFlatSiteQVal
*
* Revision 1.17  2004/11/15 20:03:49  shomrat
* Better control of period in qualifiers
*
* Revision 1.16  2004/10/18 18:44:58  shomrat
* Added Gene and Bond qual classes
*
* Revision 1.15  2004/10/05 15:33:28  shomrat
* changed const strings
*
* Revision 1.14  2004/08/30 13:32:41  shomrat
* fixed intialization of trna codon qual
*
* Revision 1.13  2004/08/19 16:26:15  shomrat
* + CFlatGeneSynonymsQVal and CFlatNumberQVal
*
* Revision 1.12  2004/05/19 14:44:38  shomrat
* + CQualContainer::Erase(..)
*
* Revision 1.11  2004/05/06 17:43:00  shomrat
* CFlatQual -> CFormatQual to prevent name collisions in ncbi_seqext lib
*
* Revision 1.10  2004/04/26 21:11:23  ucko
* Tweak previous fix so that it compiles with MSVC.
*
* Revision 1.9  2004/04/26 16:49:27  ucko
* Add an explicit "typename" annotation required by GCC 3.4.
*
* Revision 1.8  2004/04/22 15:39:05  shomrat
* Changes in context
*
* Revision 1.7  2004/03/31 15:57:49  ucko
* Add missing "typename"s in CQualContainer's typedefs.
*
* Revision 1.6  2004/03/30 20:25:26  shomrat
* + class CQualContainer
*
* Revision 1.5  2004/03/18 15:28:45  shomrat
* Fixes to quals formatting; + new Product qual
*
* Revision 1.4  2004/03/08 20:59:02  shomrat
* + GI prefix flag for Seq-id qualifiers
*
* Revision 1.3  2004/03/05 18:50:48  shomrat
* Added new qualifier classes
*
* Revision 1.2  2004/02/11 16:37:20  shomrat
* added CFlatStringListQVal class and an optional suffix to CFormatQual
*
* Revision 1.1  2003/12/17 19:49:19  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/

#endif  /* OBJTOOLS_FORMAT_ITEMS___QUALIFIERS__HPP */
