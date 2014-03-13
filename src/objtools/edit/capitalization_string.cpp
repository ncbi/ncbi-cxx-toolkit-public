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

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE



