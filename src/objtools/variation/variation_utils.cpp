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
* Author:  Igor Filippov
*
* File Description:
*   Simple library to correct reference allele in Variation object
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <sstream>
#include <iomanip>

#include <cstdlib>
#include <iostream>
#include <algorithm>

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objectio.hpp>
#include <serial/streamiter.hpp>
#include <util/compress/stream_util.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objects/variation/Variation.hpp>
#include <objects/variation/VariantPlacement.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/misc/sequence_macros.hpp>

#include <util/line_reader.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <connect/services/neticache_client.hpp>
#include <corelib/rwstream.hpp>

#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/seq_loc_util.hpp>

#include <objtools/variation/variation_utils.hpp>

void CVariationUtilities::CorrectRefAllele(CRef<CVariation>& v, CScope& scope)
{
    string old_ref;
    string new_ref;
    for (CVariation::TData::TSet::TVariations::iterator v1 = v->SetData().SetSet().SetVariations().begin(); v1 != v->SetData().SetSet().SetVariations().end(); ++v1)
        for (CVariation::TData::TSet::TVariations::iterator inst = (*v1)->SetData().SetSet().SetVariations().begin(); inst != (*v1)->SetData().SetSet().SetVariations().end(); ++inst)
            if ((*inst)->GetData().GetInstance().IsSetDelta() && !(*inst)->GetData().GetInstance().GetDelta().empty() && (*inst)->GetData().GetInstance().GetDelta().front()->IsSetSeq() 
                && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().IsLiteral()
                && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().IsSetSeq_data() 
                && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().IsIupacna() 
                && (*inst)->GetData().GetInstance().GetType() == CVariation_inst::eType_identity
                )
            {
                old_ref = (*inst)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set();
                new_ref = GetRefAlleleFromVP(v->SetData().SetSet().SetVariations().front()->SetPlacements().front(), scope, old_ref.length());
                (*inst)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(new_ref);
            }

    v->SetData().SetSet().SetVariations().front()->SetPlacements().front()->SetSeq().SetSeq_data().SetIupacna().Set(new_ref);
    v->SetData().SetSet().SetVariations().front()->SetPlacements().front()->SetSeq().SetLength(new_ref.length());
    FixAlleles(v,old_ref,new_ref);
}

void CVariationUtilities::CorrectRefAllele(CRef<CSeq_annot>& annot, CScope& scope)
{
    if (!annot->IsSetData() || !annot->GetData().IsFtable())
        NCBI_THROW(CException, eUnknown, "Ftable is not set in input Seq-annot");
    for (CSeq_annot::TData::TFtable::iterator feat = annot->SetData().SetFtable().begin(); feat != annot->SetData().SetFtable().end(); ++feat)
    {
        CVariation_ref& vr = (*feat)->SetData().SetVariation();
        string new_ref = GetAlleleFromLoc((*feat)->SetLocation(),scope);
        CorrectRefAllele(vr,new_ref);
    }
}


void CVariationUtilities::CorrectRefAllele(CVariation_ref& vr, const CSeq_loc& loc, CScope& scope)
{
    string new_ref = GetAlleleFromLoc(loc,scope);
    CorrectRefAllele(vr,new_ref);
}

void CVariationUtilities::CorrectRefAllele(CVariation_ref& vr, const string& new_ref)
{
    string old_ref;
    if (!vr.IsSetData() || !vr.GetData().IsSet() || !vr.GetData().GetSet().IsSetVariations())
        NCBI_THROW(CException, eUnknown, "Set of Variation-inst is not set in input Seq-annot");
    for (CVariation_ref::TData::TSet::TVariations::iterator inst = vr.SetData().SetSet().SetVariations().begin(); inst != vr.SetData().SetSet().SetVariations().end(); ++inst)
    {
        if (!(*inst)->IsSetData() || !(*inst)->GetData().IsInstance())
            NCBI_THROW(CException, eUnknown, "Variation-inst is not set in input Seq-annot");
        if (
             (*inst)->GetData().GetInstance().GetType() == CVariation_inst::eType_identity && 
            (*inst)->GetData().GetInstance().IsSetDelta() && !(*inst)->GetData().GetInstance().GetDelta().empty() && (*inst)->GetData().GetInstance().GetDelta().front()->IsSetSeq() 
            && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().IsLiteral()
            && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().IsSetSeq_data() 
            && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().IsIupacna())
        {
            old_ref  = (*inst)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(); 
            (*inst)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(new_ref);
        }
    }
    if (old_ref.empty())
        NCBI_THROW(CException, eUnknown, "Old reference allele not found in input Seq-annot");
    FixAlleles(vr,old_ref,new_ref);
}

void CVariationUtilities::FixAlleles(CVariation_ref& vr, string old_ref, string new_ref) 
{
    if (old_ref == new_ref) return;
    int type = CVariation_inst::eType_snv;
    bool add_old_ref = true;
    for (CVariation_ref::TData::TSet::TVariations::iterator inst = vr.SetData().SetSet().SetVariations().begin(); inst != vr.SetData().SetSet().SetVariations().end(); ++inst)
    {
        if ((*inst)->GetData().GetInstance().IsSetDelta() && !(*inst)->GetData().GetInstance().GetDelta().empty() && (*inst)->GetData().GetInstance().GetDelta().front()->IsSetSeq() 
            && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().IsLiteral()
            && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().IsSetSeq_data() 
            && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().IsIupacna())
        {
            string a = (*inst)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set();
            if (!(
                    (*inst)->GetData().GetInstance().GetType() == CVariation_inst::eType_identity 
                    ))
            {
                type = (*inst)->SetData().SetInstance().SetType();
                if (old_ref  == a) add_old_ref = false;
                if (new_ref == a)
                {
                    (*inst)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(old_ref);
                    add_old_ref = false;
                }
            }

        }
    }
    if (add_old_ref)
    {
        CRef<CVariation_inst> inst(new CVariation_inst);
        CRef<CDelta_item> item(new CDelta_item);
        inst->SetType(type);
        CSeq_literal literal;
        literal.SetLength(old_ref.size());
        CSeq_data data;
        data.SetIupacna().Set(old_ref);
        literal.SetSeq_data().Assign(data);
        item->SetSeq().SetLiteral().Assign(literal);
        inst->SetDelta().push_back(item);
        CRef<CVariation_ref> leaf(new CVariation_ref);
        leaf->SetData().SetInstance().Assign(*inst);
        vr.SetData().SetSet().SetVariations().push_back(leaf);
    }
}

string CVariationUtilities::GetRefAlleleFromVP(CRef<CVariantPlacement> vp, CScope& scope, TSeqPos length) 
{
    string new_ref;
    if(   (vp->IsSetStart_offset() && vp->GetStart_offset() != 0)
          || (vp->IsSetStop_offset()  && vp->GetStop_offset()  != 0))
        NCBI_THROW(CException, eUnknown, "Can't get sequence for an offset-based location");
    else if(length > MAX_LEN) 
        NCBI_THROW(CException, eUnknown, "Sequence is longer than the cutoff threshold");
     else 
     {
         new_ref = GetAlleleFromLoc(vp->GetLoc(),scope);
         if (new_ref.empty())
             NCBI_THROW(CException, eUnknown, "Empty residue in reference");
         for (unsigned int i=0; i<new_ref.size();i++)
             if(new_ref[i] != 'A' && new_ref[i] != 'C' && new_ref[i] != 'G' && new_ref[i] != 'T') 
                 NCBI_THROW(CException, eUnknown, "Ambiguous residues in reference");
     }
    return new_ref;
}

string CVariationUtilities::GetAlleleFromLoc(const CSeq_loc& loc, CScope& scope)
{
    string new_ref;
    if(sequence::GetLength(loc, NULL) > 0) 
    {
        try 
            {
                CSeqVector v(loc, scope, CBioseq_Handle::eCoding_Iupac);
                if(v.IsProtein()) 
                    NCBI_THROW(CException, eUnknown, "Not an NA sequence");
                v.GetSeqData(v.begin(), v.end(), new_ref);
            } catch(CException& e) 
            {
                string loc_label;   
                loc.GetLabel(&loc_label);
                NCBI_RETHROW_SAME(e, "Can't get literal for " + loc_label);
            }
    }
    return new_ref;
}
 

void CVariationUtilities::FixAlleles(CRef<CVariation> v, string old_ref, string new_ref) 
{
    if (old_ref == new_ref) return;
    for (CVariation::TData::TSet::TVariations::iterator v1 = v->SetData().SetSet().SetVariations().begin(); v1 != v->SetData().SetSet().SetVariations().end(); ++v1)
    {
        int type = CVariation_inst::eType_snv;
        bool add_old_ref = true;
        for (CVariation::TData::TSet::TVariations::iterator inst = (*v1)->SetData().SetSet().SetVariations().begin(); inst != (*v1)->SetData().SetSet().SetVariations().end(); ++inst)
        {
            if ((*inst)->GetData().GetInstance().IsSetDelta() && !(*inst)->GetData().GetInstance().GetDelta().empty() && (*inst)->GetData().GetInstance().GetDelta().front()->IsSetSeq() 
                && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().IsLiteral()
                && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().IsSetSeq_data() 
                && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().IsIupacna())
            {
                string a = (*inst)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set();
                if (! (*inst)->GetData().GetInstance().GetType() == CVariation_inst::eType_identity )
                {
                    type = (*inst)->SetData().SetInstance().SetType();
                    if (old_ref  == a) add_old_ref = false;
                    if (new_ref == a)
                    {
                        (*inst)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(old_ref);
                        add_old_ref = false;
                    }
                }
                
            }
        }

        if (add_old_ref)
        {
            CRef<CVariation_inst> inst(new CVariation_inst);
            CRef<CDelta_item> item(new CDelta_item);
            inst->SetType(type);
            CSeq_literal literal;
            literal.SetLength(old_ref.size());
            CSeq_data data;
            data.SetIupacna().Set(old_ref);
            literal.SetSeq_data().Assign(data);
            item->SetSeq().SetLiteral().Assign(literal);
            inst->SetDelta().push_back(item);
            CRef<CVariation> leaf(new CVariation);
            leaf->SetData().SetInstance().Assign(*inst);
            (*v1)->SetData().SetSet().SetVariations().push_back(leaf);
        }
    }

}

bool CVariationUtilities::IsReferenceCorrect(const CSeq_feat& feat, string& wrong_ref, string& correct_ref, CScope& scope)
{
    wrong_ref.clear();
    correct_ref.clear();
    bool found_allele_in_inst = false;
    if (feat.IsSetData() && feat.GetData().IsVariation() && feat.IsSetLocation())
    {
        const CSeq_loc& loc = feat.GetLocation(); 
        correct_ref = GetAlleleFromLoc(loc,scope);
   
        const CVariation_ref& vr = feat.GetData().GetVariation();
        if (!vr.IsSetData() || !vr.GetData().IsSet() || !vr.GetData().GetSet().IsSetVariations())
            NCBI_THROW(CException, eUnknown, "Set of Variation-inst is not set in input Seq-feat");

        for (CVariation_ref::TData::TSet::TVariations::const_iterator inst = vr.GetData().GetSet().GetVariations().begin(); inst != vr.GetData().GetSet().GetVariations().end(); ++inst)
        {
            if ((*inst)->IsSetData() && (*inst)->GetData().IsInstance() && 
                (*inst)->GetData().GetInstance().GetType() == CVariation_inst::eType_identity && 
                (*inst)->GetData().GetInstance().IsSetDelta() && !(*inst)->GetData().GetInstance().GetDelta().empty() && (*inst)->GetData().GetInstance().GetDelta().front()->IsSetSeq() 
                && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().IsLiteral()
                && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().IsSetSeq_data() 
                && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().IsIupacna())
            {
                wrong_ref  = (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get(); 
                found_allele_in_inst = true;
            }
        }
        
    }
    else
        NCBI_THROW(CException, eUnknown, "Feature is not a Variation or Location is not set");
    
    if (correct_ref.empty())
        NCBI_THROW(CException, eUnknown, "Cannot access correct reference allele at given location");
    
    if (found_allele_in_inst && wrong_ref.empty())
        NCBI_THROW(CException, eUnknown, "Old reference allele not found in input Seq-feat");
    
    if (!found_allele_in_inst)
        return true;
    else
        return (wrong_ref == correct_ref);
}

int CVariationUtilities::GetVariationType(const CVariation_ref& vr)
{
    set<int> types;
    switch(vr.GetData().Which())
    {
    case  CVariation_Base::C_Data::e_Instance : return vr.GetData().GetInstance().GetType(); break;
    case  CVariation_Base::C_Data::e_Set : 
        ITERATE(CVariation_ref::TData::TSet::TVariations, inst, vr.GetData().GetSet().GetVariations())
        {
            if ( (*inst)->IsSetData() && (*inst)->GetData().IsInstance() && (*inst)->GetData().GetInstance().GetType() != CVariation_inst::eType_identity)
                types.insert( (*inst)->GetData().GetInstance().GetType() );
        }
        break;
    default: break;            
    }

    if (types.empty())
        return  CVariation_inst::eType_unknown;
    if (types.size() == 1)
        return *types.begin();
    
    return  CVariation_inst::eType_other;
}

void CVariationUtilities::GetVariationRefAlt(const CVariation_ref& vr, string &ref,  vector<string> &alt)
{
    ref.clear();
    alt.clear();
    switch (vr.GetData().Which())
    {
    case  CVariation_Base::C_Data::e_Set :
        for (CVariation_ref::TData::TSet::TVariations::const_iterator inst_it = vr.GetData().GetSet().GetVariations().begin(); inst_it != vr.GetData().GetSet().GetVariations().end(); ++inst_it)
            if ( (*inst_it)->IsSetData() && (*inst_it)->GetData().IsInstance())
            {
                const CVariation_inst &inst = (*inst_it)->GetData().GetInstance();
                int type = inst.GetType();
                if (inst.IsSetDelta() && !inst.GetDelta().empty() && inst.GetDelta().front()->IsSetSeq() 
                    && inst.GetDelta().front()->GetSeq().IsLiteral() 
                    && inst.GetDelta().front()->GetSeq().GetLiteral().IsSetSeq_data() 
                    && inst.GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().IsIupacna())
                {
                    string a = inst.GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get();
                    if (!a.empty())
                    {
                        if (type == CVariation_inst::eType_identity )
                            ref = a;
                        else if (type != CVariation_inst::eType_del) 
                            alt.push_back(a);
                    }
                }
            }
        break;
    case  CVariation_Base::C_Data::e_Instance :
    {
        const CVariation_inst &inst = vr.GetData().GetInstance();
        int type = inst.GetType();
        if (inst.IsSetDelta() && !inst.GetDelta().empty() && inst.GetDelta().front()->IsSetSeq() 
            && inst.GetDelta().front()->GetSeq().IsLiteral() 
            && inst.GetDelta().front()->GetSeq().GetLiteral().IsSetSeq_data() 
            && inst.GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().IsIupacna())
        {
            string a = inst.GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get();
            if (!a.empty())
            {
                if (type == CVariation_inst::eType_identity )
                    ref = a;
                else if (type != CVariation_inst::eType_del) 
                    alt.push_back(a);
            }
        }
        break;
    }
    default : break;
    }           
}


void CVariationUtilities::GetVariationRefAlt(const CVariation& v, string &ref,  vector<string> &alt)
{
    ref.clear();
    alt.clear();
    switch (v.GetData().Which())
    {
    case  CVariation_Base::C_Data::e_Set :
        ITERATE(CVariation::TData::TSet::TVariations, var2, v.GetData().GetSet().GetVariations())
        {
            if ( (*var2)->IsSetData() && (*var2)->GetData().IsInstance())
            {
                const CVariation_inst &inst = (*var2)->GetData().GetInstance();
                int type = inst.GetType();
                if (inst.IsSetDelta() && !inst.GetDelta().empty() && inst.GetDelta().front()->IsSetSeq() 
                    && inst.GetDelta().front()->GetSeq().IsLiteral() 
                    && inst.GetDelta().front()->GetSeq().GetLiteral().IsSetSeq_data() 
                    && inst.GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().IsIupacna())
                {
                    string a = inst.GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get();
                    if (!a.empty())
                    {
                        if (type == CVariation_inst::eType_identity )
                            ref = a;
                        else if (type != CVariation_inst::eType_del) 
                            alt.push_back(a);
                    }
                }
            }
        }
        break;
    case  CVariation_Base::C_Data::e_Instance :
        {
            const CVariation_inst &inst = v.GetData().GetInstance();
            int type = inst.GetType();
            if (inst.IsSetDelta() && !inst.GetDelta().empty() && inst.GetDelta().front()->IsSetSeq() 
                && inst.GetDelta().front()->GetSeq().IsLiteral() 
                && inst.GetDelta().front()->GetSeq().GetLiteral().IsSetSeq_data() 
                && inst.GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().IsIupacna())
            {
                string a = inst.GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get();
                if (!a.empty())
                {
                    if (type == CVariation_inst::eType_identity )
                        ref = a;
                    else if (type != CVariation_inst::eType_del) 
                        alt.push_back(a);
                }
            }
            break;
        }
    default : break;
    }           
}

bool CVariationUtilities::CommonRepeatUnit(string alt1, string alt2)
{

    string repSubStr1 = x_RepeatedSubstring(alt1);
    string repSubStr2 = x_RepeatedSubstring(alt2);

    return repSubStr1 == repSubStr2;
}


bool CVariationUtilities::CommonRepeatUnit(const CVariation_ref& vr)
{
    string ref;
    vector<string> alts;
    CVariationUtilities::GetVariationRefAlt(vr,ref,alts);

    for(SIZE_TYPE ii = 0; ii< alts.size(); ii++) {
        for(SIZE_TYPE jj = ii+1; jj< alts.size(); jj++) {
            if(!CVariationUtilities::CommonRepeatUnit(alts[ii], alts[jj]))
                return false;
        }
    }
    return true;
}


bool CVariationUtilities::CommonRepeatUnit(const CVariation& v)
{
    string ref;
    vector<string> alts;
    CVariationUtilities::GetVariationRefAlt(v,ref,alts);

    for(SIZE_TYPE ii = 0; ii< alts.size(); ii++) {
        for(SIZE_TYPE jj = ii+1; jj< alts.size(); jj++) {
            if(!CVariationUtilities::CommonRepeatUnit(alts[ii], alts[jj]))
                return false;
        }
    }
    return true;
}


bool CVariationUtilities::x_isBaseRepeatingUnit(
    const string& candidate, const string& target) 
{
    if(target.length() % candidate.length() != 0)
        return false;


    int factor = target.length() / candidate.length();
    string test_candidate;
    for(int jj=0; jj<factor ; jj++) {
        test_candidate += candidate;
    }

    if(test_candidate == target) 
        return true;
    return false;
}


string CVariationUtilities::x_RepeatedSubstring(const string &str) 
{
    for(vector<string>::size_type ii=1; ii <= str.length() / 2 ; ++ii) {

        if( str.length() % ii != 0 ) continue;
        string candidate = str.substr(0,ii);

        //If this canddate is a base repeating unit of the target str,
        //then this is what we are looking for
        if(x_isBaseRepeatingUnit(candidate, str))
            return candidate;
    }

    return str;
}



// Variation Normalization
CCache<string,CRef<CSeqVector> > CVariationNormalization_base_cache::m_cache(CCACHE_SIZE);

void CVariationNormalization_base_cache::x_rotate_left(string &v)
{
    // simple rotation to the left
    std::rotate(v.begin(), v.begin() + 1, v.end());
}

void CVariationNormalization_base_cache::x_rotate_right(string &v)
{
    // simple rotation to the right
    std::rotate(v.rbegin(), v.rbegin() + 1, v.rend());
}

string CVariationNormalization_base_cache::x_CompactifySeq(string a)
{
    string result = a;
    for (unsigned int i=1; i<= a.size() / 2; i++)
        if (a.size() % i == 0)
        {
            int k = a.size() / i;
            string b = a.substr(0,i);
            string c;
            for (int j=0; j < k; j++)
                c += b;
            if (c == a)
            {
                result = b;
                break;
            }

        }
    return result;
}

CRef<CSeqVector> CVariationNormalization_base_cache::x_PrefetchSequence(CScope &scope, const CSeq_id &seq_id, ENa_strand strand)
{
    string accession;
    seq_id.GetLabel(&accession);

    //this is the magical thread safe line that 
    // makes sure to get a reference count on the object in the cache
    // and now the actual object in memory can not be deleted
    // until that object's counter is down to zero.
    // and that will not happen until the caller's CRef is destroyed.
    LOG_POST(Trace << "Try to get from cache for accession: " << accession );
    CRef<CSeqVector> seqvec_ref = m_cache.Get(accession);
    LOG_POST(Trace << "Got CRef for acc : " << accession );
    if ( !seqvec_ref || seqvec_ref->empty() ) 
    {
        LOG_POST(Trace << "Acc was empty or null: " << accession );
        const CBioseq_Handle& bsh = scope.GetBioseqHandle( seq_id );
        LOG_POST(Trace << "Got BioseqHandle, now get SeqVecRef: " << accession );
        seqvec_ref = Ref(new CSeqVector(bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac,strand)));
        LOG_POST(Trace << "Add it to the cache: " << accession );
        m_cache.Add(accession, seqvec_ref);
        LOG_POST(Trace << "Added to cache: " << accession );
    }
    LOG_POST(Trace << "Return the seqvec ref for : " << accession );
    return seqvec_ref;
}

string CVariationNormalization_base_cache::x_GetSeq(int pos, int length, CSeqVector &seqvec)
{
    _ASSERT(!seqvec.empty());   

    string seq;
    seqvec.GetSeqData(pos, pos+length, seq);
    return seq;
}

int CVariationNormalization_base_cache::x_GetSeqSize(CSeqVector &seqvec)
{
    return seqvec.size();
} 

template<class T>
void CVariationNormalization_base<T>::x_Shift(CVariation& v_orig, CScope &scope)
{
    CRef<CVariation> v = Ref(&v_orig);
    if (!v_orig.IsSetPlacements() && v_orig.IsSetData() && v_orig.GetData().IsSet() && v_orig.GetData().GetSet().IsSetVariations()) 
        v = v_orig.SetData().SetSet().SetVariations().front();

    for (CVariation::TPlacements::iterator ivp =  v->SetPlacements().begin(); ivp != v->SetPlacements().end(); ++ivp)
    {
        CVariantPlacement & vp = **ivp;
        if (!vp.IsSetLoc()) continue;

        const CSeq_id &seq_id = *vp.GetLoc().GetId();

        SEndPosition end_positions(
            vp.GetLoc().GetStart(eExtreme_Positional),
            vp.GetLoc().GetStop(eExtreme_Positional), -1, -1);

        string ref;
        if (vp.IsSetSeq() && vp.GetSeq().IsSetSeq_data() && vp.GetSeq().GetSeq_data().IsIupacna()) 
            ref = vp.SetSeq().SetSeq_data().SetIupacna().Set();

        bool is_deletion = false;
        CRef<CSeq_literal> ref_seqliteral;
        int type;              
        ENa_strand strand = eNa_strand_unknown;
        if (vp.GetLoc().IsSetStrand())
            strand = vp.GetLoc().GetStrand();
        CRef<CSeqVector> seqvec = x_PrefetchSequence(scope,seq_id,strand);
        if (v->IsSetData())
        {
            list<SEndPosition> end_pos_list;
            switch (v->SetData().Which())
            {
            case  CVariation_Base::C_Data::e_Instance : 
                x_ProcessInstance(v->SetData().SetInstance(),vp.SetLoc(),
                    is_deletion,ref_seqliteral,ref,end_positions,*seqvec,type);
                break;
            case  CVariation_Base::C_Data::e_Set :

                //are alleles compatible with shifting (must have common basic repeat unit)
                if( ! CVariationUtilities::CommonRepeatUnit(*v) )
                {
                    return;
                }


                NON_CONST_ITERATE(CVariation::TData::TSet::TVariations, var2, v->SetData().SetSet().SetVariations())
                {
                    SEndPosition tmp(end_positions);

                    if ( (*var2)->IsSetData() && (*var2)->SetData().IsInstance())
                        x_ProcessInstance((*var2)->SetData().SetInstance(),vp.SetLoc(),is_deletion,ref_seqliteral,ref,tmp,*seqvec,type);

                    end_pos_list.push_back(tmp);

                }
                //Check, are all things in the list idenitcal?
                end_pos_list.unique();
                if(end_pos_list.size() != 1 )
                {
                    //The alleles are too different to shift.  They end up in different places
                    //Should be caught by 'CommonRepeatUnit' above.
                    return;
                }

                end_positions = end_pos_list.front();


                break;
            default: break;
            }
        }
        if (!ref.empty() && is_deletion)
        {
            // use VarPlacement seq literal over derived seq literal
            if (vp.IsSetSeq() && vp.GetSeq().IsSetSeq_data() && vp.GetSeq().GetSeq_data().IsIupacna()) 
                ref_seqliteral = Ref(&vp.SetSeq());

            x_ModifyDeleltionLocation(vp.SetLoc(), end_positions, ref, *ref_seqliteral, *seqvec, type);
        }

    }
}


//Either called per Variation placement
// or called per SeqFeat
template<class T>
void CVariationNormalization_base<T>::x_ModifyDeleltionLocation(
    CSeq_loc& loc, SEndPosition& sep, string& ref, CSeq_literal& ref_seqliteral, CSeqVector& seqvec, int type)
{
    int pos = sep.right - ref.size() + 1;
    const bool found = x_ProcessShift(ref, sep.left ,pos, seqvec, type);
    if(!found) return;

    sep.right = pos + ref.size()-1;

    sep.CorrectNewPositions();
    if( sep.HasBadPositions() )     
        return;    


    x_ModifyLocation(loc,ref_seqliteral,ref,sep.left,sep.right,type);

}

template<class T>
void CVariationNormalization_base<T>::x_ProcessInstance(CVariation_inst &inst, CSeq_loc &loc, bool &is_deletion,  
            CRef<CSeq_literal>& ref_seqliteral, string &ref, SEndPosition& sep,
            CSeqVector &seqvec, int &rtype)
{
    int type = inst.SetType();
    if (type != CVariation_inst::eType_identity)
        rtype = type;
    if (type == CVariation_inst::eType_del)
        is_deletion = true;
    if (inst.IsSetDelta() && !inst.GetDelta().empty() && inst.GetDelta().front()->IsSetSeq() 
        && inst.GetDelta().front()->GetSeq().IsLiteral() && inst.GetDelta().front()->GetSeq().GetLiteral().IsSetSeq_data() 
        && inst.GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().IsIupacna())
    {
        string a = inst.SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set();
        if (type == CVariation_inst::eType_identity )//&& inst.IsSetObservation() && inst.GetObservation() == CVariation_inst::eObservation_reference)
        {
            ref = a;
            ref_seqliteral = Ref(&inst.SetDelta().front()->SetSeq().SetLiteral());
        }
        if (!a.empty() && type == CVariation_inst::eType_ins) 
        {
            string compact = x_CompactifySeq(a);
            string orig_compact = compact;
            const bool found = x_ProcessShift(compact, sep.left, sep.right, seqvec,type);
            if (found)
            {
                while (orig_compact != compact)
                {
                    x_rotate_left(a);
                    x_rotate_left(orig_compact);
                }
                sep.CorrectNewPositions();
                if(sep.HasBadPositions())
                    return;

                x_ModifyLocation(loc,inst.SetDelta().front()->SetSeq().SetLiteral(),a,sep.left,sep.right,type);
            }
        }
    }
}

template<class T>
void CVariationNormalization_base<T>::x_Shift(CSeq_annot& annot, CScope &scope)
{
	if (!annot.IsSetData() || !annot.GetData().IsFtable())
		NCBI_THROW(CException, eUnknown, "Ftable is not set in input Seq-annot");

	CSeq_annot::TData::TFtable ftable = annot.SetData().SetFtable();
	for (CSeq_annot::TData::TFtable::iterator ifeat = ftable.begin(); ifeat != ftable.end(); ++ifeat)
		x_Shift(**ifeat, scope);
}

template<class T>
void CVariationNormalization_base<T>::x_Shift(CSeq_feat& feat, CScope &scope)
{
	LOG_POST(Trace << "This Feat: " << MSerial_AsnText << feat << Endm);
	if (!feat.IsSetLocation()           || 
		!feat.IsSetData()               || 
		!feat.GetData().IsVariation()   || 
		!feat.GetData().GetVariation().IsSetData()) return;

	const CSeq_id &seq_id = *feat.GetLocation().GetId();
    SEndPosition end_positions(
        feat.GetLocation().GetStart(eExtreme_Positional),
        feat.GetLocation().GetStop(eExtreme_Positional), -1, -1);
	
    bool is_deletion = false;
	string ref;
	CRef<CSeq_literal> ref_seqliteral;
	
    int type;
	ENa_strand strand = eNa_strand_unknown;
	if (feat.GetLocation().IsSetStrand())
		strand = feat.GetLocation().GetStrand();

	CRef<CSeqVector> seqvec ;
	try {
		seqvec = x_PrefetchSequence(scope,seq_id,strand);
		ERR_POST(Trace << "Prefetch success for : " << seq_id.AsFastaString());
	} catch(CException& e) {
		ERR_POST(Error << "Prefetch failed for " << seq_id.AsFastaString() );
		ERR_POST(Error << e.ReportAll());
		return;
	}

	//CRef<CSeqVector> seqvec = x_PrefetchSequence(scope,seq_id,strand);

	CVariation_ref& vr = feat.SetData().SetVariation();

	try {
        list<SEndPosition> end_pos_list;
		switch(vr.GetData().Which())
		{
		case  CVariation_Base::C_Data::e_Instance : 
            x_ProcessInstance(vr.SetData().SetInstance(),feat.SetLocation(),is_deletion,ref_seqliteral,ref, end_positions, *seqvec,type); break;
		case  CVariation_Base::C_Data::e_Set : 

            //are alleles compatible with shifting (must have common basic repeat unit)
            if( ! CVariationUtilities::CommonRepeatUnit(vr) )
            {
                return;
            }

                        
            NON_CONST_ITERATE(CVariation_ref::TData::TSet::TVariations, inst, vr.SetData().SetSet().SetVariations())
			{

                SEndPosition tmp(end_positions);

				if ( (*inst)->IsSetData() && (*inst)->SetData().IsInstance())
					x_ProcessInstance((*inst)->SetData().SetInstance(),feat.SetLocation(),is_deletion,ref_seqliteral,ref,tmp,*seqvec,type);

                end_pos_list.push_back(tmp);
			}
            //Check, are all things in the list idenitcal?
            end_pos_list.unique();
            if(end_pos_list.size() != 1 )
            {
                //The alleles are too different to shift.  They end up in different places
                //Should be caught by 'CommonRepeatUnit' above.
                return;
            }

            end_positions = end_pos_list.front();


			break;
		default: break;            
		}
	} catch(CException &e) {
		ERR_POST(Error << "Exception while shifting: " << e.ReportAll() << ".  Continuing on" << Endm);
		//Ideally this would make it into the service msg'ing system.
		return;
	}

    //Special handling for deletion
	if (!ref.empty() && is_deletion)
	{
        x_ModifyDeleltionLocation(feat.SetLocation(), end_positions, ref, *ref_seqliteral, *seqvec, type);
	}
}

template<class T>
bool CVariationNormalization_base<T>::x_IsShiftable(CSeq_loc &loc, string &ref, CScope &scope, int type)
{
    if (type != CVariation_inst::eType_ins && type != CVariation_inst::eType_del)
        NCBI_THROW(CException, eUnknown, "Only insertions or deletions can currently be processed");

    if (ref.empty())
        NCBI_THROW(CException, eUnknown, "Submitted allele is empty");

    const CSeq_id &seq_id = *loc.GetId();

    SEndPosition end_positions(
        loc.GetStart(eExtreme_Positional),
        loc.GetStop(eExtreme_Positional), -1, -1);

    end_positions.CorrectNewPositions();


    ENa_strand strand = eNa_strand_unknown;
    if (loc.IsSetStrand())
        strand = loc.GetStrand();
    CRef<CSeqVector> seqvec = x_PrefetchSequence(scope,seq_id,strand);


    if (type == CVariation_inst::eType_del)
    {
        int pos = end_positions.right - ref.size() + 1;
        x_ProcessShift(ref, end_positions.new_left,pos,*seqvec,type);
        end_positions.new_right = pos + ref.size()-1;
    }
    else
    {  
        string compact = x_CompactifySeq(ref);
        string orig_compact = compact;
        x_ProcessShift(compact, end_positions.new_left ,end_positions.new_right,*seqvec,type);
        while (orig_compact != compact)
        {
            x_rotate_left(ref);
            x_rotate_left(orig_compact);
        }
    }

    bool isShifted = (end_positions.new_left != end_positions.left) || 
                     (end_positions.new_right != end_positions.right);
    
    x_ExpandLocation(loc, end_positions.new_left , end_positions.new_right);

    return isShifted;
}

template<class T>
void CVariationNormalization_base<T>::x_ExpandLocation(CSeq_loc &loc, int pos_left, int pos_right) 
{
    if (pos_left == pos_right)
    {
        if (loc.IsPnt())
            loc.SetPnt().SetPoint(pos_left);
        else
        {
            CSeq_point pnt;
            pnt.SetPoint(pos_left);
            if (loc.GetInt().IsSetStrand())
                pnt.SetStrand( loc.GetInt().GetStrand() );
            pnt.SetId().Assign(loc.GetInt().GetId());
            loc.SetPnt().Assign(pnt);
        }
    }
    else
    {
        if (loc.IsInt())
        {
            loc.SetInt().SetFrom(pos_left);
            loc.SetInt().SetTo(pos_right);
        }
        else
        {
            CSeq_interval interval;
            interval.SetFrom(pos_left);
            interval.SetTo(pos_right);
            if (loc.GetPnt().IsSetStrand())
                interval.SetStrand( loc.GetPnt().GetStrand() );
            interval.SetId().Assign(loc.GetPnt().GetId());
            loc.SetInt().Assign(interval);
        }
    }
}

bool CVariationNormalizationLeft::x_ProcessShift(string &a, int &pos,int &pos_right, CSeqVector &seqvec, int type)
{
    string orig_a = a;
    int length = a.size();
    int orig_pos = pos;
    bool found_left = false;

    string b;
    if (pos >=0 && pos+length < x_GetSeqSize(seqvec))
        b = x_GetSeq(pos,length,seqvec);
    if (type == CVariation_inst::eType_ins)
    {
        while (a != b && pos >= orig_pos - length) // Should be >= here in contrast to the right shifting because the insertion can be after the sequence.
        {
            pos--;
            x_rotate_right(a);
            if (pos >=0 && pos+length < x_GetSeqSize(seqvec))
                b = x_GetSeq(pos,length,seqvec);
        }
    }
    if (a != b) 
    {
        pos = orig_pos;
        a = orig_a;
        return false;
    }   

    while (pos >= 0 && pos+length < x_GetSeqSize(seqvec))
    {
        pos--; 
        x_rotate_right(a);
        string b;
        if (pos >=0 && pos+length < x_GetSeqSize(seqvec))
            b = x_GetSeq(pos,length,seqvec);
        if (a != b) 
        {
            pos++;
            x_rotate_left(a);
            found_left = true;
            break;
        }
    }
    if (!found_left)    // I don't really see how this could happen.
        pos = orig_pos;
    if (pos == orig_pos) return false;
    return found_left;                         
}

void CVariationNormalizationLeft::x_ModifyLocation(CSeq_loc &loc, CSeq_literal& literal, string a, int pos, int pos_right, int type)
{
    if (loc.IsInt())
    {
        CSeq_point pnt;
        pnt.SetPoint(pos);
        if (loc.GetInt().IsSetStrand())
            pnt.SetStrand( loc.GetInt().GetStrand() );
        pnt.SetId().Assign(loc.GetInt().GetId());
        loc.SetPnt().Assign(pnt);
    }
    else
        loc.SetPnt().SetPoint(pos); 
    literal.SetSeq_data().SetIupacna().Set(a);
}

bool CVariationNormalizationRight::x_ProcessShift(string &a, int &pos_left, int &pos, CSeqVector &seqvec, int type) // pos here should already point to the beginning of the seq to be inserted or deleted
{
    string orig_a = a;
    int orig_pos = pos;
    int length = a.size(); 

    string b;
    if (pos >=0 && pos+length < x_GetSeqSize(seqvec))
        b = x_GetSeq(pos,length,seqvec);
    if (type == CVariation_inst::eType_ins)
    {
        while (a != b && pos > orig_pos - length)
        {
            pos--;
            x_rotate_right(a);
            if (pos >=0 && pos+length < x_GetSeqSize(seqvec))
                b = x_GetSeq(pos,length,seqvec);
        }
    }
    if (a != b) 
    {
        pos = orig_pos;
        a = orig_a;
        return false;
    }

    bool found_right = false;
    while (pos >=0 && pos+length < x_GetSeqSize(seqvec))
    {
        pos++; 
        x_rotate_left(a);
        string b;
        if (pos >=0 && pos+length < x_GetSeqSize(seqvec))
            b = x_GetSeq(pos,length,seqvec);
        if (a != b) 
        {
            pos--;
            x_rotate_right(a);
            found_right = true;
            break;
        }
    }
    if (!found_right)     // doesn't seem possible to happen
        pos = orig_pos;
    if (type == CVariation_inst::eType_ins)
        pos += a.size();
    //if (pos == orig_pos) return false;
    return found_right;                         
}

void CVariationNormalizationRight::x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos, int type) // identical to the left point
{
    if (loc.IsInt())
    {
        CSeq_point pnt;
        pnt.SetPoint(pos);
        if (loc.GetInt().IsSetStrand())
            pnt.SetStrand( loc.GetInt().GetStrand() );
        pnt.SetId().Assign(loc.GetInt().GetId());
        loc.SetPnt().Assign(pnt);
    }
    else
        loc.SetPnt().SetPoint(pos); 
    literal.SetSeq_data().SetIupacna().Set(a);
}

void CVariationNormalizationInt::x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos_right, int type) 
{
    x_ExpandLocation(loc, pos_left, pos_right);
    literal.SetSeq_data().SetIupacna().Set(a);
}

bool CVariationNormalizationInt::x_ProcessShift(string &a, int &pos_left, int &pos_right, CSeqVector &seqvec, int type)
{
    string a_left = a;
    bool found_left = CVariationNormalizationLeft::x_ProcessShift(a_left, pos_left, pos_right,seqvec,type);
    bool found_right = CVariationNormalizationRight::x_ProcessShift(a, pos_left, pos_right,seqvec,type);
    a = a_left;
    return found_left | found_right;
}


bool CVariationNormalizationLeftInt::x_ProcessShift(string &a, int &pos_left, int &pos_right, CSeqVector &seqvec, int type)
{
    int orig_pos_left = pos_left;
    string orig_a = a;
    bool r = CVariationNormalizationLeft::x_ProcessShift(a,pos_left,pos_right,seqvec,type);
    if (type == CVariation_inst::eType_ins)
    {
        if (!r)
        {
            pos_left = orig_pos_left;
            a = orig_a;
        }
        r = true;
    }
    pos_right = pos_left;
    return r;
}

void CVariationNormalizationLeftInt::x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos_right, int type)
{
    if (type == CVariation_inst::eType_ins && pos_left > 0)
    {
        if (loc.IsPnt())
        {
            CSeq_interval interval;
            interval.SetFrom(pos_left-1);
            interval.SetTo(pos_left);
            if (loc.GetPnt().IsSetStrand())
                interval.SetStrand( loc.GetPnt().GetStrand() );
            interval.SetId().Assign(loc.GetPnt().GetId());
            loc.SetInt().Assign(interval);
        }
        else
        {
            loc.SetInt().SetFrom(pos_left-1);
            loc.SetInt().SetTo(pos_left); 
        }
        literal.SetSeq_data().SetIupacna().Set(a);
    }
    else  if (type == CVariation_inst::eType_del)
    {
        if (a.size() == 1)
        {
            if (loc.IsPnt())
                loc.SetPnt().SetPoint(pos_left);
            else
            {
                CSeq_point pnt;
                pnt.SetPoint(pos_left);
                if (loc.GetInt().IsSetStrand())
                    pnt.SetStrand( loc.GetInt().GetStrand() );
                pnt.SetId().Assign(loc.GetInt().GetId());
                loc.SetPnt().Assign(pnt);
            }
        }
        else
        {
            if (loc.IsPnt())
            {
                CSeq_interval interval;
                interval.SetFrom(pos_left);
                interval.SetTo(pos_left+a.size()-1);
                if (loc.GetPnt().IsSetStrand())
                    interval.SetStrand( loc.GetPnt().GetStrand() );
                interval.SetId().Assign(loc.GetPnt().GetId());
                loc.SetInt().Assign(interval);
            }
            else
            {
                loc.SetInt().SetFrom(pos_left);
                loc.SetInt().SetTo(pos_left+a.size()-1); 
            }
        }
        literal.SetSeq_data().SetIupacna().Set(a);
    }
    else
        CVariationNormalizationLeft::x_ModifyLocation(loc,literal,a, pos_left, pos_right,type);
}




void CVariationNormalization::AlterToVCFVar(CVariation& var, CScope &scope)
{
    CVariationNormalizationLeft::x_Shift(var,scope);
}

void CVariationNormalization::AlterToVCFVar(CSeq_annot& var, CScope& scope)
{
    CVariationNormalizationLeft::x_Shift(var,scope);
}

void CVariationNormalization::AlterToVCFVar(CSeq_feat& feat, CScope& scope)
{
    CVariationNormalizationLeft::x_Shift(feat,scope);
}

void CVariationNormalization::AlterToHGVSVar(CVariation& var, CScope& scope)
{
    CVariationNormalizationRight::x_Shift(var,scope);
}

void CVariationNormalization::AlterToHGVSVar(CSeq_annot& annot, CScope& scope)
{
	CVariationNormalizationRight::x_Shift(annot,scope);
}

void CVariationNormalization::AlterToHGVSVar(CSeq_feat& feat, CScope& scope)
{
    CVariationNormalizationRight::x_Shift(feat,scope);
}

void CVariationNormalization::NormalizeAmbiguousVars(CVariation& var, CScope &scope)
{
    CVariationNormalizationInt::x_Shift(var,scope);
}

void CVariationNormalization::NormalizeAmbiguousVars(CSeq_annot& var, CScope &scope)
{
    CVariationNormalizationInt::x_Shift(var,scope);
}

void CVariationNormalization::NormalizeAmbiguousVars(CSeq_feat& feat, CScope &scope)
{
    CVariationNormalizationInt::x_Shift(feat,scope);
}


void CVariationNormalization::AlterToVarLoc(CVariation& var, CScope& scope)
{
    CVariationNormalizationLeftInt::x_Shift(var,scope);
}

void CVariationNormalization::AlterToVarLoc(CSeq_annot& var, CScope& scope)
{
    CVariationNormalizationLeftInt::x_Shift(var,scope);
}

void CVariationNormalization::AlterToVarLoc(CSeq_feat& feat, CScope& scope)
{
    CVariationNormalizationLeftInt::x_Shift(feat,scope);
}

bool CVariationNormalization::IsShiftable(CSeq_loc &loc, string &a, CScope &scope, int type)
{
    return CVariationNormalizationInt::x_IsShiftable(loc,a,scope,type);
}

void CVariationNormalization::NormalizeVariation(CVariation& var, ETargetContext target_ctxt, CScope& scope)
{
    switch(target_ctxt) {
    case eDbSnp : NormalizeAmbiguousVars(var,scope); break;
    case eHGVS : AlterToHGVSVar(var,scope); break;
    case eVCF : AlterToVCFVar(var,scope); break;
    case eVarLoc : AlterToVarLoc(var,scope); break;
    default :  NCBI_THROW(CException, eUnknown, "Unknown context");
    }
}

void CVariationNormalization::NormalizeVariation(CSeq_annot& var, ETargetContext target_ctxt, CScope& scope)
{
    switch(target_ctxt) {
    case eDbSnp : NormalizeAmbiguousVars(var,scope); break;
    case eHGVS : AlterToHGVSVar(var,scope); break;
    case eVCF : AlterToVCFVar(var,scope); break;
    case eVarLoc : AlterToVarLoc(var,scope); break;
    default :  NCBI_THROW(CException, eUnknown, "Unknown context");
    }
}

void CVariationNormalization::NormalizeVariation(CSeq_feat& feat, ETargetContext target_ctxt, CScope& scope)
{
    switch(target_ctxt) {
    case eDbSnp : NormalizeAmbiguousVars(feat,scope); break;
    case eHGVS : AlterToHGVSVar(feat,scope); break;
    case eVCF : AlterToVCFVar(feat,scope); break;
    case eVarLoc : AlterToVarLoc(feat,scope); break;
    default :  NCBI_THROW(CException, eUnknown, "Unknown context");
    }
}

