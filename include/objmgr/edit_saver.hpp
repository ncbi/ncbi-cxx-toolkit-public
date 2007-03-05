#ifndef OBJECTS_OBJMGR_IMPL___EDIT_SAVER__HPP
#define OBJECTS_OBJMGR_IMPL___EDIT_SAVER__HPP

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
#include <corelib/plugin_manager.hpp>

#include <objects/seq/Seq_inst.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/seq_id_handle.hpp>

#include <objmgr/bio_object_id.hpp>
BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CBioseq_Handle;
class CBioseq_set_Handle;
class CSeq_entry_Handle;
class CSeq_annot_Handle;
class CSeq_feat_Handle;
class CSeq_align_Handle;
class CSeq_graph_Handle;
class CTSE_Handle;

class CSeq_feat;
class CSeq_align;
class CSeq_graph;

class CSeq_descr;
class CSeqdesc;

/// Edit Saver Interface
///
/// An instance of a class which implements this interface 
/// can be attached to a TSE in order to catch modifications 
/// which are being done on the objects associated with this TSE
/// This instance is attached to the TSE using the CDataLoaderPatcher class
///
/// @sa CDataLoaderPatcher, CScopeTransaction
///
class NCBI_XOBJMGR_EXPORT IEditSaver : public CObject
{
public:

    /// This flag can be used for optimization purpose
    enum ECallMode {
        eDo,      ///< The method is called when a modification has just been done
        eUndo     ///< The method is called when a modification has just been undone
    };
        
    virtual ~IEditSaver();

    /// Called when a transaction has just been started
    ///
    virtual void BeginTransaction() = 0;

    /// Called when a transaction is finished
    ///
    virtual void CommitTransaction() = 0;
    
    /// Called when a transaction should be undone
    ///
    virtual void RollbackTransaction() = 0;

    //------------------------------------------------------------------
    // Bioseq operations

    /// Description operations
    virtual void AddDescr  (const CBioseq_Handle&, 
                            const CSeq_descr&, ECallMode) = 0;
    virtual void SetDescr  (const CBioseq_Handle&, 
                            const CSeq_descr&, ECallMode) = 0;
    virtual void ResetDescr(const CBioseq_Handle&, ECallMode) = 0;
    virtual void AddDesc   (const CBioseq_Handle&, const CSeqdesc&, ECallMode) = 0;
    virtual void RemoveDesc(const CBioseq_Handle&, const CSeqdesc&, ECallMode) = 0;

    /// CSeq_inst operatoions
    virtual void SetSeqInst        (const CBioseq_Handle&, 
                                    const CSeq_inst&, ECallMode) = 0;
    virtual void SetSeqInstRepr    (const CBioseq_Handle&, 
                                    CSeq_inst::TRepr, ECallMode) = 0;
    virtual void SetSeqInstMol     (const CBioseq_Handle&, 
                                    CSeq_inst::TMol, ECallMode) = 0;
    virtual void SetSeqInstLength  (const CBioseq_Handle&, 
                                    CSeq_inst::TLength, ECallMode) = 0;
    virtual void SetSeqInstFuzz    (const CBioseq_Handle& handle, 
                                    const CSeq_inst::TFuzz& fuzz, ECallMode) = 0;
    virtual void SetSeqInstTopology(const CBioseq_Handle& handle, 
                                    CSeq_inst::TTopology topology, ECallMode) = 0;
    virtual void SetSeqInstStrand  (const CBioseq_Handle& handle, 
                                    CSeq_inst::TStrand strand, ECallMode) = 0;
    virtual void SetSeqInstExt     (const CBioseq_Handle& handle, 
                                    const CSeq_inst::TExt& ext, ECallMode) = 0;
    virtual void SetSeqInstHist    (const CBioseq_Handle& handle, 
                                    const CSeq_inst::THist& hist, ECallMode) = 0;
    virtual void SetSeqInstSeq_data(const CBioseq_Handle& handle, 
                                    const CSeq_inst::TSeq_data& data, 
                                    ECallMode) = 0;
    
    virtual void ResetSeqInst        (const CBioseq_Handle&, ECallMode) = 0;
    virtual void ResetSeqInstRepr    (const CBioseq_Handle&, ECallMode) = 0;
    virtual void ResetSeqInstMol     (const CBioseq_Handle&, ECallMode) = 0;
    virtual void ResetSeqInstLength  (const CBioseq_Handle&, ECallMode) = 0;
    virtual void ResetSeqInstFuzz    (const CBioseq_Handle&, ECallMode) = 0;
    virtual void ResetSeqInstTopology(const CBioseq_Handle&, ECallMode) = 0;
    virtual void ResetSeqInstStrand  (const CBioseq_Handle&, ECallMode) = 0;
    virtual void ResetSeqInstExt     (const CBioseq_Handle&, ECallMode) = 0;
    virtual void ResetSeqInstHist    (const CBioseq_Handle&, ECallMode) = 0;
    virtual void ResetSeqInstSeq_data(const CBioseq_Handle&, ECallMode) = 0;

    /// ID operation
    virtual void AddId   (const CBioseq_Handle&, 
                          const CSeq_id_Handle&, ECallMode) = 0;
    virtual void RemoveId(const CBioseq_Handle&, 
                          const CSeq_id_Handle&, ECallMode) = 0;
    typedef set<CSeq_id_Handle> TIds;
    virtual void ResetIds(const CBioseq_Handle&, const TIds&, ECallMode) = 0;

    //----------------------------------------------------------------
    // Bioseq_set operations
    virtual void AddDescr  (const CBioseq_set_Handle&, 
                            const CSeq_descr&, ECallMode) = 0;
    virtual void SetDescr  (const CBioseq_set_Handle&, 
                            const CSeq_descr&, ECallMode) = 0;
    virtual void ResetDescr(const CBioseq_set_Handle&, ECallMode) = 0;
    virtual void AddDesc   (const CBioseq_set_Handle&, 
                            const CSeqdesc&, ECallMode) = 0;
    virtual void RemoveDesc(const CBioseq_set_Handle&, 
                            const CSeqdesc&, ECallMode) = 0;

    virtual void SetBioseqSetId     (const CBioseq_set_Handle&,
                                     const CBioseq_set::TId&, ECallMode) = 0;
    virtual void SetBioseqSetColl   (const CBioseq_set_Handle&,
                                     const CBioseq_set::TColl&, ECallMode) = 0;
    virtual void SetBioseqSetLevel  (const CBioseq_set_Handle&,
                                     CBioseq_set::TLevel, ECallMode) = 0;
    virtual void SetBioseqSetClass  (const CBioseq_set_Handle&,
                                     CBioseq_set::TClass, ECallMode) = 0;
    virtual void SetBioseqSetRelease(const CBioseq_set_Handle&,
                                     const CBioseq_set::TRelease&, ECallMode) = 0;
    virtual void SetBioseqSetDate   (const CBioseq_set_Handle&,
                                     const CBioseq_set::TDate&, ECallMode) = 0;
 
    virtual void ResetBioseqSetId     (const CBioseq_set_Handle&, ECallMode) = 0;
    virtual void ResetBioseqSetColl   (const CBioseq_set_Handle&, ECallMode) = 0;
    virtual void ResetBioseqSetLevel  (const CBioseq_set_Handle&, ECallMode) = 0;
    virtual void ResetBioseqSetClass  (const CBioseq_set_Handle&, ECallMode) = 0;
    virtual void ResetBioseqSetRelease(const CBioseq_set_Handle&, ECallMode) = 0;
    virtual void ResetBioseqSetDate   (const CBioseq_set_Handle&, ECallMode) = 0;
  
    //-----------------------------------------------------------------
    // Seq_entry operations
    virtual void Attach(const CBioObjectId& old_id,
                        const CSeq_entry_Handle& entry, 
                        const CBioseq_Handle& what, ECallMode ) = 0;
    virtual void Attach(const CBioObjectId& old_id,
                        const CSeq_entry_Handle& entry, 
                        const CBioseq_set_Handle& what, ECallMode ) = 0;
    virtual void Detach(const CSeq_entry_Handle& entry, 
                        const CBioseq_Handle& what, ECallMode ) = 0;
    virtual void Detach(const CSeq_entry_Handle& entry, 
                        const CBioseq_set_Handle& what, ECallMode ) = 0;

    virtual void Attach(const CSeq_entry_Handle& entry, 
                        const CSeq_annot_Handle& what, ECallMode) = 0;
    virtual void Remove(const CSeq_entry_Handle& entry, 
                        const CSeq_annot_Handle& what, ECallMode) = 0;

    virtual void Attach(const CBioseq_set_Handle& handle, 
                        const CSeq_entry_Handle& entry, 
                        int Index, ECallMode) = 0;
    virtual void Remove(const CBioseq_set_Handle& handle, 
                        const CSeq_entry_Handle&,
                        int Index, ECallMode) = 0;

    //-----------------------------------------------------------------
    // Annotation operations
    virtual void Replace(const CSeq_feat_Handle& handle,
                         const CSeq_feat& old_value, ECallMode) = 0;
    virtual void Replace(const CSeq_align_Handle& handle,
                         const CSeq_align& old_value, ECallMode) = 0;
    virtual void Replace(const CSeq_graph_Handle& handle,
                         const CSeq_graph& old_value, ECallMode) = 0;

    virtual void Add(const CSeq_annot_Handle& handle,
                     const CSeq_feat& obj, ECallMode) = 0;
    virtual void Add(const CSeq_annot_Handle& handle,
                     const CSeq_align& obj, ECallMode) = 0;
    virtual void Add(const CSeq_annot_Handle& handle,
                     const CSeq_graph& obj, ECallMode) = 0;

    virtual void Remove(const CSeq_annot_Handle& handle, 
                        const CSeq_feat& old_value, ECallMode) = 0;
    virtual void Remove(const CSeq_annot_Handle& handle, 
                        const CSeq_align& old_value, ECallMode) = 0;
    virtual void Remove(const CSeq_annot_Handle& handle, 
                        const CSeq_graph& old_value, ECallMode) = 0;

    //-----------------------------------------------------------------
    virtual void RemoveTSE(const CTSE_Handle& handle, ECallMode) = 0;

};

END_SCOPE(objects)

NCBI_DECLARE_INTERFACE_VERSION(objects::IEditSaver,  "xeditsaver", 1, 0, 0);
 
template<>
class CDllResolver_Getter<objects::IEditSaver>
{
public:
    CPluginManager_DllResolver* operator()(void)
    {
        CPluginManager_DllResolver* resolver =
            new CPluginManager_DllResolver
            (CInterfaceVersion<objects::IEditSaver>::GetName(),
             kEmptyStr,
             CVersionInfo::kAny,
             CDll::eAutoUnload);
        resolver->SetDllNamePrefix("ncbi");
        return resolver;
    }
};

END_NCBI_SCOPE

#endif//OBJECTS_OBJMGR_IMPL___EDIT_SAVER__HPP
