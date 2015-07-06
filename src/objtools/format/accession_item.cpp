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
*   flat-file generator -- accession item implementation
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seqdesc_ci.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/accession_item.hpp>
#include <objtools/format/context.hpp>
#include <objmgr/util/objutil.hpp>
#include <algorithm>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CAccessionItem::CAccessionItem(CBioseqContext& ctx) :
    CFlatItem(&ctx), m_ExtraAccessions(0), m_IsSetRegion(false)
{
    x_GatherInfo(ctx);
}


void CAccessionItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const
{
    formatter.FormatAccession(*this, text_os);
}



/***************************************************************************/
/*                                  PRIVATE                                */
/***************************************************************************/


void CAccessionItem::x_GatherInfo(CBioseqContext& ctx)
{
    if ( ctx.GetPrimaryId() == 0 ) {
        x_SetSkip();
        return;
    }

    const CSeq_id& id = *ctx.GetPrimaryId();

    // if no accession, do not show local or general in ACCESSION
    if ((id.IsGeneral()  ||  id.IsLocal())  &&
        (ctx.Config().IsModeEntrez()  ||  ctx.Config().IsModeGBench())) {
            return;
    }
    m_Accession = id.GetSeqIdString();

    if ( ctx.IsWGS()  && ctx.GetLocation().IsWhole() && ctx.GetTech() == CMolInfo::eTech_wgs ) {
        size_t acclen = m_Accession.length();
        m_WGSAccession = m_Accession;
        if (acclen >= 12) {
            size_t stem_len = (acclen >= 15) ? 7 : 6, tail_len = acclen - stem_len;
            if (m_Accession.find_first_not_of("0", stem_len) != NPOS) {
                m_WGSAccession.replace(stem_len, tail_len, tail_len, '0');
            } else {
                m_WGSAccession.erase();
            }
        }
        /*
        if (acclen == 12 && ! NStr::EndsWith(m_WGSAccession, "000000")) {
            m_WGSAccession.replace(6, acclen - 6, acclen - 6, '0');
        } else if (acclen == 13 && ! NStr::EndsWith(m_WGSAccession, "0000000")) {
            m_WGSAccession.replace(6, acclen - 7, acclen - 7, '0');
        } else if (acclen == 14 && ! NStr::EndsWith(m_WGSAccession, "00000000")) {
            m_WGSAccession.replace(6, acclen - 8, acclen - 8, '0');
        } else if (acclen == 15 && ! NStr::EndsWith(m_WGSAccession, "00000000")) {
            m_WGSAccession.replace(6, acclen - 8, acclen - 8, '0');
        } else {
            m_WGSAccession.erase();
        }
        */
    }

    // extra accessions not done if we're taking a slice 
    // (i.e. command-line args "-from" and "-to" )
    if( ctx.GetLocation().IsWhole() ) {

        const list<string>* xtra = 0;
        CSeqdesc_CI gb_desc(ctx.GetHandle(), CSeqdesc::e_Genbank);
        if ( gb_desc ) {
            x_SetObject(*gb_desc);
            xtra = &gb_desc->GetGenbank().GetExtra_accessions();
        }

        CSeqdesc_CI embl_desc(ctx.GetHandle(), CSeqdesc::e_Embl);
        if ( embl_desc ) {
            x_SetObject(*embl_desc);
            if( embl_desc->GetEmbl().GetExtra_acc().size() > 0 ) {
                xtra = &embl_desc->GetEmbl().GetExtra_acc();
            }
        }

        if ( xtra != 0 ) {
            // no validation done if less than a certain number of accessions
            // TODO: When we've switched completely away from C, we should
            //       probably *always* validate accessions.
            const int kAccessionValidationCutoff = 20;
            ITERATE (list<string>, it, *xtra) {
                if( xtra->size() >= kAccessionValidationCutoff ) {
                    if ( ! IsValidAccession(*it) ) { 
                        continue;
                    }
                }
                m_ExtraAccessions.push_back(*it);
            }
        }

        /// add GPipe accessions as extra
        ITERATE (CBioseq::TId, it, ctx.GetHandle().GetBioseqCore()->GetId()) {
            if ((*it)->IsGpipe()) {
                m_ExtraAccessions.push_back((*it)->GetGpipe().GetAccession());
            }
        }

        sort(m_ExtraAccessions.begin(), m_ExtraAccessions.end());

    } else {
        // specific region is set
        m_Region.Reset(&ctx.GetLocation());
        m_IsSetRegion = true;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
