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
 * Authors:  Josh Cherry
 *
 * File Description:  Make protein products of coding region features
 *
 */


#include <ncbi_pch.hpp>
#include <algo/sequence/make_cdr_prods.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CAtomicCounter CMakeCdrProds::sm_Counter;

CRef<CBioseq_set> CMakeCdrProds::MakeCdrProds(CRef<CSeq_annot> annot,
                                              CBioseq_Handle handle)
{
    CRef<CBioseq_set> bioseq_set(new CBioseq_set);
    if (!annot->GetData().IsFtable()) {
        // Is this the right thing to do?
        // Could throw, or could return null CRef instead.
        return bioseq_set;
    }

    list<CRef<CSeq_feat> >& ftable = annot->SetData().SetFtable();

    NON_CONST_ITERATE (list<CRef<CSeq_feat> >, feat, ftable) {
        if (!(*feat)->GetData().IsCdregion()) {
            // not interested if not a Cdregion
            continue;
        }
        if ((*feat)->IsSetProduct()) {
            // already has a product; don't make new one
            continue;
        }

        string prot;
        CSeqTranslator::Translate(**feat, handle.GetScope(), prot);
        CRef<CSeq_data> seq_data(new CSeq_data(prot,
                                               CSeq_data::e_Iupacaa));
        CRef<CSeq_inst> seq_inst(new CSeq_inst);
        seq_inst->SetSeq_data(*seq_data);
        seq_inst->SetRepr(CSeq_inst_Base::eRepr_raw);
        seq_inst->SetMol(CSeq_inst_Base::eMol_aa);
        seq_inst->SetLength(prot.size());

        CRef<CBioseq> bio_seq(new CBioseq);
        string num = NStr::NumericToString(sm_Counter.Add(1));
        // pad to five digits
        if (num.size() < 5) {
            num.insert(SIZE_TYPE(0), 5 - num.size(), '0');
        }
        string acc = "tp" + num;
        string full_acc = "lcl|" + acc;
        CRef<CSeq_id> id(new CSeq_id(full_acc));
        bio_seq->SetId().push_back(id);
        // a title
        CRef<CSeqdesc> title(new CSeqdesc);
        title->SetTitle(string("Translation product ") + acc);
        bio_seq->SetDescr().Set().push_back(title);
        // Mol_type
        CRef<CSeqdesc> mol_type(new CSeqdesc);
        mol_type->SetMol_type( eGIBB_mol_peptide);
        bio_seq->SetDescr().Set().push_back(mol_type);
        
        // set the instance
        bio_seq->SetInst(*seq_inst);
        
        // wrap this Bio_seq in an entry
        CRef<CSeq_entry> seq_entry(new CSeq_entry);
        seq_entry->SetSeq(*bio_seq);
        
        // add this entry to our Bioseq_set
        bioseq_set->SetSeq_set().push_back(seq_entry);

        // record it as product in the annot we're handed
        CRef<CSeq_loc> prod_loc(new CSeq_loc);
        prod_loc->SetWhole(*id);
        (*feat)->SetProduct(*prod_loc);
    }

    return bioseq_set;
}


END_NCBI_SCOPE
