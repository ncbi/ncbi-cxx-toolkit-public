#ifndef OBJTOOLS_FORMAT_ITEMS___SEQUENCE_ITEM__HPP
#define OBJTOOLS_FORMAT_ITEMS___SEQUENCE_ITEM__HPP

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
* Author:  Mati Shomrat
*
* File Description:
*   Origin item for flat-file generator
*   
*
*/
#include <corelib/ncbistd.hpp>

#include <objmgr/seq_vector.hpp>

#include <objtools/format/items/item_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CBioseqContext;
class IFormatter;


///////////////////////////////////////////////////////////////////////////
//
// SEQUENCE

class CSequenceItem : public CFlatItem
{
public:
    CSequenceItem(TSeqPos from, TSeqPos to, bool first, 
                CBioseqContext& ctx);
    void Format(IFormatter& formatter, IFlatTextOStream& text_os) const;
    
    const CSeqVector& GetSequence(void) const;
    TSeqPos GetFrom(void) const;
    TSeqPos GetTo(void) const;
    bool IsFirst(void) const;

private:
    void x_GatherInfo(CBioseqContext& ctx);

    // data
    TSeqPos     m_From, m_To;
    bool        m_First;
    CSeqVector  m_Sequence;
};


//===========================================================================
//                              inline methods
//===========================================================================

inline
const CSeqVector& CSequenceItem::GetSequence(void) const
{
    return m_Sequence;
}

inline
TSeqPos CSequenceItem::GetFrom(void) const
{
    return m_From;
}

inline
TSeqPos CSequenceItem::GetTo(void) const
{
    return m_To;
}

inline
bool CSequenceItem::IsFirst(void) const
{
    return m_First;
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2005/03/28 17:16:04  shomrat
* Modifying internal representation
*
* Revision 1.4  2004/11/24 16:47:33  shomrat
* Inlined methods
*
* Revision 1.3  2004/04/22 15:39:54  shomrat
* Changes in context
*
* Revision 1.2  2004/02/19 17:50:06  shomrat
* correct typo
*
* Revision 1.1  2003/12/17 19:50:01  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/


#endif  /* OBJTOOLS_FORMAT_ITEMS___SEQUENCE_ITEM__HPP */
