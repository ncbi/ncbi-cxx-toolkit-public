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
* Author:  Sergiy Gotvyanskyy, NCBI
*
* File Description:
*   Context structure holding all table2asn parameters
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/submit/Seq_submit.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/taxon1/taxon1.hpp>
#include "table2asn_context.hpp"

#include <algo/sequence/orf.hpp>

#include <objtools/readers/message_listener.hpp>

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE

    USING_SCOPE(objects);

CTable2AsnContext::CTable2AsnContext():
    m_output(0),
    m_HandleAsSet(false),
    m_GenomicProductSet(false),
    m_SetIDFromFile(false),
    m_RemoteTaxonomyLookup(false),
    m_ProjectVersionNumber(0),
    m_flipped_struc_cmt(false),
    m_taxid(0),
    m_logger(0),
    m_RemotePubLookup(false)
{
}

CTable2AsnContext::~CTable2AsnContext()
{
}

void CTable2AsnContext::AddUserTrack(CSeq_descr& SD, const string& type, const string& lbl, const string& data) const
{
    if (data.empty())
        return;

    CRef<CObject_id> oi(new CObject_id);
    oi->SetStr(type);
    CRef<CUser_object> uo(new CUser_object);
    uo->SetType(*oi);
    vector <CRef< CUser_field > >& ud = uo->SetData();
    CRef<CUser_field> uf(new CUser_field);
    uf->SetLabel().SetStr(lbl);
    uf->SetNum(1);
    uf->SetData().SetStr(data);
    ud.push_back(uf);

    CSeq_descr::Tdata& TD = SD.Set();
    CRef<CSeqdesc> sd(new CSeqdesc());
    sd->Select(CSeqdesc::e_User);
    sd->SetUser(*uo);

    TD.push_back(sd);
}

void CTable2AsnContext::RemoteRequestTaxid()
{
    if (m_taxname.empty() && m_taxid <= 0)
        return;

    CTaxon1 taxon;
    bool is_species, is_uncultured;
    string blast_name;

    taxon.Init();


    if (!m_taxname.empty())
    {
        int taxid = taxon.GetTaxIdByName(m_taxname);
        if (m_taxid == 0)
            m_taxid = taxid;
        else
            if (m_taxid != taxid)
            {
                m_logger->PutError(
                    CLineError(ILineError::eProblem_Unset, eDiag_Error, "", 0, 
                      "Conflicting taxonomy info provided: taxid " + NStr::IntToString(m_taxid)));

                if (taxid <= 0)
                    m_taxid = taxid;
                else
                {
                    taxon.Fini();
                    m_logger->PutError(
                        CLineError(ILineError::eProblem_Unset, eDiag_Error, "", 0, 
                        "taxonomy ID for the name '" + m_taxname + "' was determined as " + NStr::IntToString(taxid)));
                    return;
                }
            }
    }

    if (m_taxid <= 0)
    {
        taxon.Fini();
        m_logger->PutError(
            CLineError(ILineError::eProblem_Unset, eDiag_Error, "", 0, 
                "No unique taxonomy ID found for the name '" + m_taxname + "'"));
        return;
    }

    CConstRef<COrg_ref> org = taxon.GetOrgRef(m_taxid, is_species, is_uncultured, blast_name);
    m_taxname = org->GetTaxname();

    taxon.Fini();
}

CRef<CSeq_annot> FindORF(const CBioseq& bioseq)
{
    if (bioseq.IsNa())
    {
        COrf::TLocVec orfs;
        CSeqVector  seq_vec(bioseq);
        COrf::FindOrfs(seq_vec, orfs);
        if (orfs.size()>0)
        {
            CRef<CSeq_id> seqid(new CSeq_id);
            seqid->Assign(*bioseq.GetId().begin()->GetPointerOrNull());
            CRef<CSeq_annot> annot = COrf::MakeCDSAnnot(orfs, 1, seqid);
            return annot;
        }
    }
    return CRef<CSeq_annot>();
}

void CTable2AsnContext::FindOpenReadingFrame(objects::CSeq_entry& entry)
{
    switch(entry.Which())
    {
    case CSeq_entry::e_Seq:
        {
        CRef<CSeq_annot> annot = FindORF(entry.SetSeq());
        if (annot.NotEmpty())
        {
            entry.SetSeq().SetAnnot().push_back(annot);
        }
        }
        break;
    case CSeq_entry::e_Set:
        NON_CONST_ITERATE(CSeq_entry::TSet::TSeq_set, it, entry.SetSet().SetSeq_set())
        {
            FindOpenReadingFrame(**it);
        }
    }
}

END_NCBI_SCOPE

