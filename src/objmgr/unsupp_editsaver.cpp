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
 * Author:  Maxim Didenko
 *
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistre.hpp>

#include <objmgr/unsupp_editsaver.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void CUnsupportedEditSaver::AddDescr(const CBioseq_Handle&, 
                              const CSeq_descr&, 
                              ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "AddDescr(const CBioseq_Handle& const CSeq_descr&, ECallMode)");
}

void CUnsupportedEditSaver::AddDescr(const CBioseq_set_Handle&, 
                              const CSeq_descr&, 
                              ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "AddDescr(const CBioseq_set_Handle&, const CSeq_descr&, ECallMode)" );
}

void CUnsupportedEditSaver::SetDescr(const CBioseq_Handle&, 
                              const CSeq_descr&, ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "SetDescr(const CBioseq_Handle&, const CSeq_descr&, ECallMode)");
}
void CUnsupportedEditSaver::SetDescr(const CBioseq_set_Handle&, 
                              const CSeq_descr&, 
                              ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "SetDescr(const CBioseq_set_Handle&, const CSeq_descr&, ECallMode)");
}

void CUnsupportedEditSaver::ResetDescr(const CBioseq_Handle&, 
                                       ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "ResetDescr(const CBioseq_Handle&, ECallMode)");
}
void CUnsupportedEditSaver::ResetDescr(const CBioseq_set_Handle&, 
                                       ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "ResetDescr(const CBioseq_set_Handle&, ECallMode)");
}

void CUnsupportedEditSaver::AddDesc(const CBioseq_Handle&, 
                             const CSeqdesc&, ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "AddDesc(const CBioseq_Handle&, const CSeqdesc&, ECallMode)");
}
void CUnsupportedEditSaver::AddDesc(const CBioseq_set_Handle&, 
                             const CSeqdesc&, ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "AddDesc(const CBioseq_set_Handle&, const CSeqdesc&, ECallMode)");
}

void CUnsupportedEditSaver::RemoveDesc(const CBioseq_Handle&, 
                                const CSeqdesc&, ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "RemoveDesc(const CBioseq_Handle&, const CSeqdesc&, ECallMode)");
}
void CUnsupportedEditSaver::RemoveDesc(const CBioseq_set_Handle&, 
                                const CSeqdesc&, ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "RemoveDesc(const CBioseq_set_Handle&, const CSeqdesc&, ECallMode)");
}

    //------------------------------------------------------------------
void CUnsupportedEditSaver::SetSeqInst(const CBioseq_Handle&, 
                                const CSeq_inst&, ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "SetSeqInst(const CBioseq_Handle&, const CSeq_inst&, ECallMode)");
}
void CUnsupportedEditSaver::SetSeqInstRepr(const CBioseq_Handle&, 
                                    CSeq_inst::TRepr, ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "SetSeqInstRepr(const CBioseq_Handle&, CSeq_inst::TRepr, ECallMode)");
}
void CUnsupportedEditSaver::SetSeqInstMol(const CBioseq_Handle&, 
                                   CSeq_inst::TMol, ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "SetSeqInstMol(const CBioseq_Handle&, CSeq_inst::TMol, ECallMode)");
}
void CUnsupportedEditSaver::SetSeqInstLength(const CBioseq_Handle&, 
                                      CSeq_inst::TLength,
                                      ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "SetSeqInstLength(const CBioseq_Handle&, CSeq_inst::TLength, ECallMode)");
}
void CUnsupportedEditSaver::SetSeqInstFuzz(const CBioseq_Handle&, 
                                    const CSeq_inst::TFuzz&, 
                                    ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "SetSeqInstFuzz(const CBioseq_Handle&, const CSeq_inst::TFuzz&, ECallMode)");
}
void CUnsupportedEditSaver::SetSeqInstTopology(const CBioseq_Handle&, 
                                        CSeq_inst::TTopology,
                                        ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "SetSeqInstTopology(const CBioseq_Handle&, CSeq_inst::TTopology, ECallMode)");
}
void CUnsupportedEditSaver::SetSeqInstStrand(const CBioseq_Handle&, 
                                      CSeq_inst::TStrand, 
                                      ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "SetSeqInstStrand(const CBioseq_Handle&, CSeq_inst::TStrand, ECallMode)");
}
void CUnsupportedEditSaver::SetSeqInstExt(const CBioseq_Handle&, 
                                   const CSeq_inst::TExt&, 
                                   ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "SetSeqInstExt(const CBioseq_Handle&, const CSeq_inst::TExt&, ECallMode)");
}
void CUnsupportedEditSaver::SetSeqInstHist(const CBioseq_Handle&, 
                                    const CSeq_inst::THist&, 
                                    ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "SetSeqInstHist(const CBioseq_Handle&, const CSeq_inst::THist&, ECallMode)");
}
void CUnsupportedEditSaver::SetSeqInstSeq_data(const CBioseq_Handle&, 
                                        const CSeq_inst::TSeq_data&, 
                                        ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "SetSeqInstSeq_data(const CBioseq_Handle&, const CSeq_inst::TSeq_data&, ECallMode)");
}
    
void CUnsupportedEditSaver::ResetSeqInst(const CBioseq_Handle&, 
                                         ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "ResetSeqInst(const CBioseq_Handle&, ECallMode)");
}
void CUnsupportedEditSaver::ResetSeqInstRepr(const CBioseq_Handle&, 
                                             ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "ResetSeqInstRepr(const CBioseq_Handle&, ECallMode)");
}
void CUnsupportedEditSaver::ResetSeqInstMol(const CBioseq_Handle&, 
                                            ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "ResetSeqInstMol(const CBioseq_Handle&, ECallMode)");
}
void CUnsupportedEditSaver::ResetSeqInstLength(const CBioseq_Handle&, 
                                               ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "ResetSeqInstLength(const CBioseq_Handle&, ECallMode)");
}
void CUnsupportedEditSaver::ResetSeqInstFuzz(const CBioseq_Handle&, 
                                             ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "ResetSeqInstFuzz(const CBioseq_Handle&, ECallMode)");
}
void CUnsupportedEditSaver::ResetSeqInstTopology(const CBioseq_Handle&, 
                                                 ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "ResetSeqInstFuzz(const CBioseq_Handle&, ECallMode)");
}
void CUnsupportedEditSaver::ResetSeqInstStrand(const CBioseq_Handle&, 
                                               ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "ResetSeqInstStrand(const CBioseq_Handle&, ECallMode)");
}
void CUnsupportedEditSaver::ResetSeqInstExt(const CBioseq_Handle&, 
                                            ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "ResetSeqInstExt(const CBioseq_Handle&, ECallMode)");
}
void CUnsupportedEditSaver::ResetSeqInstHist(const CBioseq_Handle&, 
                                             ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "ResetSeqInstHist(const CBioseq_Handle&, ECallMode)");
}
void CUnsupportedEditSaver::ResetSeqInstSeq_data(const CBioseq_Handle&, 
                                                 ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "ResetSeqInstSeq_data(const CBioseq_Handle&, ECallMode)");
}

    //----------------------------------------------------------------
void CUnsupportedEditSaver::AddId(const CBioseq_Handle&, 
                                  const CSeq_id_Handle&, 
                                  ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "AddId(const CBioseq_Handle&, const CSeq_id_Handle&, ECallMode)");
}
void CUnsupportedEditSaver::RemoveId(const CBioseq_Handle&, 
                                     const CSeq_id_Handle&, 
                                     ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "RemoveId(const CBioseq_Handle&, const CSeq_id_Handle&, ECallMode)");
}
void CUnsupportedEditSaver::ResetIds(const CBioseq_Handle&, 
                                     const TIds&,
                                     ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "ResetIds(const CBioseq_Handle&, ECallMode)");
}

void CUnsupportedEditSaver::SetBioseqSetId(const CBioseq_set_Handle&,
                                           const CBioseq_set::TId&, 
                                           ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "SetBioseqSetId(const CBioseq_set_Handle&, const CBioseq_set::TId&, ECallMode)");
}
void CUnsupportedEditSaver::SetBioseqSetColl(const CBioseq_set_Handle&,
                                             const CBioseq_set::TColl&, 
                                             ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "SetBioseqSetColl(const CBioseq_set_Handle&, const CBioseq_set::TColl&, ECallMode)"); 
}
void CUnsupportedEditSaver::SetBioseqSetLevel(const CBioseq_set_Handle&,
                                              CBioseq_set::TLevel, 
                                              ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "SetBioseqSetLevel(const CBioseq_set_Handle&, CBioseq_set::TLevel, ECallMode)");
}
void CUnsupportedEditSaver::SetBioseqSetClass(const CBioseq_set_Handle&,
                                              CBioseq_set::TClass, 
                                              ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "SetBioseqSetClass(const CBioseq_set_Handle&, CBioseq_set::TClass, ECallMode)");
}
void CUnsupportedEditSaver::SetBioseqSetRelease(const CBioseq_set_Handle&,
                                                const CBioseq_set::TRelease&, 
                                                ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "SetBioseqSetRelease(const CBioseq_set_Handle&, "
               "const CBioseq_set::TRelease&, ECallMode)");
}
void CUnsupportedEditSaver::SetBioseqSetDate(const CBioseq_set_Handle&,
                                             const CBioseq_set::TDate&, 
                                             ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "SetBioseqSetDate(const CBioseq_set_Handle&, const CBioseq_set::TDate&, ECallMode)");
}
 
void CUnsupportedEditSaver::ResetBioseqSetId(const CBioseq_set_Handle&, 
                                             ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "ResetBioseqSetId(const CBioseq_set_Handle&, ECallMode)");
}
void CUnsupportedEditSaver::ResetBioseqSetColl(const CBioseq_set_Handle&, 
                                               ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "ResetBioseqSetColl(const CBioseq_set_Handle&, ECallMode)");
}
void CUnsupportedEditSaver::ResetBioseqSetLevel(const CBioseq_set_Handle&, 
                                                ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "ResetBioseqSetLevel(const CBioseq_set_Handle&, ECallMode)");
}
void CUnsupportedEditSaver::ResetBioseqSetClass(const CBioseq_set_Handle&, 
                                                ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "ResetBioseqSetClass(const CBioseq_set_Handle&, ECallMode)");
}
void CUnsupportedEditSaver::ResetBioseqSetRelease(const CBioseq_set_Handle&, 
                                                  ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "ResetBioseqSetRelease(const CBioseq_set_Handle&, ECallMode)");
}
void CUnsupportedEditSaver::ResetBioseqSetDate(const CBioseq_set_Handle&,
                                               ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "ResetBioseqSetRelease(const CBioseq_set_Handle&, ECallMode)");
}
  
    //-----------------------------------------------------------------
void CUnsupportedEditSaver::Attach(const CBioObjectId&,
                                   const CSeq_entry_Handle&, 
                                   const CBioseq_Handle&, 
                                   ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "Attach(const CBioObjectId&, const CSeq_entry_Handle&, const CBioseq_Handle&, ECallMode)");
}
void CUnsupportedEditSaver::Attach(const CBioObjectId&,
                                   const CSeq_entry_Handle&, 
                                   const CBioseq_set_Handle&, 
                                   ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "Attach(const CBioObjectId& ,const CSeq_entry_Handle&, const CBioseq_set_Handle&, ECallMode)");
}
void CUnsupportedEditSaver::Detach(const CSeq_entry_Handle&, 
                                   const CBioseq_Handle&, ECallMode )
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "Detach(const CSeq_entry_Handle&, const CBioseq_Handle&, ECallMode)");
}
void CUnsupportedEditSaver::Detach(const CSeq_entry_Handle&, 
                                   const CBioseq_set_Handle&, ECallMode )
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "Detach(const CSeq_entry_Handle&, const CBioseq_set_Handle&, ECallMode)");
}

void CUnsupportedEditSaver::Attach(const CSeq_entry_Handle&, 
                                   const CSeq_annot_Handle&, 
                                   ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "Attach(const CSeq_entry_Handle&, const CSeq_annot_Handle&, ECallMode)");
}
void CUnsupportedEditSaver::Remove(const CSeq_entry_Handle&, 
                                   const CSeq_annot_Handle&, 
                                   ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "Remove(const CSeq_entry_Handle&, const CSeq_annot_Handle&, ECallMode)");
}

void CUnsupportedEditSaver::Attach(const CBioseq_set_Handle&, 
                            const CSeq_entry_Handle&, 
                            int /*Index*/, ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "Attach(const CBioseq_set_Handle&, const CSeq_entry_Handle&, int, ECallMode)");
}
void CUnsupportedEditSaver::Remove(const CBioseq_set_Handle&, 
                                   const CSeq_entry_Handle&, 
                                   int /*Index*/, ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "Remove(const CBioseq_set_Handle&, const CSeq_entry_Handle&, int, ECallMode)");
}
void CUnsupportedEditSaver::RemoveTSE(const CTSE_Handle&, 
                                      ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "RemoveTSE(const CTSE_Handle&, ECallMode)");
}


    //-----------------------------------------------------------------

void CUnsupportedEditSaver::Replace(const CSeq_feat_Handle&,
                                    const CSeq_feat&, 
                                    ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "Replace(const CSeq_feat_Handle&, const CSeq_feat&, ECallMode)");
}
void CUnsupportedEditSaver::Replace(const CSeq_align_Handle&,
                                    const CSeq_align&, 
                                    ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "Replace(const CSeq_align_Handle&, const CSeq_align&, ECallMode)");
}
void CUnsupportedEditSaver::Replace(const CSeq_graph_Handle&,
                                    const CSeq_graph&, 
                                    ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "Replace(const CSeq_graph_Handle&, const CSeq_graphfeat&, ECallMode)");
}

void CUnsupportedEditSaver::Add(const CSeq_annot_Handle&,
                                const CSeq_feat&, 
                                ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "Add(const CSeq_annot_Handle&, const CSeq_feat&, ECallMode)");
}
void CUnsupportedEditSaver::Add(const CSeq_annot_Handle&,
                                const CSeq_align&, 
                                ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "Add(const CSeq_annot_Handle&, const CSeq_align&, ECallMode)");
}
void CUnsupportedEditSaver::Add(const CSeq_annot_Handle&,
                                const CSeq_graph&, 
                                ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "Add(const CSeq_annot_Handle&, const CSeq_graph&, ECallMode)");
}

void CUnsupportedEditSaver::Remove(const CSeq_annot_Handle&, 
                                   const CSeq_feat&, 
                                   ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "Remove(const CSeq_annot_Handle&, const CSeq_feat&, ECallMode)");
}
void CUnsupportedEditSaver::Remove(const CSeq_annot_Handle&, 
                                   const CSeq_align&, 
                                   ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "Remove(const CSeq_annot_Handle&, const CSeq_align&, ECallMode)");
}
void CUnsupportedEditSaver::Remove(const CSeq_annot_Handle&, 
                                   const CSeq_graph&, 
                                   ECallMode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "Remove(const CSeq_annot_Handle&, const CSeq_graph&, ECallMode)");
}

END_SCOPE(objects)
END_NCBI_SCOPE
