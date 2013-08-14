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
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/taxon1/taxon1.hpp>



BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

CTable2AsnContext::CTable2AsnContext():
	m_output(0),
	//m_make_set(false),
	sHandleAsSet(false),
	gGenomicProductSet(false),
	qSetIDFromFile(false),
	TRemoteTaxonomyLookup(false),
	NProjectVersionNumber(0),
	m_taxid(0)
{
}

CTable2AsnContext::~CTable2AsnContext()
{
}


CBioseq* CTable2AsnContext::CreateNextBioSeqFromTemplate(CSeq_entry& container, bool make_set) const
{
	if (make_set)
	{
		container.SetSet();

		CRef<CSeq_entry> new_entry(new CSeq_entry);
		container.SetSet().SetSeq_set().push_back(new_entry);
		if (m_entry_template.NotNull())
		  new_entry->Assign(*m_entry_template);

		return &new_entry->SetSeq();
	}
	else
	{
		container.SetSeq();

		if (m_entry_template.NotNull())
		  container.Assign(*m_entry_template);

		return &container.SetSeq();
	}
}

CBioseq* CTable2AsnContext::GetNextBioSeqFromTemplate(CRef<CSerialObject>& container, bool make_set) const
{
	if (m_submit_template.NotNull())
	{	
		CSeq_submit* submit = 0;
		if (container.NotNull())
			submit = dynamic_cast<CSeq_submit*>(container.GetPointerOrNull());

		if (container.IsNull() || submit == 0)
		{
			submit = new CSeq_submit;
			submit->Assign(*m_submit_template);
			submit->SetData().SetEntrys().clear();
			container.Reset(submit);
		}

		CSeq_submit_Base::C_Data::TEntrys& data = submit->SetData().SetEntrys();
		CRef<CSeq_entry> new_entry;
		if (data.empty() || !make_set)
		{
			new_entry.Reset(new CSeq_entry);
			data.push_back(new_entry);
		}
		else
		{
			new_entry = data.back();
		}

		return CreateNextBioSeqFromTemplate(*new_entry, make_set);
	}
	else
	{
		CSeq_entry* entry = 0;
		if (container.IsNull())
		{
			entry = new CSeq_entry;
			container.Reset(entry);
		}
		else
			entry = dynamic_cast<CSeq_entry*>(container.GetPointerOrNull());

		return CreateNextBioSeqFromTemplate(*entry, make_set || entry->Which() == CSeq_entry_Base::e_Set);
	}
	return 0;
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

