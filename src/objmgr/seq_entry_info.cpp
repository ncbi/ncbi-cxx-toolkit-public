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
*   CSeq_entry_Info info -- entry for data source information about Seq-entry
*
*/


#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objects/seqset/Seq_entry.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_entry_Info::CSeq_entry_Info(CSeq_entry& tse)
    : m_Seq_entry(&tse), m_Parent(0), m_TSE_Info(0)
{
}


CSeq_entry_Info::CSeq_entry_Info(CSeq_entry& entry, CSeq_entry_Info& parent)
    : m_Seq_entry(&entry), m_Parent(&parent), m_TSE_Info(&parent.GetTSE_Info())
{
    _ASSERT(!parent.m_Bioseq);
    parent.m_Children.push_back(CRef<CSeq_entry_Info>(this));
}


CSeq_entry_Info::~CSeq_entry_Info(void)
{
}


CDataSource& CSeq_entry_Info::GetDataSource(void) const
{
    return GetTSE_Info().GetDataSource();
}


const CSeq_entry& CSeq_entry_Info::GetTSE(void) const
{
    return GetTSE_Info().GetTSE();
}


CSeq_entry& CSeq_entry_Info::GetTSE(void)
{
    return GetTSE_Info().GetTSE();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/06/02 16:06:38  dicuccio
 * Rearranged src/objects/ subtree.  This includes the following shifts:
 *     - src/objects/asn2asn --> arc/app/asn2asn
 *     - src/objects/testmedline --> src/objects/ncbimime/test
 *     - src/objects/objmgr --> src/objmgr
 *     - src/objects/util --> src/objmgr/util
 *     - src/objects/alnmgr --> src/objtools/alnmgr
 *     - src/objects/flat --> src/objtools/flat
 *     - src/objects/validator --> src/objtools/validator
 *     - src/objects/cddalignview --> src/objtools/cddalignview
 * In addition, libseq now includes six of the objects/seq... libs, and libmmdb
 * replaces the three libmmdb? libs.
 *
 * Revision 1.1  2003/04/24 16:12:38  vasilche
 * Object manager internal structures are splitted more straightforward.
 * Removed excessive header dependencies.
 *
 *
 * ===========================================================================
 */
