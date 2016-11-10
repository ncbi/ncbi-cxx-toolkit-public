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
* Author: Colleen Bollin, NCBI
*
* File Description:
*   functions for editing and working with coding regions
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objtools/edit/mail_report.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqtable/SeqTable_column.hpp>
#include <objects/seqtable/SeqTable_multi_data.hpp>
#include <objects/taxon3/taxon3.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

CRef<CSeq_table> MakeMailReportPreReport(CSeq_entry_Handle seh)
{
    CRef<CSeq_table> table(new CSeq_table());
    CRef<CSeqTable_column> id_col(new CSeqTable_column());
    id_col->SetHeader().SetTitle("ID");
    table->SetColumns().push_back(id_col);
    CRef<CSeqTable_column> old_tax(new CSeqTable_column());
    old_tax->SetHeader().SetTitle("old taxname");
    table->SetColumns().push_back(old_tax);
    CRef<CSeqTable_column> is_barcode(new CSeqTable_column());
    is_barcode->SetHeader().SetTitle("is_barcode");
    table->SetColumns().push_back(is_barcode);

    for (CBioseq_CI bi(seh, CSeq_inst::eMol_na); bi; ++bi) {
        CSeqdesc_CI di(*bi, CSeqdesc::e_Source);
        string taxname = "";
        if (di && di->GetSource().IsSetOrg() && di->GetSource().GetOrg().IsSetTaxname()) {
            taxname = di->GetSource().GetOrg().GetTaxname();
        }
        CRef<CSeq_id> id(new CSeq_id());
        id->Assign(*(bi->GetSeqId()));
        id_col->SetData().SetId().push_back(id);
        old_tax->SetData().SetString().push_back(taxname);
        bool barcode = false;
        CSeqdesc_CI mi(*bi, CSeqdesc::e_Molinfo);
        if (mi && mi->GetMolinfo().IsSetTech() && mi->GetMolinfo().GetTech() == CMolInfo::eTech_barcode) {
            barcode = true;
        }
        is_barcode->SetData().SetInt().push_back(barcode ? 1 : 0);
    }

    return table;
}


void MakeMailReportPostReport(CSeq_table& table, CScope& scope)
{
    CRef<CSeqTable_column> new_tax(new CSeqTable_column());
    new_tax->SetHeader().SetTitle("new taxname");   
    table.SetColumns().push_back(new_tax);
    CRef<CSeqTable_column> taxon(new CSeqTable_column());
    taxon->SetHeader().SetTitle("tax ID");
    table.SetColumns().push_back(taxon);

    vector<CRef<COrg_ref> > org_ref_list;
    ITERATE(CSeqTable_column::TData::TId, id_it, table.GetColumns()[0]->GetData().GetId()) {
        CBioseq_Handle bsh = scope.GetBioseqHandle(**id_it);
        string taxname = "";
        int taxid = 0;
        CRef<COrg_ref> org(new COrg_ref());
        CSeqdesc_CI di(bsh, CSeqdesc::e_Source);        
        if (di && di->GetSource().IsSetOrg()) {
            if (di->GetSource().GetOrg().IsSetTaxname()) {
                taxname = di->GetSource().GetOrg().GetTaxname();
            }
            if (di->GetSource().GetOrg().IsSetDb()) {
                ITERATE(COrg_ref::TDb, db_it, di->GetSource().GetOrg().GetDb()) {
                    if ((*db_it)->IsSetDb() && NStr::EqualNocase((*db_it)->GetDb(), "taxon")
                        && (*db_it)->IsSetTag() && (*db_it)->GetTag().IsId()) {
                        taxid = (*db_it)->GetTag().GetId();
                    }
                }
            }
            org->Assign(di->GetSource().GetOrg());
        } else {
            org->SetTaxname("");
        }
        new_tax->SetData().SetString().push_back(taxname);
        taxon->SetData().SetInt().push_back(taxid);
        org_ref_list.push_back(org);
    }

    CRef<CSeqTable_column> pub_stat(new CSeqTable_column());
    pub_stat->SetHeader().SetTitle("is_unpublished");
    table.SetColumns().push_back(pub_stat);
    CTaxon3 taxon3;
	taxon3.Init();
	CRef<CTaxon3_reply> reply = taxon3.SendOrgRefList(org_ref_list);
    if (reply) {
        ITERATE(CTaxon3_reply::TReply, reply_it, reply->GetReply()) {
            bool is_unpub = false;
            if ((*reply_it)->IsData() && (*reply_it)->GetData().IsSetStatus()) {
                ITERATE(CT3Reply::TData::TStatus, status_it, (*reply_it)->GetData().GetStatus()) {
                    if ((*status_it)->IsSetProperty() 
                         && NStr::EqualNocase((*status_it)->GetProperty(), "unpublished_name")) {
                        is_unpub = true;
                        break;
                    }
                }
            }
            pub_stat->SetData().SetInt().push_back(is_unpub ? 1 : 0);
        }
    } else {
        for (size_t i = 0; i < table.GetColumn(0).GetData().GetSize(); i++) {
            pub_stat->SetData().SetInt().push_back(0);
        }
    }    
}


void PrintReportLineHeader(CNcbiOstrstream& lines)
{
    lines << "Accession\tOld Name\tNew Name\n";
}


void ReportMailReportLine(CNcbiOstrstream& lines, const CSeq_table& table, size_t i, CScope* scope = NULL)
{
    string id;
    if (scope)
    {
        CBioseq_Handle bsh =  scope->GetBioseqHandle(*table.GetColumns()[0]->GetData().GetId()[i]);
        CSeq_id_Handle best = sequence::GetId(bsh, sequence::eGetId_Best);
        best.GetSeqId()->GetLabel(&id, CSeq_id::eContent);
    }
    else
    {
        table.GetColumns()[0]->GetData().GetId()[i]->GetLabel(&id, CSeq_id::eContent);
    }
    lines << id;
    lines << "\t";
    lines << table.GetColumns()[1]->GetData().GetString()[i];
    lines << "\t";
    lines << table.GetColumns()[3]->GetData().GetString()[i];
    lines << "\n";
}


string GetReportFromMailReportTable(const CSeq_table& table, CScope* scope)
{
    CNcbiOstrstream lines;

    lines << "Failed Lookups\n";
    PrintReportLineHeader(lines);
    for (size_t i = 0; i < table.GetColumns().front()->GetData().GetSize(); i++) {
        if (table.GetColumns()[4]->GetData().GetInt()[i] == 0) {
            ReportMailReportLine(lines, table, i, scope);
        }
    }
    lines << "\n\nSp. Replaced with Real\n";
    PrintReportLineHeader(lines);
    for (size_t i = 0; i < table.GetColumns().front()->GetData().GetSize(); i++) {
        if (NStr::Find(table.GetColumns()[1]->GetData().GetString()[i], " sp.") != string::npos
            && NStr::Find(table.GetColumns()[3]->GetData().GetString()[i], " sp.") == string::npos) {
            ReportMailReportLine(lines, table, i, scope);
        }
    }

    lines << "\n\nUnpublished Names\n";
    PrintReportLineHeader(lines);
    for (size_t i = 0; i < table.GetColumns().front()->GetData().GetSize(); i++) {
        if (table.GetColumns()[5]->GetData().GetInt()[i] != 0) {
            ReportMailReportLine(lines, table, i, scope);
        }
    }

    return string(CNcbiOstrstreamToString(lines));
}



END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

