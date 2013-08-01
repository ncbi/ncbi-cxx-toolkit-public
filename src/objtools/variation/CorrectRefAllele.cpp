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

#include <objtools/variation/CorrectRefAllele.hpp>

void CVariationUtilities::CorrectRefAllele(CRef<CVariation>& v, CScope& scope)
{
    string old_ref = v->SetData().SetSet().SetVariations().front()->SetPlacements().front()->SetSeq().SetSeq_data().SetIupacna().Set(); 
    string new_ref = GetRefAlleleFromVP(v->SetData().SetSet().SetVariations().front()->SetPlacements().front(), scope);
    FixAlleles(v,old_ref,new_ref);
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
                else if (!found_ref)
                    NCBI_THROW(CException, eUnknown, "New reference allele is not found in the set of existing alleles " + new_ref);
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




