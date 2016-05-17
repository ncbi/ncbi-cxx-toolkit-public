#ifndef OBJTOOLS_FORMAT_ITEMS___LOCUS_ITEM__HPP
#define OBJTOOLS_FORMAT_ITEMS___LOCUS_ITEM__HPP

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
*   Locus item for flat-file generator
*
*/
#include <corelib/ncbistd.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seq_inst.hpp>

#include <objtools/format/items/item_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CBioseq_Handle;
class CDate;
class CBioseqContext;
class IFormatter;


///////////////////////////////////////////////////////////////////////////
//
// Locus

class NCBI_FORMAT_EXPORT CLocusItem : public CFlatItem
{
public:
    typedef CMolInfo::TBiomol       TBiomol;
    typedef CSeq_inst::TStrand      TStrand;
    typedef CSeq_inst::TTopology    TTopology;

    CLocusItem(CBioseqContext& ctx);
    void Format(IFormatter& formatter, IFlatTextOStream& text_os) const;

    const string& GetName     (void) const;
    const string& GetFullName (void) const;
    size_t        GetLength   (void) const;
    TStrand       GetStrand   (void) const;
    TBiomol       GetBiomol   (void) const;
    TTopology     GetTopology (void) const;
    const string& GetDivision (void) const;
    const string& GetDate     (void) const;

    static string GetDivision(const CBioseq_Handle& bsh);

private:

    void x_GatherInfo(CBioseqContext& ctx);
    void x_SetName(CBioseqContext& ctx);
    void x_SetLength(CBioseqContext& ctx);
    void x_SetStrand(CBioseqContext& ctx);
    void x_SetBiomol(CBioseqContext& ctx);
    void x_SetTopology(CBioseqContext& ctx);
    void x_SetDivision(CBioseqContext& ctx);
    void x_SetDate(CBioseqContext& ctx);

    const CDate* x_GetDateForBioseq(const CBioseq_Handle& bsh) const;
    const CDate* x_GetLaterDate(const CDate* d1, const CDate* d2) const;

    bool x_NameHasBadChars(const string& name) const;

    // data
    string          m_Name;
    string          m_FullName;
    size_t          m_Length;
    TStrand         m_Strand;
    TBiomol         m_Biomol;
    TTopology       m_Topology;
    string          m_Division;
    string          m_Date;
};


/////////////////////////////////////////////////////////////////////////////
//
// inline methods

inline
const string& CLocusItem::GetName(void) const
{
    return m_Name;
}


inline
const string& CLocusItem::GetFullName(void) const
{
    return m_FullName;
}


inline
size_t CLocusItem::GetLength(void) const
{
    return m_Length;
}


inline
CLocusItem::TStrand CLocusItem::GetStrand(void) const
{
    return m_Strand;
}


inline
CLocusItem::TBiomol CLocusItem::GetBiomol(void) const
{
    return m_Biomol;
}


inline
CLocusItem::TTopology CLocusItem::GetTopology (void) const
{
    return m_Topology;
}


inline
const string& CLocusItem::GetDivision(void) const
{
    return m_Division;
}


inline
const string& CLocusItem::GetDate(void) const
{
    return m_Date;
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT_ITEMS___LOCUS_ITEM__HPP */
