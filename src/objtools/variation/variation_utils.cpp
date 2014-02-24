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
                //&& (*inst)->GetData().GetInstance().IsSetObservation() && (int((*inst)->GetData().GetInstance().GetObservation()) & int(CVariation_inst::eObservation_reference)) == int(CVariation_inst::eObservation_reference)
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
        CorrectRefAllele(vr,scope,new_ref);
    }
}

void CVariationUtilities::CorrectRefAllele(CRef<CVariation_ref>& var, CScope& scope)
{
    CVariation_ref& vr = *var;
    if (!vr.IsSetLocation())
        NCBI_THROW(CException, eUnknown, "Location is not set in input Variation-ref");
    string new_ref = GetAlleleFromLoc(vr.GetLocation(),scope);
    CorrectRefAllele(vr,scope,new_ref);
}

void CVariationUtilities::CorrectRefAllele(CVariation_ref& vr, CScope& scope, const string& new_ref)
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
             //(*inst)->GetData().GetInstance().IsSetObservation() && (int((*inst)->GetData().GetInstance().GetObservation()) & int(CVariation_inst::eObservation_reference)) == int(CVariation_inst::eObservation_reference) &&
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
                    //(*inst)->GetData().GetInstance().IsSetObservation() && (int((*inst)->GetData().GetInstance().GetObservation()) & int(CVariation_inst::eObservation_reference)) == int(CVariation_inst::eObservation_reference)
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

// Taken from variation_util2.cpp
string CVariationUtilities::GetRefAlleleFromVP(CRef<CVariantPlacement> vp, CScope& scope, TSeqPos length) 
{
    string new_ref;
//    TSeqPos length = vp->GetSeq().GetLength();
    
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
                if (!(
                         (*inst)->GetData().GetInstance().GetType() == CVariation_inst::eType_identity 
                         //(*inst)->GetData().GetInstance().IsSetObservation() && (int((*inst)->GetData().GetInstance().GetObservation()) & int(CVariation_inst::eObservation_reference)) == int(CVariation_inst::eObservation_reference)
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
                // (*inst)->GetData().GetInstance().IsSetObservation() && 
                //(int((*inst)->GetData().GetInstance().GetObservation()) & int(CVariation_inst::eObservation_reference)) == int(CVariation_inst::eObservation_reference) &&
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
    if ( !m_cache.Get(accession) || m_cache.Get(accession)->empty() ) 
    {
        const CBioseq_Handle& bsh = scope.GetBioseqHandle( seq_id );
        m_cache.Add(accession, Ref(new CSeqVector(bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac,strand))));
    }
    return m_cache[accession];
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
    _ASSERT(!seqvec.empty());  
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
            int pos_left = vp.GetLoc().GetStart(eExtreme_Positional);
            int pos_right = vp.GetLoc().GetStop(eExtreme_Positional);

            string ref;
            if (vp.IsSetSeq() && vp.GetSeq().IsSetSeq_data() && vp.GetSeq().GetSeq_data().IsIupacna()) 
                ref = vp.SetSeq().SetSeq_data().SetIupacna().Set();
            
            int new_pos_left = -1;
            int new_pos_right = -1;
            bool is_deletion = false;
            CSeq_literal *refref = NULL;
            int type;              
            ENa_strand strand = eNa_strand_unknown;
            if (vp.GetLoc().IsSetStrand())
                strand = vp.GetLoc().GetStrand();
            CRef<CSeqVector> seqvec = x_PrefetchSequence(scope,seq_id,strand);
            if (v->IsSetData())
            {
                switch (v->SetData().Which())
                {
                    case  CVariation_Base::C_Data::e_Instance : x_ProcessInstance(v->SetData().SetInstance(),vp.SetLoc(),is_deletion,refref,ref,pos_left,pos_right,new_pos_left,new_pos_right,*seqvec,type);break;
                    case  CVariation_Base::C_Data::e_Set :
                        for ( CVariation::TData::TSet::TVariations::iterator var2 = v->SetData().SetSet().SetVariations().begin(); var2 != v->SetData().SetSet().SetVariations().end();  ++var2 )
                        {
                            if ( (*var2)->IsSetData() && (*var2)->SetData().IsInstance())
                                x_ProcessInstance((*var2)->SetData().SetInstance(),vp.SetLoc(),is_deletion,refref,ref,pos_left,pos_right,new_pos_left,new_pos_right,*seqvec,type);
                        }
                        break;
                    default: break;
                }
            }
            if (!ref.empty() && is_deletion)
            {
                int pos = pos_right - ref.size() + 1;
                const bool found = x_ProcessShift(ref, pos_left,pos, *seqvec, type);
                pos_right = pos + ref.size()-1;
                if (found)
                {
                    if (new_pos_left == -1)
                    {
                        new_pos_left = pos_left;
                    }
                    else if (new_pos_left != pos_left)
                    {
                        ERR_POST(Error <<  "Left position is ambiguous due to different leaf alleles" << Endm);
                        continue;
                    }
                    if (new_pos_right == -1)
                    {
                        new_pos_right = pos_right;
                    }
                    else if (new_pos_right != pos_right)
                    {
                        ERR_POST(Error <<  "Right position is ambiguous due to different leaf alleles" << Endm);
                        continue;
                    }
                    if (vp.IsSetSeq() && vp.GetSeq().IsSetSeq_data() && vp.GetSeq().GetSeq_data().IsIupacna()) 
                        x_ModifyLocation(vp.SetLoc(),vp.SetSeq(),ref,pos_left,pos_right,type);
                    x_ModifyLocation(vp.SetLoc(),*refref,ref,pos_left,pos_right,type);
                    
                }
            }
            
        }
}

template<class T>
void CVariationNormalization_base<T>::x_ProcessInstance(CVariation_inst &inst, CSeq_loc &loc, bool &is_deletion,  CSeq_literal *&refref, string &ref, int &pos_left, int &pos_right, int &new_pos_left, int &new_pos_right,
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
            refref = &inst.SetDelta().front()->SetSeq().SetLiteral();
        }
        if (!a.empty() && type == CVariation_inst::eType_ins) 
        {
            string compact = x_CompactifySeq(a);
            string orig_compact = compact;
            const bool found = x_ProcessShift(compact, pos_left,pos_right, seqvec,type);
            if (found)
            {
                while (orig_compact != compact)
                {
                    x_rotate_left(a);
                    x_rotate_left(orig_compact);
                }
                if (new_pos_left == -1)
                {
                    new_pos_left = pos_left;
                }
                else if (new_pos_left != pos_left)
                {
                    ERR_POST(Error <<   "Left position is ambiguous due to different leaf alleles" << Endm);
                    return;
                }
                if (new_pos_right == -1)
                {
                    new_pos_right = pos_right;
                }
                else if (new_pos_right != pos_right)
                {
                    ERR_POST(Error <<  "Right position is ambiguous due to different leaf alleles" << Endm);
                    return;
                }
                x_ModifyLocation(loc,inst.SetDelta().front()->SetSeq().SetLiteral(),a,pos_left,pos_right,type);
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
    {
        CSeq_feat &feat = **ifeat;
        if (!feat.IsSetLocation()) continue;
            
        const CSeq_id &seq_id = *feat.GetLocation().GetId();
        int pos_left = feat.GetLocation().GetStart(eExtreme_Positional);
        int pos_right = feat.GetLocation().GetStop(eExtreme_Positional);

        int new_pos_left = -1;
        int new_pos_right = -1;
        bool is_deletion = false;
        string ref;
        CSeq_literal *refref = NULL;
        int type;
        ENa_strand strand = eNa_strand_unknown;
        if (feat.GetLocation().IsSetStrand())
            strand = feat.GetLocation().GetStrand();
        CRef<CSeqVector> seqvec = x_PrefetchSequence(scope,seq_id,strand);
        CVariation_ref& vr = feat.SetData().SetVariation();
        if (vr.IsSetData())
        {
            switch(vr.GetData().Which())
            {
                case  CVariation_Base::C_Data::e_Instance : x_ProcessInstance(vr.SetData().SetInstance(),feat.SetLocation(),is_deletion,refref,ref,pos_left,pos_right,new_pos_left,new_pos_right, *seqvec,type); break;
                case  CVariation_Base::C_Data::e_Set : 
                    for (CVariation_ref::TData::TSet::TVariations::iterator inst = vr.SetData().SetSet().SetVariations().begin(); inst != vr.SetData().SetSet().SetVariations().end(); ++inst)
                    {
                        if ( (*inst)->IsSetData() && (*inst)->SetData().IsInstance())
                            x_ProcessInstance((*inst)->SetData().SetInstance(),feat.SetLocation(),is_deletion,refref,ref,pos_left,pos_right,new_pos_left,new_pos_right,*seqvec,type);
                    }
                    break;
                default: break;
                
            }
        }
        
        if (!ref.empty() && is_deletion)
        {
            int pos = pos_right - ref.size() + 1;
            const bool found = x_ProcessShift(ref, pos_left,pos,*seqvec,type);
            pos_right = pos + ref.size()-1;
            if (found)
            {
                if (new_pos_left == -1)
                    new_pos_left = pos_left;
                else if (new_pos_left != pos_left)
                {
                    ERR_POST(Error <<   "Left position is ambiguous due to different leaf alleles" << Endm);
                    continue;
                }
                if (new_pos_right == -1)
                    new_pos_right = pos_right;
                else if (new_pos_right != pos_right)
                {
                    ERR_POST(Error <<   "Right position is ambiguous due to different leaf alleles" << Endm);
                    continue;
                }
                x_ModifyLocation(feat.SetLocation(),*refref,ref,pos_left,pos_right,type);
            }
        }
            
    }
}

template<class T>
bool CVariationNormalization_base<T>::x_IsShiftable(CSeq_loc &loc, string &ref, CScope &scope, int type)
{
    if (type != CVariation_inst::eType_ins && type != CVariation_inst::eType_del)
        NCBI_THROW(CException, eUnknown, "Only insertions or deletions can currently be processed");

    const CSeq_id &seq_id = *loc.GetId();
    int pos_left = loc.GetStart(eExtreme_Positional);
    int pos_right = loc.GetStop(eExtreme_Positional);

    ENa_strand strand = eNa_strand_unknown;
    if (loc.IsSetStrand())
        strand = loc.GetStrand();
    CRef<CSeqVector> seqvec = x_PrefetchSequence(scope,seq_id,strand);
    int old_pos_left = pos_left;
    int old_pos_right = pos_right;
    if (!ref.empty()) 
    {
        if (type == CVariation_inst::eType_del)
        {
            int pos = pos_right - ref.size() + 1;
            x_ProcessShift(ref, pos_left,pos,*seqvec,type);
            pos_right = pos + ref.size()-1;
        }
        else
        {  
            string compact = x_CompactifySeq(ref);
            string orig_compact = compact;
            x_ProcessShift(compact, pos_left,pos_right,*seqvec,type);
            while (orig_compact != compact)
            {
                x_rotate_left(ref);
                x_rotate_left(orig_compact);
            }
        }
    }
    else
        NCBI_THROW(CException, eUnknown, "Submitted allele is empty");
    bool unchanged = (pos_left == old_pos_left) && (pos_right == old_pos_right);
    x_ExpandLocation(loc,pos_left,pos_right);
    return !unchanged;
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

void CVariationNormalizationLeft::x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos, int pos_right, int type)
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

void CVariationNormalization::AlterToHGVSVar(CVariation& var, CScope& scope)
{
    CVariationNormalizationRight::x_Shift(var,scope);
}

void CVariationNormalization::AlterToHGVSVar(CSeq_annot& var, CScope& scope)
{
    CVariationNormalizationRight::x_Shift(var,scope);
}

void CVariationNormalization::NormalizeAmbiguousVars(CVariation& var, CScope &scope)
{
    CVariationNormalizationInt::x_Shift(var,scope);
}

bool CVariationNormalization::IsShiftable(CSeq_loc &loc, string &a, CScope &scope, int type)
{
    return CVariationNormalizationInt::x_IsShiftable(loc,a,scope,type);
}

void CVariationNormalization::NormalizeAmbiguousVars(CSeq_annot& var, CScope &scope)
{
    CVariationNormalizationInt::x_Shift(var,scope);
}

void CVariationNormalization::AlterToVarLoc(CVariation& var, CScope& scope)
{
    CVariationNormalizationLeftInt::x_Shift(var,scope);
}

void CVariationNormalization::AlterToVarLoc(CSeq_annot& var, CScope& scope)
{
    CVariationNormalizationLeftInt::x_Shift(var,scope);
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

