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
* Author:  Frank Ludwig, Sergiy Gotvyanskyy, NCBI
*
* File Description:
*   Reader for selected data file formats
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include "fasta_ex.hpp"

//#include <objects/seqloc/Seq_id.hpp>
//#include <objects/seqloc/Seq_loc.hpp>
//#include <objects/seq/Seq_annot.hpp>
#include <objects/seqset/Seq_entry.hpp>
//#include <objects/seqfeat/Variation_ref.hpp>

#include <objects/seq/Seq_descr.hpp>
//#include <objects/seqfeat/Org_ref.hpp>
//#include <objects/seqfeat/OrgName.hpp>
#include <objects/submit/Seq_submit.hpp>
//#include <objects/submit/Submit_block.hpp>
//#include <objects/pub/Pub.hpp>
//#include <objects/pub/Pub_equiv.hpp>
//#include <objects/biblio/Cit_sub.hpp>
//#include <objects/seq/Pubdesc.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/general/Date.hpp>
//#include <objects/biblio/Cit_sub.hpp>

#include "table2asn_context.hpp"

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


CFastaReaderEx::CFastaReaderEx(CTable2AsnContext& context, ILineReader& reader, TFlags flags) :
        objects::CFastaReader(reader, flags), m_context(context),
        m_gap_editor((objects::CSeq_gap::EType)m_context.m_gap_type, m_context.m_gap_evidences, m_context.m_gapNmin, m_context.m_gap_Unknown_length)
    {
    };

void CFastaReaderEx::AssignMolType(objects::ILineErrorListener * pMessageListener)
    {
        objects::CFastaReader::AssignMolType(pMessageListener);
        CSeq_inst& inst = SetCurrentSeq().SetInst();
        if (!inst.IsSetMol() || inst.GetMol() == CSeq_inst::eMol_na)
            inst.SetMol(CSeq_inst::eMol_dna);
    }
CRef<objects::CSeq_entry> CFastaReaderEx::ReadDIFasta(objects::ILineErrorListener * pMessageListener)
    {
        CRef<CSeq_entry> entry;
        while (!GetLineReader().AtEOF())
        {
            try {
                CRef<CSeq_entry> single(ReadOneSeq(pMessageListener));
                if (single.Empty())
                    break;

                if (entry.Empty()) {
                    entry = single;
                }

                else {
                    if (!entry->GetSeq().GetInst().IsSetExt())
                    {   //convert bioseq into delta
                        m_gap_editor.ConvertBioseqToDelta(entry->SetSeq());
                    }
                    m_gap_editor.AppendGap(entry->SetSeq());
                    m_gap_editor.AddBioseqAsLiteral(entry->SetSeq(), single->SetSeq());
                }
            }
            catch (CObjReaderParseException& e) {
                if (e.GetErrCode() == CObjReaderParseException::eEOF) {
                    break;
                }
                else {
                    throw;
                }
            }
        }

        entry->Parentize();
        return entry;
    }

void CFastaReaderEx::AssembleSeq(objects::ILineErrorListener * pMessageListener)
    {
        CFastaReader::AssembleSeq(pMessageListener);

        CAutoAddDesc molinfo_desc(SetCurrentSeq().SetDescr(), CSeqdesc::e_Molinfo);

        if (!molinfo_desc.Set().SetMolinfo().IsSetBiomol())
            molinfo_desc.Set().SetMolinfo().SetBiomol(CMolInfo::eBiomol_genomic);

        //if (!m_context.m_HandleAsSet)
        {
            CAutoAddDesc create_date_desc(SetCurrentSeq().SetDescr(), CSeqdesc::e_Create_date);
            if (create_date_desc.IsNull())
            {
                CRef<CDate> date(new CDate(CTime(CTime::eCurrent), CDate::ePrecision_day));
                create_date_desc.Set().SetCreate_date(*date);
            }
        }

    }
};
