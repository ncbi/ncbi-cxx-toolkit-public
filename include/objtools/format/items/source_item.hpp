#ifndef OBJTOOLS_FORMAT_ITEMS___SOURCE_ITEM__HPP
#define OBJTOOLS_FORMAT_ITEMS___SOURCE_ITEM__HPP

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
*          Mati Shomrat
*
* File Description:
*   Source item for flat-file generator
*
*/
#include <corelib/ncbistd.hpp>

#include <list>

#include <objtools/format/items/item_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CGB_block;
class CSeqdesc;
class CBioSource;
class CBioseqContext;
class IFormatter;


///////////////////////////////////////////////////////////////////////////
//
// SOURCE
//   ORGANISM

class NCBI_FORMAT_EXPORT CSourceItem : public CFlatItem
{
public:
    typedef int TTaxid;

    static const TTaxid kInvalidTaxid;

    CSourceItem(CBioseqContext& ctx);
    CSourceItem(CBioseqContext& ctx, const CBioSource& bsrc, const CSerialObject& obj);
    void Format(IFormatter& formatter, IFlatTextOStream& text_os) const;

    const string&       GetTaxname      (void) const;
    const string&       GetCommon       (void) const;
    const string&       GetLineage      (void) const;
    const string&       GetOrganelle    (void) const;
    const string&       GetSourceLine   (void) const;
    const list<string>& GetMod          (void) const;
    TTaxid              GetTaxid        (void) const;
    bool                IsUsingAnamorph (void) const;

private:
    void x_GatherInfo(CBioseqContext& ctx);
    void x_GatherInfo(CBioseqContext& ctx, const CBioSource& bsrc, const CSerialObject& obj);

    // Populate the object based on the source of the data
    void x_SetSource(const CGB_block&  gb, const CSeqdesc& desc);
    void x_SetSource(const CBioSource& bsrc, const CSerialObject& obj);

    // static members
    static const string       scm_Unknown;
    static const string       scm_Unclassified;
    static const list<string> scm_EmptyList;

    // data
    const string*       m_Taxname;
    const string*       m_Common;
    const string*       m_Organelle;
    string              m_Lineage;       // semicolon-delimited
    const string*       m_SourceLine;    // for "old" format style
    const list<string>* m_Mod;
    TTaxid              m_Taxid;
    bool                m_UsingAnamorph;
};


/////////////////////////////////////////////////////////////////////////////
//
// inline methods

inline
const string& CSourceItem::GetTaxname(void) const
{ 
    return *m_Taxname;
}


inline
const string& CSourceItem::GetCommon(void) const
{ 
    return *m_Common;
}


inline
const string& CSourceItem::GetLineage(void) const
{ 
    return m_Lineage;
}


inline
const string& CSourceItem::GetOrganelle(void) const
{ 
    return *m_Organelle;
}


inline
const string& CSourceItem::GetSourceLine(void) const
{ 
    return *m_SourceLine;
}


inline
const list<string>& CSourceItem::GetMod(void) const
{ 
    return *m_Mod;
}


inline
CSourceItem::TTaxid CSourceItem::GetTaxid(void) const
{
    return m_Taxid;
}


inline
bool CSourceItem::IsUsingAnamorph(void) const
{ 
    return m_UsingAnamorph;
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT_ITEMS___SOURCE_ITEM__HPP */
