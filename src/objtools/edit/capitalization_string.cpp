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
* Author:  Andrea Asztalos, Igor Filippov
*
* File Description:
*   Implement capitalization change in strings. 
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/biblio/Auth_list.hpp>

#include <util/xregexp/regexp.hpp>
#include <objtools/edit/capitalization_string.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

static const SStaticPair<const char*, const char*> set_abbreviation_list[] = 
{
    {"\\barabidopsis thaliana\\b","Arabidopsis thaliana"},
    {"\\badp\\b", "ADP" },
    {"\\batp\\b", "ATP" },
    {"\\bbac\\b", "BAC" },
    {"\\bcaenorhabditis elegans\\b", "Caenorhabditis elegans" },
    {"\\bcdna\\b", "cDNA" },
    {"\\bcdnas\\b", "cDNAs" },
    {"\\bcoa\\b", "CoA" },
    {"\\bcoi\\b", "COI" },
    {"\\bcoii\\b", "COII" },
    {"\\bdanio rerio\\b", "Danio rerio" },
    {"\\bdna\\b", "DNA" },
    {"\\bdrosophila melanogaster\\b", "Drosophila melanogaster" },
    {"\\bdsrna\\b", "dsRNA" },
    {"\\bescherichia coli\\b", "Escherichia coli" },
    {"\\bhiv\\b", "HIV" },
    {"\\bhiv\\-1\\b", "HIV-1" },
    {"\\bhiv\\-2\\b", "HIV-2" },
    {"\\bhnrna\\b", "hnRNA" },
    {"\\bhomo sapiens\\b", "Homo sapiens" },
    {"\\bmhc\\b", "MHC" },
    {"\\bmrna\\b", "mRNA" },
    {"\\bmtdna\\b", "mtDNA" },
    {"\\bmus musculus\\b", "Mus musculus" },
    {"\\bnadh\\b", "NADH" },
    {"\\bnov\\.\\b", "nov." },
    {"\\bnov\\.\\.\\b", "nov.." },
    {"\\bpcr\\b", "PCR" },
    {"\\brattus norvegicus\\b", "Rattus norvegicus" },
    {"\\brapd\\b", "RAPD" },
    {"\\brdna\\b", "rDNA" },
    {"\\brna\\b", "RNA" },
    {"\\brrna\\b", "rRNA" },
    {"\\brt\\-pcr\\b", "RT-PCR" },
    {"\\bsaccharomyces cerevisiae\\b", "Saccharomyces cerevisiae" },
    {"\\bscrna\\b", "scRNA" },
    {"\\bsiv\\-1\\b", "SIV-1" },
    {"\\bsnp\\b", "SNP"     },
    {"\\bsnps\\b", "SNPs"   },
    {"\\bsnrna\\b", "snRNA" },
    {"\\bsp\\.\\b", "sp." },
    {"\\bsp\\.\\.\\b", "sp.." },
    {"\\bssp\\.\\b", "ssp." },
    {"\\bssp\\.\\.\\b", "ssp.." },
    {"\\bssrna\\b", "ssRNA" },
    {"\\bsubsp\\.\\b", "subsp." },
    {"\\bsubsp\\.\\.\\b", "subsp.." },
    {"\\btrna\\b", "tRNA" },
    {"\\bvar\\.\\b", "var." },
    {"\\bvar\\.\\.\\b", "var.." },
    {"\\buk\\b", "UK" },
    {"\\busa\\b", "USA" },
    {"\\bU\\.S\\.A\\.\\b", "USA" },
    {"\\bU\\.S\\.A\\b", "USA" },
    {"\\bUnited States of America\\b", "USA" },
    {"\\b\\(hiv\\)\\b", "(HIV)" },
    {"\\b\\(hiv1\\)\\b", "(HIV1)" },
    {"\\b\\(hiv\\-1\\)\\b", "(HIV-1)"},
   
    {"\0","\0"}
};

static const SStaticPair<const char*, const char*> set_abbreviation_list_end_of_sentence[] = 
{
    {"\\bsp\\.$", "sp.." },
    {"\\bnov\\.$", "nov.." },
    {"\\bssp\\.$", "ssp.." },
    {"\\bvar\\.$", "var.." },
    {"\\bsubsp\\.$", "subsp.."},
    {"\0","\0"}
};

static const SStaticPair<const char*, const char*> map_state_to_abbrev[] = 
{
{ "ala", "AL"},
{ "alabama", "AL"},
{ "alas", "AK"},
{ "alaska", "AK"},
{ "ariz", "AZ"},
{ "arizona", "AZ"},
{ "ark", "AR"},
{ "arkansas", "AR"},
{ "cal", "CA"},
{ "cali", "CA"},
{ "calif", "CA"},
{ "california", "CA"},
{ "col", "CO"},
{ "colo", "CO"},
{ "colorado", "CO"},
{ "conn", "CT"},
{ "connecticut", "CT"},
{ "del", "DE"},
{ "delaware", "DE"},
{ "fla", "FL"},
{ "florida", "FL"},
{ "georgia", "GA"},
{ "hawaii", "HI"},
{ "ida", "ID"},
{ "idaho", "ID"},
{ "ill", "IL"},
{ "illinois", "IL"},
{ "ind", "IN"},
{ "indiana", "IN"},
{ "iowa", "IA"},
{ "kan", "KS"},
{ "kans", "KS"},
{ "kansas", "KS"},
{ "ken", "KY"},
{ "kent", "KY"},
{ "kentucky", "KY"},
{ "louisiana", "LA"},
{ "maine", "ME"},
{ "maryland", "MD"},
{ "mass", "MA"},
{ "massachusetts", "MA"},
{ "mich", "MI"},
{ "michigan", "MI"},
{ "minn", "MN"},
{ "minnesota", "MN"},
{ "miss", "MS"},
{ "mississippi", "MS"},
{ "missouri", "MO"},
{ "mont", "MT"},
{ "montana", "MT"},
{ "n car", "NC"},
{ "n dak", "ND"},
{ "neb", "NE"},
{ "nebr", "NE"},
{ "nebraska", "NE"},
{ "nev", "NV"},
{ "nevada", "NV"},
{ "new hampshire", "NH"},
{ "new jersey", "NJ"},
{ "new mexico", "NM"},
{ "new york", "NY"},
{ "north carolina", "NC"},
{ "north dakota", "ND"},
{ "ohio", "OH"},
{ "okla", "OK"},
{ "oklahoma", "OK"},
{ "ore", "OR"},
{ "oreg", "OR"},
{ "oregon", "OR"},
{ "penn", "PA"},
{ "penna", "PA"},
{ "pennsylvania", "PA"},
{ "puerto rico", "PR"},
{ "rhode island", "RI"},
{ "s car", "SC"},
{ "s dak", "SD"},
{ "south carolina", "SC"},
{ "south dakota", "SD"},
{ "tenn", "TN"},
{ "tennessee", "TN"},
{ "tex", "TX"},
{ "texas", "TX"},
{ "utah", "UT"},
{ "vermont", "VT"},
{ "virg", "VA"},
{ "virginia", "VA"},
{ "wash", "WA"},
{ "washington", "WA"},
{ "west virginia", "WV"},
{ "wis", "WI"},
{ "wisc", "WI"},
{ "wisconsin", "WI"},
{ "wyo", "WY"},
{ "wyoming", "WY"}

};

static const string mouse_strain_fixes[] = {
    "129/Sv" ,
    "129/SvJ" ,
    "BALB/c" ,
    "C57BL/6" ,
    "C57BL/6J" ,
    "CD-1" ,
    "CZECHII" ,
    "FVB/N",
    "FVB/N-3" ,
    "ICR" ,
    "NMRI" ,
    "NOD" ,
    "C3H" ,
    "C57BL" ,
    "C57BL/6" ,
    "C57BL/6J" ,
    "DBA/2"
};

typedef CStaticPairArrayMap<const char*, const char*, PCase_CStr> TCStringPairsMap;
DEFINE_STATIC_ARRAY_MAP(TCStringPairsMap,k_state_abbrev, map_state_to_abbrev);

static const SStaticPair<const char*, const char*> set_short_words[] = 
{
    {"\\bA\\b", "a" },
    {"\\bAbout\\b", "about" },
    {"\\bAnd\\b", "and" },
    {"\\bAt\\b", "at" },
    {"\\bBut\\b", "but" },
    {"\\bBy\\b", "by" },
    {"\\bFor\\b", "for" },
    {"\\bIn\\b", "in" },
    {"\\bIs\\b", "is" },
    {"\\bOf\\b", "of" },
    {"\\bOn\\b", "on" },
    {"\\bOr\\b", "or" },
    {"\\bThe\\b", "the" },
    {"\\bTo\\b", "to" },
    {"\\bWith\\b", "with" },
    {"\0","\0"}
};


static const SStaticPair<const char*, const char*> set_country_fixes[] = 
{

    {"\\bchnia\\b", "China" },
    {"\\bpr china\\b", "P.R. China" },
    {"\\bprchina\\b", "P.R. China" },
    {"\\bp\\.r\\.china\\b", "P.R. China" },
    {"\\bp\\.r china\\b", "P.R. China" },
    {"\\bp\\, r\\, china\\b", "P.R. China" },
    {"\\brok\\b", "ROK" },
    {"\\brsa\\b", "RSA" },
    {"\\broc\\b", "ROC" },
    {"\\buae\\b", "UAE" },
    {"\\bK\\.S\\.A\\.\\b", "K.S.A." },
    {"\\bk\\. s\\. a\\.\\b", "K. S. A." },
    {"\\bksa\\b", "KSA" },
    {"\0","\0"}
};

static const SStaticPair<const char*, const char*> set_AffiliationShortWordList[] = 
{
    {"\\bAu\\b", "au" },
    {"\\bAux\\b", "aux" },
    {"\\bA La\\b", "a la" },
    {"\\bDe La\\b", "de la" },
    {"\\bDe\\b", "de" },
    {"\\bDel\\b", "del"},
    {"\\bDes\\b", "des" },
    {"\\bDu\\b", "du" },
    {"\\bEt\\b", "et" },
    {"\\bLa\\b", "la" },
    {"\\bLe\\b", "le" },
    {"\\bLes\\b", "les" },
    {"\\bRue\\b", "rue" },
    {"\\bPo Box\\b", "PO Box" },
    {"\\bPobox\\b", "PO Box" },
    {"\\bP\\.O box\\b", "P.O. Box" },
    {"\\bP\\.Obox\\b", "P.O. Box" },
    {"\\bY\\b", "y" },
    {"\\bA\\&F\\b", "A&F" },    // Northwest A&F University
    {"\0","\0"}
};

static const char* set_ordinal_endings[] = 
{
    "\\dth\\b",
    "\\dst\\b",
    "\\dnd\\b",
    "\\drd\\b",
    "\0"
};

static const SStaticPair<const char*, const char*> set_KnownAbbreviationList[] = 
{
    {"\\bpo box\\b", "PO Box" },
    {"\\bPobox\\b", "PO Box" },
    {"\\bP\\.O box\\b", "P.O. Box" },
    {"\\bP\\.Obox\\b", "P.O. Box" },
    {"\\bPO\\.Box\\b", "P.O. Box" },
    {"\\bPO\\. Box\\b", "P.O. Box" },
    {"\\bpr china\\b", "P.R. China"},
    {"\\bprchina\\b", "P.R. China" },
    {"\\bp\\.r\\.china\\b", "P.R. China" },
    {"\\bp\\.r china\\b", "P.R. China" },
    {"\\bp\\, r\\, china\\b", "P.R. China" },
    {"\\bp\\,r\\, china\\b", "P.R. China" },
    {"\\bp\\,r\\,china\\b", "P.R. China" },
    {"\0","\0"}  // end of array
};

static const char* set_valid_country_codes[] = 
{
  "Afghanistan",
  "Albania",
  "Algeria",
  "American Samoa",
  "Andorra",
  "Angola", 
  "Anguilla",
  "Antarctica",
  "Antigua and Barbuda",
  "Arctic Ocean",
  "Argentina",   
  "Armenia",     
  "Aruba",       
  "Ashmore and Cartier Islands",
  "Atlantic Ocean",
  "Australia",
  "Austria",  
  "Azerbaijan",
  "Bahamas",   
  "Bahrain",   
  "Baker Island",
  "Baltic Sea",  
  "Bangladesh",  
  "Barbados",    
  "Bassas da India",
  "Belarus",
  "Belgium",
  "Belize", 
  "Benin",  
  "Bermuda",
  "Bhutan", 
  "Bolivia",
  "Borneo", 
  "Bosnia and Herzegovina",
  "Botswana",
  "Bouvet Island",
  "Brazil",
  "British Virgin Islands",
  "Brunei",
  "Bulgaria",
  "Burkina Faso",
  "Burundi",
  "Cambodia",
  "Cameroon",
  "Canada",  
  "Cape Verde",
  "Cayman Islands",
  "Central African Republic",
  "Chad",
  "Chile",
  "China",
  "Christmas Island",
  "Clipperton Island",
  "Cocos Islands",
  "Colombia",
  "Comoros", 
  "Cook Islands",
  "Coral Sea Islands",
  "Costa Rica",
  "Cote d'Ivoire",
  "Croatia",
  "Cuba",   
  "Curacao",
  "Cyprus", 
  "Czech Republic",
  "Democratic Republic of the Congo",
  "Denmark",
  "Djibouti",
  "Dominica",
  "Dominican Republic",
  "East Timor",
  "Ecuador",   
  "Egypt",     
  "El Salvador",
  "Equatorial Guinea",
  "Eritrea",
  "Estonia",
  "Ethiopia",
  "Europa Island",
  "Falkland Islands (Islas Malvinas)",
  "Faroe Islands",
  "Fiji",
  "Finland",
  "France", 
  "French Guiana",
  "French Polynesia",
  "French Southern and Antarctic Lands",
  "Gabon",
  "Gambia",
  "Gaza Strip",
  "Georgia",   
  "Germany",   
  "Ghana",     
  "Gibraltar", 
  "Glorioso Islands",
  "Greece",
  "Greenland",
  "Grenada",  
  "Guadeloupe",
  "Guam",
  "Guatemala",
  "Guernsey", 
  "Guinea",   
  "Guinea-Bissau",
  "Guyana",
  "Haiti", 
  "Heard Island and McDonald Islands",
  "Honduras",
  "Hong Kong",
  "Howland Island",
  "Hungary",
  "Iceland",
  "India",  
  "Indian Ocean",
  "Indonesia",   
  "Iran",
  "Iraq",
  "Ireland",
  "Isle of Man",
  "Israel",
  "Italy", 
  "Jamaica",
  "Jan Mayen",
  "Japan",
  "Jarvis Island",
  "Jersey",
  "Johnston Atoll",
  "Jordan",
  "Juan de Nova Island",
  "Kazakhstan",
  "Kenya",
  "Kerguelen Archipelago",
  "Kingman Reef",
  "Kiribati",
  "Kosovo",  
  "Kuwait",  
  "Kyrgyzstan",
  "Laos",
  "Latvia",
  "Lebanon",
  "Lesotho",
  "Liberia",
  "Libya",  
  "Liechtenstein",
  "Line Islands", 
  "Lithuania",    
  "Luxembourg",   
  "Macau",
  "Macedonia",
  "Madagascar",
  "Malawi",
  "Malaysia",
  "Maldives",
  "Mali",
  "Malta",
  "Marshall Islands",
  "Martinique",
  "Mauritania",
  "Mauritius", 
  "Mayotte",   
  "Mediterranean Sea",
  "Mexico",
  "Micronesia",
  "Midway Islands",
  "Moldova",
  "Monaco", 
  "Mongolia",
  "Montenegro",
  "Montserrat",
  "Morocco",   
  "Mozambique",
  "Myanmar",   
  "Namibia",   
  "Nauru",     
  "Navassa Island",
  "Nepal",
  "Netherlands",
  "New Caledonia",
  "New Zealand",  
  "Nicaragua",    
  "Niger",
  "Nigeria",
  "Niue",   
  "Norfolk Island",
  "North Korea",   
  "North Sea",     
  "Northern Mariana Islands",
  "Norway",
  "Oman",  
  "Pacific Ocean",
  "Pakistan",
  "Palau",   
  "Palmyra Atoll",
  "Panama",
  "Papua New Guinea",
  "Paracel Islands", 
  "Paraguay",
  "Peru",
  "Philippines",
  "Pitcairn Islands",
  "Poland",
  "Portugal",
  "Puerto Rico",
  "Qatar",
  "Republic of the Congo",
  "Reunion",
  "Romania",
  "Ross Sea",
  "Russia",  
  "Rwanda",  
  "Saint Helena",
  "Saint Kitts and Nevis",
  "Saint Lucia",
  "Saint Pierre and Miquelon",
  "Saint Vincent and the Grenadines",
  "Samoa",
  "San Marino",
  "Sao Tome and Principe",
  "Saudi Arabia",
  "Senegal",
  "Serbia", 
  "Seychelles",
  "Sierra Leone",
  "Singapore",   
  "Sint Maarten",
  "Slovakia",
  "Slovenia",
  "Solomon Islands",
  "Somalia",
  "South Africa",
  "South Georgia and the South Sandwich Islands",
  "South Korea",
  "South Sudan",
  "Southern Ocean",
  "Spain",
  "Spratly Islands",
  "Sri Lanka",
  "Sudan",
  "Suriname",
  "Svalbard",
  "Swaziland",
  "Sweden",   
  "Switzerland",
  "Syria",
  "Taiwan",
  "Tajikistan",
  "Tanzania",  
  "Tasman Sea",
  "Thailand",  
  "Togo",
  "Tokelau",
  "Tonga",  
  "Trinidad and Tobago",
  "Tromelin Island",
  "Tunisia",
  "Turkey", 
  "Turkmenistan",
  "Turks and Caicos Islands",
  "Tuvalu",
  "Uganda",
  "Ukraine",
  "United Arab Emirates",
  "United Kingdom",
  "Uruguay",
  "USA",
  "Uzbekistan",
  "Vanuatu",   
  "Venezuela", 
  "Viet Nam",  
  "Virgin Islands",
  "Wake Island",   
  "Wallis and Futuna",
  "West Bank",
  "Western Sahara",
  "Yemen",
  "Zambia",
  "Zimbabwe",
   "\0"
};


void FixCapitalizationInString (objects::CSeq_entry_Handle seh, string& str, ECapChange capchange_opt)
{
    if (NStr::IsBlank(str) || capchange_opt == eCapChange_none) {
        return;
    } else {
        switch (capchange_opt) {
            case eCapChange_tolower:
                NStr::ToLower(str); 
                FixAbbreviationsInElement(str,seh);
                FixOrgNames(seh, str);
                break;
            case eCapChange_toupper:
                NStr::ToUpper(str);
                FixAbbreviationsInElement(str,seh);
                FixOrgNames(seh, str);
                break;
            case eCapChange_firstcap_restlower:
                NStr::ToLower(str);
                if ( isalpha(str[0]) ) {
                    str[0] = toupper(str[0]);
                }
                FixAbbreviationsInElement(str,seh);
                FixOrgNames(seh, str);
                break;
            case eCapChange_firstcap_restnochange:
                if ( isalpha(str[0]) ) {
                   str[0] = toupper(str[0]);
                }
                break;
            case eCapChange_firstlower_restnochange:
                if ( isalpha(str[0]) ) {
                    str[0] = tolower(str[0]);
                }
                break;
            case eCapChange_capword_afterspace:
            case eCapChange_capword_afterspacepunc:
            {
                NStr::ToLower(str);
                vector<string> words;
                NStr::Tokenize(str, " \t\r\n", words);
                for (vector<string>::iterator word = words.begin(); word != words.end(); ++word) {
                    if (!word->empty() && isalpha(word->at(0))) {
                        word->at(0) = toupper(word->at(0));
                    }
                }
                str = NStr::Join(words, " ");
                if (capchange_opt == eCapChange_capword_afterspacepunc) {
                    bool found_punct = false;
                    for (SIZE_TYPE n = 0; n < str.size(); ++n) {
                        if (ispunct(str[n])) {
                            found_punct = true;
                        } else if (isalpha(str[n]) && found_punct) {
                            str[n] = toupper(str[n]);
                            found_punct = false;
                        }
                    }
                }
                FixAbbreviationsInElement(str,seh);
                FixOrgNames(seh, str);
            }
                break;
            default:
                break;
        }
    }
}

void FixAbbreviationsInElement(string& result, bool fix_end_of_sentence)
{
    for (int pat=0; set_abbreviation_list[pat].first[0]!='\0'; ++pat) {
        CRegexpUtil replacer( result );
        //int num_replacements = 
        replacer.Replace( set_abbreviation_list[pat].first, set_abbreviation_list[pat].second, 
            CRegexp::fCompile_ignore_case, CRegexp::fMatch_default, 0);
        replacer.GetResult().swap( result );
    }
    if (fix_end_of_sentence)
    {
        for (int pat=0; set_abbreviation_list_end_of_sentence[pat].first[0]!='\0'; ++pat) {
            CRegexpUtil replacer( result );
            replacer.Replace( set_abbreviation_list_end_of_sentence[pat].first, set_abbreviation_list_end_of_sentence[pat].second, 
                              CRegexp::fCompile_ignore_case, CRegexp::fMatch_default, 0);
            replacer.GetResult().swap( result );
        }
    }
    
}

void FixOrgNames(objects::CSeq_entry_Handle seh, string& result)
{
    vector<string> taxnames;
    FindOrgNames(seh,taxnames);
    for(vector<string>::const_iterator name=taxnames.begin(); name!=taxnames.end(); ++name) {
        CRegexpUtil replacer( result );
        //int num_replacements = 
        replacer.Replace( "\\b"+*name+"\\b", *name, CRegexp::fCompile_ignore_case, CRegexp::fMatch_default, 0);
        replacer.GetResult().swap( result );
    }
}

void FindOrgNames(objects::CSeq_entry_Handle seh, vector<string>& taxnames)
{
    if (!seh) return;
    objects::CBioseq_CI b_iter(seh, objects::CSeq_inst::eMol_na);
    for ( ; b_iter ; ++b_iter ) {
        objects::CSeqdesc_CI it (*b_iter, objects::CSeqdesc::e_Source);
        if (it) {
            if (it->GetSource().IsSetTaxname()) {
                taxnames.push_back(it->GetSource().GetTaxname());
            }
        }
    }
}

void RemoveFieldNameFromString( const string& field_name, string& str)
{
    if (NStr::IsBlank(field_name) || NStr::IsBlank(str)) {
        return;
    }
    
    NStr::TruncateSpacesInPlace(str);
    if (NStr::StartsWith(str, field_name, NStr::eNocase) && str.length() > field_name.length()
        && str[field_name.length()] == ' ') {
        NStr::ReplaceInPlace(str, field_name, kEmptyStr, 0, 1);
        NStr::TruncateSpacesInPlace(str);
    }
}

void GetStateAbbreviation(string& state)
{
    NStr::ReplaceInPlace (state, "  ", " ");
    NStr::TruncateSpacesInPlace (state);
    TCStringPairsMap::const_iterator found = k_state_abbrev.find(NStr::ToLower(state).c_str());
    if (found != k_state_abbrev.end())
        state = found->second;
    else
        NStr::ToUpper(state);
}

bool FixStateAbbreviationsInCitSub(CCit_sub& sub)
{
    bool modified = false;
    if (sub.IsSetAuthors() && sub.GetAuthors().IsSetAffil() && sub.GetAuthors().GetAffil().IsStd()) {
        CAffil::C_Std& std = sub.SetAuthors().SetAffil().SetStd();
        if (std.IsSetCountry()) {
            string country = std.GetCountry();
            NStr::ReplaceInPlace (country, "  ", " ");
            NStr::TruncateSpacesInPlace (country);

            if (NStr::CompareNocase(country, "United States of America") == 0 ||
                NStr::CompareNocase(country, "United States") == 0 ||
                NStr::CompareNocase(country, "U.S.A.") == 0 ||
                NStr::CompareNocase(country, "U S A") == 0 ||
                NStr::CompareNocase(country, "US") == 0) 
            {
                std.SetCountry("USA");
                modified = true;
            }
        }

        modified |= FixStateAbbreviationsInAffil(sub.SetAuthors().SetAffil());
    }
    return modified;
}

bool FixStateAbbreviationsInAffil(CAffil& affil)
{
    if (affil.IsStd()) {
        CAffil::C_Std& std = affil.SetStd();
        if (std.IsSetCountry() && NStr::EqualCase(std.GetCountry(), "USA")) {
            if (std.IsSetSub() && !NStr::IsBlank(std.GetSub())) {
                string state = std.GetSub();
                edit::GetStateAbbreviation(state); // update the state abbreviation
                if (!NStr::IsBlank(state) && !NStr::EqualCase(std.GetSub(), state)) {
                    std.SetSub(state);
                    return true;
                }
            }
        }
    }
    return false;
}

bool FixupMouseStrain(string& strain)
{
    if (NStr::IsBlank(strain))
        return false;

    NStr::TruncateSpacesInPlace (strain);

    bool whole_word = true;
    for (unsigned int i = 0; i < sizeof(mouse_strain_fixes)/sizeof(mouse_strain_fixes[0]); ++i) {
        CRegexpUtil replacer(strain);
        string pattern = whole_word ? ("\\b" + mouse_strain_fixes[i] + "\\b") : mouse_strain_fixes[i];
        // whole-word and case insensitive search
        if (replacer.Replace(pattern, mouse_strain_fixes[i], CRegexp::fCompile_ignore_case) > 0) {
            replacer.GetResult().swap(strain);
            return true;
        }
    }
    return false;
}

void InsertMissingSpacesAfterCommas(string& result)
{
    CRegexpUtil replacer( result );
    replacer.Replace( "\\,(\\S)", ", $1", CRegexp::fCompile_default, CRegexp::fMatch_default, 0);
    replacer.GetResult().swap( result );
}

void InsertMissingSpacesAfterNo(string& result)
{
    CRegexpUtil replacer( result );
    replacer.Replace( "No\\.(\\w)", "No. $1", CRegexp::fCompile_ignore_case, CRegexp::fMatch_default, 0);
    replacer.GetResult().swap( result );
}

void FixCapitalizationInElement(string& result)
{
    result = NStr::ToLower(result);
    bool capitalize = true;
    for (unsigned int i=0; i<result.size(); i++)
    {
        char &a = result.at(i);
        if (isalpha(a))
        {
            if (capitalize)
                a = toupper(a);
            capitalize = false;
        }
        else if (a != '\'')
            capitalize = true;
    }
}

void FixShortWordsInElement(string& result)
{
    for (int pat=0; set_short_words[pat].first[0]!='\0'; ++pat)
    {
        CRegexpUtil replacer( result );
        replacer.Replace( set_short_words[pat].first, set_short_words[pat].second, CRegexp::fCompile_ignore_case, CRegexp::fMatch_default, 0);
        replacer.GetResult().swap( result );
    }
    result.at(0) = toupper(result.at(0));
}


void FindReplaceString_CountryFixes(string& result)
{
    for (int pat=0; set_country_fixes[pat].first[0] != '\0'; ++pat)
    {
        CRegexpUtil replacer( result );
        replacer.Replace( set_country_fixes[pat].first, set_country_fixes[pat].second, CRegexp::fCompile_ignore_case, CRegexp::fMatch_default, 0);
        replacer.GetResult().swap( result );
    }
}

void CapitalizeAfterApostrophe(string& input)
{
    string result;
    CRegexp pattern("\\'\\w");
    size_t start = 0;
    for (;;) {
        pattern.GetMatch(input, start, 0, CRegexp::fMatch_default, true);
        if (pattern.NumFound() > 0) {
            const int* rslt = pattern.GetResults(0);
            if (rslt[0] != start)
                result += input.substr(start,rslt[0]-start);
            string tmp = input.substr(rslt[0], rslt[1] - rslt[0]);
            result += NStr::ToUpper(tmp);
            start = rslt[1];
        } else {
            result += input.substr(start,input.length()-start);
            break;
        }
    }
    input = result;
}

void FixAffiliationShortWordsInElement(string& result)
{
    if (result.empty()) return;
    for (int pat=0; set_AffiliationShortWordList[pat].first[0]!='\0'; ++pat)
    {
        CRegexpUtil replacer( result );
        //int num_replacements = 
        replacer.Replace( set_AffiliationShortWordList[pat].first, 
            set_AffiliationShortWordList[pat].second, CRegexp::fCompile_ignore_case, CRegexp::fMatch_default, 0);
        replacer.GetResult().swap( result );
    }
    result.at(0) = toupper(result.at(0));
    // fix d'
    {
        CRegexpUtil replacer( result );
        //int num_replacements = 
        replacer.Replace( "\\bD\\'", "d'", CRegexp::fCompile_default, CRegexp::fMatch_default, 0);
        replacer.GetResult().swap( result );
        
        string temp;
        CRegexp pattern("\\bd\\'\\w");
        size_t start = 0;
        for (;;) {
            pattern.GetMatch(result, start, 0, CRegexp::fMatch_default, true);
            if (pattern.NumFound() > 0) {
                const int* rslt = pattern.GetResults(0);
                if (rslt[0] != start)
                    temp += result.substr(start,rslt[0]-start);
                string tmp = result.substr(rslt[0], rslt[1] - rslt[0]);
                tmp = NStr::ToUpper(tmp);
                tmp.at(0) = 'd';
                temp += tmp;
                start = rslt[1];
            } else {
                temp += result.substr(start,result.length()-start);
                break;
            }
        }
        result = temp;
    }
}

void FixOrdinalNumbers(string& result)
{
    for(int p = 0; set_ordinal_endings[p][0] != '\0'; ++p)
    {
        CRegexp pattern(set_ordinal_endings[p],CRegexp::fCompile_ignore_case);
        string temp;
        size_t start = 0;
        for (;;) {
            pattern.GetMatch(result, start, 0, CRegexp::fMatch_default, true);
            if (pattern.NumFound() > 0) {
                const int* rslt = pattern.GetResults(0);
                if (rslt[0] != start)
                    temp += result.substr(start,rslt[0]-start);
                string tmp = result.substr(rslt[0], rslt[1] - rslt[0]);
                tmp = NStr::ToLower(tmp);
                temp += tmp;
                start = rslt[1];
            } else {
                temp += result.substr(start,result.length()-start);
                break;
            }
        }
        result = temp;
    }
}

void FixKnownAbbreviationsInElement(string& result)
{
    if (result.empty()) return;
    for (int pat=0; set_KnownAbbreviationList[pat].first[0] != '\0' ; ++pat)
    {
        CRegexpUtil replacer( result );
        //int num_replacements = 
        replacer.Replace( set_KnownAbbreviationList[pat].first, set_KnownAbbreviationList[pat].second, CRegexp::fCompile_ignore_case, CRegexp::fMatch_default, 0);
        replacer.GetResult().swap( result );
    }
}

void CapitalizeSAfterNumber(string& result)
{
    CRegexpUtil replacer( result );
    //int num_replacements = 
    replacer.Replace( "(\\d)s\\b", "$1S", CRegexp::fCompile_default, CRegexp::fMatch_default, 0);
    replacer.GetResult().swap( result );
}

void ResetCapitalization(string& result, bool first_is_upper)
{

    if (result.empty()) return;

    bool was_digit = false;

    if (first_is_upper)
    {
        /* Set first character to upper */
        result[0] = toupper(result[0]);
    }
    else
    {   
        /* set first character to lower */
        result[0] = tolower(result[0]);
    }
   
    if (isdigit ((Int4)(result[0])))
    {
        was_digit = true;
    }
    unsigned int i = 1;
  /* Set rest of characters to lower */
    while (i < result.size())
    {
        char &pCh = result[i];
        if (was_digit && (pCh == 'S' || pCh == 's') && (i+1 >= result.size()-1 || isspace(result[i+1])))
            {
                pCh = toupper (pCh);
                was_digit = false;
            }
            else if (isdigit (pCh))
            {
                was_digit = true;
            }
            else
            {   
                was_digit = false;
                pCh = tolower (pCh);
            }
            i++;
    }  
}    

void FixCountryCapitalization(string& result)
{
    for(unsigned int p = 0; set_valid_country_codes[p][0] != '\0'; ++p)
    {
        string name = set_valid_country_codes[p];
        CRegexpUtil replacer( result );
        replacer.Replace( "\\b"+name+"\\b", name, CRegexp::fCompile_ignore_case, CRegexp::fMatch_default, 0);
        replacer.GetResult().swap( result );
    }
}

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE



