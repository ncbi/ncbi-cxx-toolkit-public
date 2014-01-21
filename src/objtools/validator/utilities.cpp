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
 * Author:  Mati Shomrat
 *
 * File Description:
 *      Implementation of utility classes and functions.
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>

#include <serial/enumvalues.hpp>
#include <serial/serialimpl.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/object_manager.hpp>
#include <objects/taxon3/taxon3.hpp>
//#include <objtools/validator/validatorp.hpp>
#include <objtools/validator/utilities.hpp>

#include <vector>
#include <algorithm>
#include <list>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)


// =============================================================================
//                                    Functions
// =============================================================================


bool IsClassInEntry(const CSeq_entry& se, CBioseq_set::EClass clss)
{
    for ( CTypeConstIterator <CBioseq_set> si(se); si; ++si ) {
        if ( si->GetClass() == clss ) {
            return true;
        }
    }
    return false;
}


bool IsDeltaOrFarSeg(const CSeq_loc& loc, CScope* scope)
{
    CBioseq_Handle bsh = BioseqHandleFromLocation(scope, loc);
    const CSeq_entry& se = *bsh.GetTopLevelEntry().GetCompleteSeq_entry();

    if ( bsh.IsSetInst_Repr() ) {
        CBioseq_Handle::TInst::TRepr repr = bsh.GetInst_Repr();
        if ( repr == CSeq_inst::eRepr_delta ) {
            if ( !IsClassInEntry(se, CBioseq_set::eClass_nuc_prot) ) {
                return true;
            }
        }
        if ( repr == CSeq_inst::eRepr_seg ) {
            if ( !IsClassInEntry(se, CBioseq_set::eClass_parts) ) {
                return true;
            }
        }
    }

    return false;
}


// Check if string is either empty or contains just white spaces
bool IsBlankStringList(const list< string >& str_list)
{
    ITERATE( list< string >, str, str_list ) {
        if ( !NStr::IsBlank(*str) ) {
            return false;
        }
    }
    return true;
}


TGi GetGIForSeqId(const CSeq_id& id)
{
    TGi gi = ZERO_GI;
    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));

    try {
        CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id);
        gi = scope->GetGi (idh);
	} catch (CException &) {
	} catch (std::exception &) {
    }
    return gi;
}



CScope::TIds GetSeqIdsForGI(TGi gi)
{
    CScope::TIds id_list;
    CSeq_id tmp_id;
    tmp_id.SetGi(gi);
    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    scope->AddDefaults();

    try {
        id_list = scope->GetIds(tmp_id);

	} catch (CException &) {
	} catch (std::exception &) {
    }
    return id_list;
}


string GetSequenceStringFromLoc
(const CSeq_loc& loc,
 CScope& scope)
{
    CNcbiOstrstream oss;
    CFastaOstream fasta_ostr(oss);
    fasta_ostr.SetFlag(CFastaOstream::fAssembleParts);
    fasta_ostr.SetFlag(CFastaOstream::fInstantiateGaps);
    string s;

    try {
        for (CSeq_loc_CI citer (loc); citer; ++citer) {
            const CSeq_loc& part = citer.GetEmbeddingSeq_loc();
            CBioseq_Handle bsh = BioseqHandleFromLocation (&scope, part);
            if (bsh) {
                fasta_ostr.WriteSequence (bsh, &part);
            }
        }
        s = CNcbiOstrstreamToString(oss);
        NStr::ReplaceInPlace(s, "\n", "");
    } catch (CException&) {
        s = kEmptyStr;
    }

    return s;
}


CSeqVector GetSequenceFromLoc
(const CSeq_loc& loc,
 CScope& scope,
 CBioseq_Handle::EVectorCoding coding)
{
    CConstRef<CSeqMap> map = 
        CSeqMap::CreateSeqMapForSeq_loc(loc, &scope);
    return CSeqVector(*map, scope, coding, eNa_strand_plus);
}


CSeqVector GetSequenceFromFeature
(const CSeq_feat& feat,
 CScope& scope,
 CBioseq_Handle::EVectorCoding coding,
 bool product)
{

    if ( (product   &&  !feat.CanGetProduct())  ||
         (!product  &&  !feat.CanGetLocation()) ) {
        return CSeqVector();
    }

    const CSeq_loc* loc = product ? &feat.GetProduct() : &feat.GetLocation();
    return GetSequenceFromLoc(*loc, scope, coding);
}


/***** Calculate Accession for a given object *****/


static string s_GetBioseqAcc(const CSeq_id& id, int* version)
{
    try {
        return id.GetSeqIdString(version);
    } catch (CException&) {
        return kEmptyStr;
    }
}


static string s_GetBioseqAcc(const CBioseq_Handle& handle, int* version)
{
    if (handle) {
        CConstRef<CSeq_id> seqid = sequence::GetId(handle, sequence::eGetId_Best).GetSeqId();
        if (seqid) {
            return s_GetBioseqAcc(*seqid, version);
        }
    }
    return kEmptyStr;
}


static string s_GetSeq_featAcc(const CSeq_feat& feat, CScope& scope, int* version)
{
    CBioseq_Handle seq = BioseqHandleFromLocation (&scope, feat.GetLocation());
    CBioseq_set_Handle parent = seq.GetParentBioseq_set();
    if (parent && parent.IsSetClass() && parent.GetClass() == CBioseq_set::eClass_parts) {
        parent = parent.GetParentBioseq_set();
        if (parent && parent.IsSetClass() && parent.GetClass() == CBioseq_set::eClass_segset) {
            CBioseq_CI m(parent);
            if (m) {
                return s_GetBioseqAcc(*m, version);
            }
        }
    }

    return s_GetBioseqAcc(seq, version);
}


static string s_GetBioseqAcc(const CBioseq& seq, CScope& scope, int* version)
{
    CBioseq_Handle handle = scope.GetBioseqHandle(seq);
    return s_GetBioseqAcc(handle, version);
}

static const CBioseq* s_GetSeqFromSet(const CBioseq_set& bsst, CScope& scope)
{
    const CBioseq* retval = NULL;

    switch (bsst.GetClass()) {
        case CBioseq_set::eClass_gen_prod_set:
            // find the genomic bioseq
            FOR_EACH_SEQENTRY_ON_SEQSET (it, bsst) {
                if ((*it)->IsSeq()) {
                    const CSeq_inst& inst = (*it)->GetSeq().GetInst();
                    if (inst.IsSetMol()  &&  inst.GetMol() == CSeq_inst::eMol_dna) {
                        retval = &(*it)->GetSeq();
                        break;
                    }
                }
            }
            break;
        case CBioseq_set::eClass_nuc_prot:
            // find the nucleotide bioseq
            FOR_EACH_SEQENTRY_ON_SEQSET (it, bsst) {
                if ((*it)->IsSeq()  &&  (*it)->GetSeq().IsNa()) {
                    retval = &(*it)->GetSeq();
                    break;
                } else if ((*it)->IsSet()  &&
                           (*it)->GetSet().IsSetClass() &&
                           (*it)->GetSet().GetClass() == CBioseq_set::eClass_segset) {
                    retval = s_GetSeqFromSet((*it)->GetSet(), scope);
                    break;
                }
            }
            if (!retval) {
                FOR_EACH_SEQENTRY_ON_SEQSET (it, bsst) {
                    if ((*it)->IsSeq()) {
                        retval = &(*it)->GetSeq();
                        break;
                    }
                }
            }
            break;
        case CBioseq_set::eClass_segset:
            // find the master bioseq
            FOR_EACH_SEQENTRY_ON_SEQSET (it, bsst) {
                if ((*it)->IsSeq()) {
                    retval = &(*it)->GetSeq();
                    break;
                }
            }
            break;

        default:
            // find the first bioseq
            CTypeConstIterator<CBioseq> seqit(ConstBegin(bsst));
            if (seqit) {
                retval = &(*seqit);
            }
            break;
    }

    return retval;
}


static bool s_IsDescOnSeqEntry (const CSeq_entry& entry, const CSeqdesc& desc)
{
    if (entry.IsSetDescr()) {
        FOR_EACH_DESCRIPTOR_ON_SEQENTRY (it, entry) {
            if ((**it).Equals(desc)) {
                return true;
            }
        }
    }
    return false;
}



static string s_GetAccessionForSeqdesc (CSeq_entry_Handle seh, const CSeqdesc& desc, CScope& scope, int* version)
{
    if (!seh) {
        return kEmptyStr;\
    } else if (seh.IsSeq()) {
        return s_GetBioseqAcc(*(seh.GetSeq().GetCompleteBioseq()), scope, version);
    } else if (s_IsDescOnSeqEntry (*(seh.GetCompleteSeq_entry()), desc)) {
        if (seh.IsSeq()) {
            return s_GetBioseqAcc(*(seh.GetSeq().GetCompleteBioseq()), scope, version);
        } else if (seh.IsSet()) {
            const CBioseq* seq = s_GetSeqFromSet(*(seh.GetSet().GetCompleteBioseq_set()), scope);
            if (seq != NULL) {
                return s_GetBioseqAcc(*seq, scope, version);
            }
        }
    } else {
        CSeq_entry_Handle parent = seh.GetParentEntry();
        if (parent) {
            return s_GetAccessionForSeqdesc(parent, desc, scope, version);
        }
    }
    return kEmptyStr;
}


CConstRef<CSeq_id> GetReportableSeqIdForAlignment(const CSeq_align& align, CScope& scope)
{
    // temporary - to match C Toolkit
    if (align.IsSetSegs() && align.GetSegs().IsStd()) {
        return CConstRef<CSeq_id>(NULL);
    }
    try {
        if (align.IsSetDim()) {
            for (int i = 0; i < align.GetDim(); ++i) {
                const CSeq_id& id = align.GetSeq_id(i);
                CBioseq_Handle bsh = scope.GetBioseqHandle(id);
                if (bsh) {
                    return CConstRef<CSeq_id>(&id);
                }
            }
        } else if (align.IsSetSegs() && align.GetSegs().IsDendiag()) {
            const CSeq_id& id = *(align.GetSegs().GetDendiag().front()->GetIds()[0]);
            return CConstRef<CSeq_id>(&id);
        }
        // failed to find resolvable ID, use bare ID
        const CSeq_id& id = align.GetSeq_id(0); 
        return CConstRef<CSeq_id>(&id);
    } catch (CException& ) {
    }
    return CConstRef<CSeq_id>(NULL);
}


string GetAccessionFromObjects(const CSerialObject* obj, const CSeq_entry* ctx, CScope& scope, int* version)
{
    string empty_acc = "";

    if (obj && obj->GetThisTypeInfo() == CSeqdesc::GetTypeInfo() && ctx) {
        CSeq_entry_Handle seh = scope.GetSeq_entryHandle(*ctx);
        const CSeqdesc& desc = dynamic_cast<const CSeqdesc&>(*obj);
        string acc = s_GetAccessionForSeqdesc(seh, desc, scope, version);
        if (!NStr::IsBlank(acc)) {
            return acc;
        }
    }

    if (ctx) {
        if (ctx->IsSeq()) {
            return s_GetBioseqAcc(ctx->GetSeq(), scope, version);
        } else if (ctx->IsSet()) {
            const CBioseq* seq = s_GetSeqFromSet(ctx->GetSet(), scope);
            if (seq != NULL) {
                return s_GetBioseqAcc(*seq, scope, version);
            }
        }
    } else if (obj) {
        if (obj->GetThisTypeInfo() == CSeq_feat::GetTypeInfo()) {
            const CSeq_feat& feat = dynamic_cast<const CSeq_feat&>(*obj);
            return s_GetSeq_featAcc(feat, scope, version);
        } else if (obj->GetThisTypeInfo() == CBioseq::GetTypeInfo()) {
            const CBioseq& seq = dynamic_cast<const CBioseq&>(*obj);
            return s_GetBioseqAcc(seq, scope, version);
        } else if (obj->GetThisTypeInfo() == CBioseq_set::GetTypeInfo()) {
            const CBioseq_set& bsst = dynamic_cast<const CBioseq_set&>(*obj);
            const CBioseq* seq = s_GetSeqFromSet(bsst, scope);
            if (seq != NULL) {
                return s_GetBioseqAcc(*seq, scope, version);
            }
        } else if (obj->GetThisTypeInfo() == CSeq_entry::GetTypeInfo()) {
            const CSeq_entry& entry = dynamic_cast<const CSeq_entry&>(*obj);
            if (entry.IsSeq()) {
                return s_GetBioseqAcc(entry.GetSeq(), scope, version);
            } else if (entry.IsSet()) {
                const CBioseq* seq = s_GetSeqFromSet(entry.GetSet(), scope);
                if (seq != NULL) {
                    return s_GetBioseqAcc(*seq, scope, version);
                }
            }
        } else if (obj->GetThisTypeInfo() == CSeq_annot::GetTypeInfo()) {
            CSeq_annot_Handle ah = scope.GetSeq_annotHandle (dynamic_cast<const CSeq_annot&>(*obj));
            if (ah) {
                CSeq_entry_Handle seh = ah.GetParentEntry();
                if (seh) {
                    if (seh.IsSeq()) {
                        return s_GetBioseqAcc(seh.GetSeq(), version);
                    } else if (seh.IsSet()) {
                        CBioseq_set_Handle bsh = seh.GetSet();
                        const CBioseq_set& bsst = *(bsh.GetCompleteBioseq_set());
                        const CBioseq* seq = s_GetSeqFromSet(bsst, scope);
                        if (seq != NULL) {
                            return s_GetBioseqAcc(*seq, scope, version);
                        }
                    }
                }
            }
        } else if (obj->GetThisTypeInfo() == CSeq_align::GetTypeInfo()) {
            const CSeq_align& align = dynamic_cast<const CSeq_align&>(*obj);
            CConstRef<CSeq_id> id = GetReportableSeqIdForAlignment(align, scope);
            if (id) {
                CBioseq_Handle bsh = scope.GetBioseqHandle(*id);
                if (bsh) {
                    return s_GetBioseqAcc(bsh, version);
                } else {
                    return s_GetBioseqAcc(*id, version);
                }
            }
        } else if (obj->GetThisTypeInfo() == CSeq_graph::GetTypeInfo()) {
            const CSeq_graph& graph = dynamic_cast<const CSeq_graph&>(*obj);
            try {
                const CSeq_loc& loc = graph.GetLoc();
                const CSeq_id *id = loc.GetId();
                if (id) {
                    return s_GetBioseqAcc (*id, version);
                }
            } catch (CException& ) {
            }
        }
    }
    return empty_acc;
}


CBioseq_set_Handle GetSetParent (CBioseq_set_Handle set, CBioseq_set::TClass set_class)
{
    CBioseq_set_Handle gps;

    CSeq_entry_Handle parent = set.GetParentEntry();
    if (!parent) {
        return gps;
    } else if (!(parent = parent.GetParentEntry())) {
        return gps;
    } else if (!parent.IsSet()) {
        return gps;
    } else if (parent.GetSet().IsSetClass() && parent.GetSet().GetClass() == set_class) {
        return parent.GetSet();
    } else {
        return GetSetParent (parent.GetSet(), set_class);
    }    
}


CBioseq_set_Handle GetSetParent (CBioseq_Handle bioseq, CBioseq_set::TClass set_class)
{
    CBioseq_set_Handle set;

    CSeq_entry_Handle parent = bioseq.GetParentEntry();
    if (!parent) {
        return set;
    } else if (!(parent = parent.GetParentEntry())) {
        return set;
    } else if (!parent.IsSet()) {
        return set;
    } else if (parent.GetSet().IsSetClass() && parent.GetSet().GetClass() == set_class) {
        return parent.GetSet();
    } else {
        return GetSetParent (parent.GetSet(), set_class);
    }
}


CBioseq_set_Handle GetGenProdSetParent (CBioseq_set_Handle set)
{
    return GetSetParent (set, CBioseq_set::eClass_gen_prod_set);
}

CBioseq_set_Handle GetGenProdSetParent (CBioseq_Handle bioseq)
{
    return GetSetParent(bioseq, CBioseq_set::eClass_gen_prod_set);
}


CBioseq_set_Handle GetNucProtSetParent (CBioseq_Handle bioseq)
{
    return GetSetParent(bioseq, CBioseq_set::eClass_nuc_prot);
}


CBioseq_Handle GetNucBioseq (CBioseq_set_Handle bioseq_set)
{
    CBioseq_Handle nuc;

    if (!bioseq_set) {
        return nuc;
    }
    CBioseq_CI bit(bioseq_set, CSeq_inst::eMol_na);
    if (bit) {
        nuc = *bit;
    } else {
        CSeq_entry_Handle parent = bioseq_set.GetParentEntry();
        if (parent && (parent = parent.GetParentEntry())
            && parent.IsSet()) {
            nuc = GetNucBioseq (parent.GetSet());
        }
    }
    return nuc;
}
       

CBioseq_Handle GetNucBioseq (CBioseq_Handle bioseq)
{
    CBioseq_Handle nuc;

    if (bioseq.IsNucleotide()) {
        return bioseq;
    }
    CSeq_entry_Handle parent = bioseq.GetParentEntry();
    if (parent && (parent = parent.GetParentEntry())
        && parent.IsSet()) {
        nuc = GetNucBioseq (parent.GetSet());
    }
    return nuc;
}


EAccessionFormatError ValidateAccessionString (string accession, bool require_version)
{
    if (NStr::IsBlank (accession)) {
        return eAccessionFormat_null;
    } else if (accession.length() >= 16) {
        return eAccessionFormat_too_long;
    } else if (accession.length() < 3 
               || ! isalpha (accession.c_str()[0]) 
               || ! isupper (accession.c_str()[0])) {
        return eAccessionFormat_no_start_letters;
    }
    
    string str = accession;
    if (NStr::StartsWith (str, "NZ_")) {
        str = str.substr(3);
    }
    
    const char *cp = str.c_str();
    int numAlpha = 0;

    while (isalpha (*cp)) {
        numAlpha++;
        cp++;
    }

    int numUndersc = 0;

    while (*cp == '_') {
        numUndersc++;
        cp++;
    }

    int numDigits = 0;
    while (isdigit (*cp)) {
        numDigits++;
        cp++;
    }

    if ((*cp != '\0' && *cp != ' ' && *cp != '.') || numUndersc > 1) {
        return eAccessionFormat_wrong_number_of_digits;
    }

    EAccessionFormatError rval = eAccessionFormat_valid;

    if (require_version) {
        if (*cp != '.') {
            rval = eAccessionFormat_missing_version;
        }
        cp++;
        int numVersion = 0;
        while (isdigit (*cp)) {
            numVersion++;
            cp++;
        }
        if (numVersion < 1) {
            rval = eAccessionFormat_missing_version;
        } else if (*cp != '\0' && *cp != ' ') {
            rval = eAccessionFormat_bad_version;
        }
    }


    if (numUndersc == 0) {
        if ((numAlpha == 1 && numDigits == 5) 
            || (numAlpha == 2 && numDigits == 6)
            || (numAlpha == 3 && numDigits == 5)
            || (numAlpha == 4 && numDigits == 8)
            || (numAlpha == 5 && numDigits == 7)) {
            return rval;
        } 
    } else if (numUndersc == 1) {
        if (numAlpha != 2 || (numDigits != 6 && numDigits != 8 && numDigits != 9)) {
            return eAccessionFormat_wrong_number_of_digits;
        }
        char first_letter = accession.c_str()[0];
        char second_letter = accession.c_str()[1];
        if (first_letter == 'N' || first_letter == 'X' || first_letter == 'Z') { 
            if (second_letter == 'M' || second_letter == 'C'
                || second_letter == 'T' || second_letter == 'P'
                || second_letter == 'G' || second_letter == 'R'
                || second_letter == 'S' || second_letter == 'W'
                || second_letter == 'Z') {
                return rval;
            }
        }
        if ((first_letter == 'A' || first_letter == 'Y')
            && second_letter == 'P') {
            return rval;
        }
    }

    return eAccessionFormat_wrong_number_of_digits;
}


bool s_FeatureIdsMatch (const CFeat_id& f1, const CFeat_id& f2)
{
    if (!f1.IsLocal() || !f2.IsLocal()) {
        return false;
    } else if (f1.GetLocal().IsStr() && f2.GetLocal().IsStr()
               && NStr::EqualNocase(f1.GetLocal().GetStr(), f2.GetLocal().GetStr())) {
        return true;
    } else if (f1.GetLocal().IsId() && f2.GetLocal().IsId()
               && f1.GetLocal().GetId() == f2.GetLocal().GetId()) {
        return true;
    } else {
        return false;
    }
}


bool s_StringHasPMID (string str)
{
    if (NStr::IsBlank (str)) {
        return false;
    }

    size_t pos = NStr::Find (str, "(PMID ");
    if (pos == string::npos) {
        return false;
    }

    const char *ptr = str.c_str() + pos + 6;
    unsigned int numdigits = 0;
    while (*ptr != 0 && *ptr != ')') {
        if (isdigit (*ptr)) {
            numdigits++;
        }
        ptr++;
    }

    if (*ptr == ')' && numdigits > 0) {
        return true;
    } else {
        return false;
    }
}


bool HasBadCharacter (string str)
{
    if (NStr::Find (str, "?") != string::npos
        || NStr::Find (str, "!") != string::npos
        || NStr::Find (str, "~") != string::npos) {
        return true;
    } else {
        return false;
    }
}


bool EndsWithBadCharacter (string str)
{
    if (NStr::EndsWith (str, "_") || NStr::EndsWith (str, ".") 
        || NStr::EndsWith (str, ",") || NStr::EndsWith (str, ":")
        || NStr::EndsWith (str, ";")) {
        return true;
    } else {
        return false;
    }
}


bool IsBioseqWithIdInSet (const CSeq_id& id, CBioseq_set_Handle set)
{
    bool found = false;
    for (CBioseq_CI b_ci (set);
         b_ci && !found;
         ++b_ci) {
        FOR_EACH_SEQID_ON_BIOSEQ (id_it, *(b_ci->GetCompleteBioseq())) {
            if (id.Compare(**id_it) == CSeq_id::e_YES) {
                found = true;
                break;
            }
        }
    }
    return found;
}


int CheckDate (const CDate& date, bool require_full_date)
{
    int rval = eDateValid_valid;

    if (date.IsStr()) {
        if (NStr::IsBlank (date.GetStr()) || NStr::Equal (date.GetStr(), "?")) {
            rval |= eDateValid_bad_str;
        }
    } else if (date.IsStd()) {
        if (!date.GetStd().IsSetYear() || date.GetStd().GetYear() == 0) {
            rval |= eDateValid_bad_year;
        }
        if (date.GetStd().IsSetMonth() && date.GetStd().GetMonth() > 12) {
            rval |= eDateValid_bad_month;
        }
        if (date.GetStd().IsSetDay() && date.GetStd().GetDay() > 31) {
            rval |= eDateValid_bad_day;
        }
        if (require_full_date) {
            if (!date.GetStd().IsSetMonth() || date.GetStd().GetMonth() == 0) {
                rval |= eDateValid_bad_month;
            }
            if (!date.GetStd().IsSetDay() || date.GetStd().GetDay() == 0) {
                rval |= eDateValid_bad_day;
            }
        }
        if (date.GetStd().IsSetSeason() && !NStr::IsBlank (date.GetStd().GetSeason())) {
            const char * cp = date.GetStd().GetSeason().c_str();
            while (*cp != 0) {
                if (isalpha (*cp) || *cp == '-') {
                    // these are the only acceptable characters
                } else {
                    rval |= eDateValid_bad_season;
                    break;
                }
                ++cp;
            }
        }
    } else {
        rval |= eDateValid_bad_other;
    }
    return rval;
}


string GetDateErrorDescription (int flags)
{
    string reasons = "";

    if (flags & eDateValid_empty_date) {
        reasons += "EMPTY_DATE ";
    }
    if (flags & eDateValid_bad_str) {
        reasons += "BAD_STR ";
    }
    if (flags & eDateValid_bad_year) {
        reasons += "BAD_YEAR ";
    }
    if (flags & eDateValid_bad_month) {
        reasons += "BAD_MONTH ";
    }
    if (flags & eDateValid_bad_day) {
        reasons += "BAD_DAY ";
    }
    if (flags & eDateValid_bad_season) {
        reasons += "BAD_SEASON ";
    }
    if (flags & eDateValid_bad_other) {
        reasons += "BAD_OTHER ";
    }
    return reasons;
}


bool IsBioseqTSA (const CBioseq& seq, CScope* scope) 
{
    if (!scope) {
        return false;
    }
    bool is_tsa = false;
    CBioseq_Handle bsh = scope->GetBioseqHandle(seq);
    if (bsh) {
        CSeqdesc_CI desc_ci(bsh, CSeqdesc::e_Molinfo);
        while (desc_ci && !is_tsa) {
            if (desc_ci->GetMolinfo().IsSetTech() && desc_ci->GetMolinfo().GetTech() == CMolInfo::eTech_tsa) {
                is_tsa = true;
            }
            ++desc_ci;
        }
    }
    return is_tsa;
}


string GetAuthorsString (const CAuth_list& auth_list)
{
    string auth_str = "";

    if (!auth_list.IsSetNames()) {
        return auth_str;
    }

    vector<string> name_list;

    if (auth_list.GetNames().IsStd()) {
        ITERATE (CAuth_list::TNames::TStd, auth_it, auth_list.GetNames().GetStd()) {
            if ((*auth_it)->IsSetName()) {
                string label;
                (*auth_it)->GetName().GetLabel(&label);
                name_list.push_back(label);
            }
        }
    } else if (auth_list.GetNames().IsMl()) {
        ITERATE (CAuth_list::TNames::TMl, auth_it, auth_list.GetNames().GetMl()) {
            name_list.push_back((*auth_it));
        }
    } else if (auth_list.GetNames().IsStr()) {
        ITERATE (CAuth_list::TNames::TStr, auth_it, auth_list.GetNames().GetStr()) {
            name_list.push_back((*auth_it));
        }
    }

    if (name_list.size() == 0) {
        return auth_str;
    }

    auth_str = name_list.back();
    name_list.pop_back();
    if (name_list.size() > 0) {
        auth_str = "and " + auth_str;
        if (name_list.size() == 1) {
            auth_str = name_list.front() + auth_str;
        } else {        
            while (name_list.size() > 0) {
                string this_name = name_list.back();
                name_list.pop_back();
                auth_str = this_name + ", " + auth_str;
            }
        }
    }

    return auth_str;
}


static string s_GetAuthorsString (const CPubdesc& pd) 
{
    string authors_string = "";

    FOR_EACH_PUB_ON_PUBDESC (pub, pd) {
        if ((*pub)->IsSetAuthors()) {
            authors_string = GetAuthorsString ((*pub)->GetAuthors());
            break;
        }
    }
    return authors_string;
}


void GetPubdescLabels 
(const CPubdesc& pd, 
 vector<int>& pmids, vector<int>& muids, vector<int>& serials,
 vector<string>& published_labels, vector<string>& unpublished_labels)
{
    string label = "";
    bool   is_published = false;
    bool   need_label = false;
    FOR_EACH_PUB_ON_PUBDESC (it, pd) {
        if ((*it)->IsPmid()) {
            pmids.push_back ((*it)->GetPmid());
            is_published = true;
        } else if ((*it)->IsMuid()) {
            muids.push_back ((*it)->GetMuid());
            is_published = true;
        } else if ((*it)->IsGen()) {
            if ((*it)->GetGen().IsSetCit() 
                && NStr::StartsWith ((*it)->GetGen().GetCit(), "BackBone id_pub", NStr::eNocase)) {
                need_label = true;
            }
            if ((*it)->GetGen().IsSetSerial_number()) {
                serials.push_back ((*it)->GetGen().GetSerial_number());
                if ((*it)->GetGen().IsSetCit() 
                    || (*it)->GetGen().IsSetJournal()
                    || (*it)->GetGen().IsSetDate()) {
                    need_label = true;
                }
            } else {
                need_label = true;
            }
        } else {
            need_label = true;
        }
        if (need_label && NStr::IsBlank(label)) {
            // create unique label
            (*it)->GetLabel(&label, CPub::eContent, true);
            label += "; " + s_GetAuthorsString (pd);
        }
    }
    if (!NStr::IsBlank(label)) {
        if (is_published) {
            published_labels.push_back(label);
        } else {
            unpublished_labels.push_back(label);
        }
    }
}


bool IsNCBIFILESeqId (const CSeq_id& id)
{
    if (!id.IsGeneral() || !id.GetGeneral().IsSetDb()
        || !NStr::Equal(id.GetGeneral().GetDb(), "NCBIFILE")) {
        return false;
    } else {
        return true;
    }
}


bool IsRefGeneTrackingObject (const CUser_object& user)
{
    bool rval = false;

    if (user.IsSetType()) {
        const CObject_id& oi = user.GetType();
	    if (oi.IsStr() && NStr::EqualNocase(oi.GetStr(), "RefGeneTracking")) {
            rval = true;
        }
    }
    return rval;
}


string GetValidatorLocationLabel (const CSeq_loc& loc)
{
    string loc_label = "";
    loc.GetLabel(&loc_label);
    NStr::ReplaceInPlace(loc_label, ":plus", ":");
    NStr::ReplaceInPlace(loc_label, ", plus", ", ");
    NStr::ReplaceInPlace(loc_label, ":both", ":");
    NStr::ReplaceInPlace(loc_label, ", both", ", ");
    NStr::ReplaceInPlace(loc_label, ":minus", ":c");
    NStr::ReplaceInPlace(loc_label, ", minus", ", c");
    NStr::ReplaceInPlace(loc_label, "[", "(");
    NStr::ReplaceInPlace(loc_label, "]", ")");
    return loc_label;
}



string GetBioseqIdLabel(const CBioseq& sq, bool limited)
{
    string content = "";
    int num_ids_found = 0;
    bool id_found = false;

    /* find first gi */
    FOR_EACH_SEQID_ON_BIOSEQ (id_it, sq) {
        if ((*id_it)->IsGi()) {
            CNcbiOstrstream os;
            (*id_it)->WriteAsFasta(os);
            string s = CNcbiOstrstreamToString(os);
            content += s;
            num_ids_found ++;
            break;
        }
    }
    /* find first genbank */
    FOR_EACH_SEQID_ON_BIOSEQ (id_it, sq) {
        if ((*id_it)->IsGenbank()) {
            if (num_ids_found > 0) {
                content += "|";
            }
            CNcbiOstrstream os;
            (*id_it)->WriteAsFasta(os);
            string s = CNcbiOstrstreamToString(os);
            content += s;
            num_ids_found++;
            break;
        }
    }

    if (!limited || num_ids_found == 0) {
        /* find first general */
        FOR_EACH_SEQID_ON_BIOSEQ (id_it, sq) {
            if ((*id_it)->IsGeneral()) {
                if (num_ids_found > 0) {
                    content += "|";
                }
                CNcbiOstrstream os;
                (*id_it)->WriteAsFasta(os);
                string s = CNcbiOstrstreamToString(os);
                content += s;
                num_ids_found++;
                break;
            }
        }
    }
    // didn't find any?  print them all, but only the first local
    if (num_ids_found == 0) {
        bool found_local = false;
        FOR_EACH_SEQID_ON_BIOSEQ (id_it, sq) {
            if ((*id_it)->IsLocal()) {
                if (found_local) {
                    continue;
                } else {
                    found_local = true;
                }
            }
            if (id_found) {
                content += "|";
            }
            CNcbiOstrstream os;
            (*id_it)->WriteAsFasta(os);
            string s = CNcbiOstrstreamToString(os);
            content += s;
            id_found = true;
        }
    }

    return content;
}


void AppendBioseqLabel(string& str, const CBioseq& sq, bool supress_context)
{
    str += "BIOSEQ: ";

    string content = GetBioseqIdLabel (sq);

    if (!supress_context) {
        if (!content.empty()) {
            content += ": ";
        }

        const CEnumeratedTypeValues* tv;
        tv = CSeq_inst::GetTypeInfo_enum_ERepr();
        content += tv->FindName(sq.GetInst().GetRepr(), true) + ", ";
        tv = CSeq_inst::GetTypeInfo_enum_EMol();
        content += tv->FindName(sq.GetInst().GetMol(), true);
        if (sq.GetInst().IsSetLength()) {
            content += string(" len= ") + NStr::IntToString(sq.GetInst().GetLength());
        }
    }
    str += content;
}

bool HasECnumberPattern (const string& str)
{
    bool rval = false;
    if (NStr::IsBlank(str)) {
        return false;
    }

    bool is_ambig = false;
    int  numdashes = 0;
    int  numdigits = 0;
    int  numperiods = 0;

    string::const_iterator sit = str.begin();
    while (sit != str.end() && !rval) {
        if (isdigit (*sit)) {
            numdigits++;
            if (is_ambig) {
                is_ambig = false;
                numperiods = 0;
                numdashes = 0;
            }
        } else if (*sit == '-') {
            numdashes++;
            is_ambig = true;
        } else if (*sit == 'n') {
            numdashes++;
            is_ambig = true;
        } else if (*sit == '.') {
            numperiods++;
            if (numdigits > 0 && numdashes > 0) {
                is_ambig = false;
                numperiods = 0;
                numdigits = 0;
                numdashes = 0;
            } else if (numdigits == 0 && numdashes == 0) {
                is_ambig = false;
                numperiods = 0;
                numdigits = 0;
                numdashes = 0;
            } else if (numdashes > 1) {
                is_ambig = false;
                numperiods = 0;
                numdigits = 0;
                numdashes = 0;
            }
            numdigits = 0;
            numdashes = 0;
        } else {
            if (numperiods == 3) {
                if (numdigits > 0 && numdashes > 0) {
                    is_ambig = false;
                    numperiods = 0;
                    numdigits = 0;
                    numdashes = 0;
                } else if (numdigits > 0 || numdashes == 1) {
                    rval = true;
                }
            }
            is_ambig = false;
            numperiods = 0;
            numdigits = 0;
            numdashes = 0;
        }
        ++sit;
    }
    if (numperiods == 3) {
        if (numdigits > 0 && numdashes > 0) {
            rval = false;
        } else if (numdigits > 0 || numdashes == 1) {
            rval = true;
        }
    }
    return rval;
}


bool SeqIsPatent (const CBioseq& seq)
{
    bool is_patent = false;

    // some tests are suppressed if a patent ID is present
    FOR_EACH_SEQID_ON_BIOSEQ (id_it, seq) {
        if ((*id_it)->IsPatent()) {
            is_patent = true;
            break;
        }
    }
    return is_patent;
}


bool SeqIsPatent (CBioseq_Handle seq)
{
    return SeqIsPatent (*(seq.GetCompleteBioseq()));
}


bool s_PartialAtGapOrNs (
    CScope* scope,
    const CSeq_loc& loc,
    unsigned int tag
)

{
    if ( tag != sequence::eSeqlocPartial_Nostart && tag != sequence::eSeqlocPartial_Nostop ) {
        return false;
    }

    CSeq_loc_CI first, last;
    for ( CSeq_loc_CI sl_iter(loc); sl_iter; ++sl_iter ) { // EQUIV_IS_ONE not supported
        if ( !first ) {
            first = sl_iter;
        }
        last = sl_iter;
    }

    if ( first.GetStrand() != last.GetStrand() ) {
        return false;
    }
    CSeq_loc_CI temp = (tag == sequence::eSeqlocPartial_Nostart) ? first : last;

    if (!scope) {
        return false;
    }

    CConstRef<CSeq_loc> slp = temp.GetRangeAsSeq_loc();
    if (!slp) {
        return false;
    }
    const CSeq_id* id = slp->GetId();
    if (id == NULL) return false;
    CBioseq_Handle bsh = scope->GetBioseqHandle(*id);
    if (!bsh) {
        return false;
    }
    
    TSeqPos acceptor = temp.GetRange().GetFrom();
    TSeqPos donor = temp.GetRange().GetTo();
    TSeqPos start = acceptor;
    TSeqPos stop = donor;

    CSeqVector vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac,
        temp.GetStrand());
    TSeqPos len = vec.size();

    if ( temp.GetStrand() == eNa_strand_minus ) {
        swap(acceptor, donor);
        stop = len - donor - 1;
        start = len - acceptor - 1;
    }

    bool result = false;

    try {
        if (tag == sequence::eSeqlocPartial_Nostop && stop < len - 1 && vec.IsInGap(stop + 1)) {
            return true;
        } else if (tag == sequence::eSeqlocPartial_Nostart && start > 0 && vec.IsInGap(start - 1)) {
            return true;
        }
    } catch ( exception& ) {
        
        return false;
    }

    if ( (tag == sequence::eSeqlocPartial_Nostop)  &&  (stop < len - 2) ) {
        try {
            CSeqVector::TResidue res = vec[stop + 1];

            if ( IsResidue(res)  &&  isalpha (res)) {
                if ( res == 'N' ) {
                    result = true;
                }
            }
        } catch ( exception& ) {
            return false;
        }
    } else if ( (tag == sequence::eSeqlocPartial_Nostart)  &&  (start > 1) ) {
        try {
            CSeqVector::TResidue res = vec[start - 1];
        
            if ( IsResidue(res)  &&  isalpha (res)) {
                if ( res == 'N' ) {
                    result = true;
                }
            }
        } catch ( exception& ) {
            return false;
        }
    }

    return result;    
}


CBioseq_Handle BioseqHandleFromLocation (CScope* m_Scope, const CSeq_loc& loc)

{
    CBioseq_Handle bsh;
    for ( CSeq_loc_CI citer (loc); citer; ++citer) {
        const CSeq_id& id = citer.GetSeq_id();
        CSeq_id_Handle sih = CSeq_id_Handle::GetHandle(id);
        bsh = m_Scope->GetBioseqHandle (sih, CScope::eGetBioseq_All);
        if (bsh) {
            return bsh;
        }
    }
    return bsh;
}


bool s_PosIsNNotGap(CSeqVector& vec, int pos)
{
    if (pos >= vec.size()) {
        return false;
    } else if (vec[pos] != 'N' && vec[pos] != 'n') {
        return false;
    } else if (vec.IsInGap(pos)) {
        return false;
    } else {
        return true;
    }
}


void CheckBioseqEndsForNAndGap 
(CBioseq_Handle bsh,
 EBioseqEndIsType& begin_n,
 EBioseqEndIsType& begin_gap,
 EBioseqEndIsType& end_n,
 EBioseqEndIsType& end_gap)
{
    begin_n = eBioseqEndIsType_None;
    begin_gap = eBioseqEndIsType_None;
    end_n = eBioseqEndIsType_None;
    end_gap = eBioseqEndIsType_None;

    try {
        if (!bsh || bsh.GetInst_Length() < 10 || (bsh.IsSetInst_Topology() && bsh.GetInst_Topology() == CSeq_inst::eTopology_circular)) {
            return;
        }

        // check for gap at begining of sequence
        CSeqVector vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
        if (vec.IsInGap(0) /* || vec.IsInGap(1) */ ) {
            begin_gap = eBioseqEndIsType_All;
            for (int i = 0; i < 10; i++) {
                if (!vec.IsInGap(i)) {
                    begin_gap = eBioseqEndIsType_Last;
                    break;
                }
            }
        }

        // check for gap at end of sequence
        if ( /* vec.IsInGap (vec.size() - 2) || */ vec.IsInGap (vec.size() - 1)) {
            end_gap = eBioseqEndIsType_All;
            for (int i = vec.size() - 11; i < vec.size(); i++) {
                if (!vec.IsInGap(i)) {
                    end_gap = eBioseqEndIsType_Last;
                    break;
                }
            }
        }

        if (bsh.IsNa()) {
            // check for N bases at beginning of sequence
            if (s_PosIsNNotGap(vec, 0) /* || s_PosIsNNotGap(vec, 1) */ ) {
                begin_n = eBioseqEndIsType_All;
                for (int i = 0; i < 10; i++) {
                    if (!s_PosIsNNotGap(vec, i)) {
                        begin_n = eBioseqEndIsType_Last;
                        break;
                    }
                }
            }

            // check for N bases at end of sequence
            if ( /* s_PosIsNNotGap(vec, vec.size() - 2) || */ s_PosIsNNotGap(vec, vec.size() - 1)) {
                end_n = eBioseqEndIsType_All;
                for (int i = vec.size() - 10; i < vec.size(); i++) {
                    if (!s_PosIsNNotGap(vec, i)) {
                        end_n = eBioseqEndIsType_Last;
                        break;
                    }
                }
            }
        }
    } catch ( exception& ) {
        // if there are exceptions, cannot perform this calculation
    }
}


bool IsLocFullLength (const CSeq_loc& loc, const CBioseq_Handle& bsh)
{
    if (loc.IsInt() 
        && loc.GetInt().GetFrom() == 0
        && loc.GetInt().GetTo() == bsh.GetInst_Length() - 1) {
        return true;
    } else {
        return false;
    }
}


CConstRef <CSeq_feat> GetGeneForFeature (const CSeq_feat& f1, CScope *scope)
{
    const CGene_ref * g1 = f1.GetGeneXref();
    CConstRef <CSeq_feat> gene;

    if (g1) {
        if (g1->IsSuppressed()) {
            return gene;
        }
        string ref_label;
        g1->GetLabel(&ref_label);

        CBioseq_Handle bsh = BioseqHandleFromLocation(scope, f1.GetLocation());
        SAnnotSelector sel(CSeqFeatData::e_Gene);
        CFeat_CI gene_it(bsh, sel);
        while (gene_it) {
            string feat_label;
            feature::GetLabel(gene_it->GetOriginalFeature(), &feat_label, feature::fFGL_Content, scope);
            if (NStr::EqualCase(ref_label, feat_label)) {
                gene.Reset (&(gene_it->GetOriginalFeature()));
                return gene;
            }
            ++gene_it;
        }
        return gene;
    }

    return sequence::GetOverlappingGene (f1.GetLocation(), *scope);
}


bool PartialsSame (const CSeq_loc& loc1, const CSeq_loc& loc2)
{
    bool loc1_partial_start =
        loc1.IsPartialStart(eExtreme_Biological);
    bool loc1_partial_stop =
        loc1.IsPartialStop(eExtreme_Biological);
    bool loc2_partial_start =
        loc2.IsPartialStart(eExtreme_Biological);
    bool loc2_partial_stop =
        loc2.IsPartialStop(eExtreme_Biological);
    if (loc1_partial_start == loc2_partial_start  &&
        loc1_partial_stop == loc2_partial_stop) {
        return true;
    } else {
        return false;
    }
}




// Code for finding duplicate features
bool s_IsSameStrand(const CSeq_loc& l1, const CSeq_loc& l2, CScope& scope)
{
    ENa_strand s1 = sequence::GetStrand(l1, &scope);
    ENa_strand s2 = sequence::GetStrand(l2, &scope);
    if ((s1 == eNa_strand_minus && s2 == eNa_strand_minus)
        || (s1 != eNa_strand_minus && s2 != eNa_strand_minus)) {
        return true;
    } else {
        return false;
    }
}


inline
bool s_IsSameSeqAnnot(const CSeq_feat_Handle& f1, const CSeq_feat_Handle& f2, bool& diff_descriptions)
{
    bool rval = f1.GetAnnot() == f2.GetAnnot();
    diff_descriptions = false;
    if (!rval) {
        if (!f1.GetAnnot().Seq_annot_IsSetDesc() && !f2.GetAnnot().Seq_annot_IsSetDesc()) {
            // neither is set
            diff_descriptions = false;
        } else if (f1.GetAnnot().Seq_annot_IsSetDesc() && f2.GetAnnot().Seq_annot_IsSetDesc()) {
            // both are set - are they different?
            const CAnnot_descr& desc1 = f1.GetAnnot().Seq_annot_GetDesc();
            const CAnnot_descr& desc2 = f2.GetAnnot().Seq_annot_GetDesc();
            if (desc1.Get().front()->Which() != desc2.Get().front()->Which()) {
                diff_descriptions = true;
            } else {
                if (desc1.Get().front()->IsName() 
                    && NStr::EqualNocase(desc1.Get().front()->GetName(), desc2.Get().front()->GetName())) {
                    diff_descriptions = false;
                } else if (desc1.Get().front()->IsTitle()
                    && NStr::EqualNocase(desc1.Get().front()->GetTitle(), desc2.Get().front()->GetTitle())) {
                    diff_descriptions = false;
                } else {
                    diff_descriptions = true;
                }
            }
        } else {
            diff_descriptions = true;
        }
    }
    return rval;
}


bool s_AreGBQualsIdentical(const CSeq_feat& feat1, const CSeq_feat& feat2)
{
    bool rval = true;

    if (!feat1.IsSetQual()) {
        if (!feat2.IsSetQual()) {
            return true;
        } else {
            return false;
        }
    } else if (!feat2.IsSetQual()) {
        return false;
    } else {
        CSeq_feat::TQual::const_iterator gb1 = feat1.GetQual().begin();
        CSeq_feat::TQual::const_iterator gb1_end = feat1.GetQual().end();
        CSeq_feat::TQual::const_iterator gb2 = feat2.GetQual().begin();
        CSeq_feat::TQual::const_iterator gb2_end = feat2.GetQual().end();

        while ((gb1 != gb1_end) && (gb2 != gb2_end) && rval) {
            if (!(*gb1)->IsSetQual()) {
                if ((*gb2)->IsSetQual()) {
                    rval = false;
                }
            } else if (!(*gb2)->IsSetQual()) {
                rval = false;
            } else if (!NStr::Equal ((*gb1)->GetQual(), (*gb2)->GetQual())) {
                rval = false;
            }
            if (!(*gb1)->IsSetVal()) {
                if ((*gb2)->IsSetVal()) {
                    rval = false;
                }
            } else if (!(*gb2)->IsSetVal()) {
                rval = false;
            } else if (!NStr::Equal ((*gb1)->GetVal(), (*gb2)->GetVal())) {
                rval = false;
            }
            ++gb1;
            ++gb2;
        }
        if (gb1 != gb1_end || gb2 != gb2_end) {
            rval = false;
        }
    }
    return rval;
}


bool s_AreFeatureLabelsSame(const CSeq_feat& feat, const CSeq_feat& prev, CScope *scope)
{
    // compare labels and comments
    bool same_label = true;
    const string& curr_comment =
        feat.IsSetComment() ? feat.GetComment() : kEmptyStr;
    const string& prev_comment =
        prev.IsSetComment() ? prev.GetComment() : kEmptyStr;
    string curr_label = "";
    string prev_label = "";

    feature::GetLabel(feat,
        &curr_label, feature::fFGL_Content, scope);
    feature::GetLabel(prev,
        &prev_label, feature::fFGL_Content, scope);
    if (!NStr::EqualNocase(curr_comment, prev_comment) ||
        !NStr::EqualNocase(curr_label, prev_label) ) {
        same_label = false;
    } else if (!s_AreGBQualsIdentical(feat, prev)) {
        same_label = false;
    }
    return same_label;
}


bool s_IsDifferentDbxrefs(const TDbtags& list1, const TDbtags& list2)
{
    if (list1.empty()  ||  list2.empty()) {
        return false;
    } else if (list1.size() != list2.size()) {
        return true;
    }

    TDbtags::const_iterator it1 = list1.begin();
    TDbtags::const_iterator it2 = list2.begin();
    for (; it1 != list1.end(); ++it1, ++it2) {
        if (!NStr::EqualNocase((*it1)->GetDb(), (*it2)->GetDb())) {
            return true;
        }
        string str1 =
            (*it1)->GetTag().IsStr() ? (*it1)->GetTag().GetStr() : "";
        string str2 =
            (*it2)->GetTag().IsStr() ? (*it2)->GetTag().GetStr() : "";
        if ( str1.empty()  &&  str2.empty() ) {
            if (!(*it1)->GetTag().IsId()  &&  !(*it2)->GetTag().IsId()) {
                continue;
            } else if ((*it1)->GetTag().IsId()  &&  (*it2)->GetTag().IsId()) {
                if ((*it1)->GetTag().GetId() != (*it2)->GetTag().GetId()) {
                    return true;
                }
            } else {
                return true;
            }
        } else if (!str1.empty() && !str2.empty() && !NStr::EqualNocase(str1, str2)) {
            return true;
        }
    }
    return false;
}


bool s_AreFullLengthCodingRegionsWithDifferentFrames (CSeq_feat_Handle f1, CSeq_feat_Handle f2, CScope* scope)
{
    if (!f1.GetData().IsCdregion() || !f2.GetData().IsCdregion()) {
        return false;
    }

    int frame1 = 1, frame2 = 1;
    if (f1.GetData().GetCdregion().IsSetFrame()) {
        frame1 = f1.GetData().GetCdregion().GetFrame();
        if (frame1 == 0) {
            frame1 = 1;
        }
    }
    if (f2.GetData().GetCdregion().IsSetFrame()) {
        frame2 = f2.GetData().GetCdregion().GetFrame();
        if (frame2 == 0) {
            frame2 = 1;
        }
    }
    if (frame1 == frame2) {
        return false;
    }

    CBioseq_Handle bsh1 = BioseqHandleFromLocation(scope, f1.GetLocation());
    if (!IsLocFullLength (f1.GetLocation(), bsh1)) {
        return false;
    }
    CBioseq_Handle bsh2 = BioseqHandleFromLocation(scope, f2.GetLocation());
    if (!IsLocFullLength (f2.GetLocation(), bsh2)) {
        return false;
    }

    return true;
}


bool s_AreDifferentVariations (CSeq_feat_Handle f1, CSeq_feat_Handle f2)
{
    if (f1.GetData().GetSubtype() != CSeqFeatData::eSubtype_variation
        || f2.GetData().GetSubtype() != CSeqFeatData::eSubtype_variation) {
        return false;
    }
    if (!f1.IsSetQual() || !f2.IsSetQual()) {
        return false;
    }
    string replace1 = "";

    ITERATE(CSeq_feat::TQual, q, f1.GetQual()) {
        if ((*q)->IsSetQual() && NStr::Equal((*q)->GetQual(), "replace") && (*q)->IsSetVal()) {
            replace1 = (*q)->GetVal();
            if (!NStr::IsBlank(replace1)) {
                break;
            }
        }
    }

    string replace2 = "";

    ITERATE(CSeq_feat::TQual, q, f2.GetQual()) {
        if ((*q)->IsSetQual() && NStr::Equal((*q)->GetQual(), "replace") && (*q)->IsSetVal()) {
            replace2 = (*q)->GetVal();
            if (!NStr::IsBlank(replace2)) {
                break;
            }
        }
    }

    if (!NStr::IsBlank(replace1) && !NStr::Equal(replace1, replace2)) {
        return true;
    } else {
        return false;
    }
}




static bool s_AreLinkedToDifferentFeats (CSeq_feat_Handle f1, CSeq_feat_Handle f2, CSeqFeatData::ESubtype s1, CSeqFeatData::ESubtype s2)
{
    bool rval = false;
    
    if (f1.GetData().GetSubtype() == s1 && f2.GetData().GetSubtype() == s1) {
        CScope& scope = f1.GetScope();
        const CSeq_loc& loc = f1.GetLocation();
        CBioseq_Handle bsh = BioseqHandleFromLocation (&scope, loc);
        if (bsh) {
            const CTSE_Handle& tse = bsh.GetTSE_Handle();
            vector<int> mrna1;
            vector<int> mrna2;

            FOR_EACH_SEQFEATXREF_ON_SEQFEAT (itx, *(f1.GetSeq_feat())) {
                if ((*itx)->IsSetId() && (*itx)->GetId().IsLocal() 
                    && (*itx)->GetId().GetLocal().IsId()) {
                    int feat_id = (*itx)->GetId().GetLocal().GetId();
                    vector<CSeq_feat_Handle> handles = tse.GetFeaturesWithId(CSeqFeatData::e_not_set, feat_id);
                    ITERATE( vector<CSeq_feat_Handle>, feat_it, handles ) {
                        if (feat_it->IsSetData() 
                            && feat_it->GetData().GetSubtype() == s2) {
                            mrna1.push_back (feat_id);
                            break;
                        }
                    }
                }
            }
            FOR_EACH_SEQFEATXREF_ON_SEQFEAT (itx, *(f2.GetSeq_feat())) {
                if ((*itx)->IsSetId() && (*itx)->GetId().IsLocal() 
                    && (*itx)->GetId().GetLocal().IsId()) {
                    int feat_id = (*itx)->GetId().GetLocal().GetId();
                    vector<CSeq_feat_Handle> handles = tse.GetFeaturesWithId(CSeqFeatData::e_not_set, feat_id);
                    ITERATE( vector<CSeq_feat_Handle>, feat_it, handles ) {
                        if (feat_it->IsSetData() 
                            && feat_it->GetData().GetSubtype() == s2) {
                            mrna2.push_back (feat_id);
                        }
                    }
                }
            }

            if (mrna1.size() > 0 && mrna2.size() > 0) {
                rval = true;
                ITERATE (vector<int>, i1, mrna1) {
                    ITERATE (vector<int>, i2, mrna2) {
                        if (*i1 == *i2) {
                            rval = false;
                            break;
                        }
                    }
                    if (!rval) {
                        break;
                    }
                }
            }
        }
    }
    return rval;
}


static bool s_AreCodingRegionsLinkedToDifferentmRNAs (CSeq_feat_Handle f1, CSeq_feat_Handle f2)
{
    return s_AreLinkedToDifferentFeats (f1, f2, CSeqFeatData::eSubtype_cdregion, CSeqFeatData::eSubtype_mRNA);
}


bool s_AremRNAsLinkedToDifferentCodingRegions (CSeq_feat_Handle f1, CSeq_feat_Handle f2)
{
    return s_AreLinkedToDifferentFeats (f1, f2, CSeqFeatData::eSubtype_mRNA, CSeqFeatData::eSubtype_cdregion);
}


EDuplicateFeatureType 
IsDuplicate 
(CSeq_feat_Handle f1, 
 const CSeq_feat& feat1, 
 CSeq_feat_Handle f2, 
 const CSeq_feat& feat2, 
 CScope& scope)
{
    EDuplicateFeatureType dup_type = eDuplicate_Not;

    // subtypes
    CSeqFeatData::ESubtype feat1_subtype = feat1.GetData().GetSubtype();
    CSeqFeatData::ESubtype feat2_subtype = feat2.GetData().GetSubtype();
    // locations
    const CSeq_loc& feat1_loc = feat1.GetLocation();
    const CSeq_loc& feat2_loc = feat2.GetLocation();

    // if same subtype
    if (feat1_subtype == feat2_subtype) {
        // if same location and strand
        if (s_IsSameStrand(feat1_loc, feat2_loc, scope)  &&
            sequence::Compare(feat1_loc, feat2_loc, &scope) == sequence::eSame) {

            // same annot?
            bool diff_annot_desc = false;
            bool same_annot = s_IsSameSeqAnnot(f1, f2, diff_annot_desc);

            if (diff_annot_desc) {
                // don't report if features on different annots with different titles or names
            } else {
                // compare labels and comments
                bool same_label = s_AreFeatureLabelsSame (feat1, feat2, &scope);

                // Report duplicates
                if ( feat1_subtype == CSeqFeatData::eSubtype_region  &&
                    s_IsDifferentDbxrefs(feat1.GetDbxref(), feat2.GetDbxref()) ) {
                    // do not report if both have dbxrefs and they are 
                    // different.
                } else if (!same_label && s_AreFullLengthCodingRegionsWithDifferentFrames(f1, f2, &scope)) {
                    // do not report if both coding regions are full length, have different products,
                    // and have different frames
                } else if (s_AreDifferentVariations(f1, f2)) {
                    // don't report variations if replace quals are different
                } else if (s_AreCodingRegionsLinkedToDifferentmRNAs(f1, f2)) {
                    // do not report if features are coding regions linked to different mRNAs
                } else if (s_AremRNAsLinkedToDifferentCodingRegions(f1, f2)) {
                    // do not report if features are mRNAs linked to different coding regions
                } else if ( same_annot ) {
                    if (same_label) {
                        dup_type = eDuplicate_Duplicate;
                    } else if ( feat1_subtype != CSeqFeatData::eSubtype_pub ) {
                        if (PartialsSame(feat1_loc, feat2_loc)) {
                            // do not report if partial flags are different
                            dup_type = eDuplicate_SameIntervalDifferentLabel;
                        }
                    }
                } else {
                    if (same_label) {
                        dup_type = eDuplicate_DuplicateDifferentTable;
                    } else if ( feat2_subtype != CSeqFeatData::eSubtype_pub ) {
                        dup_type = eDuplicate_SameIntervalDifferentLabelDifferentTable;
                    }
                }
            }
        }
    }
    return dup_type;        
}

// specific-host functions

bool IsCommonName (const CT3Data& data)
{
	bool is_common = false;
	
	if (data.IsSetStatus()) {
		ITERATE (CT3Reply::TData::TStatus, status_it, data.GetStatus()) {
			if ((*status_it)->IsSetProperty() 
				&& NStr::Equal((*status_it)->GetProperty(), "old_name_class", NStr::eNocase)) {
				if ((*status_it)->IsSetValue() && (*status_it)->GetValue().IsStr()) {
					string value_str = (*status_it)->GetValue().GetStr();
					if (NStr::Equal(value_str, "common name", NStr::eCase) 
						|| NStr::Equal(value_str, "genbank common name", NStr::eCase)) {
						is_common = true;
                        break;
					}
				}
			}
		}
	}
	return is_common;
}

bool HasMisSpellFlag (const CT3Data& data)
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


bool FindMatchInOrgRef (string str, const COrg_ref& org)
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
	return NStr::EqualCase(str, match);
}


static const string sIgnoreHostWordList[] = {
  "cf.",
  "cf ",
  "aff ",
  "aff.",
  "near",
  "nr.",
  "nr "
};


static const int kNumIgnoreHostWordList = sizeof (sIgnoreHostWordList) / sizeof (string);

void AdjustSpecificHostForTaxServer (string& spec_host)
{
    for (int i = 0; i < kNumIgnoreHostWordList; i++) {
        NStr::ReplaceInPlace(spec_host, sIgnoreHostWordList[i], "");
    }
    NStr::ReplaceInPlace(spec_host, "  ", " ");
    NStr::TruncateSpacesInPlace(spec_host);
}


string SpecificHostValueToCheck(const string& val)
{
    if (NStr::IsBlank(val)) {
        return val;
    } else if (! isupper (val.c_str()[0])) {
        return "";
    }
    string host = val;
    AdjustSpecificHostForTaxServer(host);
    size_t pos = NStr::Find(host, " ");
    if (pos != string::npos) {
        if (NStr::StartsWith(host.substr(pos + 1), "hybrid ")) {
            pos += 7;
        } else if (NStr::StartsWith(host.substr(pos + 1), "x ")) {
            pos += 2;
        }
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
    return host;
}


string InterpretSpecificHostResult(const string& host, const CT3Reply& reply)
{
    string err_str = "";
	if (reply.IsError()) {
		err_str = "?";
        if (reply.GetError().IsSetMessage()) {
            err_str = reply.GetError().GetMessage();
        }
        if(NStr::Find(err_str, "ambiguous") != string::npos) {
            err_str = "Specific host value is ambiguous: " + host;
        } else {
            err_str = "Invalid value for specific host: " + host;
        }
	} else if (reply.IsData()) {
		if (HasMisSpellFlag(reply.GetData())) {
            err_str = "Specific host value is misspelled: " + host;
		} else if (reply.GetData().IsSetOrg()) {
			if ( ! FindMatchInOrgRef (host, reply.GetData().GetOrg()) && ! IsCommonName(reply.GetData())) {
                err_str = "Specific host value is incorrectly capitalized: " + host;
			}
		} else {
			err_str = "Invalid value for specific host: " + host;
		}
	}
    return err_str;
}


bool IsSpecificHostValid(const string& val, string& error_msg)
{
	bool is_valid = true;
	error_msg = kEmptyStr;
	
    // only check host values that start with a capital letter and have at least two words
    string host = SpecificHostValueToCheck(val);
    if (!NStr::IsBlank(host)) {
	    vector<CRef<COrg_ref> > org_req_list;
	    CRef<COrg_ref> req(new COrg_ref());
	    req->SetTaxname(host);
	    org_req_list.push_back(req);
	
	    CTaxon3 taxon3;
	    taxon3.Init();
	    CRef<CTaxon3_reply> reply = taxon3.SendOrgRefList(org_req_list);
	    if (reply && reply->GetReply().size() == 1) {
		    CTaxon3_reply::TReply::const_iterator reply_it = reply->GetReply().begin();
            error_msg = InterpretSpecificHostResult(host, **reply_it);
            if (!NStr::IsBlank(error_msg)) {
                is_valid = false;
            }
	    } else {
		    error_msg = "Invalid value for specific host: " + host;
	    }
    }
	
	return is_valid;
}

string FixSpecificHost(const string& val)
{
	string hostfix = val;
	if (NStr::IsBlank(val)) {
		return hostfix;
	}
	
	string host = val;
	
	AdjustSpecificHostForTaxServer(host);
	vector<CRef<COrg_ref> > org_req_list;
	CRef<COrg_ref> req(new COrg_ref());
	req->SetTaxname(host);
	org_req_list.push_back(req);
	
	CTaxon3 taxon3;
	taxon3.Init();
	CRef<CTaxon3_reply> reply = taxon3.SendOrgRefList(org_req_list);
	bool corrected = false;
	if (reply && reply->GetReply().size() == 1) {
		CTaxon3_reply::TReply::const_iterator reply_it = reply->GetReply().begin();
		if ((*reply_it)->IsError()) {
			hostfix = kEmptyStr;
		} else if ((*reply_it)->IsData()) {
			if (HasMisSpellFlag((*reply_it)->GetData()) && (*reply_it)->GetData().IsSetOrg()) {
				hostfix = (*reply_it)->GetData().GetOrg().GetTaxname();
				corrected = true;
			} else if ((*reply_it)->GetData().IsSetOrg()) {
				if (! FindMatchInOrgRef(host, (*reply_it)->GetData().GetOrg())
					&& ! IsCommonName((*reply_it)->GetData())) {
						hostfix = (*reply_it)->GetData().GetOrg().GetTaxname();
						corrected = true;
					}
			} else  {
				hostfix = kEmptyStr;
			}
		}
	}
	if ( !corrected && ! NStr::IsBlank(hostfix)) {
		NStr::ReplaceInPlace(hostfix, "  ", " ");
		NStr::TruncateSpacesInPlace(hostfix);
	}	
	
	return hostfix;
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
