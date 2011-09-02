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
* File Description:
*
*   Translate HGVS expression to Variation-ref seq-feats.
*   HGVS nomenclature rules: http://www.hgvs.org/mutnomen/
*
* ===========================================================================
*/

#ifndef SEQ_ID_RESOLVER_HPP_
#define SEQ_ID_RESOLVER_HPP_


#include <objects/seq/seq_id_handle.hpp>
#include <objects/genomecoll/GC_Assembly.hpp>
#include <objmgr/scope.hpp>


BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

BEGIN_objects_SCOPE
class CEntrez2Client;
END_objects_SCOPE
class CRegexp;

/// A helper class to convert a string to a seq-id.
/// Default implementation assumes a seq-id string, but user may implement
/// their own, e.g. to interpret "chr1" based on tax/assembly
class CSeq_id_Resolver : public CObject
{
public:
    CSeq_id_Resolver(CScope& scope)
      : m_scope(scope)
      , m_regexp(NULL)
    {}

    virtual bool CanCreate(const string& s);

    virtual CSeq_id_Handle Get(const string& s)
    {
        if(m_cache.find(s) == m_cache.end()) {
            m_cache[s] = x_Create(s);
        }
        return m_cache[s];
    }

    virtual ~CSeq_id_Resolver();

    typedef list<CRef<CSeq_id_Resolver> > TResolvers;
    /// Iterate through resolvers and resolve using the first one that can do it
    /// Use CSeq_id_Handle::GetHandle(s) if none matched
    static CSeq_id_Handle s_Get(TResolvers& resolvers, const string& s)
    {
        NON_CONST_ITERATE(TResolvers, it, resolvers) {
            CSeq_id_Resolver& r = **it;
            if(r.CanCreate(s)) {
                return r.Get(s);
            }
        }
        return CSeq_id_Handle::GetHandle(s);
    }

protected:
    virtual CSeq_id_Handle x_Create(const string& s);
    CScope& m_scope;
    typedef map<string, CSeq_id_Handle> TCache;
    TCache m_cache;
    CRegexp* m_regexp;
};

/// Resolve LRG seq-ids, e.g. LRG_123, LRG_123t1, LRG_123p1
class CSeq_id_Resolver__LRG : public CSeq_id_Resolver
{
public:
    CSeq_id_Resolver__LRG(CScope& scope);
    virtual ~CSeq_id_Resolver__LRG() {}
private:
    virtual CSeq_id_Handle x_Create(const string& s);
};

/// Resolve CCDS-id to an NM
class CSeq_id_Resolver__CCDS : public CSeq_id_Resolver
{
public:
    CSeq_id_Resolver__CCDS(CScope& scope);
    virtual ~CSeq_id_Resolver__CCDS();

private:
    virtual CSeq_id_Handle x_Create(const string& s);
    objects::CEntrez2Client* m_entrez;
};

/// Resolve chromosome names based on GC_Assembly
class CSeq_id_Resolver__ChrNamesFromGC : public CSeq_id_Resolver
{
public:
    CSeq_id_Resolver__ChrNamesFromGC(const CGC_Assembly& assembly, CScope& scope);
    virtual ~CSeq_id_Resolver__ChrNamesFromGC() {}

private:
    virtual CSeq_id_Handle x_Create(const string& s);
    typedef map<string, CSeq_id_Handle> TData;
    TData m_data;
};

END_NCBI_SCOPE;

#endif
