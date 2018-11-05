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
* Authors:  Justin Foley
*
* File Description:
*
* ===========================================================================
*/


#include  <ncbi_pch.hpp>

class CModValueAttribs
{
public:
    using TAttribs = map<string, string>;

    CModValueAttribs(const string& value); 

    void SetValue(const string& value);
    void AddAttrib(const string& attrib_name, const string& attrib_value);
    bool HasAdditionalAttribs(void) const;

    const string& GetValue(void) const;
    const TAttribs& GetAdditionalAttribs(void) const;

private:
    string mValue;
    TAttribs mAdditionalAttribs;
};

CModValueAttribs(const string& value) : mValue(value) {}

void CModValueAttribs::SetValue(const string& value) 
{
    mValue = value;
}

void CModValueAttribs::AddAttrib(const string& attrib_name, 
        const string& attrib_value)
{
    mAdditionalAttribs.emplace(attrib_name, attrib_value);
}

bool CModValueAttribs::HasAdditionalAttribs(void) const
{
    return !mAdditionalAttribs.empty();
}

const string& CModValueAttribs::GetValue(void) const
{
    return mValue;
}

const CModValue::TAttrings& GetAdditionalAttribs(void) const
{
    return mAdditionalAttribs;
}


using TMods = multimap<string, CModValueAttribs>;


void CModImporter::Apply(const CSeqdesc& desc, TMods& mods)
{

    if (desc.IsUser()) {
        x_ImportUserObject(desc.GetUser(), mods);
    }
    else
    if (desc.IsSource()) {
        x_ImportBioSource(desc.GetSource(), mods);
    }
    else
    if (desc.IsGenbank()) {
        x_ImportGBblock(desc.GetGenbank(), mods);
    }
    else 
    if (desc.IsMolinfo()) {
        x_ImportMolinfo(desc.GetMolinfo(), mods);
    }
}


void CModImporter::x_ImportUserObject(const CUser_object& user_object, TMods& mods)
{
    if (user_object.IsDBLink()) {
        x_ImportDBLink(user_object, mods);
    }
    else
    if (x_IsUserType(user_object, "GenomeProjectsDB")) {
        x_ImportGenomeProjects(user_object, mods);
    }
    else
    if (x_IsUserType(user_object, "TpaAssembly")) {
        x_ImportTpaAssembly(user_object, mods);
    }
}

static unordered_map<CBioSource::EGenome, string> genome_enum_to_string 
= { {CBioSource::eGenome_unknown : "unknown"},
    {CBioSource::eGenome_genomic : "genomic" },
    {CBioSource::eGenome_chloroplast : "chloroplast"},
    {CBioSource::eGenome_kinetoplast : "kinetoplast"}, 
    {CBioSource::eGenome_mitochondrion : "mitochondrial"},
    {CBioSource::eGenome_plastid : "plastid"},
    {CBioSource::eGenome_macronuclear : "macronuclear"},
    {CBioSource::eGenome_extrachrom : "extrachromosomal"},
    {CBioSource::eGenome_plasmid : "plasmid"},
    {CBioSource::eGenome_transposon : "transposon"},
    {CBioSource::eGenome_insertion_seq : "insertion sequence"},
    {CBioSource::eGenome_cyanelle : "cyanelle"},
    {CBioSource::eGenome_proviral : "provirus"},
    {CBioSource::eGenome_virion : "virion"},
    {CBioSource::eGenome_nucleomorph : "nucleomorph"},
    {CBioSource::eGenome_apicoplast : "apicoplase"},
    {CBioSource::eGenome_leucoplast : "leucoplast"},
    {CBioSource::eGenome_proplastid : "proplastid"},
    {CBioSource::eGenome_endogenous_virus : "endogenous virus"},
    {CBioSource::eGenome_hydrogenosome : "hydrogenosome"},
    {CBioSource::eGenome_chromosome : "chromosome" },
    {CBioSource::eGenome_chromatophore : "chromatophore"},
    {CBioSource::eGenome_plasmid_in_mitochondrion : "plasmid in mitochondrion"},
    {CBioSource::eGenome_plasmid_in_plastid : "plasmid in plastid"}
};

static unordered_map<CBioSource::EOrigin, string> origin_enum_to_string
= { {CBioSource::eOrigin_unknown : "unknown"},
    {CBioSource::eOrigin_natural : "natural"},
    {CBioSource::eOrigin_natmut : "natural mutant"},
    {CBioSource::eOrigin_mut : "mutant"},
    {CBioSource::eOrigin_artificial : "artificial"},
    {CBioSource::eOrigin_synthetic : "synthetic"},
    {CBioSource::eOrigin_other : "other"}
};


void CModImporter::x_ImportBioSource(const CBioSource& biosource, TMods& mods)
{
    if (biosource.IsSetGenome()) {
        auto e_genome = static_cast<CBioSource::EGenome>(biosource.GetGenome());
        _ASSERT(genome_enum_to_string.find(e_genome) != genome_enum_to_string.end());
        const auto& genome_string = genome_enum_to_string[e_genome]; 
        mods.emplace("location", genome_string);
    }

    if (biosource.IsSetOrigin()) {
        auto e_origin = static_cast<CBioSource::EOrigin>(biosource.GetOrigin());
        _ASSERT(origin_enum_to_string.find(e_origin) != origin_enum_to_string.end());
        const auto& origin_string = origin_enum_to_string[e_origin];
        mods.emplace("origin", origin_string);
    }

    if (biosource.IsSetIs_focus()) {
        mods.emplace("focus", "true");
    }

    // x_ImportPCRPrimer(...);
    //
    if (biosource.IsSetSubtype()) {
        for (const auto& pSubSource : biosource.GetSubtype()) {
            if (pSubSource) {
                x_ImportSubSource(*pSubSource, mods);
            }
        }
    }

    x_ImportOrgRef(biosource.GetOrg(), mods);
}


void CModImporter::x_ImportSubSource(const CSubSource& subsource, TMods& mods)
{
    statuc const auto& subsource_enum_to_name;
    const auto& subtype = subsource_enum_to_name[org_mod.GetSubtype()];
    const auto& subname = subsource.GetSubname();
}


void CModImporter::x_ImportOrgRef(const COrg_ref& org_ref, TMods& mods)
{
    if (org_ref.IsSetTaxname()) {
        mods.emplace("taxname", org_ref.GetTaxname());
    }
    if (org_ref.IsSetCommon()) {
        mods.emplace("common", org_ref.GetCommon());
    }
    if (org_ref.IsSetOrgname()) {
        const auto& org_name = org_ref.GetOrgname();
        x_ImportOrgName(org_name, mods);
    }

    if (org_ref.IsSetDb()) {
        for(const auto& pDbtag : org_ref.GetDb()) {
            string database = pDbtag->GetDb();
            string tag = pDbtag->GetTag().IsSetStr() ?
                         pDbtab->GetTag().GetStr() :
                         NStr::IntToString(pDbtag->GetTag().GetId());
            mods.emplace("dbxref", database + ":" + tag);
        }
    }
}

void CModImporter::x_ImportOrgName(const COrg_name& org_name, TMods& mods)
{
    if (org_name.IsSetDiv()) {
        mods.emplace("division", org_name.GetDiv());
    }

    if (org_name.IsSetLineage()) {
        mods.emplace("lineage", org_name.GetLineage());
    }

    if (org_name.IsSetGcode()) {
        mods.emplace("gcode", NStr::IntToString(org_name.GetGcode()));
    }

    if (org_name.IsSetMgcode()) {
        mods.emplace("mgcode", NStr::IntToString(org_name.GetMgcode()));
    }

    if (org_name.IsSetPgcode()) {
        mods.emplace("pgcode", NStr::IntToString(org_name.GetPgcode());
    }


    if (org_name.IsSetMod()) {
        for(const auto& pOrgMod : org_name.SetMod()) {
            if (pOrgMod) {
                x_ImportOrgMod(org_mod, mods);
            }
        }
    }
}


void CModImporter::x_ImportOrgMod(const COrgMod& org_mod, TMods& mods)
{
    static const auto& orgmod_enum_to_name;

    const auto& subtype = orgmod_enum_to_name[org_mod.GetSubtype()];
    const auto& subname = org_mod.GetSubname();

    mods.emplace(subtype, subname);
}


void CModImporter::x_ImportPMID(const CPubdesc& pub_desc, TMods& mods)
{  
    if (pub_desc.IsSetPub()) {
        for (const auto& pPub : pub_desc.GetPub().Get()) {
            if (pPub && pPub->IsPmid()) {
                const auto pmid = NStr::IntToString(pPub->GetPmid());
                mods.emplace("pmid", pmid);
            }
        }
    }
}

static unordered_map<CMolInfo::TBiomol, string> biomol_enum_to_name =
{{CMolInfo::eBiomol_cRNA : "cRNA"},
 {CMolInfo::eBiomol_genomic : "Genomic"},
 {CMolInfo::eBiomol_mRNA : "mRNA"},
 {CMolInfo::eBiomol_ncRNA : "ncRNA"},
 {CMolInfo::eBiomol_other_genetic : "Other-Genetic"},
 {CMolInfo::eBiomol_pre_RNA : "Precursor RNA"},
 {CMolInfo::eBiomol_rRNA : "rRNA"},
 {CMolInfo::eBiomol_transcribed_RNA : "Transcribed RNA"},
 {CMolInfo::eBiomol_tmRNA : "tmRNA"},
 {CMolInfo::eBiomol_tRNA : "tRNA"}};
    
};


void CModImporter::x_ImportMolInfo(const CMolinfo& mol_info, TMods& mods)
{
    if (mol_info.IsSetBiomol()) {
        const string& moltype = biomol_enum_to_name[mol_info.GetBiomol()];
        mods.emplace("moltype", moltype);
    }

    if (mol_info.IsSetTech()) {
        const string& tech =  tech_enum_to_name[mol_info.GetTech()];
        mods.emplace("tech", tech);
    }


    if (mol_info.IsSetCompleteness()) {
        const string& completeness = completeness_enum_to_name[mol_info.GetCompleteness()];
        mods.emplace("completeness", completeness);
    }
}


void CModImporter::x_ImportDBLink(const CUser_object& user_object, TMods& mods) 
{
    if (!user_object.IsDBLink()) {
        return;
    }

    static const map<string,string> label_to_mod_name =
    {{"BioProject" : "bioproject"},
     {"BioSample" : "biosample"},
     {"Sequence Read Archive" : "sra"}};


    if (user_object.IsSetData()) {
        for (const auto& pUserField : user_object.GetData()) {
            if (pUserField && 
                pUserField->IsSetLabel() &&
                pUserField->GetLabel().IsStr()) {
                const auto& label = pUserField->GetLabel().GetStr();
                const auto it = label_to_mod_name.find(label);
                if (it != label_to_mod_name.end()) {
                    string vals = "";
                    size_t index = 0;
                    for (const auto& val : pField->GetData().GetStrs()) {
                        
                        if (index > 0) {
                            vals += ",";
                        }
                        vals += val;
                        ++index;
                    }
                }
            }
        }
    }
}







