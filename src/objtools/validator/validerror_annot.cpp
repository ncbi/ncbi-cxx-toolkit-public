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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   validation of seq_annot
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objtools/validator/validatorp.hpp>
#include <objtools/validator/utilities.hpp>

#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>

#include <objmgr/bioseq_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)


CValidError_annot::CValidError_annot(CValidError_imp& imp) :
    CValidError_base(imp),
    m_GraphValidator(imp),
    m_AlignValidator(imp),
    m_FeatValidator(imp)
{
}


CValidError_annot::~CValidError_annot(void)
{
}


void CValidError_annot::ValidateSeqAnnot(const CSeq_annot_Handle& annot)
{
    if ( annot.IsAlign() ) {
        if ( annot.Seq_annot_IsSetDesc() ) {
    
            ITERATE( list< CRef< CAnnotdesc > >, iter, annot.Seq_annot_GetDesc().Get() ) {
            
                if ( (*iter)->IsUser() ) {
                    const CObject_id& oid = (*iter)->GetUser().GetType();
                    if ( oid.IsStr() ) {
                        if ( oid.GetStr() == "Blast Type" ) {
                            PostErr(eDiag_Error, eErr_SEQ_ALIGN_BlastAligns,
                                "Record contains BLAST alignments", *annot.GetCompleteSeq_annot()); // !!!

                            break;
                        }
                    }
                }
            }
        } // iterate
    } else if ( annot.IsIds() ) {
        PostErr(eDiag_Error, eErr_SEQ_ANNOT_AnnotIDs,
                "Record contains Seq-annot.data.ids", *annot.GetCompleteSeq_annot());
    } else if ( annot.IsLocs() ) {
        PostErr(eDiag_Error, eErr_SEQ_ANNOT_AnnotLOCs,
                "Record contains Seq-annot.data.locs", *annot.GetCompleteSeq_annot());
    }
}


void CValidError_annot::ValidateSeqAnnot(const CSeq_annot& annot)
{
    if ( annot.IsAlign() ) {
        if ( annot.IsSetDesc() ) {
    
            ITERATE( list< CRef< CAnnotdesc > >, iter, annot.GetDesc().Get() ) {
            
                if ( (*iter)->IsUser() ) {
                    const CObject_id& oid = (*iter)->GetUser().GetType();
                    if ( oid.IsStr() ) {
                        if ( oid.GetStr() == "Blast Type" ) {
                            PostErr(eDiag_Error, eErr_SEQ_ALIGN_BlastAligns,
                                "Record contains BLAST alignments", annot); // !!!

                            break;
                        }
                    }
                }
            }
        } // iterate
        if (m_Imp.IsValidateAlignments()) {
            FOR_EACH_ALIGN_ON_ANNOT (align, annot) {
                m_AlignValidator.ValidateSeqAlign (**align);
            }
        }
    } else if ( annot.IsIds() ) {
        PostErr(eDiag_Error, eErr_SEQ_ANNOT_AnnotIDs,
                "Record contains Seq-annot.data.ids", annot);
    } else if ( annot.IsLocs() ) {
        PostErr(eDiag_Error, eErr_SEQ_ANNOT_AnnotLOCs,
                "Record contains Seq-annot.data.locs", annot);
    } else if ( annot.IsGraph()) {
        FOR_EACH_GRAPH_ON_ANNOT (graph, annot) {
            m_GraphValidator.ValidateSeqGraph(**graph);
        }
    } else if ( annot.IsFtable() ) {
        FOR_EACH_FEATURE_ON_ANNOT (feat, annot) {
            m_FeatValidator.ValidateSeqFeat(**feat);
        }
    }
}


void CValidError_annot::ValidateSeqAnnotContext (const CSeq_annot& annot, const CBioseq& seq)
{
    if (annot.IsGraph()) {
        FOR_EACH_GRAPH_ON_ANNOT (graph, annot) {
            m_GraphValidator.ValidateSeqGraphContext (**graph, seq);
        }
    }
}

static bool x_IsEmblOrDdbjOnSet (const CSeq_id& id, CBioseq_set_Handle set)
{
    for (CBioseq_CI b_ci (set); b_ci; ++b_ci) {
        FOR_EACH_SEQID_ON_BIOSEQ (id_it, *(b_ci->GetCompleteBioseq())) {
            switch ( (*id_it)->Which() ) {
            case CSeq_id::e_Embl:
            case CSeq_id::e_Ddbj:
            case CSeq_id::e_Tpe:
            case CSeq_id::e_Tpd:
                return true;
            default:
                break;
            }
        }
    }
    return false;
}


void CValidError_annot::ValidateSeqAnnotContext (const CSeq_annot& annot, const CBioseq_set& set)
{
    if (annot.IsGraph()) {
        FOR_EACH_GRAPH_ON_ANNOT (graph, annot) {
            m_GraphValidator.ValidateSeqGraphContext (**graph, set);
        }
    } else if (annot.IsFtable()) {
        // if a feature is packaged on a set, the bioseqs in the locations should be in the set
        CBioseq_set_Handle bssh = m_Scope->GetBioseq_setHandle(set);
        FOR_EACH_SEQFEAT_ON_SEQANNOT (feat_it, annot) {
            if ((*feat_it)->IsSetLocation()) {
                for ( CSeq_loc_CI loc_it((*feat_it)->GetLocation()); loc_it; ++loc_it ) {
                    const CSeq_id& id = loc_it.GetSeq_id();
                    if (x_IsEmblOrDdbjOnSet(id, bssh)) return;

                    if (!IsBioseqWithIdInSet(id, bssh)) {
                        if (m_Imp.IsSmallGenomeSet()) {
                            m_Imp.IncrementSmallGenomeSetMisplacedCount();
                        } else {
                            m_Imp.IncrementMisplacedFeatureCount();
                        }
                        break;
                    }
                }
            }
        }
    }

}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
