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
*   flat-file generator -- defline item implementation
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objmgr/util/sequence.hpp>
#include <objmgr/util/create_defline.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/defline_item.hpp>
#include <objtools/format/context.hpp>
#include <objmgr/util/objutil.hpp>
#include <objmgr/seqdesc_ci.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CDeflineItem::CDeflineItem(CBioseqContext& ctx) :
    CFlatItem(&ctx)
{
    x_GatherInfo(ctx);
}

IFlatItem::EItem CDeflineItem::GetItemType(void) const
{
    return eItem_Defline;
}

void CDeflineItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const

{
    formatter.FormatDefline(*this, text_os);
}


const string& CDeflineItem::GetDefline(void) const
{
    return m_Defline;
}


/***************************************************************************/
/*                                  PRIVATE                                */
/***************************************************************************/


void CDeflineItem::x_GatherInfo(CBioseqContext& ctx)
{

    sequence::CDeflineGenerator Defliner;
    CConstRef<CBioseq> bioseq = ctx.GetHandle().GetBioseqCore();
    CScope& scope = ctx.GetScope();
    sequence::CDeflineGenerator::TUserFlags flags = 0;
    if ( ! ctx.Config().DoNotUseAutoDef() ) {
        flags |= sequence::CDeflineGenerator::fUseAutoDef;
    }
    if ( ctx.Config().IgnoreExistingTitle() ) {
        flags |= sequence::CDeflineGenerator::fIgnoreExisting;
    }
    if ( ctx.Config().ShowDeflineModifiers() ) {
        flags |= sequence::CDeflineGenerator::fShowModifiers;
    }
    if ( ctx.UsingSeqEntryIndex() ) {
        CRef<CSeqEntryIndex> idx = ctx.GetSeqEntryIndex();
        CBioseq_Handle bsh = scope.GetBioseqHandle(*bioseq);
        m_Defline = Defliner.GenerateDefline( bsh, *idx, flags );
    } else {
        m_Defline = Defliner.GenerateDefline( *bioseq, scope, flags );
    }
    if (! Defliner.UsePDBCompoundForDefline()) {
        ctx.SetPDBCompoundForComment(true);
    }
    if ( ctx.Config().ShowDeflineModifiers() ) {
        CompressSpaces( m_Defline );
    } else {
        CleanAndCompress (m_Defline, m_Defline.c_str());
    }
    ConvertQuotes(m_Defline);
    AddPeriod(m_Defline);
    CSeqdesc_CI di(ctx.GetHandle(), CSeqdesc::e_Title);
    if (di) {
        x_SetObject(*di);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
