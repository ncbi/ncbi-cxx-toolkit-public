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
*   date item (for EMBL)
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/general/Date.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seqdesc_ci.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/date_item.hpp>
#include <objtools/format/context.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CDateItem::CDateItem(CBioseqContext& ctx) :
    CFlatItem(&ctx)
{
    x_GatherInfo(ctx);
}


void CDateItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const

{
    formatter.FormatDate(*this, text_os);
}


void CDateItem::x_GatherInfo(CBioseqContext& ctx)
{
    // create date
    {{
        CSeqdesc_CI desc(ctx.GetHandle(), CSeqdesc::e_Create_date);
        if ( desc ) {
            m_CreateDate.Reset(&desc->GetCreate_date());
        }
    }}

    // update date
    {{
        CSeqdesc_CI desc(ctx.GetHandle(), CSeqdesc::e_Update_date);
        if ( desc ) {
            m_UpdateDate.Reset(&desc->GetUpdate_date());
        }
    }}
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.4  2004/05/21 21:42:54  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.3  2004/04/22 15:52:22  shomrat
* Changes in context
*
* Revision 1.2  2003/12/18 17:43:32  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:19:22  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/
