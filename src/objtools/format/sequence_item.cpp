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
* Author:  Mati Shomrat, NCBI
*
* File Description:
*   flat-file generator -- origin item implementation
*
*/
#include <corelib/ncbistd.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/sequence_item.hpp>
#include <objtools/format/context.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSequenceItem::CSequenceItem
(TSeqPos from,
 TSeqPos to,
 bool first, 
 CFFContext& ctx) :
    CFlatItem(ctx),
    m_From(from), m_To(to - 1), m_First(first)
{
    x_GatherInfo(ctx);
}


void CSequenceItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const

{
    formatter.FormatSequence(*this, text_os);
}


const CSeqVector& CSequenceItem::GetSequence(void) const
{
    return m_Sequence;
}


TSeqPos CSequenceItem::GetFrom(void) const
{
    return m_From + 1;
}


TSeqPos CSequenceItem::GetTo(void) const
{
    return m_To + 1;
}


bool CSequenceItem::IsFirst(void) const
{
    return m_First;
}


/***************************************************************************/
/*                                  PRIVATE                                */
/***************************************************************************/


void CSequenceItem::x_GatherInfo(CFFContext& ctx)
{
    x_SetObject(ctx.GetActiveBioseq());

    CSeq_loc loc;
    CSeq_loc::TInt& interval = loc.SetInt();
    CRef<CSeq_id> id(new CSeq_id);
    id->Assign(*ctx.GetPrimaryId());

    interval.SetId(*id);
    interval.SetFrom(m_From);
    interval.SetTo(m_To);

    
    m_Sequence = ctx.GetHandle().GetSequenceView(
        loc, 
        CBioseq_Handle::eViewConstructed);

    CSeqVector::TCoding code = CSeq_data::e_Iupacna;
    if ( ctx.IsProt() ) {
        code = ctx.IsModeRelease() ? CSeq_data::e_Iupacaa : CSeq_data::e_Ncbieaa;
    }
    m_Sequence.SetCoding(code);
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2004/02/11 22:56:37  shomrat
* use IsModeRelease method
*
* Revision 1.2  2003/12/18 17:43:36  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:24:39  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/
