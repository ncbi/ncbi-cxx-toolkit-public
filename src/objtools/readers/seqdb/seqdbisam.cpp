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

#include <ncbi_pch.hpp>
#include "seqdbisam.hpp"

BEGIN_NCBI_SCOPE

#define ISAM_VERSION 1
#define DEFAULT_NISAM_SIZE 256
#define DEFAULT_SISAM_SIZE 64
#define MEMORY_ONLY_PAGE_SIZE 1


/* ---------------------- ISAMInitSearch --------------------------
   Purpose:     Initialize ISAM Numeric Search. Checks for any errors
   
   Parameters:  ISAM Object
   Returns:     ISAM Error Code
   NOTE:        None
   ------------------------------------------------------------------*/
CSeqDBIsam::EErrorCode
CSeqDBIsam::x_InitSearch(CSeqDBLockHold & locked)
{
    if(m_Initialized == true)
        return eNoError;
    
    TIndx info_needed = 10 * sizeof(Int4);
    
    m_Atlas.Lock(locked);
    m_Atlas.GetRegion(m_IndexLease, m_IndexFname, 0, info_needed);
    
    Int4 * FileInfo = (Int4*) m_IndexLease.GetPtr(0);
    
    // Check for consistence of files and parameters
    
    Int4 Version = SeqDB_GetStdOrd(& FileInfo[0]);
    
    if (Version != ISAM_VERSION)
        return eBadVersion;
    
    Int4 IsamType = SeqDB_GetStdOrd(& FileInfo[1]);
    
    if (IsamType != m_Type)
        return eBadType;
    
    m_NumTerms    = SeqDB_GetStdOrd(& FileInfo[3]);
    m_NumSamples  = SeqDB_GetStdOrd(& FileInfo[4]);
    m_PageSize    = SeqDB_GetStdOrd(& FileInfo[5]);
    m_MaxLineSize = SeqDB_GetStdOrd(& FileInfo[6]);
    
    if(m_PageSize != MEMORY_ONLY_PAGE_SIZE) { 
        // Special case of memory-only index
        Int4 DBFileLength = SeqDB_GetStdOrd(& FileInfo[2]);
        
        Uint4 disk_file_length(0);
        bool found = m_Atlas.GetFileSize(m_DataFname,
                                               disk_file_length);
        
        if ((! found) || (DBFileLength != (Int4) disk_file_length))
            return eWrongFile;
    }
    
    // This space reserved for future use
    
    m_IdxOption = SeqDB_GetStdOrd(& FileInfo[7]);
    
    m_KeySampleOffset = (9 * sizeof(Int4));
    
    m_Initialized = true;
    
    return eNoError;
}

Int4 CSeqDBIsam::x_GetPageNumElements(Int4   SampleNum,
                                      Int4 * Start)
{
    Int4 NumElements(0);
    
    *Start = SampleNum * m_PageSize;
    
    if (SampleNum + 1 == m_NumSamples) {
        NumElements = m_NumTerms - *Start;
    } else {
        NumElements = m_PageSize;
    }
    
    return NumElements;
}

CSeqDBIsam::EErrorCode
CSeqDBIsam::x_SearchIndexNumeric(Uint4            Number, 
                                 Uint4          * Data,
                                 Uint4          * Index,
                                 Int4           & SampleNum,
                                 bool           & done,
                                 CSeqDBLockHold & locked)
{
    if(m_Initialized == false) {
        EErrorCode error = x_InitSearch(locked);
        
        if(error != eNoError) {
            done = true;
            return error;
        }
    }
    
    bool NoData = (m_Type == eNumericNoData);
    
    // Search the sample file.
    
    Int4 Start     (0);
    Int4 Stop      (m_NumSamples - 1);
    
    Uint4 obj_size = NoData ? sizeof(Int4) : sizeof(SNumericKeyData);
    
    while(Stop >= Start) {
        SampleNum = ((Uint4)(Stop + Start)) >> 1;
	
        TIndx offset_begin = m_KeySampleOffset + (obj_size * SampleNum);
        TIndx offset_end   = offset_begin + obj_size;
	
        m_Atlas.Lock(locked);
            
        if (! m_IndexLease.Contains(offset_begin, offset_end)) {
            m_Atlas.GetRegion(m_IndexLease,
                              m_IndexFname,
                              offset_begin,
                              offset_end);
        }
            
        SNumericKeyData* keydatap(0);
            
        Uint4 Key;
            
        if (NoData) {
            Key = SeqDB_GetStdOrd((Int4*) m_IndexLease.GetPtr(offset_begin));
        } else {
            keydatap = (SNumericKeyData*) m_IndexLease.GetPtr(offset_begin);
            Key = SeqDB_GetStdOrd(& (keydatap->key));
        }
            
        // If this is an exact match, return the master term number.
            
        if (Key == Number) {
            if (Data != NULL) {
                if (NoData) {
                    *Data = SampleNum * m_PageSize;
                } else {
                    *Data = SeqDB_GetStdOrd(& keydatap->data);
                }
            }
            
            if (Index != NULL)
                *Index = SampleNum * m_PageSize;
            
            done = true;
            return eNoError;
        }
        
        // Otherwise, search for the next sample.
        
        if ( Number < Key )
            Stop = --SampleNum;
        else
            Start = SampleNum +1;
    }
    
    // If the term is out of range altogether, report not finding it.
    
    if ( (SampleNum < 0) || (SampleNum >= m_NumSamples)) {
        
        if (Data != NULL)
            *Data = eNotFound;
        
        if(Index != NULL)
            *Index = eNotFound;
        
        done = true;
        return eNotFound;
    }
    
    done = false;
    return eNoError;
}

CSeqDBIsam::EErrorCode
CSeqDBIsam::x_SearchDataNumeric(Uint4            Number,
                                Uint4          * Data,
                                Uint4          * Index,
                                Int4             SampleNum,
                                CSeqDBLockHold & locked)
{
    // Load the appropriate page of numbers into memory.
    bool NoData = (m_Type == eNumericNoData);
    
    Int4 Start(0);
    Int4 NumElements = x_GetPageNumElements(SampleNum, & Start);
    
    Int4 first = Start;
    Int4 last  = Start + NumElements - 1;
    
    Int4 * KeyPage     (0);
    Int4 * KeyPageStart(0);
    
    SNumericKeyData * KeyDataPage      = NULL;
    SNumericKeyData * KeyDataPageStart = NULL;
    
    
    if (NoData) {
        TIndx offset_begin = Start*sizeof(Int4);
        TIndx offset_end = offset_begin + sizeof(Int4)*NumElements;
        
        m_Atlas.Lock(locked);
        
        if (! m_DataLease.Contains(offset_begin, offset_end)) {
            m_Atlas.GetRegion(m_DataLease,
                              m_DataFname,
                              offset_begin,
                              offset_end);
        }
        
        KeyPageStart = (Int4*) m_DataLease.GetPtr(offset_begin);
        
        KeyPage = KeyPageStart - Start;
    } else {
        TIndx offset_begin = Start * sizeof(SNumericKeyData);
        TIndx offset_end = offset_begin + sizeof(SNumericKeyData) * NumElements;
        
        m_Atlas.Lock(locked);
        
        if (! m_DataLease.Contains(offset_begin, offset_end)) {
            m_Atlas.GetRegion(m_DataLease,
                              m_DataFname,
                              offset_begin,
                              offset_end);
        }
        
        KeyDataPageStart = (SNumericKeyData*) m_DataLease.GetPtr(offset_begin);
        
        KeyDataPage = KeyDataPageStart - Start;
    }
    
    bool found   (false);
    Int4 current (0);
    
    // Search the page for the number.
    if (NoData) {
        while (first <= last) {
            current = (first+last)/2;
            
            Uint4 Key = SeqDB_GetStdOrd(& KeyPage[current]);
            
            if (Key > Number)
                last = --current;
            else if (Key < Number)
                first = ++current;
            else {
                found = true;
                break;
            }
        }
    } else {
        while (first <= last) {
            current = (first+last)/2;
            Uint4 Key = SeqDB_GetStdOrd(& KeyDataPage[current].key);
            if (Key > Number) {
                last = --current;
            } else if (Key < Number) {
                first = ++current;
            } else {
                found = true;
                break;
            }
        }
    }
    
    if (found == false) {
        if (Data != NULL)
            *Data = eNotFound;
        
        if(Index != NULL)
            *Index = eNotFound;
        
        return eNotFound;
    }
    
    if (Data != NULL) {
        if (NoData)
            *Data = Start + current;
        else
            *Data = SeqDB_GetStdOrd(& KeyDataPage[current].data);
    }
    
    if(Index != NULL)
        *Index = Start + current;
    
    return eNoError;
}

/* ------------------------NumericSearch--------------------------
   Purpose:     Main search function of Numeric ISAM
   
   Parameters:  Key - interer to search
                Data - returned value (for NIASM with data)
                Index - internal index in database
   Returns:     ISAM Error Code
   NOTE:        None
   ----------------------------------------------------------------*/
CSeqDBIsam::EErrorCode
CSeqDBIsam::x_NumericSearch(Uint4            Number, 
                            Uint4          * Data,
                            Uint4          * Index,
                            CSeqDBLockHold & locked)
{
    bool done      (false);
    Int4 SampleNum (0);
    
    EErrorCode error =
        x_SearchIndexNumeric(Number, Data, Index, SampleNum, done, locked);
    
    if (! done) {
        error = x_SearchDataNumeric(Number, Data, Index, SampleNum, locked);
    }
    
    return error;
}

CSeqDBIsam::CSeqDBIsam(CSeqDBAtlas  & atlas,
                       const string & dbname,
                       char           prot_nucl,
                       char           file_ext_char,
                       EIdentType     ident_type) // may not be needed.
    : m_Atlas          (atlas),
      m_IdentType      (ident_type),
      m_IndexLease     (atlas),
      m_DataLease      (atlas),
      m_Type           (eNumeric),
      m_NumTerms       (0),
      m_NumSamples     (0),
      m_PageSize       (0),
      m_MaxLineSize    (0),
      m_IdxOption      (0),
      m_Initialized    (false),
      m_KeySampleOffset(0),
      m_TestNonUnique  (true),
      m_FileStart      (0),
      m_FirstOffset    (0),
      m_LastOffset     (0)
{
    if (dbname.empty() ||
        (! isalpha(prot_nucl)) ||
        (! isalpha(file_ext_char))) {
        
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Error: argument not valid");
    }
    
    m_IndexFname.reserve(dbname.size() + 4);
    m_DataFname.reserve(dbname.size() + 4);
    
    m_IndexFname = dbname;
    m_IndexFname += '.';
    m_IndexFname += prot_nucl;
    m_IndexFname += file_ext_char;
    
    m_DataFname = m_IndexFname;
    m_IndexFname += 'i';
    m_DataFname  += 'd';
    
    if (! (CFile(m_IndexFname).Exists() &&
           CFile(m_DataFname).Exists()) ) {
        
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Error: Could not open input file.");
    }
    
    if(m_IsamType == eNumeric ||
       m_IsamType == eNumericNoData) {
        
        m_PageSize = DEFAULT_NISAM_SIZE;
    } else {
        m_PageSize = DEFAULT_SISAM_SIZE;
    }
}

CSeqDBIsam::~CSeqDBIsam()
{
    UnLease();
}

void CSeqDBIsam::UnLease()
{
    if (! m_IndexLease.Empty()) {
        m_Atlas.RetRegion(m_IndexLease);
    }
    if (! m_DataLease.Empty()) {
        m_Atlas.RetRegion(m_DataLease);
    }
}

bool CSeqDBIsam::x_IdentToOid(Uint4 ident, TOid & oid, CSeqDBLockHold & locked)
{
    EErrorCode err =
        x_NumericSearch(ident, & oid, 0, locked);
    
    if (err == eNoError) {
        return true;
    }
    
    oid = Uint4(-1);
    
    return false;
}

END_NCBI_SCOPE


