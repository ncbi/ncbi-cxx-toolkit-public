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

    size_t counters[256];
    for (size_t i = 0; i < 256; ++i) {
        counters[i] = 0;
    }

    ITERATE (CSeqVector, it, v) {
        ++counters[static_cast<CSeqVector_CI::TResidue>(toupper(*it))];
    }
    m_A = counters[Uchar('A')];
    m_C = counters[Uchar('C')];
    m_G = counters[Uchar('G')];
    m_T = counters[Uchar('T')];
    m_Other = v.size() - m_A - m_C - m_G - m_T;
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.7  2005/03/28 17:17:17  shomrat
* Optimizing base count
*
* Revision 1.6  2004/12/06 17:54:10  grichenk
* Replaced calls to deprecated methods
*
* Revision 1.5  2004/05/21 21:42:54  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.4  2004/04/22 15:50:56  shomrat
* Changes in context
*
* Revision 1.3  2004/03/05 18:43:30  shomrat
* Use ITERATE instead of for
*
* Revision 1.2  2003/12/18 17:43:31  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:18:05  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/
