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
    string old_ref = v->SetData().SetSet().SetVariations().front()->SetPlacements().front()->SetSeq().SetSeq_data().SetIupacna().Set(); 
    string new_ref = GetRefAlleleFromVP(v->SetData().SetSet().SetVariations().front()->SetPlacements().front(), scope);
    v->SetData().SetSet().SetVariations().front()->SetPlacements().front()->SetSeq().SetSeq_data().SetIupacna().Set(new_ref);
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
        NCBI_THROW(CException, eUnknown, "Location is not set in input Seq-annot");
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
        if ((*inst)->GetData().GetInstance().IsSetObservation() && ((*inst)->GetData().GetInstance().GetObservation() & CVariation_inst::eObservation_reference) == CVariation_inst::eObservation_reference &&
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
            if (old_ref  == a) add_old_ref = false;
            if (!((*inst)->GetData().GetInstance().IsSetObservation() && ((*inst)->GetData().GetInstance().GetObservation() & CVariation_inst::eObservation_reference) == CVariation_inst::eObservation_reference))
            {
                type = (*inst)->SetData().SetInstance().SetType();
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
string CVariationUtilities::GetRefAlleleFromVP(CRef<CVariantPlacement> vp, CScope& scope)
{
    string new_ref;
    TSeqPos length = vp->GetSeq().GetLength();
    
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
    for (CVariation::TData::TSet::TVariations::iterator v1 = v->SetData().SetSet().SetVariations().begin(); v1 != v->SetData().SetSet().SetVariations().end(); ++v1)
        if ((*v1)->IsSetPlacements())
        {
            CRef<CVariantPlacement> vp1 =  (*v1)->SetPlacements().front(); // should only be a single placement for each second level variation at this point
            bool found_ref = false;
            int type = CVariation_inst::eType_snv;
            CVariation::TData::TSet::TVariations::iterator var2 = (*v1)->SetData().SetSet().SetVariations().begin();
            while ( var2 != (*v1)->SetData().SetSet().SetVariations().end() )
            {
                bool remove_var = false;
                if ( (*var2)->IsSetData() && (*var2)->SetData().IsInstance() && (*var2)->SetData().SetInstance().IsSetDelta() && !(*var2)->SetData().SetInstance().SetDelta().empty()
                     && (*var2)->SetData().SetInstance().SetDelta().front()->IsSetSeq() && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().IsLiteral()
                     && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().IsSetSeq_data() 
                     && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().IsIupacna())
                {
                    string a = (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(); 
                    type = (*var2)->SetData().SetInstance().SetType(); // Here is hoping that type is the same for all leaves at this placement
                    if (!a.empty())
                    {
                        if (a == new_ref)
                        {
                            remove_var = true;
                            found_ref = true;
                        }
                        if (a == old_ref)
                            old_ref.clear();
                    }
                }
                if (remove_var)
                    var2 = (*v1)->SetData().SetSet().SetVariations().erase(var2);
                else
                    ++var2;
            }

            if (!new_ref.empty())
            {
                if (new_ref == old_ref)
                    old_ref.clear();
                //else if (!found_ref)
                //    NCBI_THROW(CException, eUnknown, "New reference allele is not found in the set of existing alleles " + new_ref);
            }

            if (!old_ref.empty())
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

// Variation Normalization
template<class T>
void CVariationNormalization_base<T>::x_rotate_left(string &v)
{
    // simple rotation to the left
    std::rotate(v.begin(), v.begin() + 1, v.end());
}

template<class T>
void CVariationNormalization_base<T>::x_rotate_right(string &v)
{
    // simple rotation to the right
    std::rotate(v.rbegin(), v.rbegin() + 1, v.rend());
}

template<class T>
void CVariationNormalization_base<T>::x_PrefetchSequence(CScope &scope, string accession)
{
    if (accession != m_Accession)
    {
        m_Accession = accession;
        m_Sequence.clear();
        CSeq_id seq_id( accession );
        const CBioseq_Handle& bsh = scope.GetBioseqHandle( seq_id );
        CSeqVector seq_vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
        seq_vec.GetSeqData(0, seq_vec.size(), m_Sequence);
    }
}

template<class T>
string CVariationNormalization_base<T>::x_GetSeq(int pos, int length)
{
    _ASSERT(!m_Sequence.empty());
    
    return m_Sequence.substr(pos,length);
}
 
bool CVariationNormalizationLeft::x_ProcessShift(string &a, int &pos)
{
    bool found = false;
    for (unsigned int i=0; i<a.size(); i++)
    {
        string b = x_GetSeq(pos,a.size());
        if (a == b) 
        {
            found = true;
            break;
        }
        x_rotate_left(a);
    }
    if (!found && pos > a.size())
    {
        pos -= a.size();
        for (unsigned int i=0; i<a.size(); i++)
        {
            string b = x_GetSeq(pos,a.size());
            if (a == b) 
            {
                found = true;
                break;
            }
            x_rotate_left(a);
        }
    }
    if (!found) return false;
                            
    bool found_left = false;
    while (pos >= 0)
    {
        pos--; 
        x_rotate_right(a);
        string b = x_GetSeq(pos,a.size());
        if (a != b) 
        {
            pos++;
            x_rotate_left(a);
            found_left = true;
            break;
        }
    }
    return found_left;                         
}

void CVariationNormalizationLeft::x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos)
{
    if (loc.IsInt())
    {
        CSeq_point pnt;
        pnt.SetPoint(pos);
        pnt.SetStrand( loc.GetInt().GetStrand() );
        pnt.SetId().Assign(loc.GetInt().GetId());
        loc.SetPnt().Assign(pnt);
    }
    else
        loc.SetPnt().SetPoint(pos); 
    literal.SetSeq_data().SetIupacna().Set(a);
}

template<class T>
void CVariationNormalization_base<T>::x_Shift(CRef<CVariation>& v, CScope &scope)
{
    if (v->IsSetPlacements())
        for (CVariation::TPlacements::iterator vp1 =  v->SetPlacements().begin(); vp1 != v->SetPlacements().end(); ++vp1)
        {
            int pos;
            string acc;
            int version;
            if ((*vp1)->IsSetLoc())
            {
                if ((*vp1)->GetLoc().IsPnt() && (*vp1)->GetLoc().GetPnt().IsSetPoint() && (*vp1)->GetLoc().GetPnt().IsSetId() && (*vp1)->GetLoc().GetPnt().GetId().IsOther()
                    && (*vp1)->GetLoc().GetPnt().GetId().GetOther().IsSetAccession() && (*vp1)->GetLoc().GetPnt().GetId().GetOther().IsSetVersion())
                {
                    acc = (*vp1)->GetLoc().GetPnt().GetId().GetOther().GetAccession();
                    version = (*vp1)->GetLoc().GetPnt().GetId().GetOther().GetVersion();
                    pos = (*vp1)->GetLoc().GetPnt().GetPoint();
                }
                else if ((*vp1)->GetLoc().IsInt())
                {
                    acc = (*vp1)->GetLoc().GetInt().GetId().GetOther().GetAccession();
                    version = (*vp1)->GetLoc().GetInt().GetId().GetOther().GetVersion();
                    pos = (*vp1)->GetLoc().GetInt().GetFrom();
                }
                else
                    NCBI_THROW(CException, eUnknown, "Placement is neither point nor interval");
                int new_pos = -1;
                stringstream accession;
                accession << acc << "." << version;
                x_PrefetchSequence(scope,accession.str());
                
                for ( CVariation::TData::TSet::TVariations::iterator var2 = v->SetData().SetSet().SetVariations().begin(); var2 != v->SetData().SetSet().SetVariations().end();  ++var2 )
                    if ( (*var2)->IsSetData() && (*var2)->SetData().IsInstance() && (*var2)->SetData().SetInstance().IsSetDelta() && !(*var2)->SetData().SetInstance().SetDelta().empty()
                         && (*var2)->SetData().SetInstance().SetDelta().front()->IsSetSeq() && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().IsLiteral()
                         && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().IsSetSeq_data() 
                         && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().IsIupacna())
                    {
                        string a = (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(); 
                        //cout << x_GetSeq(pos-a.size(),3*a.size()) << endl;
                        //cout << setw(a.size()) << " ";
                        //cout << a <<endl;
                        int type = (*var2)->SetData().SetInstance().SetType(); 
                        if (!a.empty() && type == CVariation_inst::eType_ins)
                        {
                            bool found = x_ProcessShift(a, pos);
                            if (found)
                            {
                                if (new_pos == -1)
                                    new_pos = pos;
                                else if (new_pos != pos)
                                    NCBI_THROW(CException, eUnknown, "Position is ambiguous due to different leaf alleles");
                                x_ModifyLocation((*vp1)->SetLoc(),(*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral(),a,pos);
                            }
                        }
                    }
            }
        }
}

template<class T>
void CVariationNormalization_base<T>::x_Shift(CRef<CSeq_annot>& var, CScope &scope)
{
    // TODO 
}

void CVariationNormalization::AlterToVCFVar(CRef<CVariation>& var, CScope &scope)
{
    CVariationNormalizationLeft::x_Shift(var,scope);
}

void CVariationNormalization::AlterToVCFVar(CRef<CSeq_annot>& var, CScope& scope)
{
    CVariationNormalizationLeft::x_Shift(var,scope);
}

template<class T>
string CVariationNormalization_base<T>::m_Sequence;
template<class T>
string CVariationNormalization_base<T>::m_Accession;
