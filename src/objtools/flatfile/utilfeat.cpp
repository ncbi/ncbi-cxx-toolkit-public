/* $Id$
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
 * File Name: utilfeat.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description: functions for features parsing
 *
 */

#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>

#include "index.h"

#include <objtools/flatfile/flatdefn.h>
#include <objtools/cleanup/cleanup.hpp>

#include "ftaerr.hpp"
#include "asci_blk.h"
#include "add.h"
#include "utilfeat.h"
#include "utilfun.h"

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "utilfeat.cpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

const char* ParFlat_GImod[] = {
    "Mitochondr",
    "Chloroplast",
    "Kinetoplas",
    "Cyanelle",
    "Chromoplast",
    "Plastid",
    "Macronuclear",
    "Extrachrom",
    "Plasmid",
    "Leucoplast",
    "Apicoplast",
    nullptr
};

const char* valid_organelle[] = {
    "apicoplast",
    "chloroplast",
    "chromatophore",
    "chromoplast",
    "cyanelle",
    "hydrogenosome",
    "kinetoplast",
    "leucoplast",
    "mitochondrion",
    "nucleomorph",
    "plastid",
    "proplastid",
    nullptr
};

/**********************************************************/
bool SeqLocHaveFuzz(const CSeq_loc& loc)
{
    bool flag;

    string loc_str;
    loc.GetLabel(&loc_str);

    if (loc_str.find('<') == string::npos && loc_str.find('>') == string::npos)
        flag = false;
    else
        flag = true;

    return (flag);
}

/**********************************************************
 *
 *   char* CpTheQualValue(qlist, qual):
 *
 *      Return qual's value if found the "qual" in the
 *   "qlist"; otherwise, return NULL.
 *
 **********************************************************/
string CpTheQualValue(const TQualVector& qlist, const Char* qual)
{
    for (const auto& cur : qlist) {
        if (cur->GetQual() != qual)
            continue;

        const string& val = cur->GetVal();
        if (val == "\"\"") {
            FtaErrPost(SEV_ERROR, ERR_FEATURE_UnknownQualSpelling, "Empty qual {} : {}", qual, val);
            break;
        }

        return NStr::Sanitize(val);
    }

    return {};
}

/**********************************************************
 *
 *   char* GetTheQualValue(qlist, qual):
 *
 *      Return qual's value if found the "qual" in the
 *   "qlist", and remove the "qual" from the qlist;
 *   otherwise, return NULL.
 *
 **********************************************************/
optional<string> GetTheQualValue(TQualVector& qlist, const Char* qual)
{
    optional<string> qvalue;

    for (TQualVector::iterator cur = qlist.begin(); cur != qlist.end(); ++cur) {
        if ((*cur)->GetQual() != qual)
            continue;

        const string& val = (*cur)->GetVal();
        if (val == "\"\"") {
            FtaErrPost(SEV_ERROR, ERR_FEATURE_UnknownQualSpelling, "Empty qual {} : {}", qual, val);
            break;
        }

        string s = tata_save(val);
        if (! s.empty())
            qvalue = s;

        qlist.erase(cur);
        break;
    }

    return qvalue;
}

/**********************************************************
 *
 *   bool DeleteQual(qlist, qual):
 *
 *      Return TRUE the "qual" has found in and removed
 *   from the "qlist".
 *
 **********************************************************/
bool DeleteQual(TQualVector& qlist, const Char* qual)
{
    bool got = false;
    for (TQualVector::iterator cur = qlist.begin(); cur != qlist.end();) {
        if ((*cur)->GetQual() != qual) {
            ++cur;
            continue;
        }

        cur = qlist.erase(cur);
        got = true;
    }

    return (got);
}

/**********************************************************
 *
 *   Uint1 GetQualValueAa(qual, checkseq):
 *
 *      Return 255 if not a valid amino acid, not in
 *   "ParFlat_AA_array".
 *
 **********************************************************/
Uint1 GetQualValueAa(const char* qval, bool checkseq)
{
    const char* str;
    const char* p;

    str = StringStr(qval, "aa:");
    if (! str)
        return (255);

    for (str += 3; *str == ' ';)
        str++;
    for (p = str; (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z');)
        p++;

    if (checkseq && ! StringStr(p, "seq:"))
        FtaErrPost(SEV_ERROR, ERR_QUALIFIER_AntiCodonLacksSequence, "Anticodon qualifier \"{}\" lacks a 'seq' field for the sequence of the anticodon.", qval);

    return CCleanup::ValidAminoAcid(string_view(str, p - str));
}

/**********************************************************/
bool GetGenomeInfo(CBioSource& bsp, string_view bptr)
{
    Int4 i = StringMatchIcase(ParFlat_GImod, bptr);
    if (i < 0)
        return false;

    if (i == 0)
        bsp.SetGenome(CBioSource::eGenome_mitochondrion);
    else if (i == 1)
        bsp.SetGenome(CBioSource::eGenome_chloroplast);
    else if (i == 2)
        bsp.SetGenome(CBioSource::eGenome_kinetoplast);
    else if (i == 3)
        bsp.SetGenome(CBioSource::eGenome_cyanelle);
    else if (i == 4)
        bsp.SetGenome(CBioSource::eGenome_chromoplast);
    else if (i == 5)
        bsp.SetGenome(CBioSource::eGenome_plastid);
    else if (i == 6)
        bsp.SetGenome(CBioSource::eGenome_macronuclear);
    else if (i == 7)
        bsp.SetGenome(CBioSource::eGenome_extrachrom);
    else if (i == 8)
        bsp.SetGenome(CBioSource::eGenome_plasmid);
    else
        bsp.SetGenome(CBioSource::eGenome_leucoplast);

    return true;
}

/**********************************************************/
static void GetTaxnameNameFromDescrs(const TSeqdescList& descrs, vector<string>& names)
{
    for (const auto& descr : descrs) {
        if (! descr->IsSource() || ! descr->GetSource().IsSetOrg() ||
            ! descr->GetSource().GetOrg().IsSetTaxname())
            continue;

        const COrg_ref& org_ref = descr->GetSource().GetOrg();
        names[0]                = org_ref.GetTaxname();

        if (org_ref.IsSetOrgname() && org_ref.GetOrgname().IsSetMod()) {
            for (const auto& mod : org_ref.GetOrgname().GetMod()) {
                if (! mod->IsSetSubname() || ! mod->IsSetSubtype())
                    continue;

                COrgMod::TSubtype stype = mod->GetSubtype();

                if (stype == COrgMod::eSubtype_old_name)
                    names[1] = mod->GetSubname();
                /* acronym(19), synonym(28), anamorph(29), teleomorph(30),
                gb-acronym(32), gb-anamorph(33), gb-synonym(34) */
                else if (stype == COrgMod::eSubtype_acronym || stype == COrgMod::eSubtype_synonym ||
                         stype == COrgMod::eSubtype_anamorph || stype == COrgMod::eSubtype_teleomorph ||
                         stype == COrgMod::eSubtype_gb_acronym || stype == COrgMod::eSubtype_gb_anamorph ||
                         stype == COrgMod::eSubtype_gb_synonym) {
                    names.push_back(mod->GetSubname());
                }
            }
        }

        if (descr->GetSource().IsSetSubtype()) {
            for (const auto& subtype : descr->GetSource().GetSubtype()) {
                /* subtype = "other" */
                if (! subtype->IsSetSubtype() || subtype->GetSubtype() != CSubSource::eSubtype_other || ! subtype->IsSetName())
                    continue;

                const Char* p = StringIStr(subtype->GetName().c_str(), "common:");
                if (! p)
                    continue;

                for (p += 7; *p == ' ';)
                    p++;
                if (*p == '\0')
                    continue;

                names.push_back(p);
            }
        }

        if (org_ref.IsSetCommon())
            names[2] = org_ref.GetCommon();

        break;
    }
}

/**********************************************************/
static void GetTaxnameName(TEntryList& seq_entries, vector<string>& names)
{
    names.resize(3);

    for (auto& entry : seq_entries) {
        for (CTypeIterator<CBioseq_set> bio_set(Begin(*entry)); bio_set; ++bio_set) {
            if (bio_set->IsSetDescr())
                GetTaxnameNameFromDescrs(bio_set->SetDescr().Set(), names);
        }

        for (CTypeIterator<CBioseq> bioseq(Begin(*entry)); bioseq; ++bioseq) {
            if (bioseq->IsSetDescr())
                GetTaxnameNameFromDescrs(bioseq->SetDescr().Set(), names);
        }
    }
}

/**********************************************************/
static void CheckDelGbblockSourceFromDescrs(TSeqdescList& descrs, const vector<string>& names)
{
    for (auto& descr : descrs) {
        if (! descr->IsGenbank())
            continue;

        if (! descr->GetGenbank().IsSetSource())
            break;

        CGB_block& gb_block = descr->SetGenbank();
        char*      p        = StringSave(gb_block.GetSource());
        char*      pper     = nullptr;

        ShrinkSpaces(p);

        size_t len = StringLen(p);
        if (p[len - 1] == '.') {
            pper       = StringSave(p);
            p[len - 1] = '\0';
        }

        char* q = StringChr(p, ' ');
        if (q)
            *q = '\0';

        if (StringMatchIcase(valid_organelle, p) >= 0) {
            if (q) {
                for (q++; *q == ' ';)
                    q++;
                fta_StringCpy(p, q);
            }
        } else if (q)
            *q = ' ';

        vector<string>::const_iterator name = names.begin();
        for (name += 2; name != names.end(); ++name) {
            if (name->empty())
                continue;

            len = name->size();
            for (q = p;; q++) {
                q = StringChr(q, '(');
                if (! q)
                    break;
                char* s = q + 1;
                if (StringEquN(s, "acronym:", 8) ||
                    StringEquN(s, "synonym:", 8))
                    s += 8;
                else if (StringEquN(s, "anamorph:", 9))
                    s += 9;
                else if (StringEquN(s, "teleomorph:", 11))
                    s += 11;
                if (*s == ' ')
                    while (*s == ' ')
                        s++;
                if (StringEquNI(s, name->c_str(), len) && s[len] == ')') {
                    char* t = nullptr;
                    for (t = s + len + 1; *t == ' ';)
                        t++;
                    if (*t != '\0')
                        fta_StringCpy(q, t);
                    else {
                        if (q > p)
                            q--;
                        *q = '\0';
                    }
                    break;
                }
            }
        }

        if (pper) {
            string s = p;
            s.append(".");
            MemFree(pper);
            pper = StringSave(s);
        }

        const string& first_name  = names[0];
        const string& second_name = names[1];

        if (NStr::CompareNocase(p, first_name.c_str()) == 0 || (pper && NStr::CompareNocase(pper, first_name.c_str()) == 0)) {
            gb_block.ResetSource();
        } else if (NStr::CompareNocase(p, second_name.c_str()) == 0 || (pper && NStr::CompareNocase(pper, second_name.c_str()) == 0)) {
            gb_block.ResetSource();
        }

        MemFree(p);
        if (pper)
            MemFree(pper);
        break;
    }
}

/**********************************************************/
static void CheckDelGbblockSource(TEntryList& seq_entries, const vector<string>& names)
{
    for (auto& entry : seq_entries) {
        for (CTypeIterator<CBioseq> bioseq(Begin(*entry)); bioseq; ++bioseq) {
            if (bioseq->IsSetDescr())
                CheckDelGbblockSourceFromDescrs(bioseq->SetDescr().Set(), names);
        }
    }
}

/**********************************************************/
void MaybeCutGbblockSource(TEntryList& seq_entries)
{
    vector<string> names; /* 0 - taxname */
                          /* 1 - 254 old-name */
                          /* 2 etc. - common name */

    GetTaxnameName(seq_entries, names);

    if (! names[0].empty())
        CheckDelGbblockSource(seq_entries, names);
}

/**********************************************************/
void MakeLocStrCompatible(string& str)
{
    const static Char STR_TO_REPLACE[] = "minus";

    // changing brackets is for backward compatibility
    if (! str.empty()) {
        if (str[0] == '[')
            str[0] = '(';

        size_t last = str.size() - 1;
        if (str[last] == ']')
            str[last] = ')';
    }

    // for backward compatibility with C-toolkit version
    size_t pos = str.find(STR_TO_REPLACE);
    while (pos != string::npos) {
        str.replace(pos, sizeof(STR_TO_REPLACE) - 1, "c");
        pos = str.find(STR_TO_REPLACE);
    }
}

/**********************************************************/
string location_to_string(const CSeq_loc& loc)
{
    string loc_str;
    loc.GetLabel(&loc_str);

    MakeLocStrCompatible(loc_str);
    return loc_str.substr(0, 50);
}

END_NCBI_SCOPE
