#ifndef SEQ_ANNOT_INFO__HPP
#define SEQ_ANNOT_INFO__HPP

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
*   Seq-annot object information
*
*/

#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataSource;
class CSeq_annot;
class CSeq_entry;
class CTSE_Info;
class CSeq_entry_Info;

class NCBI_XOBJMGR_EXPORT CSeq_annot_Info : public CObject
{
public:
    CSeq_annot_Info(CSeq_annot& annot, CSeq_entry_Info& entry);
    ~CSeq_annot_Info(void);

    CDataSource& GetDataSource(void) const;

    const CSeq_entry& GetTSE(void) const;
    const CTSE_Info& GetTSE_Info(void) const;
    CTSE_Info& GetTSE_Info(void);

    const CSeq_entry& GetSeq_entry(void) const;
    const CSeq_entry_Info& GetSeq_entry_Info(void) const;
    CSeq_entry_Info& GetSeq_entry_Info(void);

    const CSeq_annot& GetSeq_annot(void) const;
    CSeq_annot& GetSeq_annot(void);

private:
    friend class CDataSource;

    CSeq_annot_Info(const CSeq_annot_Info&);
    CSeq_annot_Info& operator=(const CSeq_annot_Info&);

    CRef<CSeq_annot>       m_Seq_annot;
    CSeq_entry_Info*       m_Seq_entry_Info;

    bool m_Indexed;
};


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
const CSeq_annot& CSeq_annot_Info::GetSeq_annot(void) const
{
    return *m_Seq_annot;
}


inline
CSeq_annot& CSeq_annot_Info::GetSeq_annot(void)
{
    return *m_Seq_annot;
}


inline
const CSeq_entry_Info& CSeq_annot_Info::GetSeq_entry_Info(void) const
{
    return *m_Seq_entry_Info;
}


inline
CSeq_entry_Info& CSeq_annot_Info::GetSeq_entry_Info(void)
{
    return *m_Seq_entry_Info;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
*
* ===========================================================================
*/

#endif  // SEQ_ANNOT_INFO__HPP
