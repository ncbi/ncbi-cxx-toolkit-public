#ifndef BIOSEQ_HANDLE__HPP
#define BIOSEQ_HANDLE__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman
*
* File Description:
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2002/01/11 19:04:00  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataSource;
class CSeq_id;
class CSeq_entry;

// Bioseq handle -- must be a copy-safe const type.
class CBioseqHandle
{
public:
    typedef int THandle;
    CBioseqHandle(void)
        : m_Value(0),
          m_DataSource(0),
          m_Entry(0) {}

    CBioseqHandle(const CBioseqHandle& h);
    CBioseqHandle& operator= (const CBioseqHandle& h);
    ~CBioseqHandle(void) {}

    // Comparison
    bool operator== (const CBioseqHandle& h) const;
    bool operator!= (const CBioseqHandle& h) const;
    bool operator<  (const CBioseqHandle& h) const;

    // Check
    operator bool(void)  const { return ( m_DataSource  &&  m_Value); }
    bool operator!(void) const { return (!m_DataSource  || !m_Value); }
  
    const CSeq_id* GetSeqId(void)  const;
    const THandle  GetHandle(void) const;
private:
    CBioseqHandle(THandle value);

    // Get data source
    CDataSource& x_GetDataSource(void) const;
    // Set the handle seq-entry and datasource
    void x_ResolveTo(CDataSource& datasource, CSeq_entry& entry) const;

    THandle              m_Value;       // Seq-id equivalent
    mutable CDataSource* m_DataSource;  // Data source for resolved handles
    mutable CSeq_entry*  m_Entry;       // Seq-entry, containing the bioseq

    friend class CDesc_CI;
    friend class CScope;
    friend class CSeqVector;
    friend class CDataSource;
    friend class CHandleRangeMap;
};


inline
void CBioseqHandle::x_ResolveTo(CDataSource& datasource, CSeq_entry& entry) const
{
    m_DataSource = &datasource;
    m_Entry = &entry;
}

inline
CBioseqHandle::CBioseqHandle(THandle value)
    : m_Value(value),
      m_DataSource(0),
      m_Entry(0)
{
}

inline
const CBioseqHandle::THandle CBioseqHandle::GetHandle(void) const
{
    return m_Value;
}

inline
CBioseqHandle::CBioseqHandle(const CBioseqHandle& h)
    : m_Value(h.m_Value),
      m_DataSource(h.m_DataSource),
      m_Entry(h.m_Entry)
{
}

inline
CBioseqHandle& CBioseqHandle::operator= (const CBioseqHandle& h)
{
    m_Value = h.m_Value;
    m_DataSource = h.m_DataSource;
    m_Entry = h.m_Entry;
    return *this;
}

inline
bool CBioseqHandle::operator== (const CBioseqHandle& h) const
{
    if ( m_Entry  &&  h.m_Entry )
        return m_DataSource == h.m_DataSource  &&  m_Entry == h.m_Entry;
    // Compare by id key
    return m_Value == h.m_Value;
}

inline
bool CBioseqHandle::operator!= (const CBioseqHandle& h) const
{
    return !(*this == h);
}

inline
bool CBioseqHandle::operator< (const CBioseqHandle& h) const
{
    if ( m_Entry != h.m_Entry )
        return m_Entry < h.m_Entry;
    return m_Value < h.m_Value;
}

inline
CDataSource& CBioseqHandle::x_GetDataSource(void) const
{
    if ( !m_DataSource )
        throw runtime_error("Can not resolve data source for bioseq handle.");
    return *m_DataSource;
}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // BIOSEQ_HANDLE__HPP
