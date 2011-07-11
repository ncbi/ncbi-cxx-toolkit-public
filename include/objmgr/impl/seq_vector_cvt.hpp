#ifndef SEQ_VECTOR_CVT__HPP
#define SEQ_VECTOR_CVT__HPP
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
*   Seq-vector conversion functions
*
*/

#if defined(NCBI_COMPILER_GCC) && defined(__i386)
# include <objmgr/impl/seq_vector_cvt_gcc_i386.hpp>
#else
# include <objmgr/impl/seq_vector_cvt_gen.hpp>
#endif

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

void NCBI_XOBJMGR_EXPORT ThrowOutOfRangeSeq_inst(TSeqPos pos);

template<class DstIter, class SrcCont>
inline
void copy_8bit_any(DstIter dst, size_t count,
                   const SrcCont& srcCont, size_t srcPos,
                   const char* table, bool reverse)
{
    size_t endPos = srcPos + count;
    if ( endPos < srcPos || endPos > srcCont.size() ) {
        ThrowOutOfRangeSeq_inst(endPos);
    }
    if ( table ) {
        if ( reverse ) {
            copy_8bit_table_reverse(dst, count, srcCont, srcPos, table);
        }
        else {
            copy_8bit_table(dst, count, srcCont, srcPos, table);
        }
    }
    else {
        if ( reverse ) {
            copy_8bit_reverse(dst, count, srcCont, srcPos);
        }
        else {
            copy_8bit(dst, count, srcCont, srcPos);
        }
    }
}


template<class DstIter, class SrcCont>
inline
void copy_4bit_any(DstIter dst, size_t count,
                   const SrcCont& srcCont, size_t srcPos,
                   const char* table, bool reverse)
{
    size_t endPos = srcPos + count;
    if ( endPos < srcPos || endPos / 2 > srcCont.size() ) {
        ThrowOutOfRangeSeq_inst(endPos);
    }
    if ( table ) {
        if ( reverse ) {
            copy_4bit_table_reverse(dst, count, srcCont, srcPos, table);
        }
        else {
            copy_4bit_table(dst, count, srcCont, srcPos, table);
        }
    }
    else {
        if ( reverse ) {
            copy_4bit_reverse(dst, count, srcCont, srcPos);
        }
        else {
            copy_4bit(dst, count, srcCont, srcPos);
        }
    }
}


template<class DstIter, class SrcCont>
inline
void copy_2bit_any(DstIter dst, size_t count,
                   const SrcCont& srcCont, size_t srcPos,
                   const char* table, bool reverse)
{
    size_t endPos = srcPos + count;
    if ( endPos < srcPos || endPos / 4 > srcCont.size() ) {
        ThrowOutOfRangeSeq_inst(endPos);
    }
    if ( table ) {
        if ( reverse ) {
            copy_2bit_table_reverse(dst, count, srcCont, srcPos, table);
        }
        else {
            copy_2bit_table(dst, count, srcCont, srcPos, table);
        }
    }
    else {
        if ( reverse ) {
            copy_2bit_reverse(dst, count, srcCont, srcPos);
        }
        else {
            copy_2bit(dst, count, srcCont, srcPos);
        }
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif//SEQ_VECTOR_CVT__HPP
