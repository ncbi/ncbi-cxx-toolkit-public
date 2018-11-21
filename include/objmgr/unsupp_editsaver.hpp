#ifndef __UNSUPPORTED_EDIT_SAVER__HPP
#define __UNSUPPORTED_EDIT_SAVER__HPP

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
* Author: Maxim Didenko
*
* File Description:
*
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbiexpt.hpp>

#include <objmgr/edit_saver.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CUnsupportedEditSaverException : public CException
{
public:
    enum EErrCode {
        eUnsupported
    };
    virtual const char* GetErrCodeString(void) const override {
        switch ( GetErrCode() ) {
        case eUnsupported:
            return "Unsupported operation";
        default:
            return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CUnsupportedEditSaverException, CException);
};

class NCBI_XOBJMGR_EXPORT CUnsupportedEditSaver : public IEditSaver
{
public:

    virtual void AddDescr(const CBioseq_Handle&, const CSeq_descr&, ECallMode);
    virtual void AddDescr(const CBioseq_set_Handle&, const CSeq_descr&, ECallMode);

    virtual void SetDescr(const CBioseq_Handle&, const CSeq_descr&, ECallMode);
    virtual void SetDescr(const CBioseq_set_Handle&, const CSeq_descr&, ECallMode);

    virtual void ResetDescr(const CBioseq_Handle&, ECallMode);
    virtual void ResetDescr(const CBioseq_set_Handle&, ECallMode);

    virtual void AddDesc(const CBioseq_Handle&, const CSeqdesc&, ECallMode);
    virtual void AddDesc(const CBioseq_set_Handle&, const CSeqdesc&, ECallMode);

    virtual void RemoveDesc(const CBioseq_Handle&, const CSeqdesc&, ECallMode);
    virtual void RemoveDesc(const CBioseq_set_Handle&, const CSeqdesc&, ECallMode);

    //------------------------------------------------------------------
    virtual void SetSeqInst(const CBioseq_Handle&, const CSeq_inst&, ECallMode);
    virtual void SetSeqInstRepr(const CBioseq_Handle&, CSeq_inst::TRepr, ECallMode);
    virtual void SetSeqInstMol(const CBioseq_Handle&, CSeq_inst::TMol, ECallMode);
    virtual void SetSeqInstLength(const CBioseq_Handle&, 
                                  CSeq_inst::TLength,
                                  ECallMode);
    virtual void SetSeqInstFuzz(const CBioseq_Handle& info, 
                                const CSeq_inst::TFuzz& fuzz, ECallMode);
    virtual void SetSeqInstTopology(const CBioseq_Handle& info, 
                                    CSeq_inst::TTopology topology, ECallMode);
    virtual void SetSeqInstStrand(const CBioseq_Handle& info, 
                                  CSeq_inst::TStrand strand, ECallMode);
    virtual void SetSeqInstExt(const CBioseq_Handle& info, 
                               const CSeq_inst::TExt& ext, ECallMode);
    virtual void SetSeqInstHist(const CBioseq_Handle& info, 
                                const CSeq_inst::THist& hist, ECallMode);
    virtual void SetSeqInstSeq_data(const CBioseq_Handle& info, 
                                    const CSeq_inst::TSeq_data& data, ECallMode);
    
    virtual void ResetSeqInst(const CBioseq_Handle&, ECallMode);
    virtual void ResetSeqInstRepr(const CBioseq_Handle&, ECallMode);
    virtual void ResetSeqInstMol(const CBioseq_Handle&, ECallMode);
    virtual void ResetSeqInstLength(const CBioseq_Handle&, ECallMode);
    virtual void ResetSeqInstFuzz(const CBioseq_Handle&, ECallMode);
    virtual void ResetSeqInstTopology(const CBioseq_Handle&, ECallMode);
    virtual void ResetSeqInstStrand(const CBioseq_Handle&, ECallMode);
    virtual void ResetSeqInstExt(const CBioseq_Handle&, ECallMode);
    virtual void ResetSeqInstHist(const CBioseq_Handle&, ECallMode);
    virtual void ResetSeqInstSeq_data(const CBioseq_Handle&, ECallMode);

    //----------------------------------------------------------------
    virtual void AddId(const CBioseq_Handle&, const CSeq_id_Handle&, ECallMode);
    virtual void RemoveId(const CBioseq_Handle&, const CSeq_id_Handle&, ECallMode);
    virtual void ResetIds(const CBioseq_Handle&, const TIds&, ECallMode);

    virtual void SetBioseqSetId(const CBioseq_set_Handle&,
                                const CBioseq_set::TId&, ECallMode);
    virtual void SetBioseqSetColl(const CBioseq_set_Handle&,
                                  const CBioseq_set::TColl&, ECallMode);
    virtual void SetBioseqSetLevel(const CBioseq_set_Handle&,
                                   CBioseq_set::TLevel, ECallMode);
    virtual void SetBioseqSetClass(const CBioseq_set_Handle&,
                                   CBioseq_set::TClass, ECallMode);
    virtual void SetBioseqSetRelease(const CBioseq_set_Handle&,
                                     const CBioseq_set::TRelease&, ECallMode);
    virtual void SetBioseqSetDate(const CBioseq_set_Handle&,
                                  const CBioseq_set::TDate&, ECallMode);
 
    virtual void ResetBioseqSetId(const CBioseq_set_Handle&, ECallMode);
    virtual void ResetBioseqSetColl(const CBioseq_set_Handle&, ECallMode);
    virtual void ResetBioseqSetLevel(const CBioseq_set_Handle&, ECallMode);
    virtual void ResetBioseqSetClass(const CBioseq_set_Handle&, ECallMode);
    virtual void ResetBioseqSetRelease(const CBioseq_set_Handle&, ECallMode);
    virtual void ResetBioseqSetDate(const CBioseq_set_Handle&, ECallMode);
  
    //-----------------------------------------------------------------
    virtual void Attach(const CBioObjectId& old_id,
                        const CSeq_entry_Handle& entry, 
                        const CBioseq_Handle& what, ECallMode );
    virtual void Attach(const CBioObjectId& old_id,
                        const CSeq_entry_Handle& entry, 
                        const CBioseq_set_Handle& what, ECallMode );
    virtual void Detach(const CSeq_entry_Handle& entry, 
                        const CBioseq_Handle& what, ECallMode );
    virtual void Detach(const CSeq_entry_Handle& entry, 
                        const CBioseq_set_Handle& what, ECallMode );

    virtual void Attach(const CSeq_entry_Handle& entry, 
                        const CSeq_annot_Handle& what, ECallMode);
    virtual void Remove(const CSeq_entry_Handle& entry, 
                        const CSeq_annot_Handle& what, ECallMode);

    virtual void Attach(const CBioseq_set_Handle& handle, 
                        const CSeq_entry_Handle& entry, 
                        int Index, ECallMode);
    virtual void Remove(const CBioseq_set_Handle& handle, 
                        const CSeq_entry_Handle&, 
                        int Index, ECallMode);

    //-----------------------------------------------------------------
    virtual void Replace(const CSeq_feat_Handle& handle,
                         const CSeq_feat& old_value, ECallMode);
    virtual void Replace(const CSeq_align_Handle& handle,
                         const CSeq_align& old_value, ECallMode);
    virtual void Replace(const CSeq_graph_Handle& handle,
                         const CSeq_graph& old_value, ECallMode);

    virtual void Add(const CSeq_annot_Handle& handle,
                     const CSeq_feat& obj, ECallMode);
    virtual void Add(const CSeq_annot_Handle& handle,
                     const CSeq_align& obj, ECallMode);
    virtual void Add(const CSeq_annot_Handle& handle,
                     const CSeq_graph& obj, ECallMode);

    virtual void Remove(const CSeq_annot_Handle& handle, 
                        const CSeq_feat& old_value, ECallMode);
    virtual void Remove(const CSeq_annot_Handle& handle, 
                        const CSeq_align& old_value, ECallMode);
    virtual void Remove(const CSeq_annot_Handle& handle, 
                        const CSeq_graph& old_value, ECallMode);

    //-----------------------------------------------------------------
    virtual void RemoveTSE(const CTSE_Handle& handle, ECallMode);
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // __UNSUPPORTED_EDIT_SAVER__HPP
