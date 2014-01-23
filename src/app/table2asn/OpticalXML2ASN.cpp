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

#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Map_ext.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Packed_seqpnt.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Rsite_ref.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/BioSource.hpp>

#include <serial/typeinfo.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/pub/Pub_equiv.hpp>

#include "table2asn_context.hpp"
#include "OpticalXML2ASN.hpp"

#include <objtools/readers/message_listener.hpp>

#include <misc/xmlwrapp/xmlwrapp.hpp>

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

namespace
{

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
public:
    COpticalxml2asnOperatorImpl(IMessageListener* logger):
      m_logger(logger),
      m_genome(CBioSource::eGenome_chromosome) // eGenome_plasmid ??
      {
      }

      int GetOpticalXMLData(const string& FileIn);
      CRef<CSeq_entry> BuildOpticalASNData(const CTable2AsnContext& context);
private:
    void BuildOpticalASNData(const CTable2AsnContext& context, const COpticalChrData& it, CBioseq& bioseq);
    void SetOpticalDescr(CSeq_descr& SD, const CTable2AsnContext& context);
    void SetOrganismData(CSeq_descr& SD, const string& enzyme, const CTable2AsnContext& context);

    CBioSource& SetSourceOrg(CSeq_descr& SD);


    vector <COpticalChrData> m_vchr;
    IMessageListener* m_logger;

public:
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


void COpticalxml2asnOperatorImpl::SetOpticalDescr(CSeq_descr& SD, const CTable2AsnContext& context)
{
    CSeq_descr::Tdata& TD = SD.Set();


    CRef<CMolInfo> mi(new CMolInfo);
    mi->SetBiomol(CMolInfo::eBiomol_genomic);
    mi->SetCompleteness(CMolInfo::eCompleteness_complete);

    CRef<CSeqdesc> sdm(new CSeqdesc());
    sdm->Select(CSeqdesc::e_Molinfo);
    sdm->SetMolinfo(*mi);

    TD.push_back(sdm);
}

CRef<CSeq_entry> COpticalxml2asnOperatorImpl::BuildOpticalASNData(const CTable2AsnContext& context)
{
    CRef<CSeq_entry> container;

    if (m_vchr.size() > 1 )
    {
        container.Reset(new CSeq_entry);
    }

    for (vector<COpticalChrData>::iterator it = m_vchr.begin(); it != m_vchr.end(); ++it)
    {
        CRef<CSeq_entry> new_entry(new CSeq_entry);
        if (context.m_entry_template.NotEmpty() && !context.m_avoid_submit_block)
            new_entry->Assign(*context.m_entry_template);
        BuildOpticalASNData(context, *it, new_entry->SetSeq());

        if (container.Empty())
            container = new_entry;
        else
            container->SetSet().SetSeq_set().push_back(new_entry);
    }

    return container;
}

void COpticalxml2asnOperatorImpl::BuildOpticalASNData(const CTable2AsnContext& context, const COpticalChrData& it, CBioseq& bioseq)
{
    string lclid;
    if (it.m_name.find("lcl|") != 0)
        lclid = "lcl|optical_map_chr_"+ it.m_name;
    else
        lclid = it.m_name;

    CRef<CSeq_id> id(new CSeq_id(lclid, CSeq_id::fParse_PartialOK));
    bioseq.SetId().clear();
    bioseq.SetId().push_back(id);

    CSeq_descr& SD = bioseq.SetDescr();
    SetOrganismData(SD, it.m_enzyme, context);
    SetOpticalDescr(SD, context);
    context.AddUserTrack(SD, "FileTrack", "FileTrackURL", context.m_url);

    CSeq_inst& inst(bioseq.SetInst());
    inst.SetRepr(CSeq_inst::eRepr_map);
    inst.SetMol(CSeq_inst::eMol_dna);
    inst.SetLength(it.m_length);
    inst.SetTopology(it.m_linear ? CSeq_inst::eTopology_linear : CSeq_inst::eTopology_circular);
    inst.SetStrand(CSeq_inst::eStrand_ds);

    CMap_ext& map = inst.SetExt().SetMap();
    list< CRef< CSeq_feat > >& td = map.Set();

    CRef<CSeq_feat> f(new CSeq_feat());

    CRef<CSeqFeatData> fd(new CSeqFeatData());
    fd->Select(CSeqFeatData::e_Rsite);

    CRef<CRsite_ref> rs(new CRsite_ref());
    rs->Select(CRsite_ref::e_Str);
    rs->SetStr(it.m_enzyme);
    fd->SetRsite(*rs);

    f->SetData(*fd);

    CRef<CPacked_seqpnt> spnt(new CPacked_seqpnt());
    int addr = -1;
    ITERATE (vector <TFragm>, fit, it.m_fragments) {
        addr += fit->second;
        spnt->AddPoint(addr);
    }

    CRef<CSeq_loc> l(new CSeq_loc(*id, spnt->GetPoints()));
    f->SetLocation(*l);

    td.push_back(f);
}

void COpticalxml2asnOperatorImpl::SetOrganismData(CSeq_descr& SD, const string& enzyme, const CTable2AsnContext& context)
{
    CSeq_descr::Tdata& TD = SD.Set();

    CBioSource& biosource = CAutoAddDesc(SD, CSeqdesc::e_Source).Set().SetSource();
    biosource.SetGenome(m_genome);
    if (context.m_taxid > 0)
        biosource.SetOrg().SetTaxId(context.m_taxid);

    // Get strain
    if (!context.m_strain.empty())
    {
        CRef<COrgMod> strain(new COrgMod(COrgMod::eSubtype_strain, context.m_strain));
        biosource.SetOrg().SetOrgname().SetMod().push_back(strain);
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

    if (biosource.IsSetTaxname())
    {
        string title = biosource.GetTaxname();
        if (!context.m_strain.empty() && !NStr::EndsWith(title, context.m_strain))
            title += " " + context.m_strain;
        if (m_genome == CBioSource::eGenome_chromosome)
            title += " chromosome";
        else if (m_genome == CBioSource::eGenome_plasmid)
            title += " plasmid";
        if (!enzyme.empty())
            title += " " + enzyme;
        title += " whole genome map";

        CRef<CSeqdesc> sd(new CSeqdesc());
        sd->SetTitle(title);
        TD.push_back(sd);
    }
}

}

COpticalxml2asnOperator::COpticalxml2asnOperator()
{
}

COpticalxml2asnOperator::~COpticalxml2asnOperator()
{
}

CRef<CSeq_entry> COpticalxml2asnOperator::LoadXML(const string& FileIn, const CTable2AsnContext& context)
{
    auto_ptr<COpticalxml2asnOperatorImpl> m_impl(new COpticalxml2asnOperatorImpl(context.m_logger));

    m_impl->GetOpticalXMLData(FileIn);

    return m_impl->BuildOpticalASNData(context);
};

void COpticalxml2asnOperator::UpdatePubDate(CSerialObject& obj)
{
#if 0
    CSeq_entry* entry = 0;
    if (obj.GetThisTypeInfo()->IsType(CSeq_entry::GetTypeInfo()))
        entry = (CSeq_entry*)(&obj);
    else
        return;

    if (entry->IsSeq())
    {
        NON_CONST_ITERATE(CSeq_descr::Tdata, it, entry->SetDescr().Set())
        {
            if ((**it).IsPub())
            {
                CSeqdesc::TPub& pub = (**it).SetPub();
                if (pub.IsSetPub())
                {
                    pub.SetPub().Set().
                }
            }
        }
    }
    else
    if (entry->IsSet())
    NON_CONST_ITERATE(CBioseq_set_Base::TSeq_set, it, entry->SetSet().SetSeq_set())
    {
        return UpdatePubDate(**it);
    }
#endif
}


using namespace xml;
int COpticalxml2asnOperatorImpl::GetOpticalXMLData(const string& FileIn)
{
    error_messages msg;
    document *doc;

    try {
        CNcbiIfstream in(FileIn.c_str());
        doc = new document(in, &msg);
    }
    catch(...) {
        m_logger->PutError(*CLineError::Create(CLineError::eProblem_GeneralParsingError, eDiag_Error, "", 0, 
            "No data found in " + FileIn + ": " + msg.print()));
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
                m_logger->PutError(
                    *CLineError::Create(CLineError::eProblem_GeneralParsingError, eDiag_Warning, "", 0, 
                    "No chromosome name found in RESTRICTION_MAP - ID '" + id + "' was used"));
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

END_NCBI_SCOPE
