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
#include <objects/pub/Pub_set.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objtools/format/items/flat_seqloc.hpp>

//#include <objtools/flat/items/flat_qual.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CFFContext;


/////////////////////////////////////////////////////////////////////////////
// low-level formatted qualifier

class CFlatQual : public CObject
{
public:
    enum EStyle {
        eEmpty,   // /name [value ignored]
        eQuoted,  // /name="value"
        eUnquoted // /name=value
    };

    CFlatQual(const string& name, const string& value, 
        const string& suffix, EStyle style = eQuoted)
        : m_Name(name), m_Value(value), m_Suffix(suffix), m_Style(style)
        { }
    CFlatQual(const string& name, const string& value, EStyle style = eQuoted)
        : m_Name(name), m_Value(value), m_Suffix(kEmptyStr), m_Style(style)
        { }

    const string& GetName (void) const  { return m_Name;  }
    const string& GetValue(void) const  { return m_Value; }
    EStyle        GetStyle(void) const  { return m_Style; }
    const string& GetSuffix(void) const { return m_Suffix; }

private:
    string m_Name, m_Value, m_Suffix;
    EStyle m_Style;
};
typedef CRef<CFlatQual>      TFlatQual;
typedef vector<TFlatQual>    TFlatQuals;


/////////////////////////////////////////////////////////////////////////////
// abstract qualifier value

class IFlatQVal : public CObject
{
public:
    enum EFlags {
        fIsNote   = 0x1,
        fIsSource = 0x2
    };
    typedef int TFlags; // binary OR of EFlags

    virtual void Format(TFlatQuals& quals, const string& name,
        CFFContext& ctx, TFlags flags = 0) const = 0;

protected:
    static void x_AddFQ(TFlatQuals& q, const string& n, const string& v,
                        CFlatQual::EStyle st = CFlatQual::eQuoted)
        { q.push_back(TFlatQual(new CFlatQual(n, v, st))); }
    static void x_AddFQ(TFlatQuals& q, const string& n, const string& v,
                        const string& s, CFlatQual::EStyle st = CFlatQual::eQuoted)
        { q.push_back(TFlatQual(new CFlatQual(n, v, s, st))); }
};


/////////////////////////////////////////////////////////////////////////////
// concrete qualifiers

class CFlatBoolQVal : public IFlatQVal
{
public:
    CFlatBoolQVal(bool value) : m_Value(value) { }
    void Format(TFlatQuals& q, const string& n, CFFContext&, TFlags) const
        { if (m_Value) { x_AddFQ(q, n, kEmptyStr, CFlatQual::eEmpty); } }
private:
    bool m_Value;
};


class CFlatIntQVal : public IFlatQVal
{
public:
    CFlatIntQVal(int value) : m_Value(value) { }
    void Format(TFlatQuals& q, const string& n, CFFContext&, TFlags) const
        { x_AddFQ(q, n, NStr::IntToString(m_Value), CFlatQual::eUnquoted); }
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
    CFlatStringQVal(const string& value,
                  CFlatQual::EStyle style = CFlatQual::eQuoted)
        : m_Value(value), m_Style(style) { }
    void Format(TFlatQuals& quals, const string& name, CFFContext& ctx,
                TFlags flags) const;

private:
    string            m_Value;
    CFlatQual::EStyle m_Style;
};


class CFlatStringListQVal : public IFlatQVal
{
public:
    CFlatStringListQVal(const list<string>& value,
                  CFlatQual::EStyle style = CFlatQual::eQuoted)
        : m_Value(value), m_Style(style) { }
    void Format(TFlatQuals& quals, const string& name, CFFContext& ctx,
                TFlags flags) const;

private:
    list<string>      m_Value;
    CFlatQual::EStyle m_Style;
};


class CFlatCodeBreakQVal : public IFlatQVal
{
public:
    CFlatCodeBreakQVal(const CCdregion::TCode_break value) : m_Value(value) { }
    void Format(TFlatQuals& quals, const string& name, CFFContext& ctx,
                TFlags flags) const;

private:
    CCdregion::TCode_break m_Value;
};


class CFlatCodonQVal : public IFlatQVal
{
public:
    CFlatCodonQVal(unsigned int codon, unsigned char aa, bool is_ascii = true);
    CFlatCodonQVal(const string& value); // for imports
    void Format(TFlatQuals& quals, const string& name, CFFContext& ctx,
                TFlags flags) const;

private:
    string m_Codon, m_AA;
    bool   m_Checked;
};


class CFlatExpEvQVal : public IFlatQVal
{
public:
    CFlatExpEvQVal(CSeq_feat::TExp_ev value) : m_Value(value) { }
    void Format(TFlatQuals& quals, const string& name, CFFContext& ctx,
                TFlags flags) const;

private:
    CSeq_feat::TExp_ev m_Value;
};


class CFlatIllegalQVal : public IFlatQVal
{
public:
    CFlatIllegalQVal(const CGb_qual& value) : m_Value(&value) { }
    void Format(TFlatQuals& quals, const string& name, CFFContext& ctx,
                TFlags flags) const;

private:
    CConstRef<CGb_qual> m_Value;
};


class CFlatLabelQVal : public CFlatStringQVal
{
public:
    CFlatLabelQVal(const string& value)
        : CFlatStringQVal(value, CFlatQual::eUnquoted) { }
    // XXX - should override Format to check syntax
};


class CFlatMolTypeQVal : public IFlatQVal
{
public:
    typedef CMolInfo::TBiomol TBiomol;
    typedef CSeq_inst::TMol   TMol;

    CFlatMolTypeQVal(TBiomol biomol, TMol mol) : m_Biomol(biomol), m_Mol(mol) { }
    void Format(TFlatQuals& quals, const string& name, CFFContext& ctx,
                TFlags flags) const;

private:
    TBiomol m_Biomol;
    TMol    m_Mol;
};


class CFlatOrgModQVal : public IFlatQVal
{
public:
    CFlatOrgModQVal(const COrgMod& value) : m_Value(&value) { }
    void Format(TFlatQuals& quals, const string& name, CFFContext& ctx,
                TFlags flags) const;

private:
    CConstRef<COrgMod> m_Value;
};


class CFlatOrganelleQVal : public IFlatQVal
{
public:
    CFlatOrganelleQVal(CBioSource::TGenome value) : m_Value(value) { }
    void Format(TFlatQuals& quals, const string& name, CFFContext& ctx,
                TFlags flags) const;

private:
    CBioSource::TGenome m_Value;
};


class CFlatPubSetQVal : public IFlatQVal
{
public:
    CFlatPubSetQVal(const CPub_set& value) : m_Value(&value) { }
    void Format(TFlatQuals& quals, const string& name, CFFContext& ctx,
                TFlags flags) const;

private:
    CConstRef<CPub_set> m_Value;
};


class CFlatSeqDataQVal : public IFlatQVal
{
public:
    CFlatSeqDataQVal(const CSeq_loc& value) : m_Value(&value) { }
    void Format(TFlatQuals& quals, const string& name, CFFContext& ctx,
                TFlags flags) const;

private:
    CConstRef<CSeq_loc> m_Value;
};


class CFlatSeqIdQVal : public IFlatQVal
{
public:
    CFlatSeqIdQVal(const CSeq_id& value) : m_Value(&value) { }
    void Format(TFlatQuals& quals, const string& name, CFFContext& ctx,
                TFlags flags) const;

private:
    CConstRef<CSeq_id> m_Value;
};


class CFlatSeqLocQVal : public IFlatQVal
{
public:
    CFlatSeqLocQVal(const CSeq_loc& value) : m_Value(&value) { }
    void Format(TFlatQuals& q, const string& n, CFFContext& ctx,
                TFlags) const
        { x_AddFQ(q, n, CFlatSeqLoc(*m_Value, ctx).GetString()); }

private:
    CConstRef<CSeq_loc> m_Value;
};


class CFlatSubSourceQVal : public IFlatQVal
{
public:
    CFlatSubSourceQVal(const CSubSource& value) : m_Value(&value) { }
    void Format(TFlatQuals& quals, const string& name, CFFContext& ctx,
                TFlags flags) const;

private:
    CConstRef<CSubSource> m_Value;
};


class CFlatXrefQVal : public IFlatQVal
{
public:
    typedef CSeq_feat::TDbxref TXref;
    CFlatXrefQVal(const TXref& value) : m_Value(value) { }
    void Format(TFlatQuals& quals, const string& name, CFFContext& ctx,
                TFlags flags) const;

private:
    TXref m_Value;
};


// ...

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2004/02/11 16:37:20  shomrat
* added CFlatStringListQVal class and an optional suffix to CFlatQual
*
* Revision 1.1  2003/12/17 19:49:19  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/

#endif  /* OBJTOOLS_FORMAT_ITEMS___QUALIFIERS__HPP */
