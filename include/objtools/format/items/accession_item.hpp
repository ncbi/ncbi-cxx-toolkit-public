#ifndef OBJTOOLS_FORMAT_ITEMS___ACCESSION_ITEM__HPP
#define OBJTOOLS_FORMAT_ITEMS___ACCESSION_ITEM__HPP

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
*   Accession item for flat-file generator
*   
*
*/
#include <corelib/ncbistd.hpp>
#include <util/range.hpp>

#include <list>

#include <objtools/format/items/item_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_loc;
class CBioseqContext;
class IFormatter;


///////////////////////////////////////////////////////////////////////////
//
// Accession

class CAccessionItem : public CFlatItem
{
public:
    // typedef
    typedef vector<string>  TExtra_accessions;

    CAccessionItem(CBioseqContext& ctx);
    void Format(IFormatter& formatter, IFlatTextOStream& text_os) const;
    
    const string& GetAccession(void) const;
    const string& GetWGSAccession(void) const;
    const TExtra_accessions& GetExtraAccessions(void) const;
    bool  IsSetRegion(void) const;
    const CSeq_loc& GetRegion(void) const;
private:
    void x_GatherInfo(CBioseqContext& ctx);

    // data
    string              m_Accession;
    string              m_WGSAccession;
    TExtra_accessions   m_ExtraAccessions;
    CConstRef<CSeq_loc> m_Region;
    bool                m_IsSetRegion;
};



/////////////////////////////////////////////////////////////////////////////
//  inline methods

inline
const string& CAccessionItem::GetAccession(void) const
{
    return m_Accession;
}


inline
const string& CAccessionItem::GetWGSAccession(void) const
{
    return m_WGSAccession;
}


inline
const CAccessionItem::TExtra_accessions& CAccessionItem::GetExtraAccessions(void) const
{
    return m_ExtraAccessions;
}


inline
bool CAccessionItem::IsSetRegion(void) const
{
    return m_IsSetRegion;
}


inline
const CSeq_loc& CAccessionItem::GetRegion(void) const
{
    _ASSERT(m_Region);
    return *m_Region;
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2005/03/28 17:13:26  shomrat
* Support for complex user location (REGION)
*
* Revision 1.4  2005/03/02 16:26:49  shomrat
* Changed contaier type for secondary accessions
*
* Revision 1.3  2004/04/22 15:33:58  shomrat
* Changes in context; + Region
*
* Revision 1.2  2004/04/13 16:41:54  shomrat
* + GetWGSAccession(); inlined methods
*
* Revision 1.1  2003/12/17 19:44:34  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/


#endif  /* OBJTOOLS_FORMAT_ITEMS___ACCESSION_ITEM__HPP */
