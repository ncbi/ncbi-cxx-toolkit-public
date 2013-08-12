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
* File Description:
*
* Optical Map XML data to ASN.1
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>


//#include <corelib/ncbiapp.hpp>
//#include <corelib/ncbiargs.hpp>
//#include <corelib/ncbienv.hpp>

#include <serial/iterator.hpp>
#include <serial/objistrxml.hpp>
#include <serial/serial.hpp>

#include <misc/xmlwrapp/xmlwrapp.hpp>

#include <objects/taxon1/taxon1.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Map_ext.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Packed_seqpnt.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Rsite_ref.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/biblio/Cit_sub.hpp>

#include "OpticalXML2ASN.hpp"


using namespace xml;

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

typedef pair<int,int> TFragm;

struct lt_fragment
{
    bool operator()(const TFragm& p1, const TFragm& p2) const
    {
	return ( p1.first < p2.first );
    }
};

class COpticalChrData
{
public:
    COpticalChrData(const string& nm, const char *enzyme, bool is_linear=true) :
	m_name(nm), m_enzyme(enzyme), m_length(0), m_linear(is_linear) {};

    void sortFragments(){ sort(m_fragments.begin(), m_fragments.end(), lt_fragment()); };
    void SetLength(){
	if (m_length == 0) {
	    ITERATE (vector <TFragm>, it, m_fragments)
		m_length += it->second;
	}
    };
    string m_name;
    string m_enzyme;
    vector <TFragm> m_fragments;
    int m_length;
    bool m_linear;
};

class COpticalxml2asnOperatorImpl
{
//    virtual void Init(void);
//    virtual int  Run(void);
public:
    int GetOpticalXMLData(const string& FileIn);
    CRef<CSerialObject> BuildOpticalASNDataSet();
    CRef<CSerialObject> BuildOpticalASNData();
private:
    void GetOpticalDescr(CRef<CSeq_descr> SD);
    void UseTemplate(CRef<CSeq_descr> SD, const string& TemplateFile);
    void AddUserTrack(CRef<CSeq_descr> SD, const string& type, const string& lbl, const string& data);
    void SetOrgData(CSeq_descr::Tdata& TD);

    vector <COpticalChrData> m_vchr;

public:
    int m_taxid;
    string m_taxname;
    string m_title;
    string m_enzyme;
    string m_bpid;
    string m_strain;
    string m_ft_url;
    CBioSource::EGenome m_genome;
};



#if 0

void COpticalxml2asnOperatorImpl::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),"Optical Map XML data to ASN.1");

    // Describe the expected command-line arguments
    arg_desc->AddDefaultKey
        ("in", "InputFile",
         "name of file to read from (standard input by default)",
         CArgDescriptions::eInputFile, "-", CArgDescriptions::fPreOpen);

    arg_desc->AddDefaultKey
        ("out", "OutputFile",
         "name of file to write to (InputFile.asn by default)",
         CArgDescriptions::eOutputFile, "-", CArgDescriptions::fPreOpen);

    arg_desc->AddDefaultKey
        ("taxid", "Taxonomy_ID",
         "Organism taxonomy ID (optional)", CArgDescriptions::eInteger, "0");

    arg_desc->AddDefaultKey
        ("taxname", "Taxonomy_name",
         "Taxonomy name (optional)", CArgDescriptions::eString, "");

    arg_desc->AddDefaultKey
        ("tmpl", "TemplateFile",
         "Template file with common ASN.1 nodes to add to output (optional)", CArgDescriptions::eIOFile, "-");

    arg_desc->AddDefaultKey
        ("ft_url", "FileTrack_URL",
         "FileTrack URL for the XML file retrieval (optional)", CArgDescriptions::eString, "");

    arg_desc->AddDefaultKey
        ("bpid", "BioProject_accession",
         "BioProject accession (optional)", CArgDescriptions::eString, "");

    arg_desc->AddDefaultKey
        ("strain", "Strain_name",
         "Strain name (optional)", CArgDescriptions::eString, "");

    SetupArgDescriptions(arg_desc.release());
}

int COpticalxml2asnOperatorImpl::Run(void)
{
    CArgs args = GetArgs();
    int nChr = GetOpticalXMLData(args["in"].AsString());
    if (nChr <= 0)
	return -1;

    string out(args["out"].AsString());
    if (out == "-")
	out = args["in"].AsString() + ".asn";
    string tmpl(args["tmpl"].AsString());

    m_taxid = args["taxid"].AsInteger();
    m_taxname = args["taxname"].AsString();

    if (m_taxid == 0 && m_taxname.empty()) {
	cerr << endl << "Error: No taxonomy information has been provided." << endl;
	return 100;
    }
    m_ft_url = args["ft_url"].AsString();
    m_bpid = args["bpid"].AsString();
    m_strain = args["strain"].AsString();
    m_genome = CBioSource::eGenome_chromosome; // eGenome_plasmid ??

    return (nChr > 1 ? BuildOpticalASNDataSet(out) : BuildOpticalASNData(out, tmpl));
}

#endif

int COpticalxml2asnOperatorImpl::GetOpticalXMLData(const string& FileIn)
{
    error_messages msg;
    document *doc;

    try {
	CNcbiIfstream in(FileIn.c_str());
	doc = new document(in, &msg);
    }
    catch(...) {
	cerr << endl << "Error: No data found in " << FileIn << ": " << msg.print() << endl;
	return -1;
    }

    node& root = doc->get_root_node();
    node::const_iterator child(root.begin()), child_end(root.end());
    for (; child != child_end; ++child) {
	if (strcmp(child->get_name(), "RESTRICTION_MAP") == 0) {
	    // Get chromosome name
	    string name;
	    const attributes& at = child->get_attributes();
	    attributes::const_iterator ait = at.find("ID");

	    string id = ait->get_value();
	    SIZE_TYPE l = NStr::FindNoCase(id, "chromosome");
	    if (l != NPOS) {
		vector<string> tok;
		NStr::TokenizePattern(id.substr(l), " ", tok, NStr::eMergeDelims);
		if (tok.size() > 1) {
		    name = tok[1];
		    if ((l = NStr::Find(name, ",")) != NPOS)
			name = name.substr(0,l);
		}
	    }
	    if (name.empty()) {
		cerr << endl << "Warning: No chromosome name found in RESTRICTION_MAP - ID '" << id << "' was used." << endl;
		name = id;
	    }

	    bool is_linear(true);
	    node::const_iterator it = child->begin();
	    for (; it != child->end() && NStr::Compare(it->get_name(),"MAP_DISPLAY"); ++it);
	    if (it != child->end()) {
		const attributes& at = it->get_attributes();
		attributes::const_iterator ait = at.find("CIRCULAR");
		if (strcmp(ait->get_value(), "true") == 0)
		    is_linear = false;
	    }
	    COpticalChrData chr(name, at.find("ENZYME")->get_value(), is_linear);

	    // Get fragments
	    it = child->begin();
	    while (it != child->end() && NStr::Compare(it->get_name(),"FRAGMENTS"))
		++it;
	    if (it != child->end()) {
		for (node::const_iterator fit = it->begin(); fit != it->end(); ++fit) {
		    if (NStr::Compare(fit->get_name(), "F") == 0) {
			const attributes& at = fit->get_attributes();
			attributes::const_iterator ait = at.find("I");
			int n = atoi(ait->get_value());
			ait = at.find("S");
			int lng = atoi(ait->get_value());
			chr.m_fragments.push_back(make_pair(n, lng));
		    }
		}
	    }
	    chr.sortFragments();
	    chr.SetLength();
	    m_vchr.push_back(chr);
	}
    }
    return m_vchr.size();
}


void COpticalxml2asnOperatorImpl::SetOrgData(CSeq_descr::Tdata& TD)
{
    CTaxon1 taxon;
    bool is_species, is_uncultured;
    string blast_name;

    CRef<CBioSource> bs(new CBioSource());
    bs->SetGenome(m_genome);

    taxon.Init();

    if (!m_taxname.empty()) {
	int taxid = taxon.GetTaxIdByName(m_taxname);
	if (m_taxid == 0)
	    m_taxid = taxid;
	else if (m_taxid != taxid) {
	    cerr << endl << "Error: Conflicting taxonomy info provided: taxid " << m_taxid << ": ";
	    if (taxid <= 0)
		m_taxid = taxid;
	    else {
		taxon.Fini();
		cerr << "taxonomy ID for the name '" << m_taxname << "' was determined as " << taxid << endl;
		return;
	    }
	}
    }
    if (m_taxid <= 0) {
	taxon.Fini();
	cerr << endl << "Error: No unique taxonomy ID found for the name '" << m_taxname << "'" << endl;
	return;
    }
    CConstRef<COrg_ref> org = taxon.GetOrgRef(m_taxid, is_species, is_uncultured, blast_name);
    bs->SetOrg().Assign(*org);

    // Get strain
    if (!m_strain.empty()) {
	CRef<COrgMod> strain(new COrgMod(COrgMod::eSubtype_strain, m_strain));
	bs->SetOrg().SetOrgname().SetMod().push_back(strain);
    }
    /* Alternative - no rule on selecting one that needed 
    CTaxon1::TNameList sn;
    if (taxon.GetTypeMaterial(m_taxid, sn) && sn.size()) {
	ITERATE (CTaxon1::TNameList, it, sn) {
	    CRef<COrgMod> strain(new COrgMod(COrgMod::eSubtype_strain, *it));
	    bs->SetOrg().SetOrgname().SetMod().push_back(strain);
	}
    }
    */
    //CRef<COrgMod> oldlin(new COrgMod(COrgMod::eSubtype_old_lineage, "old lineage"));
    //bs->SetOrg().SetOrgname().SetMod().push_back(oldlin);

    taxon.Fini();

    if (bs->IsSetTaxname()) {
	m_title = bs->GetTaxname();
	if (!m_strain.empty() && !NStr::EndsWith(m_title, m_strain))
	    m_title += " " + m_strain;
	if (m_genome == CBioSource::eGenome_chromosome)
	    m_title += " chromosome";
	else if (m_genome == CBioSource::eGenome_plasmid)
	    m_title += " plasmid";
	if (!m_enzyme.empty())
	    m_title += " " + m_enzyme;
	m_title += " whole genome map";

	CRef<CSeqdesc> sd(new CSeqdesc());
	sd->SetTitle(m_title);
	TD.push_back(sd);
    }

    CRef<CSeqdesc> sd(new CSeqdesc());
    sd->Select(CSeqdesc::e_Source);
    sd->SetSource(*bs);
    TD.push_back(sd);
}

void COpticalxml2asnOperatorImpl::GetOpticalDescr(CRef<CSeq_descr> SD)
{
    CSeq_descr::Tdata& TD = SD->Set();
    SetOrgData(TD);

    CRef<CMolInfo> mi(new CMolInfo);
    mi->SetBiomol(CMolInfo::eBiomol_genomic);
    mi->SetCompleteness(CMolInfo::eCompleteness_complete);

    CRef<CSeqdesc> sdm(new CSeqdesc());
    sdm->Select(CSeqdesc::e_Molinfo);
    sdm->SetMolinfo(*mi);

    TD.push_back(sdm);
}


void COpticalxml2asnOperatorImpl::UseTemplate(CRef<CSeq_descr> SD, const string& TemplateFile)
{
#if 0
    if (TemplateFile.empty() || TemplateFile == "-")
	return;
    try {
	CNcbiIfstream istrs(TemplateFile.c_str());

	CObjectIStream* istr = CObjectIStream::Open(eSerial_AsnText, istrs);

	for (;;) {
	    CSeq_descr::Tdata& TD = SD->Set();
	    try {
		CSubmit_block sblk;
		*istr >> sblk;
		if (sblk.CanGetCit()) {

		    CRef<CPub> pd(new CPub());
		    pd->SetSub().Assign(sblk.GetCit());


		    CRef<CSeqdesc> pub(new CSeqdesc());
		    pub->SetPub().SetPub().Set().push_back(pd);

		    TD.push_back(pub);
		}
		CRef<CSeqdesc> pub(new CSeqdesc());
		*istr >> *pub;
		TD.push_back(pub);
	    }
	    catch (CEofException&) {
		break;
	    }
	}
    }
    catch(CException &e){ cerr << endl << "Template file '" << TemplateFile << "': " << e.GetMsg() << endl; }
#endif
}

void COpticalxml2asnOperatorImpl::AddUserTrack(CRef<CSeq_descr> SD, const string& type, const string& lbl, const string& data)
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

    CSeq_descr::Tdata& TD = SD->Set();
    CRef<CSeqdesc> sd(new CSeqdesc());
    sd->Select(CSeqdesc::e_User);
    sd->SetUser(*uo);

    TD.push_back(sd);
}

CRef<CSerialObject> COpticalxml2asnOperatorImpl::BuildOpticalASNData()
{
    vector <COpticalChrData>::iterator it = m_vchr.begin();
    m_enzyme = it->m_enzyme;

	CRef<CBioseq> bioseq(new CBioseq);
	string lclid = "lcl|optical_map_chr_"+it->m_name;
	CRef<CSeq_id> id(new CSeq_id(lclid));
	bioseq->SetId().push_back(id);

    CRef<CSeq_descr> SD(new CSeq_descr());
    GetOpticalDescr(SD);
    AddUserTrack(SD, "DBLink", "BioProject", m_bpid);
    AddUserTrack(SD, "FileTrack", "FileTrackURL", m_ft_url);
    //UseTemplate(SD, TemplateFile);
    bioseq->SetDescr(*SD);

	CSeq_inst& inst(bioseq->SetInst());
	inst.SetRepr(CSeq_inst::eRepr_map);
	inst.SetMol(CSeq_inst::eMol_dna);
	inst.SetLength(it->m_length);
	inst.SetTopology(it->m_linear ? CSeq_inst::eTopology_linear : CSeq_inst::eTopology_circular);
	inst.SetStrand(CSeq_inst::eStrand_ds);
	CMap_ext& map = inst.SetExt().SetMap();

	list< CRef< CSeq_feat > >& td = map.Set();

	CRef<CSeq_feat> f(new CSeq_feat());

	CRef<CSeqFeatData> fd(new CSeqFeatData());
	fd->Select(CSeqFeatData::e_Rsite);

	CRef<CRsite_ref> rs(new CRsite_ref());
	rs->Select(CRsite_ref::e_Str);
	rs->SetStr(it->m_enzyme);
	fd->SetRsite(*rs);

	f->SetData(*fd);

	CRef<CPacked_seqpnt> spnt(new CPacked_seqpnt());
	int addr = -1;
	ITERATE (vector <TFragm>, fit, it->m_fragments) {
	    addr += fit->second;
	    spnt->AddPoint(addr);
	}

	CRef<CSeq_loc> l(new CSeq_loc(*id, spnt->GetPoints()));
	f->SetLocation(*l);

	td.push_back(f);

    CRef<CSeq_entry> se(new CSeq_entry());
    se->SetSeq(*bioseq);

    //CNcbiOfstream out(FileOut.c_str());
    //out << MSerial_AsnText << *se;

	CRef<CSerialObject> result(se);
    return result;
}

CRef<CSerialObject> COpticalxml2asnOperatorImpl::BuildOpticalASNDataSet()
{
    //CNcbiOfstream out(FileOut.c_str());

    CRef<CBioseq_set> BSS(new CBioseq_set);
    BSS->SetClass(CBioseq_set::eClass_equiv);

    CRef<CSeq_descr> SD(new CSeq_descr());
    GetOpticalDescr(SD);
    BSS->SetDescr(*SD);

    list< CRef< CSeq_entry > >& SS = BSS->SetSeq_set();

    ITERATE (vector <COpticalChrData>, it, m_vchr) {
	CRef<CBioseq> bioseq(new CBioseq);
	string lclid = "lcl|optical_map_chr_"+it->m_name;
	CRef<CSeq_id> id(new CSeq_id(lclid));
	bioseq->SetId().push_back(id);

	CSeq_inst& inst(bioseq->SetInst());
	inst.SetRepr(CSeq_inst::eRepr_map);
	inst.SetMol(CSeq_inst::eMol_dna);
	inst.SetLength(it->m_length);
	inst.SetTopology(it->m_linear ? CSeq_inst::eTopology_linear : CSeq_inst::eTopology_circular);
	inst.SetStrand(CSeq_inst::eStrand_ds);
	CMap_ext& map = inst.SetExt().SetMap();

	list< CRef< CSeq_feat > >& td = map.Set();

	CRef<CSeq_feat> f(new CSeq_feat());

	CRef<CSeqFeatData> fd(new CSeqFeatData());
	fd->Select(CSeqFeatData::e_Rsite);

	CRef<CRsite_ref> rs(new CRsite_ref());
	rs->Select(CRsite_ref::e_Str);
	rs->SetStr(it->m_enzyme);
	fd->SetRsite(*rs);

	f->SetData(*fd);

	CRef<CPacked_seqpnt> spnt(new CPacked_seqpnt());
	int addr = -1;
	ITERATE (vector <TFragm>, fit, it->m_fragments) {
	    addr += fit->second;
	    spnt->AddPoint(addr);
	}

	CRef<CSeq_loc> l(new CSeq_loc(*id, spnt->GetPoints()));
	f->SetLocation(*l);

	td.push_back(f);
	//	out << MSerial_AsnText << *bioseq;

	CRef<CSeq_entry> se(new CSeq_entry());
	se->SetSeq(*bioseq);
	SS.push_back(se);
    }

    CRef<CSeq_entry> se(new CSeq_entry());
    se->SetSet(*BSS);

    //out << MSerial_AsnText << *se;

	CRef<CSerialObject> result(se);

    return result;
}

//        cout << MSerial_AsnText << inXML;

COpticalxml2asnOperator::COpticalxml2asnOperator()
{

}
COpticalxml2asnOperator::~COpticalxml2asnOperator()
{
}

CRef<CSerialObject> COpticalxml2asnOperator::LoadXML(const string& FileIn, const CSerialObject* templ)
{
	m_impl.reset(new COpticalxml2asnOperatorImpl());

	int cnt = m_impl->GetOpticalXMLData(FileIn);

	CRef<CSerialObject> result(
        (cnt > 1)? 
		    m_impl->BuildOpticalASNDataSet() : 
	        m_impl->BuildOpticalASNData()
		);

	return result;
};


END_NCBI_SCOPE

