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
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objmgr/util/sequence.hpp>

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
 CBioseqContext& ctx) :
    CFlatItem(&ctx),
    m_From(from), m_To(to), m_First(first)
{
    x_GatherInfo(ctx);
}


void CSequenceItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const

{
    formatter.FormatSequence(*this, text_os);
}


/***************************************************************************/
/*                                  PRIVATE                                */
/***************************************************************************/


void CSequenceItem::x_GatherInfo(CBioseqContext& ctx)
{
    x_SetObject(*ctx.GetHandle().GetBioseqCore());

    const CSeq_loc& loc = ctx.GetLocation();
    m_Sequence = CSeqVector(loc, ctx.GetScope());

    CSeqVector::TCoding coding = CSeq_data::e_Iupacna;
    if (ctx.IsProt()) {
        coding = ctx.Config().IsModeRelease() ?
            CSeq_data::e_Iupacaa : CSeq_data::e_Ncbieaa;
    }
    m_Sequence.SetCoding(coding);
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.13  2005/03/28 17:24:09  shomrat
* Changed internal representation
*
* Revision 1.12  2005/02/18 15:09:26  shomrat
* CSeq_loc interface changes
*
* Revision 1.11  2004/12/06 17:54:10  grichenk
* Replaced calls to deprecated methods
*
* Revision 1.10  2004/11/24 16:52:11  shomrat
* Inilined methods
*
* Revision 1.9  2004/11/19 15:15:16  shomrat
* Do not force fetching the complete bioseq
*
* Revision 1.8  2004/09/01 15:33:44  grichenk
* Check strand in GetStart and GetEnd. Circular length argument
* made optional.
*
* Revision 1.7  2004/05/21 21:42:54  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.6  2004/05/07 15:23:51  shomrat
* Removed unreferenced varaible
*
* Revision 1.5  2004/04/27 15:14:36  shomrat
* Bug fix
*
* Revision 1.4  2004/04/22 15:56:17  shomrat
* Support partial region
*
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
