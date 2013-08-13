#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "table2asn_context.hpp"

#include <objects/submit/Submit_block.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>

#include <objects/taxon1/taxon1.hpp>



BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

CRef<CSerialObject> CTable2AsnContext::UseTemplate(CBioseq*& bioseq) const
{
	if (m_submit_template.NotNull())
	{
		CRef<CSeq_submit> submit(new CSeq_submit);
		submit->Assign(*m_submit_template);

		CSeq_submit_Base::C_Data::TEntrys& data = submit->SetData().SetEntrys();
		if (data.empty())
		{
			CRef<CSeq_entry> entry(new CSeq_entry);
			entry->SetSeq();
		}
		bioseq = &(data.front()->SetSeq());

		CRef<CSerialObject> result(submit);
		return result;
	}
	else
	{
		CRef<CSeq_entry> entry(new CSeq_entry);
		bioseq = &(entry->SetSeq());

		if (m_entry_template.NotNull())
			entry->Assign(*m_entry_template);

		CRef<CSerialObject> result(entry);
		return result;
	}
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
			cerr << endl << "Error: Conflicting taxonomy info provided: taxid " << m_taxid << ": ";
			if (taxid <= 0)
			    m_taxid = taxid;
			else 
			{
				taxon.Fini();
				cerr << "taxonomy ID for the name '" << m_taxname << "' was determined as " << taxid << endl;
				return;
			}
		}
    }

    if (m_taxid <= 0) 
	{
		taxon.Fini();
		cerr << endl << "Error: No unique taxonomy ID found for the name '" << m_taxname << "'" << endl;
		return;
    }

    CConstRef<COrg_ref> org = taxon.GetOrgRef(m_taxid, is_species, is_uncultured, blast_name);
	m_taxname = org->GetTaxname();

    taxon.Fini();
}



END_NCBI_SCOPE

