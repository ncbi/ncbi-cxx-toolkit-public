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
 * Author:  Kevin Bealer
 *
 */

/// @file writedb_general.cpp
/// Implementation for general purpose utilities for WriteDB.

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include "writedb_general.hpp"

BEGIN_NCBI_SCOPE

/// Use standard C++ definitions.
USING_SCOPE(std);

void CWriteDB_PackedSemiTree::Sort()
{
    NON_CONST_ITERATE(TPackedMap, iter, m_Packed) {
        iter->second->Sort();
    }
}

void CWriteDB_PackedSemiTree::Clear()
{
    m_Size = 0;
    TPackedMap empty;
    m_Packed.swap(empty);
}

/// Insert string data into the container.
void CWriteDB_PackedSemiTree::Insert(const char * x, int L)
{
    if (L <= PREFIX) {
        CArrayString<PREFIX> pre(x, L);
        CRef<TPacked> & packed = m_Packed[pre];
        
        if (packed.Empty()) {
            packed.Reset(new TPacked(m_Buffer));
        }
        
        packed->Insert("", 0);
    } else {
        CArrayString<PREFIX> pre(x, PREFIX);
        CRef<TPacked> & packed = m_Packed[pre];
        
        if (packed.Empty()) {
            packed.Reset(new TPacked(m_Buffer));
        }
        
        packed->Insert(x + PREFIX, L-PREFIX);
    }
    m_Size++;
}

int WriteDB_FindSequenceLength(bool protein, const string & seq)
{
    if (protein) {
        return seq.size();
    }
    
    int wholebytes = (int) seq.size() - 1;
    return (wholebytes << 2) + (seq[wholebytes] & 0x3);
}

END_NCBI_SCOPE

