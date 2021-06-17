/* utilfeat.c
 *
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
 * File Name:  utilfeat.c
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

#include "ftaerr.hpp"
#include "asci_blk.h"
#include "add.h"
#include "utilfeat.h"
#include "utilfun.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "utilfeat.cpp"

// This is the forward declaration for ValidAminoAcid(...). The main declaration located in
// ../src/objtools/cleanup/cleanup_utils.hpp
// TODO: it should be removed after ValidAminoAcid(...) will be moved into
// any of public header file.
// for finding the correct amino acid letter given an abbreviation
BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
char ValidAminoAcid(const string &abbrev);
END_SCOPE(objects)


const char *ParFlat_GImod[] = {
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
    NULL
};

const char *valid_organelle[] = {
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
    NULL
};

/**********************************************************/
bool SeqLocHaveFuzz(const objects::CSeq_loc& loc)
{
    bool flag;

    std::string loc_str;
    loc.GetLabel(&loc_str);

    if (loc_str.find('<') == std::string::npos && loc_str.find('>') == std::string::npos)
        flag = false;
    else
        flag = true;

    return(flag);
}

/**********************************************************
 *
 *   char* CpTheQualValue(qlist, qual):
 *
 *      Return qual's value if found the "qual" in the
 *   "qlist"; otherwise, return NULL.
 *
 **********************************************************/
char* CpTheQualValue(const TQualVector& qlist, const Char *qual)
{
    std::string qvalue;
    ITERATE(TQualVector, cur, qlist)
    {
        if ((*cur)->GetQual() != qual)
            continue;

        const std::string& val = (*cur)->GetVal();
        if (val == "\"\"")
        {
            ErrPostEx(SEV_ERROR, ERR_FEATURE_UnknownQualSpelling,
                      "Empty qual %s : %s", qual, val.c_str());
            break;
        }

        qvalue = NStr::Sanitize(val);
        break;
    }

    char* ret = NULL;
    if (!qvalue.empty())
        ret = StringSave(qvalue.c_str());
    return ret;
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
char* GetTheQualValue(TQualVector& qlist, const Char *qual)
{
    char*   qvalue = NULL;

    NON_CONST_ITERATE(TQualVector, cur, qlist)
    {
        if ((*cur)->GetQual() != qual)
            continue;

        const std::string& val = (*cur)->GetVal();
        if (val == "\"\"")
        {
            ErrPostEx(SEV_ERROR, ERR_FEATURE_UnknownQualSpelling,
                      "Empty qual %s : %s", qual, val.c_str());
            break;
        }

        std::vector<Char> buf(val.begin(), val.end());
        buf.push_back(0);
        qvalue = tata_save(&buf[0]);

        qlist.erase(cur);
        break;
    }

    return(qvalue);
}

/**********************************************************
 *
 *   bool DeleteQual(qlist, qual):
 *
 *      Return TRUE the "qual" has found in and removed
 *   from the "qlist".
 *
 **********************************************************/
bool DeleteQual(TQualVector& qlist, const Char *qual)
{
    bool got = false;
    for (TQualVector::iterator cur = qlist.begin(); cur != qlist.end();)
    {
        if ((*cur)->GetQual() != qual)
        {
            ++cur;
            continue;
        }

        cur = qlist.erase(cur);
        got = true;
    }

    return(got);
}

/**********************************************************
 *
 *   Uint1 GetQualValueAa(qual, checkseq):
 *
 *      Return 255 if not a valid amino acid, not in
 *   "ParFlat_AA_array".
 *
 **********************************************************/
Uint1 GetQualValueAa(char* qval, bool checkseq)
{
    char* str;
    char* p;
    Uint1   aa;
    Char    ch;

    str = StringStr(qval, "aa:");
    if(str == NULL)
        return(255);

    for(str += 3; *str == ' ';)
        str++;
    for(p = str; (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z');)
        p++;

    if(checkseq && StringStr(p, "seq:") == NULL)
        ErrPostEx(SEV_ERROR, ERR_QUALIFIER_AntiCodonLacksSequence,
                  "Anticodon qualifier \"%s\" lacks a 'seq' field for the sequence of the anticodon.",
                  qval);
    ch = *p;
    *p = '\0';
    aa = objects::ValidAminoAcid(str);
    *p = ch;

    return(aa);
}

/**********************************************************/
bool GetGenomeInfo(objects::CBioSource& bsp, const Char* bptr)
{
    Int4 i = StringMatchIcase(ParFlat_GImod, bptr);
    if(i == -1)
        return false;

    if(i == 0)
        bsp.SetGenome(5);
    else if (i == 1)
        bsp.SetGenome(2);
    else if(i == 2)
        bsp.SetGenome(4);
    else if(i == 3)
        bsp.SetGenome(12);
    else if(i == 4)
        bsp.SetGenome(3);
    else if(i == 5)
        bsp.SetGenome(6);
    else if(i == 6)
        bsp.SetGenome(7);
    else if(i == 7)
        bsp.SetGenome(8);
    else if(i == 8)
        bsp.SetGenome(9);
    else
        bsp.SetGenome(17);

    return true;
}

/**********************************************************/
static void GetTaxnameNameFromDescrs(TSeqdescList& descrs, std::vector<std::string>& names)
{
    NON_CONST_ITERATE(TSeqdescList, descr, descrs)
    {
        if (!(*descr)->IsSource() || !(*descr)->GetSource().IsSetOrg() ||
            !(*descr)->GetSource().GetOrg().IsSetTaxname())
            continue;

        const objects::COrg_ref& org_ref = (*descr)->GetSource().GetOrg();
        names[0] = org_ref.GetTaxname();

        if (org_ref.IsSetOrgname() && org_ref.GetOrgname().IsSetMod())
        {
            ITERATE(objects::COrgName::TMod, mod, org_ref.GetOrgname().GetMod())
            {
                if (!(*mod)->IsSetSubname() || !(*mod)->IsSetSubtype())
                    continue;

                int stype = (*mod)->GetSubtype();

                if (stype == 254)        /* old-name */
                    names[1] = (*mod)->GetSubname();
                /* acronym(19), synonym(28), anamorph(29), teleomorph(30),
                gb-acronym(32), gb-anamorph(33), gb-synonym(34) */
                else if (stype == 19 || stype == 28 || stype == 29 ||
                         stype == 30 || stype == 32 || stype == 33 ||
                         stype == 34)
                {
                    names.push_back((*mod)->GetSubname());
                }
            }
        }

        if ((*descr)->GetSource().IsSetSubtype())
        {
            ITERATE(objects::CBioSource::TSubtype, subtype, (*descr)->GetSource().GetSubtype())
            {
                /* subtype = "other"
                */
                if (!(*subtype)->IsSetSubtype() || (*subtype)->GetSubtype() != 255 || !(*subtype)->IsSetName())
                    continue;

                const Char* p = StringIStr((*subtype)->GetName().c_str(), "common:");
                if (p == NULL)
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
static void GetTaxnameName(TEntryList& seq_entries, std::vector<std::string>& names)
{
    names.resize(3);

    NON_CONST_ITERATE(TEntryList, entry, seq_entries)
    {
        for (CTypeIterator<objects::CBioseq_set> bio_set(Begin(*(*entry))); bio_set; ++bio_set)
        {
            if (bio_set->IsSetDescr())
                GetTaxnameNameFromDescrs(bio_set->SetDescr().Set(), names);
        }

        for (CTypeIterator<objects::CBioseq> bioseq(Begin(*(*entry))); bioseq; ++bioseq)
        {
            if (bioseq->IsSetDescr())
                GetTaxnameNameFromDescrs(bioseq->SetDescr().Set(), names);
        }
    }
}

/**********************************************************/
static void CheckDelGbblockSourceFromDescrs(TSeqdescList& descrs, const std::vector<std::string>& names)
{
    NON_CONST_ITERATE(TSeqdescList, descr, descrs)
    {
        if (!(*descr)->IsGenbank())
            continue;

        if (!(*descr)->GetGenbank().IsSetSource())
            break;

        objects::CGB_block& gb_block = (*descr)->SetGenbank();
        char* p = StringSave(gb_block.GetSource().c_str());
        char* pper = 0;

        size_t len = StringLen(p);
        if (p[len - 1] == '.')
        {
            pper = StringSave(p);
            p[len - 1] = '\0';
        }

        char* q = StringChr(p, ' ');
        if (q != NULL)
            *q = '\0';

        if (StringMatchIcase(valid_organelle, p) > -1)
        {
            if (q != NULL)
            {
                for (q++; *q == ' ';)
                    q++;
                fta_StringCpy(p, q);
            }
        }
        else if (q != NULL)
            *q = ' ';

        std::vector<std::string>::const_iterator name = names.begin();
        for (name += 2; name != names.end(); ++name)
        {
            if (name->empty())
                continue;

            len = name->size();
            for (q = p;; q++)
            {
                q = StringChr(q, '(');
                if (q == NULL)
                    break;
                char* s = q + 1;
                if (StringNCmp(s, "acronym:", 8) == 0 ||
                    StringNCmp(s, "synonym:", 8) == 0)
                    s += 8;
                else if (StringNCmp(s, "anamorph:", 9) == 0)
                    s += 9;
                else if (StringNCmp(s, "teleomorph:", 11) == 0)
                    s += 11;
                if (*s == ' ')
                    while (*s == ' ')
                        s++;
                if (StringNICmp(s, name->c_str(), len) == 0 && s[len] == ')')
                {
                    char* t = NULL;
                    for (t = s + len + 1; *t == ' ';)
                        t++;
                    if (*t != '\0')
                        fta_StringCpy(q, t);
                    else
                    {
                        if (q > p)
                            q--;
                        *q = '\0';
                    }
                    break;
                }
            }
        }

        if (pper != NULL)
        {
            MemFree(pper);
            pper = (char*)MemNew(StringLen(p) + 2);
            StringCpy(pper, p);
            StringCat(pper, ".");
        }

        const std::string& first_name = names[0];
        const std::string& second_name = names[1];

        if (StringICmp(p, first_name.c_str()) == 0 || (pper != NULL && StringICmp(pper, first_name.c_str()) == 0))
        {
            gb_block.ResetSource();
        }
        else if (StringICmp(p, second_name.c_str()) == 0 || (pper != NULL && StringICmp(pper, second_name.c_str()) == 0))
        {
            gb_block.ResetSource();
        }

        MemFree(p);
        if (pper != NULL)
            MemFree(pper);
        break;
    }
}

/**********************************************************/
static void CheckDelGbblockSource(TEntryList& seq_entries, std::vector<std::string>& names)
{
    NON_CONST_ITERATE(TEntryList, entry, seq_entries)
    {
        for (CTypeIterator<objects::CBioseq> bioseq(Begin(*(*entry))); bioseq; ++bioseq)
        {
            if (bioseq->IsSetDescr())
                CheckDelGbblockSourceFromDescrs(bioseq->SetDescr().Set(), names);
        }
    }

}

/**********************************************************/
void MaybeCutGbblockSource(TEntryList& seq_entries)
{
    std::vector<std::string> names; /* 0 - taxname */
                                    /* 1 - 254 old-name */
                                    /* 2 etc. - common name */

    GetTaxnameName(seq_entries, names);

    if (!names[0].empty())
        CheckDelGbblockSource(seq_entries, names);
}

/**********************************************************/
void MakeLocStrCompatible(std::string& str)
{
    const static Char STR_TO_REPLACE[] = "minus";

    // changing brackets is for backward compatibility
    if (!str.empty())
    {
        if (str[0] == '[')
            str[0] = '(';

        size_t last = str.size() - 1;
        if (str[last] == ']')
            str[last] = ')';
    }

    // for backward compatibility with C-toolkit version
    size_t pos = str.find(STR_TO_REPLACE);
    while (pos != std::string::npos)
    {
        str.replace(pos, sizeof(STR_TO_REPLACE) - 1, "c");
        pos = str.find(STR_TO_REPLACE);
    }
}

/**********************************************************/
Char* location_to_string(const objects::CSeq_loc& loc)
{
    std::string loc_str;
    loc.GetLabel(&loc_str);

    MakeLocStrCompatible(loc_str);

    Char* ret = StringSave(loc_str.c_str());
    if (ret != NULL && StringLen(ret) > 50)
        ret[50] = '\0';

    return ret;
}

END_NCBI_SCOPE
