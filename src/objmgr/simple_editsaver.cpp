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

#include <objmgr/simple_editsaver.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objmgr/seq_align_handle.hpp>
#include <objmgr/seq_graph_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void CSimpleEditSaver::AddDescr(const CBioseq_Handle& handle, 
                                const CSeq_descr&, 
                                IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}

void CSimpleEditSaver::AddDescr(const CBioseq_set_Handle& handle, 
                                const CSeq_descr&, 
                                IEditSaver::ECallMode mode)
{
    UpdateSet(handle, mode);
}

void CSimpleEditSaver::SetDescr(const CBioseq_Handle& handle, 
                                const CSeq_descr&, IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
void CSimpleEditSaver::SetDescr(const CBioseq_set_Handle& handle, 
                                const CSeq_descr&, 
                                IEditSaver::ECallMode mode)
{
    UpdateSet(handle, mode);
}

void CSimpleEditSaver::ResetDescr(const CBioseq_Handle& handle, 
                                  IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
void CSimpleEditSaver::ResetDescr(const CBioseq_set_Handle& handle, 
                                  IEditSaver::ECallMode mode)
{
    UpdateSet(handle, mode);
}

void CSimpleEditSaver::AddDesc(const CBioseq_Handle& handle, 
                               const CSeqdesc&, IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
void CSimpleEditSaver::AddDesc(const CBioseq_set_Handle& handle, 
                               const CSeqdesc&, IEditSaver::ECallMode mode)
{
    UpdateSet(handle, mode);
}

void CSimpleEditSaver::RemoveDesc(const CBioseq_Handle& handle, 
                                  const CSeqdesc&, IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
void CSimpleEditSaver::RemoveDesc(const CBioseq_set_Handle& handle, 
                                  const CSeqdesc&, IEditSaver::ECallMode mode)
{
    UpdateSet(handle, mode);
}

    //------------------------------------------------------------------
void CSimpleEditSaver::SetSeqInst(const CBioseq_Handle& handle, 
                                  const CSeq_inst&, IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
void CSimpleEditSaver::SetSeqInstRepr(const CBioseq_Handle& handle, 
                                      CSeq_inst::TRepr, IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
void CSimpleEditSaver::SetSeqInstMol(const CBioseq_Handle& handle, 
                                     CSeq_inst::TMol, IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
void CSimpleEditSaver::SetSeqInstLength(const CBioseq_Handle& handle, 
                                        CSeq_inst::TLength,
                                        IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
void CSimpleEditSaver::SetSeqInstFuzz(const CBioseq_Handle& handle, 
                                      const CSeq_inst::TFuzz&, 
                                      IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
void CSimpleEditSaver::SetSeqInstTopology(const CBioseq_Handle& handle, 
                                          CSeq_inst::TTopology,
                                          IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
void CSimpleEditSaver::SetSeqInstStrand(const CBioseq_Handle& handle, 
                                        CSeq_inst::TStrand, 
                                        IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
void CSimpleEditSaver::SetSeqInstExt(const CBioseq_Handle& handle, 
                                     const CSeq_inst::TExt&, 
                                     IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
void CSimpleEditSaver::SetSeqInstHist(const CBioseq_Handle& handle, 
                                      const CSeq_inst::THist&, 
                                      IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
void CSimpleEditSaver::SetSeqInstSeq_data(const CBioseq_Handle& handle, 
                                          const CSeq_inst::TSeq_data&, 
                                          IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
    
void CSimpleEditSaver::ResetSeqInst(const CBioseq_Handle& handle, 
                                    IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
void CSimpleEditSaver::ResetSeqInstRepr(const CBioseq_Handle& handle, 
                                        IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
void CSimpleEditSaver::ResetSeqInstMol(const CBioseq_Handle& handle, 
                                       IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
void CSimpleEditSaver::ResetSeqInstLength(const CBioseq_Handle& handle, 
                                          IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
void CSimpleEditSaver::ResetSeqInstFuzz(const CBioseq_Handle& handle, 
                                        IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
void CSimpleEditSaver::ResetSeqInstTopology(const CBioseq_Handle& handle, 
                                            IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
void CSimpleEditSaver::ResetSeqInstStrand(const CBioseq_Handle& handle, 
                                          IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
void CSimpleEditSaver::ResetSeqInstExt(const CBioseq_Handle& handle, 
                                       IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
void CSimpleEditSaver::ResetSeqInstHist(const CBioseq_Handle& handle, 
                                        IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}
void CSimpleEditSaver::ResetSeqInstSeq_data(const CBioseq_Handle& handle, 
                                            IEditSaver::ECallMode mode)
{
    UpdateSeq(handle, mode);
}

    //----------------------------------------------------------------

void CSimpleEditSaver::AddId(const CBioseq_Handle& handle, 
                             const CSeq_id_Handle&, 
                             IEditSaver::ECallMode mode)
{
    UpdateTSE(handle.GetTopLevelEntry(), mode);
    /*
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "AddId(const CBioseq_Handle&, const CSeq_id_Handle&, ECallMode)");
    */
}
void CSimpleEditSaver::RemoveId(const CBioseq_Handle& handle, 
                                const CSeq_id_Handle&, 
                                IEditSaver::ECallMode mode)
{
    UpdateTSE(handle.GetTopLevelEntry(), mode);
    /*
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "RemoveId(const CBioseq_Handle&, const CSeq_id_Handle&, ECallMode)");
    */
}
void CSimpleEditSaver::ResetIds(const CBioseq_Handle& handle, 
                                IEditSaver::ECallMode mode)
{
    UpdateTSE(handle.GetTopLevelEntry(), mode);
    /*
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "ResetIds(const CBioseq_Handle&, ECallMode)");
    */
}
//-------------------------------------------------------
void CSimpleEditSaver::SetBioseqSetId(const CBioseq_set_Handle& handle,
                                      const CBioseq_set::TId&, 
                                      IEditSaver::ECallMode mode)
{
    UpdateSet(handle, mode);
}
void CSimpleEditSaver::SetBioseqSetColl(const CBioseq_set_Handle& handle,
                                        const CBioseq_set::TColl&, 
                                        IEditSaver::ECallMode mode)
{
    UpdateSet(handle, mode);
}
void CSimpleEditSaver::SetBioseqSetLevel(const CBioseq_set_Handle& handle,
                                         CBioseq_set::TLevel, 
                                         IEditSaver::ECallMode mode)
{
    UpdateSet(handle, mode);
}
void CSimpleEditSaver::SetBioseqSetClass(const CBioseq_set_Handle& handle,
                                         CBioseq_set::TClass, 
                                         IEditSaver::ECallMode mode)
{
    UpdateSet(handle, mode);
}
void CSimpleEditSaver::SetBioseqSetRelease(const CBioseq_set_Handle& handle,
                                           const CBioseq_set::TRelease&, 
                                           IEditSaver::ECallMode mode)
{
    UpdateSet(handle, mode);
}
void CSimpleEditSaver::SetBioseqSetDate(const CBioseq_set_Handle& handle,
                                        const CBioseq_set::TDate&, 
                                        IEditSaver::ECallMode mode)
{
    UpdateSet(handle, mode);
}
 
void CSimpleEditSaver::ResetBioseqSetId(const CBioseq_set_Handle& handle, 
                                        IEditSaver::ECallMode mode)
{
    UpdateSet(handle, mode);
}
void CSimpleEditSaver::ResetBioseqSetColl(const CBioseq_set_Handle& handle, 
                                          IEditSaver::ECallMode mode)
{
    UpdateSet(handle, mode);
}
void CSimpleEditSaver::ResetBioseqSetLevel(const CBioseq_set_Handle& handle, 
                                           IEditSaver::ECallMode mode)
{
    UpdateSet(handle, mode);
}
void CSimpleEditSaver::ResetBioseqSetClass(const CBioseq_set_Handle& handle, 
                                           IEditSaver::ECallMode mode)
{
    UpdateSet(handle, mode);
}
void CSimpleEditSaver::ResetBioseqSetRelease(const CBioseq_set_Handle& handle, 
                                             IEditSaver::ECallMode mode)
{
    UpdateSet(handle, mode);
}
void CSimpleEditSaver::ResetBioseqSetDate(const CBioseq_set_Handle& handle,
                                          IEditSaver::ECallMode mode)
{
    UpdateSet(handle, mode);
}
  
    //-----------------------------------------------------------------

void CSimpleEditSaver::Attach(const CSeq_entry_Handle& handle, 
                              const CBioseq_Handle&, 
                              IEditSaver::ECallMode mode)
{
    UpdateTSE(handle.GetTopLevelEntry(), mode);
    /*
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "Attach(const CSeq_entry_Handle&, const CBioseq_Handle&, ECallMode)");
    */
}
void CSimpleEditSaver::Attach(const CSeq_entry_Handle& handle, 
                              const CBioseq_set_Handle&, 
                              IEditSaver::ECallMode mode)
{
    UpdateTSE(handle.GetTopLevelEntry(), mode);
    /*
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "Attach(const CSeq_entry_Handle&, const CBioseq_set_Handle&, ECallMode)");
    */
}
void CSimpleEditSaver::Reset(const CSeq_entry_Handle& handle, 
                             IEditSaver::ECallMode mode)
{
    UpdateTSE(handle.GetTopLevelEntry(), mode);
    /*
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "Reset(const CSeq_entry_Handle&, ECallMode)");
    */
}

void CSimpleEditSaver::Attach(const CSeq_entry_Handle& entry, 
                              const CSeq_annot_Handle&, 
                              IEditSaver::ECallMode mode)
{
    UpdateEntry(entry, mode);
}
void CSimpleEditSaver::Remove(const CSeq_entry_Handle& entry, 
                              const CSeq_annot_Handle&, 
                              IEditSaver::ECallMode mode)
{
    UpdateEntry(entry, mode);
}
void CSimpleEditSaver::Attach(const CBioseq_set_Handle& handle, 
                              const CSeq_entry_Handle&, 
                              int, IEditSaver::ECallMode mode)
{
    UpdateTSE(handle.GetTopLevelEntry(), mode);
    /*
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "Attach(const CBioseq_set_Handle&, const CSeq_entry_Handle&, int, ECallMode)");
    */
}
void CSimpleEditSaver::Remove(const CBioseq_set_Handle& handle, 
                              const CSeq_entry_Handle&, 
                              IEditSaver::ECallMode mode)
{
    UpdateTSE(handle.GetTopLevelEntry(), mode);
    /*
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "Remove(const CBioseq_set_Handle&, const CSeq_entry_Handle&, ECallMode)");
    */
}
/*
void CSimpleEditSaver::RemoveTSE(const CTSE_Handle& handle, 
                                      IEditSaver::ECallMode mode)
{
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "RemoveTSE(const CTSE_Handle&, ECallMode)");
}
*/
    //-----------------------------------------------------------------

void CSimpleEditSaver::Replace(const CSeq_feat_Handle& handle,
                               const CSeq_feat&, 
                               IEditSaver::ECallMode mode)
{                                               
    UpdateEntry(handle.GetAnnot().GetParentEntry(), mode);
}
void CSimpleEditSaver::Replace(const CSeq_align_Handle& handle,
                               const CSeq_align&, 
                               IEditSaver::ECallMode mode)
{
    UpdateEntry(handle.GetAnnot().GetParentEntry(), mode);
}
void CSimpleEditSaver::Replace(const CSeq_graph_Handle& handle,
                               const CSeq_graph&, 
                               IEditSaver::ECallMode mode)
{
    UpdateEntry(handle.GetAnnot().GetParentEntry(), mode);
}

void CSimpleEditSaver::Add(const CSeq_annot_Handle& handle,
                           const CSeq_feat&, 
                           IEditSaver::ECallMode mode)
{
    UpdateEntry(handle.GetParentEntry(), mode);
}
void CSimpleEditSaver::Add(const CSeq_annot_Handle& handle,
                           const CSeq_align&, 
                           IEditSaver::ECallMode mode)
{
    UpdateEntry(handle.GetParentEntry(), mode);
}
void CSimpleEditSaver::Add(const CSeq_annot_Handle& handle,
                           const CSeq_graph&, 
                           IEditSaver::ECallMode mode)
{
    UpdateEntry(handle.GetParentEntry(), mode);
}

void CSimpleEditSaver::Remove(const CSeq_feat_Handle& handle, 
                              IEditSaver::ECallMode mode)
{
    UpdateEntry(handle.GetAnnot().GetParentEntry(), mode);
}
void CSimpleEditSaver::Remove(const CSeq_align_Handle& handle, 
                              IEditSaver::ECallMode mode)
{
    UpdateEntry(handle.GetAnnot().GetParentEntry(), mode);
}
void CSimpleEditSaver::Remove(const CSeq_graph_Handle& handle, 
                              IEditSaver::ECallMode mode)
{
    UpdateEntry(handle.GetAnnot().GetParentEntry(), mode);
}


////////////////////////////////////////////////////////////
void CSimpleEditSaver::UpdateSet(const CBioseq_set_Handle& handle, ECallMode mode)
{
    UpdateTSE(handle.GetTopLevelEntry(), mode);
    /*
    NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "UpdateSet(const CBioseq_set_Handle&, ECallMode)");
    */
}

void CSimpleEditSaver::UpdateEntry(const CSeq_entry_Handle& handle,
                                   IEditSaver::ECallMode mode)
{
    if (handle.IsSeq())
        UpdateSeq(handle.GetSeq(), mode);
    else if (handle.IsSet())
        UpdateSet(handle.GetSet(), mode);
    else {
        _ASSERT(0);
        NCBI_THROW(CUnsupportedEditSaverException,
               eUnsupported,
               "UpdateEntry(const CSeq_entry_Handle&, ECallMode)"
               " : entry is not set.");
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/11/15 19:22:08  didenko
 * Added transactions and edit commands support
 *
 * ===========================================================================
 */
