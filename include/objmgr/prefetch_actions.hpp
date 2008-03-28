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


class NCBI_XOBJMGR_EXPORT CScopeSource
{
public:
    CScopeSource(void)
        {
        }
    CScopeSource(CScope& scope)
        : m_Scope(scope)
        {
        }

    static CScopeSource New(CScope& scope)
        {
            CScopeSource ret;
            ret.m_BaseScope.Set(&scope);
            return ret;
        }

    CScope& GetScope(void);

private:
    CHeapScope m_Scope;
    CHeapScope m_BaseScope;
};


class NCBI_XOBJMGR_EXPORT CPrefetchBioseq
    : public CObject, public IPrefetchAction, public CScopeSource
{
public:
    typedef CBioseq_Handle TResult;

    CPrefetchBioseq(const CScopeSource& scope,
                    const CSeq_id_Handle& id);

    virtual bool Execute(CRef<CPrefetchRequest> token);

    const CSeq_id_Handle& GetSeq_id(void) const
        {
            return m_Seq_id;
        }
    const CBioseq_Handle& GetBioseqHandle(void) const
        {
            return m_Result;
        }
    const CBioseq_Handle& GetResult(void) const
        {
            return m_Result;
        }

protected:
    CPrefetchBioseq(const CScopeSource& scope);
    CPrefetchBioseq(const CBioseq_Handle& bioseq);

private:
    CSeq_id_Handle  m_Seq_id;
    TResult         m_Result;
};


class NCBI_XOBJMGR_EXPORT CPrefetchFeat_CI
    : public CPrefetchBioseq
{
public:
    typedef CFeat_CI TResult;

    // from location
    CPrefetchFeat_CI(const CScopeSource& scope,
                     CConstRef<CSeq_loc> loc,
                     const SAnnotSelector& selector);

    // from bioseq
    CPrefetchFeat_CI(const CBioseq_Handle& bioseq,
                     const CRange<TSeqPos>& range,
                     ENa_strand strand,
                     const SAnnotSelector& selector);
    CPrefetchFeat_CI(const CScopeSource& scope,
                     const CSeq_id_Handle& seq_id,
                     const CRange<TSeqPos>& range,
                     ENa_strand strand,
                     const SAnnotSelector& selector);

    virtual bool Execute(CRef<CPrefetchRequest> token);

    const SAnnotSelector& GetSelector(void) const
        {
            return m_Selector;
        }
    const CFeat_CI& GetFeat_CI(void) const
        {
            return m_Result;
        }
    const CFeat_CI& GetResult(void) const
        {
            return m_Result;
        }

private:
    // from location
    CConstRef<CSeq_loc> m_Loc;
    // from bioseq
    CRange<TSeqPos>     m_Range;
    ENa_strand          m_Strand;
    // filter
    SAnnotSelector      m_Selector;
    // result
    TResult             m_Result;
};


template<class Handle>
class CPrefetchComplete
    : public IPrefetchAction
{
public:
    typedef Handle THandle;
    typedef typename THandle::TObject TObject;
    typedef CConstRef<TObject> TResult;

    CPrefetchComplete(const THandle& handle)
        : m_Handle(handle)
        {
        }

    virtual bool Execute(CRef<CPrefetchRequest> token)
        {
            m_Result = m_Handle.GetCompleteObject();
            return m_Result;
        }

    const THandle GetHandle(void) const
        {
            return m_Handle;
        }
    const CConstRef<TObject>& GetResult(void) const
        {
            return m_Result;
        }

private:
    THandle m_Handle;
    TResult m_Result;
};


template<>
class NCBI_XOBJMGR_EXPORT CPrefetchComplete<CBioseq_Handle>
    : public CPrefetchBioseq
{
public:
    typedef CBioseq_Handle THandle;
    typedef THandle::TObject TObject;
    typedef CConstRef<TObject> TResult;

    CPrefetchComplete(const THandle& handle);
    CPrefetchComplete(const CScopeSource& scope,
                      const CSeq_id_Handle& seq_id);

    virtual bool Execute(CRef<CPrefetchRequest> token);

    const THandle GetHandle(void) const
        {
            return GetBioseqHandle();
        }
    const CConstRef<TObject>& GetComplete(void) const
        {
            return m_Result;
        }
    const CConstRef<TObject>& GetResult(void) const
        {
            return m_Result;
        }

private:
    TResult m_Result;
};


class NCBI_XOBJMGR_EXPORT ISeq_idSource
{
public:
    virtual ~ISeq_idSource(void);
    virtual CSeq_id_Handle GetNextSeq_id(void) = 0;
};


template<class Container>
class CStdSeq_idSource : public CObject,
                         public ISeq_idSource
{
public:
    typedef Container TContainer;
    typedef typename TContainer::const_iterator TIterator;

    CStdSeq_idSource(const TContainer& cont)
        : m_Container(cont), m_Iterator(m_Container.begin())
        {
        }

    virtual CSeq_id_Handle GetNextSeq_id(void)
        {
            CSeq_id_Handle ret;
            if ( m_Iterator != m_Container.end() ) {
                ret = *m_Iterator++;
            }
            return ret;
        }

private:
    TContainer m_Container;
    TIterator m_Iterator;
};


class NCBI_XOBJMGR_EXPORT CPrefetchBioseqActionSource
    : public CObject,
      public IPrefetchActionSource
{
public:
    typedef vector<CSeq_id_Handle> TIds;

    CPrefetchBioseqActionSource(const CScopeSource& scope,
                                ISeq_idSource* ids);
    CPrefetchBioseqActionSource(const CScopeSource& scope,
                                const TIds& ids);
    
    virtual CIRef<IPrefetchAction> GetNextAction(void);

private:
    CScopeSource         m_Scope;
    CIRef<ISeq_idSource> m_Ids;
};


class NCBI_XOBJMGR_EXPORT CPrefetchFeat_CIActionSource
    : public CObject,
      public IPrefetchActionSource
{
public:
    typedef vector<CSeq_id_Handle> TIds;

    CPrefetchFeat_CIActionSource(const CScopeSource& scope,
                                 ISeq_idSource* ids,
                                 const SAnnotSelector& sel);
    CPrefetchFeat_CIActionSource(const CScopeSource& scope,
                                 const TIds& ids,
                                 const SAnnotSelector& sel);
    
    virtual CIRef<IPrefetchAction> GetNextAction(void);

private:
    CScopeSource         m_Scope;
    CIRef<ISeq_idSource> m_Ids;
    SAnnotSelector       m_Selector;
};


class NCBI_XOBJMGR_EXPORT CStdPrefetch
{
public:
    static void Wait(CRef<CPrefetchRequest> token);

    // GetBioseqHandle
    static CRef<CPrefetchRequest> GetBioseqHandle(CPrefetchManager& manager,
                                                  const CScopeSource& scope,
                                                  const CSeq_id_Handle& id);
    static CBioseq_Handle GetBioseqHandle(CRef<CPrefetchRequest> token);

    // GetFeat_CI
    static CRef<CPrefetchRequest> GetFeat_CI(CPrefetchManager& manager,
                                             const CBioseq_Handle& bioseq,
                                             const CRange<TSeqPos>& range,
                                             ENa_strand strand,
                                             const SAnnotSelector& sel);
    static CRef<CPrefetchRequest> GetFeat_CI(CPrefetchManager& manager,
                                             const CScopeSource& scope,
                                             const CSeq_id_Handle& seq_id,
                                             const CRange<TSeqPos>& range,
                                             ENa_strand strand,
                                             const SAnnotSelector& sel);
    static CRef<CPrefetchRequest> GetFeat_CI(CPrefetchManager& manager,
                                             const CScopeSource& scope,
                                             CConstRef<CSeq_loc> loc,
                                             const SAnnotSelector& sel);
    static CFeat_CI GetFeat_CI(CRef<CPrefetchRequest> token);
};


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // PREFETCH_MANAGER__HPP
