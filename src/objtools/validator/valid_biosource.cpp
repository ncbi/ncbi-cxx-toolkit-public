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

#include "validatorp.hpp"
#include "utilities.hpp"

#include <serial/iterator.hpp>
#include <serial/enumvalues.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/general/Name_std.hpp>

#include <objects/seqalign/Seq_align.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SubSource.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Textseq_id.hpp>

#include <objects/seqres/Seq_graph.hpp>

#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>

#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/scope.hpp>

#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>

#include <objects/biblio/Author.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Cit_let.hpp>
#include <objects/biblio/Cit_proc.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/PubMedId.hpp>
#include <objects/biblio/PubStatus.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Affil.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/taxon3/taxon3.hpp>
#include <objects/taxon3/Taxon3_reply.hpp>

#include <objtools/error_codes.hpp>
#include <util/sgml_entity.hpp>
#include <util/line_reader.hpp>
#include <util/util_misc.hpp>

#include <algorithm>
#include <math.h>

#include <serial/iterator.hpp>

#define NCBI_USE_ERRCODE_X   Objtools_Validator

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;


auto_ptr<CTextFsa> CValidError_imp::m_SourceQualTags;
auto_ptr<CLatLonCountryMap> CValidError_imp::m_LatLonCountryMap;
auto_ptr<CLatLonCountryMap> CValidError_imp::m_LatLonWaterMap;




static bool s_UnbalancedParentheses (string str)
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
                return true ;
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


const string sm_ValidSexQualifierValues[] = {
  "asexual",
  "bisexual",
  "diecious",
  "dioecious",
  "female",
  "hermaphrodite",
  "male",
  "monecious",
  "monoecious",
  "unisexual",
};

static bool s_IsValidSexQualifierValue (string str)

{
    str = NStr::ToLower(str);

    const string *begin = sm_ValidSexQualifierValues;
    const string *end = &(sm_ValidSexQualifierValues[sizeof(sm_ValidSexQualifierValues) / sizeof(string)]);

    return find(begin, end, str) != end;
}


const string sm_ValidModifiedPrimerBases[] = {
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

    const string *list_begin = sm_ValidModifiedPrimerBases;
    const string *list_end = &(sm_ValidModifiedPrimerBases[sizeof(sm_ValidModifiedPrimerBases) / sizeof(string)]);

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


static void s_IsCorrectLatLonFormat (string lat_lon, bool& format_correct, bool& lat_in_range, bool& lon_in_range,
                                     double& lat_value, double& lon_value)
{
    format_correct = false;
    lat_in_range = false;
    lon_in_range = false;
    double ns, ew;
    char lon, lat;
    int processed;

    lat_value = 0.0;
    lon_value = 0.0;
    
    if (NStr::IsBlank(lat_lon)) {
        return;
    } else if (sscanf (lat_lon.c_str(), "%lf %c %lf %c%n", &ns, &lat, &ew, &lon, &processed) != 4
               || processed != lat_lon.length()) {
        return;
    } else if ((lat != 'N' && lat != 'S') || (lon != 'E' && lon != 'W')) {
        return;
    } else {
        // init values found
        if (lat == 'N') {
            lat_value = ns;
        } else {
            lat_value = 0.0 - ns;
        }
        if (lon == 'E') {
            lon_value = ew;
        } else {
            lon_value = 0.0 - ew;
        }

    // make sure format is correct
        vector<string> pieces;
        NStr::Tokenize(lat_lon, " ", pieces);
        if (pieces.size() > 3) {
            int precision_lat = 0;
            size_t pos = NStr::Find(pieces[0], ".");
            if (pos != string::npos) {
                precision_lat = pieces[0].length() - pos - 1;
            }
            int precision_lon = 0;
            pos = NStr::Find(pieces[2], ".");
            if (pos != string::npos) {
                precision_lon = pieces[2].length() - pos - 1;
            }
            
            char reformatted[1000];
            sprintf (reformatted, "%.*lf %c %.*lf %c", precision_lat, ns, lat,
                                                       precision_lon, ew, lon);

            size_t len = strlen (reformatted);
            if (NStr::StartsWith(lat_lon, reformatted)
                && (len == lat_lon.length() 
                  || (len < lat_lon.length() 
                      && lat_lon.c_str()[len] == ';'))) {
                format_correct = true;
                if (ns <= 90 && ns >= 0) {
                    lat_in_range = true;
                }
                if (ew <= 180 && ew >= 0) {
                    lon_in_range = true;
                }
            }
        }
    }
}


bool CValidError_imp::IsSyntheticConstruct (const CBioSource& src) 
{
    if (!src.IsSetOrg()) {
        return false;
    }
    if (src.GetOrg().IsSetTaxname()) {
        string taxname = src.GetOrg().GetTaxname();
        if (NStr::EqualNocase(taxname, "synthetic construct") || NStr::FindNoCase(taxname, "vector") != string::npos) {
            return true;
        }
    }

    if (src.GetOrg().IsSetLineage()) {
        if (NStr::FindNoCase(src.GetOrg().GetLineage(), "artificial sequences") != string::npos) {
            return true;
        }
    }

    if (src.GetOrg().IsSetOrgname() && src.GetOrg().GetOrgname().IsSetDiv()
        && NStr::EqualNocase(src.GetOrg().GetOrgname().GetDiv(), "syn")) {
        return true;
    }
    return false;
}


bool CValidError_imp::IsArtificial (const CBioSource& src) 
{
    if (src.IsSetOrigin() 
        && (src.GetOrigin() == CBioSource::eOrigin_artificial)) {
        return true;
    }
    return false;
}


bool CValidError_imp::IsOrganelle (int genome)
{
    bool rval = false;
    switch (genome) {
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
            rval = true;
            break;
        default:
            rval = false;
            break;
    }
    return rval;
}


bool CValidError_imp::IsOrganelle (CBioseq_Handle seq)
{
    bool rval = false;
    CSeqdesc_CI sd(seq, CSeqdesc::e_Source);
    if (sd && sd->GetSource().IsSetGenome() && IsOrganelle(sd->GetSource().GetGenome())) {
        rval = true;
    }
    return rval;
}

static double DistanceOnGlobe (
  double latA,
  double lonA,
  double latB,
  double lonB
);

CLatLonCountryId *CValidError_imp::x_CalculateLatLonId(float lat_value, float lon_value, string country, string province)
{
    CLatLonCountryId *id = new CLatLonCountryId(lat_value, lon_value);

    bool goodmatch = false;

    // lookup region by coordinates, or find nearest region and calculate distance
    const CCountryExtreme * guess = m_LatLonCountryMap->GuessRegionForLatLon(lat_value, lon_value, country, province);
    if (guess) {
        id->SetFullGuess(guess->GetCountry());
        id->SetGuessCountry(guess->GetLevel0());
        id->SetGuessProvince(guess->GetLevel1());
        if (NStr::EqualNocase(country, id->GetGuessCountry())
            && (NStr::IsBlank(province) || NStr::EqualNocase(province, id->GetGuessProvince()))) {
            goodmatch = true;
        }
    } else {
        // not inside a country, check water
        guess = m_LatLonWaterMap->GuessRegionForLatLon(lat_value, lon_value, country);
        if (guess) {
            // found inside water
            id->SetGuessWater(guess->GetCountry());
            if (NStr::EqualNocase(country, id->GetGuessWater())) {
                goodmatch = true;
            }

            // also see if close to land for coastal warning (if country is land)
            // or proximity message (if country is water)
            double landdistance = 0.0;
            guess = m_LatLonCountryMap->FindClosestToLatLon (lat_value, lon_value, 5.0, landdistance);
            if (guess) {
                id->SetClosestFull(guess->GetCountry());
                id->SetClosestCountry(guess->GetLevel0());
                id->SetClosestProvince(guess->GetLevel1());
                id->SetLandDistance(m_LatLonCountryMap->AdjustAndRoundDistance (landdistance));
                if (NStr::EqualNocase(country, id->GetClosestCountry()) 
                    && (NStr::IsBlank(province) || NStr::EqualNocase(province, guess->GetLevel1()))) {
                    goodmatch = true;
                }
            }
        } else {
            // may be coastal inlet, area of data insufficiency
            double landdistance = 0.0;
            guess = m_LatLonCountryMap->FindClosestToLatLon (lat_value, lon_value, 5.0, landdistance);
            if (guess) {
                id->SetClosestFull(guess->GetCountry());
                id->SetClosestCountry(guess->GetLevel0());
                id->SetClosestProvince(guess->GetLevel1());
                id->SetLandDistance(m_LatLonCountryMap->AdjustAndRoundDistance (landdistance));
                if (NStr::EqualNocase(country, id->GetClosestCountry())
                     && (NStr::IsBlank(province) || NStr::EqualNocase(province, guess->GetLevel1()))) {
                    goodmatch = true;
                }
            }

            double waterdistance = 0.0;
            guess = m_LatLonWaterMap->FindClosestToLatLon (lat_value, lon_value, 5.0, waterdistance);
            if (guess) {
                id->SetClosestWater(guess->GetLevel0());
                id->SetWaterDistance(m_LatLonWaterMap->AdjustAndRoundDistance (waterdistance));
                if (NStr::EqualNocase(country, id->GetClosestWater())) {
                    goodmatch = true;
                }
            }
        }
    }

    // if guess is not the provided country or province, calculate distance to claimed country
    if (!goodmatch) {
        double distance = 0.0;
        guess = m_LatLonCountryMap->IsNearLatLon (lat_value, lon_value, 5.0, distance, country, province);
        if (guess) {
            id->SetClaimedFull(guess->GetCountry());
            id->SetClaimedDistance(m_LatLonCountryMap->AdjustAndRoundDistance (distance));
        } else if (NStr::IsBlank(province)) {
            guess = m_LatLonWaterMap->IsNearLatLon (lat_value, lon_value, 5.0, distance, country, province);
            if (guess) {
                id->SetClaimedFull(guess->GetCountry());
                id->SetClaimedDistance(m_LatLonWaterMap->AdjustAndRoundDistance (distance));
            }
        }
    }

    return id;
}


CLatLonCountryId::TClassificationFlags CValidError_imp::x_ClassifyLatLonId(CLatLonCountryId *id, string country, string province)
{
    CLatLonCountryId::TClassificationFlags rval = 0;

    if (!id) {
        return 0;
    }

    // compare guesses or closest regions to indicated country and province
    if (!NStr::IsBlank(id->GetGuessCountry())) {
        // if top level countries match
        if (NStr::EqualNocase(country, id->GetGuessCountry())) {
            rval |= CLatLonCountryId::fCountryMatch;
            // if both are empty, still call it a match
            if (NStr::EqualNocase(province, id->GetGuessProvince())) {
                rval |= CLatLonCountryId::fProvinceMatch;
            }
        }
        // if they don't match, are they closest?
        if (!(rval & CLatLonCountryId::fCountryMatch)) {
            if (NStr::EqualNocase(country, id->GetClosestCountry())) {
                rval |= CLatLonCountryId::fCountryClosest;
                if (NStr::EqualNocase(province, id->GetClosestProvince())) {
                    rval |= CLatLonCountryId::fProvinceClosest;
                }
            }
        } else if (!(rval & CLatLonCountryId::fProvinceMatch) && !NStr::IsBlank(province)) {
            if (NStr::EqualNocase (province, id->GetClosestProvince())) {
                rval |= CLatLonCountryId::fProvinceClosest;
            }
        }
    }

    if (!NStr::IsBlank(id->GetGuessWater())) {
        // was the non-approved body of water correctly indicated?
        if (NStr::EqualNocase(country, id->GetGuessWater())) {
            rval |= CLatLonCountryId::fWaterMatch;
        } else if (NStr::EqualNocase(country, id->GetClosestWater())) {
            rval |= CLatLonCountryId::fWaterClosest;
        }
    }

    if (!NStr::IsBlank(id->GetClosestCountry()) && NStr::EqualNocase(country, id->GetClosestCountry())) {
        if (NStr::IsBlank(id->GetGuessCountry()) && NStr::IsBlank(id->GetGuessWater())) {
            rval |= CLatLonCountryId::fCountryMatch;
            id->SetGuessCountry(id->GetClosestCountry());
            id->SetFullGuess(id->GetClosestCountry());
            if (!NStr::IsBlank(id->GetClosestProvince()) && NStr::EqualNocase(province, id->GetClosestProvince())) {
                rval |= CLatLonCountryId::fProvinceMatch;
                id->SetGuessProvince(id->GetClosestProvince());
                id->SetFullGuess(id->GetClosestFull());
            }
        } else {
            rval |= CLatLonCountryId::fCountryClosest;
            if (!NStr::IsBlank(id->GetClosestProvince()) && NStr::EqualNocase(province, id->GetClosestProvince())) {
                rval |= CLatLonCountryId::fProvinceClosest;
            }
        }
    }
    return rval;
}




void CValidError_imp::ValidateLatLonCountry
(string countryname,
 string lat_lon,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    if (NStr::IsBlank(countryname) || NStr::IsBlank(lat_lon)) {
        return;
    }

    // only do these checks if the latlon format is good
    bool format_correct, lat_in_range, lon_in_range;
    double lat_value = 0.0, lon_value = 0.0;
    s_IsCorrectLatLonFormat (lat_lon, format_correct, 
                               lat_in_range, lon_in_range,
                               lat_value, lon_value);
    if (!format_correct) {
        // may have comma and then altitude, so just get lat_lon component */
        size_t pos = NStr::Find(lat_lon, ",", 0, string::npos, NStr::eLast);
        if (pos != string::npos) {
            lat_lon = lat_lon.substr(0, pos);
            s_IsCorrectLatLonFormat (lat_lon, format_correct, 
                                       lat_in_range, lon_in_range,
                                       lat_value, lon_value);
        }
    }

    // reality checks
    if (!format_correct || !lat_in_range || !lon_in_range) {
        // incorrect lat_lon format should be reported elsewhere
        // incorrect latitude range should be reported elsewhere
        // incorrect longitude range should be reported elsewhere
        return;
    }
        
    // get rid of comments after semicolon or comma in country name
    size_t pos = NStr::Find(countryname, ";");
    if (pos != string::npos) {
         countryname = countryname.substr(0, pos);
        }
    pos = NStr::Find(countryname, ",");
    if (pos != string::npos) {
         countryname = countryname.substr(0, pos);
        }

    string country = countryname;
    string province = "";
    pos = NStr::Find(country, ":");
    if (pos != string::npos) {
        // is the full string in the list?
        if (m_LatLonCountryMap->HaveLatLonForRegion(countryname)) {
            province = country.substr(pos + 1);
            NStr::TruncateSpacesInPlace(province, NStr::eTrunc_Both);
        }
        country = country.substr(0, pos);
        NStr::TruncateSpacesInPlace(country, NStr::eTrunc_Both);
    }
    if (NStr::IsBlank(country)) {
        return;
    }

    // known exceptions - don't even bother calculating any further
    if (NStr::EqualNocase (country, "Antarctica") && lat_value < -60.0) {
        return;
    }

    if (!m_LatLonCountryMap->HaveLatLonForRegion(country)) {
        if (! m_LatLonWaterMap->HaveLatLonForRegion(country)) {
            // report unrecognized country elsewhere
            return;
        } else {
            // continue to look for nearby country for proximity report
            // (do not return)
        }
    }

    CLatLonCountryId *id = x_CalculateLatLonId(lat_value, lon_value, country, province);

    CLatLonCountryId::TClassificationFlags flags = x_ClassifyLatLonId(id, country, province);
    double neardist = 0.0;
    CLatLonCountryMap::TLatLonAdjustFlags adjustment = CLatLonCountryMap::fNone;
    CLatLonCountryId::TClassificationFlags adjusted_flags = 0;
    if (!flags && !m_LatLonCountryMap->IsNearLatLon(lat_value, lon_value, 20.0, neardist, country)
        && !m_LatLonWaterMap->IsNearLatLon(lat_value, lon_value, 20.0, neardist, country)) {
        CLatLonCountryId *adjust_id = x_CalculateLatLonId(lon_value, lat_value, country, province);
        adjusted_flags = x_ClassifyLatLonId(adjust_id, country, province);
        if (adjusted_flags) {
            delete id;
            id = adjust_id;
            flags = adjusted_flags;
            adjustment = CLatLonCountryMap::fFlip;
        } else {
            if (adjust_id) {
                delete adjust_id;
            }
            adjust_id = x_CalculateLatLonId(-lat_value, lon_value, country, province);
            adjusted_flags = x_ClassifyLatLonId(adjust_id, country, province);
            if (adjusted_flags) {
                delete id;
                id = adjust_id;
                flags = adjusted_flags;
                adjustment = CLatLonCountryMap::fNegateLat;
            } else {
                if (adjust_id) {
                    delete adjust_id;
                }
                adjust_id = x_CalculateLatLonId(lat_value, -lon_value, country, province);
                adjusted_flags = x_ClassifyLatLonId(adjust_id, country, province);
                if (adjusted_flags) {
                    delete id;
                    id = adjust_id;
                    flags = adjusted_flags;
                    adjustment = CLatLonCountryMap::fNegateLon;
                } else {
                    if (adjust_id) {
                        delete adjust_id;
                    }
                }
            }
        }
    }

    if (adjustment != CLatLonCountryMap::fNone) {
        if (adjustment == CLatLonCountryMap::fFlip) {
              PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_LatLonValue, 
                    "Latitude and longitude values appear to be exchanged",
                            obj, ctx);
        } else if (adjustment == CLatLonCountryMap::fNegateLat) {
              if (lat_value < 0.0) {
                    PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_LatLonValue, 
                        "Latitude should be set to N (northern hemisphere)",
                        obj, ctx);
              } else {
                    PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_LatLonValue, 
                        "Latitude should be set to S (southern hemisphere)",
                        obj, ctx);
              }
        } else if (adjustment == CLatLonCountryMap::fNegateLon) {
              if (lon_value < 0.0) {
                    PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_LatLonValue, 
                                  "Longitude should be set to E (eastern hemisphere)",
                                  obj, ctx);
              } else {
                    PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_LatLonValue, 
                          "Longitude should be set to W (western hemisphere)",
                                  obj, ctx);
              }
        }
    } else if ((flags & CLatLonCountryId::fCountryMatch) && (flags & CLatLonCountryId::fProvinceMatch)) {
        // success!  nothing to report
    } else if (flags & CLatLonCountryId::fWaterMatch) {
        // success!  nothing to report
    } else if (flags & CLatLonCountryId::fCountryMatch && NStr::IsBlank(province)) {
        if (!IsIndexerVersion()) {
            PostObjErr (eDiag_Info, eErr_SEQ_DESCR_LatLonState,
                        "Lat_lon " + lat_lon + " is in " + id->GetFullGuess()
                        + " (more specific than " + country + ")",
                              obj, ctx);
        }
    } else if (!NStr::IsBlank(id->GetGuessWater())) {
        if (flags & (CLatLonCountryId::fCountryClosest | CLatLonCountryId::fProvinceClosest)) {
            if (flags & CLatLonCountryId::fProvinceClosest) {
                if (id->GetLandDistance() < 22) {
                    if (! IsIndexerVersion()) {
                        // acceptable for GenBank, but inform submitters 
                        PostObjErr (eDiag_Info, eErr_SEQ_DESCR_LatLonOffshore, 
                                    "Lat_lon '" + lat_lon + "' is closest to designated subregion '" + countryname
                                    + "' at distance " + NStr::IntToString(id->GetLandDistance()) + " km, but in water '"
                                    + id->GetGuessWater() + "'",
                                    obj, ctx);
                    }
                } else {
                    PostObjErr (eDiag_Info, eErr_SEQ_DESCR_LatLonWater, 
                                "Lat_lon '" + lat_lon + "' is closest to designated subregion '" + countryname
                                + "' at distance " + NStr::IntToString(id->GetLandDistance()) + " km, but in water '"
                                + id->GetGuessWater() + "'",
                                obj, ctx);
                }
            } else if (flags & CLatLonCountryId::fCountryClosest) {
                if (!NStr::IsBlank(id->GetClaimedFull())) {
                    PostObjErr (eDiag_Info, eErr_SEQ_DESCR_LatLonWater, 
                                "Lat_lon '" + lat_lon + "' is closest to another subregion '" + id->GetClosestFull()
                                + "' at distance " + NStr::IntToString(id->GetLandDistance()) + " km, but in water '"
                                + id->GetGuessWater() + "' - claimed region '" + id->GetClaimedFull() + "' is at distance "
                                + NStr::IntToString(id->GetClaimedDistance()) + " km",
                                obj, ctx);
                } else {
                    PostObjErr (eDiag_Info, eErr_SEQ_DESCR_LatLonWater, 
                                "Lat_lon '" + lat_lon + "' is closest to another subregion '" + id->GetClosestFull()
                                + "' at distance " + NStr::IntToString(id->GetLandDistance()) + " km, but in water '"
                                + id->GetGuessWater() + "'",
                                obj, ctx);
                }
            }
        } else if (neardist > 0.0) { 
            PostObjErr (eDiag_Info, eErr_SEQ_DESCR_LatLonWater, 
                        "Lat_lon '" + lat_lon + "' is in water '" + id->GetGuessWater() + "', '"
                        + countryname + "' is " + NStr::IntToString(m_LatLonCountryMap->AdjustAndRoundDistance(neardist)) + " km away",
                        obj, ctx);
        } else {
            PostObjErr (eDiag_Info, eErr_SEQ_DESCR_LatLonWater, 
                        "Lat_lon '" + lat_lon + "' is in water '" + id->GetGuessWater() + "'",
                        obj, ctx);
        }
    } else if (!NStr::IsBlank(id->GetGuessCountry())) {
        if (NStr::IsBlank(id->GetClaimedFull())) {
            PostObjErr (eDiag_Info, eErr_SEQ_DESCR_LatLonCountry, 
                        "Lat_lon '" + lat_lon + "' maps to '" + id->GetFullGuess() + "' instead of '"
                        + countryname + "'",
                        obj, ctx);
        } else {
            if (NStr::IsBlank(province)) {
                PostObjErr (eDiag_Info, eErr_SEQ_DESCR_LatLonCountry, 
                            "Lat_lon '" + lat_lon + "' maps to '" + id->GetFullGuess() + "' instead of '"
                            + country + "' - claimed region '" + id->GetClaimedFull() 
                            + "' is at distance " + NStr::IntToString(id->GetClaimedDistance()) + " km",
                            obj, ctx);
            } else {
                PostObjErr (eDiag_Info, eErr_SEQ_DESCR_LatLonCountry, 
                            "Lat_lon '" + lat_lon + "' maps to '" + id->GetFullGuess() + "' instead of '"
                            + countryname + "' - claimed region '" + id->GetClaimedFull() 
                            + "' is at distance " + NStr::IntToString(id->GetClaimedDistance()) + " km",
                            obj, ctx);
            }
        }
    } else if (!NStr::IsBlank(id->GetClosestCountry())) {
        PostObjErr (eDiag_Info, eErr_SEQ_DESCR_LatLonCountry, 
                    "Lat_lon '" + lat_lon + "' is closest to '" + id->GetClosestCountry() + "' instead of '"
                    + countryname + "'",
                    obj, ctx);
    } else if (!NStr::IsBlank(id->GetClosestWater())) {
        PostObjErr (eDiag_Info, eErr_SEQ_DESCR_LatLonWater, 
                    "Lat_lon '" + lat_lon + "' is closest to '" + id->GetClosestWater() + "' instead of '"
                    + countryname + "'",
                    obj, ctx);
    } else {
        PostObjErr (eDiag_Info, eErr_SEQ_DESCR_LatLonCountry, 
                    "Unable to determine mapping for lat_lon '" + lat_lon + "' and country '" + countryname + "'",
                    obj, ctx);
    }


    delete id;

}


void CValidError_imp::ValidateBioSource
(const CBioSource&    bsrc,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    if ( !bsrc.CanGetOrg() ) {
        PostObjErr(eDiag_Error, eErr_SEQ_DESCR_NoOrgFound,
            "No organism has been applied to this Bioseq.  Other qualifiers may exist.", obj, ctx);
        return;
    }

    const COrg_ref& orgref = bsrc.GetOrg();
  
    // validate legal locations.
    if ( bsrc.GetGenome() == CBioSource::eGenome_transposon  ||
         bsrc.GetGenome() == CBioSource::eGenome_insertion_seq ) {
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

    bool isViral = false, isAnimal = false, isPlant = false, isBacteria = false, isArchaea = false, isFungal = false;
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
        }
    }

    int chrom_count = 0;
    bool chrom_conflict = false;
    int country_count = 0, lat_lon_count = 0;
    int fwd_primer_seq_count = 0, rev_primer_seq_count = 0;
    int fwd_primer_name_count = 0, rev_primer_name_count = 0;
    CPCRSetList pcr_set_list;
    const CSubSource *chromosome = 0;
    string countryname;
    string lat_lon;
    double lat_value = 0.0, lon_value = 0.0;
    bool germline = false;
    bool rearranged = false;
    bool transgenic = false;
    bool env_sample = false;
    bool metagenomic = false;
    bool sex = false;
    bool mating_type = false;
    bool iso_source = false;

    FOR_EACH_SUBSOURCE_ON_BIOSOURCE (ssit, bsrc) {
        ValidateSubSource (**ssit, obj, ctx);
        if (!(*ssit)->IsSetSubtype()) {
            continue;
        }

        int subtype = (*ssit)->GetSubtype();
        switch ( subtype ) {
            
        case CSubSource::eSubtype_country:
            ++country_count;
            if (!NStr::IsBlank (countryname)) {
                PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadCountryCode, 
                            "Multiple country names on BioSource", obj, ctx);
            }
            countryname = (**ssit).GetName();
            break;

        case CSubSource::eSubtype_lat_lon:
            if ((*ssit)->IsSetName()) {
                lat_lon = (*ssit)->GetName();
                bool format_correct = false, lat_in_range = false, lon_in_range = false;
                s_IsCorrectLatLonFormat (lat_lon, format_correct, 
                         lat_in_range, lon_in_range,
                         lat_value, lon_value);
            }

            ++lat_lon_count;
            if (lat_lon_count > 1) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_LatLonProblem, 
                           "Multiple lat_lon on BioSource", obj, ctx);
            }
            break;
            
        case CSubSource::eSubtype_chromosome:
            ++chrom_count;
            if ( chromosome != 0 ) {
                if ( NStr::CompareNocase((**ssit).GetName(), chromosome->GetName()) != 0) {
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
            ++fwd_primer_name_count;
            break;

        case CSubSource::eSubtype_rev_primer_name:
            if ((*ssit)->IsSetName()) {
                pcr_set_list.AddRevName((*ssit)->GetName());
            }
            ++rev_primer_name_count;
            break;
            
        case CSubSource::eSubtype_fwd_primer_seq:
            if ((*ssit)->IsSetName()) {
                pcr_set_list.AddFwdSeq((*ssit)->GetName());
            }
            ++fwd_primer_seq_count;
            break;

        case CSubSource::eSubtype_rev_primer_seq:
            if ((*ssit)->IsSetName()) {
                pcr_set_list.AddRevSeq((*ssit)->GetName());
            }
            ++rev_primer_seq_count;
            break;

        case CSubSource::eSubtype_transposon_name:
        case CSubSource::eSubtype_insertion_seq_name:
            break;
            
        case 0:
            break;
            
        case CSubSource::eSubtype_other:
            break;

        case CSubSource::eSubtype_germline:
            germline = true;
            break;

        case CSubSource::eSubtype_rearranged:
            rearranged = true;
            break;

        case CSubSource::eSubtype_transgenic:
            transgenic = true;
            break;

        case CSubSource::eSubtype_metagenomic:
            metagenomic = true;
            break;

        case CSubSource::eSubtype_environmental_sample:
            env_sample = true;
            break;

        case CSubSource::eSubtype_isolation_source:
            iso_source = true;
            break;

        case CSubSource::eSubtype_sex:
            sex = true;
            if (isAnimal || isPlant) {
                // always use /sex, do not check values at this time
            } else if (isViral) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency,
                    "Virus has unexpected sex qualifier", obj, ctx);
            } else if (isBacteria || isArchaea || isFungal) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency,
                    "Unexpected use of /sex qualifier", obj, ctx);
            } else if (s_IsValidSexQualifierValue((*ssit)->GetName())) {
                // otherwise values are restricted to specific list
            } else {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency,
                    "Unexpected use of /sex qualifier", obj, ctx);
            }

            break;

        case CSubSource::eSubtype_mating_type:
            mating_type = true;
            if (isAnimal || isPlant || isViral) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency,
                    "Unexpected use of /mating_type qualifier", obj, ctx);
            } else if (s_IsValidSexQualifierValue((*ssit)->GetName())) {
                // complain if one of the values that should go in /sex
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency,
                    "Unexpected use of /mating_type qualifier", obj, ctx);
            }
            break;

        case CSubSource::eSubtype_plasmid_name:
            if (!bsrc.IsSetGenome() || bsrc.GetGenome() != CBioSource::eGenome_plasmid) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency,
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
                    CBioSource::EGenome genome_from_name = CBioSource::GetGenomeByOrganelle (subname, NStr::eCase,  false);
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
                            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency,
                                "Plastid name subsource " + val_name + " but not " + val_name + " location", obj, ctx);
                        }
                    } else {
                        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency,
                            "Plastid name subsource contains unrecognized value", obj, ctx);
                    }
                }
            }
            break;
        
        case CSubSource::eSubtype_cell_line:
            if (isViral) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency, 
                    "Virus has unexpected cell_line qualifier", obj, ctx);
            }
            break;

        case CSubSource::eSubtype_cell_type:
            if (isViral) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency, 
                    "Virus has unexpected cell_type qualifier", obj, ctx);
            }
            break;

        case CSubSource::eSubtype_tissue_type:
            if (isViral) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency, 
                    "Virus has unexpected tissue_type qualifier", obj, ctx);
            }
            break;

        case CSubSource::eSubtype_frequency:
            break;

        case CSubSource::eSubtype_collection_date:
            break;

        default:
            break;
        }
    }
    if ( germline  &&  rearranged ) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency,
            "Germline and rearranged should not both be present", obj, ctx);
    }
    if (transgenic && env_sample) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency,
            "Transgenic and environmental sample should not both be present", obj, ctx);
    }
    if (metagenomic && (! env_sample)) {
        PostObjErr(eDiag_Critical, eErr_SEQ_DESCR_BioSourceInconsistency, 
            "Metagenomic should also have environmental sample annotated", obj, ctx);
    }
    if (sex && mating_type) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency, 
             "Sex and mating type should not both be present", obj, ctx);
    }

    if ( chrom_count > 1 ) {
        string msg = 
            chrom_conflict ? "Multiple conflicting chromosome qualifiers" :
                             "Multiple identical chromosome qualifiers";
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleChromosomes, msg, obj, ctx);
    }

    if (country_count > 1) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleSourceQualifiers, 
                   "Multiple country qualifiers present", obj, ctx);
    }
    if (lat_lon_count > 1) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleSourceQualifiers, 
                   "Multiple lat_lon qualifiers present", obj, ctx);
    }
    if (fwd_primer_seq_count > 1) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleSourceQualifiers, 
                   "Multiple fwd_primer_seq qualifiers present", obj, ctx);
    }
    if (rev_primer_seq_count > 1) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleSourceQualifiers, 
                   "Multiple rev_primer_seq qualifiers present", obj, ctx);
    }
    if (fwd_primer_name_count > 1) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleSourceQualifiers, 
                   "Multiple fwd_primer_name qualifiers present", obj, ctx);
    }
    if (rev_primer_name_count > 1) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleSourceQualifiers, 
                   "Multiple rev_primer_name qualifiers present", obj, ctx);
    }

    if ((fwd_primer_seq_count > 0 && rev_primer_seq_count == 0)
        || (fwd_primer_seq_count == 0 && rev_primer_seq_count > 0)) {
        if (fwd_primer_name_count == 0 && rev_primer_name_count == 0) {
            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadPCRPrimerSequence, 
                       "PCR primer does not have both sequences", obj, ctx);
        }
    }

    if (!pcr_set_list.AreSetsUnique()) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_DuplicatePCRPrimerSequence,
                    "PCR primer sequence has duplicates", obj, ctx);
    }

      // check that country and lat_lon are compatible
    ValidateLatLonCountry(countryname, lat_lon, obj, ctx);

      // validates orgref in the context of lineage
    if ( !orgref.IsSetOrgname()  ||
         !orgref.GetOrgname().IsSetLineage()  ||
         NStr::IsBlank(orgref.GetOrgname().GetLineage())) {

        if (IsIndexerVersion()) { 
            EDiagSev sev = eDiag_Error;

            if (IsRefSeq()) {
                FOR_EACH_DBXREF_ON_ORGREF(it, orgref) {
                    if ((*it)->IsSetDb() && NStr::EqualNocase((*it)->GetDb(), "taxon")) {
                        sev = eDiag_Critical;
                        break;
                    }
                }
            }
            if (IsEmbl() || IsDdbj()) {
                sev = eDiag_Warning;
            }
                PostObjErr(sev, eErr_SEQ_DESCR_MissingLineage, 
                         "No lineage for this BioSource.", obj, ctx);
        }
    } else {
        const COrgName& orgname = orgref.GetOrgname();
        const string& lineage = orgname.GetLineage();
        if ( bsrc.GetGenome() == CBioSource::eGenome_kinetoplast ) {
            if ( lineage.find("Kinetoplastida") == string::npos ) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrganelle, 
                         "Only Kinetoplastida have kinetoplasts", obj, ctx);
            }
        } else if ( bsrc.GetGenome() == CBioSource::eGenome_nucleomorph ) {
            if ( lineage.find("Chlorarachniophyceae") == string::npos  &&
                lineage.find("Cryptophyta") == string::npos ) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrganelle, 
                    "Only Chlorarachniophyceae and Cryptophyta have nucleomorphs", obj, ctx);
            }
    } else if ( bsrc.GetGenome() == CBioSource::eGenome_macronuclear) {
            if ( lineage.find("Ciliophora") == string::npos ) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrganelle, 
                    "Only Ciliophora have macronuclear locations", obj, ctx);
            }
        }

        if (orgname.IsSetDiv()) {
            const string& div = orgname.GetDiv();
            if (NStr::EqualCase(div, "BCT")
                && bsrc.GetGenome() != CBioSource::eGenome_unknown
                && bsrc.GetGenome() != CBioSource::eGenome_genomic
                && bsrc.GetGenome() != CBioSource::eGenome_plasmid
                && bsrc.GetGenome() != CBioSource::eGenome_chromosome) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency, 
                           "Bacterial source should not have organelle location",
                           obj, ctx);
            } else if (NStr::EqualCase(div, "ENV") && !env_sample) {
                PostObjErr(eDiag_Error, eErr_SEQ_DESCR_BioSourceInconsistency, 
                           "BioSource with ENV division is missing environmental sample subsource",
                           obj, ctx);
            }
        }

        if (!metagenomic && NStr::FindNoCase(lineage, "metagenomes") != string::npos) {
            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency, 
                       "If metagenomes appears in lineage, BioSource should have metagenomic qualifier",
                       obj, ctx);
        }

    }

    // look for conflicts in orgmods
    bool specific_host = false;
    FOR_EACH_ORGMOD_ON_ORGREF (it, orgref) {
        if (!(*it)->IsSetSubtype()) {
            continue;
        }
        COrgMod::TSubtype subtype = (*it)->GetSubtype();

        if (subtype == COrgMod::eSubtype_nat_host) {
            specific_host = true;
        } else if (subtype == COrgMod::eSubtype_strain) {
            if (env_sample) {
                PostObjErr(eDiag_Error, eErr_SEQ_DESCR_BioSourceInconsistency, "Strain should not be present in an environmental sample",
                           obj, ctx);
            }
        } else if (subtype == COrgMod::eSubtype_metagenome_source) {
            if (!metagenomic) {
                PostObjErr(eDiag_Error, eErr_SEQ_DESCR_BioSourceInconsistency, "Metagenome source should also have metagenomic qualifier",
                           obj, ctx);
            }
        }
    }
    if (env_sample && !iso_source && !specific_host) {
        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency, 
                   "Environmental sample should also have isolation source or specific host annotated",
                   obj, ctx);
    }

    ValidateOrgRef (orgref, obj, ctx);
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

    int subtype = subsrc.GetSubtype();
    switch ( subtype ) {
        
    case CSubSource::eSubtype_country:
        {
            string countryname = subsrc.GetName();
            bool is_miscapitalized = false;
            if ( CCountries::IsValid(countryname, is_miscapitalized) ) {
                if (is_miscapitalized) {
                    PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadCountryCapitalization, 
                                "Bad country capitalization [" + countryname + "]",
                                obj, ctx);
                }
            } else {
                if ( countryname.empty() ) {
                    countryname = "?";
                }
                if ( CCountries::WasValid(countryname) ) {
                    PostObjErr(eDiag_Info, eErr_SEQ_DESCR_ReplacedCountryCode,
                            "Replaced country name [" + countryname + "]", obj, ctx);
                } else {
                    PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadCountryCode,
                            "Bad country name [" + countryname + "]", obj, ctx);
                }
            }
        }
        break;

    case CSubSource::eSubtype_lat_lon:
        if (subsrc.IsSetName()) {
            bool format_correct = false, lat_in_range = false, lon_in_range = false;
            double lat_value = 0.0, lon_value = 0.0;
            string lat_lon = subsrc.GetName();
            s_IsCorrectLatLonFormat (lat_lon, format_correct, 
                                     lat_in_range, lon_in_range,
                                     lat_value, lon_value);
            if (!format_correct) {
                size_t pos = NStr::Find(lat_lon, ",");
                if (pos != string::npos) {
                    s_IsCorrectLatLonFormat (lat_lon.substr(0, pos), format_correct, lat_in_range, lon_in_range, lat_value, lon_value);
                    if (format_correct) {
                        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_LatLonFormat, 
                                   "lat_lon format has extra text after correct dd.dd N|S ddd.dd E|W format",
                                   obj, ctx);
                    }
                }
            }

            if (!format_correct) {
                PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_LatLonFormat, 
                            "lat_lon format is incorrect - should be dd.dd N|S ddd.dd E|W",
                            obj, ctx);
            } else {
                if (!lat_in_range) {
                    PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_LatLonRange, 
                                "latitude value is out of range - should be between 90.00 N and 90.00 S",
                                obj, ctx);
                }
                if (!lon_in_range) {
                    PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_LatLonRange, 
                                "longitude value is out of range - should be between 180.00 E and 180.00 W",
                                obj, ctx);
                }
            }
        }
        break;
        
    case CSubSource::eSubtype_chromosome:
        break;

    case CSubSource::eSubtype_fwd_primer_name:
        if (subsrc.IsSetName()) {
            string name = subsrc.GetName();
            char bad_ch;
            if (name.length() > 10
                && s_IsValidPrimerSequence(name, bad_ch)) {
                PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadPCRPrimerName, 
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
                && s_IsValidPrimerSequence(name, bad_ch)) {
                PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadPCRPrimerName, 
                            "PCR primer name appears to be a sequence",
                            obj, ctx);
            }
        }
        break;
        
    case CSubSource::eSubtype_fwd_primer_seq:
        {
            char bad_ch;
            if (!subsrc.IsSetName() || !s_IsValidPrimerSequence(subsrc.GetName(), bad_ch)) {
                if (bad_ch < ' ' || bad_ch > '~') {
                    bad_ch = '?';
                }

                string msg = "PCR forward primer sequence format is incorrect, first bad character is '";
                msg += bad_ch;
                msg += "'";
                PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadPCRPrimerSequence,
                            msg, obj, ctx);
            }
        }
        break;

    case CSubSource::eSubtype_rev_primer_seq:
        {
            char bad_ch;
            if (!subsrc.IsSetName() || !s_IsValidPrimerSequence(subsrc.GetName(), bad_ch)) {
                if (bad_ch < ' ' || bad_ch > '~') {
                    bad_ch = '?';
                }

                string msg = "PCR reverse primer sequence format is incorrect, first bad character is '";
                msg += bad_ch;
                msg += "'";
                PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadPCRPrimerSequence,
                            msg, obj, ctx);
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

    case CSubSource::eSubtype_plasmid_name:
        break;

    case CSubSource::eSubtype_plastid_name:
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
                PostObjErr(eDiag_Info, eErr_SEQ_DESCR_BioSourceInconsistency, 
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
                    while (sit != frequency.end() && isdigit (*sit)) {
                        ++sit;
                    }
                    if (sit != frequency.end()) {
                        bad_frequency = true;
                    }
                } else {
                    bad_frequency = true;
                }
                if (bad_frequency) {
                    PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency,
                               "bad frequency qualifier value " + frequency,
                               obj, ctx);
                }
            }
        }
        break;
    case CSubSource::eSubtype_collection_date:
        if (!subsrc.IsSetName()) {
            PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadCollectionDate, 
                        "Collection_date format is not in DD-Mmm-YYYY format",
                        obj, ctx);
        } else {
            try {
                CRef<CDate> coll_date = CSubSource::DateFromCollectionDate (subsrc.GetName());

                struct tm *tm;
                time_t t;

                time(&t);
                tm = localtime(&t);

                if (coll_date->GetStd().GetYear() > tm->tm_year + 1900) {
                    PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadCollectionDate, 
                               "Collection_date is in the future",
                               obj, ctx);
                } else if (coll_date->GetStd().GetYear() == tm->tm_year + 1900
                           && coll_date->GetStd().IsSetMonth()) {
                    if (coll_date->GetStd().GetMonth() > tm->tm_mon + 1) {
                        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadCollectionDate, 
                                   "Collection_date is in the future",
                                   obj, ctx);
                    } else if (coll_date->GetStd().GetMonth() == tm->tm_mon + 1
                               && coll_date->GetStd().IsSetDay()) {
                        if (coll_date->GetStd().GetDay() > tm->tm_mday) {
                            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadCollectionDate, 
                                       "Collection_date is in the future",
                                       obj, ctx);
                        }
                    }
                }
            } catch (CException ) {
                PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadCollectionDate, 
                            "Collection_date format is not in DD-Mmm-YYYY format",
                            obj, ctx);
            }
        } 
        break;

    default:
        break;
    }

    if (subsrc.IsSetName()) {
        if (CSubSource::NeedsNoText(subtype)) {
            if (subsrc.IsSetName() && !NStr::IsBlank(subsrc.GetName())) {
                string subname = CSubSource::GetSubtypeName (subtype);
                if (subname.length() > 0) {
                    subname[0] = toupper(subname[0]);
                }
                NStr::ReplaceInPlace(subname, "-", "_");
                PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency, 
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


static bool s_FindWholeName (string taxname, string value)
{
    if (NStr::IsBlank(taxname) || NStr::IsBlank(value)) {
        return false;
    }
    size_t pos = NStr::Find (taxname, value);
    size_t value_len = value.length();
    while (pos != string::npos 
           && ((pos != 0 && isalpha (taxname.c_str()[pos - 1])
               || isalpha (taxname.c_str()[pos + value_len])))) {
        pos = NStr::Find(taxname, value, pos + value_len);
    }
    if (pos == string::npos) {
        return false;
    } else {
        return true;
    }
}


void CValidError_imp::ValidateOrgRef
(const COrg_ref&    orgref,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    // Organism must have a name.
    if ( (!orgref.IsSetTaxname()  ||  orgref.GetTaxname().empty())  &&
         (!orgref.IsSetCommon()   ||  orgref.GetCommon().empty()) ) {
          PostObjErr(eDiag_Error, eErr_SEQ_DESCR_NoOrgFound,
            "No organism name has been applied to this Bioseq.  Other qualifiers may exist.", obj, ctx);
    }

    string taxname = "";
    if (orgref.IsSetTaxname()) {
        taxname = orgref.GetTaxname();
        if (s_UnbalancedParentheses (taxname)) {
            PostObjErr(eDiag_Error, eErr_SEQ_DESCR_UnbalancedParentheses,
                       "Unbalanced parentheses in taxname '" + orgref.GetTaxname() + "'", obj, ctx);
        }
        if (ContainsSgml(taxname)) {
            PostObjErr(eDiag_Warning, eErr_GENERIC_SgmlPresentInText, 
                       "taxname " + taxname + " has SGML", 
                       obj, ctx);
        }
    }

    if ( orgref.IsSetDb() ) {
        ValidateDbxref(orgref.GetDb(), obj, true, ctx);
    }

    if ( IsRequireTaxonID() ) {
        bool found = false;
        FOR_EACH_DBXREF_ON_ORGREF (dbt, orgref) {
            if ( NStr::CompareNocase((*dbt)->GetDb(), "taxon") == 0 ) {
                found = true;
                break;
            }
        }
        if ( !found ) {
            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_NoTaxonID,
                "BioSource is missing taxon ID", obj, ctx);
        }
    }

    if ( !orgref.IsSetOrgname() ) {
        return;
    }
    const COrgName& orgname = orgref.GetOrgname();
    ValidateOrgName(orgname, obj, ctx);

    // Look for modifiers in taxname
    string taxname_search = taxname;
    // skip first two words
    size_t pos = NStr::Find (taxname_search, " ");
    if (pos == string::npos) {
        taxname_search = "";
    } else {
        taxname_search = taxname_search.substr(pos + 1);
        NStr::TruncateSpacesInPlace(taxname_search);
        pos = NStr::Find (taxname_search, " ");
        if (pos == string::npos) {
            taxname_search = "";
        } else {
            taxname_search = taxname_search.substr(pos + 1);
            NStr::TruncateSpacesInPlace(taxname_search);
        }
    }

    // determine if variety is present and in taxname - if so,
    // can ignore missing subspecies
    // also look for specimen-voucher (nat-host) if identical to taxname
    int num_bad_subspecies = 0;
    bool have_variety_in_taxname = false;
    FOR_EACH_ORGMOD_ON_ORGNAME (it, orgname) {
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
            if (!s_FindWholeName(taxname_search, subname)) {
                ++num_bad_subspecies;
            }
        } else if (subtype == COrgMod::eSubtype_variety) {
            if (s_FindWholeName(taxname_search, subname)) {
                have_variety_in_taxname = true;
            } else {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency,
                           orgmod_name + " value specified is not found in taxname",
                           obj, ctx);
            }
        } else if (subtype == COrgMod::eSubtype_forma
                   || subtype == COrgMod::eSubtype_forma_specialis) {
            if (!s_FindWholeName(taxname_search, subname)) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency,
                           orgmod_name + " value specified is not found in taxname",
                           obj, ctx);
            }
        } else if (subtype == COrgMod::eSubtype_nat_host) {
            if (NStr::EqualNocase(subname, taxname)) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrgMod,
                           "Specific host is identical to taxname",
                           obj, ctx);
            }
        } else if (subtype == COrgMod::eSubtype_common) {
            if (orgref.IsSetCommon() 
                && NStr::EqualNocase(subname, orgref.GetCommon())) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrgMod,
                           "OrgMod common is identical to Org-ref common",
                           obj, ctx);
            }
        }
    }
    if (!have_variety_in_taxname) {
        for (int i = 0; i < num_bad_subspecies; i++) {
            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency,
                       "Subspecies value specified is not found in taxname",
                       obj, ctx);
        }
    }

}


void CValidError_imp::ValidateOrgName
(const COrgName&    orgname,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    if ( orgname.IsSetMod() ) {
        bool has_strain = false;
        FOR_EACH_ORGMOD_ON_ORGNAME (omd_itr, orgname) {
            const COrgMod& omd = **omd_itr;
            int subtype = omd.GetSubtype();
            
            switch (subtype) {
                case 0:
                case 1:
                    PostObjErr(eDiag_Critical, eErr_SEQ_DESCR_BadOrgMod, 
                               "Unknown orgmod subtype " + NStr::IntToString(subtype), obj, ctx);
                    break;
                case COrgMod::eSubtype_strain:
                    if (has_strain) {
                        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrgMod, 
                                   "Multiple strain qualifiers on the same BioSource", obj, ctx);
                    }
                    has_strain = true;
                    break;
                case COrgMod::eSubtype_variety:
                    if ( (!orgname.IsSetDiv() || !NStr::EqualNocase( orgname.GetDiv(), "PLN" ))
                        && (!orgname.IsSetLineage() ||
                            (NStr::Find(orgname.GetLineage(), "Cyanobacteria") == string::npos
                            && NStr::Find(orgname.GetLineage(), "Myxogastria") == string::npos
                            && NStr::Find(orgname.GetLineage(), "Oomycetes") == string::npos))) {
                        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrgMod, 
                            "Orgmod variety should only be in plants, fungi, or cyanobacteria", 
                            obj, ctx);
                    }
                    break;
                case COrgMod::eSubtype_other:
                    ValidateSourceQualTags( omd.GetSubname(), obj, ctx);
                    break;
                case COrgMod::eSubtype_synonym:
                    if ((*omd_itr)->IsSetSubname() && !NStr::IsBlank((*omd_itr)->GetSubname())) {
                        const string& val = (*omd_itr)->GetSubname();

                        // look for synonym/gb_synonym duplication
                        FOR_EACH_ORGMOD_ON_ORGNAME (it2, orgname) {
                            if ((*it2)->IsSetSubtype() 
                                && (*it2)->GetSubtype() == COrgMod::eSubtype_gb_synonym
                                && (*it2)->IsSetSubname()
                                && NStr::EqualNocase(val, (*it2)->GetSubname())) {
                                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency, 
                                           "OrgMod synonym is identical to OrgMod gb_synonym",
                                           obj, ctx);
                            }
                        }
                    }
                    break;
                case COrgMod::eSubtype_bio_material:
                case COrgMod::eSubtype_culture_collection:
                case COrgMod::eSubtype_specimen_voucher:
                    ValidateOrgModVoucher (omd, obj, ctx);
                    break;

                default:
                    break;
            }
            if ( omd.IsSetSubname()) {
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

        // look for multiple source vouchers
        FOR_EACH_ORGMOD_ON_ORGNAME (omd_itr, orgname) {
            if(!(*omd_itr)->IsSetSubtype() || !(*omd_itr)->IsSetSubname()) {
                continue;
            }
            const COrgMod& omd = **omd_itr;
            int subtype = omd.GetSubtype();

            if (subtype != COrgMod::eSubtype_specimen_voucher
                && subtype != COrgMod::eSubtype_bio_material
                && subtype != COrgMod::eSubtype_culture_collection) {
                continue;
            }

            string inst1 = "", coll1 = "", id1 = "";
            if (!COrgMod::ParseStructuredVoucher(omd.GetSubname(), inst1, coll1, id1)) {
                continue;
            }
            if (NStr::EqualNocase(inst1, "personal")
                || NStr::EqualCase(coll1, "DNA")) {
                continue;
            }

            COrgName::TMod::const_iterator it_next = omd_itr;
            ++it_next;
            while (it_next != orgname.GetMod().end()) {
                if (!(*it_next)->IsSetSubtype() || !(*it_next)->IsSetSubname() || (*it_next)->GetSubtype() != subtype) {
                    ++it_next;
                    continue;
                }
                int subtype_next = (*it_next)->GetSubtype();
                if (subtype_next != COrgMod::eSubtype_specimen_voucher
                    && subtype_next != COrgMod::eSubtype_bio_material
                    && subtype_next != COrgMod::eSubtype_culture_collection) {
                    ++it_next;
                    continue;
                }
                string inst2 = "", coll2 = "", id2 = "";
                if (!COrgMod::ParseStructuredVoucher((*it_next)->GetSubname(), inst2, coll2, id2)
                    || NStr::IsBlank(inst2)
                    || NStr::EqualNocase(inst2, "personal")
                    || NStr::EqualCase(coll2, "DNA")
                    || !NStr::EqualNocase (inst1, inst2)) {
                    ++it_next;
                    continue;
                }
                // at this point, we have identified two vouchers 
                // with the same institution codes
                // that are not personal and not DNA collections
                if (!NStr::IsBlank(coll1) && !NStr::IsBlank(coll2) 
                    && NStr::EqualNocase(coll1, coll2)) {
                    PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_MultipleSourceVouchers, 
                                "Multiple vouchers with same institution:collection",
                                obj, ctx);
                } else {
                    PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_MultipleSourceVouchers, 
                                "Multiple vouchers with same institution",
                                obj, ctx);
                }
                ++it_next;
            }

        }

    }
}


bool CValidError_imp::IsOtherDNA(const CBioseq_Handle& bsh) const
{
    if ( bsh ) {
        CSeqdesc_CI sd(bsh, CSeqdesc::e_Molinfo);
        if ( sd ) {
            const CSeqdesc::TMolinfo& molinfo = sd->GetMolinfo();
            if ( molinfo.CanGetBiomol()  &&
                 molinfo.GetBiomol() == CMolInfo::eBiomol_other ) {
                return true;
            }
        }
    }
    return false;
}


static bool s_CompleteGenomeNeedsChromosome (const CBioSource& source)
{
    bool rval = false;

    if (!source.IsSetGenome() 
        || source.GetGenome() == CBioSource::eGenome_genomic
        || source.GetGenome() == CBioSource::eGenome_unknown) {
        bool is_viral = false;
        if (source.IsSetOrg()) {
            if (source.GetOrg().IsSetDivision() && NStr::Equal(source.GetOrg().GetDivision(), "PHG")) {
                is_viral = true;
            } else if (source.GetOrg().IsSetLineage()) {
                if (NStr::StartsWith(source.GetOrg().GetLineage(), "Viruses; ")
                    || NStr::StartsWith(source.GetOrg().GetLineage(), "Viroids; ")) {
                    is_viral = true;
                }
            }
        }
        rval = !is_viral;
    }
    return rval;
}


void CValidError_imp::ValidateBioSourceForSeq
(const CBioSource&    source,
 const CSerialObject& obj,
 const CSeq_entry    *ctx,
 const CBioseq_Handle& bsh)
{
    if ( source.IsSetIs_focus() ) {
        // skip proteins, segmented bioseqs, or segmented parts
        if ( !bsh.IsAa()  &&
            !(bsh.GetInst().GetRepr() == CSeq_inst::eRepr_seg)  &&
            !(GetAncestor(*(bsh.GetCompleteBioseq()), CBioseq_set::eClass_parts) != 0) ) {
            if ( !CFeat_CI(bsh, CSeqFeatData::e_Biosrc) ) {
                PostObjErr(eDiag_Error,
                    eErr_SEQ_DESCR_UnnecessaryBioSourceFocus,
                    "BioSource descriptor has focus, "
                    "but no BioSource feature", obj, ctx);
            }
        }
    }
    if ( source.CanGetOrigin()  &&  
         source.GetOrigin() == CBioSource::eOrigin_synthetic ) {
        if ( !IsOtherDNA(bsh) && !bsh.IsAa()) {
            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_InvalidForType,
                "Molinfo-biomol other should be used if "
                "Biosource-location is synthetic", obj, ctx);
        }
    }

      // check locations for HIV biosource
      if (bsh.IsSetInst() && bsh.GetInst().IsSetMol()
            && source.IsSetOrg() && source.GetOrg().IsSetTaxname() 
            && (NStr::EqualNocase(source.GetOrg().GetTaxname(), "Human immunodeficiency virus")
                || NStr::EqualNocase(source.GetOrg().GetTaxname(), "Human immunodeficiency virus 1")
                || NStr::EqualNocase(source.GetOrg().GetTaxname(), "Human immunodeficiency virus 2"))) {

            if (bsh.GetInst().GetMol() == CSeq_inst::eMol_dna) {
                  if (!source.IsSetGenome() || source.GetGenome() != CBioSource::eGenome_proviral) {
                        PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency, 
                                 "HIV with moltype DNA should be proviral", 
                               obj, ctx);
                  }
            } else if (bsh.GetInst().GetMol() == CSeq_inst::eMol_rna) {
                  CSeqdesc_CI mi(bsh, CSeqdesc::e_Molinfo);
                  if (mi && mi->GetMolinfo().IsSetBiomol()
                && mi->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_mRNA) {
                        PostObjErr (eDiag_Info, eErr_SEQ_DESCR_BioSourceInconsistency, 
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
            && s_CompleteGenomeNeedsChromosome (source)) {
            PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceNeedsChromosome, 
                       "Non-viral complete genome not labeled as chromosome",
                       obj, ctx);
        }
    }

    if (mi) {
        // look for synthetic/artificial
        bool is_synthetic_construct = IsSyntheticConstruct (source);
        bool is_artificial = IsArtificial(source);

        if (is_synthetic_construct) {
            if ((!mi->GetMolinfo().IsSetBiomol()
                 || mi->GetMolinfo().GetBiomol() != CMolInfo::eBiomol_other_genetic) && !bsh.IsAa()) {
                PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_InvalidForType, 
                            "synthetic construct should have other-genetic", 
                            obj, ctx);
            }
            if (!is_artificial) {
                PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_InvalidForType, 
                            "synthetic construct should have artificial origin", 
                            obj, ctx);
            }
        } else if (is_artificial) {
            PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_InvalidForType, 
                        "artificial origin should have other-genetic and synthetic construct", 
                        obj, ctx);
        }
        if (is_artificial) {
            if ((!mi->GetMolinfo().IsSetBiomol()
                 || mi->GetMolinfo().GetBiomol() != CMolInfo::eBiomol_other_genetic)
                 && !bsh.IsAa()) {
                PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_InvalidForType, 
                            "artificial origin should have other-genetic", 
                            obj, ctx);
            }
        }
    }


    // validate subsources in context

      FOR_EACH_SUBSOURCE_ON_BIOSOURCE (it, source) {
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
                        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency, 
                                   "cRNA note conflicts with molecule type",
                                   obj, ctx);
                    } else {
                        PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency, 
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

            // look at uncultured sequence length and required modifiers
            if (orgref.IsSetTaxname()) {
                  if (NStr::EqualNocase(orgref.GetTaxname(), "uncultured bacterium")
                      && bsh.GetBioseqLength() >= 10000) {
                      PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency, 
                                 "Uncultured bacterium sequence length is suspiciously high",
                                 obj, ctx);
                  }
                  if (NStr::StartsWith(orgref.GetTaxname(), "uncultured ", NStr::eNocase)) {
                      bool is_env_sample = false;
                      FOR_EACH_SUBSOURCE_ON_BIOSOURCE (it, source) {
                          if ((*it)->IsSetSubtype() && (*it)->GetSubtype() == CSubSource::eSubtype_environmental_sample) {
                              is_env_sample = true;
                              break;
                          }
                      }
                      if (!is_env_sample) {
                          PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency, 
                                     "Uncultured should also have /environmental_sample",
                                     obj, ctx);
                      }
                  }
            }

            if (mi) {                        
            const CMolInfo& molinfo = mi->GetMolinfo();
                  // look for conflicting cRNA notes on orgmod
                  FOR_EACH_ORGMOD_ON_ORGREF (it, orgref) {
                      if ((*it)->IsSetSubtype() 
                          && (*it)->GetSubtype() == COrgMod::eSubtype_other
                          && (*it)->IsSetSubname()
                          && NStr::EqualNocase((*it)->GetSubname(), "cRNA")) {
                          if (!molinfo.IsSetBiomol() 
                              || molinfo.GetBiomol() != CMolInfo::eBiomol_cRNA) {
                              PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency, 
                                         "cRNA note conflicts with molecule type",
                                         obj, ctx);
                          } else {
                              PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency, 
                                         "cRNA note redundant with molecule type",
                                         obj, ctx);
                          }
                      }
                  }


                  if (orgref.IsSetLineage()) {
                      const string& lineage = orgref.GetOrgname().GetLineage();

                      // look for incorrect DNA stage
                      if (molinfo.IsSetBiomol()  && molinfo.GetBiomol () == CMolInfo::eBiomol_genomic
                          && bsh.IsSetInst() && bsh.GetInst().IsSetMol() && bsh.GetInst().GetMol() == CSeq_inst::eMol_dna
                          && NStr::StartsWith(lineage, "Viruses; ")
                          && NStr::FindNoCase(lineage, "no DNA stage") != string::npos) {
                          PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency, 
                                   "Genomic DNA viral lineage indicates no DNA stage",
                                   obj, ctx);
                      }
                      if (NStr::FindNoCase (lineage, "negative-strand viruses") != string::npos) {
                          bool is_ambisense = false;
                          if (NStr::FindNoCase(lineage, "Arenavirus") != string::npos
                              || NStr::FindNoCase(lineage, "Arenaviridae") != string::npos
                              || NStr::FindNoCase(lineage, "Phlebovirus") != string::npos
                              || NStr::FindNoCase(lineage, "Tospovirus") != string::npos
                              || NStr::FindNoCase(lineage, "Tenuivirus") != string::npos) {
                              is_ambisense = true;
                          }

                          bool is_synthetic = false;
                          if (orgref.IsSetDivision() && NStr::EqualNocase(orgref.GetDivision(), "SYN")) {
                              is_synthetic = true;
                          } else if (source.IsSetOrigin()
                                     && (source.GetOrigin() == CBioSource::eOrigin_mut
                                         || source.GetOrigin() == CBioSource::eOrigin_artificial
                                         || source.GetOrigin() == CBioSource::eOrigin_synthetic)) {
                              is_synthetic = true;
                          }

                          bool has_cds = false;

                          CFeat_CI cds_ci(bsh, SAnnotSelector(CSeqFeatData::e_Cdregion));
                          while (cds_ci) {
                              has_cds = true;
                              if (cds_ci->GetLocation().GetStrand() == eNa_strand_minus) {
                                  if (!molinfo.IsSetBiomol() || molinfo.GetBiomol() != CMolInfo::eBiomol_genomic) {
                                      PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency, 
                                                  "Negative-strand virus with minus strand CDS should be genomic",
                                                  obj, ctx);
                                  }
                              } else {
                                  if (!is_synthetic && !is_ambisense
                                      && (!molinfo.IsSetBiomol() 
                                          || (molinfo.GetBiomol() != CMolInfo::eBiomol_cRNA
                                              && molinfo.GetBiomol() != CMolInfo::eBiomol_mRNA))) {
                                      PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency, 
                                                  "Negative-strand virus with plus strand CDS should be mRNA or cRNA",
                                                  obj, ctx);
                                  }
                              }
                              ++cds_ci;
                          }
                          if (!has_cds) {
                              CFeat_CI misc_ci(bsh, SAnnotSelector(CSeqFeatData::eSubtype_misc_feature));
                              while (misc_ci) {
                                  if (misc_ci->IsSetComment()
                                      && NStr::FindNoCase (misc_ci->GetComment(), "nonfunctional") != string::npos) {
                                      if (misc_ci->GetLocation().GetStrand() == eNa_strand_minus) {
                                          if (!molinfo.IsSetBiomol() || molinfo.GetBiomol() != CMolInfo::eBiomol_genomic) {
                                              PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency, 
                                                          "Negative-strand virus with nonfunctional minus strand misc_feature should be genomic",
                                                          obj, ctx);
                                          }
                                      } else {
                                          if (!is_synthetic && !is_ambisense
                                              && (!molinfo.IsSetBiomol() 
                                                  || (molinfo.GetBiomol() != CMolInfo::eBiomol_cRNA
                                                      && molinfo.GetBiomol() != CMolInfo::eBiomol_mRNA))) {
                                              PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BioSourceInconsistency, 
                                                          "Negative-strand virus with nonfunctional plus strand misc_feature should be mRNA or cRNA",
                                                          obj, ctx);
                                          }
                                      }
                                  }
                                  ++misc_ci;
                              }
                          }
                      }
            }
              }

        if (!bsh.IsAa() && orgref.IsSetLineage()) {
                    const string& lineage = orgref.GetOrgname().GetLineage();
            // look for viral taxonomic conflicts
            if ((NStr::Find(lineage, " ssRNA viruses; ") != string::npos
                 || NStr::Find(lineage, " ssRNA negative-strand viruses; ") != string::npos
                 || NStr::Find(lineage, " ssRNA positive-strand viruses, no DNA stage; ") != string::npos
                 || NStr::Find(lineage, " unassigned ssRNA viruses; ") != string::npos)
                && (!bsh.IsSetInst_Mol() || bsh.GetInst_Mol() != CSeq_inst::eMol_rna)) {
                PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_MolInfoConflictsWithBioSource, 
                                "Taxonomy indicates single-stranded RNA, sequence does not agree.",
                                    obj, ctx);
            }
            if (NStr::Find(lineage, " dsRNA viruses; ") != string::npos
                && (!bsh.IsSetInst_Mol() || bsh.GetInst_Mol() != CSeq_inst::eMol_rna)) {
                PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_MolInfoConflictsWithBioSource, 
                                "Taxonomy indicates double-stranded RNA, sequence does not agree.",
                                    obj, ctx);
            }
            if (NStr::Find(lineage, " ssDNA viruses; ") != string::npos
                && (!bsh.IsSetInst_Mol() || bsh.GetInst_Mol() != CSeq_inst::eMol_dna)) {
                PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_MolInfoConflictsWithBioSource, 
                                "Taxonomy indicates single-stranded DNA, sequence does not agree.",
                                    obj, ctx);
            }
            if (NStr::Find(lineage, " dsDNA viruses; ") != string::npos
                && (!bsh.IsSetInst_Mol() || bsh.GetInst_Mol() != CSeq_inst::eMol_dna)) {
                PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_MolInfoConflictsWithBioSource, 
                                "Taxonomy indicates double-stranded DNA, sequence does not agree.",
                                    obj, ctx);
            }
          }
      }
}


bool CValidError_imp::IsTransgenic(const CBioSource& bsrc)
{
    if (bsrc.CanGetSubtype()) {
        FOR_EACH_SUBSOURCE_ON_BIOSOURCE (sbs_itr, bsrc) {
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
};


void CValidError_imp::InitializeSourceQualTags() 
{
    m_SourceQualTags.reset(new CTextFsa(true));
    size_t size = sizeof(sm_SourceQualPrefixes) / sizeof(string);

    for (size_t i = 0; i < size; ++i ) {
        m_SourceQualTags->AddWord(sm_SourceQualPrefixes[i]);
    }

    m_SourceQualTags->Prime();
}


void CValidError_imp::ValidateSourceQualTags
(const string& str,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    if ( NStr::IsBlank(str) ) return;

    size_t str_len = str.length();

    int state = m_SourceQualTags->GetInitialState();
    
    for ( size_t i = 0; i < str_len; ++i ) {
        state = m_SourceQualTags->GetNextState(state, str[i]);
        if ( m_SourceQualTags->IsMatchFound(state) ) {
            string match = m_SourceQualTags->GetMatches(state)[0];
            if ( match.empty() ) {
                match = "?";
            }
            size_t match_len = match.length();

            bool okay = true;
            if ( (int)(i - match_len) >= 0 ) {
                char ch = str[i - match_len];
                if ( !isspace((unsigned char) ch) && ch != ';' ) {
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
            if ( okay ) {
                PostObjErr(eDiag_Warning, eErr_SEQ_DESCR_StructuredSourceNote,
                    "Source note has structured tag '" + match + "'", obj, ctx);
            }
        }
    }
}


void CValidError_imp::GatherSources
(const CSeq_entry& se, 
 vector<CConstRef<CSeqdesc> >& src_descs,
 vector<CConstRef<CSeq_entry> >& desc_ctxs,
 vector<CConstRef<CSeq_feat> >& src_feats)
{
    // get source descriptors
    FOR_EACH_DESCRIPTOR_ON_SEQENTRY (it, se) {
        if ((*it)->IsSource() && (*it)->GetSource().IsSetOrg()) {
            CConstRef<CSeqdesc> desc;
            desc.Reset(*it);
            src_descs.push_back(desc);
            CConstRef<CSeq_entry> r_se;
            r_se.Reset(&se);
            desc_ctxs.push_back(r_se);
        }
    }
    // also get features
    FOR_EACH_ANNOT_ON_SEQENTRY (annot_it, se) {
        FOR_EACH_SEQFEAT_ON_SEQANNOT (feat_it, **annot_it) {
            if ((*feat_it)->IsSetData() && (*feat_it)->GetData().IsBiosrc()
                && (*feat_it)->GetData().GetBiosrc().IsSetOrg()) {
                CConstRef<CSeq_feat> feat;
                feat.Reset(*feat_it);
                src_feats.push_back(feat);
            }
        }
    }

    // if set, recurse
    if (se.IsSet()) {
        FOR_EACH_SEQENTRY_ON_SEQSET (it, se.GetSet()) {
            GatherSources (**it, src_descs, desc_ctxs, src_feats);
        }
    }
}



static bool s_HasMisSpellFlag (const CT3Data& data)
{
    bool has_misspell_flag = false;

    if (data.IsSetStatus()) {
        ITERATE (CT3Reply::TData::TStatus, status_it, data.GetStatus()) {
            if ((*status_it)->IsSetProperty()) {
                string prop = (*status_it)->GetProperty();
                if (NStr::EqualNocase(prop, "misspelled_name")) {
                    has_misspell_flag = true;
                    break;
                }
            }
        }
    }
    return has_misspell_flag;
}


static string s_FindMatchInOrgRef (string str, const COrg_ref& org)
{
    string match = "";

    if (NStr::IsBlank(str)) {
        // do nothing;
    } else if (org.IsSetTaxname() && NStr::EqualNocase(str, org.GetTaxname())) {
        match = org.GetTaxname();
    } else if (org.IsSetCommon() && NStr::EqualNocase(str, org.GetCommon())) {
        match = org.GetCommon();
    } else {
        FOR_EACH_SYN_ON_ORGREF (syn_it, org) {
            if (NStr::EqualNocase(str, *syn_it)) {
                match = *syn_it;
                break;
            }
        }
        if (NStr::IsBlank(match)) {
            FOR_EACH_ORGMOD_ON_ORGREF (mod_it, org) {
                if ((*mod_it)->IsSetSubtype()
                    && ((*mod_it)->GetSubtype() == COrgMod::eSubtype_gb_synonym
                        || (*mod_it)->GetSubtype() == COrgMod::eSubtype_old_name)
                    && (*mod_it)->IsSetSubname()
                    && NStr::EqualNocase(str, (*mod_it)->GetSubname())) {
                    match = (*mod_it)->GetSubname();
                    break;
                }
            }
        }
    }
    return match;
}


void CValidError_imp::ValidateSpecificHost 
(const vector<CConstRef<CSeqdesc> > & src_descs,
 const vector<CConstRef<CSeq_entry> > & desc_ctxs,
 const vector<CConstRef<CSeq_feat> > & src_feats)
{
    vector<CConstRef<CSeqdesc> > local_src_descs;
    vector<CConstRef<CSeq_entry> > local_desc_ctxs;
    vector<CConstRef<CSeq_feat> > local_src_feats;

    vector< CRef<COrg_ref> > org_rq_list;

    // first do descriptors
    vector<CConstRef<CSeqdesc> >::const_iterator desc_it = src_descs.begin();
    vector<CConstRef<CSeq_entry> >::const_iterator ctx_it = desc_ctxs.begin();
    while (desc_it != src_descs.end() && ctx_it != desc_ctxs.end()) {
        FOR_EACH_ORGMOD_ON_BIOSOURCE (mod_it, (*desc_it)->GetSource()) {
            if ((*mod_it)->IsSetSubtype()
                && (*mod_it)->GetSubtype() == COrgMod::eSubtype_nat_host
                && (*mod_it)->IsSetSubname()
                && isupper ((*mod_it)->GetSubname().c_str()[0])) {
                   string host = (*mod_it)->GetSubname();
                size_t pos = NStr::Find(host, " ");
                if (pos != string::npos) {
                    if (! NStr::StartsWith(host.substr(pos + 1), "sp.")
                        && ! NStr::StartsWith(host.substr(pos + 1), "(")) {
                        pos = NStr::Find(host, " ", pos + 1);
                        if (pos != string::npos) {
                            host = host.substr(0, pos);
                        }
                    } else {
                        host = host.substr(0, pos);
                    }
                }

                CRef<COrg_ref> rq(new COrg_ref);
                rq->SetTaxname(host);
                org_rq_list.push_back(rq);
                local_src_descs.push_back(*desc_it);
                local_desc_ctxs.push_back(*ctx_it);
            }
        }

        ++desc_it;
        ++ctx_it;
    }

    // collect features with specific hosts
    vector<CConstRef<CSeq_feat> >::const_iterator feat_it = src_feats.begin();
    while (feat_it != src_feats.end()) {
        FOR_EACH_ORGMOD_ON_BIOSOURCE (mod_it, (*feat_it)->GetData().GetBiosrc()) {
            if ((*mod_it)->IsSetSubtype()
                && (*mod_it)->GetSubtype() == COrgMod::eSubtype_nat_host
                && (*mod_it)->IsSetSubname()
                && isupper ((*mod_it)->GetSubname().c_str()[0])) {
                string host = (*mod_it)->GetSubname();
                size_t pos = NStr::Find(host, " ");
                if (pos != string::npos) {
                    if (! NStr::StartsWith(host.substr(pos + 1), "sp.")
                        && ! NStr::StartsWith(host.substr(pos + 1), "(")) {
                        pos = NStr::Find(host, " ", pos + 1);
                        if (pos != string::npos) {
                            host = host.substr(0, pos);
                        }
                    } else {
                        host = host.substr(0, pos);
                    }
                }

                CRef<COrg_ref> rq(new COrg_ref);
                rq->SetTaxname(host);
                org_rq_list.push_back(rq);
                local_src_feats.push_back(*feat_it);
            }
        }

        ++feat_it;
    }

    if (org_rq_list.size() > 0) {
        CTaxon3 taxon3;
        taxon3.Init();
        CRef<CTaxon3_reply> reply = taxon3.SendOrgRefList(org_rq_list);
        if (reply) {
            CTaxon3_reply::TReply::const_iterator reply_it = reply->GetReply().begin();
            vector< CRef<COrg_ref> >::iterator rq_it = org_rq_list.begin();
            // process descriptor responses
            desc_it = local_src_descs.begin();
            ctx_it = local_desc_ctxs.begin();

            while (reply_it != reply->GetReply().end()
                   && rq_it != org_rq_list.end()
                   && desc_it != local_src_descs.end()
                   && ctx_it != local_desc_ctxs.end()) {
                
                string host = (*rq_it)->GetTaxname();
                if ((*reply_it)->IsError()) {
                    string err_str = "?";
                    if ((*reply_it)->GetError().IsSetMessage()) {
                        err_str = (*reply_it)->GetError().GetMessage();
                    }
                    if(NStr::Find(err_str, "ambiguous") != string::npos) {
                        PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadSpecificHost,
                                    "Specific host value is ambiguous: " + host,
                                    **desc_it, *ctx_it);
                    } else {
                        PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadSpecificHost, 
                                    "Invalid value for specific host: " + host,
                                    **desc_it, *ctx_it);
                    }
                } else if ((*reply_it)->IsData()) {
                    if (s_HasMisSpellFlag((*reply_it)->GetData())) {
                        PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadSpecificHost, 
                                    "Specific host value is misspelled: " + host,
                                    **desc_it, *ctx_it);
                    } else if ((*reply_it)->GetData().IsSetOrg()) {
                        string match = s_FindMatchInOrgRef (host, (*reply_it)->GetData().GetOrg());
                        if (!NStr::EqualCase(match, host)) {
                            PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadSpecificHost, 
                                        "Specific host value is incorrectly capitalized: " + host,
                                        **desc_it, *ctx_it);
                        }
                    } else {
                        PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadSpecificHost, 
                                    "Invalid value for specific host: " + host,
                                    **desc_it, *ctx_it);
                    }
                }
                ++reply_it;
                ++rq_it;
                ++desc_it;
                ++ctx_it;
            }

            // TO DO - process feature responses
            feat_it = local_src_feats.begin(); 
            while (reply_it != reply->GetReply().end()
                   && rq_it != org_rq_list.end()
                   && feat_it != local_src_feats.end()) {
                string host = (*rq_it)->GetTaxname();
                if ((*reply_it)->IsError()) {
                    string err_str = "?";
                    if ((*reply_it)->GetError().IsSetMessage()) {
                        err_str = (*reply_it)->GetError().GetMessage();
                    }
                    if(NStr::Find(err_str, "ambiguous") != string::npos) {
                        PostErr (eDiag_Warning, eErr_SEQ_DESCR_BadSpecificHost,
                                    "Specific host value is ambiguous: " + host,
                                    **feat_it);
                    } else {
                        PostErr (eDiag_Warning, eErr_SEQ_DESCR_BadSpecificHost, 
                                    "Invalid value for specific host: " + host,
                                    **feat_it);
                    }
                } else if ((*reply_it)->IsData()) {
                    if (s_HasMisSpellFlag((*reply_it)->GetData())) {
                        PostErr (eDiag_Warning, eErr_SEQ_DESCR_BadSpecificHost, 
                                    "Specific host value is misspelled: " + host,
                                    **feat_it);
                    } else if ((*reply_it)->GetData().IsSetOrg()) {
                        string match = s_FindMatchInOrgRef (host, (*reply_it)->GetData().GetOrg());
                        if (!NStr::EqualCase(match, host)) {
                            PostErr (eDiag_Warning, eErr_SEQ_DESCR_BadSpecificHost, 
                                        "Specific host value is incorrectly capitalized: " + host,
                                        **feat_it);
                        }
                    } else {
                        PostErr (eDiag_Warning, eErr_SEQ_DESCR_BadSpecificHost, 
                                    "Invalid value for specific host: " + host,
                                    **feat_it);
                    }
                }
                ++reply_it;
                ++rq_it;
                ++feat_it;
            }
        }
    }
}


void CValidError_imp::ValidateTaxonomy(const CSeq_entry& se)
{
    vector<CConstRef<CSeqdesc> > src_descs;
    vector<CConstRef<CSeq_entry> > desc_ctxs;
    vector<CConstRef<CSeq_feat> > src_feats;

    GatherSources (se, src_descs, desc_ctxs, src_feats);

    // request list for taxon3
    vector< CRef<COrg_ref> > org_rq_list;

    // first do descriptors
    vector<CConstRef<CSeqdesc> >::iterator desc_it = src_descs.begin();
    vector<CConstRef<CSeq_entry> >::iterator ctx_it = desc_ctxs.begin();
    while (desc_it != src_descs.end() && ctx_it != desc_ctxs.end()) {
        CRef<COrg_ref> rq(new COrg_ref);
        const COrg_ref& org = (*desc_it)->GetSource().GetOrg();
        rq->Assign(org);
        org_rq_list.push_back(rq);

        ++desc_it;
        ++ctx_it;
    }

    // now do features
    vector<CConstRef<CSeq_feat> >::iterator feat_it = src_feats.begin();
    while (feat_it != src_feats.end()) {
        CRef<COrg_ref> rq(new COrg_ref);
        const COrg_ref& org = (*feat_it)->GetData().GetBiosrc().GetOrg();
        rq->Assign(org);
        org_rq_list.push_back(rq);

        ++feat_it;
    }

    if (org_rq_list.size() > 0) {
        CTaxon3 taxon3;
        taxon3.Init();
        CRef<CTaxon3_reply> reply = taxon3.SendOrgRefList(org_rq_list);
        if (reply) {
            CTaxon3_reply::TReply::const_iterator reply_it = reply->GetReply().begin();

            // process descriptor responses
            desc_it = src_descs.begin();
            ctx_it = desc_ctxs.begin();

            while (reply_it != reply->GetReply().end()
                   && desc_it != src_descs.end()
                   && ctx_it != desc_ctxs.end()) {
                if ((*reply_it)->IsError()) {
                    string err_str = "?";
                    if ((*reply_it)->GetError().IsSetMessage()) {
                        err_str = (*reply_it)->GetError().GetMessage();
                    }
                    PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_TaxonomyLookupProblem, 
                                "Taxonomy lookup failed with message '" + err_str + "'",
                                **desc_it, *ctx_it);
                } else if ((*reply_it)->IsData()) {
                    bool is_species_level = true;
                    bool force_consult = false;
                    bool has_nucleomorphs = false;
                    (*reply_it)->GetData().GetTaxFlags(is_species_level, force_consult, has_nucleomorphs);
                    if (!is_species_level) {
                        PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_TaxonomyLookupProblem, 
                                "Taxonomy lookup reports is_species_level FALSE",
                                **desc_it, *ctx_it);
                    }
                    if (force_consult) {
                        PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_TaxonomyLookupProblem, 
                                "Taxonomy lookup reports taxonomy consultation needed",
                                **desc_it, *ctx_it);
                    }
                    if ((*desc_it)->GetSource().IsSetGenome()
                        && (*desc_it)->GetSource().GetGenome() == CBioSource::eGenome_nucleomorph
                        && !has_nucleomorphs) {
                        PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_TaxonomyLookupProblem, 
                                "Taxonomy lookup does not have expected nucleomorph flag",
                                **desc_it, *ctx_it);
                    }
                }
                ++reply_it;
                ++desc_it;
                ++ctx_it;
            }
            // process feat responses
            feat_it = src_feats.begin(); 
            while (reply_it != reply->GetReply().end()
                   && feat_it != src_feats.end()) {
                if ((*reply_it)->IsError()) {
                    string err_str = "?";
                    if ((*reply_it)->GetError().IsSetMessage()) {
                        err_str = (*reply_it)->GetError().GetMessage();
                    }
                    PostErr (eDiag_Warning, eErr_SEQ_DESCR_TaxonomyLookupProblem, 
                            "Taxonomy lookup failed with message '" + err_str + "'",
                            **feat_it);
                } else if ((*reply_it)->IsData()) {
                    bool is_species_level = true;
                    bool force_consult = false;
                    bool has_nucleomorphs = false;
                    (*reply_it)->GetData().GetTaxFlags(is_species_level, force_consult, has_nucleomorphs);
                    if (!is_species_level) {
                        PostErr (eDiag_Warning, eErr_SEQ_DESCR_TaxonomyLookupProblem, 
                                "Taxonomy lookup reports is_species_level FALSE",
                                **feat_it);
                    }
                    if (force_consult) {
                        PostErr (eDiag_Warning, eErr_SEQ_DESCR_TaxonomyLookupProblem, 
                                "Taxonomy lookup reports taxonomy consultation needed",
                                **feat_it);
                    }
                    if ((*feat_it)->GetData().GetBiosrc().IsSetGenome()
                        && (*feat_it)->GetData().GetBiosrc().GetGenome() == CBioSource::eGenome_nucleomorph
                        && !has_nucleomorphs) {
                        PostErr (eDiag_Warning, eErr_SEQ_DESCR_TaxonomyLookupProblem, 
                                "Taxonomy lookup does not have expected nucleomorph flag",
                                **feat_it);
                    }
                }
                ++reply_it;
                ++feat_it;
            }            
        }
    }

    // Now look at specific-host values
    ValidateSpecificHost (src_descs, desc_ctxs, src_feats);

}


void CValidError_imp::ValidateTaxonomy(const COrg_ref& org, int genome)
{
    // request list for taxon3
    vector< CRef<COrg_ref> > org_rq_list;
    CRef<COrg_ref> rq(new COrg_ref);
    rq->Assign(org);
    org_rq_list.push_back(rq);

    CTaxon3 taxon3;
    taxon3.Init();
    CRef<CTaxon3_reply> reply = taxon3.SendOrgRefList(org_rq_list);
    if (reply) {
        CTaxon3_reply::TReply::const_iterator reply_it = reply->GetReply().begin();

        while (reply_it != reply->GetReply().end()) {
            if ((*reply_it)->IsError()) {
                string err_str = "?";
                if ((*reply_it)->GetError().IsSetMessage()) {
                    err_str = (*reply_it)->GetError().GetMessage();
                }
                PostErr (eDiag_Warning, eErr_SEQ_DESCR_TaxonomyLookupProblem, 
                            "Taxonomy lookup failed with message '" + err_str + "'", org);
            } else if ((*reply_it)->IsData()) {
                bool is_species_level = true;
                bool force_consult = false;
                bool has_nucleomorphs = false;
                (*reply_it)->GetData().GetTaxFlags(is_species_level, force_consult, has_nucleomorphs);
                if (!is_species_level) {
                    PostErr (eDiag_Warning, eErr_SEQ_DESCR_TaxonomyLookupProblem, 
                            "Taxonomy lookup reports is_species_level FALSE", org);
                }
                if (force_consult) {
                    PostErr (eDiag_Warning, eErr_SEQ_DESCR_TaxonomyLookupProblem, 
                            "Taxonomy lookup reports taxonomy consultation needed", org);
                }
                if (genome == CBioSource::eGenome_nucleomorph
                    && !has_nucleomorphs) {
                    PostErr (eDiag_Warning, eErr_SEQ_DESCR_TaxonomyLookupProblem, 
                            "Taxonomy lookup does not have expected nucleomorph flag", org);
                }
            }
            ++reply_it;
        }
    }

    // Now look at specific-host values
    org_rq_list.clear();

    FOR_EACH_ORGMOD_ON_ORGREF  (mod_it, org) {
        if ((*mod_it)->IsSetSubtype()
            && (*mod_it)->GetSubtype() == COrgMod::eSubtype_nat_host
            && (*mod_it)->IsSetSubname()
            && isupper ((*mod_it)->GetSubname().c_str()[0])) {
               string host = (*mod_it)->GetSubname();
            size_t pos = NStr::Find(host, " ");
            if (pos != string::npos) {
                if (! NStr::StartsWith(host.substr(pos + 1), "sp.")
                    && ! NStr::StartsWith(host.substr(pos + 1), "(")) {
                    pos = NStr::Find(host, " ", pos + 1);
                    if (pos != string::npos) {
                        host = host.substr(0, pos);
                    }
                } else {
                    host = host.substr(0, pos);
                }
            }

            CRef<COrg_ref> rq(new COrg_ref);
            rq->SetTaxname(host);
            org_rq_list.push_back(rq);
        }
    }

    reply = taxon3.SendOrgRefList(org_rq_list);
    if (reply) {
        CTaxon3_reply::TReply::const_iterator reply_it = reply->GetReply().begin();
        vector< CRef<COrg_ref> >::iterator rq_it = org_rq_list.begin();

        while (reply_it != reply->GetReply().end()
               && rq_it != org_rq_list.end()) {
            
            string host = (*rq_it)->GetTaxname();
            if ((*reply_it)->IsError()) {
                string err_str = "?";
                if ((*reply_it)->GetError().IsSetMessage()) {
                    err_str = (*reply_it)->GetError().GetMessage();
                }
                if(NStr::Find(err_str, "ambiguous") != string::npos) {
                    PostErr (eDiag_Warning, eErr_SEQ_DESCR_BadSpecificHost,
                                "Specific host value is ambiguous: " + host, org);
                } else {
                    PostErr (eDiag_Warning, eErr_SEQ_DESCR_BadSpecificHost, 
                                "Invalid value for specific host: " + host, org);
                }
            } else if ((*reply_it)->IsData()) {
                if (s_HasMisSpellFlag((*reply_it)->GetData())) {
                    PostErr (eDiag_Warning, eErr_SEQ_DESCR_BadSpecificHost, 
                                "Specific host value is misspelled: " + host, org);
                } else if ((*reply_it)->GetData().IsSetOrg()) {
                    string match = s_FindMatchInOrgRef (host, (*reply_it)->GetData().GetOrg());
                    if (!NStr::EqualCase(match, host)) {
                        PostErr (eDiag_Warning, eErr_SEQ_DESCR_BadSpecificHost, 
                                    "Specific host value is incorrectly capitalized: " + host, org);
                    }
                } else {
                    PostErr (eDiag_Warning, eErr_SEQ_DESCR_BadSpecificHost, 
                                "Invalid value for specific host: " + host, org);
                }
            }
            ++reply_it;
            ++rq_it;
        }
    }

}


CPCRSet::CPCRSet(size_t pos) : m_OrigPos (pos)
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
    for (int i = 0; i < m_SetList.size(); i++) {
        delete m_SetList[i];
    }
    m_SetList.clear();
}


void CPCRSetList::AddFwdName (string name)
{
    int pcr_num = 0;
    if (NStr::StartsWith(name, "(") && NStr::EndsWith(name, ")") && NStr::Find(name, ",") != string::npos) {
        name = name.substr(1, name.length() - 2);
        vector<string> mult_names;
        NStr::Tokenize(name, ",", mult_names);
        int name_num = 0;
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


void CPCRSetList::AddRevName (string name)
{
    int pcr_num = 0;
    if (NStr::StartsWith(name, "(") && NStr::EndsWith(name, ")") && NStr::Find(name, ",") != string::npos) {
        name = name.substr(1, name.length() - 2);
        vector<string> mult_names;
        NStr::Tokenize(name, ",", mult_names);
        int name_num = 0;
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


void CPCRSetList::AddFwdSeq (string name)
{
    int pcr_num = 0;
    if (NStr::StartsWith(name, "(") && NStr::EndsWith(name, ")") && NStr::Find(name, ",") != string::npos) {
        name = name.substr(1, name.length() - 2);
        vector<string> mult_names;
        NStr::Tokenize(name, ",", mult_names);
        int name_num = 0;
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


void CPCRSetList::AddRevSeq (string name)
{
    int pcr_num = 0;
    if (NStr::StartsWith(name, "(") && NStr::EndsWith(name, ")") && NStr::Find(name, ",") != string::npos) {
        name = name.substr(1, name.length() - 2);
        vector<string> mult_names;
        NStr::Tokenize(name, ",", mult_names);
        int name_num = 0;
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


static bool s_PCRSetCompare (
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


static bool s_PCRSetEqual (
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
    stable_sort (m_SetList.begin(), 
                 m_SetList.end(),
                 s_PCRSetCompare);

    return seq_mac_is_unique (m_SetList.begin(),
                   m_SetList.end(),
                   s_PCRSetEqual);

}




// CCountryLine
CCountryLine::CCountryLine 
(string country_name, double y, double min_x, double max_x, double scale)
: m_CountryName(country_name) ,
  m_Scale (scale)
{
    m_Y = x_ConvertLat(y);
    m_MinX = x_ConvertLon(min_x);
    m_MaxX = x_ConvertLon(max_x);

}


CCountryLine::~CCountryLine (void)
{
}


#define EPSILON 0.001

int CCountryLine::ConvertLat (double y, double scale) 
{

    int  val = 0;

    if (y < -90.0) {
        y = -90.0;
    }
    if (y > 90.0) {
        y = 90.0;
    }

    if (y > 0) {
        val = (int) (y * scale + EPSILON);
    } else {
        val = (int) (-(-y * scale + EPSILON));
    }

    return val;
}


int CCountryLine::x_ConvertLat (double y) 
{
    return ConvertLat(y, m_Scale);
}

int CCountryLine::ConvertLon (double x, double scale) 
{

  int  val = 0;

  if (x < -180.0) {
    x = -180.0;
  }
  if (x > 180.0) {
    x = 180.0;
  }

  if (x > 0) {
    val = (int) (x * scale + EPSILON);
  } else {
    val = (int) (-(-x * scale + EPSILON));
  }

  return val;
}


int CCountryLine::x_ConvertLon (double x) 
{
    return ConvertLon(x, m_Scale);
}


CCountryExtreme::CCountryExtreme (string country_name, int min_x, int min_y, int max_x, int max_y)
: m_CountryName(country_name) , m_MinX (min_x), m_MinY (min_y), m_MaxX(max_x), m_MaxY (max_y)
{
    m_Area = (1 + m_MaxY - m_MinY) * (1 + m_MaxX - m_MinX);
    size_t pos = NStr::Find(country_name, ":");
    if (pos == string::npos) {
        m_Level0 = country_name;
        m_Level1 = "";
    } else {
        m_Level0 = country_name.substr(0, pos);
        NStr::TruncateSpacesInPlace(m_Level0);
        m_Level1 = country_name.substr(pos + 1);
        NStr::TruncateSpacesInPlace(m_Level1);
    }

}


CCountryExtreme::~CCountryExtreme (void)
{

}


bool CCountryExtreme::SetMinX(int min_x) 
{ 
    if (min_x < m_MinX) { 
        m_MinX = min_x; 
        return true;
    } else { 
        return false; 
    } 
}


bool CCountryExtreme::SetMaxX(int max_x) 
{ 
    if (max_x > m_MaxX) { 
        m_MaxX = max_x; 
        return true;
    } else { 
        return false; 
    } 
}


bool CCountryExtreme::SetMinY(int min_y) 
{ 
    if (min_y < m_MinY) { 
        m_MinY = min_y; 
        return true;
    } else { 
        return false; 
    } 
}


bool CCountryExtreme::SetMaxY(int max_y) 
{ 
    if (max_y > m_MaxY) { 
        m_MaxY = max_y; 
        return true;
    } else { 
        return false; 
    } 
}


void CCountryExtreme::AddLine(const CCountryLine *line)
{
    if (line) {
        SetMinX(line->GetMinX());
        SetMaxX(line->GetMaxX());
        SetMinY(line->GetY());
        SetMaxY(line->GetY());
        m_Area += 1 + line->GetMaxX() - line->GetMinX();
    }
}


bool CCountryExtreme::DoesOverlap(const CCountryExtreme* other_block) const
{
    if (!other_block) {
        return false;
    } else if (m_MaxX >= other_block->GetMinX()
        && m_MaxX <= other_block->GetMaxX()
        && m_MaxY >= other_block->GetMinY()
        && m_MinY <= other_block->GetMaxY()) {
        return true;
    } else if (other_block->GetMaxX() >= m_MinX
        && other_block->GetMaxX() <= m_MaxX
        && other_block->GetMaxY() >= m_MinY
        && other_block->GetMinY() <= m_MaxY) {
        return true;
    } else {
        return false;
    }
}


bool CCountryExtreme::PreferTo(const CCountryExtreme* other_block, const string country, const string province, const bool prefer_new) const
{
    if (!other_block) {
        return true;
    }

    // if no preferred country, these are equal
    if (NStr::IsBlank(country)) {
        return prefer_new;
    }
    
    // if match to preferred country 
    if (NStr::EqualNocase(country, m_Level0)) {
        // if best was not preferred country, take new match
        if (!NStr::EqualNocase(country, other_block->GetLevel0())) {
            return true;
        }
        // if match to preferred province
        if (!NStr::IsBlank(province) && NStr::EqualNocase(province, m_Level1)) {
            // if best was not preferred province, take new match
            if (!NStr::EqualNocase(province, other_block->GetLevel1())) {
                return true;
            }
        }
            
        // if both match province, or neither does, or no preferred province, take smallest
        return prefer_new;
    }

    // if best matches preferred country, keep
    if (NStr::EqualNocase(country, other_block->GetLevel0())) {
        return false;
    }

    // otherwise take smallest
    return prefer_new;
}


CLatLonCountryId::CLatLonCountryId(float lat, float lon)
{
    m_Lat = lat;
    m_Lon = lon;
    m_FullGuess = "";
    m_GuessCountry = "";
    m_GuessProvince = "";
    m_GuessWater = "";
    m_ClosestFull = "";
    m_ClosestCountry = "";
    m_ClosestProvince = "";
    m_ClosestWater = "";
    m_ClaimedFull = "";
    m_LandDistance = -1;
    m_WaterDistance = -1;
    m_ClaimedDistance = -1;
}

CLatLonCountryId::~CLatLonCountryId(void)
{
}


#include "lat_lon_country.inc"

static const int k_NumLatLonCountryText = sizeof (s_DefaultLatLonCountryText) / sizeof (char *);

#include "lat_lon_water.inc"

static const int k_NumLatLonWaterText = sizeof (s_DefaultLatLonWaterText) / sizeof (char *);

void CLatLonCountryMap::x_InitFromDefaultList(const char * const *list, int num)
{
      // initialize list of country lines
    m_CountryLineList.clear();
    m_Scale = 20.0;
    string current_country = "";

    for (int i = 0; i < num; i++) {
            const string& line = list[i];

        if (line.c_str()[0] == '-') {
            // skip comment
        } else if (isalpha (line.c_str()[0])) {
            current_country = line;
        } else if (isdigit (line.c_str()[0])) {
            m_Scale = NStr::StringToDouble(line);
        } else {          
            vector<string> tokens;
              NStr::Tokenize(line, "\t", tokens);
            if (tokens.size() > 3) {
                double x = NStr::StringToDouble(tokens[1]);
                for (int j = 2; j < tokens.size() - 1; j+=2) {
                    m_CountryLineList.push_back(new CCountryLine(current_country, x, NStr::StringToDouble(tokens[j]), NStr::StringToDouble(tokens[j + 1]), m_Scale));
                }
            }
        }
    }
}




bool CLatLonCountryMap::x_InitFromFile(string filename)
{
    string fname = g_FindDataFile (filename);
    if (NStr::IsBlank (fname)) {
        return false;
    }

    CRef<ILineReader> lr = ILineReader::New (fname);
    if (lr.Empty()) {
        return false;
    } else {
        m_Scale = 20.0;
        string current_country = "";
        do {
            const string& line = *++*lr;
            if (line.c_str()[0] == '-') {
                // skip comment
            } else if (isalpha (line.c_str()[0])) {
                current_country = line;
            } else if (isdigit (line.c_str()[0])) {
                m_Scale = NStr::StringToDouble(line);
            } else {          
                vector<string> tokens;
                  NStr::Tokenize(line, "\t", tokens);
                if (tokens.size() > 3) {
                    double y = NStr::StringToDouble(tokens[1]);
                    for (int j = 2; j < tokens.size() - 1; j+=2) {
                        m_CountryLineList.push_back(new CCountryLine(current_country, y, NStr::StringToDouble(tokens[j]), NStr::StringToDouble(tokens[j + 1]), m_Scale));
                    }
                }
            }
        } while ( !lr->AtEOF() );
        return true;
    }
}


bool CLatLonCountryMap::
        s_CompareTwoLinesByCountry(const CCountryLine* line1,
                                    const CCountryLine* line2)
{
    int cmp = NStr::CompareNocase(line1->GetCountry(), line2->GetCountry());
    if (cmp == 0) {        
        if (line1->GetY() < line2->GetY()) {
            return true;
        } else if (line1->GetY() > line2->GetY()) {
            return false;
        } else {
            if (line1->GetMinX() < line2->GetMinX()) {
                return true;
            } else {
                return false;
            }
        }
    } else if (cmp < 0) {
        return true;
    } else {
        return false;
    }
}


bool CLatLonCountryMap::
        s_CompareTwoLinesByLatLonThenCountry(const CCountryLine* line1,
                                    const CCountryLine* line2)
{
    if (line1->GetY() < line2->GetY()) {
        return true;
    } else if (line1->GetY() > line2->GetY()) {
        return false;
    } if (line1->GetMinX() < line2->GetMinX()) {
        return true;
    } else if (line1->GetMinX() > line2->GetMinX()) {
        return false;
    } else if (line1->GetMaxX() < line2->GetMaxX()) {
        return true;
    } else if (line1->GetMaxX() > line2->GetMaxX()) {
        return false;
    } else {
        int cmp = NStr::CompareNocase(line1->GetCountry(), line2->GetCountry());
        if (cmp < 0) {
            return true;
        } else {
            return false;
        }
    }
}


CLatLonCountryMap::CLatLonCountryMap (bool is_water) 
{
    // initialize list of country lines
    m_CountryLineList.clear();

    if (is_water) {
        if (!x_InitFromFile("lat_lon_water.txt")) {
            x_InitFromDefaultList(s_DefaultLatLonWaterText, k_NumLatLonWaterText);
        }
    } else {
        if (!x_InitFromFile("lat_lon_country.txt")) {
            x_InitFromDefaultList(s_DefaultLatLonCountryText, k_NumLatLonCountryText);
        }
    }
    sort (m_CountryLineList.begin(), m_CountryLineList.end(), s_CompareTwoLinesByCountry);

    // set up extremes index and copy into LatLon index
    m_CountryExtremes.clear();
    m_LatLonSortedList.clear();
      size_t i, ext = 0;

    for (i = 0; i < m_CountryLineList.size(); i++) {
        if (ext > 0 && NStr::Equal(m_CountryLineList[i]->GetCountry(), m_CountryExtremes[ext - 1]->GetCountry())) {
            m_CountryExtremes[ext - 1]->AddLine(m_CountryLineList[i]);
        } else {
            m_CountryExtremes.push_back(new CCountryExtreme(m_CountryLineList[i]->GetCountry(),
                                                m_CountryLineList[i]->GetMinX(), 
                                                m_CountryLineList[i]->GetY(),
                                                m_CountryLineList[i]->GetMaxX(),
                                                m_CountryLineList[i]->GetY()));
            ext++;
        }
        m_LatLonSortedList.push_back(m_CountryLineList[i]);
        m_CountryLineList[i]->SetBlock(m_CountryExtremes[ext - 1]);
    }
    sort (m_LatLonSortedList.begin(), m_LatLonSortedList.end(), s_CompareTwoLinesByLatLonThenCountry);

}


CLatLonCountryMap::~CLatLonCountryMap (void)
{
      size_t i;

    for (i = 0; i < m_CountryLineList.size(); i++) {
        delete (m_CountryLineList[i]);
    }
    m_CountryLineList.clear();

    for (i = 0; i < m_CountryExtremes.size(); i++) {
        delete (m_CountryExtremes[i]);
    }
    m_CountryExtremes.clear();
    // note - do not delete items in m_LatLonSortedList, they are pointing to the same objects as m_CountryLineList
    m_LatLonSortedList.clear();
}


bool CLatLonCountryMap::IsCountryInLatLon(string country, double lat, double lon)
{
    bool rval = true;
    int x = CCountryLine::ConvertLon(lon, m_Scale);
    int y = CCountryLine::ConvertLat(lat, m_Scale);

    int L, R, mid;

    L = 0;
    R = m_CountryLineList.size() - 1;
    mid = 0;

    while (L < R) {
        mid = (L + R) / 2;
        int cmp = NStr::Compare(m_CountryLineList[mid]->GetCountry(), country);
        if (cmp < 0) {
            L = mid + 1;
        } else if (cmp > 0) {
            R = mid;
        } else {
            while (mid > 0 
                   && NStr::Compare(m_CountryLineList[mid - 1]->GetCountry(), country) == 0
                   && m_CountryLineList[mid - 1]->GetY() >= y) {
                mid--;
            }
            L = mid;
            R = mid;
        }
    }

    while (R < m_CountryLineList.size() 
           && NStr::EqualNocase(country, m_CountryLineList[R]->GetCountry())
           && m_CountryLineList[R]->GetY() < y) {
        R++;
    }

    while (R < m_CountryLineList.size() 
           && NStr::EqualNocase(country, m_CountryLineList[R]->GetCountry())
           && m_CountryLineList[R]->GetY() == y
           && m_CountryLineList[R]->GetMaxX() < x) {
        R++;
    }
    if (R < m_CountryLineList.size() 
           && NStr::EqualNocase(country, m_CountryLineList[R]->GetCountry())
           && m_CountryLineList[R]->GetY() == y
           && m_CountryLineList[R]->GetMinX() <= x 
           && m_CountryLineList[R]->GetMaxX() >= x) {
        return true;
    } else {
        return false;
    }    
}


const CCountryExtreme * CLatLonCountryMap::x_FindCountryExtreme (string country)
{
    int L, R, mid;

    if (NStr::IsBlank (country)) return NULL;

    L = 0;
    R = m_CountryExtremes.size() - 1;
    mid = 0;

    while (L < R) {
        mid = (L + R) / 2;
        if (NStr::CompareNocase(m_CountryExtremes[mid]->GetCountry(), country) < 0) {
            L = mid + 1;
        } else {
            R = mid;
        }
    }
    if (!NStr::EqualNocase(m_CountryExtremes[R]->GetCountry(), country)) {
        return NULL;
    } else {
        return m_CountryExtremes[R];
    }
}


bool CLatLonCountryMap::HaveLatLonForRegion(string region)
{
    if (x_FindCountryExtreme(region) == NULL) {
        return false;
    } else {
        return true;
    }
}


int CLatLonCountryMap::x_GetLatStartIndex (int y)
{
    int L, R, mid;

    L = 0;
    R = m_LatLonSortedList.size() - 1;
    mid = 0;

    while (L < R) {
        mid = (L + R) / 2;
        if (m_LatLonSortedList[mid]->GetY() < y) {
            L = mid + 1;
        } else if (m_LatLonSortedList[mid]->GetY() > y) {
            R = mid;
        } else {
            while (mid > 0 && m_LatLonSortedList[mid - 1]->GetY() == y) {
                mid--;
            }
            L = mid;
            R = mid;
        }
    }
    return R;
}


const CCountryExtreme * CLatLonCountryMap::GuessRegionForLatLon(double lat, double lon, string country, string province)
{
    int x = CCountryLine::ConvertLon(lon, m_Scale);
    int y = CCountryLine::ConvertLon(lat, m_Scale);

    int R = x_GetLatStartIndex(y);

    const CCountryExtreme *best = NULL;

    int smallest_area = -1;
    while (R < m_LatLonSortedList.size() && m_LatLonSortedList[R]->GetY() == y) {
            if (m_LatLonSortedList[R]->GetMinX() <= x 
            && m_LatLonSortedList[R]->GetMaxX() >= x) {
            const CCountryExtreme *other = m_LatLonSortedList[R]->GetBlock();
            if (best == NULL) {
                best = other;
            } else if (!best->PreferTo(other, country, province, (bool)(best->GetArea() <= other->GetArea()))) {
                best = other;
            }
             }
        R++;
      }
      return best;
}


//Distance on a spherical surface calculation adapted from
//http://www.linuxjournal.com/magazine/
//work-shell-calculating-distance-between-two-latitudelongitude-points

#define EARTH_RADIUS 6371.0 /* average radius of non-spherical earth in kilometers */
#define CONST_PI 3.14159265359

static double DegreesToRadians (
  double degrees
)

{
  return (degrees * (CONST_PI / 180.0));
}

static double DistanceOnGlobe (
  double latA,
  double lonA,
  double latB,
  double lonB
)

{
  double lat1, lon1, lat2, lon2;
  double dLat, dLon, a, c;

  lat1 = DegreesToRadians (latA);
  lon1 = DegreesToRadians (lonA);
  lat2 = DegreesToRadians (latB);
  lon2 = DegreesToRadians (lonB);

  dLat = lat2 - lat1;
  dLon = lon2 - lon1;

   a = sin (dLat / 2) * sin (dLat / 2) +
       cos (lat1) * cos (lat2) * sin (dLon / 2) * sin (dLon / 2);
   c = 2 * atan2 (sqrt (a), sqrt (1 - a));

  return (double) (EARTH_RADIUS * c);
}


CCountryExtreme * CLatLonCountryMap::FindClosestToLatLon (double lat, double lon, double range, double &distance)
{
    int x = CCountryLine::ConvertLon(lon, m_Scale);
    int y = CCountryLine::ConvertLon(lat, m_Scale);

    int maxDelta = (int) (range * m_Scale + EPSILON);
    int min_y = y - maxDelta;
    int max_y = y + maxDelta;
    int min_x = x - maxDelta;
    int max_x = x + maxDelta;

    // binary search to lowest lat
    int R = x_GetLatStartIndex(min_y);

    double closest = 0.0;
    CCountryExtreme *rval = NULL;

    while (R < m_LatLonSortedList.size() && m_LatLonSortedList[R]->GetY() <= max_y) {
        if (m_LatLonSortedList[R]->GetMaxX() < min_x || m_LatLonSortedList[R]->GetMinX() > max_x) {
            // out of range, don't bother calculating distance
        } else {
            double end;
            if (x < m_LatLonSortedList[R]->GetMinX()) {
                end = m_LatLonSortedList[R]->GetMinLon();
            } else if (x > m_LatLonSortedList[R]->GetMaxX()) {
                end = m_LatLonSortedList[R]->GetMaxLon();
            } else {
                end = lon;
            }
            double dist = DistanceOnGlobe (lat, lon, m_LatLonSortedList[R]->GetLat(), end);
            if (rval == NULL || closest > dist 
                || (closest == dist 
                    && (rval->GetArea() > m_LatLonSortedList[R]->GetBlock()->GetArea()
                        || (rval->GetArea() == m_LatLonSortedList[R]->GetBlock()->GetArea()
                            && NStr::IsBlank(rval->GetLevel1())
                            && !NStr::IsBlank(m_LatLonSortedList[R]->GetBlock()->GetLevel1()))))) {
                rval = m_LatLonSortedList[R]->GetBlock();
                closest = dist;
            }
        }
        R++;
    }
    distance = closest;
    return rval;
}


bool CLatLonCountryMap::IsClosestToLatLon (string comp_country, double lat, double lon, double range, double &distance)
{
    int x = CCountryLine::ConvertLon(lon, m_Scale);
    int y = CCountryLine::ConvertLon(lat, m_Scale);

    int maxDelta = (int) (range * m_Scale + EPSILON);
    int min_y = y - maxDelta;
    int max_y = y + maxDelta;
    int min_x = x - maxDelta;
    int max_x = x + maxDelta;

    // binary search to lowest lat
    int R = x_GetLatStartIndex(min_y);

    string country = "";
    double closest = 0.0;
    int smallest_area = -1;

    while (R < m_LatLonSortedList.size() && m_LatLonSortedList[R]->GetY() <= max_y) {
        if (m_LatLonSortedList[R]->GetMaxX() < min_x || m_LatLonSortedList[R]->GetMinX() > max_x) {
            // out of range, don't bother calculating distance
        } else {
            double end;
            if (x < m_LatLonSortedList[R]->GetMinX()) {
                end = m_LatLonSortedList[R]->GetMinLon();
            } else {
                end = m_LatLonSortedList[R]->GetMaxLon();
            }
            double dist = DistanceOnGlobe (lat, lon, m_LatLonSortedList[R]->GetLat(), end);
            if (NStr::IsBlank (country) || closest > dist) {
                country = m_LatLonSortedList[R]->GetCountry();
                closest = dist;
                const CCountryExtreme * ext = x_FindCountryExtreme(country);
                if (ext) {
                    smallest_area = ext->GetArea();
                }
            } else if (closest == dist) {
                // if the distances are the same, prefer the input country, otherwise prefer the smaller region
                if (NStr::Equal(country, comp_country)) {
                    // keep country we're searching for
                } else if (!NStr::Equal(m_LatLonSortedList[R]->GetCountry(), country)) {
                    const CCountryExtreme * ext = x_FindCountryExtreme(m_LatLonSortedList[R]->GetCountry());
                    if (ext 
                        && (ext->GetArea() < smallest_area 
                            || NStr::Equal(m_LatLonSortedList[R]->GetCountry(), comp_country))) {
                        country = m_LatLonSortedList[R]->GetCountry();
                        smallest_area = ext->GetArea();
                    }
                }
            }
        }
        R++;
    }
    distance = closest;
    return NStr::Equal(country, comp_country);
}


CCountryExtreme *CLatLonCountryMap::IsNearLatLon (double lat, double lon, double range, double &distance, const string country, const string province)
{
    int x = CCountryLine::ConvertLon(lon, m_Scale);
    int y = CCountryLine::ConvertLat(lat, m_Scale);
    double closest = -1.0;
    int maxDelta = (int) (range * m_Scale + EPSILON);
    int min_y = y - maxDelta;
    int max_y = y + maxDelta;
    int min_x = x - maxDelta;
    int max_x = x + maxDelta;
    CCountryExtreme *ext = NULL;

    // binary search to lowest lat
    int R = x_GetLatStartIndex(min_y);

    while (R < m_LatLonSortedList.size() && m_LatLonSortedList[R]->GetY() <= max_y) {
        if (m_LatLonSortedList[R]->GetMaxX() < min_x || m_LatLonSortedList[R]->GetMinX() > max_x) {
            // out of range, don't bother calculating distance
        } else if (!NStr::EqualNocase(m_LatLonSortedList[R]->GetBlock()->GetLevel0(), country)) {
            // wrong country, skip
        } else if (!NStr::IsBlank(province) && !NStr::EqualNocase(m_LatLonSortedList[R]->GetBlock()->GetLevel1(), province)) {
            // wrong province, skip
        } else {
            double end;
            if (x < m_LatLonSortedList[R]->GetMinX()) {
                end = m_LatLonSortedList[R]->GetMinLon();
            } else if (x > m_LatLonSortedList[R]->GetMaxX()) {
                end = m_LatLonSortedList[R]->GetMaxLon();
            } else {
                end = lon;
            }
            double dist = DistanceOnGlobe (lat, lon, m_LatLonSortedList[R]->GetLat(), end);
            if (closest < 0.0 ||  closest > dist) { 
                closest = dist;
                ext = m_LatLonSortedList[R]->GetBlock();
            }
        }
        R++;
    }
    distance = closest;
    return ext;
}





bool CLatLonCountryMap::DoCountryBoxesOverlap(string country1, string country2)
{
    if (NStr::IsBlank (country1) || NStr::IsBlank(country2)) return false;

    const CCountryExtreme *ext1 = x_FindCountryExtreme (country1);
    if (!ext1) {
        return false;
    }
    const CCountryExtreme *ext2 = x_FindCountryExtreme (country2);
    if (!ext2) {
        return false;
    }


    return ext1->DoesOverlap(ext2);
}


int CLatLonCountryMap::AdjustAndRoundDistance (double distance, double scale)

{
  if (scale < 1.1) {
    distance += 111.19;
  } else if (scale > 19.5 && scale < 20.5) {
    distance += 5.56;
  } else if (scale > 99.5 && scale < 100.5) {
    distance += 1.11;
  }

  return (int) (distance + 0.5);
}


int CLatLonCountryMap::AdjustAndRoundDistance (double distance)

{
  return AdjustAndRoundDistance (distance, m_Scale);
}


// ===== for validating instituation and collection codes in specimen-voucher, ================
// ===== biomaterial, and culture-collection BioSource subsource modifiers     ================

typedef map<string, string, PNocase> TInstitutionCodeMap;
static TInstitutionCodeMap s_BiomaterialInstitutionCodeMap;
static TInstitutionCodeMap s_SpecimenVoucherInstitutionCodeMap;
static TInstitutionCodeMap s_CultureCollectionInstitutionCodeMap;
static TInstitutionCodeMap s_InstitutionCodeTypeMap;
static bool                    s_InstitutionCollectionCodeMapInitialized = false;
DEFINE_STATIC_FAST_MUTEX(s_InstitutionCollectionCodeMutex);

#include "institution_codes.inc"

static void s_ProcessInstitutionCollectionCodeLine(const CTempString& line)
{
    vector<string> tokens;
    NStr::Tokenize(line, "\t", tokens);
    if (tokens.size() != 3) {
        ERR_POST_X(1, Warning << "Bad format in institution_codes.txt entry " << line
                   << "; disregarding");
    } else {
        switch (tokens[1].c_str()[0]) {
            case 'b':
                s_BiomaterialInstitutionCodeMap[tokens[0]] = tokens[2];
                break;
            case 'c':
                s_CultureCollectionInstitutionCodeMap[tokens[0]] = tokens[2];
                break;
            case 's':
                s_SpecimenVoucherInstitutionCodeMap[tokens[0]] = tokens[2];
                break;
            default:
                ERR_POST_X(1, Warning << "Bad format in institution_codes.txt entry " << line
                           << "; unrecognized subtype (" << tokens[1] << "); disregarding");
                break;
        }
        s_InstitutionCodeTypeMap[tokens[0]] = tokens[1];
    }
}


static void s_InitializeInstitutionCollectionCodeMaps(void)
{
    CFastMutexGuard GUARD(s_InstitutionCollectionCodeMutex);
    if (s_InstitutionCollectionCodeMapInitialized) {
        return;
    }
    string file = g_FindDataFile("institution_codes.txt");
    CRef<ILineReader> lr;
    if ( !file.empty() ) {
        try {
            lr = ILineReader::New(file);
        } NCBI_CATCH("s_InitializeInstitutionCollectionCodeMaps")
    }

    if (lr.Empty()) {
        ERR_POST_X(2, Info << "s_InitializeInstitutionCollectionCodeMaps: "
                   "falling back on built-in data.");
        size_t num_codes = sizeof (kInstitutionCollectionCodeList) / sizeof (char *);
        for (size_t i = 0; i < num_codes; i++) {
            const char *p = kInstitutionCollectionCodeList[i];
            s_ProcessInstitutionCollectionCodeLine(p);
        }
    } else {
        do {
            s_ProcessInstitutionCollectionCodeLine(*++*lr);
        } while ( !lr->AtEOF() );
    }

    s_InstitutionCollectionCodeMapInitialized = true;
}


void CValidError_imp::ValidateOrgModVoucher(const COrgMod& orgmod, const CSerialObject& obj, const CSeq_entry *ctx)
{
    if (!orgmod.IsSetSubtype() || !orgmod.IsSetSubname() || NStr::IsBlank(orgmod.GetSubname())) {
        return;
    }

    int subtype = orgmod.GetSubtype();
    string val = orgmod.GetSubname();

    if (NStr::Find(val, ":") == string::npos) {
      if (subtype == COrgMod::eSubtype_culture_collection) {
            PostObjErr(eDiag_Error, eErr_SEQ_DESCR_UnstructuredVoucher, 
                       "Culture_collection should be structured, but is not",
                       obj, ctx);
      }
        return;
    }

    string inst_code = "";
    string coll_code = "";
    string inst_coll = "";
    string id = "";
    if (!COrgMod::ParseStructuredVoucher(val, inst_code, coll_code, id)) {
        if (NStr::IsBlank(inst_code)) {
            PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadInstitutionCode, 
                        "Voucher is missing institution code",
                        obj, ctx);
        }
        if (NStr::IsBlank(id)) {
            PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadVoucherID, 
                        "Voucher is missing specific identifier",
                        obj, ctx);
        }
        return;
    }

    if (NStr::IsBlank (coll_code)) {
        inst_coll = inst_code;
    } else {
        inst_coll = inst_code + ":" + coll_code;
    }    

    s_InitializeInstitutionCollectionCodeMaps();

    // first, check combination of institution and collection (if collection found)
    TInstitutionCodeMap::iterator it = s_InstitutionCodeTypeMap.find(inst_coll);
    if (it != s_InstitutionCodeTypeMap.end()) {
        if (NStr::EqualCase (it->first, inst_coll)) {
            char type_ch = it->second.c_str()[0];
            if ((subtype == COrgMod::eSubtype_bio_material && type_ch != 'b') ||
                (subtype == COrgMod::eSubtype_culture_collection && type_ch != 'c') ||
                (subtype == COrgMod::eSubtype_specimen_voucher && type_ch != 's')) {
                if (type_ch == 'b') {
                    PostObjErr (eDiag_Info, eErr_SEQ_DESCR_WrongVoucherType, 
                                "Institution code " + inst_coll + " should be bio_material", 
                                obj, ctx);
                } else if (type_ch == 'c') {
                    PostObjErr (eDiag_Info, eErr_SEQ_DESCR_WrongVoucherType, 
                                "Institution code " + inst_coll + " should be culture_collection",
                                obj, ctx);
                } else if (type_ch == 's') {
                    PostObjErr (eDiag_Info, eErr_SEQ_DESCR_WrongVoucherType, 
                                "Institution code " + inst_coll + " should be specimen_voucher",
                                obj, ctx);
                }
            }
            return;
        } else if (NStr::EqualNocase(it->first, inst_coll)) {
            PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadInstitutionCode, 
                        "Institution code " + inst_coll + " exists, but correct capitalization is " + it->first,
                        obj, ctx);
            return;
        } 
    } else if (NStr::StartsWith(inst_coll, "personal", NStr::eNocase)) {
        // ignore personal collections    
        return;
    } else {
        // check for partial match
        bool found = false;
        if (NStr::Find(inst_coll, "<") == string::npos) {
            string check = inst_coll + "<";
            it = s_InstitutionCodeTypeMap.begin();
            while (!found && it != s_InstitutionCodeTypeMap.end()) {
                if (NStr::StartsWith(it->first, check, NStr::eNocase)) {
                    found = true;
                    break;
                }
                ++it;
            }
        }
        if (found) {
            PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadInstitutionCode, 
                        "Institution code " + inst_coll + " needs to be qualified with a <COUNTRY> designation",
                        obj, ctx);
            return;
        } else if (NStr::IsBlank(coll_code)) {
            PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadInstitutionCode, 
                        "Institution code " + inst_coll + " is not in list",
                        obj, ctx);
            return;
        } else {
            it = s_InstitutionCodeTypeMap.find(inst_code);
            if (it != s_InstitutionCodeTypeMap.end()) {
                if (NStr::Equal (coll_code, "DNA")) {
                    // DNA is a valid collection for any institution (using bio_material)
                    if (it->second.c_str()[0] != 'b') {
                        PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_WrongVoucherType, 
                                    "DNA should be bio_material",
                                    obj, ctx);
                    }
                } else {
                    PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadCollectionCode, 
                                "Institution code " + inst_code + " exists, but collection "
                                + inst_coll + " is not in list",
                                obj, ctx);
                }
                return;
            } else {
                found = false;
                if (NStr::Find(inst_code, "<") == string::npos) {
                    string check = inst_code + "<";
                    it = s_InstitutionCodeTypeMap.begin();
                    while (!found && it != s_InstitutionCodeTypeMap.end()) {
                        if (NStr::StartsWith(it->first, check, NStr::eNocase)) {
                            found = true;
                            break;
                        }
                        ++it;
                    }
                }
                if (found) {
                    PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadInstitutionCode, 
                                "Institution code in " + inst_coll + " needs to be qualified with a <COUNTRY> designation",
                                obj, ctx);
                } else {
                    PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BadInstitutionCode, 
                                "Institution code " + inst_coll + " is not in list",
                                obj, ctx);
                }
            }
        }
    }    
}



END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
