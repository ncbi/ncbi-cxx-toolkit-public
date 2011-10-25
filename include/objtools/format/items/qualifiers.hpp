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
#include <objects/seqfeat/Gene_nomenclature.hpp>
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

class NCBI_FORMAT_EXPORT CFormatQual : public CObject
{
public:
    enum EStyle {
        eEmpty,   // /name [value ignored]
        eQuoted,  // /name="value"
        eUnquoted // /name=value
    };
    typedef EStyle  TStyle;

    enum FFlags {
        // This turns off redundancy checks so 
        // we show this qual even if it is redundant with
        // the others.
        fFlags_showEvenIfRedund = 1 << 0
    };
    typedef int TFlags;

    // The level of trimming to do for this feature
    enum ETrim {
        // *vast* majority use this
        eTrim_Normal, 

        // a few quals need "junk" like extra periods.
        // for example, there's a gene allele of "2.." in
        // DQ533690.1 which needs both periods.
        eTrim_WhitespaceOnly
    };

    CFormatQual(const string& name,
              const string& value, 
              const string& prefix,
              const string& suffix,
              TStyle style = eQuoted,
              TFlags flags = 0,
              ETrim trim = eTrim_Normal );
    CFormatQual(const string& name,
              const string& value,
              TStyle style = eQuoted,
              TFlags flags = 0,
              ETrim trim = eTrim_Normal );

    const string& GetName  (void) const { return m_Name;   }
    const string& GetValue (void) const { return m_Value;  }
    TStyle        GetStyle (void) const { return m_Style;  }
    const string& GetPrefix(void) const { return m_Prefix; }
    const string& GetSuffix(void) const { return m_Suffix; }
    TFlags        GetFlags (void) const { return m_Flags;  }
    ETrim         GetTrim  (void) const { return m_Trim;   }

    void SetAddPeriod(bool add = true) { m_AddPeriod = add; }
    bool GetAddPeriod(void) const { return m_AddPeriod; }

private:
    string m_Name, m_Value, m_Prefix, m_Suffix;
    TStyle m_Style;
    TFlags m_Flags;
    ETrim m_Trim;
    bool m_AddPeriod;
};

typedef CRef<CFormatQual>    TFlatQual;
typedef vector<TFlatQual>    TFlatQuals;


/////////////////////////////////////////////////////////////////////////////
// abstract qualifier value

class NCBI_FORMAT_EXPORT IFlatQVal : public CObject
{
public:
    enum EFlags {
        fIsNote         = 0x1,
        fIsSource       = 0x2,
        fAddPeriod      = 0x4,
        fPrependNewline = 0x8
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
    typedef CFormatQual::ETrim    ETrim;

    IFlatQVal(const string* pfx = &kSpace, const string* sfx = &kEmptyStr)
        : m_Prefix(pfx), m_Suffix(sfx)
    { }
    TFlatQual x_AddFQ(TFlatQuals& q, const string& n, const string& v,
                      TStyle st = CFormatQual::eQuoted,
                      CFormatQual::TFlags flags = 0,
                      ETrim trim = CFormatQual::eTrim_Normal ) const {
        TFlatQual res(new CFormatQual(n, v, *m_Prefix, *m_Suffix, st, flags, trim));
        q.push_back(res); 
        return res;
    }

    mutable const string* m_Prefix;
    mutable const string* m_Suffix;
};


/////////////////////////////////////////////////////////////////////////////
// qualifiers container

template<typename Key>
class NCBI_FORMAT_EXPORT CQualContainer : public CObject
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
        return (it == m_Quals.end() || it->first == key) ? it : m_Quals.end();
    }
    const_iterator LowerBound(const Key& key) const {
        typename TQualMMap::const_iterator it = m_Quals.lower_bound(key);
        return (it == m_Quals.end() || it->first == key) ? it : m_Quals.end();
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

class NCBI_FORMAT_EXPORT CFlatBoolQVal : public IFlatQVal
{
public:
    CFlatBoolQVal(bool value) : m_Value(value) { }
    void Format(TFlatQuals& q, const string& n, CBioseqContext&, TFlags) const
        { if (m_Value) { x_AddFQ(q, n, kEmptyStr, CFormatQual::eEmpty); } }
private:
    bool m_Value;
};


class NCBI_FORMAT_EXPORT CFlatIntQVal : public IFlatQVal
{
public:
    CFlatIntQVal(int value) : m_Value(value) { }
    void Format(TFlatQuals& q, const string& n, CBioseqContext&, TFlags) const;
private:
    int m_Value;
};


// potential flags:
//  tilde mode?
//  expand SGML entities?
// (handle via subclasses?)
class NCBI_FORMAT_EXPORT CFlatStringQVal : public IFlatQVal
{
public:
    CFlatStringQVal(const string& value, 
        TStyle style = CFormatQual::eQuoted,
        ETrim trim  = CFormatQual::eTrim_Normal );
    CFlatStringQVal(const string& value, const string& pfx, const string& sfx,
        TStyle style = CFormatQual::eQuoted, ETrim trim  = CFormatQual::eTrim_Normal );
    CFlatStringQVal(const string& value, 
        ETrim trim );
        
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

    const string& GetValue(void) const { return m_Value; }
    void SetAddPeriod( bool addPeriod = true ) { m_AddPeriod = ( addPeriod ? IFlatQVal::fAddPeriod : 0 ); }

protected:
    mutable string    m_Value;
    TStyle            m_Style;
    ETrim             m_Trim;
    IFlatQVal::TFlags m_AddPeriod;
};


class NCBI_FORMAT_EXPORT CFlatNumberQVal : public CFlatStringQVal
{
public:
    CFlatNumberQVal(const string& value) :
        CFlatStringQVal(value, CFormatQual::eUnquoted)
    {}
        
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;
};


class NCBI_FORMAT_EXPORT CFlatBondQVal : public CFlatStringQVal
{
public:
    CFlatBondQVal(const string& value) : CFlatStringQVal(value)
    {}
        
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;
};


class NCBI_FORMAT_EXPORT CFlatGeneQVal : public CFlatStringQVal
{
public:
    CFlatGeneQVal(const string& value) : CFlatStringQVal(value, CFormatQual::eTrim_WhitespaceOnly)
    {}
        
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;
};


class NCBI_FORMAT_EXPORT CFlatSiteQVal : public CFlatStringQVal
{
public:
    CFlatSiteQVal(const string& value) : CFlatStringQVal(value)
    {}
        
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;
};


class NCBI_FORMAT_EXPORT CFlatStringListQVal : public IFlatQVal
{
public:
    typedef list<string>    TValue;

    CFlatStringListQVal(const list<string>& value,
                        TStyle style = CFormatQual::eQuoted)
        :   m_Value(value), m_Style(style) { }
    CFlatStringListQVal(const list<string>::const_iterator& begin,
                        const list<string>::const_iterator& end,
        TStyle style = CFormatQual::eQuoted)
        :   m_Value(begin, end), m_Style(style) { }
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

    const TValue& GetValue(void) const { return m_Value; }
    TValue& SetValue(void) { return m_Value; }

protected:
    TValue   m_Value;
    TStyle   m_Style;
};


class NCBI_FORMAT_EXPORT CFlatGeneSynonymsQVal : public CFlatStringListQVal
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

class NCBI_FORMAT_EXPORT CFlatNomenclatureQVal : public IFlatQVal
{
public:
    CFlatNomenclatureQVal( const CGene_ref_Base::TFormal_name& value ) : m_Value(&value) { }
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const; 

private:
    CConstRef<CGene_ref_Base::TFormal_name> m_Value;
};

class NCBI_FORMAT_EXPORT CFlatCodeBreakQVal : public IFlatQVal
{
public:
    CFlatCodeBreakQVal(const CCdregion::TCode_break& value) : m_Value(value) { }
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

private:
    CCdregion::TCode_break m_Value;
};


class NCBI_FORMAT_EXPORT CFlatCodonQVal : public IFlatQVal
{
public:
    CFlatCodonQVal(unsigned int codon, unsigned char aa, bool is_ascii = true);
    // CFlatCodonQVal(const string& value); // for imports
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

private:
    string m_Codon, m_AA;
    bool   m_Checked;
};


class NCBI_FORMAT_EXPORT CFlatExperimentQVal : public IFlatQVal
{
public:
    CFlatExperimentQVal(
        const string&  = "" );
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;
protected:
    string m_str;
};


class NCBI_FORMAT_EXPORT CFlatInferenceQVal : public IFlatQVal
{
public:
    CFlatInferenceQVal( const string& = "" );
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

protected:
    string m_str;
};


class NCBI_FORMAT_EXPORT CFlatIllegalQVal : public IFlatQVal
{
public:
    CFlatIllegalQVal(const CGb_qual& value) : m_Value(&value) { }
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

private:
    CConstRef<CGb_qual> m_Value;
};


class NCBI_FORMAT_EXPORT CFlatLabelQVal : public CFlatStringQVal
{
public:
    CFlatLabelQVal(const string& value)
        : CFlatStringQVal(value, CFormatQual::eUnquoted) { }
    // XXX - should override Format to check syntax
};


class NCBI_FORMAT_EXPORT CFlatMolTypeQVal : public IFlatQVal
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


class NCBI_FORMAT_EXPORT CFlatOrgModQVal : public IFlatQVal
{
public:
    CFlatOrgModQVal(const COrgMod& value) :
      IFlatQVal(&kSpace, &kSemicolon), m_Value(&value) { }

    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

private:
    CConstRef<COrgMod> m_Value;
};


class NCBI_FORMAT_EXPORT CFlatOrganelleQVal : public IFlatQVal
{
public:
    CFlatOrganelleQVal(CBioSource::TGenome value) : m_Value(value) { }
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

private:
    CBioSource::TGenome m_Value;
};


class NCBI_FORMAT_EXPORT CFlatPubSetQVal : public IFlatQVal
{
public:
    CFlatPubSetQVal(const CPub_set& value) : m_Value(&value) { }
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

private:
    CConstRef<CPub_set> m_Value;
};


class NCBI_FORMAT_EXPORT CFlatSeqIdQVal : public IFlatQVal
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


class NCBI_FORMAT_EXPORT CFlatSeqLocQVal : public IFlatQVal
{
public:
    CFlatSeqLocQVal(const CSeq_loc& value) : m_Value(&value) { }
    void Format(TFlatQuals& q, const string& n, CBioseqContext& ctx,
                TFlags) const
        { x_AddFQ(q, n, CFlatSeqLoc(*m_Value, ctx).GetString()); }

private:
    CConstRef<CSeq_loc> m_Value;
};


class NCBI_FORMAT_EXPORT CFlatSubSourceQVal : public IFlatQVal
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

class NCBI_FORMAT_EXPORT CFlatSubSourcePrimer : public IFlatQVal
{
public:
    CFlatSubSourcePrimer(
        const string& fwd_name,
        const string& fwd_seq,
        const string& rev_name,
        const string& rev_seq ) :
        IFlatQVal( &kSpace, &kSemicolon),
        m_fwd_name( fwd_name ),
        m_fwd_seq( fwd_seq ),
        m_rev_name( rev_name ),
        m_rev_seq( rev_seq )
    {
        NStr::ToLower( m_fwd_seq );
        NStr::ToLower( m_rev_seq );
    }

    void Format( 
        TFlatQuals& quals, const string& name, CBioseqContext& ctx, TFlags flags) const;

protected:
    string m_fwd_name;
    string m_fwd_seq;
    string m_rev_name;
    string m_rev_seq;
};

class NCBI_FORMAT_EXPORT CFlatXrefQVal : public IFlatQVal
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


class NCBI_FORMAT_EXPORT CFlatModelEvQVal : public IFlatQVal
{
public:
    CFlatModelEvQVal(const CUser_object& value) : m_Value(&value) { }
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

private:
    CConstRef<CUser_object> m_Value;
};


class NCBI_FORMAT_EXPORT CFlatGoQVal : public IFlatQVal
{
public:
    CFlatGoQVal(const CUser_field& value) : m_Value(&value) { }
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

    bool Equals( const CFlatGoQVal &rhs ) const;

    const string & GetTextString(void) const;
    int GetPubmedId(void) const;

private:
    CConstRef<CUser_field> m_Value;
};


class NCBI_FORMAT_EXPORT CFlatAnticodonQVal : public IFlatQVal
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


class NCBI_FORMAT_EXPORT CFlatTrnaCodonsQVal : public IFlatQVal
{
public:
    CFlatTrnaCodonsQVal(const CTrna_ext& trna, const string& comment) : 
      IFlatQVal(&kEmptyStr, &kSemicolon), m_Value(&trna), m_Seqfeat_note(comment)
    {}
    void Format(TFlatQuals& q, const string& n, CBioseqContext& ctx,
                TFlags) const;

private:
    CConstRef<CTrna_ext> m_Value;
    const string& m_Seqfeat_note;
};


class NCBI_FORMAT_EXPORT CFlatProductNamesQVal : public IFlatQVal
{
public:
    CFlatProductNamesQVal(const CProt_ref::TName& value, const string& gene) : 
        IFlatQVal(&kSpace, &kSemicolon), m_Value(value), m_Gene(gene)
    {}
        
    void Format(TFlatQuals& quals, const string& name, CBioseqContext& ctx,
                TFlags flags) const;

    const CProt_ref::TName& GetValue(void) const { return m_Value; }
    CProt_ref::TName& SetValue(void) { return m_Value; }

private:
    CProt_ref::TName m_Value;
    string           m_Gene; 
};

// ...

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT_ITEMS___QUALIFIERS__HPP */
