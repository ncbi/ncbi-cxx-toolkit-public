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


// CCountryBlock
CCountryBlock::CCountryBlock 
(string country_name, double min_x, double min_y, double max_x, double max_y)
: m_CountryName(country_name) ,
  m_MinX(min_x) ,
  m_MinY(min_y) ,
  m_MaxX(max_x) ,
  m_MaxY(max_y)
{
}


CCountryBlock::~CCountryBlock (void)
{
}

bool CCountryBlock::IsLatLonInCountryBlock (double x, double y)
{
    if (m_MinX <= x && m_MaxX >= x && m_MinY <= y && m_MaxY >= y) {
        return true;
    } else {
        return false;
    }
}


bool CCountryBlock::DoesOverlap(const CCountryBlock* other_block) const
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


static const char * s_DefaultCountryLatLonText[] = {
  "Afghanistan    AF    60.4    29.3    74.9    38.5",
  "Albania    AL    19.2    39.6    21.1    42.7",
  "Algeria    AG    -8.7    18.9    12.0    37.1",
  "American Samoa    AQ    -171.1    -11.1    -171.1    -11.0    -170.9    -14.4    -169.4    -14.2",
  "Andorra    AN    1.4    42.4    1.8    42.7",
  "Angola    AO    11.6    -18.1    24.1    -4.4",
  "Anguilla    AV    -63.2    18.1    -62.9    18.3",
  "Antarctica    AY    ",
  "Antigua and Barbuda    AC    -62.4    16.9    -62.3    16.9    -62.0    16.9    -61.7    17.7",
  "Arctic Ocean    XX    ",
  "Argentina    AR    -73.6    -55.1    -53.6    -21.8",
  "Armenia    AM    43.4    38.8    46.6    41.3",
  "Aruba    AA    -70.1    12.4    -69.8    12.7",
  "Ashmore and Cartier Islands    AT    122.9    -12.3    123.1    -12.1",
  "Atlantic Ocean    XX    ",
  "Australia    AS    112.9    -43.7    153.6    -10.0",
  "Australia: Australian Capital Territory    XX    148.7    -36.0    149.4    -35.1",
  "Australia: Jervis Bay Territory    XX    150.5    -35.2    150.8    -35.1",
  "Australia: New South Wales    XX    140.9    -37.6    153.6    -28.2",
  "Australia: Northern Territory    XX    128.9    -26.1    138.0    -10.9",
  "Australia: Queensland    XX    137.9    -29.2    153.6    -10.0",
  "Australia: South Australia    XX    128.9    -38.1    141.0    -26.0",
  "Australia: Tasmania    XX    143.8    -43.7    148.5    -39.6",
  "Australia: Victoria    XX    140.9    -39.6    150.0    -34.0",
  "Australia: Western Australia    XX    112.9    -35.2    129.0    -13.7",
  "Austria    AU    9.5    46.3    17.2    49.0",
  "Azerbaijan    AJ    45.0    38.3    50.6    41.9",
  "Bahamas    BF    -79.7    20.9    -72.7    27.2",
  "Bahrain    BA    50.3    25.7    50.7    26.3",
  "Baker Island    FQ    -176.5    0.1    -176.5    0.2",
  "Bangladesh    BG    88.0    20.5    92.7    26.6",
  "Barbados    BB    -59.7    13.0    -59.4    13.3",
  "Bassas da India    BS    39.6    -21.6    39.8    -21.4",
  "Belarus    BO    23.1    51.2    32.8    56.2",
  "Belgium    BE    2.5    49.4    6.4    51.5",
  "Belize    BH    -89.3    15.8    -86.9    18.5",
  "Benin    BN    0.7    6.2    3.9    12.4",
  "Bermuda    BD    -64.9    32.2    -64.7    32.4",
  "Bhutan    BT    88.7    26.7    92.1    28.3",
  "Bolivia    BL    -69.7    -22.9    -57.5    -9.7",
  "Bosnia and Herzegovina    BK    15.7    42.5    19.7    45.3",
  "Botswana    BC    19.9    -27.0    29.4    -17.8",
  "Bouvet Island    BV    3.3    -54.5    3.5    -54.4",
  "Brazil    BR    -74.0    -33.8    -34.8    5.0",
  "British Virgin Islands    VI    -64.8    18.2    -63.2    18.8",
  "Brunei    BX    114.0    4.0    115.4    5.0",
  "Bulgaria    BU    22.3    41.2    28.6    44.2",
  "Burkina Faso    UV    -5.6    9.4    2.4    15.1",
  "Burundi    BY    28.9    -4.5    30.8    -2.3",
  "Cambodia    CB    102.3    9.2    107.6    14.7",
  "Cameroon    CM    8.4    1.6    16.2    13.1",
  "Canada    CA    -141.0    41.7    -52.6    83.1",
  "Canada: Alberta    XX    -120.0    48.9    -110.0    60.0",
  "Canada: British Columbia    XX    -139.1    48.3    -114.1    60.0",
  "Canada: Manitoba    XX    -102.1    48.9    -89.0    60.0",
  "Canada: New Brunswick    XX    -69.1    44.5    -63.8    48.1",
  "Canada: Newfoundland and Labrador    XX    -67.9    46.6    -52.6    60.4",
  "Canada: Northwest Territories    XX    -136.5    60.0    -102.0    78.8",
  "Canada: Nova Scotia    XX    -66.4    43.3    -59.7    47.0",
  "Canada: Nunavut    XX    -120.4    60.0    -61.2    83.1",
  "Canada: Ontario    XX    -95.2    41.6    -74.3    56.9",
  "Canada: Prince Edward Island    XX    -64.5    45.9    -62.0    47.1",
  "Canada: Quebec    XX    -79.8    45.0    -57.1    62.6",
  "Canada: Saskatchewan    XX    -110.0    48.9    -101.4    60.0",
  "Canada: Yukon    XX    -141.0    60.0    -124.0    69.6",
  "Cape Verde    CV    -25.4    14.8    -22.7    17.2",
  "Cayman Islands    CJ    -81.5    19.2    -81.1    19.4    -80.2    19.6    -79.7    19.8",
  "Central African Republic    CT    14.4    2.2    27.5    11.0",
  "Chad    CD    13.4    7.4    24.0    23.5",
  "Chile    CI    -75.8    -56.0    -66.4    -17.5",
  "China    CH    73.5    20.2    134.8    53.6    108.6    18.1    111.1    20.2",
  "China: Hainan    XX    108.6    18.1    111.1    20.2",
  "Christmas Island    KT    105.5    -10.6    105.7    -10.4",
  "Clipperton Island    IP    -109.3    10.2    -109.2    10.3",
  "Cocos Islands    CK    96.8    -12.2    96.9    -11.8",
  "Colombia    CO    -79.1    -4.3    -66.9    12.5",
  "Comoros    CN    43.2    -12.5    44.5    -11.4",
  "Cook Islands    CW    -159.9    -22.0    -157.3    -18.8",
  "Coral Sea Islands    CR    ",
  "Costa Rica    CS    -87.1    5.4    -87.0    5.6    -86.0    8.0    -82.6    11.2",
  "Cote d'Ivoire    IV    -8.6    4.3    -2.5    10.7",
  "Croatia    HR    13.4    42.3    19.4    46.5",
  "Cuba    CU    -85.0    19.8    -74.1    23.3",
  "Cyprus    CY    32.2    34.5    34.6    35.7",
  "Czech Republic    EZ    12.0    48.5    18.9    51.0",
  "Democratic Republic of the Congo    CG    12.2    -13.5    31.3    5.4",
  "Denmark    DA    8.0    54.5    12.7    57.7    14.6    54.9    15.2    55.3",
  "Djibouti    DJ    41.7    10.9    43.4    12.7",
  "Dominica    DO    -61.5    15.2    -61.2    15.6",
  "Dominican Republic    DR    -72.1    17.4    -68.3    19.9",
  "East Timor    TT    124.9    -9.5    127.4    -8.3",
  "Ecuador    EC    -92.1    -1.5    -89.2    1.7    -81.1    -5.0    -75.2    1.4",
  "Ecuador: Galapagos    XX    -92.1    -1.5    -89.2    1.7",
  "Egypt    EG    24.6    21.7    35.8    31.7",
  "El Salvador    ES    -90.2    13.1    -87.7    14.4",
  "Equatorial Guinea    EK    8.4    3.2    8.9    3.8    9.2    0.8    11.3    2.3",
  "Eritrea    ER    36.4    12.3    43.1    18.0",
  "Estonia    EN    21.7    57.5    28.2    59.7",
  "Ethiopia    ET    32.9    3.4    48.0    14.9",
  "Europa Island    EU    40.3    -22.4    40.4    -22.3",
  "Falkland Islands (Islas Malvinas)    FK    -61.4    -53.0    -57.7    -51.0",
  "Faroe Islands    FO    -7.7    61.3    -6.3    62.4",
  "Fiji    FJ    -180.0    -20.7    -178.2    -15.7    -175.7    -19.8    -175.0    -15.6    176.8    -19.3    180.0    -12.5",
  "Finland    FI    19.3    59.7    31.6    70.1",
  "France    FR    -5.2    42.3    8.2    51.1    8.5    41.3    9.6    43.1",
  "France: Corsica    XX    8.5    41.3    9.6    43.1",
  "French Guiana    FG    -54.6    2.1    -51.6    5.8",
  "French Polynesia    FP    -154.7    -27.7    -134.9    -7.8",
  "French Southern and Antarctic Lands    FS    68.6    -49.8    70.6    -48.5",
  "Gabon    GB    8.6    -4.0    14.5    2.3",
  "Gambia    GA    -16.9    13.0    -13.8    13.8",
  "Gaza Strip    GZ    34.2    31.2    34.5    31.6",
  "Georgia    GG    40.0    41.0    46.7    43.6",
  "Germany    GM    5.8    47.2    15.0    55.1",
  "Ghana    GH    -3.3    4.7    1.2    11.2",
  "Gibraltar    GI    -5.4    36.1    -5.3    36.2",
  "Glorioso Islands    GO    47.2    -11.6    47.4    -11.5",
  "Greece    GR    19.3    34.8    28.2    41.8",
  "Greenland    GL    -73.3    59.7    -11.3    83.6",
  "Grenada    GJ    -61.8    11.9    -61.6    12.3",
  "Guadeloupe    GP    -63.2    17.8    -62.8    18.1    -61.9    15.8    -61.0    16.5",
  "Guam    GQ    144.6    13.2    145.0    13.7",
  "Guatemala    GT    -92.3    13.7    -88.2    17.8",
  "Guernsey    GK    -2.7    49.4    -2.4    49.5",
  "Guinea    GV    -15.1    7.1    -7.6    12.7",
  "Guinea-Bissau    PU    -16.8    10.8    -13.6    12.7",
  "Guyana    GY    -61.4    1.1    -56.5    8.6",
  "Haiti    HA    -74.5    18.0    -71.6    20.1",
  "Heard Island and McDonald Islands    HM    73.2    -53.2    73.7    -52.9",
  "Honduras    HO    -89.4    12.9    -83.2    16.5",
  "Hong Kong    HK    113.8    22.1    114.4    22.6",
  "Howland Island    HQ    -176.7    0.7    -176.6    0.8",
  "Hungary    HU    16.1    45.7    22.9    48.6",
  "Iceland    IC    -24.6    63.2    -13.5    66.6",
  "India    IN    67.3    8.0    97.4    35.5",
  "Indian Ocean    XX    ",
  "Indonesia    ID    95.0    -11.1    141.0    5.9",
  "Iran    IR    44.0    25.0    63.3    39.8",
  "Iraq    IZ    38.8    29.1    48.6    37.4",
  "Ireland    EI    -10.7    51.4    -6.0    55.4",
  "Isle of Man    IM    -4.9    54.0    -4.3    54.4",
  "Israel    IS    34.2    29.4    35.7    33.3",
  "Italy    IT    6.6    35.4    18.5    47.1",
  "Jamaica    JM    -78.4    17.7    -76.2    18.5",
  "Jan Mayen    JN    -9.1    70.8    -7.9    71.2",
  "Japan    JA    122.9    24.0    125.5    25.9    126.7    20.5    145.8    45.5",
  "Jarvis Island    DQ    -160.1    -0.4    -160.0    -0.4",
  "Jersey    JE    -2.3    49.1    -2.0    49.3",
  "Johnston Atoll    JQ    -169.6    16.7    -169.4    16.8",
  "Jordan    JO    34.9    29.1    39.3    33.4",
  "Juan de Nova Island    JU    42.6    -17.1    42.8    -16.8",
  "Kazakhstan    KZ    46.4    40.9    87.3    55.4",
  "Kenya    KE    33.9    -4.7    41.9    4.6",
  "Kerguelen Archipelago    XX    ",
  "Kingman Reef    KQ    -162.9    6.1    -162.4    6.7",
  "Kiribati    KR    172.6    0.1    173.9    3.4    174.2    -2.7    176.9    -0.5",
  "Kosovo    KV    20.0    41.8    43.3    21.9",
  "Kuwait    KU    46.5    28.5    48.4    30.1",
  "Kyrgyzstan    KG    69.2    39.1    80.3    43.2",
  "Laos    LA    100.0    13.9    107.7    22.5",
  "Latvia    LG    20.9    55.6    28.2    58.1",
  "Lebanon    LE    35.1    33.0    36.6    34.7",
  "Lesotho    LT    27.0    -30.7    29.5    -28.6",
  "Liberia    LI    -11.5    4.3    -7.4    8.6",
  "Libya    LY    9.3    19.5    25.2    33.2",
  "Liechtenstein    LS    9.4    47.0    9.6    47.3",
  "Lithuania    LH    20.9    53.9    26.9    56.4",
  "Luxembourg    LU    5.7    49.4    6.5    50.2",
  "Macau    MC    113.5    22.1    113.6    22.2",
  "Macedonia    MK    20.4    40.8    23.0    42.4",
  "Madagascar    MA    43.1    -25.7    50.5    -11.9",
  "Malawi    MI    32.6    -17.2    35.9    -9.4",
  "Malaysia    MY    98.9    5.6    98.9    5.7    99.6    1.2    104.5    6.7    109.5    0.8    119.3    7.4",
  "Maldives    MV    72.6    -0.7    73.7    7.1",
  "Mali    ML    -12.3    10.1    4.2    25.0",
  "Malta    MT    14.1    35.8    14.6    36.1",
  "Marshall Islands    RM    160.7    4.5    172.0    14.8",
  "Martinique    MB    -61.3    14.3    -60.8    14.9",
  "Mauritania    MR    -17.1    14.7    -4.8    27.3",
  "Mauritius    MP    57.3    -20.6    57.8    -20.0    59.5    -16.9    59.6    -16.7",
  "Mayotte    MF    45.0    -13.1    45.3    -12.6",
  "Mediterranean Sea    XX  ",
  "Mexico    MX    -118.5    28.8    -118.3    29.2    -117.3    14.5    -86.7    32.7",
  "Micronesia    FM    138.0    9.4    138.2    9.6    139.6    9.8    139.8    10.0    140.5    9.7    140.5    9.8    147.0    7.3    147.0    7.4    149.3    6.6    149.3    6.7    151.5    7.1    152.0    7.5    153.5    5.2    153.8    5.6    157.1    5.7    160.7    7.1    162.9    5.2    163.0    5.4",
  "Midway Islands    MQ    -178.4    28.3    -178.3    28.4    -177.4    28.1    -177.3    28.2    -174.0    26.0    -174.0    26.1    -171.8    25.7    -171.7    25.8",
  "Moldova    MD    26.6    45.4    30.2    48.5",
  "Monaco    MN    7.3    43.7    7.5    43.8",
  "Mongolia    MG    87.7    41.5    119.9    52.2",
  "Montenegro    MJ    18.4    42.2    20.4    43.6",
  "Montserrat    MH    -62.3    16.6    -62.1    16.8",
  "Morocco    MO    -13.2    27.6    -1.0    35.9",
  "Mozambique    MZ    30.2    -26.9    40.8    -10.5",
  "Myanmar    BM    92.1    9.6    101.2    28.5",
  "Namibia    WA    11.7    -29.0    25.3    -17.0",
  "Nauru    NR    166.8    -0.6    166.9    -0.5",
  "Navassa Island    BQ    -75.1    18.3    -75.0    18.4",
  "Nepal    NP    80.0    26.3    88.2    30.4",
  "Netherlands    NL    3.3    50.7    7.2    53.6",
  "Netherlands Antilles    NT    -69.2    11.9    -68.2    12.4    -63.3    17.4    -62.9    18.1",
  "New Caledonia    NC    163.5    -22.8    169.0    -19.5",
  "New Zealand    NZ    166.4    -48.1    178.6    -34.1",
  "Nicaragua    NU    -87.7    10.7    -82.6    15.0",
  "Niger    NG    0.1    11.6    16.0    23.5",
  "Nigeria    NI    2.6    4.2    14.7    13.9",
  "Niue    NE    -170.0    -19.2    -169.8    -19.0",
  "Norfolk Island    NF    168.0    -29.2    168.1    -29.0",
  "North Korea    KN    124.1    37.5    130.7    43.0",
  "North Sea    XX    ",
  "Northern Mariana Islands    CQ    144.8    14.1    146.1    20.6",
  "Norway    NO    4.6    57.9    31.1    71.2",
  "Oman    MU    51.8    16.6    59.8    25.0",
  "Pacific Ocean    XX    ",
  "Pakistan    PK    60.8    23.6    77.8    37.1",
  "Palau    PS    132.3    4.3    132.3    4.3    134.1    6.8    134.7    7.7",
  "Palmyra Atoll    LQ    -162.2    5.8    -162.0    5.9",
  "Panama    PM    -83.1    7.1    -77.2    9.6",
  "Papua New Guinea    PP    140.8    -11.7    156.0    -0.9    157.0    -4.9    157.1    -4.8    159.4    -4.7    159.5    -4.5",
  "Paracel Islands    PF    111.1    15.7    111.2    15.8",
  "Paraguay    PA    -62.7    -27.7    -54.3    -19.3",
  "Peru    PE    -81.4    -18.4    -68.7    0.0",
  "Philippines    RP    116.9    4.9    126.6    21.1",
  "Pitcairn Islands    PC    -128.4    -24.5    -128.3    -24.3",
  "Poland    PL    14.1    49.0    24.2    54.8",
  "Portugal    PO    -9.5    36.9    -6.2    42.1    -31.3    36.9    -25.0    39.8    -17.3    32.4    -16.2    33.2",
  "Portugal: Azores    XX    -31.3    36.9    -25.0    39.8",
  "Portugal: Madeira    XX    -17.3    32.4    -16.2    33.2",
  "Puerto Rico    RQ    -68.0    17.8    -65.2    18.5",
  "Qatar    QA    50.7    24.4    52.4    26.2",
  "Republic of the Congo    CF    11.2    -5.1    18.6    3.7",
  "Reunion    RE    55.2    -21.4    55.8    -20.9",
  "Romania    RO    20.2    43.6    29.7    48.3",
  "Russia    RS    -180.0    64.2    -169.0    71.6    19.7    54.3    22.9    55.3    26.9    41.1    180.0    81.3",
  "Rwanda    RW    28.8    -2.9    30.9    -1.1",
  "Saint Helena    SH    -5.8    -16.1    -5.6    -15.9",
  "Saint Kitts and Nevis    SC    62.9    17.0    62.5    17.5",
  "Saint Lucia    ST    -61.1    13.7    -60.9    14.1",
  "Saint Pierre and Miquelon    SB    -56.5    46.7    -56.2    47.1",
  "Saint Vincent and the Grenadines    VC    -61.6    12.4    -61.1    13.4",
  "Samoa    WS    -172.8    -14.1    -171.4    -13.4",
  "San Marino    SM    12.4    43.8    12.5    44.0",
  "Sao Tome and Principe    TP    6.4    0.0    1.7    7.5",
  "Saudi Arabia    SA    34.4    15.6    55.7    32.2",
  "Senegal    SG    -17.6    12.3    -11.4    16.7",
  "Serbia    RB    18.8    42.2    23.1    46.2",
  "Seychelles    SE    50.7    -9.6    51.1    -9.2    52.7    -7.2    52.8    -7.0    53.0    -6.3    53.7    -5.1    55.2    -5.9    56.0    -3.7    56.2    -7.2    56.3    -7.1",
  "Sierra Leone    SL    -13.4    6.9    -10.3    10.0",
  "Singapore    SN    103.6    1.1    104.1    1.5",
  "Slovakia    LO    16.8    47.7    22.6    49.6",
  "Slovenia    SI    13.3    45.4    16.6    46.9",
  "Solomon Islands    BP    155.5    -11.9    162.8    -5.1    165.6    -11.8    167.0    -10.1    167.1    -10.0    167.3    -9.8    168.8    -12.3    168.8    -12.3",
  "Somalia    SO    40.9    -1.7    51.4    12.0",
  "South Africa    SF    16.4    -34.9    32.9    -22.1",
  "South Georgia and the South Sandwich Islands    SX    -38.3    -54.9    -35.7    -53.9",
  "South Korea    KS    125.0    33.1    129.6    38.6",
  "Spain    SP    -9.3    35.1    4.3    43.8    -18.2    27.6    -13.4    29.5",
  "Spain: Canary Islands    XX    -18.2    27.6    -13.4    29.5",
  "Spratly Islands    PG    114.0    9.6    115.8    11.1",
  "Sri Lanka    CE    79.6    5.9    81.9    9.8",
  "Sudan    SU    21.8    3.4    38.6    23.6",
  "Suriname    NS    -58.1    1.8    -54.0    6.0",
  "Svalbard    SV    10.4    76.4    33.5    80.8",
  "Swaziland    WZ    30.7    -27.4    32.1    -25.7",
  "Sweden    SW    10.9    55.3    24.2    69.1",
  "Switzerland    SZ    5.9    45.8    10.5    47.8",
  "Syria    SY    35.7    32.3    42.4    37.3",
  "Taiwan    TW    119.3    21.9    122.0    25.3",
  "Tajikistan    TI    67.3    36.6    75.1    41.0",
  "Tanzania    TZ    29.3    -11.8    40.4    -1.0",
  "Thailand    TH    97.3    5.6    105.6    20.5",
  "Togo    TO    -0.2    6.1    1.8    11.1",
  "Tokelau    TL    -172.6    -9.5    -171.1    -8.5",
  "Tonga    TN    -176.3    -22.4    -176.2    -22.3    -175.5    -21.5    -174.5    -20.0",
  "Trinidad and Tobago    TD    -62.0    10.0    -60.5    11.3",
  "Tromelin Island    TE    54.5    -15.9    54.5    -15.9",
  "Tunisia    TS    7.5    30.2    11.6    37.5",
  "Turkey    TU    25.6    35.8    44.8    42.1",
  "Turkmenistan    TX    52.4    35.1    66.7    42.8",
  "Turks and Caicos Islands    TK    -73.8    20.9    -73.0    21.3",
  "Tuvalu    TV    176.0    -7.3    177.3    -5.6    178.4    -8.0    178.7    -7.4    179.0    -9.5    179.9    -8.5",
  "Uganda    UG    29.5    -1.5    35.0    4.2",
  "Ukraine    UP    22.1    44.3    40.2    52.4",
  "United Arab Emirates    AE    51.1    22.4    56.4    26.1",
  "United Kingdom    UK    -8.7    49.7    1.8    60.8",
  "Uruguay    UY    -58.5    -35.0    -53.1    -30.1",
  "USA    US    -124.8    24.5    -66.9    49.4    -168.2    54.3    -130.0    71.4    172.4    52.3    176.0    53.0    177.2    51.3    179.8    52.1    -179.5    51.0    -172.0    52.5    -171.5    52.0    -164.5    54.5    -164.8    23.5    -164.7    23.6    -162.0    23.0    -161.9    23.1    -160.6    18.9    -154.8    22.2",
  "USA: Alabama    XX    -88.8    30.1    -84.9    35.0",
  "USA: Alaska    XX    -168.2    54.3    -130.0    71.4    172.4    52.3    176.0    53.0    177.2    51.3    179.8    52.1    -179.5    51.0    -172.0    52.5    -171.5    52.0    -164.5    54.5",
  "USA: Alaska, Aleutian Islands    XX    172.4    52.3    176.0    53.0    177.2    51.3    179.8    52.1    -179.5    51.0    -172.0    52.5    -171.5    52.0    -164.5    54.5",
  "USA: Arizona    XX    -114.9    31.3    -109.0    37.0",
  "USA: Arkansas    XX    -94.7    33.0    -89.6    36.5",
  "USA: California    XX    -124.5    32.5    -114.1    42.0",
  "USA: Colorado    XX    -109.1    36.9    -102.0    41.0",
  "USA: Connecticut    XX    -73.8    40.9    -71.8    42.1",
  "USA: Delaware    XX    -75.8    38.4    -74.9    39.8",
  "USA: Florida    XX    -87.7    24.5    -80.0    31.0",
  "USA: Georgia    XX    -85.7    30.3    -80.8    35.0",
  "USA: Hawaii    XX    -164.8    23.5    -164.7    23.6    -162.0    23.0    -161.9    23.1    -160.6    18.9    -154.8    22.2",
  "USA: Idaho    XX    -117.3    41.9    -111.0    49.0",
  "USA: Illinois    XX    -91.6    36.9    -87.0    42.5",
  "USA: Indiana    XX    -88.1    37.7    -84.8    41.8",
  "USA: Iowa    XX    -96.7    40.3    -90.1    43.5",
  "USA: Kansas    XX    -102.1    36.9    -94.6    40.0",
  "USA: Kentucky    XX    -89.5    36.5    -82.0    39.1",
  "USA: Louisiana    XX    -94.1    28.9    -88.8    33.0",
  "USA: Maine    XX    -71.1    43.0    -66.9    47.5",
  "USA: Maryland    XX    -79.5    37.8    -75.1    39.7",
  "USA: Massachusetts    XX    -73.6    41.2    -69.9    42.9",
  "USA: Michigan    XX    -90.5    41.6    -82.1    48.3",
  "USA: Minnesota    XX    -97.3    43.4    -90.0    49.4",
  "USA: Mississippi    XX    -91.7    30.1    -88.1    35.0",
  "USA: Missouri    XX    -95.8    36.0    -89.1    40.6",
  "USA: Montana    XX    -116.1    44.3    -104.0    49.0",
  "USA: Nebraska    XX    -104.1    40.0    -95.3    43.0",
  "USA: Nevada    XX    -120.0    35.0    -114.0    42.0",
  "USA: New Hampshire    XX    -72.6    42.6    -70.7    45.3",
  "USA: New Jersey    XX    -75.6    38.9    -73.9    41.4",
  "USA: New Mexico    XX    -109.1    31.3    -103.0    37.0",
  "USA: New York    XX    -79.8    40.4    -71.9    45.0",
  "USA: North Carolina    XX    -84.4    33.8    -75.5    36.6",
  "USA: North Dakota    XX    -104.1    45.9    -96.6    49.0",
  "USA: Ohio    XX    -84.9    38.3    -80.5    42.3",
  "USA: Oklahoma    XX    -103.1    33.6    -94.4    37.0",
  "USA: Oregon    XX    -124.6    41.9    -116.5    46.3",
  "USA: Pennsylvania    XX    -80.6    39.7    -74.7    42.5",
  "USA: Rhode Island    XX    -71.9    41.1    -71.1    42.0",
  "USA: South Carolina    XX    -83.4    32.0    -78.6    35.2",
  "USA: South Dakota    XX    -104.1    42.4    -96.4    45.9",
  "USA: Tennessee    XX    -90.4    35.0    -81.7    36.7",
  "USA: Texas    XX    -106.7    25.8    -93.5    36.5",
  "USA: Utah    XX    -114.1    37.0    -109.1    42.0",
  "USA: Vermont    XX    -73.5    42.7    -71.5    45.0",
  "USA: Virginia    XX    -83.7    36.5    -75.2    39.5",
  "USA: Washington    XX    -124.8    45.5    -116.9    49.0",
  "USA: West Virginia    XX    -82.7    37.1    -77.7    40.6",
  "USA: Wisconsin    XX    -92.9    42.4    -86.3    47.3",
  "USA: Wyoming    XX    -111.1    40.9    -104.1    45.0",
  "Uzbekistan    UZ    55.9    37.1    73.1    45.6",
  "Vanuatu    NH    166.5    -20.3    170.2    -13.1",
  "Venezuela    VE    -73.4    0.7    -59.8    12.2",
  "Viet Nam    VM    102.1    8.4    109.5    23.4",
  "Virgin Islands    VQ    -65.1    17.6    -64.6    18.5",
  "Wake Island    WQ    166.5    19.2    166.7    19.3",
  "Wallis and Futuna    WF    -178.3    -14.4    -178.0    -14.2    -176.3    -13.4    -176.1    -13.2",
  "West Bank    WE    34.8    31.3    35.6    32.6",
  "Western Sahara    WI    -17.2    20.7    -8.7    27.7",
  "Yemen    YM    41.8    11.7    54.5    19.0",
  "Zambia    ZA    21.9    -18.1    33.7    -8.2",
  "Zimbabwe    ZI    25.2    -22.5    33.1    -15.6",
};


static const int k_NumCountryLatLonText = sizeof (s_DefaultCountryLatLonText) / sizeof (char *);



void CCountryLatLonMap::x_AddBlocksFromLine (string line)
{
    vector<string> tokens;
    NStr::Tokenize(line, "\t", tokens);
    if (tokens.size() < 6 || (tokens.size() - 2) % 4 > 0) {
//                ERR_POST_X(1, Warning << "Malformed country_lat_lon.txt line " << line
//                           << "; disregarding");
    } else {
        vector <CCountryBlock *> blocks_from_line;
        bool line_ok = true;
        try {
            size_t offset = 2;
            while (offset < tokens.size()) {
                CCountryBlock *block = new CCountryBlock(tokens[0],
                    NStr::StringToDouble(tokens[offset + 1]),
                    NStr::StringToDouble(tokens[offset]),
                    NStr::StringToDouble(tokens[offset + 3]),
                    NStr::StringToDouble(tokens[offset + 2]));
                blocks_from_line.push_back(block);
                offset += 4;
            }
        } catch (CException ) {
            line_ok = false;
        }
        if (line_ok) {
            for (int i = 0; i < blocks_from_line.size(); i++) {
                m_CountryBlockList.push_back(blocks_from_line[i]);
            }
        } else {
//                    ERR_POST_X(1, Warning << "Malformed country_lat_lon.txt line " << line
//                               << "; disregarding");
            for (int i = 0; i < blocks_from_line.size(); i++) {
                delete blocks_from_line[i];
            }
        }
    }
}


void CCountryLatLonMap::x_InitFromDefaultList()
{
    // initialize list of country blocks
    m_CountryBlockList.clear();

    for (int i = 0; i < k_NumCountryLatLonText; i++) {
        const string& line = s_DefaultCountryLatLonText[i];
        x_AddBlocksFromLine(line);
    }
}


bool CCountryLatLonMap::x_InitFromFile()
{
    // note - may want to do this initialization later, when needed
    string file = g_FindDataFile("country_lat_lon.txt");
    if (file.empty()) {
        return false;
    }

    CRef<ILineReader> lr(ILineReader::New(file));
    if (lr.Empty()) {
        return false;
    } else {
        for (++*lr; !lr->AtEOF(); ++*lr) {
            const string& line = **lr;
            x_AddBlocksFromLine(line);
        }
        return true;
    }
}


CCountryLatLonMap::CCountryLatLonMap (void) 
{
    // initialize list of country blocks
    m_CountryBlockList.clear();

    if (!x_InitFromFile()) {
        x_InitFromDefaultList();
    }
}


CCountryLatLonMap::~CCountryLatLonMap (void)
{
    size_t i;

    for (i = 0; i < m_CountryBlockList.size(); i++) {
        delete (m_CountryBlockList[i]);
    }
    m_CountryBlockList.clear();
}


bool CCountryLatLonMap::IsCountryInLatLon (string country, double x, double y)
{
    // note - if we need more speed later, create country index and search
    size_t i;

    for (i = 0; i < m_CountryBlockList.size(); i++) {
        if (NStr::Equal (country, m_CountryBlockList[i]->GetCountry())) {
            if (m_CountryBlockList[i]->IsLatLonInCountryBlock(x, y)) {
                return true;
            }
        }
    }
    return false;
}


string CCountryLatLonMap::GuessCountryForLatLon(double x, double y)
{
    // note - if we need more speed later, create x then y index
    // also note - search list backwards.  This way subregions will be found before countries
    for (TCountryBlockList_iter i = m_CountryBlockList.begin(); i != m_CountryBlockList.end(); i++) {
        if ((*i)->IsLatLonInCountryBlock(x, y)) {
            return (*i)->GetCountry();
        }
    }
    return "";
}


bool CCountryLatLonMap::HaveLatLonForCountry(string country)
{
    // note - if we need more speed later, create country index and search
    size_t i;

    for (i = 0; i < m_CountryBlockList.size(); i++) {
        if (NStr::Equal (country, m_CountryBlockList[i]->GetCountry())) {
            return true;
        }
    }
    return false;
}


const string CCountryLatLonMap::sm_BodiesOfWater [] = {
  "Bay",
  "Canal",
  "Channel",
  "Coastal",
  "Cove",
  "Estuary",
  "Fjord",
  "Freshwater",
  "Gulf",
  "Harbor",
  "Inlet",
  "Lagoon",
  "Lake",
  "Narrows",
  "Ocean",
  "Passage",
  "River",
  "Sea",
  "Seawater",
  "Sound",
  "Strait",
  "Water",
  "Waters"
};


bool CCountryLatLonMap::DoesStringContainBodyOfWater(const string& country)
{
    const string *begin = sm_BodiesOfWater;
    const string *end = &(sm_BodiesOfWater[sizeof(sm_BodiesOfWater) / sizeof(string)]);
    bool found = false;
    const string *it = begin;

    while (!found && it < end) {
        if (NStr::Find(country, *it) != string::npos) {
            return found = true;
        }
        ++it;
    }
    return found;
}


bool CCountryLatLonMap::DoCountryBoxesOverlap (string country1, string country2)
{
    size_t i, j;
    vector<size_t> country1_indices;
    vector<size_t> country2_indices;
    bool rval = false;

    for (i = 0; i < m_CountryBlockList.size(); i++) {
        if (NStr::Equal(country1, m_CountryBlockList[i]->GetCountry())) {
            country1_indices.push_back(i);
        } else if (NStr::Equal(country2, m_CountryBlockList[i]->GetCountry())) {
            country2_indices.push_back(i);
        }
    }
    
    for (i = 0; i < country1_indices.size() && !rval; i++) {
        for (j = 0; j < country2_indices.size() && !rval; j++) {
            if (m_CountryBlockList[country1_indices[i]]->DoesOverlap(m_CountryBlockList[country2_indices[j]])) {
                rval = true;
            }
        }
    }

    return rval;
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


static const char * s_DefaultLatLonCountryText[] = {
  "1",
  "Afghanistan",
  "\t39\t69\t72",
  "\t38\t63\t75",
  "\t37\t62\t75",
  "\t36\t60\t75",
  "\t35\t59\t75",
  "\t34\t59\t72",
  "\t33\t59\t72",
  "\t32\t59\t71",
  "\t31\t59\t70",
  "\t30\t59\t70",
  "\t29\t59\t67",
  "\t28\t59\t67",
  "Albania",
  "\t43\t18\t21",
  "\t42\t18\t21",
  "\t41\t18\t22",
  "\t40\t18\t22",
  "\t39\t18\t22",
  "\t38\t18\t21",
  "Algeria",
  "\t38\t5\t8",
  "\t37\t-1\t9",
  "\t36\t-3\t9",
  "\t35\t-3\t9",
  "\t34\t-3\t9",
  "\t33\t-3\t10",
  "\t32\t-4\t10",
  "\t31\t-6\t10",
  "\t30\t-9\t10",
  "\t29\t-9\t10",
  "\t28\t-9\t10",
  "\t27\t-9\t10",
  "\t26\t-9\t11",
  "\t25\t-9\t12",
  "\t24\t-7\t12",
  "\t23\t-5\t12",
  "\t22\t-4\t12",
  "\t21\t-2\t12",
  "\t20\t-1\t10",
  "\t19\t0\t8",
  "\t18\t1\t7",
  "\t17\t2\t4",
  "American Samoa",
  "\t-10\t-172\t-170",
  "\t-11\t-172\t-170",
  "\t-12\t-172\t-170",
  "\t-13\t-171\t-167",
  "\t-14\t-171\t-167",
  "\t-15\t-171\t-167",
  "Andorra",
  "\t43\t0\t2",
  "\t42\t0\t2",
  "\t41\t0\t2",
  "Angola",
  "\t-3\t11\t14",
  "\t-4\t11\t17",
  "\t-5\t11\t17\t19\t21",
  "\t-6\t11\t22",
  "\t-7\t11\t22",
  "\t-8\t11\t23",
  "\t-9\t11\t25",
  "\t-10\t11\t25",
  "\t-11\t11\t25",
  "\t-12\t11\t25",
  "\t-13\t11\t25",
  "\t-14\t10\t25",
  "\t-15\t10\t23",
  "\t-16\t10\t24",
  "\t-17\t10\t24",
  "\t-18\t10\t24",
  "\t-19\t19\t22",
  "Anguilla",
  "\t19\t-64\t-61",
  "\t18\t-64\t-61",
  "\t17\t-64\t-61",
  "Antarctica",
  "\t-59\t-47\t-43",
  "\t-60\t-59\t-53\t-47\t-43",
  "\t-61\t-62\t-53\t-47\t-43",
  "\t-62\t-62\t-53",
  "\t-63\t-65\t-54",
  "\t-64\t-66\t-54\t51\t56\t99\t104\t110\t114",
  "\t-65\t-69\t-56\t47\t58\t86\t117\t119\t144",
  "\t-66\t-70\t-59\t42\t70\t79\t147",
  "\t-67\t-91\t-89\t-73\t-59\t31\t35\t38\t71\t76\t156",
  "\t-68\t-91\t-89\t-76\t-60\t31\t161",
  "\t-69\t-91\t-89\t-77\t-60\t-11\t168",
  "\t-70\t-103\t-95\t-77\t-59\t-13\t171",
  "\t-71\t-106\t-87\t-81\t-79\t-77\t-58\t-15\t171",
  "\t-72\t-128\t-117\t-115\t-112\t-106\t-58\t-22\t-19\t-17\t171",
  "\t-73\t-137\t-109\t-106\t-58\t-23\t171",
  "\t-74\t-147\t-58\t-27\t170",
  "\t-75\t-150\t-59\t-32\t166",
  "\t-76\t-159\t-62\t-48\t-44\t-36\t170",
  "\t-77\t-165\t-65\t-51\t-42\t-37\t170",
  "\t-78\t-165\t-64\t-62\t-58\t-52\t-41\t-37\t170",
  "\t-79\t-165\t-58\t-55\t168",
  "\t-80\t-165\t-58\t-55\t164",
  "\t-81\t-175\t-170\t-164\t169",
  "\t-82\t-175\t177",
  "\t-83\t-180\t180",
  "\t-84\t-180\t180",
  "\t-85\t-180\t180",
  "\t-86\t-180\t180",
  "\t-87\t-180\t180",
  "\t-88\t-180\t180",
  "\t-89\t-180\t180",
  "\t-90\t-180\t180",
  "\t-90\t-180\t180",
  "Antigua and Barbuda",
  "\t18\t-62\t-60",
  "\t17\t-62\t-60",
  "\t16\t-62\t-60",
  "\t15\t-62\t-60",
  "Antigua and Barbuda",
  "\t18\t-62\t-60",
  "\t17\t-62\t-60",
  "\t16\t-62\t-60",
  "Arctic Ocean",
  "Argentina",
  "\t-20\t-67\t-61",
  "\t-21\t-68\t-60",
  "\t-22\t-68\t-59",
  "\t-23\t-69\t-57",
  "\t-24\t-69\t-52",
  "\t-25\t-69\t-52",
  "\t-26\t-70\t-52",
  "\t-27\t-70\t-52",
  "\t-28\t-71\t-52",
  "\t-29\t-71\t-54",
  "\t-30\t-71\t-55",
  "\t-31\t-71\t-56",
  "\t-32\t-71\t-56",
  "\t-33\t-71\t-56",
  "\t-34\t-71\t-56",
  "\t-35\t-72\t-55",
  "\t-36\t-72\t-55",
  "\t-37\t-72\t-55",
  "\t-38\t-72\t-55",
  "\t-39\t-72\t-56",
  "\t-40\t-72\t-60",
  "\t-41\t-73\t-61",
  "\t-42\t-73\t-61",
  "\t-43\t-73\t-62",
  "\t-44\t-73\t-63",
  "\t-45\t-73\t-64",
  "\t-46\t-73\t-64",
  "\t-47\t-74\t-64",
  "\t-48\t-74\t-64",
  "\t-49\t-74\t-64",
  "\t-50\t-74\t-66",
  "\t-51\t-74\t-66",
  "\t-52\t-73\t-66",
  "\t-53\t-71\t-62",
  "\t-54\t-69\t-62",
  "\t-55\t-69\t-62",
  "\t-56\t-67\t-65",
  "Armenia",
  "\t42\t42\t46",
  "\t41\t42\t46",
  "\t40\t42\t47",
  "\t39\t42\t47",
  "\t38\t43\t47",
  "\t37\t45\t47",
  "Aruba",
  "\t13\t-71\t-68",
  "\t12\t-71\t-68",
  "\t11\t-71\t-68",
  "Ashmore and Cartier Islands",
  "\t-11\t122\t124",
  "\t-12\t122\t124",
  "\t-13\t122\t124",
  "Atlantic Ocean",
  "Australia",
  "\t-8\t141\t143",
  "\t-9\t131\t133\t141\t143",
  "\t-10\t129\t137\t140\t144",
  "\t-11\t129\t137\t140\t144",
  "\t-12\t124\t137\t140\t144",
  "\t-13\t123\t137\t140\t146",
  "\t-14\t123\t138\t140\t146",
  "\t-15\t121\t146",
  "\t-16\t121\t147",
  "\t-17\t120\t147",
  "\t-18\t118\t149",
  "\t-19\t114\t150",
  "\t-20\t112\t151",
  "\t-21\t112\t151",
  "\t-22\t112\t152",
  "\t-23\t112\t154",
  "\t-24\t111\t154",
  "\t-25\t111\t154",
  "\t-26\t111\t154",
  "\t-27\t112\t154",
  "\t-28\t112\t154",
  "\t-29\t113\t154",
  "\t-30\t113\t154\t158\t160",
  "\t-31\t113\t154\t158\t160",
  "\t-32\t113\t154\t158\t160",
  "\t-33\t113\t129\t131\t153",
  "\t-34\t113\t125\t133\t152",
  "\t-35\t113\t124\t134\t152",
  "\t-36\t115\t119\t134\t151",
  "\t-37\t135\t151",
  "\t-38\t138\t151",
  "\t-39\t139\t149",
  "\t-40\t142\t149",
  "\t-41\t142\t149",
  "\t-42\t143\t149",
  "\t-43\t144\t149",
  "\t-44\t144\t149",
  "\t-53\t157\t159",
  "\t-54\t157\t159",
  "\t-55\t157\t159",
  "Australia: Australian Capital Territory",
  "\t-34\t147\t150",
  "\t-35\t147\t150",
  "\t-36\t147\t150",
  "Australia: Jervis Bay Territory",
  "\t-35\t150\t150",
  "Australia: New South Wales",
  "\t-27\t147\t154",
  "\t-28\t140\t154",
  "\t-29\t140\t154",
  "\t-30\t140\t154",
  "\t-31\t140\t154",
  "\t-32\t140\t154",
  "\t-33\t140\t153",
  "\t-34\t140\t152",
  "\t-35\t140\t152",
  "\t-36\t142\t151",
  "\t-37\t143\t151",
  "\t-38\t147\t151",
  "Australia: Northern Territory",
  "\t-9\t131\t133",
  "\t-10\t129\t137",
  "\t-11\t129\t137",
  "\t-12\t128\t137",
  "\t-13\t128\t137",
  "\t-14\t128\t138",
  "\t-15\t128\t139",
  "\t-16\t128\t139",
  "\t-17\t128\t139",
  "\t-18\t128\t139",
  "\t-19\t128\t139",
  "\t-20\t128\t139",
  "\t-21\t128\t139",
  "\t-22\t128\t139",
  "\t-23\t128\t139",
  "\t-24\t128\t139",
  "\t-25\t128\t139",
  "\t-26\t128\t139",
  "\t-27\t128\t139",
  "Australia: Queensland",
  "\t-9\t141\t143",
  "\t-10\t140\t144",
  "\t-11\t140\t144",
  "\t-12\t140\t144",
  "\t-13\t140\t146",
  "\t-14\t140\t146",
  "\t-15\t137\t146",
  "\t-16\t137\t147",
  "\t-17\t137\t147",
  "\t-18\t137\t149",
  "\t-19\t137\t150",
  "\t-20\t137\t151",
  "\t-21\t137\t151",
  "\t-22\t137\t152",
  "\t-23\t137\t154",
  "\t-24\t137\t154",
  "\t-25\t137\t154",
  "\t-26\t137\t154",
  "\t-27\t137\t154",
  "\t-28\t140\t154",
  "\t-29\t140\t154",
  "\t-30\t140\t152",
  "Australia: South Australia",
  "\t-25\t128\t142",
  "\t-26\t128\t142",
  "\t-27\t128\t142",
  "\t-28\t128\t142",
  "\t-29\t128\t142",
  "\t-30\t128\t142",
  "\t-31\t128\t142",
  "\t-32\t128\t142",
  "\t-33\t131\t142",
  "\t-34\t133\t142",
  "\t-35\t134\t142",
  "\t-36\t134\t141",
  "\t-37\t135\t141",
  "\t-38\t138\t141",
  "\t-39\t139\t141",
  "Australia: Tasmania",
  "\t-38\t142\t149",
  "\t-39\t142\t149",
  "\t-40\t142\t149",
  "\t-41\t142\t149",
  "\t-42\t143\t149",
  "\t-43\t144\t149",
  "\t-44\t144\t149",
  "Australia: Victoria",
  "\t-32\t139\t141",
  "\t-33\t139\t144",
  "\t-34\t139\t148",
  "\t-35\t139\t149",
  "\t-36\t139\t150",
  "\t-37\t139\t150",
  "\t-38\t139\t150",
  "\t-39\t139\t148",
  "\t-40\t145\t147",
  "Australia: Western Australia",
  "\t-12\t124\t128",
  "\t-13\t123\t130",
  "\t-14\t123\t130",
  "\t-15\t121\t130",
  "\t-16\t121\t130",
  "\t-17\t120\t130",
  "\t-18\t118\t130",
  "\t-19\t114\t130",
  "\t-20\t112\t130",
  "\t-21\t112\t130",
  "\t-22\t112\t130",
  "\t-23\t112\t130",
  "\t-24\t111\t130",
  "\t-25\t111\t130",
  "\t-26\t111\t130",
  "\t-27\t112\t130",
  "\t-28\t112\t130",
  "\t-29\t113\t130",
  "\t-30\t113\t130",
  "\t-31\t113\t130",
  "\t-32\t113\t130",
  "\t-33\t113\t129",
  "\t-34\t113\t125",
  "\t-35\t113\t124",
  "\t-36\t115\t119",
  "Austria",
  "\t50\t13\t16",
  "\t49\t11\t18",
  "\t48\t8\t18",
  "\t47\t8\t18",
  "\t46\t8\t18",
  "\t45\t8\t17",
  "Azerbaijan",
  "\t42\t43\t50",
  "\t41\t43\t51",
  "\t40\t43\t51",
  "\t39\t43\t51",
  "\t38\t43\t50",
  "\t37\t44\t50",
  "Bahamas",
  "\t27\t-79\t-76",
  "\t26\t-80\t-75",
  "\t25\t-80\t-73",
  "\t24\t-80\t-72",
  "\t23\t-80\t-71",
  "\t22\t-80\t-71",
  "\t21\t-76\t-71",
  "\t20\t-74\t-71",
  "\t19\t-74\t-72",
  "Bahrain",
  "\t27\t49\t51",
  "\t26\t49\t51",
  "\t25\t49\t51",
  "\t24\t49\t51",
  "Baker Island",
  "\t0\t-176\t-176",
  "Bangladesh",
  "\t27\t87\t90",
  "\t26\t87\t93",
  "\t25\t87\t93",
  "\t24\t87\t93",
  "\t23\t87\t93",
  "\t22\t87\t93",
  "\t21\t87\t93",
  "\t20\t88\t93",
  "\t19\t91\t93",
  "Barbados",
  "\t14\t-60\t-58",
  "\t13\t-60\t-58",
  "\t12\t-60\t-58",
  "Bassas da India",
  "\t-21\t39\t39",
  "Belarus",
  "\t57\t26\t30",
  "\t56\t25\t32",
  "\t55\t23\t32",
  "\t54\t22\t33",
  "\t53\t22\t33",
  "\t52\t22\t33",
  "\t51\t22\t32",
  "\t50\t22\t31",
  "Belgium",
  "\t52\t1\t6",
  "\t51\t1\t7",
  "\t50\t1\t7",
  "\t49\t1\t7",
  "\t48\t3\t6",
  "Belize",
  "\t19\t-90\t-86",
  "\t18\t-90\t-86",
  "\t17\t-90\t-86",
  "\t16\t-90\t-86",
  "\t15\t-90\t-87",
  "\t14\t-90\t-87",
  "Benin",
  "\t13\t1\t4",
  "\t12\t-1\t4",
  "\t11\t-1\t4",
  "\t10\t-1\t4",
  "\t9\t-1\t4",
  "\t8\t0\t4",
  "\t7\t0\t3",
  "\t6\t0\t3",
  "\t5\t0\t3",
  "Bermuda",
  "\t33\t-65\t-63",
  "\t32\t-65\t-63",
  "\t31\t-65\t-63",
  "Bhutan",
  "\t29\t88\t92",
  "\t28\t87\t93",
  "\t27\t87\t93",
  "\t26\t87\t93",
  "\t25\t87\t93",
  "Bolivia",
  "\t-8\t-67\t-64",
  "\t-9\t-70\t-64",
  "\t-10\t-70\t-63",
  "\t-11\t-70\t-61",
  "\t-12\t-70\t-59",
  "\t-13\t-70\t-59",
  "\t-14\t-70\t-59",
  "\t-15\t-70\t-57",
  "\t-16\t-70\t-56",
  "\t-17\t-70\t-56",
  "\t-18\t-70\t-56",
  "\t-19\t-70\t-56",
  "\t-20\t-69\t-56",
  "\t-21\t-69\t-56",
  "\t-22\t-69\t-61",
  "\t-23\t-69\t-61",
  "Borneo",
  "\t6\t113\t116",
  "\t5\t113\t116",
  "\t4\t113\t116",
  "\t3\t113\t116",
  "Borneo",
  "\t5\t114\t118",
  "\t4\t107\t109\t114\t118",
  "\t3\t107\t110\t113\t119",
  "\t2\t107\t120",
  "\t1\t107\t120",
  "\t0\t107\t120",
  "\t-1\t107\t120",
  "\t-2\t107\t118",
  "\t-3\t109\t117",
  "\t-4\t109\t117",
  "\t-5\t113\t117",
  "Borneo",
  "\t8\t115\t118",
  "\t7\t115\t119",
  "\t6\t114\t120",
  "\t5\t112\t120",
  "\t4\t111\t120",
  "\t3\t108\t119",
  "\t2\t108\t116",
  "\t1\t108\t116",
  "\t0\t108\t115",
  "\t-1\t109\t112",
  "Bosnia and Herzegovina",
  "\t46\t14\t19",
  "\t45\t14\t20",
  "\t44\t14\t20",
  "\t43\t14\t20",
  "\t42\t15\t20",
  "\t41\t16\t19",
  "Botswana",
  "\t-16\t22\t26",
  "\t-17\t19\t26",
  "\t-18\t19\t27",
  "\t-19\t19\t28",
  "\t-20\t19\t30",
  "\t-21\t18\t30",
  "\t-22\t18\t30",
  "\t-23\t18\t30",
  "\t-24\t18\t28",
  "\t-25\t18\t27",
  "\t-26\t19\t26",
  "\t-27\t19\t23",
  "Bouvet Island",
  "\t-53\t2\t4",
  "\t-54\t2\t4",
  "\t-55\t2\t4",
  "Brazil",
  "\t6\t-61\t-58",
  "\t5\t-65\t-58\t-52\t-50",
  "\t4\t-65\t-58\t-53\t-49",
  "\t3\t-69\t-49",
  "\t2\t-70\t-48",
  "\t1\t-71\t-45\t-30\t-28",
  "\t0\t-71\t-43\t-30\t-28",
  "\t-1\t-71\t-38\t-30\t-28",
  "\t-2\t-70\t-37\t-33\t-31",
  "\t-3\t-73\t-35\t-33\t-31",
  "\t-4\t-74\t-31",
  "\t-5\t-74\t-33",
  "\t-6\t-75\t-33",
  "\t-7\t-75\t-33",
  "\t-8\t-75\t-33",
  "\t-9\t-74\t-33",
  "\t-10\t-74\t-34",
  "\t-11\t-73\t-35",
  "\t-12\t-71\t-36",
  "\t-13\t-65\t-36",
  "\t-14\t-63\t-37",
  "\t-15\t-61\t-37",
  "\t-16\t-61\t-37",
  "\t-17\t-61\t-37",
  "\t-18\t-59\t-38",
  "\t-19\t-59\t-38\t-30\t-27",
  "\t-20\t-59\t-38\t-30\t-27",
  "\t-21\t-59\t-39\t-30\t-27",
  "\t-22\t-58\t-39",
  "\t-23\t-58\t-39",
  "\t-24\t-56\t-42",
  "\t-25\t-56\t-45",
  "\t-26\t-56\t-46",
  "\t-27\t-57\t-47",
  "\t-28\t-58\t-47",
  "\t-29\t-58\t-47",
  "\t-30\t-58\t-48",
  "\t-31\t-58\t-49",
  "\t-32\t-57\t-49",
  "\t-33\t-54\t-51",
  "\t-34\t-54\t-51",
  "British Virgin Islands",
  "\t19\t-65\t-63",
  "\t18\t-65\t-63",
  "\t17\t-65\t-63",
  "Brunei",
  "\t6\t113\t116",
  "\t5\t113\t116",
  "\t4\t113\t116",
  "\t3\t113\t116",
  "Bulgaria",
  "\t45\t21\t28",
  "\t44\t21\t29",
  "\t43\t21\t29",
  "\t42\t21\t29",
  "\t41\t21\t29",
  "\t40\t21\t29",
  "Burkina Faso",
  "\t16\t-1\t1",
  "\t15\t-3\t1",
  "\t14\t-5\t2",
  "\t13\t-5\t3",
  "\t12\t-6\t3",
  "\t11\t-6\t3",
  "\t10\t-6\t3",
  "\t9\t-6\t1",
  "\t8\t-5\t-1",
  "Burundi",
  "\t-1\t27\t31",
  "\t-2\t27\t31",
  "\t-3\t27\t31",
  "\t-4\t28\t31",
  "\t-5\t28\t31",
  "Cambodia",
  "\t15\t101\t108",
  "\t14\t101\t108",
  "\t13\t101\t108",
  "\t12\t101\t108",
  "\t11\t101\t108",
  "\t10\t101\t107",
  "\t9\t102\t107",
  "Cameroon",
  "\t14\t13\t15",
  "\t13\t13\t16",
  "\t12\t12\t16",
  "\t11\t12\t16",
  "\t10\t11\t16",
  "\t9\t11\t16",
  "\t8\t9\t16",
  "\t7\t8\t16",
  "\t6\t7\t16",
  "\t5\t7\t16",
  "\t4\t7\t16",
  "\t3\t7\t17",
  "\t2\t8\t17",
  "\t1\t8\t17",
  "\t0\t14\t17",
  "Canada",
  "\t84\t-78\t-67",
  "\t83\t-90\t-60",
  "\t82\t-96\t-60",
  "\t81\t-101\t-60",
  "\t80\t-106\t-61",
  "\t79\t-115\t-108\t-106\t-66",
  "\t78\t-121\t-69",
  "\t77\t-124\t-73",
  "\t76\t-124\t-74",
  "\t75\t-125\t-76",
  "\t74\t-125\t-75",
  "\t73\t-126\t-73",
  "\t72\t-126\t-69",
  "\t71\t-132\t-66",
  "\t70\t-142\t-65",
  "\t69\t-142\t-63",
  "\t68\t-142\t-61",
  "\t67\t-142\t-60",
  "\t66\t-142\t-60",
  "\t65\t-142\t-60",
  "\t64\t-142\t-61",
  "\t63\t-142\t-62",
  "\t62\t-142\t-63",
  "\t61\t-142\t-89\t-84\t-63",
  "\t60\t-142\t-91\t-81\t-62",
  "\t59\t-142\t-91\t-81\t-61",
  "\t58\t-140\t-88\t-81\t-60",
  "\t57\t-138\t-86\t-81\t-59",
  "\t56\t-133\t-57",
  "\t55\t-134\t-56",
  "\t54\t-134\t-54",
  "\t53\t-134\t-54",
  "\t52\t-134\t-54",
  "\t51\t-133\t-54",
  "\t50\t-132\t-52",
  "\t49\t-129\t-51",
  "\t48\t-128\t-51",
  "\t47\t-126\t-51",
  "\t46\t-90\t-51",
  "\t45\t-86\t-58\t-56\t-51",
  "\t44\t-84\t-58",
  "\t43\t-84\t-73\t-67\t-58",
  "\t42\t-84\t-75\t-67\t-63\t-61\t-58",
  "\t41\t-84\t-77",
  "\t40\t-84\t-80",
  "Canada: Alberta",
  "\t61\t-121\t-109",
  "\t60\t-121\t-109",
  "\t59\t-121\t-109",
  "\t58\t-121\t-109",
  "\t57\t-121\t-109",
  "\t56\t-121\t-109",
  "\t55\t-121\t-109",
  "\t54\t-121\t-109",
  "\t53\t-121\t-109",
  "\t52\t-121\t-109",
  "\t51\t-119\t-109",
  "\t50\t-118\t-109",
  "\t49\t-116\t-109",
  "\t48\t-115\t-109",
  "\t47\t-115\t-109",
  "Canada: British Columbia",
  "\t61\t-140\t-119",
  "\t60\t-140\t-119",
  "\t59\t-140\t-119",
  "\t58\t-140\t-119",
  "\t57\t-138\t-119",
  "\t56\t-134\t-119",
  "\t55\t-134\t-119",
  "\t54\t-134\t-117",
  "\t53\t-134\t-116",
  "\t52\t-134\t-114",
  "\t51\t-133\t-113",
  "\t50\t-132\t-113",
  "\t49\t-129\t-113",
  "\t48\t-128\t-113",
  "\t47\t-126\t-113",
  "Canada: Manitoba",
  "\t61\t-103\t-93",
  "\t60\t-103\t-93",
  "\t59\t-103\t-91",
  "\t58\t-103\t-88",
  "\t57\t-103\t-87",
  "\t56\t-103\t-87",
  "\t55\t-103\t-87",
  "\t54\t-103\t-89",
  "\t53\t-102\t-90",
  "\t52\t-102\t-92",
  "\t51\t-102\t-93",
  "\t50\t-102\t-94",
  "\t49\t-102\t-94",
  "\t48\t-102\t-94",
  "\t47\t-102\t-94",
  "Canada: New Brunswick",
  "\t49\t-67\t-63",
  "\t48\t-70\t-63",
  "\t47\t-70\t-62",
  "\t46\t-70\t-62",
  "\t45\t-68\t-62",
  "\t44\t-68\t-63",
  "\t43\t-67\t-65",
  "Canada: Newfoundland and Labrador",
  "\t61\t-65\t-63",
  "\t60\t-65\t-62",
  "\t59\t-65\t-61",
  "\t58\t-65\t-60",
  "\t57\t-65\t-59",
  "\t56\t-68\t-57",
  "\t55\t-68\t-56",
  "\t54\t-68\t-54",
  "\t53\t-68\t-54",
  "\t52\t-68\t-54",
  "\t51\t-68\t-54",
  "\t50\t-65\t-63\t-59\t-52",
  "\t49\t-60\t-51",
  "\t48\t-60\t-51",
  "\t47\t-60\t-51",
  "\t46\t-60\t-51",
  "\t45\t-56\t-51",
  "Canada: Northwest Territories",
  "\t79\t-115\t-109",
  "\t78\t-121\t-109",
  "\t77\t-124\t-109",
  "\t76\t-124\t-109",
  "\t75\t-125\t-109",
  "\t74\t-125\t-109",
  "\t73\t-126\t-109",
  "\t72\t-126\t-109",
  "\t71\t-132\t-109",
  "\t70\t-136\t-109",
  "\t69\t-137\t-109",
  "\t68\t-137\t-115\t-113\t-111",
  "\t67\t-137\t-113",
  "\t66\t-137\t-108",
  "\t65\t-135\t-100",
  "\t64\t-134\t-100",
  "\t63\t-133\t-100",
  "\t62\t-131\t-100",
  "\t61\t-130\t-101",
  "\t60\t-129\t-101",
  "\t59\t-127\t-101",
  "Canada: Nova Scotia",
  "\t48\t-61\t-59",
  "\t47\t-65\t-58",
  "\t46\t-66\t-58",
  "\t45\t-67\t-58",
  "\t44\t-67\t-58",
  "\t43\t-67\t-58",
  "\t42\t-67\t-63\t-61\t-58",
  "Canada: Nunavut",
  "\t84\t-78\t-67",
  "\t83\t-90\t-60",
  "\t82\t-96\t-60",
  "\t81\t-101\t-60",
  "\t80\t-106\t-61",
  "\t79\t-111\t-108\t-106\t-66",
  "\t78\t-111\t-69",
  "\t77\t-111\t-73",
  "\t76\t-111\t-74",
  "\t75\t-111\t-76",
  "\t74\t-111\t-75",
  "\t73\t-111\t-73",
  "\t72\t-111\t-69",
  "\t71\t-118\t-66",
  "\t70\t-121\t-65",
  "\t69\t-121\t-63",
  "\t68\t-121\t-61",
  "\t67\t-121\t-60",
  "\t66\t-121\t-60",
  "\t65\t-118\t-60",
  "\t64\t-114\t-61",
  "\t63\t-110\t-62",
  "\t62\t-103\t-63",
  "\t61\t-103\t-89\t-84\t-63",
  "\t60\t-103\t-91\t-81\t-77\t-69\t-63",
  "\t59\t-103\t-93\t-81\t-76\t-69\t-63",
  "\t58\t-81\t-75",
  "\t57\t-81\t-75",
  "\t56\t-81\t-75",
  "\t55\t-82\t-75",
  "\t54\t-83\t-76",
  "\t53\t-83\t-77",
  "\t52\t-83\t-77",
  "\t51\t-82\t-77",
  "\t50\t-80\t-78",
  "Canada: Ontario",
  "\t57\t-90\t-86",
  "\t56\t-92\t-81",
  "\t55\t-94\t-81",
  "\t54\t-95\t-81",
  "\t53\t-96\t-79",
  "\t52\t-96\t-78",
  "\t51\t-96\t-78",
  "\t50\t-96\t-78",
  "\t49\t-96\t-78",
  "\t48\t-96\t-78",
  "\t47\t-95\t-76",
  "\t46\t-90\t-73",
  "\t45\t-86\t-73",
  "\t44\t-84\t-73",
  "\t43\t-84\t-73",
  "\t42\t-84\t-75",
  "\t41\t-84\t-77",
  "\t40\t-84\t-80",
  "Canada: Prince Edward Island",
  "\t48\t-65\t-62",
  "\t47\t-65\t-60",
  "\t46\t-65\t-60",
  "\t45\t-65\t-60",
  "\t44\t-63\t-61",
  "Canada: Quebec",
  "\t63\t-79\t-71",
  "\t62\t-79\t-68",
  "\t61\t-79\t-68\t-66\t-63",
  "\t60\t-79\t-68\t-66\t-63",
  "\t59\t-79\t-62",
  "\t58\t-79\t-62",
  "\t57\t-79\t-62",
  "\t56\t-79\t-62",
  "\t55\t-80\t-62",
  "\t54\t-80\t-62",
  "\t53\t-80\t-56",
  "\t52\t-80\t-56",
  "\t51\t-80\t-56",
  "\t50\t-80\t-56",
  "\t49\t-80\t-57",
  "\t48\t-80\t-60",
  "\t47\t-80\t-60",
  "\t46\t-80\t-65\t-63\t-60",
  "\t45\t-80\t-68",
  "\t44\t-78\t-69",
  "\t43\t-75\t-73",
  "Canada: Saskatchewan",
  "\t61\t-111\t-101",
  "\t60\t-111\t-101",
  "\t59\t-111\t-101",
  "\t58\t-111\t-101",
  "\t57\t-111\t-101",
  "\t56\t-111\t-100",
  "\t55\t-111\t-100",
  "\t54\t-111\t-100",
  "\t53\t-111\t-100",
  "\t52\t-111\t-100",
  "\t51\t-111\t-100",
  "\t50\t-111\t-100",
  "\t49\t-111\t-100",
  "\t48\t-111\t-100",
  "\t47\t-111\t-100",
  "Canada: Yukon",
  "\t70\t-142\t-136",
  "\t69\t-142\t-135",
  "\t68\t-142\t-132",
  "\t67\t-142\t-131",
  "\t66\t-142\t-131",
  "\t65\t-142\t-129",
  "\t64\t-142\t-128",
  "\t63\t-142\t-127",
  "\t62\t-142\t-125",
  "\t61\t-142\t-122",
  "\t60\t-142\t-122",
  "\t59\t-142\t-122",
  "Cape Verde",
  "\t18\t-26\t-23",
  "\t17\t-26\t-21",
  "\t16\t-26\t-21",
  "\t15\t-26\t-21",
  "\t14\t-25\t-21",
  "\t13\t-25\t-22",
  "Cayman Islands",
  "\t20\t-82\t-78",
  "\t19\t-82\t-78",
  "\t18\t-82\t-78",
  "Central African Republic",
  "\t12\t21\t23",
  "\t11\t20\t24",
  "\t10\t18\t24",
  "\t9\t17\t25",
  "\t8\t14\t26",
  "\t7\t13\t27",
  "\t6\t13\t28",
  "\t5\t13\t28",
  "\t4\t13\t28",
  "\t3\t13\t26",
  "\t2\t14\t19",
  "\t1\t14\t17",
  "Chad",
  "\t24\t13\t17",
  "\t23\t13\t19",
  "\t22\t13\t21",
  "\t21\t13\t23",
  "\t20\t14\t24",
  "\t19\t14\t24",
  "\t18\t14\t24",
  "\t17\t13\t24",
  "\t16\t12\t24",
  "\t15\t12\t24",
  "\t14\t12\t24",
  "\t13\t12\t23",
  "\t12\t12\t23",
  "\t11\t13\t23",
  "\t10\t12\t23",
  "\t9\t12\t23",
  "\t8\t12\t22",
  "\t7\t13\t20",
  "\t6\t14\t18",
  "Chile",
  "\t-16\t-70\t-68",
  "\t-17\t-71\t-67",
  "\t-18\t-71\t-67",
  "\t-19\t-71\t-67",
  "\t-20\t-71\t-67",
  "\t-21\t-71\t-66",
  "\t-22\t-71\t-66",
  "\t-23\t-71\t-66",
  "\t-24\t-71\t-66",
  "\t-25\t-106\t-104\t-81\t-78\t-71\t-66",
  "\t-26\t-110\t-108\t-106\t-104\t-81\t-78\t-72\t-67",
  "\t-27\t-110\t-108\t-106\t-104\t-81\t-78\t-72\t-67",
  "\t-28\t-110\t-108\t-72\t-67",
  "\t-29\t-72\t-68",
  "\t-30\t-72\t-68",
  "\t-31\t-72\t-68",
  "\t-32\t-81\t-77\t-72\t-68",
  "\t-33\t-81\t-77\t-73\t-68",
  "\t-34\t-81\t-77\t-73\t-68",
  "\t-35\t-74\t-68",
  "\t-36\t-74\t-69",
  "\t-37\t-74\t-69",
  "\t-38\t-74\t-69",
  "\t-39\t-74\t-69",
  "\t-40\t-75\t-70",
  "\t-41\t-75\t-70",
  "\t-42\t-75\t-70",
  "\t-43\t-76\t-70",
  "\t-44\t-76\t-70",
  "\t-45\t-76\t-70",
  "\t-46\t-76\t-70",
  "\t-47\t-76\t-70",
  "\t-48\t-76\t-70",
  "\t-49\t-76\t-71",
  "\t-50\t-76\t-69",
  "\t-51\t-76\t-67",
  "\t-52\t-76\t-67",
  "\t-53\t-76\t-66",
  "\t-54\t-75\t-65",
  "\t-55\t-74\t-65",
  "\t-56\t-72\t-65",
  "China",
  "\t54\t119\t126",
  "\t53\t119\t127",
  "\t52\t118\t127",
  "\t51\t118\t128",
  "\t50\t85\t88\t115\t130",
  "\t49\t84\t90\t114\t135",
  "\t48\t81\t91\t114\t135",
  "\t47\t81\t92\t114\t135",
  "\t46\t79\t94\t110\t135",
  "\t45\t78\t96\t110\t135",
  "\t44\t78\t96\t109\t134",
  "\t43\t78\t132",
  "\t42\t75\t132",
  "\t41\t72\t132",
  "\t40\t72\t129",
  "\t39\t72\t127",
  "\t38\t72\t125",
  "\t37\t72\t123",
  "\t36\t73\t123",
  "\t35\t73\t123",
  "\t34\t74\t121",
  "\t33\t77\t122",
  "\t32\t77\t122",
  "\t31\t77\t123",
  "\t30\t77\t123",
  "\t29\t78\t123",
  "\t28\t81\t123",
  "\t27\t83\t122",
  "\t26\t84\t93\t96\t122",
  "\t25\t96\t121",
  "\t24\t96\t120",
  "\t23\t96\t120",
  "\t22\t96\t118",
  "\t21\t98\t117",
  "\t20\t98\t102\t105\t114",
  "\t19\t107\t112",
  "\t18\t107\t112",
  "\t17\t107\t111",
  "China: Hainan",
  "\t21\t108\t111",
  "\t20\t107\t112",
  "\t19\t107\t112",
  "\t18\t107\t112",
  "\t17\t107\t111",
  "Christmas Island",
  "\t-10\t105\t105",
  "Clipperton Island",
  "\t10\t-109\t-109",
  "Cocos Islands",
  "\t-11\t96\t96",
  "\t-12\t96\t96",
  "Colombia",
  "\t14\t-82\t-80",
  "\t13\t-82\t-80\t-73\t-70",
  "\t12\t-82\t-80\t-75\t-70",
  "\t11\t-82\t-80\t-76\t-70",
  "\t10\t-77\t-70",
  "\t9\t-78\t-71",
  "\t8\t-78\t-69",
  "\t7\t-78\t-66",
  "\t6\t-78\t-66",
  "\t5\t-78\t-66",
  "\t4\t-78\t-66",
  "\t3\t-79\t-66",
  "\t2\t-80\t-65",
  "\t1\t-80\t-65",
  "\t0\t-80\t-65",
  "\t-1\t-79\t-68",
  "\t-2\t-75\t-68",
  "\t-3\t-74\t-68",
  "\t-4\t-71\t-68",
  "\t-5\t-71\t-68",
  "Comoros",
  "\t-10\t42\t44",
  "\t-11\t42\t45",
  "\t-12\t42\t45",
  "\t-13\t42\t45",
  "Cook Islands",
  "\t-7\t-159\t-156",
  "\t-8\t-159\t-156",
  "\t-9\t-166\t-164\t-162\t-156",
  "\t-10\t-166\t-164\t-162\t-159",
  "\t-11\t-166\t-164\t-162\t-159",
  "\t-17\t-160\t-158",
  "\t-18\t-160\t-156",
  "\t-19\t-160\t-156",
  "\t-20\t-160\t-156",
  "\t-21\t-160\t-156",
  "\t-22\t-160\t-156",
  "Coral Sea Islands",
  "\t-15\t146\t151",
  "\t-16\t146\t151",
  "\t-17\t146\t151",
  "\t-18\t147\t149",
  "\t-20\t152\t156",
  "\t-21\t152\t156",
  "\t-22\t152\t156",
  "\t-23\t154\t156",
  "Costa Rica",
  "\t12\t-86\t-83",
  "\t11\t-86\t-82",
  "\t10\t-86\t-81",
  "\t9\t-86\t-81",
  "\t8\t-86\t-81",
  "\t7\t-84\t-81",
  "\t6\t-88\t-86",
  "\t5\t-88\t-86",
  "\t4\t-88\t-86",
  "Cote d'Ivoire",
  "\t11\t-9\t-3",
  "\t10\t-9\t-1",
  "\t9\t-9\t-1",
  "\t8\t-9\t-1",
  "\t7\t-9\t-1",
  "\t6\t-9\t-1",
  "\t5\t-9\t-1",
  "\t4\t-8\t-1",
  "\t3\t-8\t-4",
  "Croatia",
  "\t47\t14\t18",
  "\t46\t12\t20",
  "\t45\t12\t20",
  "\t44\t12\t20",
  "\t43\t12\t20",
  "\t42\t14\t19",
  "\t41\t15\t19",
  "Cuba",
  "\t24\t-84\t-79",
  "\t23\t-85\t-76",
  "\t22\t-85\t-74",
  "\t21\t-85\t-73",
  "\t20\t-85\t-73",
  "\t19\t-80\t-73",
  "\t18\t-78\t-73",
  "Cyprus",
  "\t36\t31\t35",
  "\t35\t31\t35",
  "\t34\t31\t35",
  "\t33\t31\t35",
  "Czech Republic",
  "\t52\t13\t16",
  "\t51\t11\t19",
  "\t50\t11\t19",
  "\t49\t11\t19",
  "\t48\t11\t19",
  "\t47\t12\t18",
  "Democratic Republic of the Congo",
  "\t4\t15\t19",
  "\t3\t12\t19",
  "\t2\t12\t19",
  "\t1\t12\t19",
  "\t0\t11\t19",
  "\t-1\t10\t18",
  "\t-2\t10\t18",
  "\t-3\t10\t17",
  "\t-4\t10\t17",
  "\t-5\t10\t16",
  "\t-6\t10\t13",
  "Denmark",
  "\t63\t-8\t-5",
  "\t62\t-8\t-5",
  "\t61\t-8\t-5",
  "\t60\t-7\t-5",
  "\t58\t7\t12",
  "\t57\t7\t13",
  "\t56\t7\t16",
  "\t55\t7\t16",
  "\t54\t7\t16",
  "\t53\t7\t13",
  "Djibouti",
  "\t13\t41\t44",
  "\t12\t40\t44",
  "\t11\t40\t44",
  "\t10\t40\t44",
  "\t9\t40\t43",
  "Dominica",
  "\t16\t-62\t-60",
  "\t15\t-62\t-60",
  "\t14\t-62\t-60",
  "Dominican Republic",
  "\t20\t-72\t-67",
  "\t19\t-73\t-67",
  "\t18\t-73\t-67",
  "\t17\t-73\t-67",
  "\t16\t-72\t-70",
  "East Timor",
  "\t-7\t123\t128",
  "\t-8\t123\t128",
  "\t-9\t123\t128",
  "\t-10\t123\t127",
  "Ecuador",
  "\t2\t-93\t-90\t-80\t-77",
  "\t1\t-93\t-88\t-81\t-74",
  "\t0\t-93\t-88\t-81\t-74",
  "\t-1\t-92\t-88\t-82\t-74",
  "\t-2\t-92\t-88\t-82\t-74",
  "\t-3\t-82\t-74",
  "\t-4\t-81\t-76",
  "\t-5\t-81\t-77",
  "\t-6\t-80\t-78",
  "Ecuador: Galapagos",
  "\t2\t-93\t-90",
  "\t1\t-93\t-88",
  "\t0\t-93\t-88",
  "\t-1\t-92\t-88",
  "\t-2\t-92\t-88",
  "Egypt",
  "\t32\t23\t35",
  "\t31\t23\t35",
  "\t30\t23\t35",
  "\t29\t23\t35",
  "\t28\t23\t35",
  "\t27\t23\t35",
  "\t26\t23\t35",
  "\t25\t23\t36",
  "\t24\t23\t36",
  "\t23\t23\t37",
  "\t22\t23\t37",
  "\t21\t23\t37",
  "\t20\t23\t37",
  "El Salvador",
  "\t15\t-90\t-87",
  "\t14\t-91\t-86",
  "\t13\t-91\t-86",
  "\t12\t-91\t-86",
  "Equatorial Guinea",
  "\t4\t7\t9",
  "\t3\t7\t12",
  "\t2\t7\t12",
  "\t1\t8\t12",
  "\t0\t4\t6\t8\t12",
  "\t-1\t4\t6\t8\t10",
  "\t-2\t4\t6",
  "Eritrea",
  "\t19\t37\t39",
  "\t18\t35\t40",
  "\t17\t35\t41",
  "\t16\t35\t41",
  "\t15\t35\t42",
  "\t14\t35\t43",
  "\t13\t35\t44",
  "\t12\t39\t44",
  "\t11\t40\t44",
  "Estonia",
  "\t60\t21\t29",
  "\t59\t20\t29",
  "\t58\t20\t29",
  "\t57\t20\t28",
  "\t56\t20\t28",
  "Ethiopia",
  "\t15\t35\t41",
  "\t14\t35\t42",
  "\t13\t34\t43",
  "\t12\t33\t43",
  "\t11\t33\t44",
  "\t10\t33\t44",
  "\t9\t32\t47",
  "\t8\t31\t48",
  "\t7\t31\t48",
  "\t6\t31\t48",
  "\t5\t33\t47",
  "\t4\t33\t46",
  "\t3\t34\t46",
  "\t2\t36\t42",
  "Europa Island",
  "\t-22\t40\t40",
  "Falkland Islands (Islas Malvinas)",
  "\t-50\t-62\t-56",
  "\t-51\t-62\t-56",
  "\t-52\t-62\t-56",
  "\t-53\t-62\t-57",
  "Faroe Islands",
  "\t63\t-8\t-5",
  "\t62\t-8\t-5",
  "\t61\t-8\t-5",
  "\t60\t-7\t-5",
  "Fiji",
  "\t-11\t176\t178",
  "\t-12\t176\t178",
  "\t-13\t176\t178",
  "\t-15\t-180\t-178\t176\t180",
  "\t-16\t-180\t-177\t176\t180",
  "\t-17\t-180\t-177\t176\t180",
  "\t-18\t-180\t-177\t176\t180",
  "\t-19\t-180\t-177\t176\t180",
  "\t-20\t-180\t-177\t173\t180",
  "\t-21\t173\t175",
  "\t-22\t173\t175",
  "Finland",
  "\t71\t26\t28",
  "\t70\t19\t30",
  "\t69\t19\t30",
  "\t68\t19\t31",
  "\t67\t19\t31",
  "\t66\t22\t31",
  "\t65\t22\t31",
  "\t64\t20\t32",
  "\t63\t20\t32",
  "\t62\t20\t32",
  "\t61\t20\t32",
  "\t60\t20\t31",
  "\t59\t20\t29",
  "\t58\t21\t25",
  "Finland",
  "\t61\t18\t22",
  "\t60\t18\t22",
  "\t59\t18\t22",
  "\t58\t19\t21",
  "France",
  "\t52\t0\t3",
  "\t51\t0\t5",
  "\t50\t-2\t8",
  "\t49\t-6\t9",
  "\t48\t-6\t9",
  "\t47\t-6\t9",
  "\t46\t-5\t8",
  "\t45\t-3\t8",
  "\t44\t-2\t10",
  "\t43\t-2\t10",
  "\t42\t-2\t10",
  "\t41\t-2\t10",
  "\t40\t7\t10",
  "\t17\t-62\t-59",
  "\t16\t-62\t-59",
  "\t15\t-62\t-59",
  "\t14\t-62\t-59",
  "\t13\t-62\t-59",
  "\t6\t-55\t-51",
  "\t5\t-55\t-50",
  "\t4\t-55\t-50",
  "\t3\t-55\t-50",
  "\t2\t-55\t-50",
  "\t1\t-55\t-51",
  "\t-11\t44\t46",
  "\t-12\t44\t46",
  "\t-13\t44\t46",
  "\t-19\t54\t56",
  "\t-20\t54\t56",
  "\t-21\t54\t56",
  "\t-22\t54\t56",
  "France: Corsica",
  "\t44\t8\t10",
  "\t43\t7\t10",
  "\t42\t7\t10",
  "\t41\t7\t10",
  "\t40\t7\t10",
  "French Guiana",
  "\t6\t-55\t-51",
  "\t5\t-55\t-50",
  "\t4\t-55\t-50",
  "\t3\t-55\t-50",
  "\t2\t-55\t-50",
  "\t1\t-55\t-51",
  "French Polynesia",
  "\t-6\t-141\t-139",
  "\t-7\t-141\t-138",
  "\t-8\t-141\t-137",
  "\t-9\t-141\t-137",
  "\t-10\t-141\t-137",
  "\t-11\t-140\t-137",
  "\t-13\t-149\t-140",
  "\t-14\t-149\t-139",
  "\t-15\t-152\t-139",
  "\t-16\t-152\t-137",
  "\t-17\t-152\t-135",
  "\t-18\t-150\t-148\t-146\t-135",
  "\t-19\t-142\t-135",
  "\t-20\t-142\t-134",
  "\t-21\t-152\t-150\t-141\t-134",
  "\t-22\t-152\t-146\t-141\t-133",
  "\t-23\t-152\t-146\t-136\t-133",
  "\t-24\t-150\t-146\t-136\t-133",
  "\t-26\t-145\t-143",
  "\t-27\t-145\t-143",
  "\t-28\t-145\t-143",
  "French Southern and Antarctic Lands",
  "\t-10\t46\t48",
  "\t-11\t46\t48",
  "\t-12\t46\t48",
  "\t-14\t53\t55",
  "\t-15\t53\t55",
  "\t-16\t41\t43\t53\t55",
  "\t-17\t41\t43",
  "\t-18\t41\t43",
  "\t-20\t38\t40",
  "\t-21\t38\t41",
  "\t-22\t38\t41",
  "\t-23\t39\t41",
  "\t-36\t76\t78",
  "\t-37\t76\t78",
  "\t-38\t76\t78",
  "\t-39\t76\t78",
  "\t-45\t49\t52",
  "\t-46\t49\t52",
  "\t-47\t49\t52\t67\t70",
  "\t-48\t67\t71",
  "\t-49\t67\t71",
  "\t-50\t67\t71",
  "Gabon",
  "\t3\t10\t14",
  "\t2\t8\t15",
  "\t1\t7\t15",
  "\t0\t7\t15",
  "\t-1\t7\t15",
  "\t-2\t7\t15",
  "\t-3\t8\t15",
  "\t-4\t9\t12",
  "Gambia",
  "\t14\t-17\t-12",
  "\t13\t-17\t-12",
  "\t12\t-17\t-12",
  "Gaza Strip",
  "\t32\t33\t35",
  "\t31\t33\t35",
  "\t30\t33\t35",
  "Georgia",
  "\t44\t38\t44",
  "\t43\t38\t47",
  "\t42\t38\t47",
  "\t41\t39\t47",
  "\t40\t40\t47",
  "Germany",
  "\t56\t7\t9",
  "\t55\t7\t15",
  "\t54\t5\t15",
  "\t53\t5\t15",
  "\t52\t4\t16",
  "\t51\t4\t16",
  "\t50\t4\t16",
  "\t49\t4\t15",
  "\t48\t5\t14",
  "\t47\t6\t14",
  "\t46\t6\t14",
  "Ghana",
  "\t12\t-2\t1",
  "\t11\t-3\t1",
  "\t10\t-3\t1",
  "\t9\t-3\t1",
  "\t8\t-4\t1",
  "\t7\t-4\t2",
  "\t6\t-4\t2",
  "\t5\t-4\t2",
  "\t4\t-4\t2",
  "\t3\t-3\t0",
  "Gibraltar",
  "\t37\t-6\t-4",
  "\t36\t-6\t-4",
  "\t35\t-6\t-4",
  "Glorioso Islands",
  "\t-11\t47\t47",
  "Greece",
  "\t42\t20\t27",
  "\t41\t19\t27",
  "\t40\t18\t27",
  "\t39\t18\t27",
  "\t38\t18\t28",
  "\t37\t19\t29",
  "\t36\t19\t29",
  "\t35\t20\t29",
  "\t34\t22\t28",
  "\t33\t23\t26",
  "Greenland",
  "\t84\t-47\t-23",
  "\t83\t-60\t-18",
  "\t82\t-65\t-10",
  "\t81\t-68\t-10",
  "\t80\t-69\t-10",
  "\t79\t-74\t-13",
  "\t78\t-74\t-16",
  "\t77\t-74\t-16",
  "\t76\t-73\t-16",
  "\t75\t-72\t-16",
  "\t74\t-68\t-65\t-61\t-16",
  "\t73\t-58\t-16",
  "\t72\t-57\t-19",
  "\t71\t-57\t-20",
  "\t70\t-56\t-20",
  "\t69\t-56\t-20",
  "\t68\t-55\t-21",
  "\t67\t-54\t-24",
  "\t66\t-54\t-31",
  "\t65\t-54\t-32",
  "\t64\t-54\t-34",
  "\t63\t-53\t-39",
  "\t62\t-52\t-39",
  "\t61\t-51\t-40",
  "\t60\t-50\t-41",
  "\t59\t-49\t-41",
  "\t58\t-45\t-42",
  "Grenada",
  "\t13\t-62\t-60",
  "\t12\t-62\t-60",
  "\t11\t-62\t-60",
  "Guadeloupe",
  "\t52\t0\t3",
  "\t51\t0\t5",
  "\t50\t-2\t8",
  "\t49\t-6\t9",
  "\t48\t-6\t9",
  "\t47\t-6\t9",
  "\t46\t-5\t8",
  "\t45\t-3\t8",
  "\t44\t-2\t10",
  "\t43\t-2\t10",
  "\t42\t-2\t10",
  "\t41\t-2\t10",
  "\t40\t7\t10",
  "\t17\t-62\t-59",
  "\t16\t-62\t-59",
  "\t15\t-62\t-59",
  "\t14\t-62\t-59",
  "\t13\t-62\t-59",
  "\t6\t-55\t-51",
  "\t5\t-55\t-50",
  "\t4\t-55\t-50",
  "\t3\t-55\t-50",
  "\t2\t-55\t-50",
  "\t1\t-55\t-51",
  "\t-11\t44\t46",
  "\t-12\t44\t46",
  "\t-13\t44\t46",
  "\t-19\t54\t56",
  "\t-20\t54\t56",
  "\t-21\t54\t56",
  "\t-22\t54\t56",
  "Guam",
  "\t14\t143\t145",
  "\t13\t143\t145",
  "\t12\t143\t145",
  "Guatemala",
  "\t18\t-92\t-88",
  "\t17\t-92\t-88",
  "\t16\t-93\t-87",
  "\t15\t-93\t-87",
  "\t14\t-93\t-87",
  "\t13\t-93\t-88",
  "\t12\t-92\t-89",
  "Guernsey",
  "\t50\t-3\t-1",
  "\t49\t-3\t-1",
  "\t48\t-3\t-1",
  "Guinea",
  "\t13\t-14\t-7",
  "\t12\t-15\t-7",
  "\t11\t-16\t-6",
  "\t10\t-16\t-6",
  "\t9\t-16\t-6",
  "\t8\t-14\t-6",
  "\t7\t-11\t-6",
  "\t6\t-10\t-7",
  "Guinea-Bissau",
  "\t13\t-17\t-12",
  "\t12\t-17\t-12",
  "\t11\t-17\t-12",
  "\t10\t-17\t-12",
  "\t9\t-16\t-13",
  "Guyana",
  "\t9\t-61\t-58",
  "\t8\t-61\t-57",
  "\t7\t-62\t-56",
  "\t6\t-62\t-56",
  "\t5\t-62\t-56",
  "\t4\t-62\t-56",
  "\t3\t-61\t-55",
  "\t2\t-61\t-55",
  "\t1\t-61\t-55",
  "\t0\t-60\t-55",
  "Haiti",
  "\t21\t-73\t-71",
  "\t20\t-74\t-70",
  "\t19\t-75\t-70",
  "\t18\t-75\t-70",
  "\t17\t-75\t-70",
  "Heard Island and McDonald Islands",
  "\t-51\t72\t74",
  "\t-52\t72\t74",
  "\t-53\t72\t74",
  "\t-54\t72\t74",
  "Honduras",
  "\t18\t-84\t-82",
  "\t17\t-87\t-82",
  "\t16\t-90\t-82",
  "\t15\t-90\t-82",
  "\t14\t-90\t-82",
  "\t13\t-90\t-82",
  "\t12\t-89\t-84",
  "\t11\t-88\t-86",
  "Hong Kong",
  "\t22\t113\t114",
  "Howland Island",
  "\t0\t-176\t-176",
  "Hungary",
  "\t49\t16\t23",
  "\t48\t15\t23",
  "\t47\t15\t23",
  "\t46\t15\t23",
  "\t45\t15\t22",
  "\t44\t16\t20",
  "Iceland",
  "\t67\t-24\t-13",
  "\t66\t-25\t-12",
  "\t65\t-25\t-12",
  "\t64\t-25\t-12",
  "\t63\t-25\t-12",
  "\t62\t-23\t-15",
  "India",
  "\t36\t76\t79",
  "\t35\t72\t79",
  "\t34\t72\t80",
  "\t33\t72\t80",
  "\t32\t72\t80",
  "\t31\t72\t82",
  "\t30\t71\t82\t93\t97",
  "\t29\t69\t82\t87\t89\t91\t98",
  "\t28\t68\t98",
  "\t27\t68\t98",
  "\t26\t68\t98",
  "\t25\t67\t96",
  "\t24\t67\t96",
  "\t23\t67\t95",
  "\t22\t67\t95",
  "\t21\t67\t94",
  "\t20\t68\t93",
  "\t19\t69\t88",
  "\t18\t71\t87",
  "\t17\t71\t85",
  "\t16\t72\t84",
  "\t15\t72\t83",
  "\t14\t72\t82\t91\t95",
  "\t13\t73\t81\t91\t95",
  "\t12\t71\t81\t91\t95",
  "\t11\t71\t81\t91\t94",
  "\t10\t71\t80\t91\t94",
  "\t9\t71\t80\t91\t94",
  "\t8\t72\t80\t91\t94",
  "\t7\t72\t79\t92\t94",
  "\t6\t92\t94",
  "\t5\t92\t94",
  "Indian Ocean",
  "Indonesia",
  "\t6\t94\t98\t125\t127",
  "\t5\t94\t99\t106\t109\t114\t118\t125\t128",
  "\t4\t94\t100\t104\t109\t114\t118\t124\t128",
  "\t3\t94\t102\t104\t110\t113\t119\t124\t129",
  "\t2\t94\t132",
  "\t1\t94\t137",
  "\t0\t96\t139",
  "\t-1\t96\t141",
  "\t-2\t97\t141",
  "\t-3\t98\t141",
  "\t-4\t99\t141",
  "\t-5\t101\t141",
  "\t-6\t101\t116\t118\t141",
  "\t-7\t104\t141",
  "\t-8\t105\t132\t136\t141",
  "\t-9\t109\t132\t136\t141",
  "\t-10\t115\t126\t139\t141",
  "\t-11\t119\t125",
  "Iran",
  "\t40\t43\t49",
  "\t39\t43\t49\t54\t58",
  "\t38\t43\t61",
  "\t37\t43\t62",
  "\t36\t43\t62",
  "\t35\t43\t62",
  "\t34\t44\t62",
  "\t33\t44\t62",
  "\t32\t44\t62",
  "\t31\t45\t62",
  "\t30\t46\t62",
  "\t29\t46\t63",
  "\t28\t47\t64",
  "\t27\t49\t64",
  "\t26\t50\t64",
  "\t25\t52\t64",
  "\t24\t53\t62",
  "Iraq",
  "\t38\t41\t45",
  "\t37\t40\t46",
  "\t36\t40\t47",
  "\t35\t39\t47",
  "\t34\t37\t47",
  "\t33\t37\t48",
  "\t32\t37\t48",
  "\t31\t37\t49",
  "\t30\t39\t49",
  "\t29\t41\t49",
  "\t28\t42\t49",
  "Ireland",
  "\t56\t-9\t-5",
  "\t55\t-11\t-5",
  "\t54\t-11\t-5",
  "\t53\t-11\t-4",
  "\t52\t-11\t-4",
  "\t51\t-11\t-4",
  "\t50\t-11\t-6",
  "Isle of Man",
  "\t55\t-5\t-3",
  "\t54\t-5\t-3",
  "\t53\t-5\t-3",
  "Israel",
  "\t34\t34\t36",
  "\t33\t33\t36",
  "\t32\t33\t36",
  "\t31\t33\t36",
  "\t30\t33\t36",
  "\t29\t33\t36",
  "\t28\t33\t36",
  "Italy",
  "\t48\t10\t13",
  "\t47\t6\t14",
  "\t46\t5\t14",
  "\t45\t5\t14",
  "\t44\t5\t14",
  "\t43\t5\t16",
  "\t42\t6\t18",
  "\t41\t7\t19",
  "\t40\t7\t19",
  "\t39\t7\t19",
  "\t38\t7\t19",
  "\t37\t7\t18",
  "\t36\t10\t17",
  "\t35\t10\t16",
  "\t34\t11\t13",
  "Jamaica",
  "\t19\t-79\t-75",
  "\t18\t-79\t-75",
  "\t17\t-79\t-75",
  "\t16\t-78\t-75",
  "Jan Mayen",
  "\t72\t-9\t-6",
  "\t71\t-10\t-6",
  "\t70\t-10\t-6",
  "\t69\t-10\t-7",
  "Japan",
  "\t46\t139\t143",
  "\t45\t139\t146",
  "\t44\t139\t146",
  "\t43\t138\t146",
  "\t42\t138\t146",
  "\t41\t138\t146",
  "\t40\t138\t144",
  "\t39\t137\t143",
  "\t38\t135\t143",
  "\t37\t131\t142",
  "\t36\t131\t142",
  "\t35\t128\t141",
  "\t34\t128\t141",
  "\t33\t127\t140",
  "\t32\t127\t141",
  "\t31\t127\t134\t138\t141",
  "\t30\t128\t132\t139\t141",
  "\t29\t128\t132\t139\t141",
  "\t28\t126\t131\t139\t143",
  "\t27\t125\t131\t139\t143",
  "\t26\t122\t132\t139\t143",
  "\t25\t121\t132\t140\t143\t152\t154",
  "\t24\t121\t126\t130\t132\t140\t142\t152\t154",
  "\t23\t121\t126\t140\t142\t152\t154",
  "Jarvis Island",
  "\t0\t-160\t-160",
  "Jersey",
  "\t50\t-3\t-1",
  "\t49\t-3\t-1",
  "\t48\t-3\t-1",
  "Johnston Atoll",
  "\t16\t-169\t-169",
  "Jordan",
  "\t34\t37\t39",
  "\t33\t34\t40",
  "\t32\t34\t40",
  "\t31\t34\t40",
  "\t30\t33\t39",
  "\t29\t33\t38",
  "\t28\t33\t38",
  "Juan de Nova Island",
  "\t-16\t42\t42",
  "\t-17\t42\t42",
  "Kazakhstan",
  "\t56\t67\t71",
  "\t55\t60\t77",
  "\t54\t59\t79",
  "\t53\t59\t79",
  "\t52\t48\t84",
  "\t51\t46\t86",
  "\t50\t45\t88",
  "\t49\t45\t88",
  "\t48\t45\t88",
  "\t47\t45\t87",
  "\t46\t46\t86",
  "\t45\t47\t86",
  "\t44\t48\t83",
  "\t43\t48\t56\t58\t81",
  "\t42\t49\t56\t60\t81",
  "\t41\t50\t56\t64\t81",
  "\t40\t51\t56\t65\t71",
  "\t39\t66\t69",
  "Kenya",
  "\t6\t33\t36",
  "\t5\t32\t42",
  "\t4\t32\t42",
  "\t3\t32\t42",
  "\t2\t33\t42",
  "\t1\t32\t42",
  "\t0\t32\t42",
  "\t-1\t32\t42",
  "\t-2\t32\t42",
  "\t-3\t34\t42",
  "\t-4\t36\t41",
  "\t-5\t37\t40",
  "Kerguelen Archipelago",
  "\t-47\t67\t70",
  "\t-48\t67\t71",
  "\t-49\t67\t71",
  "\t-50\t67\t71",
  "Kingman Reef",
  "\t6\t-162\t-162",
  "Kiribati",
  "\t5\t-161\t-159",
  "\t4\t-161\t-158\t171\t173",
  "\t3\t-161\t-156\t171\t173",
  "\t2\t-160\t-156\t171\t174",
  "\t1\t-158\t-156\t171\t175",
  "\t0\t-158\t-156\t171\t177",
  "\t-1\t-172\t-170\t171\t177",
  "\t-2\t-172\t-170\t173\t177",
  "\t-3\t-173\t-170\t-156\t-153\t174\t177",
  "\t-4\t-173\t-171\t-156\t-153",
  "\t-5\t-173\t-171\t-156\t-153",
  "\t-10\t-152\t-150",
  "\t-11\t-152\t-150",
  "\t-12\t-152\t-150",
  "Kosovo",
  "\t44\t19\t22",
  "\t43\t19\t22",
  "\t42\t19\t22",
  "\t41\t19\t22",
  "\t40\t19\t21",
  "Kuwait",
  "\t31\t46\t49",
  "\t30\t45\t49",
  "\t29\t45\t49",
  "\t28\t45\t49",
  "\t27\t46\t49",
  "Kyrgyzstan",
  "\t44\t72\t75",
  "\t43\t69\t81",
  "\t42\t69\t81",
  "\t41\t68\t81",
  "\t40\t68\t80",
  "\t39\t68\t78",
  "\t38\t68\t74",
  "Laos",
  "\t23\t100\t103",
  "\t22\t99\t104",
  "\t21\t99\t105",
  "\t20\t99\t105",
  "\t19\t99\t106",
  "\t18\t99\t107",
  "\t17\t99\t108",
  "\t16\t99\t108",
  "\t15\t103\t108",
  "\t14\t104\t108",
  "\t13\t104\t108",
  "\t12\t104\t107",
  "Latvia",
  "\t59\t23\t26",
  "\t58\t20\t28",
  "\t57\t19\t29",
  "\t56\t19\t29",
  "\t55\t19\t29",
  "\t54\t24\t28",
  "Lebanon",
  "\t35\t34\t37",
  "\t34\t34\t37",
  "\t33\t34\t37",
  "\t32\t34\t37",
  "Lesotho",
  "\t-27\t26\t30",
  "\t-28\t26\t30",
  "\t-29\t26\t30",
  "\t-30\t26\t30",
  "\t-31\t26\t29",
  "Liberia",
  "\t9\t-11\t-8",
  "\t8\t-12\t-7",
  "\t7\t-12\t-6",
  "\t6\t-12\t-6",
  "\t5\t-12\t-6",
  "\t4\t-11\t-6",
  "\t3\t-10\t-6",
  "Libya",
  "\t34\t10\t12",
  "\t33\t9\t16\t19\t25",
  "\t32\t9\t26",
  "\t31\t8\t26",
  "\t30\t8\t26",
  "\t29\t8\t25",
  "\t28\t8\t25",
  "\t27\t8\t25",
  "\t26\t8\t25",
  "\t25\t8\t25",
  "\t24\t8\t25",
  "\t23\t9\t25",
  "\t22\t10\t25",
  "\t21\t12\t25",
  "\t20\t17\t25",
  "\t19\t20\t25",
  "\t18\t21\t25",
  "Liechtenstein",
  "\t48\t8\t10",
  "\t47\t8\t10",
  "\t46\t8\t10",
  "Lithuania",
  "\t57\t20\t26",
  "\t56\t19\t27",
  "\t55\t19\t27",
  "\t54\t19\t27",
  "\t53\t21\t27",
  "\t52\t22\t25",
  "Luxembourg",
  "\t51\t4\t7",
  "\t50\t4\t7",
  "\t49\t4\t7",
  "\t48\t4\t7",
  "Macau",
  "\t22\t113\t113",
  "Macedonia",
  "\t43\t19\t23",
  "\t42\t19\t24",
  "\t41\t19\t24",
  "\t40\t19\t24",
  "\t39\t19\t22",
  "Madagascar",
  "\t-10\t48\t50",
  "\t-11\t47\t50",
  "\t-12\t46\t51",
  "\t-13\t46\t51",
  "\t-14\t44\t51",
  "\t-15\t43\t51",
  "\t-16\t42\t51",
  "\t-17\t42\t51",
  "\t-18\t42\t50",
  "\t-19\t42\t50",
  "\t-20\t42\t50",
  "\t-21\t42\t49",
  "\t-22\t42\t49",
  "\t-23\t42\t48",
  "\t-24\t42\t48",
  "\t-25\t42\t48",
  "\t-26\t43\t48",
  "Malawi",
  "\t-8\t31\t35",
  "\t-9\t31\t35",
  "\t-10\t31\t35",
  "\t-11\t31\t35",
  "\t-12\t31\t36",
  "\t-13\t31\t36",
  "\t-14\t31\t36",
  "\t-15\t31\t36",
  "\t-16\t33\t36",
  "\t-17\t33\t36",
  "\t-18\t34\t36",
  "Malaysia",
  "\t8\t115\t118",
  "\t7\t98\t103\t115\t119",
  "\t6\t98\t104\t114\t120",
  "\t5\t98\t104\t112\t120",
  "\t4\t99\t104\t111\t120",
  "\t3\t99\t105\t108\t119",
  "\t2\t99\t105\t108\t116",
  "\t1\t100\t105\t108\t116",
  "\t0\t101\t105\t108\t115",
  "\t-1\t109\t112",
  "Maldives",
  "\t8\t71\t73",
  "\t7\t71\t74",
  "\t6\t71\t74",
  "\t5\t71\t74",
  "\t4\t71\t74",
  "\t3\t71\t74",
  "\t2\t71\t74",
  "\t1\t71\t74",
  "\t0\t71\t74",
  "\t-1\t71\t74",
  "Mali",
  "\t25\t-7\t-2",
  "\t24\t-7\t0",
  "\t23\t-7\t1",
  "\t22\t-7\t2",
  "\t21\t-7\t3",
  "\t20\t-7\t5",
  "\t19\t-7\t5",
  "\t18\t-7\t5",
  "\t17\t-6\t5",
  "\t16\t-12\t5",
  "\t15\t-13\t5",
  "\t14\t-13\t4",
  "\t13\t-13\t1",
  "\t12\t-13\t-1",
  "\t11\t-12\t-3",
  "\t10\t-12\t-3",
  "\t9\t-9\t-4",
  "Malta",
  "\t37\t13\t15",
  "\t36\t13\t15",
  "\t35\t13\t15",
  "\t34\t13\t15",
  "Marshall Islands",
  "\t15\t167\t170",
  "\t14\t167\t170",
  "\t13\t167\t170",
  "\t12\t164\t167",
  "\t11\t164\t167\t169\t171",
  "\t10\t164\t167\t169\t171",
  "\t9\t166\t171",
  "\t8\t166\t172",
  "\t7\t166\t173",
  "\t6\t167\t173",
  "\t5\t167\t173",
  "\t4\t167\t170",
  "\t3\t167\t169",
  "Martinique",
  "\t52\t0\t3",
  "\t51\t0\t5",
  "\t50\t-2\t8",
  "\t49\t-6\t9",
  "\t48\t-6\t9",
  "\t47\t-6\t9",
  "\t46\t-5\t8",
  "\t45\t-3\t8",
  "\t44\t-2\t10",
  "\t43\t-2\t10",
  "\t42\t-2\t10",
  "\t41\t-2\t10",
  "\t40\t7\t10",
  "\t17\t-62\t-59",
  "\t16\t-62\t-59",
  "\t15\t-62\t-59",
  "\t14\t-62\t-59",
  "\t13\t-62\t-59",
  "\t6\t-55\t-51",
  "\t5\t-55\t-50",
  "\t4\t-55\t-50",
  "\t3\t-55\t-50",
  "\t2\t-55\t-50",
  "\t1\t-55\t-51",
  "\t-11\t44\t46",
  "\t-12\t44\t46",
  "\t-13\t44\t46",
  "\t-19\t54\t56",
  "\t-20\t54\t56",
  "\t-21\t54\t56",
  "\t-22\t54\t56",
  "Mauritania",
  "\t28\t-9\t-7",
  "\t27\t-9\t-5",
  "\t26\t-13\t-3",
  "\t25\t-13\t-3",
  "\t24\t-14\t-3",
  "\t23\t-14\t-3",
  "\t22\t-18\t-5",
  "\t21\t-18\t-5",
  "\t20\t-18\t-4",
  "\t19\t-18\t-4",
  "\t18\t-17\t-4",
  "\t17\t-17\t-4",
  "\t16\t-17\t-4",
  "\t15\t-17\t-4",
  "\t14\t-17\t-4",
  "\t13\t-13\t-10",
  "Mauritius",
  "\t-9\t55\t57",
  "\t-10\t55\t57",
  "\t-11\t55\t57",
  "\t-18\t56\t58\t62\t64",
  "\t-19\t56\t58\t62\t64",
  "\t-20\t56\t58\t62\t64",
  "\t-21\t56\t58",
  "Mayotte",
  "\t-12\t45\t45",
  "\t-13\t45\t45",
  "Mediterranean Sea",
  "Mexico",
  "\t33\t-118\t-112",
  "\t32\t-118\t-104",
  "\t31\t-118\t-103",
  "\t30\t-119\t-99",
  "\t29\t-119\t-98",
  "\t28\t-119\t-98",
  "\t27\t-119\t-96",
  "\t26\t-116\t-96",
  "\t25\t-115\t-96",
  "\t24\t-113\t-96",
  "\t23\t-113\t-96",
  "\t22\t-111\t-96\t-91\t-85",
  "\t21\t-111\t-95\t-91\t-85",
  "\t20\t-111\t-109\t-107\t-94\t-92\t-85",
  "\t19\t-115\t-109\t-106\t-85",
  "\t18\t-115\t-109\t-106\t-86",
  "\t17\t-115\t-109\t-105\t-86",
  "\t16\t-103\t-87",
  "\t15\t-101\t-89",
  "\t14\t-98\t-90",
  "\t13\t-93\t-91",
  "Micronesia",
  "\t10\t137\t139",
  "\t9\t137\t139\t148\t151\t153\t155",
  "\t8\t137\t139\t148\t155",
  "\t7\t148\t159",
  "\t6\t148\t154\t156\t159\t161\t164",
  "\t5\t148\t150\t152\t154\t156\t159\t161\t164",
  "\t4\t152\t154\t156\t158\t161\t164",
  "Midway Islands",
  "\t28\t-178\t-178",
  "Midway Islands",
  "\t28\t-177\t-177",
  "Midway Islands",
  "\t26\t-174\t-174",
  "Midway Islands",
  "\t25\t-171\t-171",
  "Moldova",
  "\t49\t25\t29",
  "\t48\t25\t30",
  "\t47\t25\t31",
  "\t46\t26\t31",
  "\t45\t27\t31",
  "\t44\t27\t29",
  "Monaco",
  "\t44\t6\t8",
  "\t43\t6\t8",
  "\t42\t6\t8",
  "Mongolia",
  "\t53\t97\t100",
  "\t52\t96\t103",
  "\t51\t88\t108\t112\t117",
  "\t50\t86\t117",
  "\t49\t86\t119",
  "\t48\t86\t120",
  "\t47\t86\t120",
  "\t46\t88\t120",
  "\t45\t89\t120",
  "\t44\t89\t117",
  "\t43\t91\t115",
  "\t42\t94\t112",
  "\t41\t95\t111",
  "\t40\t102\t106",
  "Montenegro",
  "\t44\t17\t21",
  "\t43\t17\t21",
  "\t42\t17\t21",
  "\t41\t17\t21",
  "\t40\t18\t20",
  "Montserrat",
  "\t17\t-63\t-61",
  "\t16\t-63\t-61",
  "\t15\t-63\t-61",
  "Morocco",
  "\t36\t-7\t-1",
  "\t35\t-7\t0",
  "\t34\t-9\t0",
  "\t33\t-10\t0",
  "\t32\t-10\t0",
  "\t31\t-10\t0",
  "\t30\t-11\t-1",
  "\t29\t-13\t-2",
  "\t28\t-14\t-4",
  "\t27\t-15\t-7",
  "\t26\t-15\t-7",
  "\t25\t-16\t-8",
  "\t24\t-17\t-11",
  "\t23\t-17\t-11",
  "\t22\t-18\t-12",
  "\t21\t-18\t-13",
  "\t20\t-18\t-13",
  "Mozambique",
  "\t-9\t38\t41",
  "\t-10\t33\t41",
  "\t-11\t33\t41",
  "\t-12\t33\t41",
  "\t-13\t29\t41",
  "\t-14\t29\t41",
  "\t-15\t29\t41",
  "\t-16\t29\t41",
  "\t-17\t29\t41",
  "\t-18\t31\t39",
  "\t-19\t31\t37",
  "\t-20\t30\t36",
  "\t-21\t30\t36",
  "\t-22\t30\t36",
  "\t-23\t30\t36",
  "\t-24\t30\t36",
  "\t-25\t30\t36",
  "\t-26\t30\t34",
  "\t-27\t31\t33",
  "Myanmar",
  "\t29\t96\t99",
  "\t28\t94\t99",
  "\t27\t94\t99",
  "\t26\t93\t99",
  "\t25\t92\t99",
  "\t24\t92\t100",
  "\t23\t91\t100",
  "\t22\t91\t102",
  "\t21\t91\t102",
  "\t20\t91\t102",
  "\t19\t91\t101",
  "\t18\t91\t100",
  "\t17\t92\t99",
  "\t16\t93\t99",
  "\t15\t92\t99",
  "\t14\t92\t100",
  "\t13\t92\t94\t96\t100",
  "\t12\t96\t100",
  "\t11\t96\t100",
  "\t10\t96\t100",
  "\t9\t96\t100",
  "\t8\t97\t99",
  "Namibia",
  "\t-15\t12\t14",
  "\t-16\t10\t26",
  "\t-17\t10\t26",
  "\t-18\t10\t26",
  "\t-19\t10\t25",
  "\t-20\t11\t21",
  "\t-21\t12\t21",
  "\t-22\t12\t21",
  "\t-23\t13\t21",
  "\t-24\t13\t20",
  "\t-25\t13\t20",
  "\t-26\t13\t20",
  "\t-27\t13\t20",
  "\t-28\t14\t20",
  "\t-29\t14\t20",
  "Nauru",
  "\t1\t165\t167",
  "\t0\t165\t167",
  "\t-1\t165\t167",
  "Navassa Island",
  "\t18\t-75\t-75",
  "Nepal",
  "\t31\t79\t83",
  "\t30\t79\t85",
  "\t29\t79\t87",
  "\t28\t79\t89",
  "\t27\t79\t89",
  "\t26\t80\t89",
  "\t25\t83\t89",
  "Netherlands",
  "\t54\t3\t8",
  "\t53\t3\t8",
  "\t52\t2\t8",
  "\t51\t2\t8",
  "\t50\t2\t7",
  "\t49\t4\t7",
  "Netherlands Antilles",
  "\t19\t-64\t-62",
  "\t18\t-64\t-61",
  "\t17\t-64\t-61",
  "\t16\t-64\t-61",
  "\t13\t-70\t-67",
  "\t12\t-70\t-67",
  "\t11\t-70\t-67",
  "New Caledonia",
  "\t-18\t158\t160\t162\t164",
  "\t-19\t158\t160\t162\t168",
  "\t-20\t158\t160\t162\t169",
  "\t-21\t162\t169",
  "\t-22\t163\t169",
  "\t-23\t165\t168",
  "New Zealand",
  "\t-17\t-170\t-168",
  "\t-18\t-170\t-168",
  "\t-19\t-170\t-168",
  "\t-20\t-170\t-168",
  "\t-28\t-178\t-176",
  "\t-29\t-178\t-176",
  "\t-30\t-178\t-176",
  "\t-33\t171\t174",
  "\t-34\t171\t175",
  "\t-35\t171\t176",
  "\t-36\t172\t179",
  "\t-37\t172\t179",
  "\t-38\t172\t179",
  "\t-39\t171\t179",
  "\t-40\t170\t179",
  "\t-41\t169\t177",
  "\t-42\t-177\t-175\t167\t177",
  "\t-43\t-177\t-175\t166\t175",
  "\t-44\t-177\t-175\t165\t174",
  "\t-45\t-177\t-175\t165\t172",
  "\t-46\t165\t172",
  "\t-47\t165\t171",
  "\t-48\t165\t169\t177\t179",
  "\t-49\t164\t167\t177\t179",
  "\t-50\t164\t167\t177\t179",
  "\t-51\t164\t170",
  "\t-52\t168\t170",
  "\t-53\t168\t170",
  "Nicaragua",
  "\t16\t-84\t-82",
  "\t15\t-87\t-81",
  "\t14\t-88\t-81",
  "\t13\t-88\t-81",
  "\t12\t-88\t-82",
  "\t11\t-88\t-82",
  "\t10\t-87\t-82",
  "\t9\t-85\t-82",
  "Niger",
  "\t24\t10\t14",
  "\t23\t8\t16",
  "\t22\t6\t16",
  "\t21\t5\t16",
  "\t20\t3\t16",
  "\t19\t3\t16",
  "\t18\t3\t16",
  "\t17\t2\t16",
  "\t16\t0\t16",
  "\t15\t-1\t16",
  "\t14\t-1\t15",
  "\t13\t-1\t14",
  "\t12\t-1\t14",
  "\t11\t0\t10",
  "\t10\t1\t4",
  "Nigeria",
  "\t14\t3\t15",
  "\t13\t2\t15",
  "\t12\t2\t15",
  "\t11\t2\t15",
  "\t10\t1\t15",
  "\t9\t1\t14",
  "\t8\t1\t14",
  "\t7\t1\t13",
  "\t6\t1\t13",
  "\t5\t1\t12",
  "\t4\t4\t10",
  "\t3\t4\t9",
  "Niue",
  "\t-27\t166\t168",
  "\t-28\t166\t168",
  "\t-29\t166\t168",
  "\t-30\t166\t168",
  "Norfolk Island",
  "\t-27\t166\t168",
  "\t-28\t166\t168",
  "\t-29\t166\t168",
  "\t-30\t166\t168",
  "North Korea",
  "\t44\t128\t130",
  "\t43\t127\t131",
  "\t42\t125\t131",
  "\t41\t123\t131",
  "\t40\t123\t131",
  "\t39\t123\t130",
  "\t38\t123\t129",
  "\t37\t123\t129",
  "\t36\t123\t127",
  "North Sea",
  "Northern Mariana Islands",
  "\t21\t143\t146",
  "\t20\t143\t146",
  "\t19\t143\t146",
  "\t18\t144\t146",
  "\t17\t144\t146",
  "\t16\t144\t146",
  "\t15\t144\t146",
  "\t14\t144\t146",
  "\t13\t144\t146",
  "Norway",
  "\t72\t22\t29",
  "\t71\t17\t32",
  "\t70\t14\t32",
  "\t69\t11\t32",
  "\t68\t11\t31",
  "\t67\t11\t26",
  "\t66\t10\t18",
  "\t65\t8\t17",
  "\t64\t6\t15",
  "\t63\t3\t15",
  "\t62\t3\t13",
  "\t61\t3\t13",
  "\t60\t3\t13",
  "\t59\t3\t13",
  "\t58\t4\t13",
  "\t57\t4\t12",
  "\t56\t5\t8",
  "Oman",
  "\t27\t55\t57",
  "\t26\t55\t57",
  "\t25\t54\t58",
  "\t24\t54\t60",
  "\t23\t54\t60",
  "\t22\t54\t60",
  "\t21\t54\t60",
  "\t20\t51\t60",
  "\t19\t50\t59",
  "\t18\t50\t58",
  "\t17\t50\t58",
  "\t16\t51\t57",
  "\t15\t51\t55",
  "Pacific Ocean",
  "Pakistan",
  "\t38\t73\t75",
  "\t37\t70\t77",
  "\t36\t70\t78",
  "\t35\t68\t78",
  "\t34\t68\t78",
  "\t33\t68\t78",
  "\t32\t65\t76",
  "\t31\t65\t76",
  "\t30\t59\t75",
  "\t29\t59\t75",
  "\t28\t59\t74",
  "\t27\t60\t73",
  "\t26\t60\t72",
  "\t25\t60\t72",
  "\t24\t60\t72",
  "\t23\t65\t72",
  "\t22\t66\t69",
  "Palau",
  "\t8\t133\t135",
  "\t7\t133\t135",
  "\t6\t131\t135",
  "\t5\t131\t135",
  "\t4\t130\t133",
  "\t3\t130\t132",
  "\t2\t130\t132",
  "\t1\t130\t132",
  "Palmyra Atoll",
  "\t5\t-162\t-162",
  "Panama",
  "\t10\t-83\t-76",
  "\t9\t-84\t-76",
  "\t8\t-84\t-76",
  "\t7\t-84\t-76",
  "\t6\t-82\t-76",
  "Papua New Guinea",
  "\t0\t141\t143\t145\t151",
  "\t-1\t139\t143\t145\t153",
  "\t-2\t139\t155",
  "\t-3\t139\t155",
  "\t-4\t139\t156",
  "\t-5\t139\t156",
  "\t-6\t139\t156",
  "\t-7\t139\t156",
  "\t-8\t139\t154",
  "\t-9\t139\t154",
  "\t-10\t139\t155",
  "\t-11\t146\t155",
  "\t-12\t152\t155",
  "Paracel Islands",
  "\t15\t111\t111",
  "Paraguay",
  "\t-18\t-62\t-57",
  "\t-19\t-63\t-56",
  "\t-20\t-63\t-56",
  "\t-21\t-63\t-54",
  "\t-22\t-63\t-53",
  "\t-23\t-63\t-53",
  "\t-24\t-62\t-53",
  "\t-25\t-61\t-53",
  "\t-26\t-59\t-53",
  "\t-27\t-59\t-53",
  "\t-28\t-59\t-54",
  "Peru",
  "\t1\t-76\t-73",
  "\t0\t-76\t-72",
  "\t-1\t-78\t-69",
  "\t-2\t-81\t-69",
  "\t-3\t-82\t-68",
  "\t-4\t-82\t-68",
  "\t-5\t-82\t-68",
  "\t-6\t-82\t-71",
  "\t-7\t-82\t-71",
  "\t-8\t-80\t-69",
  "\t-9\t-80\t-68",
  "\t-10\t-79\t-68",
  "\t-11\t-79\t-67",
  "\t-12\t-78\t-67",
  "\t-13\t-78\t-67",
  "\t-14\t-77\t-67",
  "\t-15\t-77\t-67",
  "\t-16\t-76\t-67",
  "\t-17\t-75\t-67",
  "\t-18\t-73\t-68",
  "\t-19\t-71\t-68",
  "Philippines",
  "\t22\t120\t122",
  "\t21\t120\t123",
  "\t20\t120\t123",
  "\t19\t119\t123",
  "\t18\t119\t123",
  "\t17\t118\t123",
  "\t16\t118\t123",
  "\t15\t118\t125",
  "\t14\t118\t125",
  "\t13\t118\t126",
  "\t12\t118\t126",
  "\t11\t117\t127",
  "\t10\t116\t127",
  "\t9\t115\t127",
  "\t8\t115\t127",
  "\t7\t115\t127",
  "\t6\t115\t127",
  "\t5\t117\t127",
  "\t4\t118\t126",
  "\t3\t118\t120",
  "Pitcairn Islands",
  "\t-22\t-131\t-129",
  "\t-23\t-131\t-127\t-125\t-123",
  "\t-24\t-131\t-127\t-125\t-123",
  "\t-25\t-131\t-127\t-125\t-123",
  "\t-26\t-131\t-129",
  "Poland",
  "\t55\t13\t24",
  "\t54\t13\t24",
  "\t53\t13\t24",
  "\t52\t13\t24",
  "\t51\t13\t25",
  "\t50\t13\t25",
  "\t49\t13\t25",
  "\t48\t16\t24",
  "\t47\t21\t23",
  "Portugal",
  "\t43\t-9\t-7",
  "\t42\t-9\t-5",
  "\t41\t-9\t-5",
  "\t40\t-10\t-5",
  "\t39\t-10\t-5",
  "\t38\t-10\t-5",
  "\t37\t-10\t-5",
  "\t36\t-9\t-6",
  "\t35\t-8\t-6",
  "Portugal: Azores",
  "\t40\t-32\t-26",
  "\t39\t-32\t-26",
  "\t38\t-32\t-24",
  "\t37\t-29\t-24",
  "\t36\t-26\t-24",
  "\t35\t-26\t-24",
  "Portugal: Madeira",
  "\t34\t-17\t-15",
  "\t33\t-18\t-15",
  "\t32\t-18\t-15",
  "\t31\t-18\t-14",
  "\t30\t-17\t-14",
  "\t29\t-17\t-14",
  "Puerto Rico",
  "\t19\t-68\t-64",
  "\t18\t-68\t-64",
  "\t17\t-68\t-64",
  "\t16\t-68\t-64",
  "Qatar",
  "\t27\t50\t52",
  "\t26\t49\t52",
  "\t25\t49\t52",
  "\t24\t49\t52",
  "\t23\t49\t52",
  "Republic of the Congo",
  "\t6\t18\t20\t23\t28",
  "\t5\t17\t31",
  "\t4\t17\t31",
  "\t3\t17\t32",
  "\t2\t16\t32",
  "\t1\t16\t32",
  "\t0\t15\t32",
  "\t-1\t15\t31",
  "\t-2\t14\t30",
  "\t-3\t11\t30",
  "\t-4\t11\t30",
  "\t-5\t11\t31",
  "\t-6\t11\t31",
  "\t-7\t11\t13\t15\t31",
  "\t-8\t15\t31",
  "\t-9\t16\t31",
  "\t-10\t20\t29",
  "\t-11\t21\t30",
  "\t-12\t21\t30",
  "\t-13\t25\t30",
  "\t-14\t27\t30",
  "Reunion",
  "\t-20\t55\t55",
  "\t-21\t55\t55",
  "Romania",
  "\t49\t21\t28",
  "\t48\t20\t29",
  "\t47\t19\t29",
  "\t46\t19\t30",
  "\t45\t19\t30",
  "\t44\t19\t30",
  "\t43\t20\t30",
  "\t42\t21\t29",
  "Ross Sea",
  "Russia",
  "\t82\t49\t51\t53\t66\t88\t97",
  "\t81\t35\t37\t43\t66\t77\t81\t88\t100",
  "\t80\t35\t37\t43\t66\t75\t81\t88\t105",
  "\t79\t35\t37\t43\t66\t75\t81\t89\t108",
  "\t78\t49\t52\t57\t60\t66\t68\t75\t78\t88\t108\t155\t157",
  "\t77\t59\t70\t88\t114\t136\t143\t147\t153\t155\t157",
  "\t76\t54\t70\t80\t114\t134\t153\t155\t157",
  "\t75\t53\t70\t78\t117\t134\t153",
  "\t74\t52\t130\t134\t151",
  "\t73\t50\t61\t67\t130\t134\t151",
  "\t72\t-180\t-174\t50\t59\t65\t159\t177\t180",
  "\t71\t-180\t-174\t50\t61\t65\t163\t167\t172\t177\t180",
  "\t70\t-180\t-174\t27\t37\t47\t180",
  "\t69\t-180\t-175\t27\t180",
  "\t68\t-180\t-171\t27\t180",
  "\t67\t-180\t-168\t27\t180",
  "\t66\t-180\t-167\t28\t180",
  "\t65\t-180\t-167\t28\t180",
  "\t64\t-180\t-167\t28\t180",
  "\t63\t-176\t-171\t28\t180",
  "\t62\t27\t180",
  "\t61\t25\t180",
  "\t60\t25\t175",
  "\t59\t25\t173",
  "\t58\t26\t167\t169\t171",
  "\t57\t26\t143\t150\t165",
  "\t56\t19\t23\t26\t141\t154\t167",
  "\t55\t18\t23\t26\t144\t154\t169",
  "\t54\t18\t23\t27\t144\t154\t169",
  "\t53\t18\t23\t29\t144\t154\t163\t165\t169",
  "\t52\t30\t63\t71\t144\t154\t161",
  "\t51\t30\t63\t77\t121\t124\t145\t154\t159",
  "\t50\t33\t62\t78\t121\t125\t145\t153\t159",
  "\t49\t34\t50\t53\t62\t78\t99\t101\t120\t126\t145\t152\t157",
  "\t48\t36\t49\t83\t90\t94\t98\t106\t120\t126\t145\t151\t156",
  "\t47\t36\t50\t129\t145\t149\t155",
  "\t46\t35\t50\t129\t144\t146\t154",
  "\t45\t35\t50\t129\t153",
  "\t44\t35\t49\t129\t138\t141\t151",
  "\t43\t36\t49\t129\t137\t144\t148",
  "\t42\t38\t49\t129\t136\t144\t147",
  "\t41\t42\t49\t129\t135",
  "\t40\t45\t49",
  "Rwanda",
  "\t0\t28\t31",
  "\t-1\t27\t31",
  "\t-2\t27\t31",
  "\t-3\t27\t31",
  "Saint Helena",
  "\t-6\t-15\t-13",
  "\t-7\t-15\t-13",
  "\t-8\t-15\t-13",
  "\t-14\t-6\t-4",
  "\t-15\t-6\t-4",
  "\t-16\t-6\t-4",
  "\t-17\t-6\t-4",
  "\t-36\t-13\t-11",
  "\t-37\t-13\t-11",
  "\t-38\t-13\t-11",
  "\t-39\t-11\t-8",
  "\t-40\t-11\t-8",
  "\t-41\t-11\t-8",
  "Saint Kitts and Nevis",
  "\t18\t-63\t-61",
  "\t17\t-63\t-61",
  "\t16\t-63\t-61",
  "Saint Lucia",
  "\t15\t-62\t-59",
  "\t14\t-62\t-59",
  "\t13\t-62\t-59",
  "\t12\t-62\t-59",
  "Saint Pierre and Miquelon",
  "\t48\t-57\t-55",
  "\t47\t-57\t-55",
  "\t46\t-57\t-55",
  "\t45\t-57\t-55",
  "Saint Vincent and the Grenadines",
  "\t14\t-62\t-60",
  "\t13\t-62\t-60",
  "\t12\t-62\t-60",
  "\t11\t-62\t-60",
  "Samoa",
  "\t-12\t-173\t-170",
  "\t-13\t-173\t-170",
  "\t-14\t-173\t-170",
  "\t-15\t-172\t-170",
  "San Marino",
  "\t44\t11\t13",
  "\t43\t11\t13",
  "\t42\t11\t13",
  "Sao Tome and Principe",
  "\t2\t6\t8",
  "\t1\t5\t8",
  "\t0\t5\t8",
  "\t-1\t5\t7",
  "Saudi Arabia",
  "\t33\t37\t40",
  "\t32\t35\t43",
  "\t31\t35\t44",
  "\t30\t33\t48",
  "\t29\t33\t49",
  "\t28\t33\t50",
  "\t27\t33\t51",
  "\t26\t33\t51",
  "\t25\t34\t52",
  "\t24\t35\t53",
  "\t23\t36\t56",
  "\t22\t37\t56",
  "\t21\t37\t56",
  "\t20\t37\t56",
  "\t19\t38\t56",
  "\t18\t39\t55",
  "\t17\t40\t52",
  "\t16\t40\t48",
  "\t15\t40\t48",
  "Senegal",
  "\t17\t-17\t-12",
  "\t16\t-17\t-11",
  "\t15\t-18\t-10",
  "\t14\t-18\t-10",
  "\t13\t-18\t-10",
  "\t12\t-17\t-10",
  "\t11\t-17\t-10",
  "Serbia",
  "\t47\t18\t21",
  "\t46\t17\t22",
  "\t45\t17\t23",
  "\t44\t17\t23",
  "\t43\t17\t23",
  "\t42\t18\t23",
  "\t41\t19\t23",
  "Seychelles",
  "\t-2\t54\t56",
  "\t-3\t54\t56",
  "\t-4\t52\t56",
  "\t-5\t51\t56",
  "\t-6\t51\t57",
  "\t-7\t51\t53\t55\t57",
  "\t-8\t45\t48\t51\t53\t55\t57",
  "\t-9\t45\t48",
  "\t-10\t45\t48",
  "Sierra Leone",
  "\t10\t-14\t-9",
  "\t9\t-14\t-9",
  "\t8\t-14\t-9",
  "\t7\t-14\t-9",
  "\t6\t-13\t-9",
  "\t5\t-12\t-10",
  "Singapore",
  "\t2\t102\t105",
  "\t1\t102\t105",
  "\t0\t102\t105",
  "Slovakia",
  "\t50\t16\t23",
  "\t49\t15\t23",
  "\t48\t15\t23",
  "\t47\t15\t23",
  "\t46\t16\t19",
  "Slovenia",
  "\t47\t12\t17",
  "\t46\t12\t17",
  "\t45\t12\t17",
  "\t44\t12\t16",
  "Solomon Islands",
  "\t-5\t154\t158",
  "\t-6\t154\t161",
  "\t-7\t154\t163",
  "\t-8\t154\t163\t166\t168",
  "\t-9\t155\t168",
  "\t-10\t158\t168",
  "\t-11\t158\t169",
  "\t-12\t158\t161\t165\t169",
  "\t-13\t167\t169",
  "Somalia",
  "\t12\t47\t52",
  "\t11\t47\t52",
  "\t10\t47\t52",
  "\t9\t47\t52",
  "\t8\t45\t51",
  "\t7\t44\t51",
  "\t6\t44\t50",
  "\t5\t40\t50",
  "\t4\t40\t49",
  "\t3\t39\t49",
  "\t2\t39\t48",
  "\t1\t39\t47",
  "\t0\t39\t46",
  "\t-1\t39\t44",
  "\t-2\t40\t42",
  "South Africa",
  "\t-21\t26\t32",
  "\t-22\t25\t32",
  "\t-23\t18\t21\t24\t32",
  "\t-24\t18\t32",
  "\t-25\t18\t33",
  "\t-26\t18\t33",
  "\t-27\t15\t33",
  "\t-28\t15\t33",
  "\t-29\t15\t33",
  "\t-30\t15\t32",
  "\t-31\t16\t31",
  "\t-32\t16\t31",
  "\t-33\t16\t30",
  "\t-34\t16\t28",
  "\t-35\t17\t26",
  "\t-45\t36\t38",
  "\t-46\t36\t38",
  "\t-47\t36\t38",
  "South Georgia and the South Sandwich Islands",
  "\t-52\t-43\t-36",
  "\t-53\t-43\t-33",
  "\t-54\t-43\t-33",
  "\t-55\t-39\t-33\t-29\t-26",
  "\t-56\t-35\t-33\t-29\t-25",
  "\t-57\t-29\t-25",
  "\t-58\t-28\t-25",
  "\t-59\t-28\t-25",
  "\t-60\t-28\t-25",
  "South Korea",
  "\t39\t125\t129",
  "\t38\t123\t131",
  "\t37\t123\t131",
  "\t36\t123\t131",
  "\t35\t124\t130",
  "\t34\t124\t130",
  "\t33\t124\t129",
  "\t32\t125\t127",
  "Southern Ocean",
  "Spain",
  "\t44\t-10\t0",
  "\t43\t-10\t4",
  "\t42\t-10\t4",
  "\t41\t-10\t5",
  "\t40\t-9\t5",
  "\t39\t-8\t5",
  "\t38\t-8\t5",
  "\t37\t-8\t2",
  "\t36\t-8\t1",
  "\t35\t-7\t0",
  "\t34\t-6\t-1",
  "\t30\t-14\t-12",
  "\t29\t-19\t-12",
  "\t28\t-19\t-12",
  "\t27\t-19\t-12",
  "\t26\t-19\t-14",
  "Spain: Canary Islands",
  "\t30\t-14\t-12",
  "\t29\t-19\t-12",
  "\t28\t-19\t-12",
  "\t27\t-19\t-12",
  "\t26\t-19\t-14",
  "Spratly Islands",
  "\t11\t114\t115",
  "\t10\t114\t115",
  "\t9\t114\t115",
  "Sri Lanka",
  "\t10\t78\t81",
  "\t9\t78\t82",
  "\t8\t78\t82",
  "\t7\t78\t82",
  "\t6\t78\t82",
  "\t5\t78\t82",
  "\t4\t79\t81",
  "Sudan",
  "\t23\t30\t32",
  "\t22\t23\t38",
  "\t21\t23\t38",
  "\t20\t22\t38",
  "\t19\t22\t39",
  "\t18\t22\t39",
  "\t17\t22\t39",
  "\t16\t21\t39",
  "\t15\t21\t38",
  "\t14\t20\t37",
  "\t13\t20\t37",
  "\t12\t20\t37",
  "\t11\t20\t37",
  "\t10\t21\t36",
  "\t9\t21\t35",
  "\t8\t22\t35",
  "\t7\t22\t35",
  "\t6\t23\t36",
  "\t5\t25\t36",
  "\t4\t25\t36",
  "\t3\t26\t35",
  "\t2\t29\t34",
  "Suriname",
  "\t7\t-56\t-54",
  "\t6\t-58\t-52",
  "\t5\t-59\t-52",
  "\t4\t-59\t-52",
  "\t3\t-59\t-52",
  "\t2\t-59\t-52",
  "\t1\t-58\t-53",
  "\t0\t-57\t-54",
  "Svalbard",
  "\t81\t15\t28\t30\t34",
  "\t80\t9\t34",
  "\t79\t9\t34",
  "\t78\t9\t31",
  "\t77\t9\t31",
  "\t76\t12\t26",
  "\t75\t14\t20\t23\t26",
  "\t74\t17\t20",
  "\t73\t17\t20",
  "Swaziland",
  "\t-24\t30\t33",
  "\t-25\t29\t33",
  "\t-26\t29\t33",
  "\t-27\t29\t33",
  "\t-28\t29\t32",
  "Sweden",
  "\t70\t19\t21",
  "\t69\t16\t24",
  "\t68\t15\t24",
  "\t67\t13\t25",
  "\t66\t13\t25",
  "\t65\t11\t25",
  "\t64\t10\t25",
  "\t63\t10\t22",
  "\t62\t10\t21",
  "\t61\t11\t19",
  "\t60\t10\t20",
  "\t59\t10\t20",
  "\t58\t10\t20",
  "\t57\t10\t20",
  "\t56\t10\t20",
  "\t55\t11\t19",
  "\t54\t11\t15",
  "Switzerland",
  "\t48\t5\t10",
  "\t47\t4\t11",
  "\t46\t4\t11",
  "\t45\t4\t11",
  "\t44\t5\t10",
  "Syria",
  "\t38\t39\t43",
  "\t37\t35\t43",
  "\t36\t34\t43",
  "\t35\t34\t43",
  "\t34\t34\t42",
  "\t33\t34\t42",
  "\t32\t34\t40",
  "\t31\t34\t39",
  "Taiwan",
  "\t26\t120\t123",
  "\t25\t117\t123",
  "\t24\t117\t123",
  "\t23\t117\t122",
  "\t22\t118\t122",
  "\t21\t119\t122",
  "\t20\t119\t121",
  "Tajikistan",
  "\t42\t69\t71",
  "\t41\t67\t71",
  "\t40\t66\t74",
  "\t39\t66\t75",
  "\t38\t66\t76",
  "\t37\t66\t76",
  "\t36\t66\t76",
  "\t35\t66\t73",
  "Tanzania",
  "\t1\t29\t31",
  "\t0\t29\t36",
  "\t-1\t29\t38",
  "\t-2\t29\t39",
  "\t-3\t28\t40",
  "\t-4\t28\t40",
  "\t-5\t28\t40",
  "\t-6\t28\t40",
  "\t-7\t28\t40",
  "\t-8\t29\t40",
  "\t-9\t29\t41",
  "\t-10\t30\t41",
  "\t-11\t33\t41",
  "\t-12\t33\t40",
  "Tasman Sea",
  "Thailand",
  "\t21\t98\t101",
  "\t20\t96\t102",
  "\t19\t96\t105",
  "\t18\t96\t105",
  "\t17\t96\t106",
  "\t16\t96\t106",
  "\t15\t97\t106",
  "\t14\t97\t106",
  "\t13\t97\t106",
  "\t12\t97\t103",
  "\t11\t97\t103",
  "\t10\t96\t103",
  "\t9\t96\t101",
  "\t8\t96\t101",
  "\t7\t97\t103",
  "\t6\t97\t103",
  "\t5\t98\t103",
  "\t4\t99\t102",
  "Togo",
  "\t12\t-1\t1",
  "\t11\t-1\t2",
  "\t10\t-1\t2",
  "\t9\t-1\t2",
  "\t8\t-1\t2",
  "\t7\t-1\t2",
  "\t6\t-1\t2",
  "\t5\t-1\t2",
  "Tokelau",
  "\t-7\t-173\t-171",
  "\t-8\t-173\t-170",
  "\t-9\t-173\t-170",
  "\t-10\t-172\t-170",
  "Tonga",
  "\t-14\t-176\t-172",
  "\t-15\t-176\t-172",
  "\t-16\t-176\t-172",
  "\t-17\t-175\t-172",
  "\t-18\t-176\t-172",
  "\t-19\t-176\t-172",
  "\t-20\t-176\t-173",
  "\t-21\t-177\t-173",
  "\t-22\t-177\t-173",
  "\t-23\t-177\t-175",
  "Trinidad and Tobago",
  "\t12\t-61\t-59",
  "\t11\t-62\t-59",
  "\t10\t-62\t-59",
  "\t9\t-62\t-59",
  "Tromelin Island",
  "\t-15\t54\t54",
  "Tunisia",
  "\t38\t7\t12",
  "\t37\t7\t12",
  "\t36\t7\t12",
  "\t35\t6\t12",
  "\t34\t6\t12",
  "\t33\t6\t12",
  "\t32\t6\t12",
  "\t31\t7\t12",
  "\t30\t8\t11",
  "\t29\t8\t11",
  "Turkey",
  "\t43\t25\t28\t32\t36",
  "\t42\t25\t44",
  "\t41\t24\t45",
  "\t40\t24\t45",
  "\t39\t24\t45",
  "\t38\t24\t45",
  "\t37\t25\t45",
  "\t36\t26\t45",
  "\t35\t26\t41\t43\t45",
  "\t34\t34\t37",
  "Turkmenistan",
  "\t43\t51\t61",
  "\t42\t51\t62",
  "\t41\t51\t63",
  "\t40\t51\t64",
  "\t39\t51\t67",
  "\t38\t51\t67",
  "\t37\t52\t67",
  "\t36\t52\t67",
  "\t35\t59\t65",
  "\t34\t60\t65",
  "Turks and Caicos Islands",
  "\t22\t-73\t-70",
  "\t21\t-73\t-70",
  "\t20\t-73\t-70",
  "Tuvalu",
  "\t-4\t175\t177",
  "\t-5\t175\t178",
  "\t-6\t175\t179",
  "\t-7\t175\t180",
  "\t-8\t176\t180",
  "\t-9\t177\t180",
  "\t-10\t178\t180",
  "Uganda",
  "\t5\t32\t35",
  "\t4\t29\t35",
  "\t3\t29\t35",
  "\t2\t29\t36",
  "\t1\t28\t36",
  "\t0\t28\t36",
  "\t-1\t28\t35",
  "\t-2\t28\t34",
  "Ukraine",
  "\t53\t29\t35",
  "\t52\t22\t36",
  "\t51\t22\t39",
  "\t50\t21\t41",
  "\t49\t21\t41",
  "\t48\t21\t41",
  "\t47\t21\t41",
  "\t46\t21\t40",
  "\t45\t27\t38",
  "\t44\t27\t37",
  "\t43\t32\t36",
  "United Arab Emirates",
  "\t27\t55\t57",
  "\t26\t53\t57",
  "\t25\t50\t57",
  "\t24\t50\t57",
  "\t23\t50\t57",
  "\t22\t50\t56",
  "\t21\t51\t56",
  "United Kingdom",
  "\t61\t-3\t1",
  "\t60\t-4\t1",
  "\t59\t-8\t1",
  "\t58\t-14\t-12\t-9\t0",
  "\t57\t-14\t-12\t-9\t0",
  "\t56\t-14\t-12\t-9\t0",
  "\t55\t-9\t1",
  "\t54\t-9\t1",
  "\t53\t-9\t2",
  "\t52\t-6\t2",
  "\t51\t-6\t2",
  "\t50\t-7\t2",
  "\t49\t-7\t1",
  "\t48\t-7\t-4",
  "Uruguay",
  "\t-29\t-58\t-54",
  "\t-30\t-59\t-52",
  "\t-31\t-59\t-52",
  "\t-32\t-59\t-52",
  "\t-33\t-59\t-52",
  "\t-34\t-59\t-52",
  "\t-35\t-59\t-52",
  "USA",
  "\t72\t-158\t-153",
  "\t71\t-163\t-141",
  "\t70\t-164\t-140",
  "\t69\t-167\t-140",
  "\t68\t-167\t-140",
  "\t67\t-167\t-140",
  "\t66\t-169\t-140",
  "\t65\t-169\t-140",
  "\t64\t-172\t-140",
  "\t63\t-172\t-140",
  "\t62\t-172\t-140",
  "\t61\t-174\t-138",
  "\t60\t-174\t-171\t-168\t-133",
  "\t59\t-174\t-171\t-168\t-132",
  "\t58\t-171\t-131",
  "\t57\t-171\t-168\t-163\t-150\t-138\t-129",
  "\t56\t-171\t-168\t-164\t-151\t-137\t-128",
  "\t55\t-170\t-152\t-136\t-128",
  "\t54\t-170\t-154\t-135\t-128\t171\t173",
  "\t53\t-177\t-158\t-134\t-129\t171\t180",
  "\t52\t-180\t-165\t171\t180",
  "\t51\t-180\t-167\t171\t180",
  "\t50\t-180\t-174\t-96\t-93\t176\t180",
  "\t49\t-125\t-86",
  "\t48\t-125\t-84\t-70\t-66",
  "\t47\t-125\t-82\t-71\t-66",
  "\t46\t-125\t-81\t-75\t-66",
  "\t45\t-125\t-81\t-77\t-65",
  "\t44\t-125\t-65",
  "\t43\t-125\t-65",
  "\t42\t-125\t-68",
  "\t41\t-125\t-68",
  "\t40\t-125\t-68",
  "\t39\t-125\t-71",
  "\t38\t-124\t-73",
  "\t37\t-124\t-73",
  "\t36\t-124\t-74",
  "\t35\t-123\t-74",
  "\t34\t-122\t-74",
  "\t33\t-121\t-75",
  "\t32\t-121\t-76",
  "\t31\t-119\t-78",
  "\t30\t-114\t-79",
  "\t29\t-106\t-79",
  "\t28\t-105\t-79",
  "\t27\t-174\t-172\t-104\t-94\t-90\t-88\t-83\t-79",
  "\t26\t-174\t-166\t-100\t-95\t-83\t-79",
  "\t25\t-174\t-166\t-100\t-96\t-83\t-79",
  "\t24\t-172\t-160\t-98\t-96\t-83\t-79",
  "\t23\t-165\t-158\t-83\t-79",
  "\t22\t-165\t-155",
  "\t21\t-161\t-154",
  "\t20\t-161\t-153",
  "\t19\t-158\t-153",
  "\t18\t-157\t-153",
  "\t17\t-156\t-154",
  "USA: Alabama",
  "\t36\t-89\t-84",
  "\t35\t-89\t-84",
  "\t34\t-89\t-84",
  "\t33\t-89\t-83",
  "\t32\t-89\t-83",
  "\t31\t-89\t-83",
  "\t30\t-89\t-83",
  "\t29\t-89\t-86",
  "USA: Alaska",
  "\t72\t-158\t-153",
  "\t71\t-163\t-141",
  "\t70\t-164\t-140",
  "\t69\t-167\t-140",
  "\t68\t-167\t-140",
  "\t67\t-167\t-140",
  "\t66\t-169\t-140",
  "\t65\t-169\t-140",
  "\t64\t-172\t-140",
  "\t63\t-172\t-140",
  "\t62\t-172\t-140",
  "\t61\t-174\t-138",
  "\t60\t-174\t-171\t-168\t-133",
  "\t59\t-174\t-171\t-168\t-132",
  "\t58\t-171\t-131",
  "\t57\t-171\t-168\t-163\t-150\t-138\t-129",
  "\t56\t-171\t-168\t-164\t-151\t-137\t-128",
  "\t55\t-170\t-152\t-136\t-128",
  "\t54\t-170\t-154\t-135\t-128\t171\t173",
  "\t53\t-177\t-158\t-134\t-129\t171\t180",
  "\t52\t-180\t-165\t171\t180",
  "\t51\t-180\t-167\t171\t180",
  "\t50\t-180\t-174\t176\t180",
  "USA: Alaska, Aleutian Islands",
  "\t60\t-154\t-149\t-147\t-145",
  "\t59\t-162\t-159\t-154\t-149\t-147\t-145",
  "\t58\t-171\t-169\t-162\t-159\t-155\t-149\t-147\t-145",
  "\t57\t-171\t-168\t-162\t-150",
  "\t56\t-171\t-168\t-164\t-151",
  "\t55\t-170\t-152",
  "\t54\t-170\t-154",
  "\t53\t-177\t-158",
  "\t52\t-180\t-165",
  "\t51\t-180\t-167",
  "\t50\t-180\t-174",
  "USA: Arizona",
  "\t38\t-115\t-108",
  "\t37\t-115\t-108",
  "\t36\t-115\t-108",
  "\t35\t-115\t-108",
  "\t34\t-115\t-108",
  "\t33\t-115\t-108",
  "\t32\t-115\t-108",
  "\t31\t-115\t-108",
  "\t30\t-114\t-108",
  "USA: Arkansas",
  "\t37\t-95\t-88",
  "\t36\t-95\t-88",
  "\t35\t-95\t-88",
  "\t34\t-95\t-88",
  "\t33\t-95\t-89",
  "\t32\t-95\t-90",
  "USA: California",
  "\t43\t-125\t-119",
  "\t42\t-125\t-119",
  "\t41\t-125\t-119",
  "\t40\t-125\t-119",
  "\t39\t-125\t-117",
  "\t38\t-124\t-116",
  "\t37\t-124\t-115",
  "\t36\t-124\t-113",
  "\t35\t-123\t-113",
  "\t34\t-122\t-113",
  "\t33\t-121\t-113",
  "\t32\t-121\t-113",
  "\t31\t-119\t-113",
  "USA: Colorado",
  "\t42\t-110\t-101",
  "\t41\t-110\t-101",
  "\t40\t-110\t-101",
  "\t39\t-110\t-101",
  "\t38\t-110\t-101",
  "\t37\t-110\t-101",
  "\t36\t-110\t-101",
  "USA: Connecticut",
  "\t43\t-74\t-70",
  "\t42\t-74\t-70",
  "\t41\t-74\t-70",
  "\t40\t-74\t-70",
  "USA: Delaware",
  "\t40\t-76\t-74",
  "\t39\t-76\t-74",
  "\t38\t-76\t-74",
  "\t37\t-76\t-74",
  "USA: District of Columbia",
  "\t40\t-78\t-76",
  "\t39\t-78\t-75",
  "\t38\t-78\t-75",
  "\t37\t-78\t-75",
  "USA: Florida",
  "\t32\t-88\t-84",
  "\t31\t-88\t-80",
  "\t30\t-88\t-79",
  "\t29\t-88\t-79",
  "\t28\t-86\t-79",
  "\t27\t-83\t-79",
  "\t26\t-83\t-79",
  "\t25\t-83\t-79",
  "\t24\t-83\t-79",
  "\t23\t-83\t-79",
  "USA: Georgia",
  "\t36\t-86\t-82",
  "\t35\t-86\t-81",
  "\t34\t-86\t-80",
  "\t33\t-86\t-79",
  "\t32\t-86\t-79",
  "\t31\t-86\t-79",
  "\t30\t-86\t-79",
  "\t29\t-86\t-80",
  "USA: Hawaii",
  "\t27\t-174\t-172",
  "\t26\t-174\t-166",
  "\t25\t-174\t-166",
  "\t24\t-172\t-160",
  "\t23\t-165\t-158",
  "\t22\t-165\t-155",
  "\t21\t-161\t-154",
  "\t20\t-161\t-153",
  "\t19\t-158\t-153",
  "\t18\t-157\t-153",
  "\t17\t-156\t-154",
  "USA: Idaho",
  "\t49\t-118\t-115",
  "\t48\t-118\t-114",
  "\t47\t-118\t-113",
  "\t46\t-118\t-112",
  "\t45\t-118\t-110",
  "\t44\t-118\t-110",
  "\t43\t-118\t-110",
  "\t42\t-118\t-110",
  "\t41\t-118\t-110",
  "USA: Illinois",
  "\t43\t-91\t-86",
  "\t42\t-92\t-86",
  "\t41\t-92\t-86",
  "\t40\t-92\t-86",
  "\t39\t-92\t-86",
  "\t38\t-92\t-86",
  "\t37\t-91\t-86",
  "\t36\t-91\t-87",
  "\t35\t-90\t-88",
  "USA: Indiana",
  "\t42\t-88\t-83",
  "\t41\t-88\t-83",
  "\t40\t-88\t-83",
  "\t39\t-89\t-83",
  "\t38\t-89\t-83",
  "\t37\t-89\t-83",
  "\t36\t-89\t-85",
  "USA: Iowa",
  "\t44\t-97\t-90",
  "\t43\t-97\t-89",
  "\t42\t-97\t-89",
  "\t41\t-97\t-89",
  "\t40\t-97\t-89",
  "\t39\t-96\t-89",
  "USA: Kansas",
  "\t41\t-103\t-94",
  "\t40\t-103\t-93",
  "\t39\t-103\t-93",
  "\t38\t-103\t-93",
  "\t37\t-103\t-93",
  "\t36\t-103\t-93",
  "USA: Kentucky",
  "\t40\t-85\t-83",
  "\t39\t-87\t-81",
  "\t38\t-90\t-80",
  "\t37\t-90\t-80",
  "\t36\t-90\t-80",
  "\t35\t-90\t-81",
  "USA: Louisiana",
  "\t34\t-95\t-90",
  "\t33\t-95\t-89",
  "\t32\t-95\t-88",
  "\t31\t-95\t-87",
  "\t30\t-95\t-87",
  "\t29\t-94\t-87",
  "\t28\t-94\t-87",
  "\t27\t-90\t-88",
  "USA: Maine",
  "\t48\t-70\t-66",
  "\t47\t-71\t-66",
  "\t46\t-72\t-66",
  "\t45\t-72\t-65",
  "\t44\t-72\t-65",
  "\t43\t-72\t-65",
  "\t42\t-71\t-68",
  "USA: Maryland",
  "\t40\t-80\t-74",
  "\t39\t-80\t-74",
  "\t38\t-80\t-74",
  "\t37\t-78\t-74",
  "\t36\t-77\t-74",
  "USA: Massachusetts",
  "\t43\t-74\t-69",
  "\t42\t-74\t-68",
  "\t41\t-74\t-68",
  "\t40\t-72\t-68",
  "USA: Michigan",
  "\t49\t-90\t-86",
  "\t48\t-91\t-84",
  "\t47\t-91\t-82",
  "\t46\t-91\t-81",
  "\t45\t-91\t-81",
  "\t44\t-89\t-81",
  "\t43\t-88\t-81",
  "\t42\t-88\t-81",
  "\t41\t-88\t-81",
  "\t40\t-88\t-82",
  "USA: Minnesota",
  "\t50\t-96\t-93",
  "\t49\t-98\t-88",
  "\t48\t-98\t-88",
  "\t47\t-98\t-88",
  "\t46\t-98\t-88",
  "\t45\t-97\t-90",
  "\t44\t-97\t-90",
  "\t43\t-97\t-90",
  "\t42\t-97\t-90",
  "USA: Mississippi",
  "\t36\t-91\t-88",
  "\t35\t-92\t-87",
  "\t34\t-92\t-87",
  "\t33\t-92\t-87",
  "\t32\t-92\t-87",
  "\t31\t-92\t-87",
  "\t30\t-92\t-87",
  "\t29\t-90\t-87",
  "USA: Missouri",
  "\t41\t-96\t-90",
  "\t40\t-96\t-89",
  "\t39\t-96\t-89",
  "\t38\t-96\t-88",
  "\t37\t-95\t-88",
  "\t36\t-95\t-88",
  "\t35\t-95\t-88",
  "\t34\t-91\t-88",
  "USA: Montana",
  "\t49\t-117\t-103",
  "\t48\t-117\t-103",
  "\t47\t-117\t-103",
  "\t46\t-117\t-103",
  "\t45\t-116\t-103",
  "\t44\t-115\t-103",
  "\t43\t-114\t-110",
  "USA: Nebraska",
  "\t44\t-105\t-97",
  "\t43\t-105\t-95",
  "\t42\t-105\t-94",
  "\t41\t-105\t-94",
  "\t40\t-105\t-94",
  "\t39\t-103\t-94",
  "USA: Nevada",
  "\t43\t-121\t-113",
  "\t42\t-121\t-113",
  "\t41\t-121\t-113",
  "\t40\t-121\t-113",
  "\t39\t-121\t-113",
  "\t38\t-121\t-113",
  "\t37\t-120\t-113",
  "\t36\t-119\t-113",
  "\t35\t-118\t-113",
  "\t34\t-116\t-113",
  "USA: New Hampshire",
  "\t46\t-72\t-70",
  "\t45\t-73\t-69",
  "\t44\t-73\t-69",
  "\t43\t-73\t-69",
  "\t42\t-73\t-69",
  "\t41\t-73\t-69",
  "USA: New Jersey",
  "\t42\t-76\t-73",
  "\t41\t-76\t-72",
  "\t40\t-76\t-72",
  "\t39\t-76\t-72",
  "\t38\t-76\t-73",
  "\t37\t-75\t-73",
  "USA: New Mexico",
  "\t38\t-110\t-102",
  "\t37\t-110\t-102",
  "\t36\t-110\t-102",
  "\t35\t-110\t-102",
  "\t34\t-110\t-102",
  "\t33\t-110\t-102",
  "\t32\t-110\t-102",
  "\t31\t-110\t-102",
  "\t30\t-110\t-105",
  "USA: New York",
  "\t46\t-75\t-72",
  "\t45\t-77\t-72",
  "\t44\t-80\t-72",
  "\t43\t-80\t-72",
  "\t42\t-80\t-70",
  "\t41\t-80\t-70",
  "\t40\t-79\t-70",
  "\t39\t-75\t-71",
  "USA: North Carolina",
  "\t37\t-83\t-74",
  "\t36\t-85\t-74",
  "\t35\t-85\t-74",
  "\t34\t-85\t-74",
  "\t33\t-85\t-75",
  "\t32\t-79\t-76",
  "USA: North Dakota",
  "\t49\t-105\t-96",
  "\t48\t-105\t-95",
  "\t47\t-105\t-95",
  "\t46\t-105\t-95",
  "\t45\t-105\t-95",
  "\t44\t-105\t-95",
  "USA: Ohio",
  "\t43\t-82\t-79",
  "\t42\t-85\t-79",
  "\t41\t-85\t-79",
  "\t40\t-85\t-79",
  "\t39\t-85\t-79",
  "\t38\t-85\t-79",
  "\t37\t-85\t-80",
  "USA: Oklahoma",
  "\t38\t-104\t-93",
  "\t37\t-104\t-93",
  "\t36\t-104\t-93",
  "\t35\t-104\t-93",
  "\t34\t-101\t-93",
  "\t33\t-101\t-93",
  "\t32\t-98\t-93",
  "USA: Oregon",
  "\t47\t-124\t-115",
  "\t46\t-125\t-115",
  "\t45\t-125\t-115",
  "\t44\t-125\t-115",
  "\t43\t-125\t-115",
  "\t42\t-125\t-115",
  "\t41\t-125\t-116",
  "USA: Pennsylvania",
  "\t43\t-81\t-77",
  "\t42\t-81\t-73",
  "\t41\t-81\t-73",
  "\t40\t-81\t-73",
  "\t39\t-81\t-73",
  "\t38\t-81\t-74",
  "USA: Rhode Island",
  "\t43\t-72\t-70",
  "\t42\t-72\t-70",
  "\t41\t-72\t-70",
  "\t40\t-72\t-70",
  "USA: South Carolina",
  "\t36\t-84\t-79",
  "\t35\t-84\t-77",
  "\t34\t-84\t-77",
  "\t33\t-84\t-77",
  "\t32\t-83\t-77",
  "\t31\t-82\t-78",
  "USA: South Dakota",
  "\t46\t-105\t-95",
  "\t45\t-105\t-95",
  "\t44\t-105\t-95",
  "\t43\t-105\t-95",
  "\t42\t-105\t-95",
  "\t41\t-99\t-95",
  "USA: Tennessee",
  "\t37\t-90\t-80",
  "\t36\t-91\t-80",
  "\t35\t-91\t-80",
  "\t34\t-91\t-81",
  "\t33\t-90\t-87\t-85\t-83",
  "USA: Texas",
  "\t37\t-104\t-99",
  "\t36\t-104\t-99",
  "\t35\t-104\t-97",
  "\t34\t-104\t-93",
  "\t33\t-107\t-93",
  "\t32\t-107\t-92",
  "\t31\t-107\t-92",
  "\t30\t-107\t-92",
  "\t29\t-106\t-92",
  "\t28\t-105\t-92",
  "\t27\t-104\t-94",
  "\t26\t-100\t-95",
  "\t25\t-100\t-96",
  "\t24\t-98\t-96",
  "USA: Utah",
  "\t43\t-115\t-110",
  "\t42\t-115\t-108",
  "\t41\t-115\t-108",
  "\t40\t-115\t-108",
  "\t39\t-115\t-108",
  "\t38\t-115\t-108",
  "\t37\t-115\t-108",
  "\t36\t-115\t-108",
  "USA: Vermont",
  "\t46\t-74\t-70",
  "\t45\t-74\t-70",
  "\t44\t-74\t-70",
  "\t43\t-74\t-70",
  "\t42\t-74\t-71",
  "\t41\t-74\t-71",
  "USA: Virginia",
  "\t40\t-79\t-76",
  "\t39\t-81\t-74",
  "\t38\t-83\t-74",
  "\t37\t-84\t-74",
  "\t36\t-84\t-74",
  "\t35\t-84\t-74",
  "USA: Washington",
  "\t49\t-125\t-116",
  "\t48\t-125\t-116",
  "\t47\t-125\t-115",
  "\t46\t-125\t-115",
  "\t45\t-125\t-115",
  "\t44\t-123\t-118",
  "USA: West Virginia",
  "\t41\t-81\t-79",
  "\t40\t-83\t-76",
  "\t39\t-83\t-76",
  "\t38\t-83\t-76",
  "\t37\t-83\t-77",
  "\t36\t-83\t-79",
  "USA: Wisconsin",
  "\t48\t-92\t-88",
  "\t47\t-93\t-87",
  "\t46\t-93\t-85",
  "\t45\t-93\t-85",
  "\t44\t-93\t-85",
  "\t43\t-93\t-85",
  "\t42\t-92\t-86",
  "\t41\t-92\t-86",
  "USA: Wyoming",
  "\t46\t-112\t-103",
  "\t45\t-112\t-103",
  "\t44\t-112\t-103",
  "\t43\t-112\t-103",
  "\t42\t-112\t-103",
  "\t41\t-112\t-103",
  "\t40\t-112\t-103",
  "Uzbekistan",
  "\t46\t55\t60",
  "\t45\t54\t62",
  "\t44\t54\t66",
  "\t43\t54\t67\t69\t72",
  "\t42\t54\t73",
  "\t41\t54\t74",
  "\t40\t54\t74",
  "\t39\t60\t74",
  "\t38\t61\t72",
  "\t37\t63\t69",
  "\t36\t65\t69",
  "Vanuatu",
  "\t-12\t165\t168",
  "\t-13\t165\t169",
  "\t-14\t165\t169",
  "\t-15\t165\t169",
  "\t-16\t165\t169",
  "\t-17\t166\t170",
  "\t-18\t167\t170",
  "\t-19\t167\t170",
  "\t-20\t168\t170",
  "\t-21\t168\t170",
  "Venezuela",
  "\t13\t-71\t-66",
  "\t12\t-73\t-62",
  "\t11\t-73\t-60",
  "\t10\t-74\t-59",
  "\t9\t-74\t-58",
  "\t8\t-74\t-58",
  "\t7\t-73\t-58",
  "\t6\t-73\t-59",
  "\t5\t-72\t-59",
  "\t4\t-68\t-59",
  "\t3\t-68\t-59",
  "\t2\t-68\t-61",
  "\t1\t-68\t-62",
  "\t0\t-68\t-62",
  "\t-1\t-67\t-64",
  "Viet Nam",
  "\t24\t103\t106",
  "\t23\t101\t107",
  "\t22\t101\t108",
  "\t21\t101\t108",
  "\t20\t101\t108",
  "\t19\t102\t108",
  "\t18\t102\t108",
  "\t17\t103\t109",
  "\t16\t104\t109",
  "\t15\t105\t110",
  "\t14\t106\t110",
  "\t13\t105\t110",
  "\t12\t104\t110",
  "\t11\t102\t110",
  "\t10\t102\t110",
  "\t9\t102\t109",
  "\t8\t103\t107",
  "\t7\t103\t107",
  "Virgin Islands",
  "\t19\t-66\t-63",
  "\t18\t-66\t-63",
  "\t17\t-66\t-63",
  "\t16\t-65\t-63",
  "Wake Island",
  "\t19\t166\t166",
  "Wallis and Futuna",
  "\t-12\t-177\t-175",
  "\t-13\t-179\t-175",
  "\t-14\t-179\t-175",
  "\t-15\t-179\t-176",
  "West Bank",
  "\t33\t33\t36",
  "\t32\t33\t36",
  "\t31\t33\t36",
  "\t30\t33\t36",
  "Western Sahara",
  "\t28\t-11\t-7",
  "\t27\t-13\t-7",
  "\t26\t-13\t-7",
  "\t25\t-14\t-7",
  "\t24\t-15\t-7",
  "\t23\t-15\t-11",
  "\t22\t-18\t-11",
  "\t21\t-18\t-12",
  "\t20\t-18\t-12",
  "\t19\t-18\t-16",
  "Yemen",
  "\t19\t47\t53",
  "\t18\t42\t53",
  "\t17\t41\t54",
  "\t16\t41\t54",
  "\t15\t41\t54",
  "\t14\t41\t53",
  "\t13\t41\t55",
  "\t12\t41\t49\t51\t55",
  "\t11\t42\t46\t51\t55",
  "Zambia",
  "\t-7\t27\t32",
  "\t-8\t27\t34",
  "\t-9\t22\t25\t27\t34",
  "\t-10\t22\t34",
  "\t-11\t22\t34",
  "\t-12\t20\t34",
  "\t-13\t20\t34",
  "\t-14\t20\t34",
  "\t-15\t20\t34",
  "\t-16\t20\t31",
  "\t-17\t20\t29",
  "\t-18\t21\t28",
  "\t-19\t24\t27",
  "Zimbabwe",
  "\t-14\t27\t32",
  "\t-15\t26\t33",
  "\t-16\t24\t34",
  "\t-17\t24\t34",
  "\t-18\t24\t34",
  "\t-19\t24\t34",
  "\t-20\t24\t34",
  "\t-21\t25\t34",
  "\t-22\t26\t33",
  "\t-23\t28\t32"
};

static const int k_NumLatLonCountryText = sizeof (s_DefaultLatLonCountryText) / sizeof (char *);

static const char * s_DefaultLatLonWaterText[] = {
  "1",
  "Adriatic Sea",
  "\t46\t11\t15",
  "\t45\t11\t16",
  "\t44\t11\t18",
  "\t43\t11\t20",
  "\t42\t11\t20",
  "\t41\t12\t20",
  "\t40\t14\t20",
  "\t39\t16\t20",
  "\t38\t17\t20",
  "Aegean Sea",
  "\t41\t21\t27",
  "\t40\t21\t27",
  "\t39\t21\t28",
  "\t38\t21\t29",
  "\t37\t21\t29",
  "\t36\t23\t29",
  "\t35\t23\t29",
  "Albemarle Sound",
  "\t37\t-77\t-74",
  "\t36\t-77\t-74",
  "\t35\t-77\t-74",
  "\t34\t-77\t-74",
  "Alboran Sea",
  "\t37\t-6\t-1",
  "\t36\t-6\t0",
  "\t35\t-6\t0",
  "\t34\t-6\t0",
  "Amundsen Gulf",
  "\t72\t-126\t-117",
  "\t71\t-128\t-116",
  "\t70\t-128\t-116",
  "\t69\t-128\t-116",
  "\t68\t-127\t-117",
  "Amundsen Sea",
  "\t-71\t-108\t-101",
  "\t-72\t-115\t-97",
  "\t-73\t-115\t-97",
  "\t-74\t-115\t-97",
  "\t-75\t-115\t-97",
  "\t-76\t-112\t-97",
  "Andaman Sea",
  "\t17\t93\t96",
  "\t16\t92\t97",
  "\t15\t91\t99",
  "\t14\t91\t99",
  "\t13\t91\t99",
  "\t12\t91\t99",
  "\t11\t91\t99",
  "\t10\t91\t99",
  "\t9\t91\t99",
  "\t8\t91\t99",
  "\t7\t91\t99",
  "\t6\t92\t99",
  "\t5\t92\t98",
  "\t4\t94\t96",
  "Arabian Sea",
  "\t26\t60\t67",
  "\t25\t59\t68",
  "\t24\t59\t69",
  "\t23\t58\t70",
  "\t22\t57\t71",
  "\t21\t57\t72",
  "\t20\t56\t74",
  "\t19\t55\t74",
  "\t18\t53\t74",
  "\t17\t51\t74",
  "\t16\t50\t74",
  "\t15\t50\t75",
  "\t14\t50\t75",
  "\t13\t50\t75",
  "\t12\t50\t74",
  "\t11\t50\t72",
  "\t10\t50\t72",
  "\t9\t50\t72",
  "\t8\t52\t72",
  "\t7\t54\t73",
  "\t6\t56\t73",
  "\t5\t58\t73",
  "\t4\t60\t73",
  "\t3\t63\t73",
  "\t2\t65\t73",
  "\t1\t67\t74",
  "\t0\t69\t74",
  "\t-1\t71\t74",
  "Arafura Sea",
  "\t-2\t132\t135",
  "\t-3\t132\t138",
  "\t-4\t131\t139",
  "\t-5\t131\t139",
  "\t-6\t130\t141",
  "\t-7\t129\t141",
  "\t-8\t129\t142",
  "\t-9\t129\t143",
  "\t-10\t129\t143",
  "\t-11\t130\t143",
  "\t-12\t130\t143",
  "\t-13\t133\t142",
  "Aral Sea",
  "\t47\t58\t62",
  "\t46\t57\t62",
  "\t45\t57\t62",
  "\t44\t57\t61",
  "\t43\t57\t61",
  "Arctic Ocean",
  "\t84\t-78\t-17",
  "\t83\t-89\t-3",
  "\t82\t-96\t11\t49\t101",
  "\t81\t-101\t109",
  "\t80\t-108\t118",
  "\t79\t-115\t126",
  "\t78\t-120\t134\t147\t158",
  "\t77\t-126\t162",
  "\t76\t-133\t166",
  "\t75\t-139\t170",
  "\t74\t-146\t174",
  "\t73\t-153\t178",
  "\t72\t-180\t180",
  "\t71\t-180\t180",
  "\t70\t-180\t180",
  "Atlantic Ocean",
  "\t69\t-33\t-29",
  "\t68\t-34\t-27",
  "\t67\t-39\t-26",
  "\t66\t-42\t-24",
  "\t65\t-42\t-20\t-17\t-11",
  "\t64\t-43\t-8",
  "\t63\t-43\t-6",
  "\t62\t-43\t-3",
  "\t61\t-44\t0",
  "\t60\t-44\t0",
  "\t59\t-45\t0",
  "\t58\t-45\t0",
  "\t57\t-46\t-1",
  "\t56\t-47\t-5",
  "\t55\t-48\t-6",
  "\t54\t-48\t-7",
  "\t53\t-49\t-5",
  "\t52\t-50\t-5",
  "\t51\t-51\t-4",
  "\t50\t-51\t-4",
  "\t49\t-52\t-4",
  "\t48\t-56\t-4",
  "\t47\t-60\t-4",
  "\t46\t-62\t-5",
  "\t45\t-65\t-5",
  "\t44\t-66\t-6",
  "\t43\t-68\t-6",
  "\t42\t-74\t-6",
  "\t41\t-75\t-7",
  "\t40\t-75\t-7",
  "\t39\t-76\t-7",
  "\t38\t-76\t-5",
  "\t37\t-77\t-4",
  "\t36\t-77\t-4",
  "\t35\t-78\t-4",
  "\t34\t-80\t-4",
  "\t33\t-81\t-5",
  "\t32\t-82\t-5",
  "\t31\t-82\t-7",
  "\t30\t-82\t-8",
  "\t29\t-82\t-8",
  "\t28\t-82\t-8",
  "\t27\t-81\t-9",
  "\t26\t-81\t-11",
  "\t25\t-81\t-12",
  "\t24\t-81\t-13",
  "\t23\t-81\t-13",
  "\t22\t-81\t-14",
  "\t21\t-81\t-15",
  "\t20\t-78\t-15",
  "\t19\t-76\t-15",
  "\t18\t-74\t-15",
  "\t17\t-69\t-15",
  "\t16\t-62\t-15",
  "\t15\t-61\t-15",
  "\t14\t-61\t-14",
  "\t13\t-60\t-14",
  "\t12\t-61\t-14",
  "\t11\t-61\t-13",
  "\t10\t-62\t-12",
  "\t9\t-62\t-11",
  "\t8\t-62\t-10",
  "\t7\t-61\t-9",
  "\t6\t-59\t-8",
  "\t5\t-59\t-5",
  "\t4\t-58\t-2",
  "\t3\t-53\t2",
  "\t2\t-52\t5",
  "\t1\t-52\t7",
  "\t0\t-52\t7",
  "\t-1\t-52\t7",
  "Atlantic Ocean",
  "\t1\t-50\t9",
  "\t0\t-50\t10",
  "\t-1\t-50\t11",
  "\t-2\t-47\t12",
  "\t-3\t-44\t13",
  "\t-4\t-40\t14",
  "\t-5\t-39\t14",
  "\t-6\t-37\t14",
  "\t-7\t-36\t14",
  "\t-8\t-36\t14",
  "\t-9\t-37\t14",
  "\t-10\t-38\t14",
  "\t-11\t-39\t14",
  "\t-12\t-40\t14",
  "\t-13\t-40\t14",
  "\t-14\t-40\t13",
  "\t-15\t-40\t13",
  "\t-16\t-40\t13",
  "\t-17\t-40\t13",
  "\t-18\t-41\t13",
  "\t-19\t-41\t14",
  "\t-20\t-42\t14",
  "\t-21\t-45\t15",
  "\t-22\t-46\t15",
  "\t-23\t-48\t15",
  "\t-24\t-49\t15",
  "\t-25\t-49\t16",
  "\t-26\t-49\t16",
  "\t-27\t-50\t17",
  "\t-28\t-51\t17",
  "\t-29\t-51\t18",
  "\t-30\t-52\t19",
  "\t-31\t-53\t19",
  "\t-32\t-54\t19",
  "\t-33\t-55\t20",
  "\t-34\t-56\t20",
  "\t-35\t-57\t20",
  "\t-36\t-58\t20",
  "\t-37\t-62\t20",
  "\t-38\t-63\t20",
  "\t-39\t-63\t20",
  "\t-40\t-64\t20",
  "\t-41\t-66\t20",
  "\t-42\t-66\t20",
  "\t-43\t-66\t20",
  "\t-44\t-66\t20",
  "\t-45\t-66\t20",
  "\t-46\t-67\t20",
  "\t-47\t-68\t20",
  "\t-48\t-68\t20",
  "\t-49\t-68\t20",
  "\t-50\t-69\t20",
  "\t-51\t-69\t20",
  "\t-52\t-69\t20",
  "\t-53\t-70\t20",
  "\t-54\t-70\t20",
  "\t-55\t-70\t20",
  "\t-56\t-69\t20",
  "\t-57\t-69\t20",
  "\t-58\t-69\t20",
  "\t-59\t-69\t20",
  "\t-60\t-69\t20",
  "\t-61\t-69\t20",
  "Bab el Mandeb",
  "\t14\t42\t44",
  "\t13\t42\t44",
  "\t12\t42\t44",
  "\t11\t42\t44",
  "Baffin Bay",
  "\t79\t-77\t-71",
  "\t78\t-83\t-66",
  "\t77\t-83\t-66",
  "\t76\t-83\t-61",
  "\t75\t-81\t-55",
  "\t74\t-81\t-54",
  "\t73\t-81\t-53",
  "\t72\t-79\t-53",
  "\t71\t-78\t-53",
  "\t70\t-76\t-53",
  "\t69\t-73\t-53",
  "\t68\t-70\t-53",
  "Bahia Blanca",
  "\t-37\t-63\t-60",
  "\t-38\t-63\t-60",
  "\t-39\t-63\t-60",
  "\t-40\t-63\t-60",
  "Bahia de Campeche",
  "\t22\t-94\t-89",
  "\t21\t-98\t-89",
  "\t20\t-98\t-89",
  "\t19\t-98\t-89",
  "\t18\t-97\t-89",
  "\t17\t-96\t-90",
  "Bahia Grande",
  "\t-48\t-69\t-66",
  "\t-49\t-70\t-66",
  "\t-50\t-70\t-66",
  "\t-51\t-70\t-66",
  "\t-52\t-70\t-67",
  "\t-53\t-69\t-67",
  "Baia de Maputo",
  "\t-24\t31\t33",
  "\t-25\t31\t33",
  "\t-26\t31\t33",
  "\t-27\t31\t33",
  "Baia de Marajo",
  "\t1\t-49\t-47",
  "\t0\t-50\t-47",
  "\t-1\t-50\t-47",
  "\t-2\t-50\t-47",
  "\t-3\t-50\t-48",
  "Baia de Sao Marcos",
  "\t0\t-45\t-43",
  "\t-1\t-45\t-42",
  "\t-2\t-45\t-42",
  "\t-3\t-45\t-42",
  "\t-4\t-45\t-43",
  "Balearic Sea",
  "\t42\t0\t4",
  "\t41\t-1\t5",
  "\t40\t-1\t5",
  "\t39\t-1\t5",
  "\t38\t-1\t5",
  "\t37\t-1\t3",
  "Baltic Sea",
  "\t60\t16\t24",
  "\t59\t15\t24",
  "\t58\t15\t24",
  "\t57\t13\t23",
  "\t56\t11\t23",
  "\t55\t8\t22",
  "\t54\t8\t22",
  "\t53\t8\t21",
  "\t52\t9\t15",
  "Banda Sea",
  "\t1\t121\t124",
  "\t0\t120\t126",
  "\t-1\t119\t129",
  "\t-2\t119\t131",
  "\t-3\t119\t133",
  "\t-4\t119\t134",
  "\t-5\t119\t134",
  "\t-6\t119\t134",
  "\t-7\t119\t133",
  "\t-8\t119\t132",
  "\t-9\t121\t132",
  "Barents Sea",
  "\t82\t49\t66",
  "\t81\t16\t19\t26\t66",
  "\t80\t16\t67",
  "\t79\t16\t67",
  "\t78\t16\t68",
  "\t77\t16\t69",
  "\t76\t16\t69",
  "\t75\t16\t69",
  "\t74\t18\t61",
  "\t73\t20\t57",
  "\t72\t22\t55",
  "\t71\t24\t59",
  "\t70\t26\t61",
  "\t69\t26\t61",
  "\t68\t28\t61",
  "\t67\t36\t61",
  "\t66\t43\t50",
  "\t65\t44\t48",
  "Bass Strait",
  "\t-36\t143\t150",
  "\t-37\t142\t150",
  "\t-38\t142\t150",
  "\t-39\t142\t149",
  "\t-40\t142\t149",
  "\t-41\t142\t149",
  "\t-42\t144\t148",
  "Bay of Bengal",
  "\t24\t89\t91",
  "\t23\t86\t92",
  "\t22\t85\t93",
  "\t21\t85\t94",
  "\t20\t83\t95",
  "\t19\t82\t95",
  "\t18\t81\t95",
  "\t17\t80\t95",
  "\t16\t79\t95",
  "\t15\t79\t95",
  "\t14\t79\t94",
  "\t13\t78\t93",
  "\t12\t78\t93",
  "\t11\t78\t93",
  "\t10\t78\t93",
  "\t9\t78\t93",
  "\t8\t79\t94",
  "\t7\t79\t95",
  "\t6\t80\t96",
  "\t5\t84\t96",
  "\t4\t91\t96",
  "Bay of Biscay",
  "\t49\t-6\t-3",
  "\t48\t-7\t0",
  "\t47\t-7\t0",
  "\t46\t-8\t1",
  "\t45\t-8\t1",
  "\t44\t-9\t1",
  "\t43\t-9\t0",
  "\t42\t-9\t0",
  "Bay of Fundy",
  "\t46\t-68\t-62",
  "\t45\t-68\t-62",
  "\t44\t-68\t-62",
  "\t43\t-68\t-64",
  "Bay of Plenty",
  "\t-35\t174\t177",
  "\t-36\t174\t179",
  "\t-37\t174\t179",
  "\t-38\t174\t179",
  "Beaufort Sea",
  "\t77\t-126\t-121",
  "\t76\t-133\t-121",
  "\t75\t-139\t-121",
  "\t74\t-146\t-122",
  "\t73\t-153\t-122",
  "\t72\t-157\t-122",
  "\t71\t-157\t-123",
  "\t70\t-157\t-124",
  "\t69\t-157\t-125",
  "\t68\t-145\t-127",
  "Bellingshausen Sea",
  "\t-67\t-74\t-70",
  "\t-68\t-80\t-70",
  "\t-69\t-86\t-68",
  "\t-70\t-92\t-68",
  "\t-71\t-96\t-68",
  "\t-72\t-96\t-68",
  "\t-73\t-96\t-73",
  "\t-74\t-96\t-73",
  "Bering Sea",
  "\t67\t-171\t-168",
  "\t66\t-173\t-165",
  "\t65\t-174\t-163",
  "\t64\t-174\t-163",
  "\t63\t-175\t-163",
  "\t62\t-175\t-163",
  "\t61\t-176\t-160",
  "\t60\t-176\t-160",
  "\t59\t-177\t-160",
  "\t58\t-177\t-160",
  "\t57\t-178\t-160",
  "\t56\t-178\t-160",
  "\t55\t-179\t-160",
  "\t54\t-179\t-160",
  "\t53\t-180\t-161",
  "\t52\t-180\t-163",
  "\t51\t-180\t-166",
  "\t50\t-180\t-171",
  "Bering Sea",
  "\t63\t174\t180",
  "\t62\t171\t180",
  "\t61\t165\t180",
  "\t60\t165\t180",
  "\t59\t163\t180",
  "\t58\t161\t180",
  "\t57\t161\t180",
  "\t56\t161\t180",
  "\t55\t161\t180",
  "\t54\t163\t180",
  "\t53\t165\t180",
  "\t52\t167\t180",
  "\t51\t169\t180",
  "\t50\t171\t180",
  "\t49\t178\t180",
  "Bering Strait",
  "\t67\t-171\t-168",
  "\t66\t-171\t-166",
  "\t65\t-171\t-166",
  "\t64\t-171\t-166",
  "Bight of Benin",
  "\t7\t0\t5",
  "\t6\t-1\t6",
  "\t5\t-1\t6",
  "\t4\t-1\t6",
  "\t3\t2\t6",
  "Bight of Biafra",
  "\t5\t5\t10",
  "\t4\t5\t10",
  "\t3\t5\t10",
  "\t2\t7\t10",
  "\t1\t8\t10",
  "Bismarck Sea",
  "\t0\t141\t148",
  "\t-1\t140\t152",
  "\t-2\t140\t153",
  "\t-3\t140\t153",
  "\t-4\t141\t153",
  "\t-5\t143\t153",
  "\t-6\t144\t152",
  "Black Sea",
  "\t48\t30\t32",
  "\t47\t29\t34",
  "\t46\t28\t37",
  "\t45\t27\t39",
  "\t44\t26\t41",
  "\t43\t26\t42",
  "\t42\t26\t42",
  "\t41\t26\t42",
  "\t40\t26\t42",
  "\t39\t37\t41",
  "Bo Hai",
  "\t41\t119\t123",
  "\t40\t116\t123",
  "\t39\t116\t123",
  "\t38\t116\t122",
  "\t37\t116\t122",
  "\t36\t117\t121",
  "Boca Grande",
  "\t10\t-62\t-59",
  "\t9\t-62\t-59",
  "\t8\t-62\t-59",
  "\t7\t-62\t-59",
  "Boknafjorden",
  "\t60\t4\t7",
  "\t59\t4\t7",
  "\t58\t4\t7",
  "\t57\t4\t7",
  "Bosporus",
  "\t42\t27\t30",
  "\t41\t27\t30",
  "\t40\t27\t30",
  "Bransfield Strait",
  "\t-60\t-58\t-53",
  "\t-61\t-63\t-53",
  "\t-62\t-63\t-53",
  "\t-63\t-64\t-53",
  "\t-64\t-64\t-54",
  "\t-65\t-64\t-59",
  "\t-66\t-64\t-62",
  "Bristol Bay",
  "\t60\t-161\t-155",
  "\t59\t-163\t-155",
  "\t58\t-163\t-155",
  "\t57\t-163\t-155",
  "\t56\t-163\t-156",
  "\t55\t-162\t-157",
  "\t54\t-162\t-159",
  "Bristol Channel",
  "\t52\t-7\t-1",
  "\t51\t-7\t-1",
  "\t50\t-7\t-1",
  "\t49\t-6\t-3",
  "Caribbean Sea",
  "\t23\t-84\t-79",
  "\t22\t-88\t-77",
  "\t21\t-88\t-73",
  "\t20\t-88\t-71",
  "\t19\t-89\t-60",
  "\t18\t-89\t-60",
  "\t17\t-89\t-59",
  "\t16\t-89\t-59",
  "\t15\t-88\t-58",
  "\t14\t-87\t-58",
  "\t13\t-84\t-58",
  "\t12\t-84\t-58",
  "\t11\t-84\t-58",
  "\t10\t-84\t-59",
  "\t9\t-84\t-59",
  "\t8\t-84\t-74\t-63\t-59",
  "\t7\t-83\t-75",
  "Caspian Sea",
  "\t48\t49\t52",
  "\t47\t47\t54",
  "\t46\t46\t54",
  "\t45\t45\t54",
  "\t44\t45\t54",
  "\t43\t45\t53",
  "\t42\t46\t53",
  "\t41\t46\t53",
  "\t40\t47\t54",
  "\t39\t47\t54",
  "\t38\t47\t54",
  "\t37\t47\t55",
  "\t36\t47\t55",
  "\t35\t49\t55",
  "Celebes Sea",
  "\t8\t121\t125",
  "\t7\t120\t126",
  "\t6\t117\t126",
  "\t5\t116\t126",
  "\t4\t116\t126",
  "\t3\t116\t126",
  "\t2\t116\t126",
  "\t1\t116\t126",
  "\t0\t116\t126",
  "\t-1\t117\t124",
  "Ceram Sea",
  "\t0\t124\t133",
  "\t-1\t124\t134",
  "\t-2\t124\t134",
  "\t-3\t124\t134",
  "\t-4\t124\t126\t129\t134",
  "\t-5\t130\t134",
  "\t-6\t132\t134",
  "Chaun Bay",
  "\t70\t167\t171",
  "\t69\t167\t171",
  "\t68\t167\t171",
  "\t67\t168\t171",
  "Chesapeake Bay",
  "\t40\t-77\t-74",
  "\t39\t-78\t-74",
  "\t38\t-78\t-74",
  "\t37\t-78\t-74",
  "\t36\t-78\t-74",
  "\t35\t-77\t-75",
  "Chukchi Sea",
  "\t72\t-179\t-155",
  "\t71\t-180\t-155",
  "\t70\t-180\t-155",
  "\t69\t-180\t-156",
  "\t68\t-180\t-161",
  "\t67\t-180\t-162",
  "\t66\t-176\t-162",
  "\t65\t-175\t-163",
  "\t64\t-169\t-165",
  "Cook Inlet",
  "\t62\t-152\t-148",
  "\t61\t-154\t-148",
  "\t60\t-155\t-148",
  "\t59\t-155\t-148",
  "\t58\t-155\t-150",
  "\t57\t-154\t-151",
  "Cook Strait",
  "\t-39\t173\t176",
  "\t-40\t173\t176",
  "\t-41\t173\t176",
  "\t-42\t173\t176",
  "Coral Sea",
  "\t-7\t142\t147",
  "\t-8\t142\t148\t164\t168",
  "\t-9\t142\t153\t161\t168",
  "\t-10\t141\t168",
  "\t-11\t141\t168",
  "\t-12\t141\t168",
  "\t-13\t142\t169",
  "\t-14\t142\t169",
  "\t-15\t142\t169",
  "\t-16\t144\t169",
  "\t-17\t144\t170",
  "\t-18\t144\t170",
  "\t-19\t145\t170",
  "\t-20\t145\t170",
  "\t-21\t147\t170",
  "\t-22\t148\t169",
  "\t-23\t148\t168",
  "\t-24\t149\t167",
  "\t-25\t150\t166",
  "\t-26\t151\t165",
  "\t-27\t152\t164",
  "\t-28\t152\t162",
  "\t-29\t152\t161",
  "\t-30\t152\t160",
  "Cumberland Sound",
  "\t67\t-69\t-63",
  "\t66\t-69\t-62",
  "\t65\t-69\t-62",
  "\t64\t-69\t-62",
  "\t63\t-67\t-62",
  "\t62\t-65\t-63",
  "Dardanelles",
  "\t41\t25\t27",
  "\t40\t25\t27",
  "\t39\t25\t27",
  "\t38\t25\t27",
  "Davis Sea",
  "\t-62\t90\t104",
  "\t-63\t86\t111",
  "\t-64\t84\t113",
  "\t-65\t83\t113",
  "\t-66\t82\t113",
  "\t-67\t82\t111",
  "\t-68\t82\t87",
  "Davis Strait",
  "\t70\t-70\t-52",
  "\t69\t-70\t-50",
  "\t68\t-70\t-49",
  "\t67\t-70\t-49",
  "\t66\t-67\t-49",
  "\t65\t-64\t-49",
  "\t64\t-66\t-48",
  "\t63\t-66\t-47",
  "\t62\t-66\t-44",
  "\t61\t-66\t-43",
  "\t60\t-65\t-43",
  "\t59\t-65\t-43",
  "Delaware Bay",
  "\t40\t-76\t-73",
  "\t39\t-76\t-73",
  "\t38\t-76\t-73",
  "\t37\t-76\t-73",
  "Denmark Strait",
  "\t71\t-23\t-21",
  "\t70\t-26\t-19",
  "\t69\t-31\t-18",
  "\t68\t-31\t-16",
  "\t67\t-31\t-15",
  "\t66\t-30\t-15",
  "\t65\t-28\t-15",
  "\t64\t-27\t-16",
  "\t63\t-25\t-22",
  "Disko Bay",
  "\t71\t-55\t-49",
  "\t70\t-55\t-49",
  "\t69\t-55\t-49",
  "\t68\t-54\t-49",
  "\t67\t-54\t-49",
  "Dixon Entrance",
  "\t55\t-134\t-130",
  "\t54\t-134\t-130",
  "\t53\t-134\t-130",
  "Dmitriy Laptev Strait",
  "\t74\t138\t144",
  "\t73\t138\t144",
  "\t72\t138\t144",
  "\t71\t139\t144",
  "Drake Passage",
  "\t-53\t-67\t-62",
  "\t-54\t-69\t-61",
  "\t-55\t-69\t-60",
  "\t-56\t-69\t-58",
  "\t-57\t-69\t-57",
  "\t-58\t-69\t-56",
  "\t-59\t-69\t-55",
  "\t-60\t-69\t-54",
  "\t-61\t-69\t-54",
  "\t-62\t-69\t-54",
  "\t-63\t-69\t-57",
  "\t-64\t-69\t-61",
  "\t-65\t-69\t-62",
  "\t-66\t-69\t-64",
  "\t-67\t-69\t-65",
  "East China Sea",
  "\t34\t124\t127\t129\t131",
  "\t33\t119\t131",
  "\t32\t119\t131",
  "\t31\t119\t131",
  "\t30\t119\t131",
  "\t29\t119\t131",
  "\t28\t119\t131",
  "\t27\t118\t130",
  "\t26\t118\t129",
  "\t25\t118\t129",
  "\t24\t118\t128",
  "\t23\t120\t127",
  "\t22\t122\t125",
  "East Korea Bay",
  "\t41\t127\t129",
  "\t40\t126\t129",
  "\t39\t126\t129",
  "\t38\t126\t129",
  "\t37\t126\t129",
  "East Siberian Sea",
  "\t78\t147\t158",
  "\t77\t137\t162",
  "\t76\t137\t166",
  "\t75\t137\t170",
  "\t74\t138\t174",
  "\t73\t138\t178",
  "\t72\t138\t180",
  "\t71\t142\t180",
  "\t70\t147\t180",
  "\t69\t150\t155\t157\t178",
  "\t68\t158\t176",
  "\t67\t159\t162",
  "English Channel",
  "\t52\t0\t2",
  "\t51\t-6\t2",
  "\t50\t-7\t2",
  "\t49\t-7\t2",
  "\t48\t-7\t2",
  "\t47\t-6\t0",
  "Estrecho de Magellanes",
  "\t-51\t-75\t-67",
  "\t-52\t-75\t-67",
  "\t-53\t-75\t-67",
  "\t-54\t-74\t-69",
  "\t-55\t-72\t-69",
  "Foxe Basin",
  "\t71\t-80\t-76",
  "\t70\t-83\t-73",
  "\t69\t-83\t-72",
  "\t68\t-85\t-71",
  "\t67\t-87\t-71",
  "\t66\t-87\t-71",
  "\t65\t-87\t-71",
  "\t64\t-86\t-72",
  "\t63\t-84\t-74",
  "\t62\t-81\t-78",
  "Franklin Bay",
  "\t70\t-126\t-124",
  "\t69\t-126\t-124",
  "\t68\t-126\t-124",
  "Frobisher Bay",
  "\t64\t-69\t-64",
  "\t63\t-69\t-64",
  "\t62\t-69\t-64",
  "\t61\t-68\t-64",
  "Garabogaz Bay",
  "\t43\t52\t54",
  "\t42\t51\t55",
  "\t41\t51\t55",
  "\t40\t51\t55",
  "\t39\t51\t55",
  "Geographe Bay",
  "\t-29\t114\t116",
  "\t-30\t114\t116",
  "\t-31\t114\t116",
  "\t-32\t114\t116",
  "\t-33\t114\t116",
  "\t-34\t114\t116",
  "Golfe du Lion",
  "\t44\t2\t6",
  "\t43\t2\t6",
  "\t42\t2\t6",
  "\t41\t2\t5",
  "\t40\t2\t4",
  "Golfo Corcovado",
  "\t-40\t-74\t-71",
  "\t-41\t-74\t-71",
  "\t-42\t-74\t-71",
  "\t-43\t-75\t-71",
  "\t-44\t-75\t-71",
  "\t-45\t-75\t-71",
  "\t-46\t-74\t-71",
  "Golfo de California",
  "\t32\t-115\t-112",
  "\t31\t-115\t-111",
  "\t30\t-115\t-111",
  "\t29\t-115\t-110",
  "\t28\t-115\t-108",
  "\t27\t-114\t-108",
  "\t26\t-113\t-107",
  "\t25\t-113\t-106",
  "\t24\t-112\t-105",
  "\t23\t-111\t-105",
  "\t22\t-110\t-105",
  "Golfo de Guayaquil",
  "\t-1\t-81\t-78",
  "\t-2\t-81\t-78",
  "\t-3\t-81\t-78",
  "\t-4\t-81\t-78",
  "Golfo de Panama",
  "\t10\t-80\t-78",
  "\t9\t-81\t-76",
  "\t8\t-81\t-76",
  "\t7\t-81\t-76",
  "\t6\t-81\t-77",
  "Golfo de Penas",
  "\t-45\t-76\t-73",
  "\t-46\t-76\t-73",
  "\t-47\t-76\t-73",
  "\t-48\t-76\t-73",
  "Golfo de Tehuantepec",
  "\t17\t-96\t-92",
  "\t16\t-97\t-92",
  "\t15\t-97\t-92",
  "\t14\t-97\t-92",
  "Golfo de Uraba",
  "\t9\t-78\t-75",
  "\t8\t-78\t-75",
  "\t7\t-78\t-75",
  "\t6\t-77\t-75",
  "Golfo San Jorge",
  "\t-43\t-67\t-65",
  "\t-44\t-68\t-64",
  "\t-45\t-68\t-64",
  "\t-46\t-68\t-64",
  "\t-47\t-68\t-64",
  "\t-48\t-67\t-64",
  "Golfo San Matias",
  "\t-39\t-66\t-63",
  "\t-40\t-66\t-62",
  "\t-41\t-66\t-62",
  "\t-42\t-66\t-62",
  "\t-43\t-65\t-62",
  "Great Australian Bight",
  "\t-30\t127\t133",
  "\t-31\t123\t135",
  "\t-32\t118\t136",
  "\t-33\t117\t136",
  "\t-34\t117\t140",
  "\t-35\t117\t140",
  "\t-36\t117\t141",
  "\t-37\t119\t144",
  "\t-38\t123\t144",
  "\t-39\t126\t145",
  "\t-40\t129\t146",
  "\t-41\t133\t146",
  "\t-42\t136\t147",
  "\t-43\t139\t147",
  "\t-44\t143\t147",
  "Great Barrier Reef",
  "\t-8\t141\t146",
  "\t-9\t141\t146",
  "\t-10\t141\t146",
  "\t-11\t141\t146",
  "\t-12\t141\t147",
  "\t-13\t142\t148",
  "\t-14\t142\t148",
  "\t-15\t142\t148",
  "\t-16\t144\t149",
  "\t-17\t144\t150",
  "\t-18\t144\t151",
  "\t-19\t145\t151",
  "\t-20\t145\t152",
  "\t-21\t147\t154",
  "\t-22\t148\t154",
  "\t-23\t148\t154",
  "\t-24\t149\t154",
  "\t-25\t150\t154",
  "\t-26\t151\t154",
  "Great Bear Lake",
  "\t68\t-121\t-118",
  "\t67\t-126\t-116",
  "\t66\t-126\t-116",
  "\t65\t-126\t-116",
  "\t64\t-125\t-116",
  "\t63\t-123\t-119",
  "Great Salt Lake",
  "\t42\t-114\t-110",
  "\t41\t-114\t-110",
  "\t40\t-114\t-110",
  "\t39\t-113\t-110",
  "Great Slave Lake",
  "\t63\t-117\t-108",
  "\t62\t-118\t-108",
  "\t61\t-118\t-108",
  "\t60\t-118\t-110",
  "\t59\t-117\t-113",
  "Greenland Sea",
  "\t84\t-32\t-17",
  "\t83\t-33\t-3",
  "\t82\t-33\t11",
  "\t81\t-33\t18",
  "\t80\t-30\t-27\t-25\t18",
  "\t79\t-24\t18",
  "\t78\t-22\t18",
  "\t77\t-23\t18",
  "\t76\t-23\t18",
  "\t75\t-23\t17",
  "\t74\t-28\t14",
  "\t73\t-28\t10",
  "\t72\t-28\t5",
  "\t71\t-27\t0",
  "\t70\t-26\t-4",
  "\t69\t-29\t-7",
  "\t68\t-29\t-9",
  "\t67\t-29\t-10",
  "\t66\t-27\t-10",
  "\t65\t-25\t-11",
  "\t64\t-24\t-12",
  "Gulf of Aden",
  "\t16\t49\t52",
  "\t15\t46\t52",
  "\t14\t44\t52",
  "\t13\t42\t52",
  "\t12\t41\t52",
  "\t11\t41\t52",
  "\t10\t41\t52",
  "\t9\t42\t47",
  "Gulf of Alaska",
  "\t61\t-150\t-138",
  "\t60\t-152\t-137",
  "\t59\t-156\t-135",
  "\t58\t-157\t-135",
  "\t57\t-159\t-135",
  "\t56\t-164\t-139",
  "\t55\t-164\t-145",
  "\t54\t-164\t-152",
  "\t53\t-164\t-158",
  "Gulf of Anadyr",
  "\t67\t-180\t-177",
  "\t66\t-180\t-174",
  "\t65\t-180\t-172",
  "\t64\t-180\t-172",
  "\t63\t-176\t-172",
  "Gulf of Aqaba",
  "\t30\t33\t35",
  "\t29\t33\t35",
  "\t28\t33\t35",
  "\t27\t33\t35",
  "\t26\t33\t35",
  "Gulf of Boothia",
  "\t72\t-90\t-88",
  "\t71\t-93\t-84",
  "\t70\t-93\t-83",
  "\t69\t-93\t-83",
  "\t68\t-93\t-83",
  "\t67\t-91\t-83",
  "\t66\t-89\t-85",
  "Gulf of Bothnia",
  "\t66\t20\t26",
  "\t65\t20\t26",
  "\t64\t17\t26",
  "\t63\t16\t26",
  "\t62\t16\t24",
  "\t61\t16\t24",
  "\t60\t16\t24",
  "\t59\t16\t24",
  "\t58\t17\t24",
  "Gulf of Carpentaria",
  "\t-11\t135\t142",
  "\t-12\t134\t142",
  "\t-13\t134\t142",
  "\t-14\t134\t142",
  "\t-15\t134\t142",
  "\t-16\t134\t142",
  "\t-17\t136\t142",
  "\t-18\t138\t141",
  "Gulf of Finland",
  "\t61\t23\t31",
  "\t60\t21\t31",
  "\t59\t21\t31",
  "\t58\t21\t31",
  "Gulf of Gabes",
  "\t36\t9\t12",
  "\t35\t9\t12",
  "\t34\t9\t12",
  "\t33\t9\t12",
  "\t32\t9\t11",
  "Gulf of Guinea",
  "\t6\t-6\t3",
  "\t5\t-8\t8",
  "\t4\t-8\t9",
  "\t3\t-8\t10",
  "\t2\t-6\t10",
  "\t1\t-3\t11",
  "\t0\t1\t11",
  "\t-1\t4\t11",
  "Gulf of Honduras",
  "\t18\t-89\t-87",
  "\t17\t-89\t-86",
  "\t16\t-89\t-85",
  "\t15\t-89\t-85",
  "\t14\t-89\t-85",
  "Gulf of Kamchatka",
  "\t57\t161\t164",
  "\t56\t160\t164",
  "\t55\t160\t164",
  "\t54\t160\t164",
  "\t53\t160\t163",
  "Gulf of Khambhat",
  "\t23\t71\t73",
  "\t22\t71\t74",
  "\t21\t69\t74",
  "\t20\t69\t74",
  "\t19\t69\t73",
  "\t18\t71\t73",
  "Gulf of Kutch",
  "\t24\t67\t71",
  "\t23\t67\t71",
  "\t22\t67\t71",
  "\t21\t67\t71",
  "Gulf of Maine",
  "\t45\t-70\t-65",
  "\t44\t-71\t-64",
  "\t43\t-71\t-64",
  "\t42\t-71\t-64",
  "\t41\t-71\t-65",
  "\t40\t-70\t-67",
  "Gulf of Mannar",
  "\t10\t77\t80",
  "\t9\t76\t80",
  "\t8\t76\t80",
  "\t7\t76\t80",
  "\t6\t78\t80",
  "Gulf of Martaban",
  "\t18\t95\t98",
  "\t17\t94\t98",
  "\t16\t94\t98",
  "\t15\t94\t98",
  "\t14\t94\t98",
  "\t13\t96\t98",
  "Gulf of Masira",
  "\t21\t56\t59",
  "\t20\t56\t59",
  "\t19\t56\t59",
  "\t18\t56\t58",
  "Gulf of Mexico",
  "\t31\t-90\t-83",
  "\t30\t-96\t-81",
  "\t29\t-98\t-81",
  "\t28\t-98\t-81",
  "\t27\t-98\t-80",
  "\t26\t-98\t-79",
  "\t25\t-98\t-78",
  "\t24\t-98\t-78",
  "\t23\t-98\t-78",
  "\t22\t-98\t-82",
  "\t21\t-98\t-82",
  "\t20\t-98\t-83",
  "\t19\t-98\t-93",
  "Gulf of Ob",
  "\t73\t71\t76",
  "\t72\t70\t76",
  "\t71\t70\t76",
  "\t70\t70\t77",
  "\t69\t71\t78",
  "\t68\t70\t79",
  "\t67\t68\t79",
  "\t66\t68\t79",
  "\t65\t68\t74",
  "Gulf of Olenek",
  "\t74\t117\t124",
  "\t73\t117\t124",
  "\t72\t117\t124",
  "\t71\t118\t124",
  "Gulf of Oman",
  "\t27\t55\t58",
  "\t26\t55\t62",
  "\t25\t55\t62",
  "\t24\t55\t62",
  "\t23\t55\t61",
  "\t22\t56\t61",
  "\t21\t58\t60",
  "Gulf of Papua",
  "\t-6\t142\t146",
  "\t-7\t141\t147",
  "\t-8\t141\t147",
  "\t-9\t141\t147",
  "Gulf of Riga",
  "\t60\t22\t24",
  "\t59\t21\t25",
  "\t58\t20\t25",
  "\t57\t20\t25",
  "\t56\t20\t25",
  "\t55\t22\t24",
  "Gulf of Sakhalin",
  "\t55\t138\t143",
  "\t54\t138\t143",
  "\t53\t138\t143",
  "\t52\t139\t143",
  "Gulf of Sidra",
  "\t33\t14\t20",
  "\t32\t14\t21",
  "\t31\t14\t21",
  "\t30\t14\t21",
  "\t29\t16\t21",
  "Gulf of St. Lawrence",
  "\t52\t-59\t-55",
  "\t51\t-65\t-55",
  "\t50\t-65\t-55",
  "\t49\t-67\t-56",
  "\t48\t-67\t-53",
  "\t47\t-67\t-53",
  "\t46\t-67\t-53",
  "\t45\t-65\t-54",
  "\t44\t-64\t-60",
  "Gulf of Suez",
  "\t30\t31\t34",
  "\t29\t31\t34",
  "\t28\t31\t35",
  "\t27\t31\t35",
  "\t26\t32\t35",
  "Gulf of Thailand",
  "\t14\t98\t101",
  "\t13\t98\t103",
  "\t12\t98\t104",
  "\t11\t98\t106",
  "\t10\t98\t106",
  "\t9\t98\t106",
  "\t8\t98\t106",
  "\t7\t98\t105",
  "\t6\t99\t104",
  "\t5\t99\t103",
  "Gulf of Tonkin",
  "\t22\t105\t110",
  "\t21\t105\t111",
  "\t20\t104\t111",
  "\t19\t104\t111",
  "\t18\t104\t111",
  "\t17\t104\t109",
  "\t16\t105\t108",
  "Gulf of Yana",
  "\t76\t135\t138",
  "\t75\t135\t141",
  "\t74\t135\t141",
  "\t73\t133\t142",
  "\t72\t131\t142",
  "\t71\t131\t142",
  "\t70\t131\t140",
  "Gulf St. Vincent",
  "\t-31\t136\t138",
  "\t-32\t135\t138",
  "\t-33\t134\t139",
  "\t-34\t134\t139",
  "\t-35\t134\t139",
  "\t-36\t135\t139",
  "Hamilton Inlet",
  "\t55\t-59\t-56",
  "\t54\t-61\t-56",
  "\t53\t-61\t-56",
  "\t52\t-61\t-57",
  "Hangzhou Bay",
  "\t31\t119\t123",
  "\t30\t119\t123",
  "\t29\t119\t123",
  "\t28\t120\t123",
  "Helodranon' Antongila",
  "\t-14\t48\t51",
  "\t-15\t48\t51",
  "\t-16\t48\t51",
  "\t-17\t48\t50",
  "Hudson Bay",
  "\t67\t-87\t-84",
  "\t66\t-88\t-84",
  "\t65\t-94\t-81",
  "\t64\t-94\t-78",
  "\t63\t-94\t-77",
  "\t62\t-95\t-76",
  "\t61\t-95\t-76",
  "\t60\t-95\t-76",
  "\t59\t-95\t-76",
  "\t58\t-95\t-75",
  "\t57\t-95\t-75",
  "\t56\t-93\t-75",
  "\t55\t-93\t-75",
  "\t54\t-88\t-75",
  "\t53\t-83\t-77",
  "Hudson Strait",
  "\t65\t-79\t-71",
  "\t64\t-81\t-69",
  "\t63\t-81\t-64",
  "\t62\t-81\t-63",
  "\t61\t-79\t-63",
  "\t60\t-73\t-63",
  "\t59\t-71\t-63",
  "IJsselmeer",
  "\t54\t3\t7",
  "\t53\t3\t7",
  "\t52\t3\t7",
  "\t51\t3\t6",
  "Indian Ocean",
  "\t11\t49\t53",
  "\t10\t49\t55",
  "\t9\t49\t57",
  "\t8\t48\t59\t80\t85",
  "\t7\t48\t61\t79\t92",
  "\t6\t47\t64\t78\t96",
  "\t5\t46\t66\t77\t97",
  "\t4\t45\t68\t75\t98",
  "\t3\t44\t70\t74\t99",
  "\t2\t43\t99",
  "\t1\t40\t101",
  "\t0\t39\t101",
  "\t-1\t39\t102",
  "\t-2\t38\t103",
  "\t-3\t38\t104",
  "\t-4\t37\t105",
  "\t-5\t37\t107",
  "\t-6\t37\t111",
  "\t-7\t37\t119",
  "\t-8\t38\t120",
  "\t-9\t38\t123",
  "\t-10\t38\t125",
  "\t-11\t38\t126",
  "\t-12\t43\t127",
  "\t-13\t48\t127",
  "\t-14\t48\t127",
  "\t-15\t48\t127",
  "\t-16\t48\t126",
  "\t-17\t48\t125",
  "\t-18\t47\t124",
  "\t-19\t47\t123",
  "\t-20\t47\t122",
  "\t-21\t46\t120",
  "\t-22\t46\t117",
  "\t-23\t46\t115",
  "\t-24\t38\t114",
  "\t-25\t31\t114",
  "\t-26\t31\t115",
  "\t-27\t30\t115",
  "\t-28\t30\t115",
  "\t-29\t29\t116",
  "\t-30\t28\t116",
  "\t-31\t27\t116",
  "\t-32\t22\t116",
  "\t-33\t18\t117",
  "\t-34\t18\t120",
  "\t-35\t18\t124",
  "\t-36\t18\t127",
  "\t-37\t18\t130",
  "\t-38\t18\t134",
  "\t-39\t18\t137",
  "\t-40\t18\t140",
  "\t-41\t18\t144",
  "\t-42\t18\t148",
  "\t-43\t18\t151",
  "\t-44\t18\t153",
  "\t-45\t18\t156",
  "\t-46\t18\t159",
  "\t-47\t18\t161",
  "\t-48\t18\t164",
  "\t-49\t18\t167",
  "\t-50\t18\t167",
  "\t-51\t18\t167",
  "\t-52\t18\t167",
  "\t-53\t18\t167",
  "\t-54\t18\t167",
  "\t-55\t18\t167",
  "\t-56\t18\t167",
  "\t-57\t18\t167",
  "\t-58\t18\t167",
  "\t-59\t18\t167",
  "\t-60\t18\t167",
  "\t-61\t18\t167",
  "Inner Sea",
  "\t35\t129\t136",
  "\t34\t129\t136",
  "\t33\t129\t136",
  "\t32\t129\t136",
  "\t31\t130\t133",
  "Inner Seas",
  "\t59\t-7\t-4",
  "\t58\t-8\t-4",
  "\t57\t-8\t-3",
  "\t56\t-9\t-3",
  "\t55\t-9\t-3",
  "\t54\t-9\t-3",
  "\t53\t-8\t-4",
  "Ionian Sea",
  "\t41\t15\t18",
  "\t40\t15\t22",
  "\t39\t14\t24",
  "\t38\t14\t24",
  "\t37\t14\t24",
  "\t36\t14\t23",
  "\t35\t14\t23",
  "Irish Sea",
  "\t55\t-7\t-1",
  "\t54\t-7\t-1",
  "\t53\t-7\t-1",
  "\t52\t-7\t-1",
  "\t51\t-7\t-2",
  "\t50\t-7\t-4",
  "James Bay",
  "\t55\t-83\t-77",
  "\t54\t-83\t-77",
  "\t53\t-83\t-77",
  "\t52\t-83\t-77",
  "\t51\t-83\t-77",
  "\t50\t-81\t-77",
  "\t49\t-80\t-78",
  "Java Sea",
  "\t-1\t105\t114",
  "\t-2\t104\t117",
  "\t-3\t104\t119",
  "\t-4\t103\t120",
  "\t-5\t103\t120",
  "\t-6\t103\t120",
  "\t-7\t104\t119",
  "\t-8\t111\t118",
  "Joseph Bonaparte Gulf",
  "\t-12\t126\t130",
  "\t-13\t126\t130",
  "\t-14\t126\t130",
  "\t-15\t126\t130",
  "\t-16\t127\t130",
  "Kane Basin",
  "\t81\t-73\t-63",
  "\t80\t-79\t-63",
  "\t79\t-79\t-63",
  "\t78\t-79\t-63",
  "\t77\t-79\t-67",
  "Kangertittivaq",
  "\t72\t-29\t-23",
  "\t71\t-30\t-20",
  "\t70\t-30\t-20",
  "\t69\t-30\t-20",
  "\t68\t-28\t-26",
  "Kara Sea",
  "\t82\t64\t96",
  "\t81\t64\t98",
  "\t80\t64\t103",
  "\t79\t64\t103",
  "\t78\t65\t103",
  "\t77\t65\t102",
  "\t76\t59\t102",
  "\t75\t56\t102",
  "\t74\t55\t100",
  "\t73\t54\t88",
  "\t72\t54\t88",
  "\t71\t54\t80",
  "\t70\t54\t69\t74\t80",
  "\t69\t55\t70\t77\t80",
  "\t68\t59\t70",
  "\t67\t65\t70",
  "Karaginskiy Gulf",
  "\t61\t162\t167",
  "\t60\t161\t167",
  "\t59\t160\t167",
  "\t58\t160\t167",
  "\t57\t160\t166",
  "\t56\t161\t164",
  "Karskiye Vorota Strait",
  "\t71\t56\t60",
  "\t70\t56\t60",
  "\t69\t56\t60",
  "Kattegat",
  "\t59\t10\t12",
  "\t58\t9\t13",
  "\t57\t9\t13",
  "\t56\t8\t13",
  "\t55\t8\t13",
  "\t54\t8\t13",
  "\t53\t8\t12",
  "Khatanga Gulf",
  "\t76\t111\t114",
  "\t75\t108\t114",
  "\t74\t105\t114",
  "\t73\t104\t114",
  "\t72\t104\t113",
  "\t71\t104\t107",
  "Korea Strait",
  "\t37\t128\t131",
  "\t36\t126\t133",
  "\t35\t125\t133",
  "\t34\t125\t133",
  "\t33\t125\t133",
  "\t32\t125\t131",
  "\t31\t126\t130",
  "Kotzebue Sound",
  "\t68\t-164\t-160",
  "\t67\t-165\t-159",
  "\t66\t-165\t-159",
  "\t65\t-165\t-159",
  "Kronotskiy Gulf",
  "\t55\t158\t162",
  "\t54\t158\t162",
  "\t53\t158\t162",
  "\t52\t158\t161",
  "La Perouse Strait",
  "\t47\t140\t142",
  "\t46\t140\t143",
  "\t45\t140\t143",
  "\t44\t140\t143",
  "Labrador Sea",
  "\t61\t-65\t-43",
  "\t60\t-65\t-42",
  "\t59\t-65\t-42",
  "\t58\t-65\t-42",
  "\t57\t-64\t-43",
  "\t56\t-63\t-43",
  "\t55\t-63\t-44",
  "\t54\t-62\t-45",
  "\t53\t-60\t-46",
  "\t52\t-58\t-46",
  "\t51\t-57\t-47",
  "\t50\t-57\t-48",
  "\t49\t-57\t-49",
  "\t48\t-57\t-49",
  "\t47\t-55\t-50",
  "\t46\t-54\t-51",
  "Laccadive Sea",
  "\t15\t73\t75",
  "\t14\t70\t75",
  "\t13\t70\t76",
  "\t12\t70\t76",
  "\t11\t70\t77",
  "\t10\t70\t77",
  "\t9\t70\t79",
  "\t8\t70\t80",
  "\t7\t70\t81",
  "\t6\t70\t81",
  "\t5\t71\t81",
  "\t4\t71\t80",
  "\t3\t71\t79",
  "\t2\t71\t78",
  "\t1\t71\t76",
  "\t0\t71\t75",
  "\t-1\t71\t74",
  "Lago de Maracaibo",
  "\t11\t-72\t-70",
  "\t10\t-73\t-70",
  "\t9\t-73\t-70",
  "\t8\t-73\t-70",
  "Lake Baikal",
  "\t56\t107\t110",
  "\t55\t107\t110",
  "\t54\t105\t110",
  "\t53\t104\t110",
  "\t52\t102\t110",
  "\t51\t102\t109",
  "\t50\t102\t107",
  "Lake Chad",
  "\t14\t13\t15",
  "\t13\t13\t15",
  "\t12\t13\t15",
  "\t11\t13\t15",
  "Lake Champlain",
  "\t46\t-74\t-72",
  "\t45\t-74\t-72",
  "\t44\t-74\t-72",
  "\t43\t-74\t-72",
  "\t42\t-74\t-72",
  "Lake Erie",
  "\t44\t-80\t-77",
  "\t43\t-84\t-77",
  "\t42\t-84\t-77",
  "\t41\t-84\t-77",
  "\t40\t-84\t-79",
  "Lake Huron",
  "\t47\t-82\t-80",
  "\t46\t-82\t-78",
  "\t45\t-82\t-78",
  "\t44\t-82\t-78",
  "\t43\t-82\t-78",
  "Lake Huron",
  "\t47\t-85\t-80",
  "\t46\t-85\t-78",
  "\t45\t-85\t-78",
  "\t44\t-85\t-78",
  "\t43\t-84\t-78",
  "\t42\t-84\t-80",
  "Lake Huron",
  "\t47\t-84\t-80",
  "\t46\t-84\t-80",
  "\t45\t-84\t-80",
  "\t44\t-84\t-80",
  "Lake Huron",
  "\t45\t-84\t-82",
  "\t44\t-84\t-82",
  "\t43\t-84\t-82",
  "\t42\t-84\t-82",
  "Lake Malawi",
  "\t-8\t32\t35",
  "\t-9\t32\t35",
  "\t-10\t32\t35",
  "\t-11\t32\t35",
  "\t-12\t33\t36",
  "\t-13\t33\t36",
  "\t-14\t33\t36",
  "\t-15\t33\t36",
  "Lake Michigan",
  "\t47\t-86\t-84",
  "\t46\t-88\t-83",
  "\t45\t-89\t-83",
  "\t44\t-89\t-83",
  "\t43\t-89\t-84",
  "\t42\t-88\t-85",
  "\t41\t-88\t-85",
  "\t40\t-88\t-85",
  "Lake Okeechobee",
  "\t28\t-82\t-79",
  "\t27\t-82\t-79",
  "\t26\t-82\t-79",
  "\t25\t-82\t-79",
  "Lake Ontario",
  "\t45\t-78\t-74",
  "\t44\t-80\t-74",
  "\t43\t-80\t-74",
  "\t42\t-80\t-75",
  "Lake Saint Clair",
  "\t43\t-84\t-81",
  "\t42\t-84\t-81",
  "\t41\t-84\t-81",
  "Lake Superior",
  "\t50\t-89\t-87",
  "\t49\t-90\t-84",
  "\t48\t-92\t-83",
  "\t47\t-93\t-83",
  "\t46\t-93\t-83",
  "\t45\t-93\t-83",
  "Lake Superior",
  "\t48\t-85\t-83",
  "\t47\t-86\t-83",
  "\t46\t-86\t-83",
  "\t45\t-86\t-83",
  "Lake Tahoe",
  "\t40\t-121\t-118",
  "\t39\t-121\t-118",
  "\t38\t-121\t-118",
  "\t37\t-121\t-118",
  "Lake Tanganyika",
  "\t-2\t28\t30",
  "\t-3\t28\t30",
  "\t-4\t28\t30",
  "\t-5\t28\t31",
  "\t-6\t28\t31",
  "\t-7\t28\t32",
  "\t-8\t28\t32",
  "\t-9\t29\t32",
  "Lake Victoria",
  "\t1\t30\t35",
  "\t0\t30\t35",
  "\t-1\t30\t35",
  "\t-2\t30\t35",
  "\t-3\t30\t34",
  "\t-4\t31\t33",
  "Lake Winnipeg",
  "\t55\t-99\t-96",
  "\t54\t-100\t-96",
  "\t53\t-100\t-95",
  "\t52\t-100\t-95",
  "\t51\t-99\t-95",
  "\t50\t-99\t-95",
  "\t49\t-97\t-95",
  "Laptev Sea",
  "\t82\t95\t101",
  "\t81\t95\t109",
  "\t80\t95\t118",
  "\t79\t95\t126",
  "\t78\t96\t134",
  "\t77\t101\t139",
  "\t76\t103\t139",
  "\t75\t104\t139",
  "\t74\t111\t138",
  "\t73\t111\t137",
  "\t72\t111\t137",
  "\t71\t112\t114\t126\t136",
  "\t70\t127\t134",
  "\t69\t129\t132",
  "Ligurian Sea",
  "\t45\t7\t10",
  "\t44\t6\t10",
  "\t43\t6\t10",
  "\t42\t6\t10",
  "Lincoln Sea",
  "\t84\t-70\t-39",
  "\t83\t-70\t-39",
  "\t82\t-70\t-39",
  "\t81\t-69\t-40",
  "\t80\t-54\t-48\t-46\t-43",
  "Long Island Sound",
  "\t42\t-74\t-71",
  "\t41\t-74\t-71",
  "\t40\t-74\t-71",
  "\t39\t-74\t-71",
  "Lutzow-Holm Bay",
  "\t-67\t32\t41",
  "\t-68\t32\t41",
  "\t-69\t32\t41",
  "\t-70\t32\t40",
  "\t-71\t37\t39",
  "Luzon Strait",
  "\t23\t119\t121",
  "\t22\t119\t122",
  "\t21\t119\t123",
  "\t20\t119\t123",
  "\t19\t119\t123",
  "\t18\t119\t123",
  "\t17\t119\t123",
  "M'Clure Strait",
  "\t77\t-123\t-119",
  "\t76\t-124\t-114",
  "\t75\t-125\t-113",
  "\t74\t-125\t-113",
  "\t73\t-125\t-113",
  "\t72\t-116\t-114",
  "Mackenzie Bay",
  "\t70\t-140\t-133",
  "\t69\t-140\t-133",
  "\t68\t-140\t-133",
  "\t67\t-138\t-134",
  "Makassar Strait",
  "\t2\t116\t122",
  "\t1\t116\t122",
  "\t0\t115\t122",
  "\t-1\t115\t121",
  "\t-2\t115\t120",
  "\t-3\t115\t120",
  "\t-4\t115\t120",
  "\t-5\t116\t120",
  "\t-6\t118\t120",
  "Marguerite Bay",
  "\t-66\t-70\t-65",
  "\t-67\t-71\t-65",
  "\t-68\t-71\t-65",
  "\t-69\t-71\t-65",
  "\t-70\t-71\t-65",
  "Massachusetts Bay",
  "\t43\t-72\t-69",
  "\t42\t-72\t-69",
  "\t41\t-72\t-69",
  "\t40\t-71\t-69",
  "McMurdo Sound",
  "\t-71\t165\t167",
  "\t-72\t163\t170",
  "\t-73\t161\t170",
  "\t-74\t159\t170",
  "\t-75\t159\t170",
  "\t-76\t159\t170",
  "\t-77\t161\t170",
  "\t-78\t161\t170",
  "\t-79\t162\t166",
  "Mediterranean Sea",
  "\t38\t10\t15",
  "\t37\t9\t24\t26\t37",
  "\t36\t9\t37",
  "\t35\t9\t37",
  "\t34\t9\t36",
  "\t33\t9\t36",
  "\t32\t9\t36",
  "\t31\t11\t36",
  "\t30\t23\t35",
  "\t29\t27\t30",
  "Mediterranean Sea",
  "\t44\t4\t10",
  "\t43\t3\t10",
  "\t42\t2\t10",
  "\t41\t2\t10",
  "\t40\t2\t10",
  "\t39\t-1\t11",
  "\t38\t-2\t13",
  "\t37\t-3\t13",
  "\t36\t-3\t13",
  "\t35\t-3\t11",
  "\t34\t-2\t1",
  "Melville Bay",
  "\t77\t-68\t-59",
  "\t76\t-68\t-56",
  "\t75\t-68\t-55",
  "\t74\t-67\t-55",
  "\t73\t-62\t-55",
  "Molucca Sea",
  "\t5\t125\t127",
  "\t4\t124\t128",
  "\t3\t124\t129",
  "\t2\t123\t129",
  "\t1\t122\t129",
  "\t0\t122\t129",
  "\t-1\t122\t129",
  "\t-2\t122\t128",
  "Mozambique Channel",
  "\t-9\t39\t44",
  "\t-10\t39\t49",
  "\t-11\t39\t50",
  "\t-12\t39\t50",
  "\t-13\t39\t50",
  "\t-14\t39\t49",
  "\t-15\t38\t48",
  "\t-16\t35\t48",
  "\t-17\t34\t46",
  "\t-18\t33\t45",
  "\t-19\t33\t45",
  "\t-20\t33\t45",
  "\t-21\t33\t45",
  "\t-22\t34\t44",
  "\t-23\t33\t45",
  "\t-24\t31\t46",
  "\t-25\t31\t46",
  "\t-26\t31\t46",
  "\t-27\t31\t39",
  "North Sea",
  "\t61\t-2\t7",
  "\t60\t-3\t7",
  "\t59\t-4\t8",
  "\t58\t-5\t10",
  "\t57\t-5\t10",
  "\t56\t-5\t10",
  "\t55\t-4\t10",
  "\t54\t-4\t10",
  "\t53\t-2\t10",
  "\t52\t-1\t10",
  "\t51\t-1\t5",
  "\t50\t-1\t5",
  "\t49\t0\t2",
  "Norton Sound",
  "\t65\t-165\t-159",
  "\t64\t-165\t-159",
  "\t63\t-165\t-159",
  "\t62\t-165\t-159",
  "Norwegian Sea",
  "\t77\t13\t19",
  "\t76\t9\t21",
  "\t75\t4\t23",
  "\t74\t-1\t25",
  "\t73\t-5\t27",
  "\t72\t-9\t28",
  "\t71\t-10\t28",
  "\t70\t-11\t28",
  "\t69\t-12\t28",
  "\t68\t-12\t24",
  "\t67\t-13\t18",
  "\t66\t-14\t15",
  "\t65\t-14\t15",
  "\t64\t-14\t14",
  "\t63\t-14\t12",
  "\t62\t-12\t10",
  "\t61\t-9\t9",
  "\t60\t-7\t7",
  "\t59\t-4\t6",
  "Pacific Ocean",
  "\t59\t-140\t-135",
  "\t58\t-146\t-134",
  "\t57\t-153\t-133",
  "\t56\t-159\t-133",
  "\t55\t-164\t-132",
  "\t54\t-167\t-131",
  "\t53\t-172\t-130",
  "\t52\t-180\t-129",
  "\t51\t-180\t-126",
  "\t50\t-180\t-123",
  "\t49\t-180\t-123",
  "\t48\t-180\t-123",
  "\t47\t-180\t-122",
  "\t46\t-180\t-122",
  "\t45\t-180\t-122",
  "\t44\t-180\t-122",
  "\t43\t-180\t-123",
  "\t42\t-180\t-123",
  "\t41\t-180\t-123",
  "\t40\t-180\t-122",
  "\t39\t-180\t-120",
  "\t38\t-180\t-120",
  "\t37\t-180\t-120",
  "\t36\t-180\t-119",
  "\t35\t-180\t-117",
  "\t34\t-180\t-116",
  "\t33\t-180\t-115",
  "\t32\t-180\t-115",
  "\t31\t-180\t-114",
  "\t30\t-180\t-113",
  "\t29\t-180\t-113",
  "\t28\t-180\t-113",
  "\t27\t-180\t-111",
  "\t26\t-180\t-111",
  "\t25\t-180\t-110",
  "\t24\t-180\t-105",
  "\t23\t-180\t-104",
  "\t22\t-180\t-104",
  "\t21\t-180\t-104",
  "\t20\t-180\t-103",
  "\t19\t-180\t-101",
  "\t18\t-180\t-99",
  "\t17\t-180\t-97",
  "\t16\t-180\t-91",
  "\t15\t-180\t-90",
  "\t14\t-180\t-86",
  "\t13\t-180\t-85",
  "\t12\t-180\t-84",
  "\t11\t-180\t-84",
  "\t10\t-180\t-82",
  "\t9\t-180\t-80",
  "\t8\t-180\t-76",
  "\t7\t-180\t-76",
  "\t6\t-180\t-76",
  "\t5\t-180\t-76",
  "\t4\t-180\t-76",
  "\t3\t-180\t-76",
  "\t2\t-180\t-76",
  "\t1\t-180\t-76",
  "\t0\t-180\t-77",
  "\t-1\t-180\t-78",
  "Pacific Ocean",
  "\t57\t161\t164",
  "\t56\t161\t166",
  "\t55\t160\t168",
  "\t54\t157\t170",
  "\t53\t157\t172",
  "\t52\t156\t179",
  "\t51\t155\t180",
  "\t50\t154\t180",
  "\t49\t153\t180",
  "\t48\t152\t180",
  "\t47\t150\t180",
  "\t46\t148\t180",
  "\t45\t147\t180",
  "\t44\t143\t180",
  "\t43\t142\t180",
  "\t42\t141\t180",
  "\t41\t140\t180",
  "\t40\t140\t180",
  "\t39\t139\t180",
  "\t38\t139\t180",
  "\t37\t139\t180",
  "\t36\t138\t180",
  "\t35\t138\t180",
  "\t34\t138\t180",
  "\t33\t138\t180",
  "\t32\t138\t180",
  "\t31\t139\t180",
  "\t30\t139\t180",
  "\t29\t139\t180",
  "\t28\t140\t180",
  "\t27\t141\t180",
  "\t26\t141\t180",
  "\t25\t141\t180",
  "\t24\t140\t180",
  "\t23\t140\t180",
  "\t22\t140\t180",
  "\t21\t140\t180",
  "\t20\t140\t180",
  "\t19\t143\t180",
  "\t18\t144\t180",
  "\t17\t145\t180",
  "\t16\t145\t180",
  "\t15\t144\t180",
  "\t14\t144\t180",
  "\t13\t143\t180",
  "\t12\t141\t180",
  "\t11\t140\t180",
  "\t10\t138\t180",
  "\t9\t136\t180",
  "\t8\t135\t180",
  "\t7\t133\t180",
  "\t6\t132\t180",
  "\t5\t130\t180",
  "\t4\t128\t180",
  "\t3\t127\t180",
  "\t2\t127\t180",
  "\t1\t127\t180",
  "\t0\t127\t180",
  "\t-1\t128\t180",
  "Pacific Ocean",
  "\t3\t-93\t-90",
  "\t2\t-93\t-89",
  "\t1\t-180\t-79",
  "\t0\t-180\t-79",
  "\t-1\t-180\t-79",
  "\t-2\t-180\t-79",
  "\t-3\t-180\t-79",
  "\t-4\t-180\t-79",
  "\t-5\t-180\t-78",
  "\t-6\t-180\t-78",
  "\t-7\t-180\t-77",
  "\t-8\t-180\t-77",
  "\t-9\t-180\t-76",
  "\t-10\t-180\t-76",
  "\t-11\t-180\t-75",
  "\t-12\t-180\t-75",
  "\t-13\t-180\t-74",
  "\t-14\t-180\t-73",
  "\t-15\t-180\t-71",
  "\t-16\t-180\t-69",
  "\t-17\t-180\t-69",
  "\t-18\t-180\t-69",
  "\t-19\t-180\t-69",
  "\t-20\t-180\t-69",
  "\t-21\t-179\t-173\t-171\t-69",
  "\t-22\t-176\t-173\t-168\t-69",
  "\t-23\t-166\t-69",
  "\t-24\t-163\t-69",
  "\t-25\t-161\t-69",
  "\t-26\t-158\t-69",
  "\t-27\t-156\t-69",
  "\t-28\t-153\t-69",
  "\t-29\t-150\t-70",
  "\t-30\t-148\t-70",
  "\t-31\t-145\t-70",
  "\t-32\t-143\t-70",
  "\t-33\t-140\t-70",
  "\t-34\t-138\t-70",
  "\t-35\t-135\t-70",
  "\t-36\t-133\t-71",
  "\t-37\t-130\t-71",
  "\t-38\t-128\t-72",
  "\t-39\t-125\t-72",
  "\t-40\t-123\t-72",
  "\t-41\t-120\t-72",
  "\t-42\t-177\t-175\t-118\t-72",
  "\t-43\t-177\t-175\t-115\t-72",
  "\t-44\t-177\t-175\t-113\t-72",
  "\t-45\t-177\t-175\t-110\t-72",
  "\t-46\t-108\t-72",
  "\t-47\t-105\t-72",
  "\t-48\t-103\t-72",
  "\t-49\t-100\t-72",
  "\t-50\t-98\t-71",
  "\t-51\t-95\t-71",
  "\t-52\t-92\t-71",
  "\t-53\t-90\t-68",
  "\t-54\t-87\t-67",
  "\t-55\t-85\t-67",
  "\t-56\t-82\t-67",
  "\t-57\t-80\t-67",
  "\t-58\t-77\t-67",
  "\t-59\t-75\t-67",
  "\t-60\t-72\t-67",
  "\t-61\t-70\t-67",
  "Pacific Ocean",
  "\t4\t171\t173",
  "\t3\t170\t174",
  "\t2\t170\t174",
  "\t1\t128\t180",
  "\t0\t128\t180",
  "\t-1\t128\t180",
  "\t-2\t130\t180",
  "\t-3\t133\t142\t145\t180",
  "\t-4\t133\t136\t150\t180",
  "\t-5\t152\t180",
  "\t-6\t153\t180",
  "\t-7\t154\t180",
  "\t-8\t156\t180",
  "\t-9\t158\t180",
  "\t-10\t160\t180",
  "\t-11\t160\t180",
  "\t-12\t166\t180",
  "\t-13\t166\t180",
  "\t-14\t166\t180",
  "\t-15\t166\t180",
  "\t-16\t167\t180",
  "\t-17\t167\t180",
  "\t-18\t167\t180",
  "\t-19\t168\t180",
  "\t-20\t167\t180",
  "\t-21\t166\t180",
  "\t-22\t165\t180",
  "\t-23\t164\t180",
  "\t-24\t163\t180",
  "\t-25\t161\t180",
  "\t-26\t160\t180",
  "\t-27\t159\t180",
  "\t-28\t158\t180",
  "\t-29\t158\t180",
  "\t-30\t158\t180",
  "\t-31\t158\t180",
  "\t-32\t158\t180",
  "\t-33\t161\t180",
  "\t-34\t165\t180",
  "\t-35\t169\t180",
  "\t-36\t172\t180",
  "\t-37\t173\t180",
  "\t-38\t174\t180",
  "\t-39\t175\t180",
  "\t-40\t173\t180",
  "\t-41\t172\t180",
  "\t-42\t170\t180",
  "\t-43\t169\t180",
  "\t-44\t169\t180",
  "\t-45\t166\t180",
  "\t-46\t166\t180",
  "\t-47\t165\t180",
  "\t-48\t165\t180",
  "\t-49\t165\t180",
  "\t-50\t165\t180",
  "\t-51\t165\t180",
  "\t-52\t165\t180",
  "\t-53\t165\t180",
  "\t-54\t165\t180",
  "\t-55\t165\t180",
  "\t-56\t165\t180",
  "\t-57\t165\t180",
  "\t-58\t165\t180",
  "\t-59\t165\t180",
  "\t-60\t165\t180",
  "\t-61\t165\t180",
  "Palk Strait",
  "\t11\t78\t80",
  "\t10\t77\t81",
  "\t9\t77\t81",
  "\t8\t77\t81",
  "\t7\t78\t80",
  "Pamlico Sound",
  "\t36\t-78\t-74",
  "\t35\t-78\t-74",
  "\t34\t-78\t-74",
  "\t33\t-77\t-75",
  "Persian Gulf",
  "\t31\t46\t51",
  "\t30\t46\t51",
  "\t29\t46\t52",
  "\t28\t46\t57",
  "\t27\t47\t58",
  "\t26\t47\t58",
  "\t25\t48\t58",
  "\t24\t49\t57",
  "\t23\t49\t55",
  "\t22\t50\t53",
  "Philippine Sea",
  "\t36\t135\t139",
  "\t35\t135\t140",
  "\t34\t132\t141",
  "\t33\t130\t141",
  "\t32\t129\t141",
  "\t31\t129\t141",
  "\t30\t129\t142",
  "\t29\t128\t143",
  "\t28\t127\t143",
  "\t27\t126\t143",
  "\t26\t126\t143",
  "\t25\t120\t143",
  "\t24\t120\t143",
  "\t23\t119\t143",
  "\t22\t119\t144",
  "\t21\t119\t146",
  "\t20\t119\t146",
  "\t19\t120\t147",
  "\t18\t121\t147",
  "\t17\t120\t147",
  "\t16\t120\t147",
  "\t15\t120\t147",
  "\t14\t120\t147",
  "\t13\t120\t146",
  "\t12\t121\t146",
  "\t11\t123\t145",
  "\t10\t124\t144",
  "\t9\t124\t142",
  "\t8\t125\t141",
  "\t7\t125\t139",
  "\t6\t124\t137",
  "\t5\t124\t136",
  "\t4\t124\t134",
  "\t3\t124\t133",
  "\t2\t124\t131",
  "\t1\t127\t129",
  "Porpoise Bay",
  "\t-65\t125\t131",
  "\t-66\t125\t131",
  "\t-67\t125\t131",
  "\t-68\t126\t130",
  "Prince William Sound",
  "\t62\t-149\t-145",
  "\t61\t-149\t-144",
  "\t60\t-149\t-144",
  "\t59\t-149\t-144",
  "\t58\t-148\t-146",
  "Prydz Bay",
  "\t-66\t68\t75",
  "\t-67\t68\t80",
  "\t-68\t66\t80",
  "\t-69\t66\t80",
  "\t-70\t66\t78",
  "\t-71\t65\t74",
  "\t-72\t65\t72",
  "\t-73\t65\t71",
  "\t-74\t65\t68",
  "Puget Sound",
  "\t49\t-123\t-121",
  "\t48\t-124\t-121",
  "\t47\t-124\t-121",
  "\t46\t-124\t-121",
  "Qiongzhou Strait",
  "\t21\t108\t111",
  "\t20\t108\t111",
  "\t19\t108\t111",
  "\t18\t108\t110",
  "Queen Charlotte Sound",
  "\t54\t-130\t-128",
  "\t53\t-132\t-127",
  "\t52\t-132\t-126",
  "\t51\t-132\t-126",
  "\t50\t-132\t-126",
  "\t49\t-130\t-126",
  "Red Sea",
  "\t29\t33\t36",
  "\t28\t32\t36",
  "\t27\t32\t37",
  "\t26\t32\t38",
  "\t25\t32\t39",
  "\t24\t33\t39",
  "\t23\t34\t40",
  "\t22\t34\t40",
  "\t21\t34\t41",
  "\t20\t35\t42",
  "\t19\t36\t42",
  "\t18\t36\t43",
  "\t17\t36\t43",
  "\t16\t37\t43",
  "\t15\t38\t44",
  "\t14\t38\t44",
  "\t13\t39\t44",
  "\t12\t40\t44",
  "\t11\t41\t44",
  "Rio de la Plata",
  "\t-31\t-59\t-57",
  "\t-32\t-59\t-57",
  "\t-33\t-59\t-53",
  "\t-34\t-59\t-53",
  "\t-35\t-59\t-53",
  "\t-36\t-58\t-54",
  "\t-37\t-58\t-55",
  "Ronne Entrance",
  "\t-70\t-76\t-74",
  "\t-71\t-76\t-72",
  "\t-72\t-76\t-72",
  "\t-73\t-76\t-72",
  "Ross Sea",
  "\t-70\t169\t180",
  "\t-71\t167\t180",
  "\t-72\t167\t180",
  "\t-73\t167\t180",
  "\t-74\t168\t180",
  "\t-75\t168\t180",
  "\t-76\t165\t180",
  "\t-77\t160\t180",
  "\t-78\t158\t180",
  "\t-79\t157\t180",
  "\t-80\t157\t180",
  "\t-81\t157\t180",
  "\t-82\t159\t180",
  "\t-83\t160\t180",
  "\t-84\t166\t180",
  "\t-85\t176\t180",
  "Ross Sea",
  "\t-70\t-110\t-101",
  "\t-71\t-116\t-101",
  "\t-72\t-131\t-101",
  "\t-73\t-137\t-101",
  "\t-74\t-147\t-107",
  "\t-75\t-153\t-113",
  "\t-76\t-159\t-126",
  "\t-77\t-165\t-131",
  "\t-78\t-165\t-137",
  "\t-79\t-165\t-142",
  "\t-80\t-165\t-147",
  "\t-81\t-175\t-147",
  "\t-82\t-176\t-149",
  "\t-83\t-179\t-152",
  "\t-84\t-179\t-155",
  "\t-85\t-179\t-155",
  "\t-86\t-159\t-155",
  "Salton Sea",
  "\t34\t-117\t-114",
  "\t33\t-117\t-114",
  "\t32\t-117\t-114",
  "Scotia Sea",
  "\t-50\t-59\t-53",
  "\t-51\t-60\t-46",
  "\t-52\t-60\t-40",
  "\t-53\t-60\t-35",
  "\t-54\t-60\t-35",
  "\t-55\t-59\t-35",
  "\t-56\t-59\t-36",
  "\t-57\t-58\t-37",
  "\t-58\t-58\t-39",
  "\t-59\t-57\t-40",
  "\t-60\t-57\t-41",
  "\t-61\t-56\t-43",
  "\t-62\t-56\t-49",
  "Sea of Azov",
  "\t48\t36\t40",
  "\t47\t33\t40",
  "\t46\t33\t40",
  "\t45\t33\t39",
  "\t44\t33\t39",
  "Sea of Crete",
  "\t39\t22\t24",
  "\t38\t21\t25",
  "\t37\t21\t29",
  "\t36\t21\t29",
  "\t35\t22\t29",
  "\t34\t22\t28",
  "Sea of Japan",
  "\t52\t139\t143",
  "\t51\t139\t143",
  "\t50\t139\t143",
  "\t49\t138\t143",
  "\t48\t137\t143",
  "\t47\t137\t143",
  "\t46\t135\t143",
  "\t45\t134\t143",
  "\t44\t130\t143",
  "\t43\t129\t142",
  "\t42\t128\t142",
  "\t41\t127\t141",
  "\t40\t127\t141",
  "\t39\t127\t141",
  "\t38\t127\t141",
  "\t37\t127\t140",
  "\t36\t126\t139",
  "\t35\t125\t138",
  "\t34\t125\t137",
  "\t33\t125\t133",
  "\t32\t125\t131",
  "\t31\t126\t130",
  "Sea of Marmara",
  "\t42\t26\t30",
  "\t41\t25\t30",
  "\t40\t25\t30",
  "\t39\t25\t30",
  "Sea of Okhotsk",
  "\t60\t141\t156",
  "\t59\t139\t156",
  "\t58\t137\t157",
  "\t57\t137\t157",
  "\t56\t137\t157",
  "\t55\t136\t157",
  "\t54\t136\t157",
  "\t53\t136\t157",
  "\t52\t136\t139\t141\t158",
  "\t51\t142\t158",
  "\t50\t142\t158",
  "\t49\t141\t157",
  "\t48\t141\t156",
  "\t47\t141\t155",
  "\t46\t140\t154",
  "\t45\t140\t153",
  "\t44\t140\t151",
  "\t43\t141\t149",
  "\t42\t143\t148",
  "Shark Bay",
  "\t-23\t112\t114",
  "\t-24\t112\t115",
  "\t-25\t112\t115",
  "\t-26\t112\t115",
  "\t-27\t112\t115",
  "Shelikhova Gulf",
  "\t63\t162\t166",
  "\t62\t155\t166",
  "\t61\t153\t166",
  "\t60\t153\t165",
  "\t59\t153\t164",
  "\t58\t153\t162",
  "\t57\t154\t160",
  "\t56\t155\t158",
  "Skagerrak",
  "\t60\t8\t12",
  "\t59\t6\t12",
  "\t58\t6\t12",
  "\t57\t6\t12",
  "\t56\t6\t11",
  "\t55\t7\t9",
  "Sognefjorden",
  "\t62\t3\t8",
  "\t61\t3\t8",
  "\t60\t3\t8",
  "\t59\t4\t8",
  "Solomon Sea",
  "\t-3\t151\t155",
  "\t-4\t146\t155",
  "\t-5\t145\t157",
  "\t-6\t145\t160",
  "\t-7\t145\t161",
  "\t-8\t146\t162",
  "\t-9\t147\t163",
  "\t-10\t147\t163",
  "\t-11\t148\t163",
  "\t-12\t152\t162",
  "South China Sea",
  "\t24\t112\t121",
  "\t23\t112\t121",
  "\t22\t109\t122",
  "\t21\t108\t123",
  "\t20\t108\t123",
  "\t19\t107\t123",
  "\t18\t105\t123",
  "\t17\t105\t123",
  "\t16\t105\t121",
  "\t15\t106\t121",
  "\t14\t107\t121",
  "\t13\t108\t121",
  "\t12\t107\t121",
  "\t11\t104\t121",
  "\t10\t104\t120",
  "\t9\t103\t120",
  "\t8\t102\t119",
  "\t7\t101\t118",
  "\t6\t101\t117",
  "\t5\t101\t117",
  "\t4\t101\t117",
  "\t3\t102\t116",
  "\t2\t102\t114",
  "\t1\t101\t113",
  "\t0\t101\t112",
  "\t-1\t101\t111",
  "\t-2\t103\t111",
  "\t-3\t103\t111",
  "\t-4\t105\t107",
  "Southern Ocean",
  "\t-59\t-70\t180",
  "\t-60\t-74\t180",
  "\t-61\t-77\t180",
  "\t-62\t-81\t180",
  "\t-63\t-84\t180",
  "\t-64\t-88\t180",
  "\t-65\t-92\t91\t103\t180",
  "\t-66\t-95\t87\t110\t180",
  "\t-67\t-99\t52\t54\t85\t112\t180",
  "\t-68\t-102\t51\t54\t84\t112\t122\t141\t180",
  "\t-69\t-106\t-69\t-66\t44\t74\t80\t145\t180",
  "\t-70\t-108\t-73\t-64\t33\t154\t180",
  "\t-71\t-108\t-79\t-63\t33\t159\t180",
  "\t-72\t-108\t-85\t-62\t2\t24\t27\t161\t163\t166\t180",
  "\t-73\t-100\t-91",
  "St. Helena Bay",
  "\t-30\t16\t19",
  "\t-31\t16\t19",
  "\t-32\t16\t19",
  "\t-33\t16\t19",
  "St. Lawrence River",
  "\t51\t-67\t-63",
  "\t50\t-69\t-63",
  "\t49\t-72\t-63",
  "\t48\t-72\t-63",
  "\t47\t-74\t-66",
  "\t46\t-75\t-68",
  "\t45\t-75\t-69",
  "\t44\t-75\t-72",
  "Storfjorden",
  "\t79\t17\t22",
  "\t78\t16\t22",
  "\t77\t15\t22",
  "\t76\t15\t22",
  "\t75\t15\t19",
  "Strait of Belle Isle",
  "\t53\t-56\t-54",
  "\t52\t-58\t-54",
  "\t51\t-58\t-54",
  "\t50\t-58\t-54",
  "Strait of Georgia",
  "\t51\t-126\t-122",
  "\t50\t-126\t-121",
  "\t49\t-126\t-121",
  "\t48\t-126\t-121",
  "\t47\t-124\t-121",
  "Strait of Gibraltar",
  "\t37\t-7\t-4",
  "\t36\t-7\t-4",
  "\t35\t-7\t-4",
  "\t34\t-6\t-4",
  "Strait of Juan de Fuca",
  "\t49\t-125\t-121",
  "\t48\t-125\t-121",
  "\t47\t-125\t-121",
  "Strait of Malacca",
  "\t9\t97\t99",
  "\t8\t97\t100",
  "\t7\t95\t101",
  "\t6\t94\t101",
  "\t5\t94\t101",
  "\t4\t94\t102",
  "\t3\t96\t103",
  "\t2\t97\t104",
  "\t1\t98\t104",
  "\t0\t99\t104",
  "\t-1\t101\t104",
  "Strait of Singapore",
  "\t2\t102\t105",
  "\t1\t102\t105",
  "\t0\t102\t105",
  "Straits of Florida",
  "\t27\t-81\t-77",
  "\t26\t-82\t-77",
  "\t25\t-84\t-77",
  "\t24\t-84\t-77",
  "\t23\t-84\t-77",
  "\t22\t-84\t-78",
  "Sulu Sea",
  "\t13\t118\t122",
  "\t12\t118\t123",
  "\t11\t118\t123",
  "\t10\t117\t124",
  "\t9\t116\t124",
  "\t8\t115\t124",
  "\t7\t115\t124",
  "\t6\t115\t123",
  "\t5\t115\t123",
  "\t4\t116\t121",
  "Sulzberger Bay",
  "\t-75\t-153\t-144",
  "\t-76\t-159\t-144",
  "\t-77\t-159\t-144",
  "\t-78\t-159\t-144",
  "Taiwan Strait",
  "\t26\t117\t122",
  "\t25\t116\t122",
  "\t24\t116\t122",
  "\t23\t116\t121",
  "\t22\t116\t121",
  "Tasman Sea",
  "\t-28\t152\t160",
  "\t-29\t152\t160",
  "\t-30\t151\t162",
  "\t-31\t150\t166",
  "\t-32\t150\t170",
  "\t-33\t149\t174",
  "\t-34\t149\t174",
  "\t-35\t148\t175",
  "\t-36\t148\t175",
  "\t-37\t147\t175",
  "\t-38\t146\t176",
  "\t-39\t146\t176",
  "\t-40\t146\t176",
  "\t-41\t146\t176",
  "\t-42\t145\t175",
  "\t-43\t145\t172",
  "\t-44\t145\t171",
  "\t-45\t147\t169",
  "\t-46\t150\t168",
  "\t-47\t152\t168",
  "\t-48\t155\t168",
  "\t-49\t158\t167",
  "\t-50\t160\t167",
  "\t-51\t163\t167",
  "Tatar Strait",
  "\t54\t139\t142",
  "\t53\t139\t142",
  "\t52\t139\t142",
  "\t51\t140\t142",
  "\t50\t140\t142",
  "The North Western Passages",
  "\t81\t-101\t-95",
  "\t80\t-108\t-90\t-88\t-82",
  "\t79\t-114\t-80",
  "\t78\t-117\t-80",
  "\t77\t-120\t-80",
  "\t76\t-120\t-81",
  "\t75\t-120\t-78",
  "\t74\t-120\t-76",
  "\t73\t-106\t-76",
  "\t72\t-106\t-76",
  "\t71\t-118\t-116\t-106\t-83",
  "\t70\t-119\t-112\t-108\t-83",
  "\t69\t-119\t-85",
  "\t68\t-119\t-92",
  "\t67\t-118\t-92",
  "\t66\t-116\t-106\t-104\t-94",
  "\t65\t-97\t-94",
  "Timor Sea",
  "\t-7\t125\t131",
  "\t-8\t123\t131",
  "\t-9\t121\t132",
  "\t-10\t121\t133",
  "\t-11\t121\t133",
  "\t-12\t122\t133",
  "\t-13\t124\t133",
  "\t-14\t125\t131",
  "Torres Strait",
  "\t-8\t140\t144",
  "\t-9\t140\t144",
  "\t-10\t140\t144",
  "\t-11\t141\t144",
  "\t-12\t141\t143",
  "Trondheimsfjorden",
  "\t65\t10\t12",
  "\t64\t7\t12",
  "\t63\t7\t12",
  "\t62\t7\t12",
  "Tsugaru Strait",
  "\t42\t139\t142",
  "\t41\t139\t142",
  "\t40\t139\t142",
  "\t39\t139\t142",
  "Tyrrhenian Sea",
  "\t45\t8\t11",
  "\t44\t8\t11",
  "\t43\t8\t12",
  "\t42\t8\t14",
  "\t41\t8\t16",
  "\t40\t8\t17",
  "\t39\t7\t17",
  "\t38\t7\t17",
  "\t37\t7\t17",
  "\t36\t10\t14",
  "Uchiura Bay",
  "\t43\t139\t144",
  "\t42\t139\t144",
  "\t41\t139\t144",
  "\t40\t139\t143",
  "\t39\t140\t142",
  "Uda Bay",
  "\t57\t136\t139",
  "\t56\t134\t139",
  "\t55\t134\t139",
  "\t54\t134\t139",
  "\t53\t134\t139",
  "\t52\t135\t138",
  "Ungava Bay",
  "\t61\t-71\t-63",
  "\t60\t-71\t-63",
  "\t59\t-71\t-63",
  "\t58\t-71\t-64",
  "\t57\t-71\t-64",
  "\t56\t-70\t-66",
  "Vestfjorden",
  "\t69\t12\t18",
  "\t68\t11\t18",
  "\t67\t11\t18",
  "\t66\t11\t17",
  "\t65\t12\t14",
  "Vil'kitskogo Strait",
  "\t79\t99\t106",
  "\t78\t99\t106",
  "\t77\t99\t106",
  "\t76\t99\t106",
  "\t75\t99\t101",
  "Vincennes Bay",
  "\t-65\t103\t111",
  "\t-66\t103\t111",
  "\t-67\t103\t111",
  "Viscount Melville Sound",
  "\t76\t-110\t-103",
  "\t75\t-115\t-103",
  "\t74\t-116\t-103",
  "\t73\t-116\t-103",
  "\t72\t-116\t-104",
  "\t71\t-114\t-107",
  "Weddell Sea",
  "\t-70\t-62\t-9",
  "\t-71\t-63\t-9",
  "\t-72\t-63\t-9",
  "\t-73\t-64\t-10",
  "\t-74\t-66\t-13",
  "\t-75\t-78\t-14",
  "\t-76\t-84\t-17",
  "\t-77\t-84\t-25",
  "\t-78\t-84\t-22",
  "\t-79\t-84\t-22",
  "\t-80\t-82\t-22",
  "\t-81\t-79\t-23",
  "\t-82\t-70\t-36",
  "\t-83\t-66\t-50\t-48\t-42",
  "\t-84\t-62\t-57",
  "White Sea",
  "\t69\t37\t45",
  "\t68\t30\t33\t37\t45",
  "\t67\t30\t45",
  "\t66\t30\t45",
  "\t65\t31\t45",
  "\t64\t33\t41",
  "\t63\t33\t41",
  "\t62\t35\t38",
  "Wrigley Gulf",
  "\t-72\t-131\t-124",
  "\t-73\t-135\t-123",
  "\t-74\t-135\t-123",
  "\t-75\t-135\t-123",
  "Yellow Sea",
  "\t41\t123\t125",
  "\t40\t120\t126",
  "\t39\t120\t126",
  "\t38\t119\t127",
  "\t37\t119\t127",
  "\t36\t118\t127",
  "\t35\t118\t127",
  "\t34\t118\t127",
  "\t33\t118\t127",
  "\t32\t119\t127",
  "\t31\t119\t125",
  "\t30\t120\t123",
  "Yellowstone Lake",
  "\t45\t-111\t-109",
  "\t44\t-111\t-109",
  "\t43\t-111\t-109",
  "Yenisey Gulf",
  "\t74\t77\t81",
  "\t73\t77\t83",
  "\t72\t77\t84",
  "\t71\t77\t84",
  "\t70\t79\t84",
  "\t69\t81\t84",
  "Yucatan Channel",
  "\t23\t-86\t-84",
  "\t22\t-88\t-83",
  "\t21\t-88\t-83",
  "\t20\t-88\t-83"
};


static const int k_NumLatLonWaterText = sizeof (s_DefaultLatLonWaterText) / sizeof (char *);

void CLatLonCountryMap::x_InitFromDefaultList(const char **list, int num)
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
                for (int j = 2; j < tokens.size() - 1; j++) {
                    m_CountryLineList.push_back(new CCountryLine(current_country, x, NStr::StringToDouble(tokens[j]), NStr::StringToDouble(tokens[j + 1]), m_Scale));
                }
            }
        }
    }
}




bool CLatLonCountryMap::x_InitFromFile(string filename)
{
      // note - may want to do this initialization later, when needed
    string dir;
    if (CNcbiApplication* app = CNcbiApplication::Instance()) {
        dir = app->GetConfig().Get("NCBI", "Data");
        if ( !dir.empty()  
            && CFile(CDirEntry::MakePath(dir, filename)).Exists()) {
            dir = CDirEntry::AddTrailingPathSeparator(dir);
        } else {
            dir.erase();
        }
    }
    if (dir.empty()) {
        return false;
    }

    CRef<ILineReader> lr;
    if ( !dir.empty() ) {
        lr.Reset(ILineReader::New
                 (CDirEntry::MakePath(dir, filename)));
    }
    if (lr.Empty()) {
        return false;
    } else {
        m_Scale = 20.0;
        string current_country = "";
        for (++*lr; !lr->AtEOF(); ++*lr) {
                  const string& line = **lr;
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
            }
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
    CRef<ILineReader> lr(ILineReader::New(file));

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
