#ifndef SEQ_ANNOT_HANDLE__HPP
#define SEQ_ANNOT_HANDLE__HPP

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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*    Handle to Seq-annot object
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;
class CSeq_annot;
class CSeq_annot_Info;
class CSeq_annot_CI;
class CAnnotTypes_CI;
class CAnnot_CI;

/////////////////////////////////////////////////////////////////////////////
// CSeq_annot_Handle
/////////////////////////////////////////////////////////////////////////////

class NCBI_XOBJMGR_EXPORT CSeq_annot_Handle
{
public:
    CSeq_annot_Handle(void);
    CSeq_annot_Handle(CScope& scope, const CSeq_annot_Info& annot);
    CSeq_annot_Handle(const CSeq_annot_Handle& sah);
    ~CSeq_annot_Handle(void);

    CSeq_annot_Handle& operator=(const CSeq_annot_Handle& sah);

    CScope& GetScope(void) const;

    const CSeq_annot& GetSeq_annot(void) const;
    operator bool(void) const;
    bool operator!(void) const;

    bool operator==(const CSeq_annot_Handle& annot) const;
    bool operator!=(const CSeq_annot_Handle& annot) const;
    bool operator<(const CSeq_annot_Handle& annot) const;

private:
    void x_Set(CScope& scope, const CSeq_annot_Info& annot);
    void x_Reset(void);

    mutable CRef<CScope>       m_Scope;
    CConstRef<CSeq_annot_Info> m_Seq_annot;

    friend class CSeq_annot_CI;
    friend class CAnnotTypes_CI;
    friend class CAnnot_CI;
};


/////////////////////////////////////////////////////////////////////////////
// CSeq_annot_Handle inline methods
/////////////////////////////////////////////////////////////////////////////


inline
CSeq_annot_Handle::operator bool(void) const
{
    return m_Seq_annot;
}


inline
bool CSeq_annot_Handle::operator!(void) const
{
    return !m_Seq_annot;
}


inline
bool CSeq_annot_Handle::operator==(const CSeq_annot_Handle& annot) const
{
    return m_Seq_annot == annot.m_Seq_annot;
}


inline
bool CSeq_annot_Handle::operator!=(const CSeq_annot_Handle& annot) const
{
    return m_Seq_annot != annot.m_Seq_annot;
}


inline
bool CSeq_annot_Handle::operator<(const CSeq_annot_Handle& annot) const
{
    return m_Seq_annot < annot.m_Seq_annot;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/09/30 16:21:59  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* ===========================================================================
*/

#endif//SEQ_ANNOT_HANDLE__HPP
