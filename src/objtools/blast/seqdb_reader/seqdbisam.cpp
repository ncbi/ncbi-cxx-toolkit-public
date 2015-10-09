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

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdbisam.hpp>
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

    if (IsamType == eNumericLongId && m_Type == eNumeric) {
        m_LongId = true;
        m_TermSize = 12;
        IsamType = eNumeric;
    }

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

Int4 CSeqDBIsam::x_GetPageNumElements(Int4   sample_num,
                                      Int4 * start)
{
    Int4 num_elements(0);

    *start = sample_num * m_PageSize;

    if (sample_num + 1 == m_NumSamples) {
        num_elements = m_NumTerms - *start;
    } else {
        num_elements = m_PageSize;
    }

    return num_elements;
}

CSeqDBIsam::EErrorCode
CSeqDBIsam::x_SearchIndexNumeric(Int8             Number,
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

    if (x_OutOfBounds(Number, locked)) {
        done = true;
        return eNotFound;
    }

    _ASSERT(m_Type != eNumericNoData);

    // Search the sample file.

    Int4 Start     (0);
    Int4 Stop      (m_NumSamples - 1);

    while(Stop >= Start) {
        SampleNum = ((Uint4)(Stop + Start)) >> 1;

        TIndx offset_begin = m_KeySampleOffset + (m_TermSize * SampleNum);
        TIndx offset_end   = offset_begin + m_TermSize;

        m_Atlas.Lock(locked);

        if (! m_IndexLease.Contains(offset_begin, offset_end)) {
            m_Atlas.GetRegion(m_IndexLease,
                              m_IndexFname,
                              offset_begin,
                              offset_end);
        }

        const void* keydatap(0);

        Int8 Key(0);

        keydatap = m_IndexLease.GetPtr(offset_begin);
        Key = x_GetNumericKey (keydatap);

        // If this is an exact match, return the master term number.

        if (Key == Number) {
            if (Data != NULL) {
                *Data = x_GetNumericData(keydatap);
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

void
CSeqDBIsam::x_SearchNegativeMulti(int                  vol_start,
                                  int                  vol_end,
                                  CSeqDBNegativeList & ids,
                                  bool                 use_tis,
                                  CSeqDBLockHold     & locked)
{
    m_Atlas.Lock(locked);

    if(m_Initialized == false) {
        EErrorCode error = x_InitSearch(locked);

        if(error != eNoError) {
            // Most ordinary errors (missing IDs for example) are
            // ignored for "multi" mode searches.  But if a GI list is
            // specified, and cannot be interpreted, it is an error.

            NCBI_THROW(CSeqDBException,
                       eArgErr,
                       "Error: Unable to use ISAM index in batch mode.");
        }
    }

    m_Atlas.Lock(locked);

    // We can use Parabolic Binary Search for the negative GI list but
    // not for the ISAM file data, because in the negative ID list
    // case, every line of the ISAM data must be looked at.

    _ASSERT(m_Type != eNumericNoData);

    //......................................................................
    //
    // Translate the entire Gi List.
    //
    //......................................................................

    int gilist_size = use_tis ? ids.GetNumTis() : ids.GetNumGis();

    int gilist_index = 0;

    int sample_index(0);
    const void * data_page (0);

    while(sample_index < m_NumSamples) {
        int start = 0, num_elements = 0;

        x_MapDataPage(sample_index,
                      start,
                      num_elements,
                      & data_page,
                      locked);

        for(int i = 0; i < num_elements; i++) {
            Int8 isam_key(0);
            int isam_data(0);

            // 1. Get the ID+OID from the data page.

            x_GetDataElement(data_page,
                             i,
                             isam_key,
                             isam_data);

            // 2. Look for it in the negative id list.

            bool found = false;

            if (gilist_index < gilist_size) {
                found = x_FindInNegativeList(ids,
                                             gilist_index,
                                             isam_key,
                                             use_tis);
            }

            // 3. If not found, add the OID to the negative ID list.

            if (isam_data < vol_end) {
                if (found) {
                    // OID is found, but may not be included yet.
                    ids.AddVisibleOid(isam_data + vol_start);
                } else {
                    // OID is included for iteration.
                    ids.AddIncludedOid(isam_data + vol_start);
                }
            }
        }

        // Move to next data page.  Note that for a negative ID list
        // processing, we don't actually fetch any samples, because
        // every ID->OID line needs to be examined anyway.

        sample_index ++;
    }
}


CSeqDBIsam::EErrorCode
CSeqDBIsam::x_SearchDataNumeric(Int8             Number,
                                int            * Data,
                                Uint4          * Index,
                                Int4             SampleNum,
                                CSeqDBLockHold & locked)
{
    // Load the appropriate page of numbers into memory.
    _ASSERT(m_Type != eNumericNoData);

    Int4 Start(0);
    Int4 NumElements = x_GetPageNumElements(SampleNum, & Start);

    Int4 first = Start;
    Int4 last  = Start + NumElements - 1;

    const void * KeyDataPage      = NULL;
    const void * KeyDataPageStart = NULL;

    TIndx offset_begin = Start * m_TermSize;
    TIndx offset_end = offset_begin + m_TermSize * NumElements;

    m_Atlas.Lock(locked);

    if (! m_DataLease.Contains(offset_begin, offset_end)) {
        m_Atlas.GetRegion(m_DataLease,
                          m_DataFname,
                          offset_begin,
                          offset_end);
    }

    KeyDataPageStart = m_DataLease.GetPtr(offset_begin);

    KeyDataPage = (char *)KeyDataPageStart - Start * m_TermSize;

    bool found   (false);
    Int4 current (0);

    // Search the page for the number.
    while (first <= last) {
        current = (first+last)/2;

        Int8 Key = x_GetNumericKey((char *)KeyDataPage + current * m_TermSize);

        if (Key > Number) {
            last = --current;
        } else if (Key < Number) {
            first = ++current;
        } else {
            found = true;
            break;
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
        *Data = x_GetNumericData((char *)KeyDataPage + current * m_TermSize);
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
CSeqDBIsam::x_NumericSearch(Int8             Number,
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
    if (SEQDB_ISEOL(c)) {
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

    for(i = 0; (i < bytes) && i < (int) term_in.size(); i++) {
        char ch1 = term_in[i];
        char ch2 = file_data[i];

        if (ch1 != ch2) {
            ch1 = s_SeqDBIsam_NullifyEOLs(ch1);
            ch2 = s_SeqDBIsam_NullifyEOLs(ch2);

            if (ignore_case) {
                ch1 = toupper((unsigned char) ch1);
                ch2 = toupper((unsigned char) ch2);
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

    if (((p == end) || ENDS_ISAM_KEY(*p)) && (i == (int) term_in.size())) {
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

    if (x_OutOfBounds(term_in, locked)) {
        return eNotFound;
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

        if (BytesToEnd > (TIndx) max_lines_2) {
            BytesToEnd = max_lines_2;
        }

        if (diff == -1) {
            x_ExtractAllData(term_in,
                             SampleNum,
                             indices_out,
                             terms_out,
                             values_out,
                             locked);

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
            ? tolower((unsigned char) term_in[diff]) < tolower((unsigned char) KeyData[diff])
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
                       ESeqDBIdType   ident_type)
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
      m_LastOffset     (0),
      m_LongId         (false),
      m_TermSize       (8)
{
    // These are the types that readdb.c seems to use.

    switch(ident_type) {
    case eGiId:
    case ePigId:
    case eTiId:
        m_Type = eNumeric;
        break;

    case eStringId:
    case eHashId:
        m_Type = eString;
        break;

    default:
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Error: ident type argument not valid");
    }

    x_MakeFilenames(dbname,
                    prot_nucl,
                    file_ext_char,
                    m_IndexFname,
                    m_DataFname);

    if (! (CFile(m_IndexFname).Exists() &&
           CFile(m_DataFname).Exists()) ) {

        string msg("Error: Could not open input file (");
        msg += m_IndexFname + "/" + m_DataFname + ")";
        NCBI_THROW(CSeqDBException, eFileErr, msg);
    }

    if(m_Type == eNumeric) {
        m_PageSize = DEFAULT_NISAM_SIZE;
    } else {
        m_PageSize = DEFAULT_SISAM_SIZE;
    }
}

void CSeqDBIsam::x_MakeFilenames(const string & dbname,
                                 char           prot_nucl,
                                 char           file_ext_char,
                                 string       & index_name,
                                 string       & data_name)
{
    if (dbname.empty() ||
        (! isalpha((unsigned char) prot_nucl)) ||
        (! isalpha((unsigned char) file_ext_char))) {

        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Error: argument not valid");
    }

    index_name.reserve(dbname.size() + 4);
    data_name.reserve(dbname.size() + 4);

    index_name = dbname;
    index_name += '.';
    index_name += prot_nucl;
    index_name += file_ext_char;

    data_name = index_name;
    index_name += 'i';
    data_name  += 'd';
}

bool CSeqDBIsam::IndexExists(const string & dbname,
                             char           prot_nucl,
                             char           file_ext_char)
{
    string iname, dname;
    x_MakeFilenames(dbname, prot_nucl, file_ext_char, iname, dname);

    return CFile(iname).Exists() && CFile(dname).Exists();
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

bool CSeqDBIsam::x_IdentToOid(Int8 ident, TOid & oid, CSeqDBLockHold & locked)
{
    EErrorCode err =
        x_NumericSearch(ident, & oid, 0, locked);

    if (err == eNoError) {
        return true;
    }

    oid = -1u;  /* NCBI_FAKE_WARNING */

    return false;
}

void CSeqDBIsam::StringToOids(const string   & acc,
                              vector<TOid>   & oids,
                              bool             adjusted,
                              bool           & version_check,
                              CSeqDBLockHold & locked)
{
    bool strip_version = version_check;
    version_check = false;

    _ASSERT(m_IdentType == eStringId);

    m_Atlas.Lock(locked);

    if(m_Initialized == false) {
        if (eNoError != x_InitSearch(locked)) {
            return;
        }
    }

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

    if ((! found) && strip_version) {
        size_t pos = acc.find(".");

        bool is_version = false;

        if (pos != string::npos) {
            int ver_len = acc.size() - pos - 1;

            is_version = (ver_len <= 3 && ver_len >= 1);

            for(size_t vp = pos+1; vp < acc.size(); vp++) {
                if (! isdigit(acc[vp])) {
                    is_version = false;
                    break;
                }
            }
        }

        if (is_version) {
            string nover(acc, 0, pos);

            err = x_StringSearch(nover,
                                 keys_out,
                                 data_out,
                                 indices_out,
                                 locked);

            if (data_out.size()) {
                version_check = true;
            }

            if (err < 0) {
                return;
            }
        }
    }

    if (err != eNotFound) {
        found = true;
    }

    if (! found) {
        // Use CSeq_id to parse the id string and build a replacement,
        // FASTA type string.  This allows some IDs, such as PDBs with
        // chains, such as '1qcfA' to be parsed.

        string id;

        try {
            CSeq_id seqid(acc, CSeq_id::fParse_RawText | CSeq_id::fParse_AnyLocal);
            id = seqid.AsFastaString();
        }
        catch(CSeqIdException &) {
        }

        if (id.size() &&
            ((err = x_StringSearch(id,
                                   keys_out,
                                   data_out,
                                   indices_out,
                                   locked)) < 0)) {
            return;
        }
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

bool CSeqDBIsam::x_SparseStringToOids(const string   &,
                                      vector<int>    &,
                                      bool,
                                      CSeqDBLockHold &)
{
    cerr << " this should be derived from readdb_acc2fastaEx().." << endl;
    _TROUBLE;
    return false;
}

void CSeqDBIsam::IdsToOids(int              vol_start,
                           int              vol_end,
                           CSeqDBGiList   & ids,
                           CSeqDBLockHold & locked)
{
    // The vol_start parameter is needed because translations in the
    // GI list should refer to global OIDs, not per-volume OIDs.

    switch (m_IdentType) {
    case eGiId:
        x_TranslateGiList<TGi>(vol_start, ids, locked);
        break;

    case eTiId:
        x_TranslateGiList<Int8>(vol_start, ids, locked);
        break;

    case eStringId:
        x_TranslateGiList<string>(vol_start, ids, locked);
        break;

    default:
        NCBI_THROW(CSeqDBException,
                       eArgErr,
                       "Error: Wrong type of idlist specified.");
    }
}

void CSeqDBIsam::IdsToOids(int                  vol_start,
                           int                  vol_end,
                           CSeqDBNegativeList & ids,
                           CSeqDBLockHold     & locked)
{
    // The vol_start parameter is needed because translations in the
    // GI list should refer to global OIDs, not per-volume OIDs.

    _ASSERT(m_IdentType == eGiId || m_IdentType == eTiId);

    m_Atlas.Lock(locked);
    ids.InsureOrder();

    if ((m_IdentType == eGiId) && ids.GetNumGis()) {
        x_SearchNegativeMulti(vol_start,
                              vol_end,
                              ids,
                              false,
                              locked);
    }

    if ((m_IdentType == eTiId) && ids.GetNumTis()) {
        x_SearchNegativeMulti(vol_start,
                              vol_end,
                              ids,
                              true,
                              locked);
    }
}

void CSeqDBIsam::x_FindIndexBounds(CSeqDBLockHold & locked)
{
    Int4 Start (0);
    Int4 Stop  (m_NumSamples - 1);

    m_Atlas.Lock(locked);

    if (m_Type == eNumeric) {
        //
        // Get first key from data file

        int num_elements(0);
        int start(0);
        const void * data_page(0);

        x_MapDataPage(Start,
                      start,
                      num_elements,
                      & data_page,
                      locked);

        _ASSERT(num_elements);

        int elem_index = 0;

        Int8 data_gi(0);
        int data_oid(-1);

        x_GetDataElement(data_page,
                         elem_index,
                         data_gi,
                         data_oid);

        m_FirstKey.SetNumeric(data_gi);


        //
        // Get last key from data file

        x_MapDataPage(Stop,
                      start,
                      num_elements,
                      & data_page,
                      locked);

        _ASSERT(num_elements);

        elem_index = num_elements - 1;

        x_GetDataElement(data_page,
                         elem_index,
                         data_gi,
                         data_oid);

        m_LastKey.SetNumeric(data_gi);
    } else {
        //
        // Load the appropriate page of terms into memory.

        const char * beginp(0);
        const char * endp(0);

        //
        // Load the first page

        x_LoadPage(Start, Start + 1, & beginp, & endp, locked);

        // Get first term

        vector<string> keys_out;
        vector<string> data_out; // not used

        x_ExtractData(beginp,
                      endp,
                      keys_out,
                      data_out);

        x_Lower(keys_out.front());
        m_FirstKey.SetString(keys_out.front());


        //
        // Load the last page

        x_LoadPage(Stop, Stop + 1, & beginp, & endp, locked);

        // Advance to last item

        const char * lastp(0);
        const char * indexp(beginp);

        while (indexp < endp) {
            // Remember our new "last term" value.

            lastp = indexp;

            // Skip remainder of term, and any nulls after it.

            while((indexp < endp) && s_SeqDBIsam_NullifyEOLs(*indexp)) {
                indexp++;
            }
            while((indexp < endp) && (! s_SeqDBIsam_NullifyEOLs(*indexp))) {
                indexp++;
            }
        }

        // Get the last key

        _ASSERT(lastp);

        keys_out.clear();
        data_out.clear();

        x_ExtractData(lastp,
                      endp,
                      keys_out,
                      data_out);

        x_Lower(keys_out.front());
        m_LastKey.SetString(keys_out.front());
    }
}

bool CSeqDBIsam::x_OutOfBounds(Int8 key, CSeqDBLockHold & locked)
{
    if (! m_FirstKey.IsSet()) {
        x_FindIndexBounds(locked);
    }

    if (! (m_FirstKey.IsSet() && m_LastKey.IsSet())) {
        return false;
    }

    _ASSERT(m_Type == eNumeric);

    if (m_FirstKey.OutsideFirstBound(key)) {
        return true;
    }

    if (m_LastKey.OutsideLastBound(key)) {
        return true;
    }

    return false;
}

bool CSeqDBIsam::x_OutOfBounds(string key, CSeqDBLockHold & locked)
{
    if (! m_FirstKey.IsSet()) {
        x_FindIndexBounds(locked);
    }

    if (! (m_FirstKey.IsSet() && m_LastKey.IsSet())) {
        return false;
    }

    _ASSERT(m_Type == eString);

    x_Lower(key);

    if (m_FirstKey.OutsideFirstBound(key)) {
        return true;
    }

    if (m_LastKey.OutsideLastBound(key)) {
        return true;
    }

    return false;
}

void CSeqDBIsam::GetIdBounds(Int8           & low_id,
                             Int8           & high_id,
                             int            & count,
                             CSeqDBLockHold & locked)
{
    m_Atlas.Lock(locked);

    if(m_Initialized == false) {
        EErrorCode error = x_InitSearch(locked);

        if(error != eNoError) {
            count = 0;
            return;
        }
    }

    if (! (m_FirstKey.IsSet() && m_LastKey.IsSet())) {
        x_FindIndexBounds(locked);
    }

    low_id = m_FirstKey.GetNumeric();
    high_id = m_LastKey.GetNumeric();
    count = m_NumTerms;
}

void CSeqDBIsam::GetIdBounds(string         & low_id,
                             string         & high_id,
                             int            & count,
                             CSeqDBLockHold & locked)
{
    m_Atlas.Lock(locked);

    if(m_Initialized == false) {
        EErrorCode error = x_InitSearch(locked);

        if(error != eNoError) {
            count = 0;
            return;
        }
    }

    if (! (m_FirstKey.IsSet() && m_LastKey.IsSet())) {
        x_FindIndexBounds(locked);
    }

    low_id = m_FirstKey.GetString();
    high_id = m_LastKey.GetString();
    count = m_NumTerms;
}

void CSeqDBIsam::HashToOids(unsigned         hash,
                            vector<TOid>   & oids,
                            CSeqDBLockHold & locked)
{
    _ASSERT(m_IdentType == eHashId);

    m_Atlas.Lock(locked);

    if(m_Initialized == false) {
        if (eNoError != x_InitSearch(locked)) {
            return;
        }
    }

    bool found = false;

    string key(NStr::UIntToString(hash));

    EErrorCode err = eNoError;

    vector<string> keys_out;
    vector<string> data_out;
    vector<TIndx>  indices_out;

    if ((err = x_StringSearch(key,
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
            oids.push_back(atoi(iter->c_str()));
        }
    }
}

END_NCBI_SCOPE

