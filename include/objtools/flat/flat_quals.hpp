#ifndef OBJECTS_FLAT___FLAT_QUALS__HPP
#define OBJECTS_FLAT___FLAT_QUALS__HPP

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
* Author:  Aaron Ucko, NCBI
*
* File Description:
*   new (early 2003) flat-file generator -- qualifier types
*   (mainly of interest to implementors)
*
*/

#include <objtools/flat/flat_feature.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/pub/Pub_set.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SubSource.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)

class CFlatBoolQV : public IFlatQV
{
public:
    CFlatBoolQV(bool value) : m_Value(value) { }
    void Format(TFlatQuals& q, const string& n, CFlatContext&, TFlags) const
        { if (m_Value) { x_AddFQ(q, n, kEmptyStr, CFlatQual::eEmpty); } }

private:
    bool m_Value;
};


class CFlatIntQV : public IFlatQV
{
public:
    CFlatIntQV(int value) : m_Value(value) { }
    void Format(TFlatQuals& q, const string& n, CFlatContext&, TFlags) const
        { x_AddFQ(q, n, NStr::IntToString(m_Value), CFlatQual::eUnquoted); }

private:
    int m_Value;
};


// potential flags:
//  tilde mode?
//  expand SGML entities?
// (handle via subclasses?)
class CFlatStringQV : public IFlatQV
{
public:
    CFlatStringQV(const string& value,
                  CFlatQual::EStyle style = CFlatQual::eQuoted)
        : m_Value(value), m_Style(style) { }
    void Format(TFlatQuals& quals, const string& name, CFlatContext& ctx,
                TFlags flags) const;

private:
    string            m_Value;
    CFlatQual::EStyle m_Style;
};


class CFlatCodeBreakQV : public IFlatQV
{
public:
    CFlatCodeBreakQV(CCdregion::TCode_break value) : m_Value(value) { }
    void Format(TFlatQuals& quals, const string& name, CFlatContext& ctx,
                TFlags flags) const;

private:
    CCdregion::TCode_break m_Value;
};


class CFlatCodonQV : public IFlatQV
{
public:
    CFlatCodonQV(unsigned int codon, unsigned char aa, bool is_ascii = true);
    CFlatCodonQV(const string& value); // for imports
    void Format(TFlatQuals& quals, const string& name, CFlatContext& ctx,
                TFlags flags) const;

private:
    string m_Codon, m_AA;
    bool   m_Checked;
};


class CFlatExpEvQV : public IFlatQV
{
public:
    CFlatExpEvQV(CSeq_feat::TExp_ev value) : m_Value(value) { }
    void Format(TFlatQuals& quals, const string& name, CFlatContext& ctx,
                TFlags flags) const;

private:
    CSeq_feat::TExp_ev m_Value;
};


class CFlatIllegalQV : public IFlatQV
{
public:
    CFlatIllegalQV(const CGb_qual& value) : m_Value(&value) { }
    void Format(TFlatQuals& quals, const string& name, CFlatContext& ctx,
                TFlags flags) const;

private:
    CConstRef<CGb_qual> m_Value;
};


class CFlatLabelQV : public CFlatStringQV
{
public:
    CFlatLabelQV(const string& value)
        : CFlatStringQV(value, CFlatQual::eUnquoted) { }
    // XXX - should override Format to check syntax
};


class CFlatMolTypeQV : public IFlatQV
{
public:
    typedef CMolInfo::TBiomol TBiomol;
    typedef CSeq_inst::TMol   TMol;

    CFlatMolTypeQV(TBiomol biomol, TMol mol) : m_Biomol(biomol), m_Mol(mol) { }
    void Format(TFlatQuals& quals, const string& name, CFlatContext& ctx,
                TFlags flags) const;

private:
    TBiomol m_Biomol;
    TMol    m_Mol;
};


class CFlatOrgModQV : public IFlatQV
{
public:
    CFlatOrgModQV(const COrgMod& value) : m_Value(&value) { }
    void Format(TFlatQuals& quals, const string& name, CFlatContext& ctx,
                TFlags flags) const;

private:
    CConstRef<COrgMod> m_Value;
};


class CFlatOrganelleQV : public IFlatQV
{
public:
    CFlatOrganelleQV(CBioSource::TGenome value) : m_Value(value) { }
    void Format(TFlatQuals& quals, const string& name, CFlatContext& ctx,
                TFlags flags) const;

private:
    CBioSource::TGenome m_Value;
};


class CFlatPubSetQV : public IFlatQV
{
public:
    CFlatPubSetQV(const CPub_set& value) : m_Value(&value) { }
    void Format(TFlatQuals& quals, const string& name, CFlatContext& ctx,
                TFlags flags) const;

private:
    CConstRef<CPub_set> m_Value;
};


class CFlatSeqDataQV : public IFlatQV
{
public:
    CFlatSeqDataQV(const CSeq_loc& value) : m_Value(&value) { }
    void Format(TFlatQuals& quals, const string& name, CFlatContext& ctx,
                TFlags flags) const;

private:
    CConstRef<CSeq_loc> m_Value;
};


class CFlatSeqIdQV : public IFlatQV
{
public:
    CFlatSeqIdQV(const CSeq_id& value) : m_Value(&value) { }
    void Format(TFlatQuals& quals, const string& name, CFlatContext& ctx,
                TFlags flags) const;

private:
    CConstRef<CSeq_id> m_Value;
};


class CFlatSeqLocQV : public IFlatQV
{
public:
    CFlatSeqLocQV(const CSeq_loc& value) : m_Value(&value) { }
    void Format(TFlatQuals& q, const string& n, CFlatContext& ctx,
                TFlags) const
        { x_AddFQ(q, n, CFlatLoc(*m_Value, ctx).GetString()); }

private:
    CConstRef<CSeq_loc> m_Value;
};


class CFlatSubSourceQV : public IFlatQV
{
public:
    CFlatSubSourceQV(const CSubSource& value) : m_Value(&value) { }
    void Format(TFlatQuals& quals, const string& name, CFlatContext& ctx,
                TFlags flags) const;

private:
    CConstRef<CSubSource> m_Value;
};


class CFlatXrefQV : public IFlatQV
{
public:
    typedef CSeq_feat::TDbxref TXref;
    CFlatXrefQV(const TXref& value) : m_Value(value) { }
    void Format(TFlatQuals& quals, const string& name, CFlatContext& ctx,
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
* Revision 1.5  2004/07/21 23:41:28  jcherry
* Removed const qualifier from argument passed by value
*
* Revision 1.4  2004/01/27 17:12:06  ucko
* Add missing #include directive for Code_break.hpp.
*
* Revision 1.3  2003/06/02 16:01:39  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.2  2003/03/21 18:47:47  ucko
* Turn most structs into (accessor-requiring) classes; replace some
* formerly copied fields with pointers to the original data.
*
* Revision 1.1  2003/03/10 16:39:08  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_FLAT___FLAT_QUALS__HPP */
