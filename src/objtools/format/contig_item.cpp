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
*   Contig item for flat-file
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seg_ext.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/contig_item.hpp>
#include <objtools/format/items/flat_seqloc.hpp>
#include <objtools/format/context.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CContigItem::CContigItem(CBioseqContext& ctx) :
    CFlatItem(&ctx), m_Loc(new CSeq_loc)
{
    x_GatherInfo(ctx);
}


void CContigItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const
{
    formatter.FormatContig(*this, text_os);
}


void CContigItem::x_GatherInfo(CBioseqContext& ctx)
{
    typedef CSeq_loc_mix::Tdata::value_type TLoc;

    if ( !ctx.GetHandle().IsSetInst_Ext() ) {
        return;
    }

    CSeq_loc_mix::Tdata& data = m_Loc->SetMix().Set();
    const CSeq_ext& const_ext = ctx.GetHandle().GetInst_Ext();
    CSeq_ext& ext = const_cast<CSeq_ext&>(const_ext);

    if (ctx.IsSegmented()) {
        ITERATE (CSeg_ext::Tdata, it, ext.GetSeg().Get()) {
            data.push_back(*it);
        }
    } else if ( ctx.IsDelta() ) {
        NON_CONST_ITERATE (CDelta_ext::Tdata, it, ext.SetDelta().Set()) {
            if ((*it)->IsLoc()) {
                data.push_back(TLoc(&((*it)->SetLoc())));
            } else {  // literal
                const CSeq_literal& lit = (*it)->GetLiteral();
                TSeqPos len = lit.CanGetLength() ? lit.GetLength() : 0;
                CRef<CFlatGapLoc> flat_loc(new CFlatGapLoc(len));
                if (lit.IsSetFuzz()) {
                    flat_loc->SetFuzz(&lit.GetFuzz());
                }
                data.push_back(TLoc(&*flat_loc));
            }
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
