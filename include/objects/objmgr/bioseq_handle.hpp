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
* Revision 1.3  2002/01/23 21:59:29  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:26:36  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:04:00  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/objmgr1/seq_id_handle.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataSource;
class CSeqMap;

// Bioseq handle -- must be a copy-safe const type.
class CBioseq_Handle
{
public:
    // Destructor
    virtual ~CBioseq_Handle(void);

    // Bioseq core -- using partially populated CBioseq
    typedef CConstRef<CBioseq> TBioseqCore;


    CBioseq_Handle(void)
        : m_DataSource(0),
          m_Entry(0) {}
    CBioseq_Handle(const CBioseq_Handle& h);
    CBioseq_Handle& operator= (const CBioseq_Handle& h);

    // Comparison
    bool operator== (const CBioseq_Handle& h) const;
    bool operator!= (const CBioseq_Handle& h) const;
    bool operator<  (const CBioseq_Handle& h) const;

    // Check
    operator bool(void)  const { return ( m_DataSource  &&  m_Value); }
    bool operator!(void) const { return (!m_DataSource  || !m_Value); }

    const CSeq_id* GetSeqId(void)  const;

    // Get the complete bioseq (as loaded by now)
    // Declared "virtual" to avoid circular dependencies with seqloc
    virtual const CBioseq& GetBioseq(void) const;

    // Get top level seq-entry for a bioseq
    virtual const CSeq_entry& GetTopLevelSeqEntry(void) const;

    // Get bioseq core structure
    // Declared "virtual" to avoid circular dependencies with seqloc
    virtual TBioseqCore GetBioseqCore(void) const;

    // Get sequence map. References to the whole bioseqs may have
    // length of 0 unless GetSequence() has been called for the handle.
    virtual const CSeqMap& GetSeqMap(void) const;

private:
    CBioseq_Handle(CSeq_id_Handle value);

    // Get the internal seq-id handle
    const CSeq_id_Handle  GetHandle(void) const;
    // Get data source
    CDataSource& x_GetDataSource(void) const;
    // Set the handle seq-entry and datasource
    void x_ResolveTo(CDataSource& datasource, CSeq_entry& entry) const;

    CSeq_id_Handle       m_Value;       // Seq-id equivalent
    mutable CDataSource* m_DataSource;  // Data source for resolved handles
    mutable CSeq_entry*  m_Entry;       // Seq-entry, containing the bioseq

    friend class CDesc_CI;
    friend class CScope;
    friend class CSeqVector;
    friend class CDataSource;
    friend class CHandleRangeMap;
};


inline
void CBioseq_Handle::x_ResolveTo(CDataSource& datasource, CSeq_entry& entry) const
{
    m_DataSource = &datasource;
    m_Entry = &entry;
}

inline
CBioseq_Handle::CBioseq_Handle(CSeq_id_Handle value)
    : m_Value(value),
      m_DataSource(0),
      m_Entry(0)
{
}

inline
const CSeq_id_Handle CBioseq_Handle::GetHandle(void) const
{
    return m_Value;
}

inline
CBioseq_Handle::CBioseq_Handle(const CBioseq_Handle& h)
    : m_Value(h.m_Value),
      m_DataSource(h.m_DataSource),
      m_Entry(h.m_Entry)
{
}

inline
CBioseq_Handle& CBioseq_Handle::operator= (const CBioseq_Handle& h)
{
    m_Value = h.m_Value;
    m_DataSource = h.m_DataSource;
    m_Entry = h.m_Entry;
    return *this;
}

inline
bool CBioseq_Handle::operator== (const CBioseq_Handle& h) const
{
    if ( m_Entry  &&  h.m_Entry )
        return m_DataSource == h.m_DataSource  &&  m_Entry == h.m_Entry;
    // Compare by id key
    return m_Value == h.m_Value;
}

inline
bool CBioseq_Handle::operator!= (const CBioseq_Handle& h) const
{
    return !(*this == h);
}

inline
bool CBioseq_Handle::operator< (const CBioseq_Handle& h) const
{
    if ( m_Entry != h.m_Entry )
        return m_Entry < h.m_Entry;
    return m_Value < h.m_Value;
}

inline
CDataSource& CBioseq_Handle::x_GetDataSource(void) const
{
    if ( !m_DataSource )
        throw runtime_error("Can not resolve data source for bioseq handle.");
    return *m_DataSource;
}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // BIOSEQ_HANDLE__HPP
