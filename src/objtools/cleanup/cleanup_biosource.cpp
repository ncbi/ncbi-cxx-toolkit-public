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
 * Author:  Robert G. Smith
 *
 * File Description:
 *   implementation of BasicCleanup for CBioSource and sub-objects.
 *
 */

#include <ncbi_pch.hpp>
#include "cleanup_utils.hpp"
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/BinomialOrgName.hpp>
#include <objects/seqfeat/MultiOrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <corelib/ncbistr.hpp>
#include <set>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>

#include <objects/general/Dbtag.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/sequence.hpp>

#include "cleanupp.hpp"


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::



/****  BioSource cleanup ****/

void CCleanup_imp::BasicCleanup(CBioSource& bs)
{
    if (bs.IsSetOrg()) {
        BasicCleanup(bs.SetOrg());
        // convert COrg_reg.TMod string to SubSource objects
        x_OrgModToSubtype(bs);
    }
    if (bs.IsSetSubtype()) {
        x_SubtypeCleanup(bs);
    }
}


static bool s_NoNameSubtype(CSubSource::TSubtype val)
{
    if (val == CSubSource::eSubtype_germline    ||
        val == CSubSource::eSubtype_rearranged  ||
        val == CSubSource::eSubtype_transgenic  ||
        val == CSubSource::eSubtype_environmental_sample) {
        return true;
    }
    return false;
}

static CSubSource* s_StringToSubSource(const string& str)
{
    size_t pos = str.find('=');
    if (pos == NPOS) {
        pos = str.find(' ');
    }
    string subtype = str.substr(0, pos);
    try {
        CSubSource::TSubtype val = CSubSource::GetSubtypeValue(subtype);
        
        string name;
        if (pos != NPOS) {
            string name = str.substr(pos + 1);
        }
        NStr::TruncateSpacesInPlace(name);
        
        if (s_NoNameSubtype(val) ) {
            if (NStr::IsBlank(name)) {
                name = " ";
            }
        }
        
        if (NStr::IsBlank(name)) {
            return NULL;
        }
        
        size_t num_spaces = 0;
        bool has_comma = false;
        ITERATE (string, it, name) {
            if (isspace((unsigned char)(*it))) {
                ++num_spaces;
            } else if (*it == ',') {
                has_comma = true;
                break;
            }
        }
        
        if (num_spaces > 4  ||  has_comma) {
            return NULL;
        }
        return new CSubSource(val, name);
    } catch (CSerialException&) {}
    return NULL;
}


void CCleanup_imp::x_OrgModToSubtype(CBioSource& bs)
{
    _ASSERT(bs.IsSetOrg());
    
    if (!bs.SetOrg().IsSetMod()) {
        return;
    }
    CBioSource::TOrg::TMod& mod_list = bs.SetOrg().SetMod();
    
    CBioSource::TOrg::TMod::iterator it = mod_list.begin();
    while (it != mod_list.end()) {
        CRef<CSubSource> subsrc(s_StringToSubSource(*it));
        if (subsrc) {
            bs.SetSubtype().push_back(subsrc);
            it = mod_list.erase(it);
            ChangeMade(CCleanupChange::eChangeSubsource);
        } else {
            ++it;
        }
    }
}


// remove those with no name unless it has a subtype that doesn't need a name.
static bool s_SubSourceRemove(const CRef<CSubSource>& s1)
{
    return  ! s1->IsSetName()  &&  ! s_NoNameSubtype(s1->GetSubtype());
}


// is st1 < st2
static bool s_SubsourceCompare(const CRef<CSubSource>& st1,
                               const CRef<CSubSource>& st2)
{
    if (st1->GetSubtype() < st2->GetSubtype()) {
        return true;
    } else if (st1->GetSubtype() == st2->GetSubtype()) {
        if ( st1->IsSetName()  &&  st2->IsSetName()) {
            if (NStr::CompareNocase(st1->GetName(), st2->GetName()) < 0) {
                return true;
            }
        } else if ( ! st1->IsSetName()  &&  st2->IsSetName()) {
            return true;
        }
    }
    return false;
}


// Two SubSource's are equal and duplicates if:
// they have the same subtype
// and the same name (or don't require a name).
static bool s_SubsourceEqual(const CRef<CSubSource>& st1,
                             const CRef<CSubSource>& st2)
{
    if ( st1->GetSubtype() == st2->GetSubtype() ) {
        if ( s_NoNameSubtype(st2->GetSubtype())  ||  
             ( ! st1->IsSetName()  &&  ! st2->IsSetName() ) ||
             ( st1->IsSetName()  &&  st2->IsSetName()  &&
               NStr::CompareNocase(st1->GetName(), st2->GetName()) == 0) ) {
            return true;
        }            
    }
    return false;
}

/*
    Strip all parentheses and commas from the
    beginning and end of the string.
*/
void x_TrimParensAndCommas(string& str)
{
    SIZE_TYPE st = str.find_first_not_of("(),");
    if (st == NPOS){
        str.erase();
    } else if (st > 0) {
        str.erase(0, st);
    }
    
    SIZE_TYPE en = str.find_last_not_of(",()");
    if (en < str.size() - 1) {
        str.erase(en + 1);
    }
}


void x_CombinePrimerStrings(string& orig_seq, const string& new_seq)
{
    if (new_seq.empty()) {
        return;
    }
    if (orig_seq.empty()) {
        orig_seq = new_seq;
        return;
    }
    string new_seq_trim(new_seq);
    x_TrimParensAndCommas(new_seq_trim);
    if ( orig_seq.find(new_seq_trim) != NPOS ) {
        return;
    }
    x_TrimParensAndCommas(orig_seq);
    orig_seq = '(' + orig_seq + ',' + new_seq_trim + ')';
}


bool s_RemoveRedundantPlastidNameSubSourceQualifier (CBioSource& bs)
{
    bool rval = false;

    if (!bs.IsSetGenome()) {
        return false;
    }
    CBioSource::TGenome genome = bs.GetGenome();
    string plastid_name = "";
    switch (genome) {
        case CBioSource::eGenome_apicoplast:
            plastid_name = "apicoplast";
            break;
        case CBioSource::eGenome_chloroplast:
            plastid_name = "chloroplast";
            break;
        case CBioSource::eGenome_chromoplast:
            plastid_name = "chromoplast";
            break;
        case CBioSource::eGenome_kinetoplast:
            plastid_name = "kinetoplast";
            break;
        case CBioSource::eGenome_leucoplast:
            plastid_name = "leucoplast";
            break;
        case CBioSource::eGenome_plastid:
            plastid_name = "plastid";
            break;
        case CBioSource::eGenome_proplastid:
            plastid_name = "proplastid";
            break;
        default:
            return false;
            break;
    }

    CBioSource::TSubtype& subtypes = bs.SetSubtype();
    CBioSource::TSubtype::iterator it = subtypes.begin();
    while (it != subtypes.end()) {
        if (*it) {
            CSubSource& ss = **it;
            if ( ss.GetSubtype() == CSubSource::eSubtype_plastid_name
                 && NStr::Equal (ss.GetName(), plastid_name)) {
                it = subtypes.erase(it);
                rval = true;
            } else {
                ++it;
            }
        } else {
            ++it;
        }
    }
    return rval;
}


void CCleanup_imp::x_SubtypeCleanup(CBioSource& bs)
{
    _ASSERT(bs.IsSetSubtype());
    
    CBioSource::TSubtype& subtypes = bs.SetSubtype();
    
    NON_CONST_ITERATE (CBioSource::TSubtype, it, subtypes) {
        if (*it) {
            BasicCleanup(**it);
        }
    }
    
    // remove those with no name unless it has a subtype that doesn't need a name.
    size_t subtypes_cnt = subtypes.size();
    subtypes.remove_if(s_SubSourceRemove);
    if (subtypes_cnt != subtypes.size()) {
        ChangeMade(CCleanupChange::eCleanSubsource);
    }

    // remove plastid-name qual if the value is the same as the biosource location
    if (s_RemoveRedundantPlastidNameSubSourceQualifier (bs)) {
        ChangeMade(CCleanupChange::eCleanSubsource);
    }
    
    
    // merge any duplicate fwd_primer_seq and rev_primer_seq.
    // and any duplicate fwd_primer_name and rev_primer_name.
    // these are iterators pointing to the subtype into which we will merge others.
    CBioSource::TSubtype::iterator fwd_primer_seq = subtypes.end();
    CBioSource::TSubtype::iterator rev_primer_seq = subtypes.end();
    CBioSource::TSubtype::iterator fwd_primer_name = subtypes.end();
    CBioSource::TSubtype::iterator rev_primer_name = subtypes.end();
    CBioSource::TSubtype::iterator it = subtypes.begin();
    while (it != subtypes.end()) {
        if (*it) {
            CSubSource& ss = **it;
            if ( ss.GetSubtype() == CSubSource::eSubtype_fwd_primer_seq  || 
                 ss.GetSubtype() == CSubSource::eSubtype_rev_primer_seq ) {
                NStr::ToLower(ss.SetName());
                ss.SetName(NStr::Replace(ss.GetName(), " ", kEmptyStr));
                if (ss.GetSubtype() == CSubSource::eSubtype_fwd_primer_seq) {
                    if (fwd_primer_seq == subtypes.end() ) {
                        fwd_primer_seq = it;
                    } else {
                        x_CombinePrimerStrings((*fwd_primer_seq)->SetName(), ss.GetName());
                        ChangeMade(CCleanupChange::eChangeSubsource);
                        it = subtypes.erase(it);
                        continue;
                    }
                } else if (ss.GetSubtype() == CSubSource::eSubtype_rev_primer_seq) {
                    if (rev_primer_seq == subtypes.end() ) {
                        rev_primer_seq = it;
                    } else {
                        x_CombinePrimerStrings((*rev_primer_seq)->SetName(), ss.GetName());
                        ChangeMade(CCleanupChange::eChangeSubsource);
                        it = subtypes.erase(it);
                        continue;
                    }
                }
            } else if ( ss.GetSubtype() == CSubSource::eSubtype_fwd_primer_name  || 
                        ss.GetSubtype() == CSubSource::eSubtype_rev_primer_name ) {
                if (ss.GetSubtype() == CSubSource::eSubtype_fwd_primer_name) {
                    if (fwd_primer_name == subtypes.end() ) {
                        fwd_primer_name = it;
                    } else {
                        x_CombinePrimerStrings((*fwd_primer_name)->SetName(), ss.GetName());
                        ChangeMade(CCleanupChange::eChangeSubsource);
                        it = subtypes.erase(it);
                        continue;
                    }
                } else if (ss.GetSubtype() == CSubSource::eSubtype_rev_primer_name) {
                    if (rev_primer_name == subtypes.end() ) {
                        rev_primer_name = it;
                    } else {
                        x_CombinePrimerStrings((*rev_primer_name)->SetName(), ss.GetName());
                        ChangeMade(CCleanupChange::eChangeSubsource);
                        it = subtypes.erase(it);
                        continue;
                    }
                }
            }
        }
        ++it;
    }
    
    // sort and remove duplicates.
    // Do not sort before merging primer_seq's above.
    if (! objects::is_sorted(subtypes.begin(), subtypes.end(),
                             s_SubsourceCompare)) {
        ChangeMade(CCleanupChange::eCleanSubsource);
        subtypes.sort(s_SubsourceCompare);        
    }
    subtypes_cnt = subtypes.size();
    subtypes.unique(s_SubsourceEqual);
    if (subtypes_cnt != subtypes.size()) {
        ChangeMade(CCleanupChange::eCleanSubsource);
    }
}


void CCleanup_imp::BasicCleanup(COrg_ref& oref)
{
    CLEAN_STRING_MEMBER(oref, Taxname);
    CLEAN_STRING_MEMBER(oref, Common);
    CLEAN_STRING_LIST(oref, Mod);
    CLEAN_STRING_LIST(oref, Syn);
    
    if (oref.IsSetOrgname()) {
        if (oref.IsSetMod()) {
            x_ModToOrgMod(oref);
        }
        BasicCleanup(oref.SetOrgname());
    }
    
    if (oref.IsSetDb()) {
        COrg_ref::TDb& dbxref = oref.SetDb();
        
        // dbxrefs cleanup
        size_t dbxref_cnt = dbxref.size();
        COrg_ref::TDb::iterator it = dbxref.begin();
        while (it != dbxref.end()) {
            if (it->Empty()) {
                it = dbxref.erase(it);
                continue;
            }
            BasicCleanup(**it);
            
            ++it;
        }
        
        // sort/unique db_xrefs
        if ( ! objects::is_sorted(dbxref.begin(), dbxref.end(),
                                  SDbtagCompare())) {
            ChangeMade(CCleanupChange::eCleanDbxrefs);
            stable_sort(dbxref.begin(), dbxref.end(), SDbtagCompare());            
        }
        it = unique(dbxref.begin(), dbxref.end(), SDbtagEqual());
        dbxref.erase(it, dbxref.end());
        if (dbxref_cnt != dbxref.size()) {
            ChangeMade(CCleanupChange::eCleanDbxrefs);
        }
    }
    
}


static COrgMod* s_StringToOrgMod(const string& str)
{
    try {
        size_t pos = str.find('=');
        if (pos == NPOS) {
            pos = str.find(' ');
        }
        if (pos == NPOS) {
            return NULL;
        }
        
        string subtype = str.substr(0, pos);
        string subname = str.substr(pos + 1);
        NStr::TruncateSpacesInPlace(subname);
        
        
        size_t num_spaces = 0;
        bool has_comma = false;
        ITERATE (string, it, subname) {
            if (isspace((unsigned char)(*it))) {
                ++num_spaces;
            } else if (*it == ',') {
                has_comma = true;
                break;
            }
        }
        if (num_spaces > 4  ||  has_comma) {
            return NULL;
        }
        
        return new COrgMod(subtype, subname);
    } catch (CSerialException&) {}
    return NULL;
}


void CCleanup_imp::x_ModToOrgMod(COrg_ref& oref)
{
    _ASSERT(oref.IsSetMod()  &&  oref.IsSetOrgname());
    
    COrg_ref::TMod& mod_list = oref.SetMod();
    COrg_ref::TOrgname& orgname = oref.SetOrgname();
    
    COrg_ref::TMod::iterator it = mod_list.begin();
    while (it != mod_list.end()) {
        CRef<COrgMod> orgmod(s_StringToOrgMod(*it));
        if (orgmod) {
            orgname.SetMod().push_back(orgmod);
            it = mod_list.erase(it);
            ChangeMade(CCleanupChange::eChangeOrgmod);
        } else {
            ++it;
        }
    }   
}


// is om1 < om2
// sort by subname first because of how we check equality below.
static bool s_OrgModCompareNameFirst(const CRef<COrgMod>& om1,
                                     const CRef<COrgMod>& om2)
{
    if (NStr::CompareNocase(om1->GetSubname(), om2->GetSubname()) < 0) {
        return true;
    }
    if (NStr::CompareNocase(om1->GetSubname(), om2->GetSubname()) == 0  &&
        om1->GetSubtype() < om2->GetSubtype() ) {
        return true;
    }
    return false;
}


// Two OrgMod's are equal and duplicates if:
// they have the same subname and same subtype
// or one has subtype 'other'.
static bool s_OrgModEqual(const CRef<COrgMod>& om1, const CRef<COrgMod>& om2)
{
    if (NStr::CompareNocase(om1->GetSubname(), om2->GetSubname()) == 0  &&
        (om1->GetSubtype() == om2->GetSubtype() ||
         om2->GetSubtype() == COrgMod::eSubtype_other)) {
        return true;
    }
    return false;
}



// is om1 < om2
// to sort subtypes together.
static bool s_OrgModCompareSubtypeFirst(const CRef<COrgMod>& om1,
                                        const CRef<COrgMod>& om2)
{
    if (om1->GetSubtype() < om2->GetSubtype()) {
        return true;
    }
    if (om1->GetSubtype() == om2->GetSubtype()  &&
        NStr::CompareNocase(om1->GetSubname(), om2->GetSubname()) < 0 ) {
        return true;
    }
    return false;
}

void CCleanup_imp::BasicCleanup(COrgName& on)
{
    CLEAN_STRING_MEMBER(on, Attrib);
    CLEAN_STRING_MEMBER_JUNK(on, Lineage);
    CLEAN_STRING_MEMBER_JUNK(on, Div);
    if (on.IsSetMod()) {
        COrgName::TMod& mods = on.SetMod();
        size_t mods_cnt = mods.size();
        COrgName::TMod::iterator it = mods.begin();
        while (it != mods.end() ) {
            BasicCleanup(**it);
            if ((*it)->GetSubname().empty()) {
                it = mods.erase(it);
            } else {
                ++it;
            }
        }
        
        // if type of COrgName::TMod is changed from 'list' 
        // these will need to be changed.
        if ( ! objects::is_sorted(mods.begin(), mods.end(),
                                  s_OrgModCompareSubtypeFirst)) {
            ChangeMade(CCleanupChange::eCleanOrgmod);
        }
        mods.sort(s_OrgModCompareNameFirst);
        mods.unique(s_OrgModEqual);
        mods.sort(s_OrgModCompareSubtypeFirst);
        if (mods.size() != mods_cnt) {
            ChangeMade(CCleanupChange::eCleanOrgmod);
        }
    }
}


void CCleanup_imp::BasicCleanup(COrgMod& om)
{
    CLEAN_STRING_MEMBER(om, Subname);
    CLEAN_STRING_MEMBER(om, Attrib);
}


void CCleanup_imp::BasicCleanup(CSubSource& ss)
{
    CLEAN_STRING_MEMBER(ss, Name);
    CLEAN_STRING_MEMBER(ss, Attrib);
}


// ExtendedCleanup methods

bool CCleanup_imp::x_IsMergeableBioSource(const CSeqdesc& sd)
{
    if (sd.Which() == CSeqdesc::e_Source 
        && sd.GetSource().CanGetOrg()
        && sd.GetSource().GetOrg().CanGetTaxname()
        && !NStr::IsBlank(sd.GetSource().GetOrg().GetTaxname())) {
        return true;
    } else {
        return false;
    }
}


// Two BioSources are mergeable if the following conditions are met:
// 1)	For the following items, either the value for one or both BioSources 
// is not set or the values are identical:
// BioSource.genome
// BioSource.origin
// BioSource.is-focus
// BioSource.org.taxname
// BioSource.org.common
// BioSource.org.orgname.name
// BioSource.org.orgname.attrib
// BioSource.org.orgname.lineage
// BioSource.org.orgname.gcode
// BioSource.org.orgname.mgcode
// BioSource.org.orgname.div
// 2)	For the following items, either one or both BioSources have empty lists
// or the lists for the two BioSources are identical:
// BioSource.subtype
// BioSource.org.syn
// BioSource.org.orgname.mod
// BioSource.org.mod
// 3)	No Dbtag in the BioSource.org.db list from one BioSource can have the 
// same db value as an item in the BioSource.org.db list from the other BioSource 
// unless the two Dbtags have identical tag values.

#define MERGEABLE_STRING_VALUE(o1, o2, x) \
    if ((o1).CanGet##x() && (o2).CanGet##x() \
        && !NStr::IsBlank ((o1).Get##x()) \
        && !NStr::IsBlank ((o2).Get##x()) \
        && !NStr::Equal ((o1).Get##x(), (o2).Get##x())) { \
        return false; \
    }    

#define MERGEABLE_STRING_LIST(o1, o2, x) \
    if ((o1).CanGet##x() && (o2).CanGet##x() \
        && (o1).Get##x().size() > 0 \
        && (o2).Get##x().size() > 0) { \
        list <string>::const_iterator it1 = (o1).Get##x().begin(); \
        list <string>::const_iterator it2 = (o2).Get##x().begin(); \
        while (it1 != (o1).Get##x().end() && it2 != (o2).Get##x().end()) { \
            if (!NStr::Equal ((*it1), (*it2))) { \
                return false; \
            } \
            ++it1; ++it2; \
        } \
        if (it1 != (o1).Get##x().end() || it2 != (o2).Get##x().end()) { \
            return false; \
        } \
    } 
    
#define MERGEABLE_INT_VALUE(o1, o2, x) \
    (!(o1).CanGet##x() || !(o2).CanGet##x() \
     || (o1).Get##x() == 0 \
     || (o2).Get##x() == 0 \
     || (o1).Get##x() == (o2).Get##x())

#define MATCH_STRING_VALUE(o1, o2, x) \
    ((!(o1).CanGet##x() && !(o2).CanGet##x()) \
     || ((o1).CanGet##x() && (o2).CanGet##x() \
         && NStr::Equal((o1).Get##x(), (o2).Get##x())))

#define MATCH_STRING_LIST(o1, o2, x) \
    if (((o1).CanGet##x() && (o1).Get##x().size() > 0 && !(o2).CanGet##x()) \
        || (!(o1).CanGet##x() && (o2).CanGet##x() && (o2).Get##x().size() > 0)) { \
        return false; \
    } else if ((o1).CanGet##x() && (o2).CanGet##x()) { \
        list <string>::const_iterator it1 = (o1).Get##x().begin(); \
        list <string>::const_iterator it2 = (o2).Get##x().begin(); \
        while (it1 != (o1).Get##x().end() && it2 != (o2).Get##x().end()) { \
            if (!NStr::Equal ((*it1), (*it2))) { \
                return false; \
            } \
            ++it1; ++it2; \
        } \
        if (it1 != (o1).Get##x().end() || it2 != (o2).Get##x().end()) { \
            return false; \
        } \
    } 
    
#define MATCH_INT_VALUE(o1, o2, x) \
    ((!(o1).CanGet##x() && !(o2).CanGet##x()) \
     || ((o1).CanGet##x() && (o2).CanGet##x() \
         && (o1).Get##x() == (o2).Get##x()) \
     || ((o1).CanGet##x() && (o1).Get##x() == 0 \
         && !(o2).CanGet##x()) \
     || ((o2).CanGet##x() && (o2).Get##x() == 0 \
         && !(o1).CanGet##x()))         

bool Match (const CBinomialOrgName& n1, const CBinomialOrgName& n2)
{
  bool bMatch = true;
  
  bMatch &= MATCH_STRING_VALUE(n1, n2, Genus);
  bMatch &= MATCH_STRING_VALUE(n1, n2, Species);
  bMatch &= MATCH_STRING_VALUE(n1, n2, Subspecies);
  return bMatch;
}

bool Match (const COrgName& org1, const COrgName& org2);
bool Match (const COrgName::TName& n1, const COrgName::TName&n2);

bool Match (const CMultiOrgName& n1, const CMultiOrgName& n2)
{
    bool match = true;
    CMultiOrgName::Tdata::const_iterator it1 = n1.Get().begin();
    CMultiOrgName::Tdata::const_iterator it2 = n2.Get().begin();
    
    while (it1 != n1.Get().end() && it2 != n2.Get().end() && match) {
        match = Match (**it1, **it2);
        ++it1;
        ++it2;
    }
    if (it1 != n1.Get().end() || it2 != n2.Get().end() && match) {
        match = false;
    }
    
    return match;
}

bool Match (const COrgName::TMod& mod_list1,
            const COrgName::TMod& mod_list2);
            
bool Match (const CBioSource::TSubtype& mod_list1,
            const CBioSource::TSubtype& mod_list2);

bool Match (const COrgName& org1, const COrgName& org2)
{
    bool match = true;
    // check name
    if ((org1.CanGetName() && org2.CanGetName() 
         && !Match (org1.GetName(), org2.GetName()))
        || (org1.CanGetName() && !org2.CanGetName())
        || (!org1.CanGetName() && org2.CanGetName())) {
        return false;
    }
    // check attrib
    match &= MATCH_STRING_VALUE(org1, org2, Attrib);
    // check mod
    match &= Match (org1.GetMod(), org2.GetMod());

  
    // check lineage
    match &= MATCH_STRING_VALUE(org1, org2, Lineage);
    if (!match) return false;

    // check gcode
    match &= MATCH_INT_VALUE(org1, org2, Gcode);
    
    // check mgcode
    match &= MATCH_INT_VALUE(org1, org2, Mgcode);
  
    // check div
    match &= MATCH_STRING_VALUE(org1, org2, Div);
    
    return match;
}


bool Match (const COrgName::TName& n1, const COrgName::TName&n2)
{
    bool match = true;
    if (n1.Which() != n2.Which()) {
        return false;
    }
    
    switch (n1.Which()) {
        case COrgName::TName::e_not_set:
            break;
        case COrgName::TName::e_Binomial:
            match = Match (n1.GetBinomial(), n2.GetBinomial());
            break;
        case COrgName::TName::e_Virus:
            match = NStr::Equal (n1.GetVirus(), n2.GetVirus());
            break;
        case COrgName::TName::e_Hybrid:
            match = Match (n1.GetHybrid(), n2.GetHybrid());
            break;
        case COrgName::TName::e_Namedhybrid:
            match = Match (n1.GetNamedhybrid(), n2.GetNamedhybrid());
            break;
        case COrgName::TName::e_Partial:
            break;        
    }
    return match;
}

bool CCleanup_imp::x_OkToMerge (const COrgName& on1, const COrgName& on2)
{
    // test name
    if (on1.CanGetName() && on2.CanGetName()
        && !Match (on1.GetName(), on2.GetName())) {
        return false;
    }
    
    // test attrib
    MERGEABLE_STRING_VALUE(on1, on2, Attrib);
    // test lineage
    MERGEABLE_STRING_VALUE(on1, on2, Lineage);
    // test gcode
    if (!MERGEABLE_INT_VALUE(on1, on2, Gcode)) return false;
    // test mgcode
    if (!MERGEABLE_INT_VALUE(on1, on2, Mgcode)) return false;
    
    // test div
    MERGEABLE_STRING_VALUE(on1, on2, Div);
    
    if (!on1.IsSetMod() || !on2.IsSetMod() || Match (on1.GetMod(), on2.GetMod())) {
        return true;
    } else {
        return false;
    }
}


bool CCleanup_imp::x_OkToMerge (const COrg_ref::TDb& db1, const COrg_ref::TDb& db2)
{
    if (db1.size() == 0 || db2.size() == 0) return true;
    
    for (unsigned int i1 = 0; i1 < db1.size(); i1++) {
        if (!db1[i1]->IsSetDb()) continue;
        string db1_db = db1[i1]->GetDb();
        if (NStr::IsBlank (db1_db)) continue;
        
        for (unsigned int i2 = 0; i2 < db1.size(); i2++) {
            if (NStr::Equal (db1_db, db2[i2]->GetDb())
                && ! db1[i1]->Match (*(db2[i2]))) {
                return false;
            }
        }
    }
    return true;           
}

        
bool CCleanup_imp::x_OkToMerge(const COrg_ref& org1, const COrg_ref& org2)
{
    // test taxnames
    MERGEABLE_STRING_VALUE (org1, org2, Taxname);
    
    // test common names
    MERGEABLE_STRING_VALUE (org1, org2, Common);

    // test synonyms
    MERGEABLE_STRING_LIST (org1, org2, Syn);
    // test mod
    MERGEABLE_STRING_LIST (org1, org2, Mod);
    
    // test orgname
    if (!x_OkToMerge(org1.GetOrgname(), org2.GetOrgname())) {
        return false;
    }
    
    // test db
    if (org1.IsSetDb() && org2.IsSetDb()
        && !x_OkToMerge (org1.GetDb(), org2.GetDb())) {
        return false;
    }
    
    return true;    
}


bool CCleanup_imp::x_OkToMerge(const CBioSource& src1, const CBioSource& src2)
{
    // check genome
    if (!MERGEABLE_INT_VALUE(src1, src2, Genome)) return false;
    // check origin
    if (!MERGEABLE_INT_VALUE(src1, src2, Origin)) return false;
    
    // Check Subsource modifiers
    if (src1.CanGetSubtype() && src2.CanGetSubtype()
        && src1.GetSubtype().size() > 0
        && src2.GetSubtype().size() > 0
        && !Match (src1.GetSubtype(), src2.GetSubtype())) {
        return false;
    }
     
    // check Org-ref
    if (src1.CanGetOrg() && src2.CanGetOrg()
        && !x_OkToMerge (src1.GetOrg(), src2.GetOrg())) {
        return false;
    }
    bool taxname_match = false;
    if (!src1.CanGetOrg() && !src2.CanGetOrg()) {
        return false;
    }
    
    if (src1.CanGetIs_focus() && src2.CanGetIs_focus()
        && ((src1.IsSetIs_focus() && !src2.IsSetIs_focus())
            || (!src1.IsSetIs_focus() && src2.IsSetIs_focus()))) {
        return false;
    }
    return true;
}


bool IsTransgenic(const CBioSource& bsrc)
{
    if (bsrc.CanGetSubtype()) {
        ITERATE (CBioSource::TSubtype, sub, bsrc.GetSubtype()) {
            if ((*sub)->GetSubtype() == CSubSource::eSubtype_transgenic) {
                return true;
            }
        }
    }
    return false;
}


bool CCleanup_imp::x_OkToConvertToDescriptor (CBioseq_Handle bh, const CBioSource& src)
{
    ITERATE (CSeq_descr::Tdata, desc_it, bh.GetDescr().Get()) {
        if ((*desc_it)->Which() == CSeqdesc::e_Source) {
            if (IsTransgenic ((*desc_it)->GetSource())
                || !x_OkToMerge((*desc_it)->GetSource(), src)) {
                return false;
            }
        }
    }
    return true;
}


#define MERGE_INT_VALUE(o1, o2, x, changed) \
    if ((!(o1).CanGet##x() || (o1).Get##x() == 0) \
        && (o2).CanGet##x() && (o2).Get##x() != 0) { \
        (o1).Set##x((o2).Get##x()); \
        ChangeMade (CCleanupChange::eChangeBioSourceOther); \
        changed = true; \
    }

#define MERGE_STRING_VALUE(o1, o2, x, changed) \
    if ((!(o1).CanGet##x() || NStr::IsBlank((o1).Get##x())) \
        && (o2).CanGet##x() && !NStr::IsBlank((o2).Get##x())) { \
        (o1).Set##x((o2).Get##x()); \
        ChangeMade (CCleanupChange::eChangeBioSourceOther); \
        changed = true; \
    }



bool CCleanup_imp::x_Merge (COrgName& on, const COrgName& add_on)
{
    bool changed = false;
    // mod
    if ((!on.CanGetMod() || on.GetMod().empty())
        && add_on.CanGetMod() && !add_on.GetMod().empty()) {
        ITERATE (COrgName::TMod, it, add_on.GetMod()) {
            on.SetMod().push_back (*it);
        }
        ChangeMade (CCleanupChange::eAddOrgMod);
        changed = true;
    }
            
    // merge gcode
    MERGE_INT_VALUE (on, add_on, Gcode, changed);
    // merge mgcode
    MERGE_INT_VALUE (on, add_on, Mgcode, changed);
    // merge lineage
    MERGE_STRING_VALUE (on, add_on, Lineage, changed);
    // merge div
    MERGE_STRING_VALUE (on, add_on, Div, changed);
    return changed;
}


bool CCleanup_imp::x_Merge (COrg_ref::TDb& db1, const COrg_ref::TDb& db2)
{
    bool changed = false;
    
    ITERATE (COrg_ref::TDb, it2, db2) {
        bool found = false;
        ITERATE (COrg_ref::TDb, it1, db1) {
            if ((*it2)->Match(**it1)) {
                found = true;
            }
        }
        if (!found) {
            db1.push_back (*it2);
            ChangeMade (CCleanupChange::eChangeBioSourceOther);
            changed = true;
        }
    }
    return changed;
}


bool CCleanup_imp::x_Merge (COrg_ref& org, const COrg_ref& add_org)
{
    bool changed = false;
    
    // syn
    if ((!org.CanGetSyn() || org.GetSyn().empty())
        && add_org.CanGetSyn() && !add_org.GetSyn().empty()) {
        ITERATE (COrg_ref::TSyn, it, add_org.GetSyn()) {
            org.SetSyn().push_back(*it);
            ChangeMade (CCleanupChange::eChangeBioSourceOther);
            changed = true;
        }
    }
    
    // mod
    if ((!org.CanGetMod() || org.GetMod().empty())
        && add_org.CanGetMod() && !add_org.GetMod().empty()) {
        ITERATE (COrg_ref::TMod, it, add_org.GetMod()) {
            org.SetMod().push_back(*it);
            ChangeMade (CCleanupChange::eChangeBioSourceOther);
            changed = true;
        }
    }
        
    // merge db
    // note - don't need to check for empty db list and reset after this,
    // because either the db list was empty before and isn't now because
    // we added to it, or else it wasn't empty before and won't be empty after
    if (add_org.CanGetDb() && !add_org.GetDb().empty()) {  
        changed |= x_Merge (org.SetDb(), add_org.GetDb());
    }
    
    // taxname
    MERGE_STRING_VALUE (org, add_org, Taxname, changed);
        
    // common
    MERGE_STRING_VALUE (org, add_org, Common, changed);
    
    // orgname
    changed |= x_Merge (org.SetOrgname(), add_org.GetOrgname());
    
    return changed;
}

// combine BioSource descriptors
// Was MergeDupBioSources in C Toolkit
// Lists
// For the following items, if the list in the first BioSource is empty, the list from the second BioSource will be copied:
// BioSource.subtype
// BioSource.org.syn
// BioSource.org.orgname.mod
// BioSource.org.mod
// Any Dbtag found in BioSource.org.db  for the second BioSource that is missing in the first will be added to the first.
// 
// Individual Values
// For the following values, if the value is not set in the first BioSource, the value for the second BioSource will be used:
// BioSource.genome
// BioSource.origin
// BioSource.is-focus
// BioSource.org.common
// BioSource.org.orgname.name
// BioSource.org.orgname.attrib
// BioSource.org.orgname.lineage
// BioSource.org.orgname.gcode
// BioSource.org.orgname.mgcode
// BioSource.org.orgname.div
//
bool CCleanup_imp::x_Merge (CBioSource& src, const CBioSource& add_src)
{
    bool changed = false;
    
    // BioSource.subtype
    if ((!src.CanGetSubtype() || src.GetSubtype().empty())
         && add_src.CanGetSubtype() && !add_src.GetSubtype().empty()) {
        ITERATE (CBioSource::TSubtype, it, add_src.GetSubtype()) {
            src.SetSubtype().push_back(*it);
        }
        ChangeMade (CCleanupChange::eAddSubSource);
        changed = true;
    }
    
    // genome
    if ((!src.CanGetGenome() || src.GetGenome() == CBioSource::eGenome_unknown)
        && add_src.CanGetGenome()
        && add_src.GetGenome() != CBioSource::eGenome_unknown) {
        src.SetGenome(add_src.GetGenome());
        ChangeMade (CCleanupChange::eChangeBioSourceGenome);
        changed = true;
    }

    // origin
    if ((!src.CanGetOrigin() || src.GetOrigin() == CBioSource::eOrigin_unknown)
        && add_src.CanGetOrigin()
        && add_src.GetOrigin() != CBioSource::eOrigin_unknown) {
        src.SetOrigin(add_src.GetOrigin());
        ChangeMade (CCleanupChange::eChangeBioSourceOrigin);
        changed = true;
    }
    
    // is-focus
    if ((!src.CanGetIs_focus() || !src.IsSetIs_focus())
        && add_src.CanGetIs_focus()
        && add_src.IsSetIs_focus()) {
        src.SetIs_focus();
        ChangeMade (CCleanupChange::eChangeBioSourceOther);
        changed = true;
    }

    
    if (add_src.CanGetOrg()) {
        changed |= x_Merge (src.SetOrg(), add_src.GetOrg());
    }
    return changed;
}


bool CCleanup_imp::x_MergeDuplicateBioSources(CSeqdesc& sd1, CSeqdesc& sd2)
{
    if (x_IsMergeableBioSource(sd1) && x_IsMergeableBioSource(sd2)
        && x_OkToMerge(sd1.GetSource(),
                       sd2.GetSource())) {
        // add information from sd1 to sd2     
        x_Merge (sd1.SetSource(), sd2.GetSource());    
        return true;
    } else {
        return false;
    }                               
}


void CCleanup_imp::x_CleanOrgNameStrings(COrgName &on)
{
    CLEAN_STRING_MEMBER(on, Attrib);
    CLEAN_STRING_MEMBER_JUNK(on, Lineage);
    CLEAN_STRING_MEMBER_JUNK(on, Div);
    if (on.IsSetMod()) {
        NON_CONST_ITERATE(COrgName::TMod, modI, on.SetMod()) {
            CleanString ((*modI)->SetSubname());
        }
    }
}


void CCleanup_imp::x_ExtendedCleanSubSourceList (CBioSource &bs)
{
    if (bs.IsSetSubtype()) {
        CBioSource::TSubtype& subtypes = bs.SetSubtype();
        CBioSource::TSubtype tmp;
        tmp.clear();
        NON_CONST_ITERATE (CBioSource::TSubtype, it, subtypes) {
            if ((*it)->CanGetAttrib()) {
                CleanString((*it)->SetAttrib());
            }
            int subtype = (*it)->GetSubtype();
            if (subtype != CSubSource::eSubtype_germline
                && subtype != CSubSource::eSubtype_rearranged
                && subtype != CSubSource::eSubtype_transgenic
                && subtype != CSubSource::eSubtype_environmental_sample) {
                CleanString((*it)->SetName());
                if (!NStr::IsBlank((*it)->GetName())) {
                    tmp.push_back(*it);
                }
            } else {
                tmp.push_back(*it);
            }            
        }
        if (subtypes.size() > tmp.size()) {
            subtypes.clear();
            NON_CONST_ITERATE (CBioSource::TSubtype, it, tmp) {
                subtypes.push_back(*it);
            }            
        }
    }    
}


void CCleanup_imp::x_SetSourceLineage(const CSeq_entry& se, string lineage)
{
    if (se.Which() == CSeq_entry::e_Seq) {
        x_SetSourceLineage(m_Scope->GetBioseqHandle(se.GetSeq()), lineage);
    } else if (se.Which() == CSeq_entry::e_Set) {
        x_SetSourceLineage(m_Scope->GetBioseq_setHandle(se.GetSet()), lineage);
    }
}


void CCleanup_imp::x_SetSourceLineage(CSeq_entry_Handle seh, string lineage)
{
    x_SetSourceLineage(*(seh.GetCompleteSeq_entry()), lineage);
}


void CCleanup_imp::x_SetSourceLineage(CBioseq_Handle bh, string lineage)
{
    if (!bh.CanGetInst_Repr()
        || (bh.GetInst_Repr() != CSeq_inst::eRepr_raw
            && bh.GetInst_Repr() != CSeq_inst::eRepr_const)) {
        return;
    }
    CBioseq_EditHandle eh = bh.GetEditHandle();
    if (eh.IsSetDescr()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, eh.SetDescr().Set()) {
            if ((*desc_it)->Which() == CSeqdesc::e_Source
                && (*desc_it)->GetSource().CanGetOrg()) {
                (*desc_it)->SetOrg().SetOrgname().SetLineage (lineage);
            }
        }
    }
}


void CCleanup_imp::x_SetSourceLineage(CBioseq_set_Handle bh, string lineage)
{
    CBioseq_set_EditHandle eh = bh.GetEditHandle();
    if (eh.IsSetDescr()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, eh.SetDescr().Set()) {
            if ((*desc_it)->Which() == CSeqdesc::e_Source
                && (*desc_it)->GetSource().CanGetOrg()) {
                (*desc_it)->SetOrg().SetOrgname().SetLineage (lineage);
            }
        }
    }
    
    ITERATE (list< CRef< CSeq_entry > >, it, bh.GetCompleteBioseq_set()->GetSeq_set()) {
        x_SetSourceLineage(m_Scope->GetSeq_entryHandle(**it), lineage);
    }    
}


void CCleanup_imp::x_ConvertOrgDescToSourceDescriptor(CBioseq_set_Handle bh)
{
    CBioseq_set_EditHandle eh = bh.GetEditHandle();

    if (eh.IsSetDescr()) {
        CSeq_descr::Tdata remove_list;
        NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, eh.SetDescr().Set()) {
            if ((*desc_it)->Which() == CSeqdesc::e_Org) {
                remove_list.push_back(*desc_it);
            }
        }
        for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
            it1 != remove_list.end(); ++it1) { 
            CRef<CSeqdesc> desc(new CSeqdesc);
            desc->SetSource().SetOrg((*it1)->SetOrg());
            eh.AddSeqdesc(*desc);
            ChangeMade(CCleanupChange::eAddDescriptor);
            eh.RemoveSeqdesc(**it1);
            ChangeMade(CCleanupChange::eRemoveDescriptor);
        }        
    }
}


void CCleanup_imp::x_ConvertOrgDescToSourceDescriptor(CBioseq_Handle bh)
{
    CBioseq_EditHandle eh = bh.GetEditHandle();

    if (eh.IsSetDescr()) {
        CSeq_descr::Tdata remove_list;
        NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, eh.SetDescr().Set()) {
            if ((*desc_it)->Which() == CSeqdesc::e_Org) {
                remove_list.push_back(*desc_it);
            }
        }
        for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
            it1 != remove_list.end(); ++it1) { 
            CRef<CSeqdesc> desc(new CSeqdesc);
            desc->SetSource().SetOrg((*it1)->SetOrg());
            eh.AddSeqdesc(*desc);
            ChangeMade(CCleanupChange::eAddDescriptor);
            eh.RemoveSeqdesc(**it1);
            ChangeMade(CCleanupChange::eRemoveDescriptor);
        }        
    }
}


// The BioSource descriptor on a nuc-prot set will be compared to the BioSource descriptor
// on the nucleotide sequence or the nucleotide segmented set (not descriptors inside the
// segmented set) and to the BioSource descriptors on protein sequences in the nuc-prot set.
// 1) If no BioSource descriptor exists on the nuc-prot set, the first BioSource descriptor 
// found on the nucleotide sequence or nucleotide segmented set or one of the proteins will 
// be promoted to the nuc-prot set.
// 2) Each BioSource descriptor on the nuc-prot set or nucleotide segmented set that is mergeable
// with the BioSource descriptor on the nuc-prot set will be merged with the BioSource descriptor
// on the nuc-prot set and removed.  

void CCleanup_imp::x_FixNucProtSources (CBioseq_set_Handle bh)
{
    if (!bh.CanGetClass()
        || bh.GetClass() != CBioseq_set::eClass_nuc_prot
        || !bh.GetCompleteBioseq_set()->IsSetSeq_set()) {
        return;
    }
    
    CConstRef<CBioseq_set> b = bh.GetCompleteBioseq_set();
    list< CRef< CSeq_entry > > set = (*b).GetSeq_set();
    
    // first, if the nucleotide is a segmented set, fix sources on the segmented set
    if ((*(set.begin()))->Which() == CSeq_entry::e_Set) {
        x_FixSegSetSource (m_Scope->GetBioseq_setHandle((*(set.begin()))->GetSet()),
                           &bh);
    }
    
    // Promote and remove mergeable source descriptors on the nucleotide set/sequence
    // and protein sequences        
    ITERATE (list< CRef< CSeq_entry > >, it, set) {
        if ((*it)->Which() == CSeq_entry::e_Set) {
            CBioseq_set_EditHandle eh = (m_Scope->GetBioseq_setHandle((*it)->GetSet())).GetEditHandle();
            if (eh.GetDescr().Get().empty()) continue;
            
            CSeq_descr::Tdata remove_list;
            remove_list.clear();
            
            NON_CONST_ITERATE (CSeq_descr::Tdata, itd, eh.SetDescr().Set()) {
                if ((*itd)->Which() == CSeqdesc::e_Source
                    && x_PromoteMergeableSource (bh, (*itd)->GetSource())) { 
                    remove_list.push_back (*itd);                   
                }                
            }
            for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
                it1 != remove_list.end(); ++it1) { 
                eh.RemoveSeqdesc(**it1);
                ChangeMade(CCleanupChange::eRemoveDescriptor);
            }        

            if (eh.SetDescr().Set().size() == 0) {
                eh.ResetDescr();
            }
        } else if ((*it)->Which() == CSeq_entry::e_Seq) {
            CBioseq_EditHandle eh = (m_Scope->GetBioseqHandle((*it)->GetSeq())).GetEditHandle();
            if (eh.GetDescr().Get().empty()) continue;

            CSeq_descr::Tdata remove_list;
            remove_list.clear();
            
            NON_CONST_ITERATE (CSeq_descr::Tdata, itd, eh.SetDescr().Set()) {
                if ((*itd)->Which() == CSeqdesc::e_Source
                    && x_PromoteMergeableSource (bh, (*itd)->GetSource())) { 
                    remove_list.push_back (*itd);                   
                }                
            }
            for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
                it1 != remove_list.end(); ++it1) { 
                eh.RemoveSeqdesc(**it1);
                ChangeMade(CCleanupChange::eRemoveDescriptor);
            }        

            if (eh.SetDescr().Set().size() == 0) {
                eh.ResetDescr();
            }
        }
    }
}


bool Match (const CBioSource::TSubtype& mod_list1,
            const CBioSource::TSubtype& mod_list2)
{                                              
    CBioSource::TSubtype::const_iterator it1 = mod_list1.begin();
    CBioSource::TSubtype::const_iterator it2 = mod_list2.begin();
    while (it1 != mod_list1.end() && it2 != mod_list2.end()) {
        if ((*it1)->GetSubtype() != (*it2)->GetSubtype()) {
            return false;
        } else if (!NStr::Equal((*it1)->GetName(), (*it2)->GetName())) {
            return false;
        }
        ++it1;
        ++it2;
    }
    if (it1 != mod_list1.end() || it2 != mod_list2.end()) {
        return false;
    } else {
        return true;
    }
}


bool Match (const COrgName::TMod& mod_list1,
            const COrgName::TMod& mod_list2)
{                                              
    COrgName::TMod::const_iterator it1 = mod_list1.begin();
    COrgName::TMod::const_iterator it2 = mod_list2.begin();
    while (it1 != mod_list1.end() && it2 != mod_list2.end()) {
        if ((*it1)->GetSubtype() != (*it2)->GetSubtype()) {
            return false;
        } else if (!NStr::Equal((*it1)->GetSubname(), (*it2)->GetSubname())) {
            return false;
        }
        ++it1;
        ++it2;
    }
    if (it1 != mod_list1.end() || it2 != mod_list2.end()) {
        return false;
    } else {
        return true;
    }
}


// Two BioSources are identical if all of the following criteria are met:
// 1)	Both have non-blank BioSource.org.taxname values that are identical.
// 2)	For the following items, either the value for both BioSources is not set or the values are identical:
// BioSource.genome
// BioSource.origin
// BioSource.is-focus
// BioSource.org.common
// BioSource.org.orgname.name
// BioSource.org.orgname.attrib
// BioSource.org.orgname.lineage
// BioSource.org.orgname.gcode
// BioSource.org.orgname.mgcode
// BioSource.org.orgname.div
// 3)	For the following items, either both BioSources have empty lists or both have identical lists (both in order and content):
// BioSource.subtype
// BioSource.org.syn
// BioSource.org.orgname.mod
// BioSource.org.mod
// BioSource.org.db
//

bool CCleanup_imp::x_Identical (const COrg_ref::TDb& db1, const COrg_ref::TDb& db2)
{
    if (db1.size() != db2.size()) return false;
    
    COrg_ref::TDb::const_iterator it1 = db1.begin();
    COrg_ref::TDb::const_iterator it2 = db2.begin();
    
    while (it1 != db1.end() && it2 != db2.end()) {
        if (!(*it1)->Match (**it2)) {
            return false;
        }
        ++it1;
        ++it2;
    }
    if (it1 != db1.end() || it2 != db2.end()) {
        return false;
    }
    return true;           
}

        
bool CCleanup_imp::x_Identical(const COrg_ref& org1, const COrg_ref& org2)
{
    if (!(MATCH_STRING_VALUE(org1, org2, Common))) {
        return false;
    }
    
    MATCH_STRING_LIST(org1, org2, Syn);
    
    MATCH_STRING_LIST(org1, org2, Mod);
    
    if ((org1.CanGetDb() && org1.GetDb().size() > 0 && !org1.CanGetDb())
        || (!org1.CanGetDb() && org2.CanGetDb() && org2.GetDb().size() > 0)
        || (org1.CanGetDb() && org2.CanGetDb() && !x_Identical (org1.GetDb(), org2.GetDb()))) {
        return false;
    }
    
    if ((org1.CanGetOrgname() && !org2.CanGetOrgname())
        || (!org1.CanGetOrgname() && org2.CanGetOrgname())
        || !Match (org1.GetOrgname(), org2.GetOrgname())) {
        return false;
    }
    return true;
}


bool CCleanup_imp::x_Identical(const CBioSource& src1, const CBioSource& src2)
{
    bool identical = true;
    
    if (!src1.CanGetOrg()
        || !src2.CanGetOrg()
        || !src1.GetOrg().CanGetTaxname()
        || !src2.GetOrg().CanGetTaxname()
        || NStr::IsBlank (src1.GetOrg().GetTaxname())
        || NStr::IsBlank (src2.GetOrg().GetTaxname())
        || !NStr::Equal (src1.GetOrg().GetTaxname(), src2.GetOrg().GetTaxname())) {
        return false;
    }

    if (!(MATCH_INT_VALUE(src1, src2, Genome))) {
        return false;
    }
    
    if (!(MATCH_INT_VALUE(src1, src2, Origin))) {
        return false;
    }
    
    if ((src1.CanGetIs_focus() && !src2.CanGetIs_focus())
        || (!src1.CanGetIs_focus() && src2.CanGetIs_focus())
        || (src1.CanGetIs_focus() && src2.CanGetIs_focus()
            && ((src1.IsSetIs_focus() && ! src2.IsSetIs_focus())
                || (!src1.IsSetIs_focus() && src2.IsSetIs_focus())))) {
        return false;
    }
    
    // Check Subsource modifiers
    if ((src1.CanGetSubtype() && src1.GetSubtype().size() > 0 && !src2.CanGetSubtype())
        || (!src1.CanGetSubtype() && src2.CanGetSubtype() && src2.GetSubtype().size() > 0)
        || (src1.CanGetSubtype() && src2.CanGetSubtype()
            && !Match (src1.GetSubtype(), src2.GetSubtype()))) {
        return false;
    }

    return x_Identical (src1.GetOrg(), src2.GetOrg());
    
}


bool CCleanup_imp::x_IsBioSourceEmpty (const CBioSource& src)
{
    if (!src.IsSetOrg() || (!src.GetOrg().IsSetTaxname() && !src.GetOrg().IsSetCommon() && !src.GetOrg().IsSetDb())) {
        return true;
    } else {       
        return false;
    }
}


void CCleanup_imp::x_Common (list< CRef< CSubSource > >& mod_list1,
                             const list< CRef< CSubSource > >& mod_list2,
                             bool flag_changes)
{                                     
    CBioSource::TSubtype::iterator it1 = mod_list1.begin();
    while (it1 != mod_list1.end()) {
        CBioSource::TSubtype::const_iterator it2 = mod_list2.begin();
        bool found = false;
        while (it2 != mod_list2.end() && !found) {
            if (((!(*it1)->CanGetSubtype() && !(*it2)->CanGetSubtype())
                 ||(*it1)->GetSubtype() == (*it2)->GetSubtype())
                && ((!(*it1)->CanGetName() && !(*it2)->CanGetName())
                    || NStr::Equal((*it1)->GetName(), (*it2)->GetName()))) {
                found = true;                
            }
            ++it2;
        }
        if (!found) {
            it1 = mod_list1.erase(it1);
            if (flag_changes) {
                ChangeMade (CCleanupChange::eChangeSubsource);
            }
        } else {
            ++it1;
        }
    }
}


void CCleanup_imp::x_Common (list< CRef< COrgMod > >& mod_list1,
                             const list< CRef< COrgMod > >& mod_list2,
                             bool flag_changes)
{                                              
    COrgName::TMod::iterator it1 = mod_list1.begin();
    while (it1 != mod_list1.end()) {
        COrgName::TMod::const_iterator it2 = mod_list2.begin();
        bool found = false;
        while (it2 != mod_list2.end() && !found) {
            if (((!(*it1)->CanGetSubtype() && !(*it2)->CanGetSubtype())
                 ||(*it1)->GetSubtype() == (*it2)->GetSubtype())
                && ((!(*it1)->CanGetSubname() && !(*it2)->CanGetSubname())
                    || NStr::Equal((*it1)->GetSubname(), (*it2)->GetSubname()))) {
                found = true;
            }
            ++it2;
        }
        if (!found) {
            it1 = mod_list1.erase(it1);
            if (flag_changes) {
                ChangeMade (CCleanupChange::eChangeOrgmod);
            }
        } else {
            ++it1;
        }
    }
}

#define COMMON_INT_VALUE(o1, o2, x, y, b) \
    if ((o1).IsSet##x() && (o2).IsSet##x() \
        && (o1).Get##x() != (o2).Get##x()) { \
        (o1).Reset##x(); \
        if (b) { \
            ChangeMade (y); \
        } \
    }

#define COMMON_STRING_VALUE(o1, o2, x, y, b) \
    if ((o1).IsSet##x() && (o2).IsSet##x() \
        && !NStr::Equal((o1).Get##x(), (o2).Get##x())) { \
        (o1).Reset##x(); \
        if (b) { \
            ChangeMade (y); \
        } \
    }

#define COMMON_STRING_LIST(o1, o2, x, y, b) \
    if ((o1).CanGet##x()) { \
        if (!(o2).CanGet##x()) { \
            (o1).Reset##x(); \
        } else { \
            list <string>::iterator it1 = (o1).Set##x().begin(); \
            while (it1 != (o1).Set##x().end()) { \
                list <string>::const_iterator it2 = (o2).Get##x().begin(); \
                while (it2 != (o2).Get##x().end() \
                       && !NStr::Equal(*it1, *it2)) { \
                    ++it2; \
                } \
                if (it2 == (o2).Get##x().end()) { \
                    it1 = (o1).Set##x().erase(it1); \
                    if (b) { \
                        ChangeMade (y); \
                    } \
                } else { \
                    ++ it1; \
                } \
            } \
            if ((o1).Get##x().empty()) { \
                (o1).Reset##x(); \
            } \
        } \
    } 

void CCleanup_imp::x_Common (COrgName& host, const COrgName& guest, bool flag_changes)
{
    // BioSource.org.orgname.name
    if (host.IsSetName()
        && ((guest.IsSetName() && !Match (host.GetName(), guest.GetName()))
            || !guest.IsSetName())) {
        host.ResetName();
        if (flag_changes) {
            ChangeMade (CCleanupChange::eChangeBioSourceOther);
        }
    }
     
    COMMON_STRING_VALUE (host, guest, Attrib, CCleanupChange::eChangeBioSourceOther, flag_changes);
    COMMON_STRING_VALUE (host, guest, Lineage, CCleanupChange::eChangeBioSourceOther, flag_changes);
    COMMON_INT_VALUE (host, guest, Gcode, CCleanupChange::eChangeBioSourceOther, flag_changes);
    COMMON_INT_VALUE (host, guest, Mgcode, CCleanupChange::eChangeBioSourceOther, flag_changes);
    COMMON_STRING_VALUE (host, guest, Div, CCleanupChange::eChangeBioSourceOther, flag_changes);

    x_Common (host.SetMod(), guest.GetMod(), flag_changes);
    if (host.GetMod().empty()) {
        host.ResetMod();
    }
}
  
    
void CCleanup_imp::x_Common (COrg_ref::TDb& db1, const COrg_ref::TDb& db2, bool flag_changes)
{
    COrg_ref::TDb::iterator it1 = db1.begin();
    while (it1 != db1.end()) {
        bool found = false;
        ITERATE (COrg_ref::TDb, it2, db2) {
            if ((*it1)->Match (**it2)) {            
                found = true;
                break;
            }
        }
        if (!found) {
            it1 = db1.erase(it1);
            if (flag_changes) {
              ChangeMade (CCleanupChange::eChangeBioSourceOther);
            }
        } else {
            ++it1;
        }
    }    
}

        
void CCleanup_imp::x_Common (COrg_ref& host, const COrg_ref& guest, bool flag_changes)
{
    COMMON_STRING_VALUE (host, guest, Taxname, CCleanupChange::eChangeBioSourceOther, flag_changes);
    COMMON_STRING_VALUE (host, guest, Common, CCleanupChange::eChangeBioSourceOther, flag_changes);
    COMMON_STRING_LIST (host, guest, Mod, CCleanupChange::eChangeBioSourceOther, flag_changes);

    if (host.IsSetDb()) {
        if (guest.IsSetDb()) {
            x_Common (host.SetDb(), guest.GetDb(), flag_changes);
            if (host.GetDb().empty()) {
                host.ResetDb();
            }
        } else {
            host.ResetDb();
            if (flag_changes) {
                ChangeMade (CCleanupChange::eChangeBioSourceOther);
            }
        }
    }
    
    COMMON_STRING_LIST (host, guest, Syn, CCleanupChange::eChangeBioSourceOther, flag_changes);
    
    if (host.IsSetOrgname()) {
        if (guest.IsSetOrgname()) {
            x_Common (host.SetOrgname(), guest.GetOrgname(), flag_changes);
        } else {
            host.ResetOrgname();
            if (flag_changes) {
                ChangeMade (CCleanupChange::eChangeBioSourceOther);
            }
        }
    }
}    

// This was BioSourceCommon in toporg.c in the C Toolkit
// If the BioSources are not identical for all of the parts in a segmented set, the following steps 
// will be taken to create a ìcommonî BioSource descriptor:
// 1. If no BioSource descriptor exists on the segmented set, the first BioSource descriptor found 
// on any part will be copied to the segmented set.
// 2. For the following items, if the value for the BioSource descriptor on any part is set and 
// does not match the value for the BioSource descriptor on the segmented set, the value will be 
// unset for the BioSource descriptor on the segmented set:
// BioSource.genome
// BioSource.origin
// BioSource.is-focus
// BioSource.org.taxname
// BioSource.org.common
// BioSource.org.orgname.name
// BioSource.org.orgname.attrib
// BioSource.org.orgname.lineage
// BioSource.org.orgname.gcode
// BioSource.org.orgname.mgcode
// BioSource.org.orgname.div
// 3. Any item in the BioSource.subtype list that is not _common_ (subtype and subname match) to all
// of the parts will be removed from the BioSource on the segmented set.
// 4. Any item in the BioSource.org.mod list that is not common to all of the parts will be removed
// from the BioSource on the segmented set.
// 5. Any item in the BioSource.org.db list that is not common to all of the parts will be removed
// from the BioSource on the segmented set.
// 6. Any item in the BioSource.org.syn list that is not common to all of the parts will be removed
// from the BioSource on the segmented set.
// 7. Any item in the BioSource.org.orgname.mod list that is not common (subtype and subname match)
// to all of the parts will be removed from the BioSource on the segmented set.
void CCleanup_imp::x_Common(CBioSource& host, const CBioSource& guest, bool flag_changes)
{
    COMMON_INT_VALUE (host, guest, Genome, CCleanupChange::eChangeBioSourceGenome, flag_changes);
    COMMON_INT_VALUE (host, guest, Origin, CCleanupChange::eChangeBioSourceGenome, flag_changes);

    if (host.CanGetIs_focus() && host.IsSetIs_focus()
        && guest.CanGetIs_focus() && !guest.IsSetIs_focus()) {
        host.ResetIs_focus();
        if (flag_changes) {
            ChangeMade (CCleanupChange::eChangeBioSourceOther);
        }
    }    

    // remove subsources from the host that differ from guest
    if (!guest.IsSetSubtype()) {
        host.ResetSubtype();
    } else if (host.IsSetSubtype()) {
        x_Common(host.SetSubtype(), guest.GetSubtype(), flag_changes);
        if (host.GetSubtype().empty()) {
            host.ResetSubtype();
        }
    }
    
    if (host.IsSetOrg()) {
        if (!guest.IsSetOrg()) {
            host.ResetOrg();
        } else {
            x_Common (host.SetOrg(), guest.GetOrg(), flag_changes);
        }
    }
}


void CCleanup_imp::x_MoveDescriptor (CBioseq_set_Handle dst, CBioseq_Handle src, CSeqdesc::E_Choice choice)
{
    if (!src.IsSetDescr()) return;
    
    CBioseq_set_EditHandle bseh = dst.GetEditHandle();
    CBioseq_EditHandle beh = src.GetEditHandle();
    
    CSeq_descr::Tdata remove_list;
    remove_list.clear();
            
    NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, beh.SetDescr().Set()) {
        if ((*desc_it)->Which() == choice) {
            CRef<CSeqdesc> new_desc(new CSeqdesc);
            new_desc->Assign (**desc_it);
            bseh.AddSeqdesc (*new_desc);
            ChangeMade (CCleanupChange::eAddDescriptor);
            remove_list.push_back (*desc_it);
        }
    }
    for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
        it1 != remove_list.end(); ++it1) { 
        beh.RemoveSeqdesc(**it1);
        ChangeMade(CCleanupChange::eRemoveDescriptor);
    }        

    if (beh.SetDescr().Set().size() == 0) {
        beh.ResetDescr();
    }    
}


void CCleanup_imp::x_MoveDescriptor (CBioseq_set_Handle dst, CBioseq_set_Handle src, CSeqdesc::E_Choice choice)
{
    if (!src.IsSetDescr()) return;
    
    CBioseq_set_EditHandle bseh = dst.GetEditHandle();
    CBioseq_set_EditHandle beh = src.GetEditHandle();
    
    CSeq_descr::Tdata remove_list;
    remove_list.clear();
            
    NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, beh.SetDescr().Set()) {
        if ((*desc_it)->Which() == choice) {
            CRef<CSeqdesc> new_desc(new CSeqdesc);
            new_desc->Assign (**desc_it);
            bseh.AddSeqdesc (*new_desc);
            ChangeMade (CCleanupChange::eAddDescriptor);
            remove_list.push_back (*desc_it);
        }
    }
    for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
        it1 != remove_list.end(); ++it1) { 
        beh.RemoveSeqdesc(**it1);
        ChangeMade(CCleanupChange::eRemoveDescriptor);
    }        

    if (beh.SetDescr().Set().size() == 0) {
        beh.ResetDescr();
    }
}


bool CCleanup_imp::x_PromoteMergeableSource (CBioseq_set_Handle dst, const CBioSource& biosrc)
{
    bool ok_to_promote = false;
    bool found_any = false;
    bool changed = false;
    
    CRef<CBioSource> new_src(new CBioSource);
    if (dst.IsSetDescr()) {
        CBioseq_set_EditHandle beh = dst.GetEditHandle();
        NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, beh.SetDescr().Set()) {
            if ((*desc_it)->IsSource()) {
                found_any = true;
                if (x_OkToMerge ((*desc_it)->GetSource(), biosrc)) {
                    ok_to_promote = true;
                    changed = x_Merge ((*desc_it)->SetSource(), biosrc);
                    break;
                }
            }
        }
    }
    
    if (!found_any) {
        ok_to_promote = true;
        new_src->Assign (biosrc);           
        CRef<CSeqdesc> new_desc(new CSeqdesc);
        new_desc->SetSource(*new_src);
        CBioseq_set_EditHandle beh = dst.GetEditHandle();
        beh.AddSeqdesc (*new_desc);   
        ChangeMade (CCleanupChange::eAddDescriptor);     
    } 
    return ok_to_promote;               
}


// BioSource descriptors on the master sequence or parts set of a segmented set will be 
// promoted to the segmented set.
// The BioSource descriptors on a segset and on the parts of the segmented set will be examined.  
// Identical BioSources on SegSet Parts
// If the BioSources are identical for all of the parts in a segmented set
// and the segmented set has no BioSource descriptor, the BioSource descriptor on the first part
// will be promoted to the segmented set and the remaining BioSource descriptors on the parts will
// be removed.
// If the BioSources are identical for all of the parts in a segmented set and
// the segmented set has a BioSource descriptor that is mergeable with the 
// BioSources from the parts, the BioSource descriptor on the first part will be merged 
// with the mergeable BioSource descriptor on the segmented set and the BioSource
// descriptors on the parts will be removed.
// If the BioSources are identical for all of the parts in a segmented set and the segmented set has
// at least one BioSource descriptor but no BioSource descriptors that are mergeable 
// with the BioSources from the parts, the BioSource descriptors on the parts will remain on the parts.
// Different BioSources on Segmented Set Parts
// If the BioSources are not identical for all of the parts in a segmented set, the following steps 
// will be taken to create a ìcommonî BioSource descriptor:
// 1. If no BioSource descriptor exists on the segmented set, the first BioSource descriptor found 
// on any part will be copied to the segmented set.
// 2. For the following items, if the value for the BioSource descriptor on any part is set and 
// does not match the value for the BioSource descriptor on the segmented set, the value will be 
// unset for the BioSource descriptor on the segmented set:
// BioSource.genome
// BioSource.origin
// BioSource.is-focus
// BioSource.org.taxname
// BioSource.org.common
// BioSource.org.orgname.name
// BioSource.org.orgname.attrib
// BioSource.org.orgname.lineage
// BioSource.org.orgname.gcode
// BioSource.org.orgname.mgcode
// BioSource.org.orgname.div
// 3. Any item in the BioSource.subtype list that is not _common_ (subtype and subname match) to all
// of the parts will be removed from the BioSource on the segmented set.
// 4. Any item in the BioSource.org.mod list that is not common to all of the parts will be removed
// from the BioSource on the segmented set.
// 5. Any item in the BioSource.org.db list that is not common to all of the parts will be removed
// from the BioSource on the segmented set.
// 6. Any item in the BioSource.org.syn list that is not common to all of the parts will be removed
// from the BioSource on the segmented set.
// 7. Any item in the BioSource.org.orgname.mod list that is not common (subtype and subname match)
// to all of the parts will be removed from the BioSource on the segmented set.
//
// If after this process the BioSource on the segmented set is empty (see definition 4) the BioSource
// on the segmented set will be removed.

void CCleanup_imp::x_FixSegSetSource 
(CBioseq_set_Handle segset,
 CBioseq_set_Handle parts,
 CBioseq_set_Handle *nuc_prot_parent)
{
    // move source descriptors on master sequence and parts set to segset
    if (segset.GetCompleteBioseq_set()->IsSetSeq_set()) {
        ITERATE (list< CRef< CSeq_entry > >, it, segset.GetCompleteBioseq_set()->GetSeq_set()) {        
            if ((*it)->Which() == CSeq_entry::e_Seq
                && (*it)->GetSeq().IsSetDescr()) {
                x_MoveDescriptor (segset, 
                                  m_Scope->GetBioseqHandle ((*it)->GetSeq()), 
                                  CSeqdesc::e_Source);
            }
        }
    }
    x_MoveDescriptor (segset, parts, CSeqdesc::e_Source);
    
    // now check to see if the sources on all the parts are identical
    if (!parts.GetCompleteBioseq_set()->IsSetSeq_set()) {
        return;
    }
    
    // check to see if the segset has any source descriptors now.
    // if not, and if there is a nuc-prot parent set, merge/promote
    // descriptors to the nuc-prot parent set, otherwise merge/promote
    // to the seg-set and worry about merging to the parent nuc-prot set
    // later.  Do this to avoid creating a descriptor that might be
    // removed later as a duplicate.
    bool segset_has_source = false;
    if (nuc_prot_parent != NULL) {
        ITERATE (CSeq_descr::Tdata, desc_it, segset.GetDescr().Get()) {
            if ((*desc_it)->Which() == CSeqdesc::e_Source) {
                segset_has_source = true;
                break;
            }
        }
    }
    
    list< CRef< CSeq_entry > > set = parts.GetCompleteBioseq_set()->GetSeq_set();
    
    CSeq_descr::Tdata src_list;
    vector<CBioseq_Handle> bh_list;
    bool all_src_same = true;
    bool all_src_found = true;
    ITERATE (list< CRef< CSeq_entry > >, it, set) {        
        if ((*it)->Which() == CSeq_entry::e_Seq
                && (*it)->GetSeq().IsSetDescr()) {
            bool found = false;
            ITERATE (CSeq_descr::Tdata, desc_it, (*it)->GetSeq().GetDescr().Get()) {
                if ((*desc_it)->Which() == CSeqdesc::e_Source) {
                    if (src_list.size() > 0) {
                        if (!x_Identical((*desc_it)->GetSource(), (*(src_list.begin()))->GetSource())) {
                            all_src_same = false;
                        }
                    }
                    src_list.push_back (*desc_it);
                    bh_list.push_back (m_Scope->GetBioseqHandle ((*it)->GetSeq()));
                    found = true;
                }
            }
            if (!found) {
                all_src_same = false;
                all_src_found = false;
            }
        }
    }
    if (src_list.size() == 0) {
        // no source descriptors on parts found, nothing to do
    } else if (!all_src_found) {
        // don't have sources on all parts, can't continue
    } else if (all_src_same) {
        // try to merge with nuc-prot set if possible, other
        if ((nuc_prot_parent != NULL && !segset_has_source
            && x_PromoteMergeableSource (*nuc_prot_parent, (*(src_list.begin()))->GetSource()))
            || x_PromoteMergeableSource (segset, (*(src_list.begin()))->GetSource())) {            
            unsigned int index = 0;
            NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, src_list) {
                CBioseq_EditHandle eh = bh_list[index].GetEditHandle();
                eh.RemoveSeqdesc(**desc_it);
                ChangeMade (CCleanupChange::eRemoveDescriptor);
                if (eh.SetDescr().Set().size() == 0) {
                    eh.ResetDescr();
                }
                index++;
            }
        }
    } else {
        CBioseq_set_EditHandle set_eh = segset.GetEditHandle();
        bool found = false;
        CRef<CSeqdesc> found_src;
        if (set_eh.IsSetDescr()) {
            NON_CONST_ITERATE (CSeq_descr::Tdata, segset_src_it, set_eh.SetDescr().Set()) {
                if ((*segset_src_it)->Which() == CSeqdesc::e_Source) {
                    found = true;
                
                    ITERATE (CSeq_descr::Tdata, desc_it, src_list) {
                        x_Common((*segset_src_it)->SetSource(), (*desc_it)->GetSource(), true);
                    }
                    found_src = *segset_src_it;
                    break;
                }
            }
        }
        if (found) {
            // remove the descriptor if it is empty or if it is mergeable with 
            // the nuc-prot parent source
            if (x_IsBioSourceEmpty(found_src->GetSource())
                || (nuc_prot_parent != NULL 
                    && x_PromoteMergeableSource (*nuc_prot_parent, found_src->GetSource()))) {
                set_eh.RemoveSeqdesc(*found_src);
                ChangeMade(CCleanupChange::eRemoveDescriptor);
                if (set_eh.SetDescr().Set().size() == 0) {
                    set_eh.ResetDescr();
                }
            } 
        } else {        
            CRef<CSeqdesc> new_src(new CSeqdesc());
            CRef<CBioSource> new_biosrc(new CBioSource());
            new_biosrc->Assign ((*src_list.begin())->SetSource());
            new_src->SetSource(*new_biosrc);
            ITERATE (CSeq_descr::Tdata, desc_it, src_list) {
                x_Common(new_src->SetSource(), (*desc_it)->GetSource(), false);
            }
            if (!x_IsBioSourceEmpty(new_src->GetSource())) {
                // if the common source is mergeable with the nuc-prot parent, merge it and skip
                // creating a descriptor on the segmented set that will just be
                // removed later, otherwise put the common source on the segmented set
                if (nuc_prot_parent == NULL 
                    || !x_PromoteMergeableSource (*nuc_prot_parent, new_src->GetSource())) {
                    set_eh.AddSeqdesc(*new_src);
                    ChangeMade(CCleanupChange::eAddDescriptor);
                }
            }            
        }
    }
}


void CCleanup_imp::x_FixSegSetSource (CBioseq_set_Handle bh, CBioseq_set_Handle *nuc_prot_parent)
{
    if (!bh.GetCompleteBioseq_set()->IsSetSeq_set()) {
        return;
    }
    
    CConstRef<CBioseq_set> b = bh.GetCompleteBioseq_set();
    list< CRef< CSeq_entry > > set = (*b).GetSeq_set();
    
    if (bh.IsSetClass() && bh.GetClass() == CBioseq_set::eClass_segset) {
        ITERATE (list< CRef< CSeq_entry > >, it, set) {
            if ((*it)->Which() == CSeq_entry::e_Set
                && (*it)->GetSet().IsSetClass()
                && (*it)->GetSet().GetClass() == CBioseq_set::eClass_parts) {
                
                
                x_FixSegSetSource (bh, m_Scope->GetBioseq_setHandle((*it)->GetSet()), nuc_prot_parent);
            }
        }
    }      
}


bool s_ContainsDescriptor (const CBioseq& bs, CSeqdesc::E_Choice desc_type)
{
    if (bs.IsSetDescr()) {
        ITERATE (CSeq_descr::Tdata, desc_it, bs.GetDescr().Get()) {
            if ((*desc_it)->Which() == desc_type) {
                return true;
            }
        }
    }
    return false;
}


bool s_ContainsDescriptor (const CBioseq_set& bs, CSeqdesc::E_Choice desc_type)
{
    if (bs.IsSetDescr()) {
        ITERATE (CSeq_descr::Tdata, desc_it, bs.GetDescr().Get()) {
            if ((*desc_it)->Which() == desc_type) {
                return true;
            }
        }
    }
    
    if (bs.CanGetSeq_set()) {
        ITERATE (list < CRef <CSeq_entry > >, it, bs.GetSeq_set()) {
            if ((*it)->Which() == CSeq_entry::e_Seq
                && s_ContainsDescriptor ((*it)->GetSeq(), desc_type)) {
                return true;
            } else if ((*it)->Which() == CSeq_entry::e_Set
                && s_ContainsDescriptor((*it)->GetSet(), desc_type)) {
                return true;
            }                
        }
    }
    return false;
}


// If a BioSource descriptor appears on a WGS, Mut, Pop, Phy, or Eco set, 
// place a copy of the  BioSource descriptor on each member of the set 
// that does not already have a BioSource descriptor anywhere on or in 
// the set member and remove the BioSource descriptor from the parent set.
void CCleanup_imp::x_FixSetSource (CBioseq_set_Handle bh)
{
    if (bh.IsEmptySeq_set()) return;
    CSeqdesc_CI desc_ci (bh.GetParentEntry(), CSeqdesc::e_Source, 1);
    if (!desc_ci) return;
    
    ITERATE (list < CRef < CSeq_entry > >, it, bh.GetCompleteBioseq_set()->GetSeq_set()) {
        if ((*it)->Which() == CSeq_entry::e_Seq 
            && ! s_ContainsDescriptor ((*it)->GetSeq(), CSeqdesc::e_Source)) {
            CRef<CSeqdesc> new_desc(new CSeqdesc);
            new_desc->Assign (*desc_ci);
            CBioseq_EditHandle beh = (m_Scope->GetBioseqHandle((**it).GetSeq())).GetEditHandle();
            beh.AddSeqdesc (*new_desc);
            ChangeMade (CCleanupChange::eAddDescriptor);
        } else if ((*it)->Which() == CSeq_entry::e_Set
            && ! s_ContainsDescriptor ((*it)->GetSet(), CSeqdesc::e_Source)) {
            CRef<CSeqdesc> new_desc(new CSeqdesc);
            new_desc->Assign (*desc_ci);
            CBioseq_set_EditHandle beh = (m_Scope->GetBioseq_setHandle((**it).GetSet())).GetEditHandle();
            beh.AddSeqdesc (*new_desc);
            ChangeMade (CCleanupChange::eAddDescriptor);
        }
    }
    CBioseq_set_EditHandle bseh = bh.GetEditHandle();
    bseh.RemoveSeqdesc(*desc_ci);
    ChangeMade (CCleanupChange::eRemoveDescriptor);
}


// cleanup up BioSource Descriptors and Features
// This process should start at the lowest levels and work its way up.
// 1) Convert Org_ref descriptors to BioSource descriptors 
// 2) Convert Org_ref features to BioSource features.
// 3) Convert appropriate full-length BioSource features to descriptors 
// 4) Merge biosource features
// 5) Merge existing BioSource descriptors.
// 6) Fix segmented sets.
// 7) Fix nuc-prot sets (which could contain segmented sets).
// 8) Propagate BioSource descriptors on WGS, Mut, Pop, Phy, and Eco Sets

void CCleanup_imp::x_ExtendedCleanupBioSourceFeatures(CBioseq_Handle bh)
{    
    vector<CSeq_feat_Handle> source_feat_list; // list of source features on this Bioseq
                                               // that were not converted to descriptors
                                               // use this to find source features that
                                               // should be merged
    bool any_removed = false;
    
    bh.GetCompleteBioseq()->GetAnnot();

    CFeat_CI feat_ci(bh);
    while (feat_ci) {
        if (feat_ci->GetFeatType() == CSeqFeatData::e_Biosrc) {
            const CSeq_feat& cf = feat_ci->GetOriginalFeature();
            if (IsFeatureFullLength(cf, m_Scope)
                && x_OkToConvertToDescriptor(bh, cf.GetData().GetBiosrc())) {
                // convert full-length source features to descriptors
                
                // create new descriptor to hold source feature source
                CRef<CSeqdesc> desc(new CSeqdesc);             
                desc->Select(CSeqdesc::e_Source);
                desc->SetSource(const_cast< CBioSource& >(cf.GetData().GetBiosrc()));            
                CBioseq_EditHandle eh = bh.GetEditHandle();
                eh.AddSeqdesc(*desc);
                // remove feature
                CSeq_feat_EditHandle efh (feat_ci->GetSeq_feat_Handle());
                efh.Remove();
                ChangeMade (CCleanupChange::eConvertFeatureToDescriptor);
                any_removed = true;
            } else {
                bool found = false;
                for (unsigned int i = 0; i < source_feat_list.size() && !found; i++) {
                    // can this feature be merged with another one?
                    if (x_OkToMerge (cf.GetData().GetBiosrc(),
                        source_feat_list[i].GetData().GetBiosrc())
                        && sequence::Compare (cf.GetLocation(),
                                              source_feat_list[i].GetLocation(),
                                              m_Scope) == sequence::eSame) {
                        // merge the two features
                        CRef<CBioSource> new_src;
                        new_src->Assign (cf.GetData().GetBiosrc());
                        x_Merge (*new_src, source_feat_list[i].GetData().GetBiosrc());
                        CRef<CSeq_feat> new_feat;
                        new_feat->Assign (*(source_feat_list[i].GetSeq_feat()));
                        CSeq_feat_EditHandle rfh (source_feat_list[i]);
                        rfh.Replace (*new_feat);
                        
                        // remove the old feature
                        CSeq_feat_EditHandle efh (feat_ci->GetSeq_feat_Handle());
                        efh.Remove();
                        ChangeMade (CCleanupChange::eRemoveFeat);
                        found = true;
                    }         
                }  
                if (!found) {                            
                    source_feat_list.push_back (feat_ci->GetSeq_feat_Handle());
                }
            }
        }
        ++feat_ci;                
    }
    if (any_removed && source_feat_list.size() == 0) {
      x_RemoveEmptyFeatureAnnots(bh);
    }      
}


void CCleanup_imp::x_ExtendedCleanupBioSourceDescriptorsAndFeatures(CBioseq_Handle bh)
{
    // First, convert Org-ref descriptors to BioSource descriptors
    x_ConvertOrgDescToSourceDescriptor (bh);
    // Convert Org-ref features to BioSource features
    x_ConvertOrgFeatToSource (bh);

    // Convert full-length BioSource features to descriptors and
    // merge BioSource features
    x_ExtendedCleanupBioSourceFeatures (bh);

    if (bh.IsSetDescr()) {
        // Now merge descriptors
        CSeq_descr::Tdata remove_list;    
        CBioseq_EditHandle edith = m_Scope->GetEditHandle(bh);     
        x_RecurseDescriptorsForMerge(edith.SetDescr(),
                                     &ncbi::objects::CCleanup_imp::x_IsMergeableBioSource,
                                     &ncbi::objects::CCleanup_imp::x_MergeDuplicateBioSources,
                                     remove_list);           
        for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
            it1 != remove_list.end(); ++it1) { 
            edith.RemoveSeqdesc(**it1);
            ChangeMade(CCleanupChange::eRemoveDescriptor);
        }        
    }
}


void CCleanup_imp::x_ExtendedCleanupBioSourceDescriptorsAndFeatures(CBioseq_set_Handle bss)
{
    // First, clean the members of this set, unless this is a nuc-prot or segset
    if (bss.GetCompleteBioseq_set()->IsSetSeq_set()
        && (!bss.CanGetClass() 
            || (bss.GetClass() != CBioseq_set::eClass_segset 
                && bss.GetClass() != CBioseq_set::eClass_nuc_prot))) {
       CConstRef<CBioseq_set> b = bss.GetCompleteBioseq_set();
       list< CRef< CSeq_entry > > set = (*b).GetSeq_set();
       
       ITERATE (list< CRef< CSeq_entry > >, it, set) {
            switch ((**it).Which()) {
                case CSeq_entry::e_Seq:
                    x_ExtendedCleanupBioSourceDescriptorsAndFeatures(m_Scope->GetBioseqHandle((**it).GetSeq()));
                    break;
                case CSeq_entry::e_Set:
                {
                    CBioseq_set_Handle bssh = m_Scope->GetBioseq_setHandle((**it).GetSet());
                    x_ExtendedCleanupBioSourceDescriptorsAndFeatures(bssh);
                }
                    break;
                case CSeq_entry::e_not_set:
                default:
                    break;
            }
        }
    }
    
    // do generic tasks for this level
    x_ConvertOrgDescToSourceDescriptor (bss);
    x_ConvertOrgFeatToSource (bss);

    // features are fixed when the Bioseq is fixed

    if (bss.IsSetDescr()) {
        // merge descriptors
        CSeq_descr::Tdata remove_list;    
        CBioseq_set_EditHandle edith = bss.GetEditHandle ();     
        x_RecurseDescriptorsForMerge(edith.SetDescr(),
                                     &ncbi::objects::CCleanup_imp::x_IsMergeableBioSource,
                                     &ncbi::objects::CCleanup_imp::x_MergeDuplicateBioSources,
                                     remove_list);           
        for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
            it1 != remove_list.end(); ++it1) { 
            edith.RemoveSeqdesc(**it1);
            ChangeMade(CCleanupChange::eRemoveDescriptor);
        }        
    }

    // Now do set class-specific fixing
    if (bss.CanGetClass()) {
        switch (bss.GetClass()) {
            case CBioseq_set::eClass_segset:
                x_FixSegSetSource (bss);
                break;
            case CBioseq_set::eClass_nuc_prot:
                x_FixNucProtSources (bss);
                break;
            case CBioseq_set::eClass_genbank:
            case CBioseq_set::eClass_wgs_set:
            case CBioseq_set::eClass_mut_set:
            case CBioseq_set::eClass_pop_set:
            case CBioseq_set::eClass_phy_set:
            case CBioseq_set::eClass_eco_set:
                x_FixSetSource (bss);
                break;
            default:
                break;
        }
    }
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE
