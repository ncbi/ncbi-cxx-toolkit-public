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
*   CSeq_annot_Info info -- entry for data source information about Seq-annot
*
*/


#include <objects/objmgr/impl/seq_annot_info.hpp>
#include <objects/objmgr/impl/seq_entry_info.hpp>
#include <objects/seq/Seq_annot.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_annot_Info::CSeq_annot_Info(CSeq_annot& annot, CSeq_entry_Info& entry)
    : m_Seq_annot(&annot), m_Seq_entry_Info(&entry)
{
    entry.m_Annots.push_back(CRef<CSeq_annot_Info>(this));
}


CSeq_annot_Info::~CSeq_annot_Info(void)
{
}


CDataSource& CSeq_annot_Info::GetDataSource(void) const
{
    return GetSeq_entry_Info().GetDataSource();
}


const CSeq_entry& CSeq_annot_Info::GetTSE(void) const
{
    return GetSeq_entry_Info().GetTSE();
}


const CTSE_Info& CSeq_annot_Info::GetTSE_Info(void) const
{
    return GetSeq_entry_Info().GetTSE_Info();
}


CTSE_Info& CSeq_annot_Info::GetTSE_Info(void)
{
    return GetSeq_entry_Info().GetTSE_Info();
}


const CSeq_entry& CSeq_annot_Info::GetSeq_entry(void) const
{
    return GetSeq_entry_Info().GetSeq_entry();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/04/24 16:12:38  vasilche
 * Object manager internal structures are splitted more straightforward.
 * Removed excessive header dependencies.
 *
 *
 * ===========================================================================
 */
