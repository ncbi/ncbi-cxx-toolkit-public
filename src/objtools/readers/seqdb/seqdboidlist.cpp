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
    
    {
        Uint4 bits = 0;
        const Uint4 * filep = (const Uint4 *) m_Bits;
        const Uint4 * filee = (const Uint4 *) m_BitEnd;
        
        while (filep < filee) {
            Uint4 x = *filep;
            while(x) {
                if (x & 1)
                    bits++; 
                x >>= 1;
            }
            filep++;
        }
        
        cout << "Lots of bytes, total bits = " << bits << endl;
    }
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
    // If the specified OID is valid, use it.
    
    if (x_IsSet(oid)) {
        return true;
    }
    
    // OPTIONAL portion
    
    Uint4 whole_word_oids = m_NumOIDs & -32;
    
    while(oid < whole_word_oids) {
        if (x_IsSet(oid)) {
            return true;
        }
        
        oid ++;
        
        if ((oid & 31) == 0) {
            const Uint4 * bp = ((const Uint4*) m_Bits + (oid             >> 5));
            const Uint4 * ep = ((const Uint4*) m_Bits + (whole_word_oids >> 5));
            
            while((bp < ep) && (0 == *bp)) {
                ++ bp;
                oid += 32;
            }
        } else if ((oid & 7) == 0) {
            const char * bp = ((const char*) m_Bits + (oid             >> 3));
            const char * ep = ((const char*) m_Bits + (whole_word_oids >> 3));
            
            while((bp < ep) && (0 == *bp)) {
                ++ bp;
                oid += 8;
            }
        }
    }
    
    // END of OPTIONAL portion
    
    while(oid < m_NumOIDs) {
        if (x_IsSet(oid)) {
            return true;
        }
        
        oid++;
    }
    
    return false;
}

END_NCBI_SCOPE

