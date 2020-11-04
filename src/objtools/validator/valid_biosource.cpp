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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko, Mati Shomrat, ....
 *
 * File Description:
 *   Implementation of private parts of the validator
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiapp.hpp>
#include <objmgr/object_manager.hpp>

#include <objtools/validator/validatorp.hpp>
#include <objtools/validator/utilities.hpp>
#include <objtools/validator/validerror_bioseq.hpp>

#include <serial/iterator.hpp>
#include <serial/enumvalues.hpp>

//#include <objects/seqset/Bioseq_set.hpp>
//#include <objects/seqset/Seq_entry.hpp>

//#include <objects/seq/Bioseq.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SubSource.hpp>

#include <objmgr/seqdesc_ci.hpp>
#include <util/sgml_entity.hpp>

#define NCBI_USE_ERRCODE_X   Objtools_Validator

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;

const string kInvalidReplyMsg = "Taxonomy service returned invalid reply";

auto_ptr<CTextFsa> CValidError_imp::m_SourceQualTags;


static bool s_UnbalancedParentheses(string str)
{
    if (NStr::IsBlank(str)) {
        return false;
    }

    int par = 0, bkt = 0;
    string::iterator it = str.begin();
    while (it != str.end()) {
        if (*it == '(') {
            ++par;
        } else if (*it == ')') {
            --par;
            if (par < 0) {
                return true;
            }
        } else if (*it == '[') {
            ++bkt;
        } else if (*it == ']') {
            --bkt;
            if (bkt < 0) {
                return true;
            }
        }
        ++it;
    }
    if (par > 0 || bkt > 0) {
        return true;
    } else {
        return false;
    }
}


#if 0
const char* sm_ValidModifiedPrimerBases[] = {
    "ac4c",
    "chm5u",
    "cm",
    "cmnm5s2u",
    "cmnm5u",
    "d",
    "fm",
    "gal q",
    "gm",
    "i",
    "i6a",
    "m1a",
    "m1f",
    "m1g",
    "m1i",
    "m22g",
    "m2a",
    "m2g",
    "m3c",
    "m4c",
    "m5c",
    "m6a",
    "m7g",
    "mam5u",
    "mam5s2u",
    "man q",
    "mcm5s2u",
    "mcm5u",
    "mo5u",
    "ms2i6a",
    "ms2t6a",
    "mt6a",
    "mv",
    "o5u",
    "osyw",
    "p",
    "q",
    "s2c",
    "s2t",
    "s2u",
    "s4u",
    "t",
    "t6a",
    "tm",
    "um",
    "yw",
    "x",
    "OTHER"
};

static bool s_IsValidPrimerSequence (string str, char& bad_ch)
{
    bad_ch = 0;
    if (NStr::IsBlank(str)) {
        return false;
    }

    if (NStr::Find(str, ",") == string::npos) {
        if (NStr::Find(str, "(") != string::npos
            || NStr::Find(str, ")") != string::npos) {
            return false;
        }
    } else {
        if (!NStr::StartsWith(str, "(") || !NStr::EndsWith(str, ")")) {
            return false;
        }
    }

    if (NStr::Find(str, ";") != string::npos) {
        return false;
    }

    const char* *list_begin = sm_ValidModifiedPrimerBases;
    const char* *list_end = &(sm_ValidModifiedPrimerBases[sizeof(sm_ValidModifiedPrimerBases) / sizeof(const char*)]);

    size_t pos = 0;
    string::iterator sit = str.begin();
    while (sit != str.end()) {
        if (*sit == '<') {
            size_t pos2 = NStr::Find (str, ">", pos + 1);
            if (pos2 == string::npos) {
                bad_ch = '<';
                return false;
            }
            string match = str.substr(pos + 1, pos2 - pos - 1);
            if (find(list_begin, list_end, match) == list_end) {
                bad_ch = '<';
                return false;
            }
            sit += pos2 - pos + 1;
            pos = pos2 + 1;
        } else {
            if (*sit != '(' && *sit != ')' && *sit != ',' && *sit != ':') {
                if (!isalpha (*sit)) {
                    bad_ch = *sit;
                    return false;
                }
                char ch = toupper(*sit);
                if (strchr ("ABCDGHKMNRSTVWY", ch) == NULL) {
                    bad_ch = tolower (ch);
                    return false;
                }
            }
            ++sit;
            ++pos;
        }
    }

    return true;
}
#endif


bool CValidError_imp::IsSyntheticConstruct(const CBioSource& src)
{
    if (!src.IsSetOrg()) {
        return false;
    }
    const COrg_ref& org = src.GetOrg();
    if (org.IsSetTaxname()) {
        const string & taxname = org.GetTaxname();
        if (NStr::EqualNocase(taxname, "synthetic construct") ||
            NStr::FindNoCase(taxname, "vector") != string::npos) {
            return true;
        }
    }

    if (org.IsSetLineage()) {
        if (NStr::FindNoCase(org.GetLineage(), "artificial sequences") != string::npos) {
            return true;
        }
    }

    if (src.GetOrg().IsSetOrgname() && src.GetOrg().GetOrgname().IsSetDiv()
        && NStr::EqualNocase(src.GetOrg().GetOrgname().GetDiv(), "syn")) {
        return true;
    }
    return false;
}


bool CValidError_imp::IsArtificial(const CBioSource& src)
{
    if (src.IsSetOrigin()
        && (src.GetOrigin() == CBioSource::eOrigin_artificial)) {
        return true;
    }
    return false;
}


static string x_RepairCountryName (string countryname)
{
    if (NStr::CompareNocase (countryname, "USA: Washington DC") == 0 || NStr::CompareNocase (countryname, "USA: Washington, DC") == 0) {
        countryname = "USA: District of Columbia";
    } else if (NStr::StartsWith (countryname, "USA:") && NStr::EndsWith (countryname, ", Puerto Rico")) {
        countryname = "USA: Puerto Rico";
    } else if (NStr::StartsWith (countryname, "Puerto Rico")) {
        countryname = "USA: Puerto Rico";
    }
    if (NStr::StartsWith (countryname, "USA: Puerto Rico")) {
        countryname = countryname.substr(5);
    }

    if (NStr::StartsWith (countryname, "China: Hong Kong")) {
        countryname = countryname.substr(7);
    }

    return countryname;
}


void CValidError_imp::ValidateLatLonCountry
(string countryname,
string lat_lon,
const CSerialObject& obj,
const CSeq_entry *ctx)
{
    CSubSource::ELatLonCountryErr errcode;
    countryname = x_RepairCountryName (countryname);
    string error = CSubSource::ValidateLatLonCountry(countryname, lat_lon, IsLatLonCheckState(), errcode);
    if (!NStr::IsBlank(error)) {
        EErrType errtype = CValidator::ConvertCode(errcode);
        EDiagSev sev = (errtype == eErr_SEQ_DESCR_LatLonValue) ? eDiag_Warning : eDiag_Info;
        if (errtype != eErr_UNKNOWN) {
            PostObjErr(sev, errtype, error, obj, ctx);
        }
    }
}


/* note - special case for sex because it prevents a different message from being displayed, do not list here */
static const CSubSource::ESubtype sUnexpectedViralSubSourceQualifiers[] = {
    CSubSource::eSubtype_cell_line,
    CSubSource::eSubtype_cell_type,
    CSubSource::eSubtype_tissue_type,
    CSubSource::eSubtype_dev_stage
};

static const int sNumUnexpectedViralSubSourceQualifiers = sizeof(sUnexpectedViralSubSourceQualifiers) / sizeof(CSubSource::TSubtype);


static bool IsUnexpectedViralSubSourceQualifier(CSubSource::TSubtype subtype)
{
    int i;
    bool rval = false;

    for (i = 0; i < sNumUnexpectedViralSubSourceQualifiers && !rval; i++) {
        if (subtype == sUnexpectedViralSubSourceQualifiers[i]) {
            rval = true;
        }
    }
    return rval;
}

static const COrgMod::TSubtype sUnexpectedViralOrgModQualifiers[] = {
    COrgMod::eSubtype_breed,
    COrgMod::eSubtype_cultivar,
    COrgMod::eSubtype_specimen_voucher
};

static const int sNumUnexpectedViralOrgModQualifiers = sizeof(sUnexpectedViralOrgModQualifiers) / sizeof(COrgMod::TSubtype);

static bool IsUnexpectedViralOrgModQualifier(COrgMod::TSubtype subtype)
{
    int i;
    bool rval = false;

    for (i = 0; i < sNumUnexpectedViralOrgModQualifiers && !rval; i++) {
        if (subtype == sUnexpectedViralOrgModQualifiers[i]) {
            rval = true;
        }
    }
    return rval;
}


CBioSourceKind& CBioSourceKind::operator=(const CBioSource& bsrc)
{
    SetNotGood();

    if (bsrc.IsSetGenome()) {
        CBioSource::EGenome genome = (CBioSource::EGenome) bsrc.GetGenome();
        switch (genome) {
            //       case CBioSource::eGenome_genomic: break;
            //       case CBioSource::eGenome_plastid: break;
            //       case CBioSource::eGenome_macronuclear: break;
            //       case CBioSource::eGenome_extrachrom: break;
            //       case CBioSource::eGenome_plasmid: break;
            //       case CBioSource::eGenome_transposon: break;
            //       case CBioSource::eGenome_insertion_seq: break;
            //       case CBioSource::eGenome_proviral: break;
            //       case CBioSource::eGenome_virion: break;
            //       case CBioSource::eGenome_endogenous_virus: break;
        case CBioSource::eGenome_chloroplast:
        case CBioSource::eGenome_chromoplast:
        case CBioSource::eGenome_kinetoplast:
        case CBioSource::eGenome_mitochondrion:
        case CBioSource::eGenome_cyanelle:
        case CBioSource::eGenome_nucleomorph:
        case CBioSource::eGenome_apicoplast:
        case CBioSource::eGenome_leucoplast:
        case CBioSource::eGenome_proplastid:
        case CBioSource::eGenome_hydrogenosome:
        case CBioSource::eGenome_chromatophore:
            m_organelle = true;
            break;
        case CBioSource::eGenome_chromosome:
            m_eukaryote = true;
            break;
            //       case CBioSource::eGenome_chromatophore: break;
        default:
            break;
        }
    } 
    if (bsrc.IsSetLineage()) {
        const string& lineage = bsrc.GetLineage();
        if (NStr::StartsWith(lineage, "Eukaryota"))
            m_eukaryote = true;
        else if (NStr::StartsWith(lineage, "Bacteria"))
            m_bacteria = true;
        else if (NStr::StartsWith(lineage, "Archaea"))
            m_archaea = true;
        //else
        //    NCBI_ASSERT(0, "Not fully implemented switch statement");
    }

    return *this;
}



void CValidError_imp::ValidateBioSource
(const CBioSource&    bsrc,
const CSerialObject& obj,
const CSeq_entry *ctx)
{
    if (!bsrc.CanGetOrg()) {
        PostObjErr(eDiag_Error, eErr_SEQ_DESCR_NoOrgFound,
            "No organism has been applied to this Bioseq.  Other qualifiers may exist.", obj, ctx);
        return;
    }

    const COrg_ref& orgref = bsrc.GetOrg();

    // look at uncultured required modifiers
    if (orgref.IsSetTaxname()) {
        const string & taxname = orgref.GetTaxname();
        if (NStr::StartsWith(taxname, "uncultured ", NStr::eNocase)) {
            bool is_env_sample = false;
            FOR_EACH_SUBSOURCE_ON_BIOSOURCE(it, bsrc)
            {
                if ((*it)->IsSetSubtype() && (*it)->GetSubtype() == CSubSource::eSubtype_environmental_sample) {
                    is_env_sample = true;
                    break;
                }
            }
            if (!is_env_sample) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_UnculturedNeedsEnvSample,
                    "Uncultured should also have /environmental_sample",
                    obj, ctx);
            }
        } else if (NStr::EqualNocase(taxname, "blank sample")) {
            PostObjErr(eDiag_Error, eErr_SEQ_DESCR_TaxonomyBlankSample,
                "Blank sample should not be associated with any sequences",
                obj, ctx);
        }
    }

    // validate legal locations.
    if (bsrc.GetGenome() == CBioSource::eGenome_transposon ||
        bsrc.GetGenome() == CBioSource::eGenome_insertion_seq) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_ObsoleteSourceLocation,
            "Transposon and insertion sequence are no longer legal locations",
            obj, ctx);
    }

    if (IsIndexerVersion()
        && bsrc.IsSetGenome()
        && bsrc.GetGenome() == CBioSource::eGenome_chromosome) {
        PostObjErr(eDiag_Info, eErr_SEQ_DESCR_ChromosomeLocation,
            "INDEXER_ONLY - BioSource location is chromosome",
            obj, ctx);
    }

    bool isViral = false, isAnimal = false, isPlant = false,
        isBacteria = false, isArchaea = false, isFungal = false,
        isViroid = false;
    if (bsrc.IsSetLineage()) {
        string lineage = bsrc.GetLineage();
        if (NStr::StartsWith(lineage, "Viruses; ", NStr::eNocase)) {
            isViral = true;
        } else if (NStr::StartsWith(lineage, "Eukaryota; Metazoa; ", NStr::eNocase)) {
            isAnimal = true;
        } else if (NStr::StartsWith(lineage, "Eukaryota; Viridiplantae; Streptophyta; Embryophyta; ", NStr::eNocase)
            || NStr::StartsWith(lineage, "Eukaryota; Rhodophyta; ", NStr::eNocase)
            || NStr::StartsWith(lineage, "Eukaryota; stramenopiles; Phaeophyceae; ", NStr::eNocase)) {
            isPlant = true;
        } else if (NStr::StartsWith(lineage, "Bacteria; ", NStr::eNocase)) {
            isBacteria = true;
        } else if (NStr::StartsWith(lineage, "Archaea; ", NStr::eNocase)) {
            isArchaea = true;
        } else if (NStr::StartsWith(lineage, "Eukaryota; Fungi; ", NStr::eNocase)) {
            isFungal = true;
        } else if (NStr::StartsWith(lineage, "Viroids;", NStr::eNocase)) {
            isViroid = true;
        }
    }

    TCount count;
    bool chrom_conflict = false;
    CPCRSetList pcr_set_list;
    const CSubSource *chromosome = 0;
    string countryname;
    string lat_lon;
    double lat_value = 0.0, lon_value = 0.0;

    FOR_EACH_SUBSOURCE_ON_BIOSOURCE(ssit, bsrc)
    {
        ValidateSubSource(**ssit, obj, ctx);
        if (!(*ssit)->IsSetSubtype()) {
            continue;
        }

        if ((*ssit)->IsSetName()) {
            string str = (*ssit)->GetName();
            if (NStr::Equal(str, "N/A") || NStr::Equal(str, "Missing")) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency,
                    "Subsource name should not be " + str,
                    obj, ctx);
            }
        }

        int subtype = (*ssit)->GetSubtype();
        count[subtype]++;

        switch (subtype) {

        case CSubSource::eSubtype_country:
            countryname = (**ssit).GetName();
            break;

        case CSubSource::eSubtype_lat_lon:
            if ((*ssit)->IsSetName()) {
                lat_lon = (*ssit)->GetName();
                bool format_correct = false, lat_in_range = false, lon_in_range = false, precision_correct = false;
                CSubSource::IsCorrectLatLonFormat(lat_lon, format_correct, precision_correct,
                    lat_in_range, lon_in_range,
                    lat_value, lon_value);
            }
            break;

        case CSubSource::eSubtype_altitude:
            if (!(*ssit)->IsSetName() || !CSubSource::IsAltitudeValid((*ssit)->GetName())) {
                string val = "";
                if ((*ssit)->IsSetName()) {
                    val = (*ssit)->GetName();
                }
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadAltitude,
                    "'" + val + "' is an invalid altitude value, altitude should be provided in meters",
                    obj, ctx);
            }
            break;

        case CSubSource::eSubtype_chromosome:
            if (chromosome != 0) {
                if (NStr::CompareNocase((**ssit).GetName(), chromosome->GetName()) != 0) {
                    chrom_conflict = true;
                }
            } else {
                chromosome = ssit->GetPointer();
            }
            break;

        case CSubSource::eSubtype_fwd_primer_name:
            if ((*ssit)->IsSetName()) {
                pcr_set_list.AddFwdName((*ssit)->GetName());
            }
            break;

        case CSubSource::eSubtype_rev_primer_name:
            if ((*ssit)->IsSetName()) {
                pcr_set_list.AddRevName((*ssit)->GetName());
            }
            break;

        case CSubSource::eSubtype_fwd_primer_seq:
            if ((*ssit)->IsSetName()) {
                pcr_set_list.AddFwdSeq((*ssit)->GetName());
            }
            break;

        case CSubSource::eSubtype_rev_primer_seq:
            if ((*ssit)->IsSetName()) {
                pcr_set_list.AddRevSeq((*ssit)->GetName());
            }
            break;

        case CSubSource::eSubtype_sex:
        {
            EDiagSev sev = eDiag_Warning;
            if (IsGpipe() && IsGenomic()) {
                sev = eDiag_Error;
            }
            if (isAnimal || isPlant) {
                /* always allow /sex, but now check values */
                const string str = (*ssit)->GetName();
                if (!CSubSource::IsValidSexQualifierValue(str)) {
                    PostObjErr(eDiag_Error, eErr_SEQ_DESCR_InvalidSexQualifier,
                        "Invalid value (" + str + ") for /sex qualifier", obj, ctx);
                }
            } else if (isViral) {
                PostObjErr(sev, eErr_SEQ_DESCR_InvalidSexQualifier,
                    "Virus has unexpected Sex qualifier", obj, ctx);
            } else if (isBacteria || isArchaea || isFungal) {
                PostObjErr(sev, eErr_SEQ_DESCR_InvalidSexQualifier,
                    "Unexpected use of /sex qualifier", obj, ctx);
            } else {
                const string str = (*ssit)->GetName();
                if (!CSubSource::IsValidSexQualifierValue(str)) {
                    // otherwise values are restricted to specific list
                    PostObjErr(eDiag_Error, eErr_SEQ_DESCR_InvalidSexQualifier,
                        "Invalid value (" + str + ") for /sex qualifier", obj, ctx);
                }
            }
        }
            break;

        case CSubSource::eSubtype_mating_type:
            if (isAnimal || isPlant || isViral) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_InvalidMatingType,
                    "Unexpected use of /mating_type qualifier", obj, ctx);
            } else if (CSubSource::IsValidSexQualifierValue((*ssit)->GetName())) {
                // complain if one of the values that should go in /sex
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_InvalidMatingType,
                    "Unexpected use of /mating_type qualifier", obj, ctx);
            }
            break;

        case CSubSource::eSubtype_plasmid_name:
            if (!bsrc.IsSetGenome() || bsrc.GetGenome() != CBioSource::eGenome_plasmid) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_MissingPlasmidLocation,
                    "Plasmid subsource but not plasmid location", obj, ctx);
            }
            break;

        case CSubSource::eSubtype_plastid_name:
        {
            if ((*ssit)->IsSetName()) {
                CBioSource::TGenome genome = CBioSource::eGenome_unknown;
                if (bsrc.IsSetGenome()) {
                    genome = bsrc.GetGenome();
                }

                const string& subname = ((*ssit)->GetName());
                CBioSource::EGenome genome_from_name = CBioSource::GetGenomeByOrganelle(subname, NStr::eCase, false);
                if (genome_from_name == CBioSource::eGenome_chloroplast
                    || genome_from_name == CBioSource::eGenome_chromoplast
                    || genome_from_name == CBioSource::eGenome_kinetoplast
                    || genome_from_name == CBioSource::eGenome_plastid
                    || genome_from_name == CBioSource::eGenome_apicoplast
                    || genome_from_name == CBioSource::eGenome_leucoplast
                    || genome_from_name == CBioSource::eGenome_proplastid
                    || genome_from_name == CBioSource::eGenome_chromatophore) {
                    if (genome_from_name != genome) {
                        string val_name = CBioSource::GetOrganelleByGenome(genome_from_name);
                        if (NStr::StartsWith(val_name, "plastid:")) {
                            val_name = val_name.substr(8);
                        }
                        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadPlastidName,
                            "Plastid name subsource " + val_name + " but not " + val_name + " location", obj, ctx);
                    }
                } else {
                    PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadPlastidName,
                        "Plastid name subsource contains unrecognized value", obj, ctx);
                }
            }
        }
            break;

        case CSubSource::eSubtype_cell_line:
            if ((*ssit)->IsSetName() && orgref.IsSetTaxname()) {
                string warning = CSubSource::CheckCellLine((*ssit)->GetName(), orgref.GetTaxname());
                if (!NStr::IsBlank(warning)) {
                    PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_SuspectedContaminatedCellLine,
                        warning, obj, ctx);
                }
            }
            break;
        case CSubSource::eSubtype_tissue_type:
            if (isBacteria) {
                PostObjErr(eDiag_Error, eErr_SEQ_DESCR_InvalidTissueType,
                    "Tissue-type is inappropriate for bacteria", obj, ctx);
            } else if (isViroid) {
                PostObjErr(eDiag_Error, eErr_SEQ_DESCR_InvalidTissueType,
                    "Viroid has unexpected tissue-type qualifier", obj, ctx);
            }
            break;
        }

        if (isViral && IsUnexpectedViralSubSourceQualifier(subtype)) {
            string subname = CSubSource::GetSubtypeName(subtype);
            if (subname.length() > 0) {
                subname[0] = toupper(subname[0]);
            }
            PostObjErr(eDiag_Warning, 
                subtype == CSubSource::eSubtype_tissue_type ? eErr_SEQ_DESCR_InvalidTissueType : eErr_SEQ_DESCR_BioSourceInconsistency,
                "Virus has unexpected " + subname + " qualifier", obj, ctx);
        }
    }

    if (IsIndexerVersion() && ctx != NULL && CValidError_bioseq::IsWGS(*ctx) &&
        chromosome != NULL && (!bsrc.IsSetGenome() || bsrc.GetGenome() != CBioSource::eGenome_chromosome)) {
        // exception for /map="unlocalized"
        bool suppress = false;
        for (auto it : bsrc.GetSubtype()) {
            if (it->IsSetSubtype() && it->GetSubtype() == CSubSource::eSubtype_map &&
                it->IsSetName() && NStr::Equal(it->GetName(), "unlocalized")) {
                suppress = true;
                break;
            }
        }
        if (!suppress) {
            if (chromosome->IsSetName() && NStr::EqualNocase(chromosome->GetName(), "Unknown")) {
                const CSeq_entry& entry = *ctx;
                if (entry.IsSeq()) {
                    const CBioseq& bsp = entry.GetSeq();
                    FOR_EACH_SEQID_ON_BIOSEQ(itr, bsp) {
                        const CSeq_id& sid = **itr;
                        switch (sid.Which()) {
                        case CSeq_id::e_Genbank:
                        case CSeq_id::e_Embl:
                        case CSeq_id::e_Ddbj:
                        case CSeq_id::e_Tpg:
                        case CSeq_id::e_Tpe:
                        case CSeq_id::e_Tpd:
                        {
                            const CTextseq_id* tsid = sid.GetTextseq_Id();
                            // need to check accession format
                            if (tsid && tsid->IsSetAccession()) {
                                const string& acc = tsid->GetAccession();
                                if (acc.length() == 8) {
                                    suppress = true;
                                }
                            }
                        }
                        break;

                        default:
                            break;
                        }
                    }
                }
            }
        }
        if (!suppress) {
            string msg = "INDEXER_ONLY - source contains chromosome value '";
            if (chromosome->IsSetName()) {
                msg += chromosome->GetName();
            }
            msg += "' but the BioSource location is not set to chromosome";
            PostObjErr(eDiag_Error, eErr_SEQ_DESCR_ChromosomeWithoutLocation,
                msg, obj, ctx);
        }
    }

    ITERATE(TCount, it, count)
    {
        if (it->second <= 1) continue;
        if (CSubSource::IsMultipleValuesAllowed(it->first)) continue;
        string qual = "***";
        switch (it->first) {
        case CSubSource::eSubtype_chromosome:
            qual = chrom_conflict ? "conflicting chromosome" : "identical chromosome"; break;
        case CSubSource::eSubtype_germline:
            qual = "germline"; break;
        case CSubSource::eSubtype_rearranged:
            qual = "rearranged"; break;
        case CSubSource::eSubtype_plasmid_name:
            qual = "plasmid_name"; break;
        case CSubSource::eSubtype_segment:
            qual = "segment"; break;
        case CSubSource::eSubtype_country:
            qual = "country"; break;
        case CSubSource::eSubtype_transgenic:
            qual = "transgenic"; break;
        case CSubSource::eSubtype_environmental_sample:
            qual = "environmental_sample"; break;
        case CSubSource::eSubtype_lat_lon:
            qual = "lat_lon"; break;
        case CSubSource::eSubtype_collection_date:
            qual = "collection_date"; break;
        case CSubSource::eSubtype_collected_by:
            qual = "collected_by"; break;
        case CSubSource::eSubtype_identified_by:
            qual = "identified_by"; break;
        case CSubSource::eSubtype_fwd_primer_seq:
            qual = "fwd_primer_seq"; break;
        case CSubSource::eSubtype_rev_primer_seq:
            qual = "rev_primer_seq"; break;
        case CSubSource::eSubtype_fwd_primer_name:
            qual = "fwd_primer_name"; break;
        case CSubSource::eSubtype_rev_primer_name:
            qual = "rev_primer_name"; break;
        case CSubSource::eSubtype_metagenomic:
            qual = "metagenomic"; break;
        case CSubSource::eSubtype_altitude:
            qual = "altitude"; break;
        default:
            qual = CSubSource::GetSubtypeName(it->first);
            break;
        }
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleSourceQualifiers, "Multiple " + qual + " qualifiers present", obj, ctx);
    }

    if (count[CSubSource::eSubtype_germline] && count[CSubSource::eSubtype_rearranged]) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency,
            "Germline and rearranged should not both be present", obj, ctx);
    }
    if (count[CSubSource::eSubtype_transgenic] && count[CSubSource::eSubtype_environmental_sample]) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency,
            "Transgenic and environmental sample should not both be present", obj, ctx);
    }
    if (count[CSubSource::eSubtype_metagenomic] && !count[CSubSource::eSubtype_environmental_sample]) {
        PostObjErr(eDiag_Critical, eErr_SEQ_DESCR_MissingEnvironmentalSample,
            "Metagenomic should also have environmental sample annotated", obj, ctx);
    }
    if (count[CSubSource::eSubtype_sex] && count[CSubSource::eSubtype_mating_type]) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency,
            "Sex and mating type should not both be present", obj, ctx);
    }
    if (bsrc.IsSetGenome() && bsrc.GetGenome() == CBioSource::eGenome_plasmid && !count[CSubSource::eSubtype_plasmid_name]) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_MissingPlasmidName,
            "Plasmid location set but plasmid name missing. Add a plasmid source modifier with the plasmid name. Use unnamed if the name is not known.",
            obj, ctx);
    }

    if (static_cast<bool>(count[CSubSource::eSubtype_fwd_primer_seq]) != static_cast<bool>(count[CSubSource::eSubtype_rev_primer_seq]) &&
        !count[CSubSource::eSubtype_fwd_primer_name] && 
        !count[CSubSource::eSubtype_rev_primer_name]) {
        // if there are forward primers then there should also be reverse primers, and vice versa,
        // but ignore this if there are primer names of either flavor
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadPCRPrimerSequence,
            "PCR primer does not have both sequences", obj, ctx);
    }
    
    bool has_duplicate_primers = false;
    if (!pcr_set_list.AreSetsUnique()) {
        has_duplicate_primers = true;
    }
    if (bsrc.IsSetPcr_primers() && !CPCRSetList::AreSetsUnique(bsrc.GetPcr_primers())) {
        has_duplicate_primers = true;
    }

    if (has_duplicate_primers) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_DuplicatePCRPrimerSequence,
            "PCR primer sequence has duplicates", obj, ctx);
    }

    // check that country and lat_lon are compatible
    ValidateLatLonCountry(countryname, lat_lon, obj, ctx);

    // validates orgref in the context of lineage
    if (!orgref.IsSetOrgname() ||
        !orgref.GetOrgname().IsSetLineage() ||
        NStr::IsBlank(orgref.GetOrgname().GetLineage())) {

        if (!IsSeqSubmitParent() && IsIndexerVersion()) {
            EDiagSev sev = eDiag_Error;

            if (IsRefSeq()) {
                FOR_EACH_DBXREF_ON_ORGREF(it, orgref)
                {
                    if ((*it)->IsSetDb() && NStr::EqualNocase((*it)->GetDb(), "taxon")) {
                        sev = eDiag_Critical;
                        break;
                    }
                }
            }
            if (IsEmbl() || IsDdbj()) {
                sev = eDiag_Warning;
            }
            if (!IsWP()) {
                PostObjErr(sev, eErr_SEQ_DESCR_MissingLineage,
                    "No lineage for this BioSource.", obj, ctx);
            }
        }
    } else {
        const COrgName& orgname = orgref.GetOrgname();
        const string& lineage = orgname.GetLineage();
        if (bsrc.GetGenome() == CBioSource::eGenome_kinetoplast) {
            if (lineage.find("Kinetoplastida") == string::npos && lineage.find("Kinetoplastea") == string::npos) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrganelleLocation,
                    "Only Kinetoplastida have kinetoplasts", obj, ctx);
            }
        } else if (bsrc.GetGenome() == CBioSource::eGenome_nucleomorph) {
            if (lineage.find("Chlorarachniophyceae") == string::npos  &&
                lineage.find("Cryptophyta") == string::npos) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrganelleLocation,
                    "Only Chlorarachniophyceae and Cryptophyta have nucleomorphs", obj, ctx);
            }
        } else if (bsrc.GetGenome() == CBioSource::eGenome_macronuclear) {
            if (lineage.find("Ciliophora") == string::npos) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrganelleLocation,
                    "Only Ciliophora have macronuclear locations", obj, ctx);
            }
        }

        if (orgname.IsSetDiv()) {
            const string& div = orgname.GetDiv();
            if ((NStr::EqualCase(div, "BCT") || NStr::EqualCase(div, "VRL"))
                && bsrc.GetGenome() != CBioSource::eGenome_unknown
                && bsrc.GetGenome() != CBioSource::eGenome_genomic
                && bsrc.GetGenome() != CBioSource::eGenome_plasmid
                && bsrc.GetGenome() != CBioSource::eGenome_chromosome) {
                if (NStr::EqualCase(div, "BCT") && bsrc.GetGenome() == CBioSource::eGenome_extrachrom) {
                    // it's ok
                } else if (NStr::EqualCase(div, "VRL") && bsrc.GetGenome() == CBioSource::eGenome_proviral) {
                    // it's ok
                } else {
                    PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrganelleLocation,
                        "Bacterial or viral source should not have organelle location",
                        obj, ctx);
                }
            } else if (NStr::EqualCase(div, "ENV") && !count[CSubSource::eSubtype_environmental_sample]) {
                PostObjErr(eDiag_Error, eErr_SEQ_DESCR_MissingEnvironmentalSample,
                    "BioSource with ENV division is missing environmental sample subsource",
                    obj, ctx);
            }
        }

        if (!count[CSubSource::eSubtype_metagenomic] && NStr::FindNoCase(lineage, "metagenomes") != string::npos) {
            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_MissingMetagenomicQualifier,
                "If metagenomes appears in lineage, BioSource should have metagenomic qualifier",
                obj, ctx);
        }

    }

    // look for conflicts in orgmods, also look for unexpected viral qualifiers
    bool specific_host = false;
    if (orgref.IsSetOrgMod()) {
        for (auto it : orgref.GetOrgname().GetMod())
        {
            if (!it->IsSetSubtype()) {
                continue;
            }
            COrgMod::TSubtype subtype = it->GetSubtype();

            if (subtype == COrgMod::eSubtype_nat_host) {
                specific_host = true;
            }
            else if (subtype == COrgMod::eSubtype_strain) {
                if (count[CSubSource::eSubtype_environmental_sample]) {
                    PostObjErr(eDiag_Error, eErr_SEQ_DESCR_StrainWithEnvironSample, "Strain should not be present in an environmental sample",
                        obj, ctx);
                }
            }
            else if (subtype == COrgMod::eSubtype_metagenome_source) {
                if (!count[CSubSource::eSubtype_metagenomic]) {
                    PostObjErr(eDiag_Error, eErr_SEQ_DESCR_MissingMetagenomicQualifier, "Metagenome source should also have metagenomic qualifier",
                        obj, ctx);
                }
            }
            if (isViral && IsUnexpectedViralOrgModQualifier(subtype)) {
                string subname = COrgMod::GetSubtypeName(subtype);
                if (subname.length() > 0) {
                    subname[0] = toupper(subname[0]);
                }
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency,
                    "Virus has unexpected " + subname + " qualifier", obj, ctx);
            }
        }
    }
    if (count[CSubSource::eSubtype_environmental_sample] && !count[CSubSource::eSubtype_isolation_source] && !specific_host) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_EnvironSampleMissingQualifier,
            "Environmental sample should also have isolation source or specific host annotated",
            obj, ctx);
    }

    ValidateOrgRef(orgref, obj, ctx);
    if (bsrc.IsSetPcr_primers()) {
        ValidatePCRReactionSet(bsrc.GetPcr_primers(), obj, ctx);
    }
}


void CValidError_imp::x_ReportPCRSeqProblem
(const string& primer_kind,
char badch,
const CSerialObject& obj,
const CSeq_entry *ctx)
{
    if (badch < ' ' || badch > '~') {
        badch = '?';
    }
    string msg = "PCR " + primer_kind + " primer sequence format is incorrect, first bad character is '";
    msg += badch;
    msg += "'";
    PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadPCRPrimerSequence,
        msg, obj, ctx);
}


void CValidError_imp::x_CheckPCRPrimer
(const CPCRPrimer& primer,
const string& primer_kind,
const CSerialObject& obj,
const CSeq_entry *ctx)
{
    char badch = 0;
    if (primer.IsSetSeq() && !CPCRPrimerSeq::IsValid(primer.GetSeq(), badch)) {
        x_ReportPCRSeqProblem(primer_kind, badch, obj, ctx);
    }
    badch = 0;
    if (primer.IsSetName() && primer.GetName().Get().length() > 10
        && CPCRPrimerSeq::IsValid(primer.GetName(), badch)) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadPCRPrimerName,
            "PCR " + primer_kind + " primer name appears to be a sequence",
            obj, ctx);
    }

}


void CValidError_imp::ValidatePCRReactionSet
(const CPCRReactionSet& pcrset,
const CSerialObject& obj,
const CSeq_entry *ctx)
{
    for (auto it : pcrset.Get()) 
    {
        if (it->IsSetForward()) {
            for (auto pit : it->GetForward().Get())
            {
                x_CheckPCRPrimer(*pit, "forward", obj, ctx);
            }
        }
        if (it->IsSetReverse()) {
            for (auto pit : it->GetReverse().Get())
            {
                x_CheckPCRPrimer(*pit, "reverse", obj, ctx);
            }
        }
    }

}


void CValidError_imp::ValidateSubSource
(const CSubSource&    subsrc,
const CSerialObject& obj,
const CSeq_entry *ctx)
{
    if (!subsrc.IsSetSubtype()) {
        PostObjErr(eDiag_Critical, eErr_SEQ_DESCR_BadSubSource,
            "Unknown subsource subtype 0", obj, ctx);
        return;
    }

    // get taxname from object
    string taxname = kEmptyStr;
    if (obj.GetThisTypeInfo() == CSeqdesc::GetTypeInfo()) {
        const CSeqdesc* desc = dynamic_cast <const CSeqdesc*> (&obj);
        if (desc && desc->IsSource() && desc->GetSource().IsSetTaxname()) {
            taxname = desc->GetSource().GetOrg().GetTaxname();
        }
    } else if (obj.GetThisTypeInfo() == CSeq_feat::GetTypeInfo()) {
        const CSeq_feat* feat = dynamic_cast < const CSeq_feat* > (&obj);
        if (feat && feat->IsSetData()) {
            const auto& fdata = feat->GetData();
            if (fdata.IsBiosrc() && fdata.GetBiosrc().IsSetTaxname()) {
                taxname = feat->GetData().GetBiosrc().GetOrg().GetTaxname();
            }
        }
    }
    string sname = kEmptyStr;
    if (subsrc.IsSetName()) {
        sname = subsrc.GetName();
    }

    int subtype = subsrc.GetSubtype();
    switch (subtype) {

    case CSubSource::eSubtype_country:
    {
        string countryname = subsrc.GetName();
        bool is_miscapitalized = false;
        if (CCountries::IsValid(countryname, is_miscapitalized)) {
            if (is_miscapitalized) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadCountryCapitalization,
                    "Bad country capitalization [" + countryname + "]",
                    obj, ctx);
            }
            if (NStr::EndsWith(countryname, ":")) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadCountryCode,
                    "Colon at end of country name [" + countryname + "]", obj, ctx);
            }
            if (CCountries::WasValid(countryname)) {
                PostObjErr(eDiag_Info, eErr_SEQ_DESCR_ReplacedCountryCode,
                    "Replaced country name [" + countryname + "]", obj, ctx);
            }
        } else {
            if (countryname.empty()) {
                countryname = "?";
            }
            PostObjErr(eDiag_Error, eErr_SEQ_DESCR_BadCountryCode,
                "Bad country name [" + countryname + "]", obj, ctx);
        }
    }
        break;

    case CSubSource::eSubtype_lat_lon:
        if (subsrc.IsSetName()) {
            bool format_correct = false, lat_in_range = false, lon_in_range = false, precision_correct = false;
            double lat_value = 0.0, lon_value = 0.0;
            string lat_lon = subsrc.GetName();
            CSubSource::IsCorrectLatLonFormat(lat_lon, format_correct, precision_correct,
                lat_in_range, lon_in_range,
                lat_value, lon_value);
            if (!format_correct) {
                size_t pos = NStr::Find(lat_lon, ",");
                if (pos != string::npos) {
                    CSubSource::IsCorrectLatLonFormat(lat_lon.substr(0, pos), format_correct, precision_correct, lat_in_range, lon_in_range, lat_value, lon_value);
                    if (format_correct) {
                        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_LatLonFormat,
                            "lat_lon format has extra text after correct dd.dd N|S ddd.dd E|W format",
                            obj, ctx);
                    }
                }
            }

            if (!format_correct) {
                PostObjErr(eDiag_Error, eErr_SEQ_DESCR_LatLonFormat,
                    "lat_lon format is incorrect - should be dd.dd N|S ddd.dd E|W",
                    obj, ctx);
            } else {
                if (!lat_in_range) {
                    PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_LatLonRange,
                        "latitude value is out of range - should be between 90.00 N and 90.00 S",
                        obj, ctx);
                }
                if (!lon_in_range) {
                    PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_LatLonRange,
                        "longitude value is out of range - should be between 180.00 E and 180.00 W",
                        obj, ctx);
                }
                if (!precision_correct) {
                    /*
                    PostObjErr (eDiag_Info, eErr_SEQ_DESCR_LatLonPrecision,
                    "lat_lon precision is incorrect - should only have two digits to the right of the decimal point",
                    obj, ctx);
                    */
                }
            }
        }
        break;

    case CSubSource::eSubtype_fwd_primer_name:
        if (subsrc.IsSetName()) {
            string name = subsrc.GetName();
            char bad_ch;
            if (name.length() > 10
                && CPCRPrimerSeq::IsValid(name, bad_ch)) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadPCRPrimerName,
                    "PCR primer name appears to be a sequence",
                    obj, ctx);
            }
        }
        break;

    case CSubSource::eSubtype_rev_primer_name:
        if (subsrc.IsSetName()) {
            string name = subsrc.GetName();
            char bad_ch;
            if (name.length() > 10
                && CPCRPrimerSeq::IsValid(name, bad_ch)) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadPCRPrimerName,
                    "PCR primer name appears to be a sequence",
                    obj, ctx);
            }
        }
        break;

    case CSubSource::eSubtype_fwd_primer_seq:
    {
        char bad_ch = 0;
        if (!subsrc.IsSetName() || !CPCRPrimerSeq::IsValid(subsrc.GetName(), bad_ch)) {
            x_ReportPCRSeqProblem("forward", bad_ch, obj, ctx);
        }
    }
        break;

    case CSubSource::eSubtype_rev_primer_seq:
    {
        char bad_ch = 0;
        if (!subsrc.IsSetName() || !CPCRPrimerSeq::IsValid(subsrc.GetName(), bad_ch)) {
            x_ReportPCRSeqProblem("reverse", bad_ch, obj, ctx);
        }
    }
        break;

    case CSubSource::eSubtype_transposon_name:
    case CSubSource::eSubtype_insertion_seq_name:
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_ObsoleteSourceQual,
            "Transposon name and insertion sequence name are no "
            "longer legal qualifiers", obj, ctx);
        break;

    case 0:
        PostObjErr(eDiag_Critical, eErr_SEQ_DESCR_BadSubSource,
            "Unknown subsource subtype 0", obj, ctx);
        break;

    case CSubSource::eSubtype_other:
        ValidateSourceQualTags(subsrc.GetName(), obj, ctx);
        break;

    case CSubSource::eSubtype_germline:
        break;

    case CSubSource::eSubtype_rearranged:
        break;

    case CSubSource::eSubtype_transgenic:
        break;

    case CSubSource::eSubtype_metagenomic:
        break;

    case CSubSource::eSubtype_environmental_sample:
        break;

    case CSubSource::eSubtype_isolation_source:
        break;

    case CSubSource::eSubtype_sex:
        break;

    case CSubSource::eSubtype_mating_type:
        break;

    case CSubSource::eSubtype_chromosome:
        if (!CSubSource::IsChromosomeNameValid(sname, taxname)) {
            PostObjErr(eDiag_Error, eErr_SEQ_DESCR_BadPlasmidChromosomeLinkageName,
                "Problematic plasmid/chromosome/linkage group name '" + sname + "'",
                obj, ctx);        
        }
        break;
    case CSubSource::eSubtype_linkage_group:
        if (!CSubSource::IsLinkageGroupNameValid(sname, taxname)) {
            PostObjErr(eDiag_Error, eErr_SEQ_DESCR_BadPlasmidChromosomeLinkageName,
                "Problematic plasmid/chromosome/linkage group name '" + sname + "'",
                obj, ctx);        
        }
        break;
    case CSubSource::eSubtype_plasmid_name:
        if (!CSubSource::IsPlasmidNameValid(sname, taxname)) {
            PostObjErr(eDiag_Error, eErr_SEQ_DESCR_BadPlasmidChromosomeLinkageName,
                "Problematic plasmid/chromosome/linkage group name '" + sname + "'",
                obj, ctx);        
        }
        break;
    case CSubSource::eSubtype_segment:
        if (!CSubSource::IsSegmentValid(sname)) {
            PostObjErr(eDiag_Error, eErr_SEQ_DESCR_BadTextInSourceQualifier,
                CSubSource::GetSubtypeName(subsrc.GetSubtype()) + " value should start with letter or number",
                obj, ctx);
        }
        break;
    case CSubSource::eSubtype_endogenous_virus_name:
        if (!CSubSource::IsEndogenousVirusNameValid(sname)) {
            PostObjErr(eDiag_Error, eErr_SEQ_DESCR_BadTextInSourceQualifier,
                CSubSource::GetSubtypeName(subsrc.GetSubtype()) + " value should start with letter or number",
                obj, ctx);
        }
        break;

    case CSubSource::eSubtype_cell_line:
        break;

    case CSubSource::eSubtype_cell_type:
        break;

    case CSubSource::eSubtype_tissue_type:
        break;

    case CSubSource::eSubtype_frequency:
        if (subsrc.IsSetName() && !NStr::IsBlank(subsrc.GetName())) {
            const string& frequency = subsrc.GetName();
            if (NStr::Equal(frequency, "0")) {
                //ignore
            } else if (NStr::Equal(frequency, "1")) {
                PostObjErr(eDiag_Info, eErr_SEQ_DESCR_BadBioSourceFrequencyValue,
                    "bad frequency qualifier value " + frequency,
                    obj, ctx);
            } else {
                string::const_iterator sit = frequency.begin();
                bool bad_frequency = false;
                if (*sit == '0') {
                    ++sit;
                }
                if (sit != frequency.end() && *sit == '.') {
                    ++sit;
                    if (sit == frequency.end()) {
                        bad_frequency = true;
                    }
                    while (sit != frequency.end() && isdigit(*sit)) {
                        ++sit;
                    }
                    if (sit != frequency.end()) {
                        bad_frequency = true;
                    }
                } else {
                    bad_frequency = true;
                }
                if (bad_frequency) {
                    PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadBioSourceFrequencyValue,
                        "bad frequency qualifier value " + frequency,
                        obj, ctx);
                }
            }
        }
        break;
    case CSubSource::eSubtype_collection_date:
        if (!subsrc.IsSetName()) {
            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadCollectionDate,
                "Collection_date format is not in DD-Mmm-YYYY format",
                obj, ctx);
        } else {
            string problem = CSubSource::GetCollectionDateProblem(subsrc.GetName());
            if (!NStr::IsBlank(problem)) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadCollectionDate, problem, obj, ctx);
            }
        }
        break;

    default:
        break;
    }

    if (subsrc.IsSetName()) {
        if (CSubSource::NeedsNoText(subtype)) {
            if (subsrc.IsSetName() && !NStr::IsBlank(subsrc.GetName())) {
                string subname = CSubSource::GetSubtypeName(subtype);
                if (subname.length() > 0) {
                    subname[0] = toupper(subname[0]);
                }
                NStr::ReplaceInPlace(subname, "-", "_");
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadTextInSourceQualifier,
                    subname + " qualifier should not have descriptive text",
                    obj, ctx);
            }
        } else {
            const string& subname = subsrc.GetName();
            if (s_UnbalancedParentheses(subname)) {
                PostObjErr(eDiag_Error, eErr_SEQ_DESCR_UnbalancedParentheses,
                    "Unbalanced parentheses in subsource '" + subname + "'",
                    obj, ctx);
            }
            if (ContainsSgml(subname)) {
                PostObjErr(eDiag_Warning, eErr_GENERIC_SgmlPresentInText,
                    "subsource " + subname + " has SGML",
                    obj, ctx);
            }
        }
    }
}


static bool s_FindWholeName(const string& taxname, const string& value)
{
    if (NStr::IsBlank(taxname) || NStr::IsBlank(value)) {
        return false;
    }
    size_t pos = NStr::Find(taxname, value);
    size_t value_len = value.length();
    while (pos != string::npos
        && (((pos != 0 && isalpha(taxname.c_str()[pos - 1]))
        || isalpha(taxname.c_str()[pos + value_len])))) {
        pos = NStr::Find(taxname, value, pos + value_len);
    }
    if (pos == string::npos) {
        return false;
    } else {
        return true;
    }
}


static bool s_HasMetagenomeSource(const COrg_ref& org)
{
    if (!org.IsSetOrgMod()) {
        return false;
    }
    for (auto it : org.GetOrgname().GetMod()) {
        if (it->IsSetSubtype() && it->GetSubtype() == COrgMod::eSubtype_metagenome_source) {
            return true;
        }
    }
    return false;
}


bool CValidError_imp::s_IsSalmonellaGenus(const string& taxname)
{
    bool rval = false;
    auto pos = NStr::Find(taxname, " ");
    if (pos == string::npos) {
        if (NStr::EqualNocase(taxname, "Salmonella")) {
            rval = true;
        }
    } else if (pos > 0 && NStr::EqualNocase(taxname.substr(0, pos), "Salmonella")) {
        rval = true;
    }
    return rval;
}

EDiagSev CValidError_imp::x_SalmonellaErrorLevel()
{
    // per RW-1097
    return eDiag_Warning;

//    if (m_IsRefSeq) {
//        return eDiag_Warning;
//    } else {
//        return eDiag_Error;
//    }
}


void CValidError_imp::ValidateOrgRef
(const COrg_ref&    orgref,
const CSerialObject& obj,
const CSeq_entry *ctx)
{
    // Organism must have a name.
    if ((!orgref.IsSetTaxname() || orgref.GetTaxname().empty()) &&
        (!orgref.IsSetCommon() || orgref.GetCommon().empty())) {
        PostObjErr(eDiag_Fatal, eErr_SEQ_DESCR_NoOrgFound,
            "No organism name included in the source. Other qualifiers may exist.", obj, ctx);
    }

    string taxname = "";
    string lineage = "";
    if (orgref.IsSetOrgname() && orgref.GetOrgname().IsSetLineage()) {
        lineage = orgref.GetOrgname().GetLineage();
    }
    if (orgref.IsSetTaxname()) {
        taxname = orgref.GetTaxname();
        if (IsGenomeSubmission()) {
            if (s_HasMetagenomeSource(orgref)) {
                // suppress OrganismIsUndefinedSpecies message
            } else if ((NStr::EndsWith(taxname, " sp.", NStr::eNocase) ||
                NStr::EndsWith(taxname, " sp", NStr::eNocase) ||
                NStr::EndsWith(taxname, " (in: Fungi)", NStr::eNocase) ||
                NStr::EndsWith(taxname, " (in: Bacteria)", NStr::eNocase) ||
                NStr::EndsWith(taxname, " bacterium", NStr::eNocase) ||
                NStr::EndsWith(taxname, " archaeon", NStr::eNocase) ||
                NStr::EqualNocase(taxname, "bacterium") ||
                NStr::EqualNocase(taxname, "archaeon")) &&
                !NStr::StartsWith(taxname, "uncultured ", NStr::eNocase) &&
                !NStr::Equal(taxname, "Haemoproteus sp.", NStr::eNocase) &&
                !NStr::StartsWith(taxname, "symbiont ", NStr::eNocase) &&
                !NStr::StartsWith(taxname, "endosymbiont ", NStr::eNocase) &&
                NStr::FindNoCase(taxname, " symbiont ") == NPOS &&
                NStr::FindNoCase(taxname, " endosymbiont ") == NPOS) {

                PostObjErr(eDiag_Error, eErr_SEQ_DESCR_OrganismIsUndefinedSpecies,
                    "Organism '" + taxname + "' is undefined species and does not have a specific identifier.",
                    obj, ctx);
            }
        }
        if (s_UnbalancedParentheses(taxname)) {
            PostObjErr(eDiag_Error, eErr_SEQ_DESCR_UnbalancedParentheses,
                "Unbalanced parentheses in taxname '" + orgref.GetTaxname() + "'", obj, ctx);
        }
        if (ContainsSgml(taxname)) {
            PostObjErr(eDiag_Warning, eErr_GENERIC_SgmlPresentInText,
                "taxname " + taxname + " has SGML",
                obj, ctx);
        }

#if 0
        // VR-723: taxname must match orgname.name if present
        // commented out for now
        if (orgref.IsSetOrgname() && orgref.GetOrgname().IsSetName()) {
            ValidateTaxNameOrgname(taxname, orgref.GetOrgname(), obj, ctx);
        }
#endif
    }

    if (orgref.IsSetDb()) {
        ValidateDbxref(orgref.GetDb(), obj, true, ctx);
    }

    bool has_taxon = false;
    FOR_EACH_DBXREF_ON_ORGREF(dbt, orgref)
    {
        if (NStr::CompareNocase((*dbt)->GetDb(), "taxon") != 0) continue;
        has_taxon = true;
    }

    EDiagSev sev = eDiag_Warning;
    if (! IsLocalGeneralOnly()) {
        sev = eDiag_Error;
    }
    if (IsRequireTaxonID() && /* IsIndexerVersion() && */ !has_taxon) {
        PostObjErr(sev, eErr_SEQ_DESCR_NoTaxonID,
                   "BioSource is missing taxon ID", obj, ctx);
    }

    if (!orgref.IsSetOrgname()) {
        return;
    }
    const COrgName& orgname = orgref.GetOrgname();
    ValidateOrgName(orgname, has_taxon, obj, ctx);

    // Look for modifiers in taxname
    string taxname_search = taxname;
    // skip first two words
    size_t pos = NStr::Find(taxname_search, " ");
    if (pos == string::npos) {
        taxname_search.clear();
    } else {
        taxname_search = taxname_search.substr(pos + 1);
        NStr::TruncateSpacesInPlace(taxname_search);
        pos = NStr::Find(taxname_search, " ");
        if (pos == string::npos) {
            taxname_search.clear();
        } else {
            taxname_search = taxname_search.substr(pos + 1);
            NStr::TruncateSpacesInPlace(taxname_search);
        }
    }

    // determine if variety is present and in taxname - if so,
    // can ignore missing subspecies
    // also look for specimen-voucher (nat-host) if identical to taxname
    FOR_EACH_ORGMOD_ON_ORGNAME(it, orgname)
    {
        if (!(*it)->IsSetSubtype() || !(*it)->IsSetSubname()) {
            continue;
        }
        int subtype = (*it)->GetSubtype();
        const string& subname = (*it)->GetSubname();
        string orgmod_name = COrgMod::GetSubtypeName(subtype);
        if (orgmod_name.length() > 0) {
            orgmod_name[0] = toupper(orgmod_name[0]);
        }
        NStr::ReplaceInPlace(orgmod_name, "-", " ");
        if (subtype == COrgMod::eSubtype_sub_species) {
            if (!orgref.IsSubspeciesValid(subname)) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_OrgModMissingValue,
                    "Subspecies value specified is not found in taxname",
                    obj, ctx);
            }
        } else if (subtype == COrgMod::eSubtype_variety) {
            if (!orgref.IsVarietyValid(subname)) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_OrgModMissingValue,
                    orgmod_name + " value specified is not found in taxname",
                    obj, ctx);
            }
        } else if (subtype == COrgMod::eSubtype_forma
            || subtype == COrgMod::eSubtype_forma_specialis) {
            if (!s_FindWholeName(taxname_search, subname)) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_OrgModMissingValue,
                    orgmod_name + " value specified is not found in taxname",
                    obj, ctx);
            }
        } else if (subtype == COrgMod::eSubtype_nat_host) {
            if (NStr::EqualNocase(subname, taxname)) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_HostIdenticalToOrganism,
                    "Specific host is identical to taxname",
                    obj, ctx);
            }

        } else if (subtype == COrgMod::eSubtype_serotype) {
            // RW-1063
            if (s_IsSalmonellaGenus(taxname)) {
                PostObjErr(eDiag_Error, eErr_SEQ_DESCR_BadOrgMod,
                    "Salmonella organisms should use serovar instead of serotype.",
                    obj, ctx);
            }
        } else if (subtype == COrgMod::eSubtype_serovar) {
            // RW-1064
            if (s_IsSalmonellaGenus(taxname) && NStr::Find(taxname, subname) == string::npos) {
                PostObjErr(x_SalmonellaErrorLevel(), eErr_SEQ_DESCR_BadOrgMod,
                    "Salmonella organism name should contain the serovar value.",
                    obj, ctx);
            }            
        }
    }
}


//LCOV_EXCL_START
//per VR-723, the call to this code is commented out
static bool s_MatchOrgname(const string& taxname, const COrgName& orgname, string& mismatch)
{
    mismatch = kEmptyStr;
    bool rval = false;
    if (!orgname.IsSetName()) {
        return false;
    }
    orgname.GetFlatName(mismatch, NULL);
    if (NStr::Equal(taxname, mismatch)) {
        return true;
    }
    // special cases
    switch (orgname.GetName().Which()) {
    case COrgName::C_Name::e_Hybrid:
    {
        const auto& hybrid = orgname.GetName().GetHybrid().Get();
        for (auto it : hybrid) {
            if (it->IsSetName() && s_MatchOrgname(taxname, *it, mismatch)) {
                rval = true;
                break;
            }
        }
        if (!rval && hybrid.size() > 1 &&
            hybrid.front()->IsSetName()) {
            // use first element for error
            s_MatchOrgname(taxname, *(hybrid.front()), mismatch);
        }
    }
        break;
    case COrgName::C_Name::e_Partial:
    {
        const auto& partial = orgname.GetName().GetPartial().Get();
        for (auto it : partial) {
            if (it->IsSetName()) {
                mismatch = it->GetName();
                rval = NStr::Equal(taxname, mismatch);
                if (rval) {
                    break;
                }
            }
        }
        if (!rval && partial.size() > 1 &&
            partial.front()->IsSetName()) {
            // use first element for error
            mismatch = partial.front()->GetName();
        }
    }
        break;
    default:
        break;
    }
    return rval;
}


void CValidError_imp::ValidateTaxNameOrgname
(const string& taxname, 
 const COrgName& orgname, 
 const CSerialObject& obj, 
 const CSeq_entry *ctx)
{
    string mismatch = kEmptyStr;
    if (!s_MatchOrgname(taxname, orgname, mismatch)) {
        PostObjErr(eDiag_Error, eErr_SEQ_DESCR_BioSourceInconsistency,
                    "Taxname does not match orgname ('" + taxname + "', '" + mismatch + "')",
                    obj, ctx);
    }
}
//LCOV_EXCL_STOP


void CValidError_imp::ValidateOrgName
(const COrgName& orgname,
const bool has_taxon,
const CSerialObject& obj,
const CSeq_entry *ctx)
{
    if (orgname.IsSetMod()) {
        bool has_strain = false;
        vector<string> vouchers;
        FOR_EACH_ORGMOD_ON_ORGNAME(omd_itr, orgname)
        {
            const COrgMod& omd = **omd_itr;
            int subtype = omd.GetSubtype();

            if (omd.IsSetSubname()) {
                string str = omd.GetSubname();
                if (NStr::Equal(str, "N/A") || NStr::Equal(str, "Missing")) {
                    PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency,
                        "Orgmod name should not be " + str,
                        obj, ctx);
                }
            }

            switch (subtype) {
            case 0:
            case 1:
                PostObjErr(eDiag_Critical, eErr_SEQ_DESCR_BadOrgMod,
                    "Unknown orgmod subtype " + NStr::IntToString(subtype), obj, ctx);
                break;
            case COrgMod::eSubtype_strain:
                if (omd.IsSetSubname()) {
                    string str = omd.GetSubname();
                    if (NStr::StartsWith(str, "subsp. ")) {
                        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_OrgModValueInvalid,
                            "Orgmod.strain should not start with subsp.",
                            obj, ctx);
                    } else if (NStr::StartsWith(str, "serovar ")) {
                        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_OrgModValueInvalid,
                            "Orgmod.strain should not start with serovar",
                            obj, ctx);
                    } else if (!COrgMod::IsStrainValid(str)) {
                        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_OrgModValueInvalid,
                            "Orgmod.strain should not be '" + str + "'",
                            obj, ctx);
                    }
                }
                if (has_strain) {
                    PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleStrains,
                        "Multiple strain qualifiers on the same BioSource", obj, ctx);
                }
                has_strain = true;
                break;
            case COrgMod::eSubtype_serovar:
                if (omd.IsSetSubname()) {
                    string str = omd.GetSubname();
                    if (NStr::StartsWith(str, "subsp. ")) {
                        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_OrgModValueInvalid,
                            "Orgmod.serovar should not start with subsp.",
                            obj, ctx);
                    } else if (NStr::StartsWith(str, "strain ")) {
                        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_OrgModValueInvalid,
                            "Orgmod.serovar should not start with strain",
                            obj, ctx);
                    }
                }
                break;
            case COrgMod::eSubtype_sub_species:
                if (omd.IsSetSubname()) {
                    string str = omd.GetSubname();
                    if (NStr::Find(str, "subsp. ") != string::npos) {
                        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_OrgModValueInvalid,
                            "Orgmod.sub-species should not contain subsp.",
                            obj, ctx);
                    }
                }
                break;
            case COrgMod::eSubtype_variety:
                if ((!orgname.IsSetDiv() || !NStr::EqualNocase(orgname.GetDiv(), "PLN"))
                    && (!orgname.IsSetLineage() ||
                    (NStr::Find(orgname.GetLineage(), "Cyanobacteria") == string::npos
                    && NStr::Find(orgname.GetLineage(), "Myxogastria") == string::npos
                    && NStr::Find(orgname.GetLineage(), "Oomycetes") == string::npos))) {
                    if (!has_taxon) {
                        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadVariety,
                            "Orgmod variety should only be in plants, fungi, or cyanobacteria",
                            obj, ctx);
                    }
                }
                break;
            case COrgMod::eSubtype_other:
                ValidateSourceQualTags(omd.GetSubname(), obj, ctx);
                break;
            case COrgMod::eSubtype_synonym:
                if ((*omd_itr)->IsSetSubname() && !NStr::IsBlank((*omd_itr)->GetSubname())) {
                    const string& val = (*omd_itr)->GetSubname();

                    // look for synonym/gb_synonym duplication
                    FOR_EACH_ORGMOD_ON_ORGNAME(it2, orgname)
                    {
                        if ((*it2)->IsSetSubtype()
                            && (*it2)->GetSubtype() == COrgMod::eSubtype_gb_synonym
                            && (*it2)->IsSetSubname()
                            && NStr::EqualNocase(val, (*it2)->GetSubname())) {
                            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_OrgModValueInvalid,
                                "OrgMod synonym is identical to OrgMod gb_synonym",
                                obj, ctx);
                        }
                    }
                }
                break;
            case COrgMod::eSubtype_bio_material:
            case COrgMod::eSubtype_culture_collection:
            case COrgMod::eSubtype_specimen_voucher:
                ValidateOrgModVoucher(omd, obj, ctx);
                vouchers.push_back(omd.GetSubname());
                break;

            case COrgMod::eSubtype_type_material:
                if (!(*omd_itr)->IsSetSubname() || 
                    !COrgMod::IsValidTypeMaterial((*omd_itr)->GetSubname()))  {
                    PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadTypeMaterial,
                            "Bad value for type_material", obj, ctx);
                }
                break;

            default:
                break;
            }
            if (omd.IsSetSubname()) {
                const string& subname = omd.GetSubname();

                if (subtype != COrgMod::eSubtype_old_name && s_UnbalancedParentheses(subname)) {
                    PostObjErr(eDiag_Error, eErr_SEQ_DESCR_UnbalancedParentheses,
                        "Unbalanced parentheses in orgmod '" + subname + "'",
                        obj, ctx);
                }
                if (ContainsSgml(subname)) {
                    PostObjErr(eDiag_Warning, eErr_GENERIC_SgmlPresentInText,
                        "orgmod " + subname + " has SGML",
                        obj, ctx);
                }
            }
        }

        string err = COrgMod::CheckMultipleVouchers(vouchers);
        if (!err.empty()) PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_IdenticalInstitutionCode, err, obj, ctx);
    }
}


bool CValidError_imp::IsOtherDNA(const CBioseq_Handle& bsh) const
{
    if (bsh) {
        CSeqdesc_CI sd(bsh, CSeqdesc::e_Molinfo);
        if (sd) {
            const CSeqdesc::TMolinfo& molinfo = sd->GetMolinfo();
            if (molinfo.CanGetBiomol() &&
                molinfo.GetBiomol() == CMolInfo::eBiomol_other) {
                return true;
            }
        }
    }
    return false;
}


static bool s_CompleteGenomeNeedsChromosome(const CBioSource& source)
{
    bool rval = false;

    if (!source.IsSetGenome()
        || source.GetGenome() == CBioSource::eGenome_genomic
        || source.GetGenome() == CBioSource::eGenome_unknown) {
        bool is_viral = false;
        if (source.IsSetOrg()) {
            const COrg_ref& org = source.GetOrg();
            if (org.IsSetDivision() && NStr::Equal(org.GetDivision(), "PHG")) {
                is_viral = true;
            } else if (org.IsSetLineage()) {
                const string& lineage = org.GetLineage();
                if (NStr::StartsWith(lineage, "Viruses; ")
                    || NStr::StartsWith(lineage, "Viroids; ")) {
                    is_viral = true;
                }
            }
        }
        rval = !is_viral;
    }
    return rval;
}


bool s_IsBacteria(const CBioSource& source)
{
    bool rval = false;

    if (source.IsSetLineage()) {
        string lineage = source.GetLineage();
        if (NStr::StartsWith(lineage, "Bacteria; ", NStr::eNocase)) {
            rval = true;
        }
    }
    return rval;
}


bool s_IsArchaea(const CBioSource& source)
{
    bool rval = false;

    if (source.IsSetLineage()) {
        string lineage = source.GetLineage();
        if (NStr::StartsWith(lineage, "Archaea; ", NStr::eNocase)) {
            rval = true;
        }
    }
    return rval;
}


bool s_IsBioSample(const CBioseq_Handle& bsh)
{
    bool rval = false;
    CSeqdesc_CI d(bsh, CSeqdesc::e_User);
    while (d && !rval) {
        const auto & user = d->GetUser();
        if (user.IsSetType() && user.GetType().IsStr() && NStr::Equal(user.GetType().GetStr(), "DBLink")) {
            for (auto f : user.GetData()) {
                if (f->IsSetLabel() && f->GetLabel().IsStr() && NStr::Equal(f->GetLabel().GetStr(), "BioSample")
                    && f->IsSetData() && (f->GetData().IsStr() || f->GetData().IsStrs())) {
                    rval = true;
                    break;
                }
            }
        }
        ++d;
    }
    return rval;
}


void CValidError_imp::ValidateBioSourceForSeq
(const CBioSource&    source,
const CSerialObject& obj,
const CSeq_entry    *ctx,
const CBioseq_Handle& bsh)
{
    m_biosource_kind = source;

    const auto & inst = bsh.GetInst();

    if (source.IsSetIs_focus()) {
        // skip proteins, segmented bioseqs, or segmented parts
        if (!bsh.IsAa() &&
            !(inst.GetRepr() == CSeq_inst::eRepr_seg) &&
            !(GetAncestor(*(bsh.GetCompleteBioseq()), CBioseq_set::eClass_parts) != 0)) {
            if (!CFeat_CI(bsh, CSeqFeatData::e_Biosrc)) {
                PostObjErr(eDiag_Error,
                    eErr_SEQ_DESCR_UnnecessaryBioSourceFocus,
                    "BioSource descriptor has focus, "
                    "but no BioSource feature", obj, ctx);
            }
        }
    }
    if (source.CanGetOrigin() &&
        source.GetOrigin() == CBioSource::eOrigin_synthetic) {
        if (!IsOtherDNA(bsh) && !bsh.IsAa()) {
            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_InvalidForType,
                "Molinfo-biomol other should be used if "
                "Biosource-location is synthetic", obj, ctx);
        }
    }

    // check locations for HIV biosource
    if (inst.IsSetMol()
        && source.IsSetOrg() && source.GetOrg().IsSetTaxname()
        && (NStr::EqualNocase(source.GetOrg().GetTaxname(), "Human immunodeficiency virus")
        || NStr::EqualNocase(source.GetOrg().GetTaxname(), "Human immunodeficiency virus 1")
        || NStr::EqualNocase(source.GetOrg().GetTaxname(), "Human immunodeficiency virus 2"))) {

        if (inst.GetMol() == CSeq_inst::eMol_dna) {
            if (!source.IsSetGenome() || source.GetGenome() != CBioSource::eGenome_proviral) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentVirusMoltype,
                    "HIV with moltype DNA should be proviral",
                    obj, ctx);
            }
        } else if (inst.GetMol() == CSeq_inst::eMol_rna) {
            CSeqdesc_CI mi(bsh, CSeqdesc::e_Molinfo);
            if (mi && mi->GetMolinfo().IsSetBiomol()
                && mi->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_mRNA) {
                PostObjErr(eDiag_Info, eErr_SEQ_DESCR_InconsistentVirusMoltype,
                    "HIV with mRNA molecule type is rare",
                    obj, ctx);
            }
        }
    }

    CSeqdesc_CI mi(bsh, CSeqdesc::e_Molinfo);
    CSeqdesc_CI ti(bsh, CSeqdesc::e_Title);

    // look for viral completeness
    if (mi && mi->IsMolinfo() && ti && ti->IsTitle()) {
        const CMolInfo& molinfo = mi->GetMolinfo();
        if (molinfo.IsSetBiomol() && molinfo.GetBiomol() == CMolInfo::eBiomol_genomic
            && molinfo.IsSetCompleteness() && molinfo.GetCompleteness() == CMolInfo::eCompleteness_complete
            && NStr::Find(ti->GetTitle(), "complete genome") != string::npos
            && s_CompleteGenomeNeedsChromosome(source)) {
            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceNeedsChromosome,
                "Non-viral complete genome not labeled as chromosome",
                obj, ctx);
        }
    }

    if (mi) {
        // look for synthetic/artificial
        bool is_synthetic_construct = IsSyntheticConstruct(source);
        bool is_artificial = IsArtificial(source);

        if (is_synthetic_construct) {
            if ((!mi->GetMolinfo().IsSetBiomol()
                || mi->GetMolinfo().GetBiomol() != CMolInfo::eBiomol_other_genetic) && !bsh.IsAa()) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_SyntheticConstructWrongMolType,
                    "synthetic construct should have other-genetic",
                    obj, ctx);
            }
            if (!is_artificial) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_SyntheticConstructNeedsArtificial,
                    "synthetic construct should have artificial origin",
                    obj, ctx);
            }
        } else if (is_artificial) {
            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_InvalidForType,
                "artificial origin should have other-genetic and synthetic construct",
                obj, ctx);
        }
        if (is_artificial) {
            if ((!mi->GetMolinfo().IsSetBiomol()
                || mi->GetMolinfo().GetBiomol() != CMolInfo::eBiomol_other_genetic)
                && !bsh.IsAa()) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_InvalidForType,
                    "artificial origin should have other-genetic",
                    obj, ctx);
            }
        }
    }


    // validate subsources in context

    FOR_EACH_SUBSOURCE_ON_BIOSOURCE(it, source)
    {
        if (!(*it)->IsSetSubtype()) {
            continue;
        }
        CSubSource::TSubtype subtype = (*it)->GetSubtype();

        switch (subtype) {
        case CSubSource::eSubtype_other:
            // look for conflicting cRNA notes on subsources
            if (mi && (*it)->IsSetName() && NStr::EqualNocase((*it)->GetName(), "cRNA")) {
                const CMolInfo& molinfo = mi->GetMolinfo();
                if (!molinfo.IsSetBiomol()
                    || molinfo.GetBiomol() != CMolInfo::eBiomol_cRNA) {
                    PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentVirusMoltype,
                        "cRNA note conflicts with molecule type",
                        obj, ctx);
                } else {
                    PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentVirusMoltype,
                        "cRNA note redundant with molecule type",
                        obj, ctx);
                }
            }
            break;
        default:
            break;
        }
    }


    // look at orgref in context
    if (source.IsSetOrg()) {
        const COrg_ref& orgref = source.GetOrg();

        if (mi) {
            const CMolInfo& molinfo = mi->GetMolinfo();
            // look for conflicting cRNA notes on orgmod
            if (orgref.IsSetOrgname() && orgref.GetOrgname().IsSetMod()) {
                for (auto it : orgref.GetOrgname().GetMod())
                {
                    if (it->IsSetSubtype()
                        && it->GetSubtype() == COrgMod::eSubtype_other
                        && it->IsSetSubname()
                        && NStr::EqualNocase(it->GetSubname(), "cRNA")) {
                        if (!molinfo.IsSetBiomol()
                            || molinfo.GetBiomol() != CMolInfo::eBiomol_cRNA) {
                            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentVirusMoltype,
                                "cRNA note conflicts with molecule type",
                                obj, ctx);
                        }
                        else {
                            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentVirusMoltype,
                                "cRNA note redundant with molecule type",
                                obj, ctx);
                        }
                    }
                }
            }

            if (orgref.IsSetLineage()) {
                const string& lineage = orgref.GetOrgname().GetLineage();

                // look for incorrect DNA stage
                if (molinfo.IsSetBiomol() && molinfo.GetBiomol() == CMolInfo::eBiomol_genomic
                    && bsh.IsSetInst() && bsh.GetInst().IsSetMol() && bsh.GetInst().GetMol() == CSeq_inst::eMol_dna
                    && NStr::StartsWith(lineage, "Viruses; ")
                    && NStr::FindNoCase(lineage, "no DNA stage") != string::npos) {
                    PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentVirusMoltype,
                        "Genomic DNA viral lineage indicates no DNA stage",
                        obj, ctx);
                }
            }
        }

    }

    if ( (IsGpipe() || IsIndexerVersion() ) & s_IsBioSample(bsh) ) {
        bool is_bact = s_IsBacteria(source);
        bool is_arch = s_IsArchaea(source);
        if ( is_bact || is_arch ) {
            bool has_strain = false;
            bool has_isolate = false;
            bool env_sample = false;
            if (source.IsSetSubtype()) {
                ITERATE(CBioSource::TSubtype, s, source.GetSubtype())
                {
                    if ((*s)->IsSetSubtype() && (*s)->GetSubtype() == CSubSource::eSubtype_environmental_sample) {
                        env_sample = true;
                        break;
                    }
                }
            }
            if (!env_sample && source.IsSetOrg()
                && source.GetOrg().IsSetOrgname()) {
                const auto& orgname = source.GetOrg().GetOrgname();
                if (orgname.IsSetMod()) {
                    for (auto om : orgname.GetMod()) {
                        if (om->IsSetSubtype()) {
                            if (om->GetSubtype() == COrgMod::eSubtype_isolate) {
                                has_isolate = true;
                                break;
                            }
                            else if (om->GetSubtype() == COrgMod::eSubtype_strain) {
                                has_strain = true;
                                break;
                            }
                        }
                    }
                }
            }


            if (!has_strain && !has_isolate && !env_sample) {
                if (is_bact) {
                    PostObjErr(eDiag_Error, eErr_SEQ_DESCR_BacteriaMissingSourceQualifier,
                        "Bacteria should have strain or isolate or environmental sample",
                        obj, ctx);
                } else if (is_arch) {
                    PostObjErr(eDiag_Error, eErr_SEQ_DESCR_BacteriaMissingSourceQualifier,
                        "Archaea should have strain or isolate or environmental sample",
                        obj, ctx);
                }
            }
        }
    }


}




bool CValidError_imp::IsTransgenic(const CBioSource& bsrc)
{
    if (bsrc.CanGetSubtype()) {
        FOR_EACH_SUBSOURCE_ON_BIOSOURCE(sbs_itr, bsrc)
        {
            const CSubSource& sbs = **sbs_itr;
            if (sbs.GetSubtype() == CSubSource::eSubtype_transgenic) {
                return true;
            }
        }
    }
    return false;
}


const string CValidError_imp::sm_SourceQualPrefixes[] = {
    "acronym:",
    "altitude:",
    "anamorph:",
    "authority:",
    "biotype:",
    "biovar:",
    "bio_material:",
    "breed:",
    "cell_line:",
    "cell_type:",
    "chemovar:",
    "chromosome:",
    "clone:",
    "clone_lib:",
    "collected_by:",
    "collection_date:",
    "common:",
    "country:",
    "cultivar:",
    "culture_collection:",
    "dev_stage:",
    "dosage:",
    "ecotype:",
    "endogenous_virus_name:",
    "environmental_sample:",
    "forma:",
    "forma_specialis:",
    "frequency:",
    "fwd_pcr_primer_name",
    "fwd_pcr_primer_seq",
    "fwd_primer_name",
    "fwd_primer_seq",
    "genotype:",
    "germline:",
    "group:",
    "haplogroup:",
    "haplotype:",
    "identified_by:",
    "insertion_seq_name:",
    "isolate:",
    "isolation_source:",
    "lab_host:",
    "lat_lon:",
    "left_primer:",
    "linkage_group:",
    "map:",
    "mating_type:",
    "metagenome_source:",
    "metagenomic:",
    "nat_host:",
    "pathovar:",
    "phenotype:",
    "placement:",
    "plasmid_name:",
    "plastid_name:",
    "pop_variant:",
    "rearranged:",
    "rev_pcr_primer_name",
    "rev_pcr_primer_seq",
    "rev_primer_name",
    "rev_primer_seq",
    "right_primer:",
    "segment:",
    "serogroup:",
    "serotype:",
    "serovar:",
    "sex:",
    "specimen_voucher:",
    "strain:",
    "subclone:",
    "subgroup:",
    "substrain:",
    "subtype:",
    "sub_species:",
    "synonym:",
    "taxon:",
    "teleomorph:",
    "tissue_lib:",
    "tissue_type:",
    "transgenic:",
    "transposon_name:",
    "type:",
    "variety:",
    "whole_replicon:",
};


void CValidError_imp::InitializeSourceQualTags()
{
    m_SourceQualTags.reset(new CTextFsa(true));
    size_t size = sizeof(sm_SourceQualPrefixes) / sizeof(string);

    for (size_t i = 0; i < size; ++i) {
        m_SourceQualTags->AddWord(sm_SourceQualPrefixes[i]);
    }

    m_SourceQualTags->Prime();
}


void CValidError_imp::ValidateSourceQualTags
(const string& str,
const CSerialObject& obj,
const CSeq_entry *ctx)
{
    if (NStr::IsBlank(str)) return;

    size_t str_len = str.length();

    int state = m_SourceQualTags->GetInitialState();

    for (size_t i = 0; i < str_len; ++i) {
        state = m_SourceQualTags->GetNextState(state, str[i]);
        if (m_SourceQualTags->IsMatchFound(state)) {
            string match = m_SourceQualTags->GetMatches(state)[0];
            if (match.empty()) {
                match = "?";
            }
            size_t match_len = match.length();

            bool okay = true;
            if ((int)(i - match_len) >= 0) {
                char ch = str[i - match_len];
                if (!isspace((unsigned char)ch) && ch != ';') {
                    okay = false;
#if 0
                    // look to see if there's a longer match in the list
                    for (size_t k = 0; 
                        k < sizeof (CValidError_imp::sm_SourceQualPrefixes) / sizeof (string); 
                        k++) {
                        size_t pos = NStr::FindNoCase (str, CValidError_imp::sm_SourceQualPrefixes[k]);
                        if (pos != string::npos) {
                            if (pos == 0 || isspace ((unsigned char) str[pos]) || str[pos] == ';') {
                                okay = true;
                                match = CValidError_imp::sm_SourceQualPrefixes[k];
                                break;
                            }
                        }
                    }
#endif
                }
            }
            if (okay) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_StructuredSourceNote,
                    "Source note has structured tag '" + match + "'", obj, ctx);
            }
        }
    }
}



static bool x_HasTentativeName(const CUser_object &user_object)
{
    if (!FIELD_IS_SET_AND_IS(user_object, Type, Str) ||
        user_object.GetType().GetStr() != "StructuredComment") {
        return false;
    }

    FOR_EACH_USERFIELD_ON_USEROBJECT(user_field_iter, user_object)
    {
        const CUser_field &field = **user_field_iter;
        if (FIELD_IS_SET_AND_IS(field, Label, Str) && FIELD_IS_SET_AND_IS(field, Data, Str)) {
            if (GET_FIELD(field.GetLabel(), Str) == "Tentative Name") {
                if (GET_FIELD(field.GetData(), Str) != "not provided") {
                    return true;
                }
            }
        }
    }

    return false;
}


static string x_GetTentativeName(const CUser_object &user_object)
{
    if (!FIELD_IS_SET_AND_IS(user_object, Type, Str) ||
        user_object.GetType().GetStr() != "StructuredComment") {
        return "";
    }

    FOR_EACH_USERFIELD_ON_USEROBJECT(user_field_iter, user_object)
    {
        const CUser_field &field = **user_field_iter;
        if (FIELD_IS_SET_AND_IS(field, Label, Str) && FIELD_IS_SET_AND_IS(field, Data, Str)) {
            if (GET_FIELD(field.GetLabel(), Str) == "Tentative Name") {
                if (GET_FIELD(field.GetData(), Str) != "not provided") {
                    return GET_FIELD(field.GetData(), Str);
                }
            }
        }
    }

    return "";
}


void CValidError_imp::GatherTentativeName
(const CSeq_entry& se,
vector<CConstRef<CSeqdesc> >& usr_descs,
vector<CConstRef<CSeq_entry> >& desc_ctxs,
vector<CConstRef<CSeq_feat> >& usr_feats)
{
    // get source descriptors
    if (se.IsSetDescr()) {
        for (auto it : se.GetDescr().Get())
        {
            if (it->IsUser() && x_HasTentativeName(it->GetUser())) {
                CConstRef<CSeqdesc> desc;
                desc.Reset(it);
                usr_descs.push_back(desc);
                CConstRef<CSeq_entry> r_se;
                r_se.Reset(&se);
                desc_ctxs.push_back(r_se);
            }
        }
    }
    // also get features
    if (se.IsSetAnnot()) {
        for (auto annot_it : se.GetAnnot()) {
            if (annot_it->IsFtable()) {
                for (auto feat_it : annot_it->GetData().GetFtable()) {
                     if (feat_it->IsSetData() && feat_it->GetData().IsUser()
                        && x_HasTentativeName(feat_it->GetData().GetUser())) {
                        CConstRef<CSeq_feat> feat;
                        feat.Reset(feat_it);
                        usr_feats.push_back(feat);
                    }
                }
            }
        }
    }

    // if set, recurse
    if (se.IsSet()) {
        FOR_EACH_SEQENTRY_ON_SEQSET(it, se.GetSet())
        {
            GatherTentativeName(**it, usr_descs, desc_ctxs, usr_feats);
        }
    }
}


const size_t kDefaultChunkSize = 1000;
void CValidError_imp::ValidateOrgRefs(CTaxValidationAndCleanup& tval)
{
    vector< CRef<COrg_ref> > org_rq_list = tval.GetTaxonomyLookupRequest();

    if (org_rq_list.size() > 0) {
        
        size_t chunk_size = kDefaultChunkSize;
        size_t i = 0;
        while (i < org_rq_list.size()) {
            size_t len = min(chunk_size, org_rq_list.size() - i);
            vector< CRef<COrg_ref> >  tmp_rq(org_rq_list.begin() + i, org_rq_list.begin() + i + len);
            CRef<CTaxon3_reply> reply = x_GetTaxonService()->SendOrgRefList(tmp_rq);
            if (!reply || !reply->IsSetReply()) {
                if (chunk_size > 20) {
                    chunk_size = chunk_size / 2;
                } else {
                    PostErr(eDiag_Error, eErr_SEQ_DESCR_TaxonomyServiceProblem,
                        "Taxonomy service connection failure", *(tval.GetTopReportObject()));
                    break;
                }
            } else {
                tval.ReportIncrementalTaxLookupErrors(*reply, *this, (IsPatent() || IsINSDInSep()), i);
                i += chunk_size;
            }
        }
    }
}

void CValidError_imp::ValidateSpecificHost
(CTaxValidationAndCleanup& tval)
{
    vector<CRef<COrg_ref> > org_rq_list = tval.GetSpecificHostLookupRequest(false);

    if (org_rq_list.size() == 0) {
        return;
    }

    size_t chunk_size = kDefaultChunkSize;
    size_t i = 0;
    while (i < org_rq_list.size()) {
        size_t len = min(chunk_size, org_rq_list.size() - i);
        vector< CRef<COrg_ref> >  tmp_rq(org_rq_list.begin() + i, org_rq_list.begin() + i + len);
        CRef<CTaxon3_reply> tmp_spec_host_reply = x_GetTaxonService()->SendOrgRefList(tmp_rq);
        string err_msg = kEmptyStr;
        if (tmp_spec_host_reply) {
            err_msg = tval.IncrementalSpecificHostMapUpdate(tmp_rq, *tmp_spec_host_reply);
        } else {
            err_msg = "Connection to taxonomy failed";
        }
        if (!NStr::IsBlank(err_msg)) {
            PostErr(eDiag_Error, eErr_SEQ_DESCR_TaxonomyLookupProblem, err_msg, *(tval.GetTopReportObject()));
            return;
        }
        i += chunk_size;
    }

    tval.ReportSpecificHostErrors(*this);
}


void CValidError_imp::ValidateStrain
(CTaxValidationAndCleanup& tval)
{
    vector<CRef<COrg_ref> > org_rq_list = tval.GetStrainLookupRequest();

    if (org_rq_list.size() == 0) {
        return;
    }

    size_t chunk_size = kDefaultChunkSize;
    size_t i = 0;
    while (i < org_rq_list.size()) {
        size_t len = min(chunk_size, org_rq_list.size() - i);
        vector< CRef<COrg_ref> >  tmp_rq(org_rq_list.begin() + i, org_rq_list.begin() + i + len);
        CRef<CTaxon3_reply> tmp_spec_host_reply = x_GetTaxonService()->SendOrgRefList(tmp_rq);
        string err_msg = tval.IncrementalStrainMapUpdate(tmp_rq, *tmp_spec_host_reply);
        if (!NStr::IsBlank(err_msg)) {
            PostErr(eDiag_Error, eErr_SEQ_DESCR_TaxonomyLookupProblem, err_msg, *(tval.GetTopReportObject()));
            return;
        }
        i += chunk_size;
    }

    tval.ReportStrainErrors(*this);
}


void CValidError_imp::ValidateSpecificHost(const CSeq_entry& se)
{
    CTaxValidationAndCleanup tval;
    tval.Init(se);
    ValidateSpecificHost(tval);
}


bool IsOrgNotFound(const CT3Error& error)
{
    const string err_str = error.IsSetMessage() ? error.GetMessage() : "?";

    if (NStr::Equal(err_str, "Organism not found")) {
        return true;
    } else {
        return false;
    }
}


void CValidError_imp::ValidateTentativeName(const CSeq_entry& se)
{
    vector<CConstRef<CSeqdesc> > src_descs;
    vector<CConstRef<CSeq_entry> > desc_ctxs;
    vector<CConstRef<CSeq_feat> > src_feats;

    GatherTentativeName(se, src_descs, desc_ctxs, src_feats);

    // request list for taxon3
    vector< CRef<COrg_ref> > org_rq_list;

    // first do descriptors
    vector<CConstRef<CSeqdesc> >::iterator desc_it = src_descs.begin();
    vector<CConstRef<CSeq_entry> >::iterator ctx_it = desc_ctxs.begin();
    while (desc_it != src_descs.end() && ctx_it != desc_ctxs.end()) {
        const string& taxname = x_GetTentativeName((*desc_it)->GetUser());
        CRef<COrg_ref> rq(new COrg_ref);
        rq->SetTaxname(taxname);
        org_rq_list.push_back(rq);

        ++desc_it;
        ++ctx_it;
    }

    // now do features
    vector<CConstRef<CSeq_feat> >::iterator feat_it = src_feats.begin();
    while (feat_it != src_feats.end()) {
        const string& taxname = x_GetTentativeName((*feat_it)->GetData().GetUser());
        CRef<COrg_ref> rq(new COrg_ref);
        rq->SetTaxname(taxname);
        org_rq_list.push_back(rq);

        ++feat_it;
    }

    if (org_rq_list.empty()) {
        return;
    }

    CRef<CTaxon3_reply> reply = x_GetTaxonService()->SendOrgRefList(org_rq_list);
    if (!reply || !reply->IsSetReply()) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_TaxonomyServiceProblem,
            "Taxonomy service connection failure", se);
    }

    const auto& rlist = reply->GetReply();
    CTaxon3_reply::TReply::const_iterator reply_it = rlist.begin();

    // process descriptor responses
    desc_it = src_descs.begin();
    ctx_it = desc_ctxs.begin();
    size_t pos = 0;

    while (reply_it != rlist.end()
        && desc_it != src_descs.end()
        && ctx_it != desc_ctxs.end()) {
        if ((*reply_it)->IsError()) {
            if (IsOrgNotFound((*reply_it)->GetError())) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadTentativeName,
                    "Taxonomy lookup failed for Tentative Name '" + org_rq_list[pos]->GetTaxname() + "'",
                    **desc_it, *ctx_it);
            } else {
                HandleTaxonomyError((*reply_it)->GetError(),
                    eErr_SEQ_DESCR_BadTentativeName, **desc_it, *ctx_it);
            }
        }
        ++reply_it;
        ++desc_it;
        ++ctx_it;
        ++pos;
    }

    // process feat responses
    feat_it = src_feats.begin();
    while (reply_it != rlist.end()
        && feat_it != src_feats.end()) {
        if ((*reply_it)->IsError()) {
            if (IsOrgNotFound((*reply_it)->GetError())) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadTentativeName,
                    "Taxonomy lookup failed for Tentative Name '" + org_rq_list[pos]->GetTaxname() + "'",
                    **feat_it);
            } else {
                HandleTaxonomyError((*reply_it)->GetError(),
                    eErr_SEQ_DESCR_BadTentativeName, **feat_it);
            }
        }
        ++reply_it;
        ++feat_it;
        ++pos;
    }
}

void CValidError_imp::HandleTaxonomyError(const CT3Error& error,
    const EErrType type, const CSeqdesc& desc, const CSeq_entry* entry)
{
    const string err_str = error.IsSetMessage() ? error.GetMessage() : "?";

    if (NStr::Equal(err_str, "Organism not found")) {
        string msg = "Organism not found in taxonomy database";
        if (error.IsSetOrg()) {
            const auto& e_org = error.GetOrg();
            const auto& d_org = desc.GetSource().GetOrg();
            if (e_org.IsSetTaxname() &&
                !NStr::Equal(e_org.GetTaxname(), "Not valid") &&
                (!d_org.IsSetTaxname() ||
                    !NStr::Equal(d_org.GetTaxname(), e_org.GetTaxname()))) {
                msg += " (suggested:" + e_org.GetTaxname() + ")";
            }
        }
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_OrganismNotFound,
            msg,
            desc, entry);
    } else if (NStr::Equal(err_str, kInvalidReplyMsg)) {
        PostObjErr(eDiag_Error, eErr_SEQ_DESCR_TaxonomyLookupProblem,
            err_str,
            desc, entry);
    } else if (NStr::Find(err_str, "ambiguous name") != NPOS) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_TaxonomyAmbiguousName,
            "Taxonomy lookup failed with message '" + err_str + "'",
            desc, entry);
    } else {
        PostObjErr(eDiag_Warning, type,
            "Taxonomy lookup failed with message '" + err_str + "'",
            desc, entry);
    }
}

void CValidError_imp::HandleTaxonomyError(const CT3Error& error,
    const EErrType type, const CSeq_feat& feat)
{
    const string err_str = error.IsSetMessage() ? error.GetMessage() : "?";

    if (NStr::Equal(err_str, kInvalidReplyMsg)) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_TaxonomyLookupProblem,
            err_str,
            feat);
    } else if (NStr::Find(err_str, "ambiguous name") != NPOS) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_TaxonomyAmbiguousName,
            "Taxonomy lookup failed with message '" + err_str + "'",
            feat);
    } else {
        PostErr(eDiag_Warning, type,
            "Taxonomy lookup failed with message '" + err_str + "'",
            feat);
    }
}

void CValidError_imp::HandleTaxonomyError(const CT3Error& error,
    const string& host, const COrg_ref& org)
{
    const string err_str = error.IsSetMessage() ? error.GetMessage() : "?";

    if (NStr::Equal(err_str, "Organism not found")) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_OrganismNotFound,
            "Organism not found in taxonomy database",
            org);
    } else if (NStr::FindNoCase(err_str, "ambiguous") != string::npos) {
        PostObjErr(eDiag_Info, eErr_SEQ_DESCR_AmbiguousSpecificHost,
            "Specific host value is ambiguous: " + host, org);
    } else if (NStr::Equal(err_str, kInvalidReplyMsg)) {
        PostObjErr(eDiag_Error, eErr_SEQ_DESCR_TaxonomyLookupProblem,
            err_str,
            org);
    } else {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadSpecificHost,
            "Invalid value for specific host: " + host, org);
    }
}


void CValidError_imp::ValidateTaxonomy(const CSeq_entry& se)
{
    CTaxValidationAndCleanup tval;
    tval.Init(se);

    ValidateOrgRefs(tval);

    // Now look at specific-host values
    ValidateSpecificHost(tval);

    // Commented out until TM-725 is resolved
    ValidateStrain(tval);

    ValidateTentativeName(se);
}


void CValidError_imp::ValidateTaxonomy(const COrg_ref& org, int genome)
{
    CTaxValidationAndCleanup tval;
    tval.CheckOneOrg(org, genome, *this);
}


CPCRSet::CPCRSet(size_t pos) : m_OrigPos(pos)
{
}


CPCRSet::~CPCRSet(void)
{
}


CPCRSetList::CPCRSetList(void)
{
    m_SetList.clear();
}


CPCRSetList::~CPCRSetList(void)
{
    for (size_t i = 0; i < m_SetList.size(); i++) {
        delete m_SetList[i];
    }
    m_SetList.clear();
}


void CPCRSetList::AddFwdName(string name)
{
    unsigned int pcr_num = 0;
    if (NStr::StartsWith(name, "(") && NStr::EndsWith(name, ")") && NStr::Find(name, ",") != string::npos) {
        name = name.substr(1, name.length() - 2);
        vector<string> mult_names;
        NStr::Split(name, ",", mult_names, 0);
        unsigned int name_num = 0;
        while (name_num < mult_names.size()) {
            while (pcr_num < m_SetList.size() && !NStr::IsBlank(m_SetList[pcr_num]->GetFwdName())) {
                pcr_num++;
            }
            if (pcr_num == m_SetList.size()) {
                m_SetList.push_back(new CPCRSet(pcr_num));
            }
            m_SetList[pcr_num]->SetFwdName(mult_names[name_num]);
            name_num++;
            pcr_num++;
        }
    } else {
        while (pcr_num < m_SetList.size() && !NStr::IsBlank(m_SetList[pcr_num]->GetFwdName())) {
            pcr_num++;
        }
        if (pcr_num == m_SetList.size()) {
            m_SetList.push_back(new CPCRSet(pcr_num));
        }
        m_SetList[pcr_num]->SetFwdName(name);
    }
}


void CPCRSetList::AddRevName(string name)
{
    unsigned int pcr_num = 0;
    if (NStr::StartsWith(name, "(") && NStr::EndsWith(name, ")") && NStr::Find(name, ",") != string::npos) {
        name = name.substr(1, name.length() - 2);
        vector<string> mult_names;
        NStr::Split(name, ",", mult_names, 0);
        unsigned int name_num = 0;
        while (name_num < mult_names.size()) {
            while (pcr_num < m_SetList.size() && !NStr::IsBlank(m_SetList[pcr_num]->GetRevName())) {
                pcr_num++;
            }
            if (pcr_num == m_SetList.size()) {
                m_SetList.push_back(new CPCRSet(pcr_num));
            }
            m_SetList[pcr_num]->SetRevName(mult_names[name_num]);
            name_num++;
            pcr_num++;
        }
    } else {
        while (pcr_num < m_SetList.size() && !NStr::IsBlank(m_SetList[pcr_num]->GetRevName())) {
            pcr_num++;
        }
        if (pcr_num == m_SetList.size()) {
            m_SetList.push_back(new CPCRSet(pcr_num));
        }
        m_SetList[pcr_num]->SetRevName(name);
    }
}


void CPCRSetList::AddFwdSeq(string name)
{
    unsigned int pcr_num = 0;
    if (NStr::StartsWith(name, "(") && NStr::EndsWith(name, ")") && NStr::Find(name, ",") != string::npos) {
        name = name.substr(1, name.length() - 2);
        vector<string> mult_names;
        NStr::Split(name, ",", mult_names, 0);
        unsigned int name_num = 0;
        while (name_num < mult_names.size()) {
            while (pcr_num < m_SetList.size() && !NStr::IsBlank(m_SetList[pcr_num]->GetFwdSeq())) {
                pcr_num++;
            }
            if (pcr_num == m_SetList.size()) {
                m_SetList.push_back(new CPCRSet(pcr_num));
            }
            m_SetList[pcr_num]->SetFwdSeq(mult_names[name_num]);
            name_num++;
            pcr_num++;
        }
    } else {
        while (pcr_num < m_SetList.size() && !NStr::IsBlank(m_SetList[pcr_num]->GetFwdSeq())) {
            pcr_num++;
        }
        if (pcr_num == m_SetList.size()) {
            m_SetList.push_back(new CPCRSet(pcr_num));
        }
        m_SetList[pcr_num]->SetFwdSeq(name);
    }
}


void CPCRSetList::AddRevSeq(string name)
{
    unsigned int pcr_num = 0;
    if (NStr::StartsWith(name, "(") && NStr::EndsWith(name, ")") && NStr::Find(name, ",") != string::npos) {
        name = name.substr(1, name.length() - 2);
        vector<string> mult_names;
        NStr::Split(name, ",", mult_names, 0);
        unsigned int name_num = 0;
        while (name_num < mult_names.size()) {
            while (pcr_num < m_SetList.size() && !NStr::IsBlank(m_SetList[pcr_num]->GetRevSeq())) {
                pcr_num++;
            }
            if (pcr_num == m_SetList.size()) {
                m_SetList.push_back(new CPCRSet(pcr_num));
            }
            m_SetList[pcr_num]->SetRevSeq(mult_names[name_num]);
            name_num++;
            pcr_num++;
        }
    } else {
        while (pcr_num < m_SetList.size() && !NStr::IsBlank(m_SetList[pcr_num]->GetRevSeq())) {
            pcr_num++;
        }
        if (pcr_num == m_SetList.size()) {
            m_SetList.push_back(new CPCRSet(pcr_num));
        }
        m_SetList[pcr_num]->SetRevSeq(name);
    }
}


static bool s_PCRSetCompare(
    const CPCRSet* p1,
    const CPCRSet* p2
    )

{
    int compare = NStr::CompareNocase(p1->GetFwdSeq(), p2->GetFwdSeq());
    if (compare < 0) {
        return true;
    } else if (compare > 0) {
        return false;
    } else if ((compare = NStr::CompareNocase(p1->GetRevSeq(), p2->GetRevSeq())) < 0) {
        return true;
    } else if (compare > 0) {
        return false;
    } else if ((compare = NStr::CompareNocase(p1->GetFwdName(), p2->GetFwdName())) < 0) {
        return true;
    } else if (compare > 0) {
        return false;
    } else if ((compare = NStr::CompareNocase(p1->GetRevName(), p2->GetRevName())) < 0) {
        return true;
    } else if (p1->GetOrigPos() < p2->GetOrigPos()) {
        return true;
    } else {
        return false;
    }
}


static bool s_PCRSetEqual(
    const CPCRSet* p1,
    const CPCRSet* p2
    )

{
    if (NStr::EqualNocase(p1->GetFwdSeq(), p2->GetFwdSeq())
        && NStr::EqualNocase(p1->GetRevSeq(), p2->GetRevSeq())
        && NStr::EqualNocase(p1->GetFwdName(), p2->GetFwdName())
        && NStr::EqualNocase(p1->GetRevName(), p2->GetRevName())) {
        return true;
    } else {
        return false;
    }
}


bool CPCRSetList::AreSetsUnique(void)
{
    stable_sort(m_SetList.begin(),
        m_SetList.end(),
        s_PCRSetCompare);

    return seq_mac_is_unique(m_SetList.begin(),
        m_SetList.end(),
        s_PCRSetEqual);

}


static bool s_PCRPrimerLess(const CPCRPrimer& p1, const CPCRPrimer& p2)
{
    if (!p1.IsSetSeq() && p2.IsSetSeq()) {
        return true;
    } else if (p1.IsSetSeq() && !p2.IsSetSeq()) {
        return false;
    } else if (p1.IsSetSeq() && p2.IsSetSeq()) {
        int compare = NStr::CompareNocase(p1.GetSeq().Get(), p2.GetSeq().Get());
        if (compare < 0) {
            return true;
        } else if (compare > 0) {
            return false;
        }
    }
    if (!p1.IsSetName() && p2.IsSetName()) {
        return true;
    } else if (p1.IsSetName() && !p2.IsSetName()) {
        return false;
    } else if (p1.IsSetName() && p2.IsSetName()) {
        return (NStr::CompareNocase(p1.GetName().Get(), p2.GetName().Get()) < 0);
    } else {
        return false;
    }
}


static bool s_PCRPrimerSetLess(const CPCRPrimerSet& s1, const CPCRPrimerSet& s2)
{
    if (!s1.IsSet() && s2.IsSet()) {
        return true;
    } else if (s1.IsSet() && !s2.IsSet()) {
        return false;
    } else if (!s1.IsSet() && !s2.IsSet()) {
        return false;
    } else if (s1.Get().size() < s2.Get().size()) {
        return true;
    } else if (s1.Get().size() > s2.Get().size()) {
        return false;
    } else {
        auto it1 = s1.Get().begin();
        auto it2 = s2.Get().begin();
        while (it1 != s1.Get().end()) {
            if (s_PCRPrimerLess(**it1, **it2)) {
                return true;
            } else if (s_PCRPrimerLess(**it2, **it1)) {
                return false;
            }  else {
                // the two are equal, continue comparisons
            }
            ++it1;
            ++it2;
        }
        return false;
    }
}


static bool s_PCRReactionLess(
    CConstRef<CPCRReaction> pp1,
    CConstRef<CPCRReaction> pp2
)

{
    const CPCRReaction& p1 = *pp1;
    const CPCRReaction& p2 = *pp2;
    if (!p1.IsSetForward() && p2.IsSetForward()) {
        return true;
    } else if (p1.IsSetForward() && !p2.IsSetForward()) {
        return false;
    } else if (p2.IsSetForward() && p1.IsSetForward()) {
        if (s_PCRPrimerSetLess(p1.GetForward(), p2.GetForward())) {
            return true;
        } else if (s_PCRPrimerSetLess(p2.GetForward(), p1.GetForward())) {
            return false;
        }
    } 
    if (!p1.IsSetReverse() && p2.IsSetReverse()) {
        return true;
    } else if (p1.IsSetReverse() && !p2.IsSetReverse()) {
        return false;
    } else if (p1.IsSetReverse() && p2.IsSetReverse()) {
        if (s_PCRPrimerSetLess(p1.GetReverse(), p2.GetReverse())) {
            return true;
        } else if (s_PCRPrimerSetLess(p2.GetReverse(), p1.GetReverse())) {
            return false;
        }
    } 
    return false;
}

struct SPCRReactionLess
{
    template <typename T>
    bool operator()(T l, T r) const
    {
        _ASSERT(l.NotEmpty());
        _ASSERT(r.NotEmpty());

        return s_PCRReactionLess(l, r);
    }
};

using TPCRReactionSet = set<CConstRef<CPCRReaction>, SPCRReactionLess>;

bool CPCRSetList::AreSetsUnique(const CPCRReactionSet& primers)
{
    if (!primers.IsSet() || primers.Get().size() < 2) {
           return true;
    }
    TPCRReactionSet already_seen;
    for (auto it : primers.Get()) {
        if (already_seen.find(it) != already_seen.end()) {
            return false;
        }
        already_seen.insert(it);
    }
    return true;
}



// ===== for validating instituation and collection codes in specimen-voucher, ================
void CValidError_imp::ValidateOrgModVoucher(const COrgMod& orgmod, const CSerialObject& obj, const CSeq_entry *ctx)
{

    if (!orgmod.IsSetSubtype() || !orgmod.IsSetSubname() || NStr::IsBlank(orgmod.GetSubname())) {
        return;
    }

    int subtype = orgmod.GetSubtype();
    string val = orgmod.GetSubname();

    string error = "";
    switch (subtype) {
    case COrgMod::eSubtype_culture_collection:
        error = COrgMod::IsCultureCollectionValid(val);
        break;
    case COrgMod::eSubtype_bio_material:
        error = COrgMod::IsBiomaterialValid(val);
        break;
    case COrgMod::eSubtype_specimen_voucher:
        error = COrgMod::IsSpecimenVoucherValid(val);
        break;
    default:
        break;
    }

    vector<string> error_list;
    NStr::Split(error, "\n", error_list, 0);
    ITERATE(vector<string>, err, error_list) {
        if (NStr::IsBlank(*err)) {
            // do nothing
        } else if (NStr::FindNoCase(*err, "should be structured") != string::npos) {
            PostObjErr(eDiag_Error, eErr_SEQ_DESCR_UnstructuredVoucher, *err, obj, ctx);
        } else if (NStr::FindNoCase(*err, "missing institution code") != string::npos) {
            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadInstitutionCode, *err, obj, ctx);
        } else if (NStr::FindNoCase(*err, "missing specific identifier") != string::npos) {
            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_IncorrectlyFormattedVoucherID, *err, obj, ctx);
        } else if (NStr::FindNoCase(*err, "should be") != string::npos) {
            EDiagSev level = eDiag_Info;
            if (NStr::StartsWith(*err, "DNA")) {
                level = eDiag_Warning;
            }
            PostObjErr(level, eErr_SEQ_DESCR_WrongVoucherType, *err, obj, ctx);
        } else if (NStr::StartsWith(*err, "Personal")) {
            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_MissingPersonalCollectionName,
                *err, obj, ctx);
        } else if (NStr::FindNoCase(*err, "should not be qualified with a <COUNTRY> designation") != string::npos) {
            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadInstitutionCountry, *err, obj, ctx);
        } else if (NStr::FindNoCase(*err, "needs to be qualified with a <COUNTRY> designation") != string::npos) {
            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadInstitutionCode, *err, obj, ctx);
        } else if (NStr::FindNoCase(*err, " exists, but collection ") != string::npos) {
            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadCollectionCode, *err, obj, ctx);
        } else {
            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadInstitutionCode, *err, obj, ctx);
        }
    }
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
