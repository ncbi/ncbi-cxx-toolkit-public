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

#include <corelib/ncbistr.hpp>
#include "seqdboidlist.hpp"
#include "seqdbfile.hpp"

BEGIN_NCBI_SCOPE

/// Index file.
///
/// Index files (extension nin or pin) contain information on where to
/// find information in other files.  The OID is the (implied) key.


// Public Constructor
//
// This is the user-visible constructor, which builds the top level
// node in the dbalias node tree.  This design effectively treats the
// user-input database list as if it were an alias file containing
// only the DBLIST specification.












// Note: this is a bad design.  Instead of a class that is derived
// from a file, use an internal file.  It only has to be opened in the
// cases where the input is a file.  In other cases, the data can be
// kept in a non-file and the rest of the class works.
//
// All that the iterating parts of the code need to know is where the
// bytes are kept.  The rest of the stuff can be


CSeqDBOIDList::CSeqDBOIDList(const string & filename, bool use_mmap)
    : m_RawFile(use_mmap),
      m_NumOIDs(0),
      m_Bits   (0),
      m_BitEnd (0)
{
    m_RawFile.Open(filename);
    m_RawFile.ReadSwapped(& m_NumOIDs);
    
    Uint4 file_length = m_RawFile.GetFileLength();
    
    m_Bits   = m_RawFile.GetRegion(sizeof(Int4), file_length);
    m_BitEnd = m_Bits + file_length - sizeof(Int4);
}

CSeqDBOIDList::~CSeqDBOIDList()
{
}

bool CSeqDBOIDList::x_IsSet(TOID oid)
{
    const char * bp = m_Bits + (oid >> 3);
    
    Int4 bitnum = (oid & 7);
    
    if (bp < m_BitEnd) {
        if (((*bp) << bitnum) & 0x80) {
            return true;
        }
    }
    
    return false;
}

bool CSeqDBOIDList::x_FindNext(TOID & oid)
{
    // We have a bit somewhere in this byte, so..
    while(oid < m_NumOIDs) {
        if (x_IsSet(oid)) {
            return true;
        }
        
        oid++;
        
        // If we have come into a new byte, check if it is null and
        // skip any null bytes.  It would be more efficient to skip
        // words, but this is probably good enough to reduce the
        // runtime of this part of the code to a microscopic amount,
        // which is good enough for me.
        
        if ((oid & 0x7) == 0) {
            const char * bp = m_Bits + (oid >> 3);
            const char * ep = m_BitEnd;
            
            while((bp < ep) && (0 == *bp)) {
                ++ bp;
            }
        }
    }
    
    return false;
}

END_NCBI_SCOPE

