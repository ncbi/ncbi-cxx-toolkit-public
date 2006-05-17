#ifndef PREFETCH_ACTIONS__HPP
#define PREFETCH_ACTIONS__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   Prefetch manager
*
*/

#include <objmgr/prefetch_manager.hpp>
#include <objmgr/impl/heap_scope.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/feat_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;

/** @addtogroup ObjectManagerCore
 *
 * @{
 */


class NCBI_XOBJMGR_EXPORT CPrefetchAction_GetBioseqHandle
    : public CObject, public IPrefetchAction
{
public:
    typedef CBioseq_Handle TResult;

    CPrefetchAction_GetBioseqHandle(CScope& scope, const CSeq_id_Handle& id);

    virtual bool Execute(CPrefetchToken token);

    const CBioseq_Handle& GetResult(void) const
        {
            return m_Result;
        }

private:
    CHeapScope      m_Scope;
    CSeq_id_Handle  m_Seq_id;
    TResult         m_Result;
};


class NCBI_XOBJMGR_EXPORT CPrefetchAction_Feat_CI
    : public CObject, public IPrefetchAction
{
public:
    typedef CFeat_CI TResult;

    CPrefetchAction_Feat_CI(CScope& scope,
                            CConstRef<CSeq_loc> loc,
                            const SAnnotSelector& selector);
    CPrefetchAction_Feat_CI(const CBioseq_Handle& bioseq,
                            const CRange<TSeqPos>& range,
                            ENa_strand strand,
                            const SAnnotSelector& selector);
    CPrefetchAction_Feat_CI(CScope& scope,
                            const CSeq_id_Handle& seq_id,
                            const CRange<TSeqPos>& range,
                            ENa_strand strand,
                            const SAnnotSelector& selector);

    virtual bool Execute(CPrefetchToken token);

    const CFeat_CI& GetResult(void) const
        {
            return m_Result;
        }

private:
    CHeapScope          m_Scope;
    // from location
    CConstRef<CSeq_loc> m_Loc;
    // from bioseq
    CBioseq_Handle      m_Bioseq;
    // from seq-id
    CSeq_id_Handle      m_Seq_id;
    CRange<TSeqPos>     m_Range;
    ENa_strand          m_Strand;
    // filter
    SAnnotSelector      m_Selector;
    // result
    TResult             m_Result;
};


template<class Handle>
class CPrefetchAction_GetComplete
    : public CObject, public IPrefetchAction
{
public:
    typedef Handle THandle;
    typedef typename THandle::TObject TObject;
    typedef CConstRef<TObject> TResult;

    CPrefetchAction_GetComplete(const Handle& handle)
        : m_Handle(handle)
        {
        }

    virtual bool Execute(CPrefetchToken token)
        {
            m_Result = m_Handle.GetCompleteObject();
            return m_Result;
        }

    const CConstRef<TObject>& GetResult(void) const
        {
            return m_Result;
        }

private:
    THandle m_Handle;
    TResult m_Result;
};


class CWaitingListener
    : public CObject, public IPrefetchListener
{
public:
    CWaitingListener(void)
        : m_Sema(0, kMax_Int)
        {
        }

    virtual void PrefetchNotify(CPrefetchToken token, EEvent /*event*/)
        {
            if ( token.IsDone() ) {
                m_Sema.Post();
            }
        }

    void Wait(void)
        {
            m_Sema.Wait();
            m_Sema.Post();
        }
    
private:
    CSemaphore m_Sema;
};


class NCBI_XOBJMGR_EXPORT CStdPrefetch : public CPrefetchManager
{
public:
    static void Wait(CPrefetchToken token);

    // GetBioseqHandle
    static CPrefetchToken GetBioseqHandle(CPrefetchManager& manager,
                                          CScope& scope,
                                          const CSeq_id_Handle& id);
    CPrefetchToken GetBioseqHandle(CScope& scope,
                                   const CSeq_id_Handle& id)
        {
            return GetBioseqHandle(*this, scope, id);
        }
    static CBioseq_Handle GetBioseqHandle(CPrefetchToken token);

    // GetFeat_CI
    static CPrefetchToken GetFeat_CI(CPrefetchManager& manager,
                                     CScope& scope,
                                     CConstRef<CSeq_loc> loc,
                                     const SAnnotSelector& sel);
    CPrefetchToken GetFeat_CI(CScope& scope,
                              CConstRef<CSeq_loc> loc,
                              const SAnnotSelector& sel)
        {
            return GetFeat_CI(*this, scope, loc, sel);
        }
    static CPrefetchToken GetFeat_CI(CPrefetchManager& manager,
                                     CScope& scope,
                                     const CSeq_id_Handle& seq_id,
                                     const CRange<TSeqPos>& range,
                                     ENa_strand strand,
                                     const SAnnotSelector& sel);
    CPrefetchToken GetFeat_CI(CScope& scope,
                              const CSeq_id_Handle& seq_id,
                              const CRange<TSeqPos>& range,
                              ENa_strand strand,
                              const SAnnotSelector& sel)
        {
            return GetFeat_CI(*this, scope, seq_id, range, strand, sel);
        }
    static CPrefetchToken GetFeat_CI(CPrefetchManager& manager,
                                     const CBioseq_Handle& bioseq,
                                     const CRange<TSeqPos>& range,
                                     ENa_strand strand,
                                     const SAnnotSelector& sel);
    CPrefetchToken GetFeat_CI(CScope& scope,
                              const CBioseq_Handle& bioseq,
                              const CRange<TSeqPos>& range,
                              ENa_strand strand,
                              const SAnnotSelector& sel)
        {
            return GetFeat_CI(*this, bioseq, range, strand, sel);
        }
    static CFeat_CI GetFeat_CI(CPrefetchToken token);

};


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // PREFETCH_MANAGER__HPP
