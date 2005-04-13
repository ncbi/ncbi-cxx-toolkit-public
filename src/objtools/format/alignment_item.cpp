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
* Authors:  Mati Shomrat, NCBI;  Aaron Ucko, NCBI
*
* File Description:
*   Alignment item for flat-file
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/alignment_item.hpp>
#include <objtools/format/context.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CAlignmentItem::CAlignmentItem(const CSeq_align& align, CBioseqContext& ctx)
    : CFlatItem(&ctx), m_Align(&align)
{
}


void CAlignmentItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const

{
    formatter.FormatAlignment(*this, text_os);
}


void CAlignmentItem::x_GatherInfo(CBioseqContext& ctx)
{
    // !!!
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2005/04/13 14:37:01  ucko
* Accept const Seq_align objects, as there's no reason not to.
*
* Revision 1.1  2004/06/21 18:52:45  ucko
* Add an item type for Seq-aligns (handled by GFF 3)
*
* ===========================================================================
*/
