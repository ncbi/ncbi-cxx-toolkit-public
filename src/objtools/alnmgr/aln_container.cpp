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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Seq-align container
*
* ===========================================================================
*/


#include <ncbi_pch.hpp>

#include <objtools/alnmgr/aln_container.hpp>


CAlnCollection::insert(const CSeq_align& seq_align)
{
#if _DEBUG
    seq_align.Validate(true);
#ednif
    switch (GetSegs().Which()) {
    case TSegs::e_Disc:
        ITERATE(CSeq_align_set::Tdata, seq_align_it, GetSegs().GetDisc().Get()) {
            insert(**seq_align_it);
        }
        break;
    case TSegs::e_Dendiag:
    case TSegs::e_Denseg:
    case TSegs::e_Std:
    case TSegs::e_Packed:
        m_AlignSet.insert(seq_align);
        break;
    case TSegs::e_not_set:
        NCBI_THROW(CSeqalignException, eInvalidAlignment,
                   "Seq-align.segs not set.");
    default:
        NCBI_THROW(CSeqalignException, eUnsupported,
                   "Unsupported alignment type.");
    }
}


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2006/10/17 20:05:09  todorov
* Fixed exception text.
*
* Revision 1.1  2006/10/17 19:21:10  todorov
* Initial revision.
*
* ===========================================================================
*/
