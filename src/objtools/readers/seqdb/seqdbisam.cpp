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

/// Place these definitions in the ncbi namespace
BEGIN_NCBI_SCOPE

/// Import this namespace
USING_SCOPE(objects);

/// Format version of the ISAM files
#define ISAM_VERSION 1

/// Default page size for numeric indices
#define DEFAULT_NISAM_SIZE 256

/// Default page size for string indices
#define DEFAULT_SISAM_SIZE 64

/// Special page size value which indicates a memory-only string index
#define MEMORY_ONLY_PAGE_SIZE 1


CSeqDBIsam::EErrorCode
CSeqDBIsam::x_InitSearch(CSeqDBLockHold & locked)
{
    if(m_Initialized == true)
        return eNoError;
    
    TIndx info_needed = 10 * sizeof(Int4);
    
    m_Atlas.Lock(locked);
    
    bool found_index_file =
        m_Atlas.GetFileSize(m_IndexFname, m_IndexFileLength, locked);
    
    if ((! found_index_file) || (m_IndexFileLength < info_needed)) {
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
        
        TIndx disk_file_length(0);
        bool found_data_file =
            m_Atlas.GetFileSize(m_DataFname, disk_file_length, locked);
        
        if ((! found_data_file) || (m_DataFileLength != disk_file_length)) {
            return eWrongFile;
        }
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
CSeqDBIsam::x_SearchIndexNumeric(int              Number, 
                                 int            * Data,
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
    
    int obj_size = int(NoData ? sizeof(Int4) : sizeof(SNumericKeyData));
    
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
CSeqDBIsam::x_SearchDataNumeric(int              Number,
                                int            * Data,
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
CSeqDBIsam::x_NumericSearch(int              Number, 
                            int            * Data,
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

int CSeqDBIsam::x_DiffCharLease(const string   & term_in,
                                CSeqDBMemLease & lease,
                                const string   & file_name,
                                TIndx            file_length,
                                Uint4            at_least,
                                TIndx            KeyOffset,
                                bool             ignore_case,
                                CSeqDBLockHold & locked)
{
    int result(-1);
    
    m_Atlas.Lock(locked);
    
    // Add one to term_end to insure we don't consider "AA" and "AAB"
    // as equal.
    
    TIndx offset_begin = KeyOffset;
    TIndx term_end     = KeyOffset + term_in.size() + 1;
    TIndx map_end      = term_end + at_least;
    
    if (map_end > file_length) {
        map_end = file_length;
        
        if (term_end > map_end) {
            term_end = map_end;
            result = int(file_length - offset_begin);
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

/// Return NUL for nulls or EOL characters
///
/// This function returns a NUL byte for any of NUL, CR, or NL.  This
/// is done because these characters are used to terminate the
/// variable length records in a string-based ISAM file.
///
/// @param c
///   A character
/// @return
///   NUL or the same character
static inline char
s_SeqDBIsam_NullifyEOLs(char c)
{
    if ((c == '\n') || (c == '\r')) {
        return 0;
    } else {
        return c;
    }
}

/// The terminating character for string ISAM keys when data is present.
const char ISAM_DATA_CHAR = (char) 2;

/// Returns true if the character is a terminator for an ISAM key.
static inline bool ENDS_ISAM_KEY(char P)
{
    return (P == ISAM_DATA_CHAR) || (s_SeqDBIsam_NullifyEOLs(P) == 0);
}

Int4 CSeqDBIsam::x_DiffChar(const string & term_in,
                            const char   * begin,
                            const char   * end,
                            bool           ignore_case)
{
    int result(-1);
    int i(0);
    
    const char * file_data = begin;
    int bytes = int(end - begin);
    
    for(i = 0; (i < bytes) && i < term_in.size(); i++) {
        char ch1 = term_in[i];
        char ch2 = file_data[i];
        
        if (ch1 != ch2) {
            ch1 = s_SeqDBIsam_NullifyEOLs(ch1);
            ch2 = s_SeqDBIsam_NullifyEOLs(ch2);
            
            if (ignore_case) {
                ch1 = toupper(ch1);
                ch2 = toupper(ch2);
            }
            
            if (ch1 != ch2) {
                break;
            }
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

void CSeqDBIsam::x_ExtractPageData(const string   & term_in,
                                   TIndx            page_index,
                                   const char     * beginp,
                                   const char     * endp,
                                   vector<TIndx>  & indices_out,
                                   vector<string> & keys_out,
                                   vector<string> & data_out)
{
    // Collect all 'good' data from the page.
    
    bool ignore_case = true;
    
    Uint4 TermNum(0);
    
    const char * indexp(beginp);
    bool found_match(false);
    
    while (indexp < endp) {
        Int4 Diff = x_DiffChar(term_in,
                               indexp,
                               endp,
                               ignore_case);
        
        if (Diff == -1) { // Complete match
            found_match = true;
            
            x_ExtractData(indexp,
                          endp,
                          keys_out,
                          data_out);
            
            indices_out.push_back(page_index + TermNum);
        } else {
            // If we found a match, but the current term doesn't
            // match, then we are past the set of matching entries.
            
            if (found_match) {
                break;
            }
        }
        
        // Skip remainder of term, and any nulls after it.
        
        while((indexp < endp) && s_SeqDBIsam_NullifyEOLs(*indexp)) {
            indexp++;
        }
        while((indexp < endp) && (! s_SeqDBIsam_NullifyEOLs(*indexp))) {
            indexp++;
        }
        
        TermNum++;
    }
}

void CSeqDBIsam::x_ExtractAllData(const string   & term_in,
                                  TIndx            sample_index,
                                  vector<TIndx>  & indices_out,
                                  vector<string> & keys_out,
                                  vector<string> & data_out,
                                  CSeqDBLockHold & locked)
{
    // The object at sample_index is known to match; we will iterate
    // over the surrounding values to see if they match as well.  No
    // assumptions about how many keys can match are made here.
    
    bool ignore_case = true;
    
    int pre_amt  = 1;
    int post_amt = 1;
    
    bool done_b(false), done_e(false);
    
    const char * beginp(0);
    const char * endp(0);
    
    TIndx beg_off(0);
    TIndx end_off(0);
    
    while(! (done_b && done_e)) {
        if (sample_index < pre_amt) {
            beg_off = 0;
            done_b = true;
        } else {
            beg_off = sample_index - pre_amt;
        }
        
        if ((m_NumSamples - sample_index) < post_amt) {
            end_off = m_NumSamples;
            done_e = true;
        } else {
            end_off = sample_index + post_amt;
        }
        
        x_LoadPage(beg_off, end_off, & beginp, & endp, locked);
        
        if (! done_b) {
            Int4 diff_begin = x_DiffChar(term_in,
                                         beginp,
                                         endp,
                                         ignore_case);
            
            if (diff_begin != -1) {
                done_b = true;
            } else {
                pre_amt ++;
            }
        }
        
        if (! done_e) {
            const char * last_term(0);
            const char * p(endp-1);
            
            // Skip over any non-terminating junk at the end
            
            enum { eEndNulls, eLastTerm } search_stage = eEndNulls;
            
            while(p > beginp) {
                bool terminal = (0 == s_SeqDBIsam_NullifyEOLs(*p));
                
                if (search_stage == eEndNulls) {
                    if (! terminal) { 
                        search_stage = eLastTerm;
                    }
                } else {
                    if (terminal) {
                        last_term = p + 1;
                        break;
                    }
                }
                
                p--;
            }
            
            if (! last_term) {
                last_term = beginp;
            }
            
            Int4 diff_end = x_DiffChar(term_in,
                                       last_term,
                                       endp,
                                       ignore_case);
            
            if (diff_end != -1) {
                done_e = true;
            } else {
                post_amt ++;
            }
        }
    }
    
    x_ExtractPageData(term_in,
                      m_PageSize * beg_off,
                      beginp,
                      endp,
                      indices_out,
                      keys_out,
                      data_out);
}

void CSeqDBIsam::x_ExtractData(const char     * key_start,
                               const char     * map_end,
                               vector<string> & keys_out,
                               vector<string> & data_out)
{
    const char * data_ptr(0);
    const char * p(key_start);
    
    while(p < map_end) {
        switch(s_SeqDBIsam_NullifyEOLs(*p)) {
        case 0:
            if (data_ptr) {
                keys_out.push_back(string(key_start, data_ptr));
                data_out.push_back(string(data_ptr+1, p));
            } else {
                keys_out.push_back(string(key_start, p));
                data_out.push_back("");
            }
            return;
            
        case ISAM_DATA_CHAR:
            data_ptr = p;
            
        default:
            p++;
        }
    }
}

CSeqDBIsam::TIndx
CSeqDBIsam::x_GetIndexKeyOffset(TIndx            sample_offset,
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
CSeqDBIsam::x_GetIndexString(TIndx            key_offset,
                             int              length,
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
    
    const char * key_offset_addr =
        (const char *) m_IndexLease.GetPtr(key_offset);
    
    if (trim_to_null) {
        for(int i = 0; i<length; i++) {
            if (! key_offset_addr[i]) {
                length = i;
                break;
            }
        }
    }
    
    str.assign(key_offset_addr, length);
}

// Given an index, this computes the diff from the input term.  It
// also returns the offset for that sample's key in KeyOffset.

int CSeqDBIsam::x_DiffSample(const string   & term_in,
                             Uint4            SampleNum,
                             TIndx          & KeyOffset,
                             CSeqDBLockHold & locked)
{
    // Meaning:
    // a. Compute SampleNum*4
    // b. Address this number into SamplePos (indexlease)
    // c. Swap this number to compute Key offset.
    // d. Add to beginning of file to get key data pointer.
    
    bool ignore_case(true);
    
    TIndx SampleOffset(m_KeySampleOffset);
    
    if(m_PageSize != MEMORY_ONLY_PAGE_SIZE) {
        SampleOffset += (m_NumSamples + 1) * sizeof(Uint4);
    }
    
    TIndx offset_begin = SampleOffset + (SampleNum * sizeof(Uint4));
    TIndx offset_end   = offset_begin + sizeof(Uint4);
    
    m_Atlas.Lock(locked);
    
    if (! m_IndexLease.Contains(offset_begin, offset_end)) {
        m_Atlas.GetRegion(m_IndexLease,
                          m_IndexFname,
                          offset_begin,
                          offset_end);
    }
    
    KeyOffset = SeqDB_GetStdOrd((Int4*) m_IndexLease.GetPtr(offset_begin));
    
    Uint4 max_lines_2 = m_MaxLineSize * 2;
    
    return x_DiffCharLease(term_in,
                           m_IndexLease,
                           m_IndexFname,
                           m_IndexFileLength,
                           max_lines_2,
                           KeyOffset,
                           ignore_case,
                           locked);
}

void CSeqDBIsam::x_LoadPage(TIndx             SampleNum1,
                            TIndx             SampleNum2,
                            const char     ** beginp,
                            const char     ** endp,
                            CSeqDBLockHold &  locked)
{
    // Load the appropriate page of terms into memory.
    
    _ASSERT(SampleNum2 > SampleNum1);
    
    TIndx begin_offset = m_KeySampleOffset + SampleNum1       * sizeof(Uint4);
    TIndx end_offset   = m_KeySampleOffset + (SampleNum2 + 1) * sizeof(Uint4);
    
    m_Atlas.Lock(locked);
    
    if (! m_IndexLease.Contains(begin_offset, end_offset)) {
        m_Atlas.GetRegion(m_IndexLease, m_IndexFname, begin_offset, end_offset);
    }
    
    Uint4 * key_offsets((Uint4*) m_IndexLease.GetPtr(begin_offset));
    
    Uint4 key_off1 = SeqDB_GetStdOrd(& key_offsets[0]);
    Uint4 key_off2 = SeqDB_GetStdOrd(& key_offsets[SampleNum2 - SampleNum1]);
    
    if (! m_DataLease.Contains(key_off1, key_off2)) {
        m_Atlas.GetRegion(m_DataLease, m_DataFname, key_off1, key_off2);
    }
    
    *beginp = (const char *) m_DataLease.GetPtr(key_off1);
    *endp   = (const char *) m_DataLease.GetPtr(key_off2);
}


// ------------------------StringSearch--------------------------
// Purpose:     Main search function of string search.
// 
// Parameters:  Key - interer to search
//              Data - returned value
//              Index - internal index in database
// Returns:     ISAM Error Code
// NOTE:        None
// --------------------------------------------------------------

CSeqDBIsam::EErrorCode
CSeqDBIsam::x_StringSearch(const string   & term_in,
                           //bool             follow_match,
                           //bool             short_match,
                           vector<string> & terms_out,
                           vector<string> & values_out,
                           vector<TIndx>  & indices_out,
                           CSeqDBLockHold & locked)
{
    // These are always false; They may relate to the prior find_one /
    // expand_to_many method of getting multiple OIDs.
    
    bool short_match(false);
    bool follow_match(false);
    
    size_t preexisting_data_count = values_out.size();
    
    if (m_Initialized == false) {
        EErrorCode error = x_InitSearch(locked);
        
        if(error != eNoError) {
            return error;
        }
    }
    
    // We will set this option to avoid more complications
    bool ignore_case = true;
    
    // search the sample file first
    
    TIndx Start(0);
    TIndx Stop(m_NumSamples - 1);
    
    int Length = (int) term_in.size();
    
    TIndx SampleOffset(m_KeySampleOffset);
    
    if(m_PageSize != MEMORY_ONLY_PAGE_SIZE) {
        SampleOffset += (m_NumSamples + 1) * sizeof(Uint4);
    }
    
    int found_short(-1);
    
    string short_term;
    int SampleNum(-1);
    
    while(Stop >= Start) {
        SampleNum = ((Uint4)(Stop + Start)) >> 1;
        
        TIndx KeyOffset(0);
        
        int diff = x_DiffSample(term_in, SampleNum, KeyOffset, locked);
        
        // If this is an exact match, return the master term number.
        
        const char * KeyData = m_IndexLease.GetPtr(KeyOffset);
        TIndx BytesToEnd = m_IndexFileLength - KeyOffset;
        
        Uint4 max_lines_2 = m_MaxLineSize * 2;
        
        if (BytesToEnd > max_lines_2) {
            BytesToEnd = max_lines_2;
        }
        
        if (diff == -1) {
            //if (collect_all) {
            
            x_ExtractAllData(term_in,
                             SampleNum,
                             indices_out,
                             terms_out,
                             values_out,
                             locked);

//           } else {
//               x_ExtractData(KeyData,
//                             KeyData + BytesToEnd,
//                             terms_out,
//                             values_out);
//              
//               indices_out.push_back(m_PageSize * SampleNum);
//           }
            
            return eNoError;
        }
        
        // If the key is a superset of the sample term, backup until
        // just before the term.
        
        if (short_match && (diff >= Length)) {
            if (SampleNum > 0)
                SampleNum--;
            
            while(SampleNum > 0) {
                TIndx key_offset =
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
            
            TIndx key_offset =
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
    
    const char * beginp(0);
    const char * endp(0);
    
    x_LoadPage(SampleNum, SampleNum + 1, & beginp, & endp, locked);
    
    // Search the page for the term.
    
    x_ExtractPageData(term_in,
                      m_PageSize * SampleNum,
                      beginp,
                      endp,
                      indices_out,
                      terms_out,
                      values_out);
    
    // For now the short and follow logic is not implemented.
    
    EErrorCode rv(eNoError);
    
    if (preexisting_data_count == values_out.size()) {
        rv = eNotFound;
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
    
    if(m_Type == eNumeric ||
       m_Type == eNumericNoData) {
        
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

bool CSeqDBIsam::x_IdentToOid(int ident, TOid & oid, CSeqDBLockHold & locked)
{
    EErrorCode err =
        x_NumericSearch(ident, & oid, 0, locked);
    
    if (err == eNoError) {
        return true;
    }
    
    oid = Uint4(-1);
    
    return false;
}

/// Find the end of a single element in a Seq-id set
/// 
/// Seq-id strings sometimes contain several Seq-ids.  This function
/// looks for the end of the first Seq-id, and will return its length.
/// Static methods of CSeq_id are used to evaluate tokens.
/// 
/// @param str
///   Seq-id string to search.
/// @param pos
///   Position at which to start search.
/// @return
///   End position of first fasta id, or string::npos in case of error.

static size_t
s_SeqDB_EndOfFastaID(const string & str, size_t pos)
{
    // (Derived from s_EndOfFastaID()).
    
    size_t vbar = str.find('|', pos);
    
    if (vbar == string::npos) {
        return string::npos; // bad
    }
    
    string portion(str, pos, vbar - pos);
    
    CSeq_id::E_Choice choice =
        CSeq_id::WhichInverseSeqId(portion.c_str());
    
    if (choice != CSeq_id::e_not_set) {
        size_t vbar_prev = vbar;
        int count;
        for (count=0; ; ++count, vbar_prev = vbar) {
            vbar = str.find('|', vbar_prev + 1);
            
            if (vbar == string::npos) {
                break;
            }
            
            int start_pt = int(vbar_prev + 1);
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

/// Parse string into a sequence of Seq-id objects.
///
/// A string is broken down into Seq-ids and the set of Seq-ids is
/// returned.
///
/// @param line
///   The string to interpret.
/// @param seqids
///   The returned set of Seq-id objects.
/// @return
///   true if any Seq-id objects were found.

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
            break;
        }
        
        string element(line, pos, end - pos);
        
        CRef<CSeq_id> id;
        
        try {
            id = new CSeq_id(element);
        }
        catch(invalid_argument &) {
            // Maybe this should be done: "seqids.clear();"
            break;
        }
        
        seqids.push_back(id);
        pos = end + 1;
    }
    
    return ! seqids.empty();
}

void CSeqDBIsam::StringToOids(const string   & acc,
                              vector<TOid>   & oids,
                              bool             adjusted,
                              CSeqDBLockHold & locked)
{
    _ASSERT(m_IdentType == eStringID);
    
    m_Atlas.Lock(locked);
    
    if(m_Initialized == false) {
        if (eNoError != x_InitSearch(locked)) {
            return;
        }
    }
    
    if (m_IdxOption) {
        // Under what circumstance can this path be taken?
        
        x_SparseStringToOids(acc, oids, adjusted, locked);
    } else {
        bool found = false;
        
        string accession(string("gb|") + acc + "|");
        string locus_str(string("gb||") + acc);
        
        EErrorCode err = eNoError;
        
        vector<string> keys_out;
        vector<string> data_out;
        vector<TIndx>  indices_out;
        
        if (! adjusted) {
            if ((err = x_StringSearch(accession,
                                      keys_out,
                                      data_out,
                                      indices_out,
                                      locked)) < 0) {
                return;
            }
            
            if (err == eNoError) {
                found = true;
            }
            
            if ((! found) &&
                (err = x_StringSearch(locus_str,
                                      keys_out,
                                      data_out,
                                      indices_out,
                                      locked)) < 0) {
                return;
            }
            
            if (err != eNotFound) {
                found = true;
            }
        }
        
        if ((! found) &&
            (err = x_StringSearch(acc,
                                  keys_out,
                                  data_out,
                                  indices_out,
                                  locked)) < 0) {
            return;
        }
        
        if (err != eNotFound) {
            found = true;
        }
        
        if (found) {
            ITERATE(vector<string>, iter, data_out) {
                oids.push_back(atoi((*iter).c_str()));
            }
        }
    }
}

bool CSeqDBIsam::x_SparseStringToOids(const string   &,
                                      vector<int>    &,
                                      bool,
                                      CSeqDBLockHold &)
{
    cerr << " this should be derived from readdb_acc2fastaEx().." << endl;
    _TROUBLE;
    return false;
}

CSeqDBIsam::EIdentType
CSeqDBIsam::SimplifySeqid(CSeq_id       & bestid,
                          const string  * acc,
                          int           & num_id,
                          string        & str_id,
                          bool          & simpler)
{
    EIdentType result = eStringID;
    
    CTextseq_id * tsip = 0;
    
    bool use_version = false;
    
    bool matched = true;
    
    switch(bestid.Which()) {
    case CSeq_id::e_Gi:
        simpler = true;
        num_id = bestid.GetGi();
        result = eGi;
        break;
        
    case CSeq_id::e_Gibbsq:    /* gibbseq */
        simpler = true;
        result = eStringID;
        str_id = NStr::UIntToString(bestid.GetGibbsq());
        break;
        
    case CSeq_id::e_General:
        {
            const CDbtag & dbt = bestid.GetGeneral();
            
            if (dbt.CanGetDb()) {
                if (dbt.GetDb() == "BL_ORD_ID") {
                    simpler = true;
                    num_id = dbt.GetTag().GetId();
                    result = eOID;
                    break;
                }
                
                if (dbt.GetDb() == "PIG") {
                    simpler = true;
                    num_id = dbt.GetTag().GetId();
                    result = ePig;
                    break;
                }
            }
            
            if (dbt.CanGetTag() && dbt.GetTag().IsStr()) {
                result = eStringID;
                str_id = dbt.GetTag().GetStr();
            } else {
                // Use the default logic.
                matched = false;
            }
        }
        break;
        
    case CSeq_id::e_Local:     /* local */
        simpler = true;
        result = eStringID;
        str_id = bestid.GetLocal().GetStr();
        break;
        
        
        // tsip types
        
    case CSeq_id::e_Embl:      /* embl */
        tsip = & (bestid.SetEmbl());
        use_version = true;
        break;
        
    case CSeq_id::e_Ddbj:      /* ddbj */
        tsip = & (bestid.SetDdbj());
        use_version = true;
        break;
        
    case CSeq_id::e_Genbank:   /* genbank */
        tsip = & (bestid.SetGenbank());
        use_version = true;
        break;
        
    case CSeq_id::e_Tpg:       /* Third Party Annot/Seq Genbank */
        tsip = & (bestid.SetTpg());
        use_version = true;
        break;
        
    case CSeq_id::e_Tpe:       /* Third Party Annot/Seq EMBL */
        tsip = & (bestid.SetTpe());
        use_version = true;
        break;
        
    case CSeq_id::e_Tpd:       /* Third Party Annot/Seq DDBJ */
        tsip = & (bestid.SetTpd());
        use_version = true;
        break;
        
    case CSeq_id::e_Other:     /* other */
        tsip = & (bestid.SetOther());
        use_version = true;
        break;
        
    case CSeq_id::e_Pir:       /* pir   */
        tsip = & (bestid.SetPir());
        break;
        
    case CSeq_id::e_Swissprot: /* swissprot */
        tsip = & (bestid.SetSwissprot());
        break;
        
    case CSeq_id::e_Prf:       /* prf   */
        tsip = & (bestid.SetPrf());
        break;
        
    default:
        matched = false;
    }
    
    // Default: if we have a string, use it; if we only have seqid,
    // create a string.  This should not happen if the seqid matches
    // one of the cases above, which currently correspond to all the
    // supported seqid types.
    
    CSeq_id::ELabelFlags label_flags = (CSeq_id::ELabelFlags)
        (CSeq_id::fLabel_GeneralDbIsContent | CSeq_id::fLabel_Version);
    
    if (! matched) {
        // (should not happen normally)
        
        simpler = false;
        result  = eStringID;
        
        if (acc) {
            str_id = *acc;
        } else {
            bestid.GetLabel(& str_id, CSeq_id::eFasta, label_flags);
        }
    }
    
    if (tsip) {
        simpler = true;
        result = eStringID;
        
        string tmp_name;
        
        if (tsip->CanGetName() && tsip->CanGetAccession()) {
            tmp_name = tsip->GetName();
            tsip->ResetName();
        }
        
        bestid.GetLabel(& str_id, CSeq_id::eFasta, label_flags);
        
        if (! tmp_name.empty()) {
            tsip->SetName(tmp_name);
        }
    }
    
    return result;
}

CSeqDBIsam::EIdentType
CSeqDBIsam::TryToSimplifyAccession(const string & acc,
                                   int          & num_id,
                                   string       & str_id,
                                   bool         & simpler)
{
    EIdentType result = eStringID;
    num_id = (Uint4)-1;
    
    vector< CRef< CSeq_id > > seqid_set;
    
    if (s_SeqDB_ParseSeqIDs(acc, seqid_set)) {
        // Something like SeqIdFindBest()
        CRef<CSeq_id> bestid =
            FindBestChoice(seqid_set, CSeq_id::BestRank);
        
        result = SimplifySeqid(*bestid, & acc, num_id, str_id, simpler);
    } else {
        str_id = acc;
        result = eStringID;
        simpler = false;
    }
    
    return result;
}

END_NCBI_SCOPE


