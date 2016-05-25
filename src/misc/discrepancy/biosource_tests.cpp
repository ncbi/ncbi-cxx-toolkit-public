/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Authors: Colleen Bollin, based on similar discrepancy tests
 *
 */

#include <ncbi_pch.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objmgr/seqdesc_ci.hpp>

#include "discrepancy_core.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(biosource_tests);


// MAP_CHROMOSOME_CONFLICT

const string kMapChromosomeConflict = "[n] source[s] on eukaryotic sequence[s] [has] map but not chromosome";


//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(MAP_CHROMOSOME_CONFLICT, CSeqdesc, eDisc | eOncaller, "Eukaryotic sequences with a map source qualifier should also have a chromosome source qualifier")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSource() || !context.IsEukaryotic() || !obj.GetSource().IsSetSubtype()) {
        return;
    }

    bool has_map = false;
    bool has_chromosome = false;

    ITERATE(CBioSource::TSubtype, it, obj.GetSource().GetSubtype()) {
        if ((*it)->IsSetSubtype()) {
            if ((*it)->GetSubtype() == CSubSource::eSubtype_map) {
                has_map = true;
            } else if ((*it)->GetSubtype() == CSubSource::eSubtype_chromosome) {
                has_chromosome = true;
                break;
            }
        } 
    }

    if (has_map && !has_chromosome) {
        m_Objs[kMapChromosomeConflict].Add(*context.NewDiscObj(CConstRef<CSeqdesc>(&obj)),
            false).Fatal();
    }

}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(MAP_CHROMOSOME_CONFLICT)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// INFLUENZA_DATE_MISMATCH
/* For Influenza A viruses, the year of the collection date should be
* the last number before the second set of parentheses in the tax name.
* For Influenza B viruses, the year should be the last token in the
* tax name (tokens are separated by / characters).
*/
static int GetYearFromInfluenzaA(const string& taxname)
{
    size_t pos = NStr::Find(taxname, "(");
    if (pos == string::npos) {
        return 0;
    }
    pos = NStr::Find(taxname, "(", pos + 1);
    if (pos == string::npos) {
        return 0;
    }
    pos--;
    while (pos > 0 && isspace(taxname.c_str()[pos])) {
        pos--;
    }
    if (pos < 4 ||
        !isdigit(taxname.c_str()[pos]) ||
        !isdigit(taxname.c_str()[pos - 1]) ||
        !isdigit(taxname.c_str()[pos - 2]) ||
        !isdigit(taxname.c_str()[pos - 3])) {
        return 0;
    }
    return NStr::StringToInt(taxname.substr(pos - 3, 4));
}


static int GetYearFromInfluenzaB(const string& taxname)
{
    size_t pos = NStr::Find(taxname, "/");
    if (pos == string::npos) {
        return 0;
    }
    ++pos;
    while (isspace(taxname.c_str()[pos])) {
        ++pos;
    }
    size_t len = 0;
    while (isdigit(taxname.c_str()[pos + len])) {
        len++;
    }
    if (len > 0) {
        return NStr::StringToInt(taxname.substr(pos, len));
    } else {
        return 0;
    }
}


static bool DoInfluenzaStrainAndCollectionDateMisMatch(const CBioSource& src)
{
    if (!src.IsSetOrg() || !src.GetOrg().IsSetTaxname()) {
        return false;
    }
    
    int year = 0;
    if (NStr::StartsWith(src.GetOrg().GetTaxname(), "Influenza A virus ")) {
        year = GetYearFromInfluenzaA(src.GetOrg().GetTaxname());
    } else if (NStr::StartsWith(src.GetOrg().GetTaxname(), "Influenza B virus ")) {
        year = GetYearFromInfluenzaB(src.GetOrg().GetTaxname());
    } else {
        // not influenza A or B, no mismatch can be calculated
        return false;
    }

    if (year > 0 && src.IsSetSubtype()) {
        ITERATE(CBioSource::TSubtype, it, src.GetSubtype()) {
            if ((*it)->IsSetSubtype() &&
                (*it)->GetSubtype() == CSubSource::eSubtype_collection_date &&
                (*it)->IsSetName()) {
                try {
                    CRef<CDate> date = CSubSource::DateFromCollectionDate((*it)->GetName());
                    if (date->IsStd() && date->GetStd().IsSetYear() && date->GetStd().GetYear() == year) {
                        // match found, no mismatch
                        return false;
                    } else {
                        return true;
                    }
                } catch (CException) {
                    //unable to parse date, assume mismatch
                    return true;
                }
            }
        }
    }
    return true;
}


const string kInfluenzaDateMismatch = "[n] influenza strain[s] conflict with collection date";
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(INFLUENZA_DATE_MISMATCH, CBioSource, eOncaller, "Influenza Strain/Collection Date Mismatch")
//  ----------------------------------------------------------------------------
{
    if (DoInfluenzaStrainAndCollectionDateMisMatch(obj)) {
        if (context.GetCurrentSeqdesc() != NULL) {
            m_Objs[kInfluenzaDateMismatch].Add(*context.NewDiscObj(context.GetCurrentSeqdesc()), false).Fatal();
        } else if (context.GetCurrentSeq_feat() != NULL) {
            m_Objs[kInfluenzaDateMismatch].Add(*context.NewDiscObj(context.GetCurrentSeq_feat()), false).Fatal();
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(INFLUENZA_DATE_MISMATCH)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
