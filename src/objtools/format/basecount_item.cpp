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
    
    CSeqVector v = ctx.GetHandle().GetSequenceView(
            ctx.GetLocation(),
            CBioseq_Handle::eViewConstructed,
            CBioseq_Handle::eCoding_Iupac);

    ITERATE (CSeqVector, it, v) {
        CSeqVector_CI::TResidue res = toupper(*it);
        switch ( res ) {
        case 'A':
            ++m_A;
            break;
        case 'C':
            ++m_C;
            break;
        case 'G':
            ++m_G;
            break;
        case 'T':
            ++m_T;
            break;
        default:
            ++m_Other;
            break;
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
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
