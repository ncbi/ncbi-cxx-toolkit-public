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
*   flat-file generator -- base count item implementation
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_vector_ci.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/basecount_item.hpp>
#include <objtools/format/context.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CBaseCountItem::CBaseCountItem(CBioseqContext& ctx) :
    CFlatItem(&ctx),
    m_A(0), m_C(0), m_G(0), m_T(0), m_Other(0)
{
    x_GatherInfo(ctx);
}

IFlatItem::EItem CBaseCountItem::GetItemType(void) const
{
    return eItem_BaseCount;
}

void CBaseCountItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const

{
    formatter.FormatBasecount(*this, text_os);
}


/***************************************************************************/
/*                                  PRIVATE                                */
/***************************************************************************/


void CBaseCountItem::x_GatherInfo(CBioseqContext& ctx)
{
    if ( ctx.IsProt() ) {
        x_SetSkip();
        return;
    }
    
    CSeqVector v(ctx.GetLocation(), ctx.GetHandle().GetScope(),
                 CBioseq_Handle::eCoding_Iupac);
    const size_t COUNT = kMax_UChar+1;
    TSeqPos counters[COUNT] = {}; // "{}" will set all counters to zero

    // every how many bases do we check if the flatfile generator is canceled?
    // (feel free to adjust this number if it makes things faster)
    const static TSeqPos kCancelCheckBases = 4096;

    CSeqVector_CI it(v, 0, CSeqVector_CI::eCaseConversion_lower);
    TSeqPos count = v.size();
    while ( count > 0 ) {
        const TSeqPos uBasesToCopyRightNow = min( kCancelCheckBases, count );
        ITERATE_SIMPLE(uBasesToCopyRightNow) {
            ++counters[*it];
            ++it;
        }
        count -= uBasesToCopyRightNow;
        ctx.ThrowIfCanceled();
    }
    m_A = counters[Uchar('a')];
    m_C = counters[Uchar('c')];
    m_G = counters[Uchar('g')];
    m_T = counters[Uchar('t')];
    m_Other = v.size() - m_A - m_C - m_G - m_T;
}


END_SCOPE(objects)
END_NCBI_SCOPE
