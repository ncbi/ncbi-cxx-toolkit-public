#ifndef OBJTOOLS_FORMAT_ITEMS___ALIGNMENT_ITEM__HPP
#define OBJTOOLS_FORMAT_ITEMS___ALIGNMENT_ITEM__HPP

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
*   Alignment item for flat-file generator (used for GFF 3)
*
*/

#include <objtools/format/items/item_base.hpp>
#include <objects/seqalign/Seq_align.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CBioseqContext;
class IFormatter;


///////////////////////////////////////////////////////////////////////////
//
// ALIGNMENT

class CAlignmentItem : public CFlatItem
{
public:
    CAlignmentItem(const CSeq_align& align, CBioseqContext& ctx);
    void Format(IFormatter& formatter, IFlatTextOStream& text_os) const;

    const CSeq_align& GetAlign(void) const { return *m_Align; }

private:
    void x_GatherInfo(CBioseqContext& ctx);
    // data
    CConstRef<CSeq_align> m_Align;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2005/04/13 14:36:56  ucko
* Accept const Seq_align objects, as there's no reason not to.
*
* Revision 1.1  2004/06/21 18:48:43  ucko
* Add an item type for Seq-aligns (handled by GFF 3)
*
* ===========================================================================
*/


#endif  /* OBJTOOLS_FORMAT_ITEMS___ALIGNMENT_ITEM__HPP */
