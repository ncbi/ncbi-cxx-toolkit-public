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
* Revision 1.13  2002/04/18 20:35:10  gouriano
* correction in comment
*
* Revision 1.12  2002/04/11 12:07:28  grichenk
* Redesigned CAnnotTypes_CI to resolve segmented sequences correctly.
*
* Revision 1.11  2002/03/19 19:17:33  gouriano
* added const qualifier to GetTitle and GetSeqVector
*
* Revision 1.10  2002/03/15 18:10:04  grichenk
* Removed CRef<CSeq_id> from CSeq_id_Handle, added
* key to seq-id map th CSeq_id_Mapper
*
* Revision 1.9  2002/03/04 15:08:43  grichenk
* Improved CTSE_Info locks
*
* Revision 1.8  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.7  2002/02/07 21:27:33  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.6  2002/02/01 21:49:10  gouriano
* minor changes to make it compilable and run on Solaris Workshop
*
* Revision 1.5  2002/01/29 17:05:53  grichenk
* GetHandle() -> GetKey()
*
* Revision 1.4  2002/01/28 19:45:33  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
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

#include <objects/objmgr1/seq_id_handle.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/Bioseq.hpp>
#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataSource;
class CSeqMap;
class CSeqVector;
class CTSE_Info;


// Bioseq handle -- must be a copy-safe const type.
class CBioseq_Handle
{
public:
    // Destructor
    virtual ~CBioseq_Handle(void);

    // Bioseq core -- using partially populated CBioseq
    typedef CConstRef<CBioseq> TBioseqCore;


    CBioseq_Handle(void)
        : m_Scope(0),
          m_DataSource(0),
          m_Entry(0),
          m_TSE(0) {}
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
    // length of 0 unless GetSeqVector() has been called for the handle.
    virtual const CSeqMap& GetSeqMap(void) const;
    // Get sequence map with resolved multi-level references.
    // The resulting map should contain regions of literals, gaps
    // and references to literals only (but not gaps or refs) of other
    // sequences.
    virtual const CSeqMap& GetResolvedSeqMap(void) const;

    // Get sequence: Iupacna or Iupacaa
    virtual CSeqVector GetSeqVector(bool plus_strand = true) const;


    // Get sequence's title (used in various flat-file formats.)
    // This function is here rather than in CBioseq because it may need
    // to inspect other sequences.  The reconstruct flag indicates that it
    // should ignore any existing title Seqdesc.
    enum EGetTitleFlags {
        fGetTitle_Reconstruct = 0x1, // ignore existing title Seqdesc.
        fGetTitle_Accession   = 0x2, // prepend (accession)
        fGetTitle_Organism    = 0x4  // append [organism]
    };
    typedef int TGetTitleFlags;
    virtual string GetTitle(TGetTitleFlags flags = 0) const;

private:
    CBioseq_Handle(CSeq_id_Handle value);

    // Get the internal seq-id handle
    const CSeq_id_Handle  GetKey(void) const;
    // Get data source
    CDataSource& x_GetDataSource(void) const;
    // Set the handle seq-entry and datasource
    void x_ResolveTo(CScope& scope, CDataSource& datasource,
                     CSeq_entry& entry, CTSE_Info& tse);

    CSeq_id_Handle       m_Value;       // Seq-id equivalent
    CScope*              m_Scope;
    mutable CDataSource* m_DataSource;  // Data source for resolved handles
    mutable CSeq_entry*  m_Entry;       // Seq-entry, containing the bioseq
    mutable CTSE_Info*   m_TSE;         // Top level seq-entry

    friend class CSeqVector;
    friend class CHandleRangeMap;
    friend class CDataSource;
    friend class CAnnot_CI;
    friend class CAnnotTypes_CI;
};


inline
CBioseq_Handle::CBioseq_Handle(CSeq_id_Handle value)
    : m_Value(value),
      m_Scope(0),
      m_DataSource(0),
      m_Entry(0),
      m_TSE(0)
{
}

inline
const CSeq_id_Handle CBioseq_Handle::GetKey(void) const
{
    return m_Value;
}

inline
bool CBioseq_Handle::operator== (const CBioseq_Handle& h) const
{
    if (m_Scope != h.m_Scope)
    {
        throw runtime_error(
            "Unable to compare CBioseq_Handles from different scopes");
    }
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
    if (m_Scope != h.m_Scope)
    {
        throw runtime_error(
            "Unable to compare CBioseq_Handles from different scopes");
    }
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
