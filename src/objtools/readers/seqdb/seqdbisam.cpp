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

/// @file seqdbisam.cpp
/// Implementation for the CSeqDBIsam class, which manages an ISAM
/// index of some particular kind of identifiers.

#include <ncbi_pch.hpp>
#include "seqdbisam.hpp"
#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/general__.hpp>
#include <corelib/ncbiutil.hpp>

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

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
    
    bool found = m_Atlas.GetFileSize(m_IndexFname, m_IndexFileLength);
    
    if ((! found) || (m_IndexFileLength < info_needed)) {
        return eWrongFile;
    }
    
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
        m_DataFileLength = SeqDB_GetStdOrd(& FileInfo[2]);
        
        Uint4 disk_file_length(0);
        bool found = m_Atlas.GetFileSize(m_DataFname, disk_file_length);
        
        if ((! found) || (m_DataFileLength != disk_file_length))
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
    m_Atlas.Lock(locked);
    
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
        
        Uint4 Key(0);
        
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

// ------------------------NumericSearch--------------------------
// Purpose:     Main search function of Numeric ISAM
// 
// Parameters:  Key - interer to search
//              Data - returned value (for NIASM with data)
//              Index - internal index in database
// Returns:     ISAM Error Code
// NOTE:        None
// ----------------------------------------------------------------

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

Int4 CSeqDBIsam::x_DiffCharLease(const string   & term_in,
                                 CSeqDBMemLease & lease,
                                 const string   & file_name,
                                 Uint4            file_length,
                                 Uint4            at_least,
                                 Uint4            KeyOffset,
                                 bool             ignore_case,
                                 CSeqDBLockHold & locked)
{
    Int4 result(-1);
    
    m_Atlas.Lock(locked);
    
    // Add one to term_end to insure we don't "AA" and "AAB" as equal.
    
    Uint4 offset_begin = KeyOffset;
    Uint4 term_end     = KeyOffset + term_in.size() + 1;
    Uint4 map_end      = term_end + at_least;
    
    if (map_end > file_length) {
        map_end = file_length;
        
        if (term_end > map_end) {
            term_end = map_end;
            result = file_length - offset_begin;
        }
    }
    
    if (! lease.Contains(offset_begin, map_end)) {
        m_Atlas.GetRegion( lease,
                           file_name,
                           offset_begin,
                           term_end );
    }
    
    const char * file_data = lease.GetPtr(offset_begin);
    
    Int4 dc_result =
        x_DiffChar(term_in,
                   file_data,
                   file_data + term_in.size() + 1,
                   ignore_case);
    
    if (dc_result != -1) {
        result = dc_result;
    }
    
    return dc_result;
}

const char ISAM_DATA_CHAR = (char) 2;

inline bool ENDS_ISAM_KEY(char P)
{
    return (P == 0) || (P == ISAM_DATA_CHAR) || (P == '\n') || (P == '\r');
}

Int4 CSeqDBIsam::x_DiffChar(const string & term_in,
                            const char   * begin,
                            const char   * end,
                            bool           ignore_case)
{
    Int4 result(-1);
    Uint4 i(0);
    
    const char * file_data = begin;
    Uint4 bytes = end-begin;
    
    for(i = 0; (i < bytes) && i < term_in.size(); i++) {
        char ch1 = term_in[i];
        char ch2 = file_data[i];
        
        if (ignore_case) {
            ch1 = toupper(ch1);
            ch2 = toupper(ch2);
        }
        
        if (ch1 != ch2) {
            break;
        }
    }
    
    const char * p = file_data + i;
    
    while((p < end) && ((*p) == ' ')) {
        p++;
    }
    
    if (((p == end) || ENDS_ISAM_KEY(*p)) && (i == term_in.size())) {
        result = -1;
    } else {
        result = i;
    }
    
    return result;
}

void CSeqDBIsam::x_ExtractData(const char * key_start,
                               const char * map_end,
                               string     & key_out,
                               string     & data_out)
{
    const char * data_ptr(0);
    const char * p(key_start);
    
    while(p < map_end) {
        switch(*p) {
        case 0:
            if (data_ptr) {
                key_out.assign(key_start, data_ptr);
                data_out.assign(data_ptr+1, p);
            } else {
                key_out.assign(key_start, p);
                data_out.clear();
            }
            return;
            
        case ISAM_DATA_CHAR:
            data_ptr = p;
            
        default:
            p++;
        }
    }
}

Uint4
CSeqDBIsam::x_GetIndexKeyOffset(Uint4            sample_offset,
                                Uint4            sample_num,
                                CSeqDBLockHold & locked)
{
    TIndx offset_begin = sample_offset + (sample_num * sizeof(Uint4));
    TIndx offset_end   = offset_begin + sizeof(Uint4);
    
    m_Atlas.Lock(locked);
    
    if (! m_IndexLease.Contains(offset_begin, offset_end)) {
        m_Atlas.GetRegion(m_IndexLease,
                          m_IndexFname,
                          offset_begin,
                          offset_end);
    }
    
    Int4 * key_offset_addr = (Int4 *) m_IndexLease.GetPtr(offset_begin);
    
    return SeqDB_GetStdOrd(key_offset_addr);
}

void
CSeqDBIsam::x_GetIndexString(Uint4            key_offset,
                             Uint4            length,
                             string         & str,
                             bool             trim_to_null,
                             CSeqDBLockHold & locked)
{
    TIndx offset_end = key_offset + length;
    
    m_Atlas.Lock(locked);
    
    if (! m_IndexLease.Contains(key_offset, offset_end)) {
        m_Atlas.GetRegion(m_IndexLease,
                          m_IndexFname,
                          key_offset,
                          offset_end);
    }
    
    Int4 * key_offset_addr = (Int4 *) m_IndexLease.GetPtr(key_offset);
    
    if (trim_to_null) {
        for(Uint4 i = 0; i<length; i++) {
            if (! key_offset_addr[i]) {
                length = i;
                break;
            }
        }
    }
    
    str.assign(key_offset_addr, key_offset_addr + length);
}


// ------------------------StringSearch--------------------------
// Purpose:     Main search function of Numeric ISAM
// 
// Parameters:  Key - interer to search
//              Data - returned value (for NIASM with data)
//              Index - internal index in database
// Returns:     ISAM Error Code
// NOTE:        None
// --------------------------------------------------------------

CSeqDBIsam::EErrorCode
CSeqDBIsam::x_StringSearch(const string   & term_in,
                           bool             follow_match,
                           bool             short_match,
                           string         & term_out,
                           string         & value_out,
                           Uint4          & index_out,
                           CSeqDBLockHold & locked)
{
    if (m_Initialized == false) {
        EErrorCode error = x_InitSearch(locked);
        
        if(error != eNoError) {
            return error;
        }
    }
    
    // We will set this option to avoid more complications
    bool ignore_case = true;
    
    // search the sample file first
    
    Int4 Start(0);
    Int4 Stop(m_NumSamples - 1);
    
    Int4 Length    = term_in.size();
    
    Uint4 SampleOffset(m_KeySampleOffset);
    
    if(m_PageSize != MEMORY_ONLY_PAGE_SIZE) {
        SampleOffset += (m_NumSamples + 1) * sizeof(Uint4);
    }
    
    Int4 found_short(-1);
    
    string short_term;
    Int4 SampleNum(-1);
    
    while(Stop >= Start) {
        SampleNum = ((Uint4)(Stop + Start)) >> 1;
        
        // Meaning:
        // a. Compute SampleNum*4
        // b. Address this number into SamplePos (indexlease)
        // c. Swap this number to compute Key offset.
        // d. Add to beginning of file to get key data pointer.
        
        TIndx offset_begin = SampleOffset + (SampleNum * sizeof(Uint4));
        TIndx offset_end   = offset_begin + sizeof(Uint4);
        
        m_Atlas.Lock(locked);
        
        if (! m_IndexLease.Contains(offset_begin, offset_end)) {
            m_Atlas.GetRegion(m_IndexLease,
                              m_IndexFname,
                              offset_begin,
                              offset_end);
        }
        
        Uint4 KeyOffset =
            SeqDB_GetStdOrd((Int4*) m_IndexLease.GetPtr(offset_begin));
        
        Uint4 max_lines_2 = m_MaxLineSize * 2;
        
        Int4 diff = x_DiffCharLease(term_in,
                                    m_IndexLease,
                                    m_IndexFname,
                                    m_IndexFileLength,
                                    max_lines_2,
                                    KeyOffset,
                                    ignore_case,
                                    locked);
        
        // If this is an exact match, return the master term number.
        
        const char * KeyData = m_IndexLease.GetPtr(KeyOffset);
        Uint4 BytesToEnd = m_IndexFileLength - KeyOffset;
        
        if (BytesToEnd > max_lines_2) {
            BytesToEnd = max_lines_2;
        }
        
        if (diff == -1) {
            x_ExtractData(KeyData, KeyData + BytesToEnd, term_out, value_out);
            
            index_out = m_PageSize * SampleNum;
            return eNoError;
        }
        
        // If the key is a superset of the sample term, backup until
        // just before the term.
        
        if (short_match && (diff >= Length)) {
            if (SampleNum > 0)
                SampleNum--;
            
            while(SampleNum > 0) {
                Uint4 key_offset =
                    x_GetIndexKeyOffset(SampleOffset,
                                        SampleNum,
                                        locked);
                
                string prefix;
                x_GetIndexString(key_offset, Length, prefix, false, locked);
                
                if (ignore_case) {
                    if (NStr::CompareNocase(prefix, term_in) != 0) {
                        break;
                    }
                } else {
                    if (prefix != term_in) {
                        break;
                    }
                }
                
                SampleNum--;
            }
            
            found_short = SampleNum + 1;
            
            Uint4 key_offset =
                x_GetIndexKeyOffset(SampleOffset,
                                    SampleNum + 1,
                                    locked);
            
            string prefix;
            x_GetIndexString(key_offset, max_lines_2, short_term, true, locked);
            
            break;
        } else {
            // If preceding is desired, note the key.
            
            if (follow_match) {
                found_short = SampleNum;
                
                x_GetIndexString(KeyOffset, max_lines_2, short_term, true, locked);
            }
        }
        
        // Otherwise, search for the next sample.
        
        if (ignore_case
            ? tolower(term_in[diff]) < tolower(KeyData[diff])
            : term_in[diff] < KeyData[diff]) {
            Stop = --SampleNum;
        } else {
            Start = SampleNum + 1;
        }
    }
    
    
    // If the term is out of range altogether, report not finding it.
    
    if ( (SampleNum < 0) || (SampleNum >= m_NumSamples)) {
        return eNotFound;
    }
    
    
    // Load the appropriate page of terms into memory.
    
    Uint4 begin_offset = m_KeySampleOffset + SampleNum * sizeof(Uint4);
    Uint4 end_offset   = m_KeySampleOffset + (SampleNum+2) * sizeof(Uint4);
    
    if (! m_IndexLease.Contains(begin_offset, end_offset)) {
        m_Atlas.GetRegion(m_IndexLease, m_IndexFname, begin_offset, end_offset);
    }
    
    Uint4 * key_offsets((Uint4*) m_IndexLease.GetPtr(begin_offset));
    
    Uint4 key_off1 = SeqDB_GetStdOrd(& key_offsets[0]);
    Uint4 key_off2 = SeqDB_GetStdOrd(& key_offsets[1]);
    Uint4 num_bytes = key_off2 - key_off1;
    
    if (! m_DataLease.Contains(key_off1, key_off2)) {
        m_Atlas.GetRegion(m_DataLease, m_DataFname, key_off1, key_off2);
    }
    
    string Page;
    Page.reserve(num_bytes + 1);
    
    Page.assign((const char *) m_DataLease.GetPtr(key_off1),
                (const char *) m_DataLease.GetPtr(key_off2));
    
    Page.resize(num_bytes + 1);
    Page[Page.size()-1] = 0;
    
    // Remove all \n and \r characters
    
    for(Uint4 i = 0; i < Page.size(); i++) {
        switch(Page[i]) {
        case '\n':
        case '\r':
            Page[i] = 0;
            break;
            
        default:
            break;
        }
    }
    
    // Search the page for the term.
    
    Uint4 TermNum = 0;
    Uint4 idx = 0;
    
    while (idx < Page.size()) {
        Int4 Diff = x_DiffChar(term_in,
                               Page.data() + idx,
                               Page.data() + Page.size(),
                               ignore_case);
        
        if (Diff == -1) { // Complete match
            break;
        }
        
        if (short_match && (Diff >= Length)) { // Partially complete
            break;
        }
        
        // Just next available term accepted
        
        // (Note: non-portable code)
        
        if (follow_match) {
            if (ignore_case) {
                if (toupper(term_in[Diff]) < toupper(Page[Diff + idx])) {
                    break;
                }
            } else {
                if (term_in[Diff] < Page[Diff + idx]) {
                    break;
                }
            }
        }
        
        // Skip remainder of term, and any nulls after it.
        
        while((idx < Page.size()) && (Page[idx])) {
            idx++;
        }
        while((idx < Page.size()) && (! Page[idx])) {
            idx++;
        }
        
        TermNum++;
    }
    
    
    // If we didn't find a match in the page, then we failed, unless
    // the items that begins the next page is a match (only possible
    // if ISAM_SHORT_KEY or ISAM_FOLLOW_KEY was specified).
    
    EErrorCode rv(eNotFound);
    
    if (idx >= num_bytes) {
        if (found_short >= 0) {
            const char * p = short_term.data();
            
            x_ExtractData(p, p + short_term.size(), term_out, value_out);
            
            index_out = m_PageSize * found_short;
            
            rv = eNoError;
        } else {
            index_out = (Uint4) -1;
            
            rv = eNotFound;
        }
    } else {
        // Otherwise, we found a match.
        
        x_ExtractData(Page.data() + idx,
                      Page.data() + Page.size(),
                      term_out,
                      value_out);
        
        index_out = (m_PageSize * SampleNum) + TermNum;
        
        rv = eNoError;
    }
    
    return rv;
}


CSeqDBIsam::CSeqDBIsam(CSeqDBAtlas  & atlas,
                       const string & dbname,
                       char           prot_nucl,
                       char           file_ext_char,
                       EIdentType     ident_type)
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
    // These are the types that readdb.c seems to use.
    
    switch(ident_type) {
    case eGi:
    case ePig:
        m_Type = eNumeric;
        break;
        
    case eStringID:
        m_Type = eString;
        break;
        
    default:
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Error: ident type argument not valid");
    }
    
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

static size_t
s_SeqDB_EndOfFastaID(const string & str, size_t pos)
{
    // (Derived from s_EndOfFastaID()).
    
    size_t vbar = str.find('|', pos);
    
    if (vbar == string::npos) {
        return string::npos; // bad
    }
    
    CSeq_id::E_Choice choice =
        CSeq_id::WhichInverseSeqId(str.substr(pos, vbar - pos).c_str());
    
    if (choice != CSeq_id::e_not_set) {
        size_t vbar_prev = vbar;
        int count;
        for (count=0; ; ++count, vbar_prev = vbar) {
            vbar = str.find('|', vbar_prev + 1);
            
            if (vbar == string::npos) {
                break;
            }
            
            int start_pt = vbar_prev + 1;
            string element(str, start_pt, vbar - start_pt);
            
            choice = CSeq_id::WhichInverseSeqId(element.c_str());
            
            if (choice != CSeq_id::e_not_set) {
                vbar = vbar_prev;
                break;
            }
        }
    } else {
        return string::npos; // bad
    }
    
    return (vbar == string::npos) ? str.size() : vbar;
}

static bool
s_SeqDB_ParseSeqIDs(const string              & line,
                    vector< CRef< CSeq_id > > & seqids)
{
    // (Derived from s_ParseFastaDefline()).
    
    // try to parse out IDs
    
    seqids.clear();
    size_t pos = 0;
    
    while (pos < line.size()) {
        size_t end = s_SeqDB_EndOfFastaID(line, pos);
        
        if (end == string::npos) {
            // We didn't get a clean parse -- ignore the data after
            // this point, and return what we have.
            
            return ! seqids.empty();
        }
        
        string element(line, pos, end - pos);
        CRef<CSeq_id> id(new CSeq_id(element));
        seqids.push_back(id);
        pos = end + 1;
    }
    
    return ! seqids.empty();
}

bool CSeqDBIsam::StringToOid(const string   & acc,
                             Uint4          & oid,
                             CSeqDBLockHold & locked)
{
    _ASSERT(m_IdentType == eStringID);
    
    m_Atlas.Lock(locked);
    
    if(m_Initialized == false) {
        if (eNoError != x_InitSearch(locked)) {
            return false;
        }
    }
    
    if (m_IdxOption) {
        
        // Under what circumstance can this path be taken?
        
        return x_SparseStringToOid(acc, oid, locked);
    } else {
        string accession(string("gb|") + acc + "|");
        string locus_str(string("gb||") + acc);
        
        EErrorCode err = eNoError;
        
        string keyout, data;
        Uint4 index(0);
        
        if ((err = x_StringSearch(accession, false, false, keyout, data, index, locked)) < 0) {
            return false;
        }
        
        if (err != eNotFound) {
            //oids.push_back(atol(data.c_str()));
            oid = atol(data.c_str());
            return true;
        }
        
        if ((err = x_StringSearch(locus_str, false, false, keyout, data, index, locked)) < 0) {
            return false;
        }
        
        if (err != eNotFound) {
            oid = atol(data.c_str());
            return true;
        }
        
        if ((err = x_StringSearch(acc, false, false, keyout, data, index, locked)) < 0) {
            return false;
        }
        
        if (err != eNotFound) {
            oid = atol(data.c_str());
            return true;
        }
    }
    
    return false;
}

bool CSeqDBIsam::x_SparseStringToOid(const string   & acc,
                                     Uint4          & oids,
                                     CSeqDBLockHold & locked)
{
    cerr << " this should be derived from readdb_acc2fastaEx().." << endl;
    assert(0);
    return false;
}

CSeqDBIsam::EIdentType
CSeqDBIsam::x_SimplifySeqID(const string  & acc,
                            CRef<CSeq_id>   bestid,
                            Uint4         & num_id,
                            string        & str_id)
{
    EIdentType result = eStringID;
    
    const CTextseq_id * tsip = 0;
    
    switch(bestid->Which()) {
    case CSeq_id::e_Gi:
        num_id = bestid->GetGi();
        result = eGi;
        break;
        
    case CSeq_id::e_Gibbsq:    /* gibbseq */
        result = eStringID;
        str_id = NStr::UIntToString(bestid->GetGibbsq());
        break;
        
    case CSeq_id::e_General:
        {
            const CDbtag & dbt = bestid->GetGeneral();
            
            if (dbt.CanGetDb()) {
                if (dbt.GetDb() == "BL_ORD_ID") {
                    num_id = dbt.GetTag().GetId();
                    result = eOID;
                    break;
                }
                
                if (dbt.GetDb() == "PIG") {
                    num_id = dbt.GetTag().GetId();
                    result = ePig;
                    break;
                }
            }
            
            result = eStringID;
            str_id = dbt.GetTag().GetStr();
        }
        break;
        
    case CSeq_id::e_Local:     /* local */
        result = eStringID;
        str_id = bestid->GetLocal().GetStr();
        break;
        
        
        // tsip types
        
    case CSeq_id::e_Embl:      /* embl */
        tsip = & (bestid->GetEmbl());
        break;
        
    case CSeq_id::e_Ddbj:      /* ddbj */
        tsip = & (bestid->GetDdbj());
        break;
        
    case CSeq_id::e_Genbank:   /* genbank */
        tsip = & (bestid->GetGenbank());
        break;
        
    case CSeq_id::e_Tpg:       /* Third Party Annot/Seq Genbank */
        tsip = & (bestid->GetTpg());
        break;
        
    case CSeq_id::e_Tpe:       /* Third Party Annot/Seq EMBL */
        tsip = & (bestid->GetTpe());
        break;
        
    case CSeq_id::e_Tpd:       /* Third Party Annot/Seq DDBJ */
        tsip = & (bestid->GetTpd());
        break;
        
    case CSeq_id::e_Other:     /* other */
        tsip = & (bestid->GetOther());
        break;
        
    case CSeq_id::e_Pir:       /* pir   */
        tsip = & (bestid->GetPir());
        break;
        
    case CSeq_id::e_Swissprot: /* swissprot */
        tsip = & (bestid->GetSwissprot());
        break;
        
    case CSeq_id::e_Prf:       /* prf   */
        tsip = & (bestid->GetPrf());
        break;
        
        
        // Default: do nothing to string; maybe it will work as is.
        
    default:
        result = eStringID;
        str_id = acc;
        break;
    }
    
    if (tsip) {
        result = eStringID;
        
        if (tsip->CanGetAccession()) {
            str_id = tsip->GetAccession();
        } else if (tsip->CanGetName()) {
            str_id = tsip->GetName();
        } else {
            str_id.clear();
        }
    }
    
    return result;
}

CSeqDBIsam::EIdentType
CSeqDBIsam::TryToSimplifyAccession(const string & acc,
                                   Uint4        & num_id,
                                   string       & str_id)
{
    EIdentType result = eStringID;
    num_id = (Uint4)-1;
    
    vector< CRef< CSeq_id > > seqid_set;
    s_SeqDB_ParseSeqIDs(acc, seqid_set);
    
    if (seqid_set.size()) {
        // Something like SeqIdFindBest()
        CRef<CSeq_id> bestid =
            FindBestChoice(seqid_set, CSeq_id::BestRank);
        
        result = x_SimplifySeqID(acc, bestid, num_id, str_id);
    } else {
        str_id = acc;
        result = eStringID;
    }
    
    return result;
}

END_NCBI_SCOPE


