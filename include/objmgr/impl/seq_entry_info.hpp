#ifndef OBJECTS_OBJMGR_IMPL___SEQ_ENTRY_INFO__HPP
#define OBJECTS_OBJMGR_IMPL___SEQ_ENTRY_INFO__HPP

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
*   Seq_entry info -- entry for data source
*
*/


#include <corelib/ncbiobj.hpp>
#include <list>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// forward declaration
class CSeq_entry;
class CBioseq;
class CSeq_annot;
class CDataSource;
class CTSE_Info;
class CSeq_entry_Info;
class CBioseq_Info;
class CSeq_annot_Info;

////////////////////////////////////////////////////////////////////
//
//  CSeq_entry_Info::
//
//    General information and indexes for seq-entries
//


class NCBI_XOBJMGR_EXPORT CSeq_entry_Info : public CObject
{
public:
    // 'ctors
    CSeq_entry_Info(CSeq_entry& entry, CSeq_entry_Info& parent);
    virtual ~CSeq_entry_Info(void);

    CDataSource& GetDataSource(void) const;

    const CSeq_entry& GetTSE(void) const;
    CSeq_entry& GetTSE(void);

    const CTSE_Info& GetTSE_Info(void) const;
    CTSE_Info& GetTSE_Info(void);

    const CSeq_entry_Info* GetParentSeq_entry_Info(void) const;

    bool NullSeq_entry(void) const;
    const CSeq_entry& GetSeq_entry(void) const;
    CSeq_entry& GetSeq_entry(void);

    const CBioseq_Info& GetBioseq_Info(void) const;
    CBioseq_Info& GetBioseq_Info(void);

protected:
    friend class CDataSource;
    friend class CBioseq_Info;
    friend class CSeq_annot_Info;
    friend class SAnnotILevel;
    friend class CSeq_annot_CI;

    CSeq_entry_Info(CSeq_entry& entry);

    // Seq-entry pointer
    CRef<CSeq_entry>      m_Seq_entry;

    // parent Seq-entry info
    CSeq_entry_Info*      m_Parent;

    // top level Seq-entry info
    CTSE_Info*            m_TSE_Info;

    typedef list< CRef<CSeq_entry_Info> > TChildren;
    typedef list< CRef<CSeq_annot_Info> > TAnnots;
    
    TChildren             m_Children;
    CRef<CBioseq_Info>    m_Bioseq;
    TAnnots               m_Annots;

    // Hide copy methods
    CSeq_entry_Info(const CSeq_entry_Info&);
    CSeq_entry_Info& operator= (const CSeq_entry_Info&);
};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
const CTSE_Info& CSeq_entry_Info::GetTSE_Info(void) const
{
    return *m_TSE_Info;
}


inline
CTSE_Info& CSeq_entry_Info::GetTSE_Info(void)
{
    return *m_TSE_Info;
}


inline
const CSeq_entry_Info* CSeq_entry_Info::GetParentSeq_entry_Info(void) const
{
    return m_Parent;
}


inline
bool CSeq_entry_Info::NullSeq_entry(void) const
{
    return m_Seq_entry;
}


inline
const CSeq_entry& CSeq_entry_Info::GetSeq_entry(void) const
{
    return *m_Seq_entry;
}


inline
CSeq_entry& CSeq_entry_Info::GetSeq_entry(void)
{
    return *m_Seq_entry;
}


inline
const CBioseq_Info& CSeq_entry_Info::GetBioseq_Info(void) const
{
    return *m_Bioseq;
}


inline
CBioseq_Info& CSeq_entry_Info::GetBioseq_Info(void)
{
    return *m_Bioseq;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2003/07/25 15:25:24  grichenk
* Added CSeq_annot_CI class
*
* Revision 1.1  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_OBJMGR_IMPL___SEQ_ENTRY_INFO__HPP */
