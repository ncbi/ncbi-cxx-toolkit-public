#ifndef OBJTOOLS_FORMAT_ITEMS___BASECOUNT_ITEM__HPP
#define OBJTOOLS_FORMAT_ITEMS___BASECOUNT_ITEM__HPP

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
*   base count item for flat-file generator
*   
*
*/
#include <corelib/ncbistd.hpp>

#include <list>

#include <objtools/format/items/item_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CBioseqContext;
class IFormatter;


///////////////////////////////////////////////////////////////////////////
//
// Base Count

class CBaseCountItem : public CFlatItem
{
public:
    CBaseCountItem(CBioseqContext& ctx);
    void Format(IFormatter& formatter, IFlatTextOStream& text_os) const;
    
    void GetCounts(SIZE_TYPE& a, SIZE_TYPE& c, SIZE_TYPE& g, SIZE_TYPE& t,
        SIZE_TYPE& other) const;
    SIZE_TYPE GetA(void) const { return m_A; }
    SIZE_TYPE GetC(void) const { return m_C; }
    SIZE_TYPE GetG(void) const { return m_G; }
    SIZE_TYPE GetT(void) const { return m_T; }
    SIZE_TYPE GetOther(void) const  { return m_Other; }

private:
    void x_GatherInfo(CBioseqContext& ctx);

    // data
    mutable TSeqPos     m_A, m_C, m_G, m_T, m_Other;
};


inline
void CBaseCountItem::GetCounts
(SIZE_TYPE& a,
 SIZE_TYPE& c,
 SIZE_TYPE& g,
 SIZE_TYPE& t,
 SIZE_TYPE& other) const
{
    a = m_A;
    c = m_C;
    g = m_G;
    t = m_T;
    other = m_Other;
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2004/04/22 15:34:12  shomrat
* Changes in context
*
* Revision 1.1  2003/12/17 19:44:53  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/


#endif  /* OBJTOOLS_FORMAT_ITEMS___BASECOUNT_ITEM__HPP */
