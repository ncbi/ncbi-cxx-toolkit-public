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
#include <objects/seqfeat/SeqFeatData.hpp>

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

typedef pair<pair<int,int>,int> TFragm;

struct lt_fragment
{
    bool operator()(const TFragm& p1, const TFragm& p2) const
    {
        if (p1.first.first == p2.first.first)
            return (p1.first.second < p2.first.second);
        return (p1.first.first < p2.first.first);
    }
};

class COpticalChrData
{
public:
    COpticalChrData(const string& nm, const char *enzyme, bool is_linear=true) :
      m_name(nm), m_enzyme(enzyme), m_linear(is_linear) {};

      void sortFragments(){ m_fragments.sort(lt_fragment()); };
      string m_name;
      string m_enzyme;
      list <TFragm> m_fragments;
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


    list <COpticalChrData> m_vchr;
    IMessageListener* m_logger;

public:
    CBioSource::EGenome m_genome;
};


void COpticalxml2asnOperatorImpl::SetOpticalDescr(CSeq_descr& SD, const CTable2AsnContext& context)
{
    CAutoAddDesc desc(SD, CSeqdesc::e_Molinfo);
    desc.Set().SetMolinfo().SetBiomol(CMolInfo::eBiomol_genomic);
    desc.Set().SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_complete);
}

CRef<CSeq_entry> COpticalxml2asnOperatorImpl::BuildOpticalASNData(const CTable2AsnContext& context)
{
    CRef<CSeq_entry> container;

    if (m_vchr.size() > 1 )
    {
        container.Reset(new CSeq_entry);
    }

    for (list<COpticalChrData>::iterator it = m_vchr.begin(); it != m_vchr.end(); ++it)
    {
        CRef<CSeq_entry> new_entry(new CSeq_entry);
        if (context.m_entry_template.NotEmpty())
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

    CSeq_inst& inst(bioseq.SetInst());
    inst.SetRepr(CSeq_inst::eRepr_map);
    inst.SetMol(CSeq_inst::eMol_dna);
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

    f->SetLocation().SetPacked_pnt().SetId(*id);
    CPacked_seqpnt::TPoints& points = f->SetLocation().SetPacked_pnt().SetPoints();
    points.reserve(it.m_fragments.size());
    
    TSeqPos addr = 0;
    // There are two approaches of specifying fragments: locations and lengths of fragments
    if (context.m_optmap_use_locations)
    {
        ITERATE (list <TFragm>, fit, it.m_fragments) {
            list<TFragm>::const_iterator next = fit;
            next++;
            if (next != it.m_fragments.end())
            {
                if (next->second <= fit->second) {
                    // if locations are not increasing raise an error
                    m_logger->PutError(*auto_ptr<CLineError>(
                        CLineError::Create(CLineError::eProblem_GeneralParsingError, eDiag_Error, "", 0,
                        string("Locations are overlapping at row I=") + 
                        (fit->first.first == 0 ? NStr::IntToString(fit->first.second) :
                        NStr::IntToString(fit->first.first)  + "." + NStr::IntToString(fit->first.second)))));

                    points.clear();
                    addr = 0;
                    break;
                }
            }
            points.push_back(fit->second);
            addr = fit->second;
        }
    }
    else
    {
        ITERATE (list <TFragm>, fit, it.m_fragments) 
        {
            TSeqPos prev = addr;
            if (prev > addr)
            {
                m_logger->PutError(*auto_ptr<CLineError>(
                    CLineError::Create(CLineError::eProblem_GeneralParsingError, eDiag_Error, "", 0,
                    string("Location reached numeric limit at row I=") + 
                    (fit->first.first == 0 ? NStr::IntToString(fit->first.second) :
                    NStr::IntToString(fit->first.first)  + "." + NStr::IntToString(fit->first.second)))));

                points.clear();
                addr = 0;
                break;
            }
            addr += fit->second;
            points.push_back(addr - 1);
        }
    }

    inst.SetLength(addr);

    td.push_back(f);
}

void COpticalxml2asnOperatorImpl::SetOrganismData(CSeq_descr& SD, const string& enzyme, const CTable2AsnContext& context)
{
    CBioSource& biosource = CAutoAddDesc(SD, CSeqdesc::e_Source).Set().SetSource();
    biosource.SetGenome(m_genome);
    if (context.m_taxid > 0)
        biosource.SetOrg().SetTaxId(context.m_taxid);
    if (!context.m_OrganismName.empty())
        biosource.SetOrg().SetTaxname(context.m_OrganismName);

    // Get strain
    if (!context.m_strain.empty())
    {
        CRef<COrgMod> strain(new COrgMod(COrgMod::eSubtype_strain, context.m_strain));
        biosource.SetOrg().SetOrgname().SetMod().push_back(strain);
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

    if (m_impl->GetOpticalXMLData(FileIn)>0)
        return m_impl->BuildOpticalASNData(context);
    else
        return CRef<CSeq_entry>();
};

void COpticalxml2asnOperator::UpdatePubDate(CSerialObject& obj)
{
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
        m_logger->PutError(*auto_ptr<CLineError>(
            CLineError::Create(CLineError::eProblem_GeneralParsingError, eDiag_Error, "", 0,
            "No data found in " + FileIn + ": " + msg.print())));
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
                m_logger->PutError(*auto_ptr<CLineError>(
                    CLineError::Create(CLineError::eProblem_GeneralParsingError, eDiag_Warning, "", 0,
                    "No chromosome name found in RESTRICTION_MAP - ID '" + id + "' was used")));
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
                        TFragm fragm;
                        CTempString s(ait->get_value());

                        size_t n = s.find('.');
                        if (n == CTempString::npos)
                        {
                            fragm.first.first = 0;
                            fragm.first.second = NStr::StringToUInt(s);
                        }
                        else
                        {
                            fragm.first.first  = NStr::StringToUInt(s.substr(0, n));
                            fragm.first.second = NStr::StringToUInt(s.substr(n+1, s.length()-n-1));
                        }


                        ait = at.find("S");
                        fragm.second = NStr::StringToUInt(ait->get_value());
                        chr.m_fragments.push_back(fragm);
                    }
                }
            }
            chr.sortFragments();
            m_vchr.push_back(chr);
        }
    }
    return m_vchr.size();
}

END_NCBI_SCOPE
