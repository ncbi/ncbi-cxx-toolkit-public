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

/// @file seqdbfile.cpp
/// Several classes providing access to the component files of a
/// database volume.
#include <ncbi_pch.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdbfile.hpp>

BEGIN_NCBI_SCOPE

/// Index file.
///
/// Index files (extension nin or pin) contain information on where to
/// find information in other files.  The OID is the (implied) key.


// A Word About Mutexes and Mutability in the File Classes
//
// The stream object in CSeqDBRawFile is mutable: this is because the
// stream access methods modify the file.  Specifically, they modify
// the file offset.  This means that two users of a stream object will
// step on each other if they try to read from different offsets
// concurrently.  Memory mapping does not have this problem of course.
//
// To fix this, the file object is mutable, but to access it, the user
// needs to hold the m_FileLock mutex.
//
// One goal I have for these classes is to eliminate all locking for
// the mmap case.  Locking is not needed to call a const method, so
// methods are marked const whenever possible.  After construction of
// CSeqDB, ONLY const methods are called.
//
// Some of the const methods need to modify fields; to do this, I mark
// the fields 'mutable' and hold a mutex whenever accessing them.
//
// Each method falls into one of these categories:
//
// 1. Non-const: called only during CSeqDB construction.
// 2. Const: no changes to any fields.
// 3. Const: modifies mutable fields while holding m_FileLock.

typedef CSeqDBAtlas::TIndx TIndx;

TIndx CSeqDBRawFile::ReadSwapped(CSeqDBFileMemMap & lease,
                                 TIndx            offset,
                                 Uint4          * value) const
                                 
{
    *value = SeqDB_GetStdOrd((Uint4 *) lease.GetFileDataPtr(m_FileName,offset));
    
    return offset + sizeof(*value);
}

TIndx CSeqDBRawFile::ReadSwapped(CSeqDBFileMemMap & lease,
                                 TIndx            offset,
                                 Uint8          * value) const
                                 
{
    *value = SeqDB_GetBroken((Int8 *) lease.GetFileDataPtr(m_FileName,offset));
    
    return offset + sizeof(*value);
}

TIndx CSeqDBRawFile::ReadSwapped(CSeqDBFileMemMap & lease,
                                 TIndx            offset,
                                 string         * value) const                                 
{
    Uint4 len = 0;
    
    len = SeqDB_GetStdOrd((Int4 *) lease.GetFileDataPtr(m_FileName,offset));
    
    offset += sizeof(len);
    
    value->assign(lease.GetFileDataPtr(offset), (int) len);
    
    return offset + len;
}

CSeqDBExtFile::CSeqDBExtFile(CSeqDBAtlas    & atlas,
                             const string   & dbfilename,
                             char             prot_nucl)
                             
    : m_Atlas   (atlas), 
      m_FileName(dbfilename),
      m_Lease    (atlas),      
      m_File    (atlas)
{
    if ((prot_nucl != 'p') && (prot_nucl != 'n')) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Error: Invalid sequence type requested.");
    }
    
    x_SetFileType(prot_nucl);
    
    if (! m_File.Open(CSeqDB_Path(m_FileName))) {
        //m_Atlas.Unlock(locked);
        
        string msg = string("Error: File (") + m_FileName + ") not found.";
        
        NCBI_THROW(CSeqDBException, eFileErr, msg);
    }    
    
    m_Lease.Init(m_FileName);
}

CSeqDBIdxFile::CSeqDBIdxFile(CSeqDBAtlas    & atlas,
                             const string   & dbname,
                             char             prot_nucl)
                             
    : CSeqDBExtFile(atlas, dbname + ".-in", prot_nucl),
      m_HdrLease     (atlas),
      m_SeqLease      (atlas),
      m_AmbLease      (atlas),
      m_NumOIDs       (0),
      m_VolLen        (0),
      m_MaxLen        (0),
      m_MinLen        (0),      
      m_OffHdr        (0),
      m_EndHdr        (0),
      m_OffSeq        (0),
      m_EndSeq        (0),
      m_OffAmb        (0),
      m_EndAmb        (0),
      m_LMDBFile	  (kEmptyStr) ,
      m_Volume        (0)
{
    //Verify();
    
    // Input validation
    
    if (dbname.empty()) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Error: dbname should not be an empty string.");
    }
    
    if ((prot_nucl != 'p') && (prot_nucl != 'n')) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Error: Invalid sequence type requested.");
    }
    
    TIndx offset = 0;
    
    
    Uint4 f_format_version = 0;
    Uint4 f_db_seqtype = 0;
    
        
    offset = x_ReadSwapped(m_Lease, offset, & f_format_version);
    
    TIndx off1(0), off2(0), off3(0), offend(0);
    
    try {
    	 bool dbv5 = ( 5 == f_format_version );
        if (!dbv5 && (f_format_version != 4)) {
            NCBI_THROW(CSeqDBException,
                       eFileErr,
                       "Error: Not a valid version 4 or 5 database.");
        }

        offset = x_ReadSwapped(m_Lease, offset, & f_db_seqtype);
        if (dbv5) {
        	offset = x_ReadSwapped(m_Lease, offset, & m_Volume);
        }
        
        offset = x_ReadSwapped(m_Lease, offset, & m_Title);
        if (dbv5) {
            offset = x_ReadSwapped(m_Lease, offset, & m_LMDBFile);
       }
        
        offset = x_ReadSwapped(m_Lease, offset, & m_Date);

        
        offset = x_ReadSwapped(m_Lease, offset, & m_NumOIDs);

        
        offset = x_ReadSwapped(m_Lease, offset, & m_VolLen);

        
        offset = x_ReadSwapped(m_Lease, offset, & m_MaxLen);
        
        TIndx region_bytes = 4 * (m_NumOIDs + 1);
        
        off1   = offset;
        off2   = off1 + region_bytes;
        off3   = off2 + region_bytes;
        offend = off3 + region_bytes;
    }
    catch(...) {        
        throw;
    }
    
        
    char db_seqtype = ((f_db_seqtype == 1) ? 'p' : 'n');
    
    if (db_seqtype != x_GetSeqType()) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Error: requested sequence type does not match DB.");
    }
    
    m_OffHdr = off1; m_EndHdr = off2;
    m_OffSeq = off2; m_EndSeq = off3;
    
    if (db_seqtype == 'n') {
        m_OffAmb = off3; m_EndAmb = offend;
    } else {
        m_OffAmb = m_EndAmb = 0;
    }
}

END_NCBI_SCOPE

