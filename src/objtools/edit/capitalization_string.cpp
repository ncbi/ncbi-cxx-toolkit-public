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
* Author:  Andrea Asztalos
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

typedef CStaticPairArrayMap<const char*, const char*, PCase_CStr> TCStringPairsMap;
DEFINE_STATIC_ARRAY_MAP(TCStringPairsMap,k_state_abbrev, map_state_to_abbrev);

void FixCapitalizationInString (objects::CSeq_entry_Handle seh, string& str, ECapChange capchange_opt)
{
    if (NStr::IsBlank(str) || capchange_opt == eCapChange_none) {
        return;
    } else {
        switch (capchange_opt) {
            case eCapChange_tolower:
                NStr::ToLower(str); 
                FixAbbreviationsInElement(str);
                FixOrgNames(seh, str);
                break;
            case eCapChange_toupper:
                NStr::ToUpper(str);
                FixAbbreviationsInElement(str);
                FixOrgNames(seh, str);
                break;
            case eCapChange_firstcap_restlower:
                NStr::ToLower(str);
                if ( isalpha(str[0]) ) {
                    str[0] = toupper(str[0]);
                }
                FixAbbreviationsInElement(str);
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
                FixAbbreviationsInElement(str);
                FixOrgNames(seh, str);
            }
                break;
            default:
                break;
        }
    }
}

void FixAbbreviationsInElement(string& result)
{
    for (int pat=0; set_abbreviation_list[pat].first[0]!='\0'; ++pat) {
        CRegexpUtil replacer( result );
        //int num_replacements = 
        replacer.Replace( set_abbreviation_list[pat].first, set_abbreviation_list[pat].second, 
            CRegexp::fCompile_ignore_case, CRegexp::fMatch_default, 0);
        replacer.GetResult().swap( result );
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
                NStr::CompareNocase(country, "U S A") == 0 ) 
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

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE



