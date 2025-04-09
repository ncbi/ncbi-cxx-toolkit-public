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

/// @file seqdbvol.cpp
/// Implementation for the CSeqDBVol class, which provides an
/// interface for all functionality of one database volume.
#include <ncbi_pch.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdbvol.hpp>
#include "seqdboidlist.hpp"

#include <objects/general/general__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>

#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/serial.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_safe_static.hpp>

#include <sstream>

BEGIN_NCBI_SCOPE

TGi CSeqDBGiIndex::GetSeqGI(TOid             oid,
                            CSeqDBLockHold & locked)
{
    const char* data(0);

    if (m_NumOIDs == 0) {
        
        data = m_Lease.GetFileDataPtr(0);

        // TODO we may want to check the version number and file type
        m_Size = (Int4) SeqDB_GetStdOrd((Int4 *) (data+8));
        m_NumOIDs = (Int4) SeqDB_GetStdOrd((Int4 *) (data+12));
    }

    if (oid >= m_NumOIDs || oid < 0) return INVALID_GI;

    TIndx offset = oid * m_Size + 32;    
    data = m_Lease.GetFileDataPtr(offset);
    return GI_FROM(Uint4, SeqDB_GetStdOrd((Uint4 *) data));
}

CSeqDBVol::CSeqDBVol(CSeqDBAtlas        & atlas,
                     const string       & name,
                     char                 prot_nucl,
                     CSeqDBGiList       * user_list,
                     CSeqDBNegativeList * neg_list,
                     int                  vol_start,
                     CSeqDBLockHold     & locked)
    : m_Atlas        (atlas),
      m_IsAA         (prot_nucl == 'p'),
      m_VolName      (name),
	  m_HasGis       (true),
      m_TaxCache     (256),
      m_MemBit       (0),
      m_OidMaskType  (0),
      m_VolStart     (vol_start),
      m_VolEnd       (0),
      m_DeflineCache (256),
      m_HaveColumns  (false),
      m_SeqFileOpened(false),
      m_HdrFileOpened(false),
      m_HashFileOpened(false),
      m_OidFileOpened(false)
{
    if (user_list) {
        m_UserGiList.Reset(user_list);
        m_OidMaskType = static_cast<int>(m_UserGiList->GetMaskOpts());
    }
    if (neg_list) {
        m_NegativeList.Reset(neg_list);
    }

    m_Idx.Reset(new CSeqDBIdxFile(atlas, name, prot_nucl));

    m_VolEnd = m_VolStart + m_Idx->GetNumOIDs();

    // To allow for empty volumes, we must tolerate the absence of all
    // files other than the index file.
}

void
CSeqDBVol::OpenSeqFile(CSeqDBLockHold & locked) const{
    if (!m_SeqFileOpened) x_OpenSeqFile();
}

void
CSeqDBVol::x_OpenSeqFile(void) const {
    CFastMutexGuard mtx_gurad(m_MtxSeq);
    if (!m_SeqFileOpened && m_Idx->GetNumOIDs() != 0) {
        m_Seq.Reset(new CSeqDBSeqFile(m_Atlas, m_VolName, (m_IsAA?'p':'n')));
    }
    m_SeqFileOpened = true;
}

void
CSeqDBVol::x_OpenHdrFile(void) const {
    CFastMutexGuard mtx_gurad(m_MtxHdr);
    if (!m_HdrFileOpened && m_Idx->GetNumOIDs() != 0) {
        m_Hdr.Reset(new CSeqDBHdrFile(m_Atlas, m_VolName, (m_IsAA?'p':'n')));
    }
    m_HdrFileOpened = true;
}

void
CSeqDBVol::x_OpenPigFile(void) const{
    CFastMutexGuard mtx_gurad(m_MtxPig);
    if (m_IsamPig.NotEmpty()) {
    	m_IsamPig->AddReference();
    }
    else if (CSeqDBIsam::IndexExists(m_VolName, (m_IsAA?'p':'n'), 'p') &&
             m_Idx->GetNumOIDs() != 0) {
        m_IsamPig = new CSeqDBIsam(m_Atlas, m_VolName, (m_IsAA?'p':'n'), 'p', ePigId);
    }
}

void
CSeqDBVol::x_UnleasePigFile(void) const{
    CFastMutexGuard mtx_gurad(m_MtxPig);
    if (m_IsamPig.NotEmpty()) {
    	if (m_IsamPig->ReferencedOnlyOnce()) {
    		m_IsamPig.Reset();
    	}
    	else {
    		m_IsamPig->RemoveReference();
    	}
    }
}

void
CSeqDBVol::x_OpenGiFile(void) const{
    CFastMutexGuard mtx_gurad(m_MtxGi);
    if (m_IsamGi.NotEmpty()) {
    	m_IsamGi->AddReference();
    }
    else if ((m_Idx->GetNumOIDs() != 0) && m_HasGis) {
    	try {
        m_IsamGi = new CSeqDBIsam(m_Atlas, m_VolName, (m_IsAA?'p':'n'), 'n', eGiId);
    	} catch(CException &) {
    		m_IsamGi.Reset();
    		m_HasGis = false;
    	}
    }
}

void
CSeqDBVol::x_UnleaseGiFile(void) const{
    CFastMutexGuard mtx_gurad(m_MtxGi);
    if (m_IsamGi.NotEmpty()) {
    	if (m_IsamGi->ReferencedOnlyOnce()) {
    		m_IsamGi.Reset();
    	}
    	else {
    		m_IsamGi->RemoveReference();
    	}
    }
}

void
CSeqDBVol::x_OpenStrFile(void) const{
    CFastMutexGuard mtx_gurad(m_MtxStr);
    if (m_IsamStr.NotEmpty()) {
    	m_IsamStr->AddReference();
    }
    else if(CSeqDBIsam::IndexExists(m_VolName, (m_IsAA?'p':'n'), 's') &&
            m_Idx->GetNumOIDs() != 0) {
        m_IsamStr = new CSeqDBIsam(m_Atlas, m_VolName, (m_IsAA?'p':'n'), 's', eStringId);
    }
}

void
CSeqDBVol::x_UnleaseStrFile(void) const{
    CFastMutexGuard mtx_gurad(m_MtxStr);
    if (m_IsamStr.NotEmpty()) {
    	if (m_IsamStr->ReferencedOnlyOnce()) {
    		m_IsamStr.Reset();
    	}
    	else {
    		m_IsamStr->RemoveReference();
    	}
    }
}

void
CSeqDBVol::x_OpenTiFile(void) const{
    CFastMutexGuard mtx_gurad(m_MtxTi);
    if (m_IsamTi.NotEmpty()) {
    	m_IsamTi->AddReference();
    }
    else if (CSeqDBIsam::IndexExists(m_VolName, (m_IsAA?'p':'n'), 't') &&
             m_Idx->GetNumOIDs() != 0) {
        m_IsamTi = new CSeqDBIsam(m_Atlas, m_VolName, (m_IsAA?'p':'n'), 't', eTiId);
    }
}

void
CSeqDBVol::x_UnleaseTiFile(void) const{
    CFastMutexGuard mtx_gurad(m_MtxTi);
    if (m_IsamTi.NotEmpty()) {
    	if (m_IsamTi->ReferencedOnlyOnce()) {
    		m_IsamTi.Reset();
    	}
    	else {
    		m_IsamTi->RemoveReference();
    	}
    }
}

void
CSeqDBVol::x_OpenHashFile(void) const{
    DEFINE_STATIC_FAST_MUTEX(mtx);
    CFastMutexGuard mtx_guard(mtx);
    if (!m_HashFileOpened &&
         CSeqDBIsam::IndexExists(m_VolName, (m_IsAA?'p':'n'), 'h') &&
         m_Idx->GetNumOIDs() != 0) {
        m_IsamHash =
            new CSeqDBIsam(m_Atlas,
                           m_VolName,
                           (m_IsAA?'p':'n'),
                           'h',
                           eHashId);
    }
    m_HashFileOpened = true;
}

void
CSeqDBVol::x_OpenOidFile(void) const{
    DEFINE_STATIC_FAST_MUTEX(mtx);
    CFastMutexGuard mtx_guard(mtx);
    if (!m_OidFileOpened &&
         CSeqDBGiIndex::IndexExists(m_VolName, (m_IsAA?'p':'n')) &&
         m_Idx->GetNumOIDs() != 0) {
        m_GiIndex =
            new CSeqDBGiIndex(m_Atlas,
                              m_VolName,
                              (m_IsAA?'p':'n'));
    }
    m_OidFileOpened = true;
}

char CSeqDBVol::GetSeqType() const
{
    return x_GetSeqType();
}

char CSeqDBVol::x_GetSeqType() const
{
    return m_Idx->GetSeqType();
}

string CSeqDBVol::GetLMDBFileName() const
{
	return m_Idx->GetLMDBFileName();
}

int CSeqDBVol::GetSeqLengthProt(int oid) const
{
    TIndx start_offset = 0;
    TIndx end_offset   = 0;

    //m_Atlas.Lock(locked);
    m_Idx->GetSeqStartEnd(oid, start_offset, end_offset);

    _ASSERT('p' == m_Idx->GetSeqType());

    // Subtract one, for the inter-sequence null.
    return int(end_offset - start_offset - 1);
}

// Assumes locked.

int CSeqDBVol::GetSeqLengthExact(int oid) const
{
    TIndx start_offset = 0;
    TIndx end_offset   = 0;

    if (!m_SeqFileOpened) x_OpenSeqFile();
    m_Idx->GetSeqStartEnd(oid, start_offset, end_offset);

    _ASSERT(m_Idx->GetSeqType() == 'n');

    int whole_bytes = int(end_offset - start_offset - 1);

    // The last byte is partially full; the last two bits of
    // the last byte store the number of nucleotides in the
    // last byte (0 to 3).

    char amb_char = 0;    
    m_Seq->ReadBytes(& amb_char, end_offset - 1, end_offset);

    int remainder = amb_char & 3;
    return (whole_bytes * 4) + remainder;
}

int CSeqDBVol::GetSeqLengthApprox(int oid) const
{
    TIndx start_offset = 0;
    TIndx end_offset   = 0;

    //m_Atlas.Lock(locked);
    m_Idx->GetSeqStartEnd(oid, start_offset, end_offset);

    _ASSERT(m_Idx->GetSeqType() == 'n');

    int whole_bytes = int(end_offset - start_offset - 1);

    // Same principle as below - but use lower bits of oid
    // instead of fetching the actual last byte.  this should
    // correct for bias, unless sequence length modulo 4 has a
    // significant statistical bias, which seems unlikely to
    // me.

    return (whole_bytes * 4) + (oid & 0x03);
}

/// Translation table type
typedef vector<Uint1> TTable;

/// Build NA2 to NcbiNA4 translation table
///
/// This builds a translation table for nucleotide data.  The table
/// will be used by s_SeqDBMapNA2ToNA4().  The table is indexed by the
/// packed nucleotide representation, or "NA2" format, which encodes
/// four bases per byte.  The elements of the table are the unpacked
/// "Ncbi-NA4" representation, which encodes two bases per byte.
///
/// @return
///    The NA2 to NA4 translation table
static void
s_SeqDBMapNA2ToNA4Setup(TTable& translated)
{
    translated.resize(512);

    Uint1 convert[16] = { 0x11,  0x12, 0x14, 0x18,
                          0x21,  0x22, 0x24, 0x28,
                          0x41,  0x42, 0x44, 0x48,
                          0x81,  0x82, 0x84, 0x88 };
    Int2 pair1 = 0;
    Int2 pair2 = 0;

    for(pair1 = 0; pair1 < 16; pair1++) {
        for(pair2 = 0; pair2 < 16; pair2++) {
            Int2 index = (pair1 * 16 + pair2) * 2;

            translated[index]   = convert[pair1];
            translated[index+1] = convert[pair2];
        }
    }
}

class CNA2ToNA4MapTable
{
public:
	CNA2ToNA4MapTable () {
		s_SeqDBMapNA2ToNA4Setup(storage);
	}
	TTable storage;
};

/// Convert sequence data from NA2 to NA4 format
///
/// This uses a translation table to convert nucleotide data.  The
/// input data is in NA2 format, the output data will be in NcbiNA4
/// format.
///
/// @param buf2bit
///    The NA2 input data. [in]
/// @param buf4bit
///    The NcbiNA4 output data. [out]
/// @param base_length
///    The length (in bases) of the input data. [in]
static void
s_SeqDBMapNA2ToNA4(const char   * buf2bit,
                   vector<char> & buf4bit,
                   int            base_length)
{
    static CSafeStatic<CNA2ToNA4MapTable> t;
    TTable& expanded = t.Get().storage;

    int estimated_length = (base_length + 1)/2;
    int bytes = 0;

    buf4bit.resize(estimated_length);

    int inp_chars = base_length/4;

    for(int i=0; i<inp_chars; i++) {
        Uint4 inp_char = (buf2bit[i] & 0xFF);

        buf4bit[bytes]   = expanded[ (inp_char*2)     ];
        buf4bit[bytes+1] = expanded[ (inp_char*2) + 1 ];
        bytes += 2;
    }

    int bases_remain = base_length - (inp_chars*4);

    if (bases_remain) {
        Uint1 remainder_bits = 2 * bases_remain;
        Uint1 remainder_mask = (0xFF << (8 - remainder_bits)) & 0xFF;
        Uint4 last_masked = buf2bit[inp_chars] & remainder_mask;

        buf4bit[bytes++] = expanded[ (last_masked*2) ];

        if (bases_remain > 2) {
            buf4bit[bytes ++] = expanded[ (last_masked*2)+1 ];
        }
    }

    buf4bit.resize(bytes);

    _ASSERT(estimated_length == (int)buf4bit.size());
}

/// Build NA2 to Ncbi-NA8 translation table
///
/// This builds a translation table for nucleotide data.  The table
/// will be used by s_SeqDBMapNA2ToNA8().  The table is indexed by the
/// packed nucleotide representation, or "NA2" format, which encodes
/// four bases per byte.  The elements of the table are the unpacked
/// "Ncbi-NA8" representation, which encodes one base per byte.
///
/// @return
///    The NA2 to NA8 translation table
static void
s_SeqDBMapNA2ToNA8Setup(TTable& translated)
{
    // Builds a table; each two bit slice holds 0,1,2 or 3.  These are
    // converted to whole bytes containing 1,2,4, or 8, respectively.

    for(int i = 0; i<256; i++) {
        int p1 = (i >> 6) & 0x3;
        int p2 = (i >> 4) & 0x3;
        int p3 = (i >> 2) & 0x3;
        int p4 = i & 0x3;

        translated.push_back(1 << p1);
        translated.push_back(1 << p2);
        translated.push_back(1 << p3);
        translated.push_back(1 << p4);
    }
}

class CNA2ToNA8MapTable
{
public:
	CNA2ToNA8MapTable () {
		s_SeqDBMapNA2ToNA8Setup(storage);
	}
	TTable storage;
};
/// Convert sequence data from NA2 to NA8 format
///
/// This uses a translation table to convert nucleotide data.  The
/// input data is in NA2 format, the output data will be in Ncbi-NA8
/// format.  This function also optionally adds sentinel bytes to the
/// start and end of the data (needed by some applications).
///
/// @param buf2bit
///    The NA2 input data. [in]
/// @param buf8bit
///    The start of the Ncbi-NA8 output data. [out]
/// @param buf8bit_end
///    The end of the Ncbi-NA8 output data. [out]
/// @param sentinel_bytes
///    Specify true if sentinel bytes should be included. [in]
/// @param range
///    The subregion of the sequence to work on. [in]
static void
s_SeqDBMapNA2ToNA8(const char        * buf2bit,
                   char              * buf8bit,
                   const SSeqDBSlice & range)
{
    // Design note: The variable "p" makes this algorithm much easier
    // to write correctly.  It represents a pointer into the input
    // data and is maintained to point at the next unused byte of
    // input data.

    static CSafeStatic<CNA2ToNA8MapTable> t;
    TTable& expanded = t.Get().storage;

    int pos = range.begin;

    int input_chars_begin = range.begin     / 4;
    int input_chars_end   = (range.end + 3) / 4;

    int whole_chars_begin = (range.begin + 3) / 4;
    int whole_chars_end   = range.end         / 4;

    int p = input_chars_begin;

    if (p < whole_chars_begin) {
        Int4 table_offset = (buf2bit[input_chars_begin] & 0xFF) * 4;

        int endpt = (input_chars_begin + 1) * 4;

        if (endpt > range.end) {
            endpt = range.end;
        }

        for(int k = range.begin; k < endpt; k++) {
            switch(k & 0x3) {
            case 0:
                _ASSERT(0);
                break;

            case 1:
                buf8bit[pos++] = expanded[ table_offset + 1 ];
                break;

            case 2:
                buf8bit[pos++] = expanded[ table_offset + 2 ];
                break;

            case 3:
                buf8bit[pos++] = expanded[ table_offset + 3 ];
                break;
            }
        }

        p ++;
    }

    // In a nucleotide search, this loop is probably a noticeable time
    // consumer, at least relative to the CSeqDB universe.  Each input
    // byte is used to look up a 4 byte output translation.  That four
    // byte section is copied to the output vector.  By pre-processing
    // the arithmetic in the ~Setup() function, we can just pull bytes
    // from a vector.

    p = whole_chars_begin;

    while(p < whole_chars_end) {
        Int4 table_offset = (buf2bit[p] & 0xFF) * 4;

        buf8bit[pos++] = expanded[ table_offset ];
        buf8bit[pos++] = expanded[ table_offset + 1 ];
        buf8bit[pos++] = expanded[ table_offset + 2 ];
        buf8bit[pos++] = expanded[ table_offset + 3 ];
        p++;
    }

    if (p < input_chars_end) {
        Int4 table_offset = (buf2bit[p] & 0xFF) * 4;

        int remains = (range.end & 0x3);
        _ASSERT(remains);

        buf8bit[pos++] = expanded[ table_offset ];

        if (remains > 1) {
            buf8bit[pos++] = expanded[ table_offset + 1 ];

            if (remains > 2) {
                buf8bit[pos++] = expanded[ table_offset + 2 ];
            }
        }
    }
}

unsigned SeqDB_ncbina8_to_blastna8[] = {
    15, /* Gap, 0 */
    0,  /* A,   1 */
    1,  /* C,   2 */
    6,  /* M,   3 */
    2,  /* G,   4 */
    4,  /* R,   5 */
    9,  /* S,   6 */
    13, /* V,   7 */
    3,  /* T,   8 */
    8,  /* W,   9 */
    5,  /* Y,  10 */
    12, /* H,  11 */
    7,  /* K,  12 */
    11, /* D,  13 */
    10, /* B,  14 */
    14  /* N,  15 */
};

/// Convert sequence data from Ncbi-NA8 to Blast-NA8 format
///
/// This uses a translation table to convert nucleotide data.  The
/// input data is in Ncbi-NA8 format, the output data will be in
/// Blast-NA8 format.  The data is converted in-place.
///
/// @param buf
///    The array of nucleotides to convert. [in|out]
/// @param range
///    The range of opearation. [in]
static void
s_SeqDBMapNcbiNA8ToBlastNA8(char              * buf,
                            const SSeqDBSlice & range)
{
    for(int i = range.begin; i < range.end; i++)
        buf[i] = SeqDB_ncbina8_to_blastna8[ buf[i] & 0xF ];
}

//--------------------
// NEW (long) version
//--------------------

/// Get length of ambiguous region (new version)
///
/// Given an ambiguity element in the new format, this returns the
/// length of the ambiguous region.
///
/// @param ambchars
///     The packed ambiguity data. [in]
/// @param i
///     The index into the ambiguity data. [in]
/// @return
///     The region length.
inline Uint4 s_ResLenNew(const vector<Int4> & ambchars, Uint4 i)
{
    return (ambchars[i] >> 16) & 0xFFF;
}

/// Get position of ambiguous region (new version)
///
/// Given an ambiguity element in the new format, this returns the
/// position of the ambiguous region.
///
/// @param ambchars
///     The packed ambiguity data. [in]
/// @param i
///     The index into the ambiguity data. [in]
/// @return
///     The region length.
inline Uint4 s_ResPosNew(const vector<Int4> & ambchars, Uint4 i)
{
    return ambchars[i+1];
}

//-----------------------
// OLD (compact) version
//-----------------------

/// Get ambiguous residue value (old version)
///
/// Given an ambiguity element in the old format, this returns the
/// residue value to use for all bases in the ambiguous region.
///
/// @param ambchars
///     The packed ambiguity data. [in]
/// @param i
///     The index into the ambiguity data. [in]
/// @return
///     The residue value.
inline Uint4 s_ResVal(const vector<Int4> & ambchars, Uint4 i)
{
    return (ambchars[i] >> 28) & 0xF;
}

/// Get ambiguous region length (old version)
///
/// Given an ambiguity element in the old format, this returns the
/// length of the ambiguous region.
///
/// @param ambchars
///     The packed ambiguity data. [in]
/// @param i
///     The index into the ambiguity data. [in]
/// @return
///     The residue value.
inline Uint4 s_ResLenOld(const vector<Int4> & ambchars, Uint4 i)
{
    return (ambchars[i] >> 24) & 0xF;
}

/// Get ambiguous residue value (old version)
///
/// Given an ambiguity element in the old format, this returns the
/// position of the ambiguous region.
///
/// @param ambchars
///     The packed ambiguity data. [in]
/// @param i
///     The index into the ambiguity data. [in]
/// @return
///     The residue value.
inline Uint4 s_ResPosOld(const vector<Int4> & ambchars, Uint4 i)
{
    return ambchars[i] & 0xFFFFFF; // RES_OFFSET
}

/// Rebuild an ambiguous region from sequence and ambiguity data
///
/// When sequence data for a blast database is built, ambiguous
/// regions are replaced with random strings of the four standard
/// nucleotides.  The ambiguity data is seperately encoded as a
/// sequence of integer values.  This function unpacks the ambiguity
/// data and replaces the randomized bases with correct (ambiguous)
/// encodings.  This version works with 4 bit representations.
///
/// @param buf4bit
///     Sequence data for a sequence. [in|out]
/// @param amb_chars
///     Corresponding ambiguous data. [in]
static void
s_SeqDBRebuildDNA_NA4(vector<char>       & buf4bit,
                      const vector<Int4> & amb_chars)
{
    if (amb_chars.empty())
        return;

    // Number of ambiguities.
    Uint4 amb_num = amb_chars[0];

    // The new format is indicated by setting the highest order bit in
    // the LENGTH field.  Either all ambiguities for this sequence
    // will use the new format, or all will use the old format.

    bool new_format = (amb_num & 0x80000000) != 0;

    if (new_format) {
	amb_num &= 0x7FFFFFFF;
    }

    for(Uint4 i=1; i < amb_num+1; i++) {
        Int4  row_len  = 0;
        Int4  position = 0;
        Uint1 char_r   = 0;

	if (new_format) {
            char_r    = s_ResVal   (amb_chars, i);
            row_len   = s_ResLenNew(amb_chars, i);
            position  = s_ResPosNew(amb_chars, i);
	} else {
            char_r    = s_ResVal   (amb_chars, i);
            row_len   = s_ResLenOld(amb_chars, i);
            position  = s_ResPosOld(amb_chars, i);
	}

        Int4  pos = position / 2;
        Int4  rem = position & 1;  /* 0 or 1 */
        Uint1 char_l = char_r << 4;

        Int4 j;
        Int4 index = pos;

        // This could be made slightly faster for long runs.

        for(j = 0; j <= row_len; j++) {
            if (!rem) {
           	buf4bit[index] = (buf4bit[index] & 0x0F) + char_l;
            	rem = 1;
            } else {
           	buf4bit[index] = (buf4bit[index] & 0xF0) + char_r;
            	rem = 0;
                index++;
            }
    	}

	if (new_format) // for new format we have 8 bytes for each element.
            i++;
    }
}

/// Rebuild an ambiguous region from sequence and ambiguity data
///
/// When sequence data for a blast database is built, ambiguous
/// regions are replaced with random strings of the four standard
/// nucleotides.  The ambiguity data is seperately encoded as a
/// sequence of integer values.  This function unpacks the ambiguity
/// data and replaces the randomized bases with correct (ambiguous)
/// encodings.  This version works with 8 bit representations.
///
/// @param seq
///   Sequence data for a sequence. [in|out]
/// @param amb_chars
///   Corresponding ambiguous data. [in]
/// @param region
///   If non-null, the part of the sequence to get. [in]
static void
s_SeqDBRebuildDNA_NA8(char               * seq,
                      const vector<Int4> & amb_chars,
                      const SSeqDBSlice  & region)
{
    if (amb_chars.empty() || !seq ) return;

    Uint4 amb_num = amb_chars[0];

    /* Check if highest order bit set. */
    bool new_format = (amb_num & 0x80000000) != 0;

    if (new_format)  amb_num &= 0x7FFFFFFF;

    for(Uint4 i = 1; i < amb_num+1; i++) {
        Int4  row_len  = 0;
        Int4  position = 0;
        Uint1 trans_ch = 0;

	if (new_format) {
            trans_ch  = s_ResVal   (amb_chars, i);
            row_len   = s_ResLenNew(amb_chars, i) + 1;
            position  = s_ResPosNew(amb_chars, i);
	} else {
            trans_ch  = s_ResVal   (amb_chars, i);
            row_len   = s_ResLenOld(amb_chars, i) + 1;
            position  = s_ResPosOld(amb_chars, i);
	}

        if (new_format) ++i;

        if (position + row_len <= region.begin)
            continue;

        if(position >= region.end)
        	break;

        for (int j = 0; j < row_len; ++j, ++position)
            if ( position >= region.begin  && position < region.end)
                seq[position] = trans_ch;
    }
}

/// Store protein sequence data in a Seq-inst
///
/// This function reads length elements from seq_buffer and stores
/// them in a Seq-inst object.  It also sets appropriate encoding
/// information in that object.
///
/// @param seqinst
///     The Seq-inst to return the data in. [out]
/// @param seq_buffer
///     The input sequence data. [in]
/// @param length
///     The length (in bases) of the input data. [in]
static void
s_SeqDBWriteSeqDataProt(CSeq_inst  & seqinst,
                        const char * seq_buffer,
                        int          length)
{
    // stuff - ncbistdaa
    // mol = aa

    // This possibly/probably copies several times.
    // 1. One copy into stdaa_data.
    // 2. Second copy into NCBIstdaa.
    // 3. Third copy into seqdata.

    vector<char> aa_data;
    aa_data.resize(length);

    for(int i = 0; i < length; i++) {
        aa_data[i] = seq_buffer[i];
    }

    seqinst.SetSeq_data().SetNcbistdaa().Set().swap(aa_data);
    seqinst.SetMol(CSeq_inst::eMol_aa);
}

/// Store non-ambiguous nucleotide sequence data in a Seq-inst
///
/// This function reads length elements from seq_buffer and stores
/// them in a Seq-inst object.  It also sets appropriate encoding
/// information in that object.  No ambiguity information is used.
/// The input array is assumed to be in 2 bit representation.
///
/// @param seqinst
///     The Seq-inst to return the data in. [out]
/// @param seq_buffer
///     The input sequence data. [in]
/// @param length
///     The length (in bases) of the input data. [in]
static void
s_SeqDBWriteSeqDataNucl(CSeq_inst    & seqinst,
                        const char   * seq_buffer,
                        int            length)
{
    int whole_bytes  = length / 4;
    int partial_byte = ((length & 0x3) != 0) ? 1 : 0;

    vector<char> na_data;
    na_data.resize(whole_bytes + partial_byte);

    for(int i = 0; i<whole_bytes; i++) {
        na_data[i] = seq_buffer[i];
    }

    if (partial_byte) {
        na_data[whole_bytes] = seq_buffer[whole_bytes] & (0xFF - 0x03);
    }

    seqinst.SetSeq_data().SetNcbi2na().Set().swap(na_data);
    seqinst.SetMol(CSeq_inst::eMol_na);
}

/// Store non-ambiguous nucleotide sequence data in a Seq-inst
///
/// This function reads length elements from seq_buffer and stores
/// them in a Seq-inst object.  It also sets appropriate encoding
/// information in that object.  No ambiguity information is used.
/// The input array is assumed to be in Ncbi-NA4 representation.
///
/// @param seqinst
///     The Seq-inst to return the data in. [out]
/// @param seq_buffer
///     The input sequence data in Ncbi-NA4 format. [in]
/// @param length
///     The length (in bases) of the input data. [in]
/// @param amb_chars
///     The ambiguity data for this sequence. [in]
static void
s_SeqDBWriteSeqDataNucl(CSeq_inst    & seqinst,
                        const char   * seq_buffer,
                        int            length,
                        vector<Int4> & amb_chars)
{
    vector<char> buffer_4na;
    s_SeqDBMapNA2ToNA4(seq_buffer, buffer_4na, length); // length is not /4 here
    s_SeqDBRebuildDNA_NA4(buffer_4na, amb_chars);

    seqinst.SetSeq_data().SetNcbi4na().Set().swap(buffer_4na);
    seqinst.SetMol(CSeq_inst::eMol_na);
}

/// Get the title string for a CBioseq
///
/// GetBioseq will use this function to get a title field when
/// constructing the CBioseq object.
///
/// @param deflines
///   The set of deflines for this sequence. [in]
/// @param title
///   The returned title string. [out]
static void
s_GetBioseqTitle(CRef<CBlast_def_line_set> deflines, string & title)
{
    title.erase();

    string seqid_str;

    typedef list< CRef<CBlast_def_line> >::const_iterator TDefIt;
    typedef list< CRef<CSeq_id        > >::const_iterator TSeqIt;

    const list< CRef<CBlast_def_line> > & dl = deflines->Get();

    bool first_defline(true);

    for(TDefIt iter = dl.begin(); iter != dl.end(); iter++) {
        ostringstream oss;

        const CBlast_def_line & defline = **iter;

        if (! title.empty()) {
            //oss << "\1";
            oss << " ";
        }

        bool wrote_seqids(false);

        if ((!first_defline) && defline.CanGetSeqid()) {
            const list< CRef<CSeq_id > > & sl = defline.GetSeqid();

            bool first_seqid(true);

            // First should look like: "<title>"
            // Others should look like: " ><seqid>|<seqid>|<seqid><title>"

            // Should this be two sections not one loop?

            for (TSeqIt seqit = sl.begin(); seqit != sl.end(); seqit++) {
                if (first_seqid) {
                    oss << ">";
                } else {
                    oss << "|";
                }

                (*seqit)->WriteAsFasta(oss);

                first_seqid = false;
                wrote_seqids = true;
            }
        }

        // Omit seqids from first defline
        first_defline = false;

        if (defline.CanGetTitle()) {
            if (wrote_seqids) {
                oss << " ";
            }
            oss << defline.GetTitle();
        }

        title += oss.str();
    }
}

/// Search for a Seq-id in a list of Seq-ids
///
/// This iterates over a list of Seq-ids, and returns true if a
/// specific Seq-id is equivalent to one found in the list.
///
/// @param seqids
///     A list of Seq-ids to search. [in]
/// @param target
///     The Seq-id to search for. [in]
/// @return
///     True if the Seq-id was found.
static bool
s_SeqDB_SeqIdIn(const list< CRef< CSeq_id > > & seqids, const CSeq_id & target)
{
    typedef list< CRef<CSeq_id> > TSeqidList;

    ITERATE(TSeqidList, iter, seqids) {
        CSeq_id::E_SIC rv = (**iter).Compare(target);

        switch(rv) {
        case CSeq_id::e_YES:
            return true;

        case CSeq_id::e_NO:
            return false;

        default:
            break;
        }
    }

    return false;
}

CRef<CBlast_def_line_set>
CSeqDBVol::x_GetTaxDefline(int                    oid,
                           TGi                    preferred_gi,
                           const CSeq_id        * preferred_seqid)
                           
{
    // Commented out, not used at this time.
//    typedef list< CRef<CBlast_def_line> > TBDLL;
//    typedef TBDLL::iterator               TBDLLIter;
//    typedef TBDLL::const_iterator         TBDLLConstIter;

    // 1. read a defline set w/ gethdr, filtering by membership bit.

    CRef<CBlast_def_line_set> BDLS =
        x_GetFilteredHeader(oid, NULL);

    // 2. if there is a preferred gi, bump it to the top.

    if (preferred_gi != ZERO_GI || preferred_seqid) {
        CRef<CBlast_def_line_set> new_bdls(new CBlast_def_line_set);

        CRef<const CSeq_id> seqid;
        if (preferred_gi != ZERO_GI) {
            seqid.Reset(new CSeq_id(CSeq_id::e_Gi, preferred_gi));
        } else {
            seqid.Reset(preferred_seqid);
        }

        bool found = false;

        ITERATE(list< CRef<CBlast_def_line> >, iter, BDLS->Get()) {
            if ((! found) && s_SeqDB_SeqIdIn((**iter).GetSeqid(), *seqid)) {
                found = true;
                new_bdls->Set().push_front(*iter);
            } else {
                new_bdls->Set().push_back(*iter);
            }
        }

        return new_bdls;
    }

    return BDLS;
}

list< CRef<CSeqdesc> >
CSeqDBVol::x_GetTaxonomy(int                    oid,
                         TGi                    preferred_gi,
                         const CSeq_id        * preferred_seqid)
                         

{
    const bool provide_new_taxonomy_info = true;
    const bool use_taxinfo_cache         = (CThread::GetSelf() == 0 ? true : false);

    const char * TAX_ORGREF_DB_NAME = "taxon";

    list< CRef<CSeqdesc> > taxonomy;

    CRef<CBlast_def_line_set> bdls =
        x_GetTaxDefline(oid, preferred_gi, preferred_seqid);

    if (bdls.Empty()) {
        return taxonomy;
    }

    typedef list< CRef<CBlast_def_line> > TBDLL;
//    typedef TBDLL::iterator               TBDLLIter;  // not used at this time
    typedef TBDLL::const_iterator         TBDLLConstIter;

    const TBDLL & dl = bdls->Get();

    // Lock for sake of tax cache

    //m_Atlas.Lock(locked);

    for(TBDLLConstIter iter = dl.begin(); iter != dl.end(); iter ++) {
        TTaxId taxid = ZERO_TAX_ID;

        if ((*iter)->CanGetTaxid()) {
            taxid = (*iter)->GetTaxid();
        }
        if (taxid <= ZERO_TAX_ID) {
            continue;
        }

        bool have_org_desc = false;

        if (use_taxinfo_cache && m_TaxCache.Lookup(TAX_ID_TO(int, taxid)).NotEmpty()) {
            have_org_desc = true;
        }

        SSeqDBTaxInfo tnames(taxid);
        bool found_taxid_in_taxonomy_blastdb = true;

        if ((! have_org_desc) && provide_new_taxonomy_info) {
            try {
                found_taxid_in_taxonomy_blastdb = CSeqDBTaxInfo::GetTaxNames(taxid, tnames);
            } catch (CSeqDBException &) {
                found_taxid_in_taxonomy_blastdb = false;
            }
        }

        if (provide_new_taxonomy_info) {
            if (have_org_desc) {
                taxonomy.push_back(m_TaxCache.Lookup(TAX_ID_TO(int, taxid)));
            } else {
                CRef<CDbtag> org_tag(new CDbtag);
                org_tag->SetDb(TAX_ORGREF_DB_NAME);
                org_tag->SetTag().SetId(TAX_ID_TO(int, taxid));

                CRef<COrg_ref> org(new COrg_ref);
                if (found_taxid_in_taxonomy_blastdb) {
                    org->SetTaxname().swap(tnames.scientific_name);
                    org->SetCommon().swap(tnames.common_name);
                }
                org->SetDb().push_back(org_tag);

                CRef<CBioSource>   source;
                source.Reset(new CBioSource);
                source->SetOrg(*org);

                CRef<CSeqdesc> desc(new CSeqdesc);
                desc->SetSource(*source);

                taxonomy.push_back(desc);

                if (use_taxinfo_cache) {
                    m_TaxCache.Lookup(TAX_ID_TO(int, taxid)) = desc;
                }
            }
        }
    }

    return taxonomy;
}

/// Efficiently decode a Blast-def-line-set from binary ASN.1.
/// @param oss Octet string sequence of binary ASN.1 data.
/// @param bdls Blast def line set decoded from oss.
static CRef<CBlast_def_line_set>
s_OssToDefline(const CUser_field::TData::TOss & oss)
{
    typedef const CUser_field::TData::TOss TOss;

    const char * data = NULL;
    size_t size = 0;
    string temp;

    if (oss.size() == 1) {
        // In the single-element case, no copies are needed.

        const vector<char> & v = *oss.front();
        data = & v[0];
        size = v.size();
    } else {
        // Determine the octet string length and do one allocation.

        ITERATE (TOss, iter1, oss) {
            size += (**iter1).size();
        }

        temp.reserve(size);

        ITERATE (TOss, iter3, oss) {
            // 23.2.4[1] "The elements of a vector are stored contiguously".
            temp.append(& (**iter3)[0], (*iter3)->size());
        }

        data = & temp[0];
    }

    CObjectIStreamAsnBinary inpstr(data, size);
    CRef<CBlast_def_line_set> retval(new CBlast_def_line_set);
    inpstr >> *retval;
    return retval;
}

template<class T>
CRef<CBlast_def_line_set>
s_ExtractBlastDefline(const T& bioseq)
{
    CRef<CBlast_def_line_set> failure;
    if ( !bioseq.IsSetDescr() ) {
        return failure;
    }

    const CSeq_descr::Tdata& descList = bioseq.GetDescr().Get();
    ITERATE(CSeq_descr::Tdata, iter, descList) {
        if ( !(*iter)->IsUser() ) {
            continue;
        }

        const CUser_object& uobj = (*iter)->GetUser();
        const CObject_id& uobjid = uobj.GetType();
        if (uobjid.IsStr() && uobjid.GetStr() == kAsnDeflineObjLabel) {
            const vector< CRef< CUser_field > >& usf = uobj.GetData();
            _ASSERT( !usf.empty() );
            _ASSERT(usf.front()->CanGetData());
            if (usf.front()->GetData().IsOss()) { //only one user field
                return s_OssToDefline(usf.front()->GetData().GetOss());
            }
        }
    }
    return failure;
}

CRef<CBlast_def_line_set>
CSeqDB::ExtractBlastDefline(const CBioseq_Handle & handle)
{ return s_ExtractBlastDefline(handle); }

CRef<CBlast_def_line_set>
CSeqDB::ExtractBlastDefline(const CBioseq & bioseq)
{ return s_ExtractBlastDefline(bioseq); }

CRef<CSeqdesc>
CSeqDBVol::x_GetAsnDefline(int oid) const
{
    CRef<CSeqdesc> asndef;

    vector<char> hdr_data;
    x_GetFilteredBinaryHeader(oid, hdr_data);

    if (! hdr_data.empty()) {
        CRef<CUser_object> uobj(new CUser_object);

        CRef<CObject_id> uo_oi(new CObject_id);
        uo_oi->SetStr(kAsnDeflineObjLabel);
        uobj->SetType(*uo_oi);

        CRef<CUser_field> uf(new CUser_field);

        CRef<CObject_id> uf_oi(new CObject_id);
        uf_oi->SetStr(kAsnDeflineObjLabel);
        uf->SetLabel(*uf_oi);

        vector< vector<char>* > & strs = uf->SetData().SetOss();
        uf->SetNum(1);

        strs.push_back(new vector<char>);
        strs[0]->swap(hdr_data);

        uobj->SetData().push_back(uf);

        asndef = new CSeqdesc;
        asndef->SetUser(*uobj);
    }

    return asndef;
}

CRef<CBioseq>
CSeqDBVol::GetBioseq(int                    oid,
                     TGi                    target_gi,
                     const CSeq_id        * target_seq_id,
                     bool                   seqdata,
                     CSeqDBLockHold       & locked)
{
    typedef list< CRef<CBlast_def_line> > TDeflines;
    CRef<CBioseq> null_result;

    CRef<CBlast_def_line>     defline;
    list< CRef< CSeq_id > >   seqids;

    if (!m_SeqFileOpened) x_OpenSeqFile();

    // Get the defline set; but do not modify the object returned by
    // GetFilteredHeader, since that object lives in the cache.

    CRef<CBlast_def_line_set> orig_deflines =
        x_GetFilteredHeader(oid,  NULL);

    CRef<CBlast_def_line_set> defline_set;

    if ((target_gi != ZERO_GI) || target_seq_id) {
        defline_set.Reset(new CBlast_def_line_set);

        CRef<const CSeq_id > seqid;
        CRef<const CSeq_id > seqid_lcl;
        if (target_gi != ZERO_GI) {
            seqid.Reset(new CSeq_id(CSeq_id::e_Gi, target_gi));
            seqid_lcl.Reset(new CSeq_id(CSeq_id::e_Local, NStr::NumericToString(target_gi)));
        }
        else {
            seqid.Reset(target_seq_id);
            seqid_lcl.Reset(new CSeq_id(CSeq_id::e_Local, seqid->GetSeqIdString(true)));
        }

        CRef<CBlast_def_line> filt_dl;

        ITERATE(TDeflines, iter, orig_deflines->Get()) {
            if ((seqid.NotEmpty() && s_SeqDB_SeqIdIn((**iter).GetSeqid(), *seqid)) ||
                (seqid_lcl.NotEmpty() && s_SeqDB_SeqIdIn((**iter).GetSeqid(), *seqid_lcl))) {
                filt_dl = *iter;
                break;
            }
        }

        if (filt_dl.Empty()) {
            NCBI_THROW(CSeqDBException, eArgErr,
                       "Error: oid headers do not contain target gi/seq_id.");
        } else {
            defline_set->Set().push_back(filt_dl);
        }
    } else {
        defline_set = orig_deflines;
    }

    if (defline_set.Empty() ||
        (! defline_set->CanGet()) ||
        (0 == defline_set->Get().size())) {
        return null_result;
    }

    defline = defline_set->Get().front();
    if (! defline->CanGetSeqid()) {
        return null_result;
    }
    seqids = defline->GetSeqid();

    // Get length & sequence.

    CRef<CBioseq> bioseq(new CBioseq);

    bool is_prot = (x_GetSeqType() == 'p');

    if (seqdata) {
        const char * seq_buffer = 0;
        int length = x_GetSequence(oid, & seq_buffer);

        if (length < 1) {
            return null_result;
        }

        // If protein, we set bsp->mol = Seq_mol_aa, seq_data_type =
        // Seq_code_ncbistdaa; then we write the buffer into the byte
        // store (or equivalent).
        //
        // Nucleotide sequences require more work:
        // a. Try to get ambchars
        // b. If there are any, convert sequence to 4 byte rep.
        // c. Otherwise write to a byte store.
        // d. Set mol = Seq_mol_na;

        CSeq_inst & seqinst = bioseq->SetInst();

        if (is_prot) {
            s_SeqDBWriteSeqDataProt(seqinst, seq_buffer, length);
        } else {
            // nucl
            vector<Int4> ambchars;

            x_GetAmbChar(oid, ambchars);

            if (ambchars.empty()) {
                // keep as 2 bit
                s_SeqDBWriteSeqDataNucl(seqinst, seq_buffer, length);
            } else {
                // translate to 4 bit
                s_SeqDBWriteSeqDataNucl(seqinst, seq_buffer, length, ambchars);
            }

            // mol = na
            seqinst.SetMol(CSeq_inst::eMol_na);
        }

        if (seq_buffer) {
            seq_buffer = 0;
        }

        // Set the length and repr (== raw).

        seqinst.SetLength(length);
        seqinst.SetRepr(CSeq_inst::eRepr_raw);
    } else {
        CSeq_inst & seqinst = bioseq->SetInst();
        seqinst.SetRepr(CSeq_inst::eRepr_not_set);

        bioseq->SetInst().SetMol(is_prot
                                 ? CSeq_inst::eMol_aa
                                 : CSeq_inst::eMol_na);
    }

    // Set the id (Seq_id)

    bioseq->SetId().swap(seqids);

    // If the format is binary, we get the defline and chain it onto
    // the bsp->desc list; then we read and append taxonomy names to
    // the list (x_GetTaxonomy()).

    if (defline_set.NotEmpty()) {
        // Convert defline to string.

        string description;

        s_GetBioseqTitle(defline_set, description);

        CRef<CSeqdesc> desc1(new CSeqdesc);
        desc1->SetTitle().swap(description);

        CRef<CSeqdesc> desc2( x_GetAsnDefline(oid) );

        CSeq_descr & seq_desc_set = bioseq->SetDescr();
        seq_desc_set.Set().push_back(desc1);

        if (! desc2.Empty()) {
            seq_desc_set.Set().push_back(desc2);
        }
    }

    list< CRef<CSeqdesc> > tax =
        x_GetTaxonomy(oid, target_gi, target_seq_id);

    ITERATE(list< CRef<CSeqdesc> >, iter, tax) {
        bioseq->SetDescr().Set().push_back(*iter);
    }

    return bioseq;
}

char * CSeqDBVol::x_AllocType(size_t           length,
                              ESeqDBAllocType  alloc_type) const
{
    // Allocation using the atlas is not intended for the end user.
    // 16 bytes are added as insurance against potential off-by-one or
    // off-by-a-few errors.

    length += 16;

    char * retval = 0;

    switch(alloc_type) {
    case eMalloc:
        retval = (char*) malloc(length);
        break;

    case eNew:
        retval = new char[length];
        break;

    case eAtlas:
    default:
        retval = CSeqDBAtlas::Alloc(length + 16, false);
    }

    return retval;
}

int CSeqDBVol::GetAmbigSeq(int                oid,
                           char            ** buffer,
                           int                nucl_code,
                           ESeqDBAllocType    alloc_type,
                           SSeqDBSlice      * region,
                           CSeqDB::TSequenceRanges  * masks) const
{
    char * buf1 = 0;
    int baselen =
        x_GetAmbigSeq(oid, & buf1, nucl_code, alloc_type, region, masks);

    *buffer = buf1;
    return baselen;
}

static void s_SeqDBMaskSequence(char                    * seq,
                                CSeqDB::TSequenceRanges * masks,
                                char                      mask_letter,
                                const SSeqDBSlice       & range)
{
    if (!masks || masks->empty()) return;

    // TODO This could be optimized with binary search
    unsigned int i(0);
    unsigned int begin(range.begin);
    unsigned int end(range.end);

    while (i < masks->size() && (*masks)[i].second <= begin) ++i;

    while (i < masks->size() && (*masks)[i].first < end) {
        for (size_t j = max((*masks)[i].first, begin);
                 j < min((*masks)[i].second, end); ++j) {
            seq[j] = mask_letter;
        }
        ++i;
    }
}

int CSeqDBVol::GetAmbigPartialSeq(int                oid,
                                  char            ** buffer,
                                  int                nucl_code,
                                  ESeqDBAllocType    alloc_type,
                                  CSeqDB::TSequenceRanges  * partial_ranges,
                                  CSeqDB::TSequenceRanges  * masks) const
{

	if ((partial_ranges == NULL) || (partial_ranges->size() == 0)) {
        NCBI_THROW(CSeqDBException, eFileErr, "Error: Empty partial fetching ranges.");
	}

    const char * tmp(0);
    int base_length = x_GetSequence(oid, &tmp);
	if (base_length < 1) {
	    NCBI_THROW(CSeqDBException, eFileErr, "Error: could not get sequence or range.");
	}

    int num_ranges = static_cast<int>(partial_ranges->size());
    if ((*partial_ranges)[num_ranges - 1].second > static_cast<TSeqPos>(base_length)) {
        NCBI_THROW(CSeqDBException, eFileErr, "Error: region beyond sequence range.");
    }

    bool sentinel = (nucl_code == kSeqDBNuclBlastNA8);
    *buffer = x_AllocType(base_length + (sentinel ? 2 : 0), alloc_type);
    char *seq = *buffer + (sentinel ? 1 : 0);

    vector<Int4> ambchars;
	x_GetAmbChar(oid, ambchars);
    ITERATE(CSeqDB::TSequenceRanges, riter, *(partial_ranges)) {
    	int begin(riter->first);
        int end(riter->second);

	    if (begin) seq[begin - 1] = (char) FENCE_SENTRY;
	    if (end < base_length) seq[end] = (char) FENCE_SENTRY;
	}

    //cerr << "Oid: " << oid << endl;

    ITERATE(CSeqDB::TSequenceRanges, riter, *(partial_ranges)) {
        SSeqDBSlice slice(max(0, (int)riter->first), min(base_length, (int)riter->second));
	    //cerr << "Use range set: " << riter->first << "\t" << riter->second << "\t" << "length: " << base_length << endl;
	    s_SeqDBMapNA2ToNA8(tmp, seq, slice);
	    s_SeqDBRebuildDNA_NA8(seq, ambchars, slice);
	    s_SeqDBMaskSequence(seq, masks, (char)14, slice);
	    if (sentinel){
	    	s_SeqDBMapNcbiNA8ToBlastNA8(seq, slice);
	    }
	}

	if (sentinel) {
	    (*buffer)[0] = (char)15;
	    (*buffer)[base_length+1] = (char)15;
	}
	if (masks) {
		masks->clear();
	}
    return base_length;
}




/// List of offset ranges as begin/end pairs.
typedef set< pair<int, int> > TRangeVector;

int CSeqDBVol::x_GetAmbigSeq(int                oid,
                             char            ** buffer,
                             int                nucl_code,
                             ESeqDBAllocType    alloc_type,
                             SSeqDBSlice      * region,
                             CSeqDB::TSequenceRanges *masks) const
{
    // Note the use of the third argument of x_GetSequence() to manage
    // the lifetime of the acquired region.  Specifying false for that
    // argument ties the lifetime to the CSeqDBSeqFile's memory lease.

    const char * tmp(0);
    int base_length = x_GetSequence(oid, &tmp);

    if (region && region->end > base_length )
        NCBI_THROW(CSeqDBException, eFileErr, "Error: region beyond sequence range.");

    SSeqDBSlice range = region ? (*region) : SSeqDBSlice(0, base_length);
    base_length = range.end - range.begin;

    if (base_length < 1)
        NCBI_THROW(CSeqDBException, eFileErr, "Error: could not get sequence or range.");

    if (m_Idx->GetSeqType() == 'p') {

        tmp += range.begin;
        *buffer = x_AllocType(base_length, alloc_type);
        memcpy(*buffer, tmp, base_length);
        s_SeqDBMaskSequence(*buffer - range.begin, masks, (char)21, range);

    } else {

        bool sentinel = (nucl_code == kSeqDBNuclBlastNA8);
        *buffer = x_AllocType(base_length + (sentinel ? 2 : 0), alloc_type);
        char *seq = *buffer - range.begin + (sentinel ? 1 : 0);

        // Get ambiguity characters.

        vector<Int4> ambchars;
        x_GetAmbChar(oid, ambchars);

        // Determine if we want to filter by offset ranges.  This
        // is only done if:
        //
        // 1. No range is specified by the user.
        // 2. We have cached ranges.
        TRangeList range_set;
        bool use_range_set = true;
        {
    	CFastMutexGuard mtx_guard(m_MtxCachedRange);
        TRangeCache::iterator rciter = m_RangeCache.find(oid);
        if (region
         || rciter == m_RangeCache.end()
         || rciter->second->GetRanges().empty()
         || CSeqDBRangeList::ImmediateLength() >= base_length) {
            use_range_set = false;
        }
        else {
            range_set = rciter->second->GetRanges();
        }
        }

        if (!use_range_set) {
            s_SeqDBMapNA2ToNA8(tmp, seq, range);
            s_SeqDBRebuildDNA_NA8(seq, ambchars, range);
            s_SeqDBMaskSequence(seq, masks, (char)14, range);
            if (sentinel) s_SeqDBMapNcbiNA8ToBlastNA8(seq, range);

        } else {

            _ASSERT (!region);
            // Place 'fence' sentinel bytes around each range; this is done
            // before any of the range data is mapped so that the range data
            // is free to replace the sentinel bytes if needed; that would
            // only happen if range_set are adjacent or overlapping.

            ITERATE(TRangeVector, riter, range_set) {
                int begin(riter->first);
                int end(riter->second);

                if (begin) seq[begin - 1] = (char) FENCE_SENTRY;
                if (end < base_length) seq[end] = (char) FENCE_SENTRY;
            }

            ITERATE(TRangeVector, riter, range_set) {

                SSeqDBSlice slice(max(0, riter->first),
                                  min(range.end, riter->second));

                s_SeqDBMapNA2ToNA8(tmp, seq, slice);
                s_SeqDBRebuildDNA_NA8(seq, ambchars, slice);
                s_SeqDBMaskSequence(seq, masks, (char)14, slice);
                if (sentinel) s_SeqDBMapNcbiNA8ToBlastNA8(seq, slice);
            }
        }

        // Put back the sentinel at last
        if (sentinel) {
            (*buffer)[0] = (char)15;
            (*buffer)[base_length+1] = (char)15;
        }
    }

    // Clear the masks after consumption
    if (masks) masks->clear();

    return base_length;
}

void SeqDB_UnpackAmbiguities(const CTempString & sequence,
                             const CTempString & ambiguities,
                             string            & result)
{
    result.resize(0);

    // The code in this block is derived from GetBioseq() and
    // s_SeqDBWriteSeqDataNucl().

    // Get the length and the (probably mmapped) data.

    if (sequence.length() == 0) {
        NCBI_THROW(CSeqDBException, eFileErr,
                   "Error: packed sequence data is not valid.");
    }

    const char * seq_buffer = sequence.data();

    int whole_bytes = static_cast<int>(sequence.length()) - 1;
    int remainder = sequence[whole_bytes] & 3;
    int base_length = (whole_bytes * 4) + remainder;

    if (base_length == 0) {
        return;
    }

    // Get ambiguity characters.

    vector<Int4> ambchars;
    ambchars.reserve(ambiguities.length()/4);

    for(size_t i = 0; i < ambiguities.length(); i+=4) {
        Int4 A = SeqDB_GetStdOrd((int*) (ambiguities.data() + i));
        ambchars.push_back(A);
    }

    // Combine and translate to 4 bits-per-character encoding.

    char * buffer_na8 = (char*) malloc(base_length);

    try {
        SSeqDBSlice range(0, base_length);

        s_SeqDBMapNA2ToNA8(seq_buffer, buffer_na8, range);

        s_SeqDBRebuildDNA_NA8(buffer_na8, ambchars, range);
    }
    catch(...) {
        free(buffer_na8);
        throw;
    }

    result.assign(buffer_na8, base_length);

    free(buffer_na8);
}


int CSeqDBVol::x_GetSequence(int              oid,
                             const char    ** buffer) const
{
    TIndx start_offset = 0;
    TIndx end_offset   = 0;

    int length = -1;

    //m_Atlas.Lock(locked);
    if (!m_SeqFileOpened) x_OpenSeqFile();

    if (oid >= m_Idx->GetNumOIDs()) return -1;

    m_Idx->GetSeqStartEnd(oid, start_offset, end_offset);

    char seqtype = m_Idx->GetSeqType();

    if ('p' == seqtype) {
        // Subtract one, for the inter-sequence null.

        end_offset --;

        length = int(end_offset - start_offset);

        // Although we subtracted one above to get the correct length,
        // we expand the range here by one byte in both directions.
        // The normal consumer of this data relies on them, and can
        // walk off memory if a sequence ends on a slice boundary.        
        *buffer = m_Seq->GetFileDataPtr(start_offset-1) + 1;
        if (! (*buffer - 1)) return -1;

    } else if ('n' == seqtype) {
        // The last byte is partially full; the last two bits of the
        // last byte store the number of nucleotides in the last byte
        // (0 to 3).

        // 'Hold' is used if we are going to fetch another kind of
        // data after this data, but before we are done actually using
        // this data.  If can_release is true, we will return after
        // this.  If keep is true, we don't need hold because keep
        // will already have preserved the region.

        
        *buffer = m_Seq->GetFileDataPtr(start_offset);

        if (! (*buffer))  return -1;

        // If we are returning a hold on the sequence (keep), and the
        // caller does not need the lock after this (can_release) we
        // can let go of the lock (because the hold will prevent GC of
        // the underlying data).  This will allow the following data
        // access to occur outside of the locked duration - lowering
        // contention in the nucleotide case.

        /* do not release the lock since we may be getting more...
        if (keep && can_release) {
            m_Atlas.Unlock(locked);
        }*/

        int whole_bytes = int(end_offset - start_offset - 1);

        char last_char = (*buffer)[whole_bytes];

        int remainder = last_char & 3;
        length = (whole_bytes * 4) + remainder;
    }

    return length;
}

list< CRef<CSeq_id> > CSeqDBVol::GetSeqIDs(int  oid) const
{
    list< CRef< CSeq_id > > seqids;

    CRef<CBlast_def_line_set> defline_set =
        x_GetFilteredHeader(oid, NULL);

    if ((! defline_set.Empty()) && defline_set->CanGet()) {
        ITERATE(list< CRef<CBlast_def_line> >, defline, defline_set->Get()) {
            if (! (*defline)->CanGetSeqid()) {
                continue;
            }

            ITERATE(list< CRef<CSeq_id> >, seqid, (*defline)->GetSeqid()) {
                seqids.push_back(*seqid);
            }
        }
    }

    return seqids;
}

list< CRef<CSeq_id> > CSeqDBVol::GetSeqIDs(int  oid, CObjectIStreamAsnBinary *inpstr ) const
{
    list< CRef< CSeq_id > > seqids;

    CRef<CBlast_def_line_set> defline_set =
        x_GetFilteredHeader(oid, NULL, inpstr);

    if ((! defline_set.Empty()) && defline_set->CanGet()) {
        ITERATE(list< CRef<CBlast_def_line> >, defline, defline_set->Get()) {
            if (! (*defline)->CanGetSeqid()) {
                continue;
            }

            ITERATE(list< CRef<CSeq_id> >, seqid, (*defline)->GetSeqid()) {
                seqids.push_back(*seqid);
            }
        }
    }

    return seqids;
}

TGi CSeqDBVol::GetSeqGI(int              oid,
                        CSeqDBLockHold & locked) const
{
    if (!m_OidFileOpened) x_OpenOidFile();
    if (!m_GiIndex.Empty()) {
         return m_GiIndex->GetSeqGI(oid, locked);
    }
    return INVALID_GI;
}

Uint8 CSeqDBVol::GetVolumeLength() const
{
    return m_Idx->GetVolumeLength();
}

CRef<CBlast_def_line_set>
CSeqDBVol::GetFilteredHeader(int                    oid,
                             CSeqDBLockHold       & locked) const
{
    return x_GetFilteredHeader(oid, NULL);
}

bool s_IncludeDefline_Taxid(const CBlast_def_line & def, const set<TTaxId> & user_tax_ids)
{
	CBlast_def_line::TTaxIds tax_ids;
	if (def.IsSetTaxid()) {
	    tax_ids.insert(def.GetTaxid());
	}
	if(def.IsSetLinks()) {
		CBlast_def_line::TLinks leaf_ids = def.GetLinks();
#ifdef NCBI_STRICT_TAX_ID
        ITERATE(CBlast_def_line::TLinks, it, leaf_ids) tax_ids.insert(TAX_ID_FROM(int, *it));
#else
        tax_ids.insert(leaf_ids.begin(), leaf_ids.end());
#endif
    }

	if(user_tax_ids.size() > tax_ids.size()) {
		ITERATE(CBlast_def_line::TTaxIds, itr, tax_ids) {
			if(user_tax_ids.find(*itr) != user_tax_ids.end()) {
				return true;
			}
		}

	}
	else {
		ITERATE(set<TTaxId>, itr, user_tax_ids) {
			if(tax_ids.find(*itr) != tax_ids.end()) {
				return true;
			}
		}
	}
	return false;
}

bool s_IncludeDefline_MaskFilter(const CBlast_def_line & def, Uint8 mask)
{
    ITERATE(list< CRef<CSeq_id> >, seqid, def.GetSeqid()) {
    	if (mask & EOidMaskType::fExcludeModel) {
    		if((*seqid)->IdentifyAccession() & CSeq_id::fAcc_predicted) {
    			return false;
    		}
    	}
    }
	return true;
}


bool s_IncludeDefline_NegativeTaxid(const CBlast_def_line & def, const set<TTaxId> & user_tax_ids)
{
	CBlast_def_line::TTaxIds taxid_set = def.GetTaxIds();
	if(taxid_set.size() > user_tax_ids.size()) {
		return true;
	}
	ITERATE(CBlast_def_line::TTaxIds, itr, taxid_set) {
		if(user_tax_ids.find(*itr) == user_tax_ids.end()) {
			return true;
		}
	}
	return false;
}

CRef<CBlast_def_line_set>
CSeqDBVol::x_GetFilteredHeader(int                    oid,
                               bool                 * changed) const
{
    typedef list< CRef<CBlast_def_line> > TBDLL;
    typedef TBDLL::iterator               TBDLLIter;

    //m_Atlas.Lock(locked);

    const bool useCache = (CThread::GetSelf() == 0 ? true : false);
    TDeflineCacheItem & cached = m_DeflineCache.Lookup(oid);

    if (useCache && cached.first.NotEmpty()) {
        if (changed) {
            *changed = cached.second;
        }

        return cached.first;
    }

    bool asn_changed = false;

    CRef<CBlast_def_line_set> BDLS =
        x_GetHdrAsn1(oid, true, & asn_changed);

    bool id_filter = x_HaveIdFilter();

    if (id_filter || m_MemBit || m_OidMaskType) {
        // Create the memberships mask (should this be fixed to allow
        // membership bits greater than 32?)

        TBDLL & dl = BDLS->Set();

        for(TBDLLIter iter = dl.begin(); iter != dl.end(); ) {
            const CBlast_def_line & defline = **iter;

            bool have_memb = true;

            if (m_MemBit) {
                have_memb =
                    defline.CanGetMemberships() &&
                    defline.IsSetMemberships() &&
                    (! defline.GetMemberships().empty());

                if (have_memb) {
                    int bits = defline.GetMemberships().front();
                    int memb_mask = 0x1 << (m_MemBit-1);

                    if ((bits & memb_mask) == 0) {
                        have_memb = false;
                    }
                }
            }

            // Here we must pass both the user-gi and volume-gi test,
            // for each defline, but not necessarily for each Seq-id.
            if (have_memb) {
            	if (id_filter && defline.CanGetSeqid()) {
            		have_memb = false;
            		bool have_user = false, have_volume = false;
            		ITERATE(list< CRef<CSeq_id> >, seqid, defline.GetSeqid()) {
            			x_FilterHasId(**seqid, have_user, have_volume);
            			if (have_user && have_volume) break;
            		}
            		have_memb = have_user && have_volume;
            	}

            	if(have_memb && (!m_UserGiList.Empty()) && (m_UserGiList->GetNumTaxIds() > 0)) {
               		have_memb = s_IncludeDefline_Taxid(defline, m_UserGiList->GetTaxIdsList());
            	}

            	if (!have_memb && !m_VolumeGiLists.empty()) {
               		NON_CONST_ITERATE(TGiLists, vtaxid, m_VolumeGiLists) {
               			if( (*vtaxid)->GetNumTaxIds() > 0) {
               				have_memb = s_IncludeDefline_Taxid(defline, (*vtaxid)->GetTaxIdsList());
               				if(have_memb){
               					break;
               				}
               			}
               		}
            	}

            	if(have_memb && (!m_NegativeList.Empty()) && (m_NegativeList->GetNumTaxIds() > 0)) {
               		have_memb = s_IncludeDefline_NegativeTaxid(defline, m_NegativeList->GetTaxIdsList());
            	}

            	if(have_memb && (m_OidMaskType != 0)) {
               		have_memb = s_IncludeDefline_MaskFilter(defline, m_OidMaskType);
            	}
            }

            if (! have_memb) {
                TBDLLIter eraseme = iter++;
                dl.erase(eraseme);
                asn_changed = true;
            } else {
                iter++;
            }
        }
    }

    if (useCache)
    {
    	if (asn_changed) {
       	 cached.first = BDLS;
       	 cached.second = asn_changed;
    	} else {
       	 cached.first = BDLS;
       	 cached.second = asn_changed;
    	}
    }

    return BDLS;
}

CRef<CBlast_def_line_set>
CSeqDBVol::x_GetFilteredHeader(int                    oid,
                               bool                 * changed,
			       CObjectIStreamAsnBinary  *inpstr ) const
{
    typedef list< CRef<CBlast_def_line> > TBDLL;
    typedef TBDLL::iterator               TBDLLIter;

    //m_Atlas.Lock(locked);

    const bool useCache = (CThread::GetSelf() == 0 ? true : false);
    TDeflineCacheItem & cached = m_DeflineCache.Lookup(oid);

    if (useCache && cached.first.NotEmpty()) {
        if (changed) {
            *changed = cached.second;
        }

        return cached.first;
    }

    bool asn_changed = false;

    CRef<CBlast_def_line_set> BDLS =
        x_GetHdrAsn1(oid, true, & asn_changed, inpstr);

    bool id_filter = x_HaveIdFilter();

    if (id_filter || m_MemBit || m_OidMaskType) {
        // Create the memberships mask (should this be fixed to allow
        // membership bits greater than 32?)

        TBDLL & dl = BDLS->Set();

        for(TBDLLIter iter = dl.begin(); iter != dl.end(); ) {
            const CBlast_def_line & defline = **iter;

            bool have_memb = true;

            if (m_MemBit) {
                have_memb =
                    defline.CanGetMemberships() &&
                    defline.IsSetMemberships() &&
                    (! defline.GetMemberships().empty());

                if (have_memb) {
                    int bits = defline.GetMemberships().front();
                    int memb_mask = 0x1 << (m_MemBit-1);

                    if ((bits & memb_mask) == 0) {
                        have_memb = false;
                    }
                }
            }

            // Here we must pass both the user-gi and volume-gi test,
            // for each defline, but not necessarily for each Seq-id.
            if (have_memb) {
            	if (id_filter && defline.CanGetSeqid()) {
            		have_memb = false;
            		bool have_user = false, have_volume = false;
            		ITERATE(list< CRef<CSeq_id> >, seqid, defline.GetSeqid()) {
            			x_FilterHasId(**seqid, have_user, have_volume);
            			if (have_user && have_volume) break;
            		}
            		have_memb = have_user && have_volume;
            	}

            	if(have_memb && (!m_UserGiList.Empty()) && (m_UserGiList->GetNumTaxIds() > 0)) {
               		have_memb = s_IncludeDefline_Taxid(defline, m_UserGiList->GetTaxIdsList());
            	}

            	if (!have_memb && !m_VolumeGiLists.empty()) {
               		NON_CONST_ITERATE(TGiLists, vtaxid, m_VolumeGiLists) {
               			if( (*vtaxid)->GetNumTaxIds() > 0) {
               				have_memb = s_IncludeDefline_Taxid(defline, (*vtaxid)->GetTaxIdsList());
               				if(have_memb){
               					break;
               				}
               			}
               		}
            	}

            	if(have_memb && (!m_NegativeList.Empty()) && (m_NegativeList->GetNumTaxIds() > 0)) {
               		have_memb = s_IncludeDefline_NegativeTaxid(defline, m_NegativeList->GetTaxIdsList());
            	}

            	if(have_memb && (m_OidMaskType != 0)) {
               		have_memb = s_IncludeDefline_MaskFilter(defline, m_OidMaskType);
            	}

            }

            if (! have_memb) {
                TBDLLIter eraseme = iter++;
                dl.erase(eraseme);
                asn_changed = true;
            } else {
                iter++;
            }
        }
    }

    if (useCache)
    {
    	if (asn_changed) {
       	 cached.first = BDLS;
       	 cached.second = asn_changed;
    	} else {
       	 cached.first = BDLS;
       	 cached.second = asn_changed;
    	}
    }

    return BDLS;
}
CRef<CBlast_def_line_set>
CSeqDBVol::x_GetHdrAsn1(int              oid,
                        bool             adjust_oids,
                        bool           * changed) const
{
    CRef<CBlast_def_line_set> bdls;

    CTempString raw_data = x_GetHdrAsn1Binary(oid);

    if (! raw_data.size()) {
        return bdls;
    }

    // Now create an ASN.1 object from the memory chunk provided here.

    CObjectIStreamAsnBinary inpstr(raw_data.data(), raw_data.size());

    bdls.Reset(new objects::CBlast_def_line_set);

    inpstr >> *bdls;

    if (adjust_oids && bdls.NotEmpty() && m_VolStart) {
        NON_CONST_ITERATE(list< CRef<CBlast_def_line> >, dl, bdls->Set()) {
            if (! (**dl).CanGetSeqid()) {
                continue;
            }

            NON_CONST_ITERATE(list< CRef<CSeq_id> >, id, (*dl)->SetSeqid()) {
                CSeq_id & seqid = **id;

                if (seqid.Which() == CSeq_id::e_General) {
                    CDbtag & dbt = seqid.SetGeneral();

                    if (dbt.GetDb() == "BL_ORD_ID") {
                        int vol_oid = dbt.GetTag().GetId();
                        dbt.SetTag().SetId(m_VolStart + vol_oid);
                        if (changed) {
                            *changed = true;
                        }
                    }
                }
            }
        }
    }

    return bdls;
}

CRef<CBlast_def_line_set>
CSeqDBVol::x_GetHdrAsn1(int              oid,
                        bool             adjust_oids,
                        bool           * changed,
			CObjectIStreamAsnBinary *inpstr ) const
{
    CRef<CBlast_def_line_set> bdls;

    CTempString raw_data = x_GetHdrAsn1Binary(oid);

    if (! raw_data.size()) {
        return bdls;
    }

    /*
     * FixNonPrint(how);
     * ResetThisState();
     * OpenFromBuffer(buffer, size);
     */
    inpstr->Close(); // insted of ResetThisState
    inpstr->OpenFromBuffer( raw_data.data(), raw_data.size());

    bdls.Reset(new objects::CBlast_def_line_set);

    (*inpstr)  >> *bdls; // NEW METHOD

    if (adjust_oids && bdls.NotEmpty() && m_VolStart) {
        NON_CONST_ITERATE(list< CRef<CBlast_def_line> >, dl, bdls->Set()) {
            if (! (**dl).CanGetSeqid()) {
                continue;
            }

            NON_CONST_ITERATE(list< CRef<CSeq_id> >, id, (*dl)->SetSeqid()) {
                CSeq_id & seqid = **id;

                if (seqid.Which() == CSeq_id::e_General) {
                    CDbtag & dbt = seqid.SetGeneral();

                    if (dbt.GetDb() == "BL_ORD_ID") {
                        int vol_oid = dbt.GetTag().GetId();
                        dbt.SetTag().SetId(m_VolStart + vol_oid);
                        if (changed) {
                            *changed = true;
                        }
                    }
                }
            }
        }
    }

    return bdls;
}

CTempString
CSeqDBVol::x_GetHdrAsn1Binary(int oid) const
{
    TIndx hdr_start = 0;
    TIndx hdr_end   = 0;

    //m_Atlas.Lock(locked);

    if (!m_HdrFileOpened) x_OpenHdrFile();

    m_Idx->GetHdrStartEnd(oid, hdr_start, hdr_end);    
    const char * asn_region = m_Hdr->GetFileDataPtr(hdr_start);

    return CTempString(asn_region, hdr_end - hdr_start);
}

void
CSeqDBVol::x_GetFilteredBinaryHeader(int                    oid,
                                     vector<char>         & hdr_data ) const
{
    // This method's client is GetBioseq() and related methods.  That
    // code needs filtered ASN.1 headers; eliminating the fetching of
    // filtered data here is not necessary (the cache will hit.)

    // If the data changed after deserialization, we need to serialize
    // the modified version.  If not, we can just copy the binary data
    // from disk.

    bool changed = false;

    CRef<CBlast_def_line_set> dls =
        x_GetFilteredHeader(oid,  & changed);

    if (changed) {
        CNcbiOstrstream asndata;

        {{
            CObjectOStreamAsnBinary outpstr(asndata);
            outpstr << *dls;
        }}
        string s = CNcbiOstrstreamToString(asndata);
        hdr_data.assign(s.data(), s.data() + s.size());
    } else {
        CTempString raw = x_GetHdrAsn1Binary(oid);
        hdr_data.assign(raw.data(), raw.data() + raw.size());
    }
}

void CSeqDBVol::x_GetAmbChar(int              oid,
                             vector<Int4>   & ambchars) const
{
    TIndx start_offset = 0;
    TIndx end_offset   = 0;

    //m_Atlas.Lock(locked);

    bool ok = m_Idx->GetAmbStartEnd(oid, start_offset, end_offset);

    if (! ok) {
        NCBI_THROW(CSeqDBException, eFileErr,
                   "File error: could not get ambiguity data.");
    }

    int length = int(end_offset - start_offset);

    if (length) {
        int total = length / 4;

        // 'hold' should be false here because we only need the data
        // for the duration of this function.        
        Int4 * buffer =
            (Int4*) m_Seq->GetFileDataPtr(start_offset);                                     

        // This is probably unnecessary
        total &= 0x7FFFFFFF;

        ambchars.resize(total);

        for(int i = 0; i<total; i++) {
            ambchars[i] = SeqDB_GetStdOrd((const int *)(& buffer[i]));
        }
    } else {
        ambchars.clear();
    }
}

int CSeqDBVol::GetNumOIDs() const
{
    return m_Idx->GetNumOIDs();
}

string CSeqDBVol::GetTitle() const
{
    return m_Idx->GetTitle();
}

string CSeqDBVol::GetDate() const
{
    return m_Idx->GetDate();
}

int CSeqDBVol::GetMaxLength() const
{
    return m_Idx->GetMaxLength();
}

int CSeqDBVol::GetMinLength() const
{
    return m_Idx->GetMinLength();
}

bool CSeqDBVol::PigToOid(int pig, int & oid) const
{
	bool rv = false;
    x_OpenPigFile();
    if (m_IsamPig.NotEmpty()) {
    	rv = m_IsamPig->PigToOid(pig, oid);
    	x_UnleasePigFile();
    }
    return rv;
}

bool CSeqDBVol::GetPig(int oid, int & pig, CSeqDBLockHold & locked) const
{
    pig = -1;

    CRef<CBlast_def_line_set> BDLS = x_GetHdrAsn1(oid, false, NULL);

    if (BDLS.Empty() || (! BDLS->IsSet())) {
        return false;
    }

    typedef list< CRef< CBlast_def_line > >::const_iterator TI1;
    typedef list< int >::const_iterator TI2;

    TI1 it1 = BDLS->Get().begin();

    for(; it1 != BDLS->Get().end();  it1++) {
        if ((*it1)->IsSetOther_info()) {
            TI2 it2 = (*it1)->GetOther_info().begin();
            TI2 it2end = (*it1)->GetOther_info().end();

            for(; it2 != it2end;  it2++) {
                if (*it2 != -1) {
                    pig = *it2;
                    return true;
                }
            }
        }
    }

    return false;
}

bool CSeqDBVol::TiToOid(Int8                   ti,
                        int                  & oid,
                        CSeqDBLockHold       & locked) const
{
    // Note: this is the (Int8 to int) interface layer; code below
    // this point (in the call stack) uses int; code above this level,
    // up to the user level, uses Int8.

    x_OpenTiFile();
    if (m_IsamTi.Empty()) {
        // If the "nti/ntd" files become ubiquitous, this could be
        // removed.  For now, I will look up trace IDs in the string
        // DB if the database in question does not have the Trace ID
        // ISAM files.  (The following could be made more efficient.)

        CSeq_id seqid(string("gnl|ti|") + NStr::Int8ToString(ti));
        vector<int> oids;

        SeqidToOids(seqid, oids,  locked);

        if (oids.size()) {
            oid = oids[0];
        }
        return ! oids.empty();
    }
    else {
    	bool rv = false;
    	rv = m_IsamTi->IdToOid(ti, oid);
    	x_UnleaseTiFile();
    	return rv;
    }
}

bool CSeqDBVol::GiToOid(TGi gi, int & oid, CSeqDBLockHold & locked) const
{
	bool rv = false;
     x_OpenGiFile();
    if (m_IsamGi.Empty()) {
        return false;
    }
    rv = m_IsamGi->IdToOid(GI_TO(Int8, gi), oid);
    x_UnleaseGiFile();
    return rv;
}

void CSeqDBVol::IdsToOids(CSeqDBGiList   & ids,
                          CSeqDBLockHold & locked) const
{
    if (ids.GetNumGis()) {
         x_OpenGiFile();
        if (m_IsamGi.NotEmpty()) {
            m_IsamGi->IdsToOids(m_VolStart, m_VolEnd, ids);            
            x_UnleaseGiFile();
        } else {
            NCBI_THROW(CSeqDBException,
                       eArgErr,
                       "GI list specified but no ISAM file found for GI in " + m_VolName);
        }
    }

    if (ids.GetNumTis()) {
        x_OpenTiFile();
        if (m_IsamTi.NotEmpty()) {
            m_IsamTi->IdsToOids(m_VolStart, m_VolEnd, ids);
            x_UnleaseTiFile();
        } else {
            NCBI_THROW(CSeqDBException,
                       eArgErr,
                       "TI list specified but no ISAM file found for TI in " + m_VolName);
        }
    }

    if (ids.GetNumPigs()) {
        x_OpenPigFile();
        if (m_IsamPig.NotEmpty()) {
            m_IsamPig->IdsToOids(m_VolStart, m_VolEnd, ids);
            x_UnleasePigFile();
        } else {
            NCBI_THROW(CSeqDBException,
                       eArgErr,
                       "IPG list specified but no ISAM file found for IPG in " + m_VolName);
        }
    }

    if (ids.GetNumSis() && (GetLMDBFileName() == kEmptyStr)) {
        x_OpenStrFile();
        if (m_IsamStr.NotEmpty()) {
            m_IsamStr->IdsToOids(m_VolStart, m_VolEnd, ids);
            x_UnleaseStrFile();
        } else {
            NCBI_THROW(CSeqDBException,
                       eArgErr,
                       "SI list specified but no ISAM file found for SI in " + m_VolName);
        }
    }
}

void CSeqDBVol::IdsToOids(CSeqDBNegativeList & ids,
                          CSeqDBLockHold     & locked) const
{
    // Numeric translation is done in batch mode.

    if (ids.GetNumGis()) {
         x_OpenGiFile();
        if (m_IsamGi.NotEmpty()) {
            m_IsamGi->IdsToOids(m_VolStart, m_VolEnd, ids);
            x_UnleaseGiFile();
        } else {
            NCBI_THROW(CSeqDBException,
                       eArgErr,
                       "GI list specified but no ISAM file found for GI in " + m_VolName);
        }
    }

    if (ids.GetNumTis()) {
        x_OpenTiFile();
        if (m_IsamTi.NotEmpty()) {
            m_IsamTi->IdsToOids(m_VolStart, m_VolEnd, ids);
            x_UnleaseTiFile();
        } else {
            NCBI_THROW(CSeqDBException,
                       eArgErr,
                       "TI list specified but no ISAM file found for TI in " + m_VolName);
        }
    }

    if (ids.GetNumSis()) {
        x_OpenStrFile();
        if (m_IsamStr.NotEmpty()) {
            m_IsamStr->IdsToOids(m_VolStart, m_VolEnd, ids);
            x_UnleaseStrFile();
        } else {
            NCBI_THROW(CSeqDBException,
                       eArgErr,
                       "SI list specified but no ISAM file found for SI in " + m_VolName);
        }
    }
}

bool CSeqDBVol::GetGi(int                    oid,
                      TGi                  & gi,
                      CSeqDBLockHold       & locked) const
{
    gi = INVALID_GI;

    if (!CSeqDBIsam::IndexExists(m_VolName, (m_IsAA?'p':'n'), 'n')) {
        return false;
    }

    CRef<CBlast_def_line_set> BDLS =
        x_GetFilteredHeader(oid,  NULL);

    if (BDLS.Empty() || (! BDLS->IsSet())) {
        return false;
    }

    typedef list< CRef< CBlast_def_line > >::const_iterator TI1;
    typedef list< CRef< CSeq_id > >::const_iterator TI2;

    TI1 it1 = BDLS->Get().begin();

    // Iterate over all blast def lines in the set

    for(; it1 != BDLS->Get().end();  it1++) {
        if ((*it1)->CanGetSeqid()) {
            TI2 it2 = (*it1)->GetSeqid().begin();
            TI2 it2end = (*it1)->GetSeqid().end();

            // Iterate within each defline

            for(; it2 != it2end;  it2++) {
                if ((*it2)->IsGi()) {
                    gi = (*it2)->GetGi();
                    return true;
                }
            }
        }
    }

    return false;
}

void CSeqDBVol::x_StringToOids(const string          & acc,
                               ESeqDBIdType            id_type,
                               Int8                    ident,
                               const string          & str_id,
                               bool                    simpler,
                               vector<int>           & oids) const
{
    bool vcheck (false);
    bool fits_in_four = (ident == -1) || ! (ident >> 32);
    bool needs_four = true;

    switch(id_type) {
    case eStringId:
        x_OpenStrFile();
        if (! m_IsamStr.Empty()) {
            // Not simplified
            vcheck = true;
            m_IsamStr->StringToOids(str_id, oids, simpler, vcheck);
            x_UnleaseStrFile();
        }
        break;

    case ePigId:
        // Converted to PIG type.
        x_OpenPigFile();
        if (! m_IsamPig.Empty()) {
            int oid(-1);

            if (m_IsamPig->PigToOid((int) ident, oid)) {
                oids.push_back(oid);
            }
            x_UnleasePigFile();
        }
        break;

    case eGiId:
        // Converted to GI type.
        x_OpenGiFile();
        if (! m_IsamGi.Empty()) {
            int oid(-1);

            if (m_IsamGi->IdToOid(ident, oid)) {
                oids.push_back(oid);
            }
            x_UnleaseGiFile();
        }
        break;

    case eTiId:
        // Converted to TI type.
         x_OpenTiFile();
        if (! m_IsamTi.Empty()) {
            int oid(-1);

            if (m_IsamTi->IdToOid(ident, oid)) {
                oids.push_back(oid);
            }
            x_UnleaseTiFile();
        }
        else {
        	x_OpenStrFile();
        	if (m_IsamStr.NotEmpty()) {
            	// Not every database with TIs has a TI index, so fall
            	// back to a string comparison if the first attempt fails.
            	//
            	// 1. TI's don't have versions.
            	// 2. Specify "adjusted" as true, because lookup of
            	//    "gb|.." and similar tricks are not needed for TIs.
        		m_IsamStr->StringToOids(acc, oids, true, vcheck);
        		x_UnleaseStrFile();
        	}
        }
        break;

    case eOID:
        // Converted to OID directly.
        oids.push_back((int) ident);
        break;

    case eHashId:
        _ASSERT(0);
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Internal error: hashes are not Seq-ids.");
    }

    if ((! fits_in_four) && needs_four) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "ID overflows range of specified type.");
    }

    if (vcheck) {
        x_CheckVersions(acc, oids);
    }
}

void CSeqDBVol::x_CheckVersions(const string         & acc,
                                vector<int>          & oids) const
{
    // If we resolved a string id by stripping the version off of the
    // string, we need to check (for each OID) if the real ID had a
    // matching version.

    // This condition happens in two cases: where the database has a
    // different version of the same ID, and in the case of sparse
    // databases (which do not store the version).  In the latter
    // case, we may get a list of OIDs where some of the OIDs pass
    // this test, and others fail.

    size_t pos = acc.find(".");
    _ASSERT(pos != acc.npos);

    string ver_str(acc, pos+1, acc.size()-(pos+1));
    int vernum = NStr::StringToInt(ver_str,
                                   NStr::fConvErr_NoThrow |
                                   NStr::fAllowTrailingSymbols);

    string nover(acc, 0, pos);

    size_t pos2(0);
    while((pos2 = nover.find("|")) != nover.npos) {
        nover.erase(0, pos2+1);
    }

    NON_CONST_ITERATE(vector<int>, iter, oids) {
        list< CRef<CSeq_id> > ids =
            GetSeqIDs(*iter);

        bool found = false;

        ITERATE(list< CRef<CSeq_id> >, seqid, ids) {
            const CTextseq_id * id = (*seqid)->GetTextseq_Id();

            if (id &&
                id->CanGetAccession() &&
                id->GetAccession() == nover &&
                id->CanGetVersion() &&
                id->GetVersion() == vernum) {

                found = true;
                break;
            }
        }

        if (! found) {
            *iter = -1;
        }
    }

    oids.erase(remove(oids.begin(), oids.end(), -1), oids.end());
}

void CSeqDBVol::AccessionToOids(const string         & acc,
                                vector<int>          & oids,
                                CSeqDBLockHold       & locked) const
{
    Int8   ident   (-1);
    string str_id;
    bool   simpler (false);

    ESeqDBIdType id_type = SeqDB_SimplifyAccession(acc, ident, str_id, simpler);

    x_StringToOids(acc, id_type, ident, str_id, simpler, oids);

}

void CSeqDBVol::SeqidToOids(CSeq_id              & seqid,
                            vector<int>          & oids,
                            CSeqDBLockHold       & locked) const
{
    Int8   ident   (-1);
    string str_id;
    bool   simpler (false);

    ESeqDBIdType id_type = SeqDB_SimplifySeqid(seqid, 0, ident, str_id, simpler);

    x_StringToOids(seqid.AsFastaString(), id_type, ident, str_id, simpler, oids);

}

void CSeqDBVol::UnLease()
{
    m_Idx->UnLease();

    if (m_Seq.NotEmpty()) {
        m_Seq->UnLease();
    }
    if (m_Hdr.NotEmpty()) {
        m_Hdr->UnLease();
    }
    if (m_IsamPig.NotEmpty()) {
        m_IsamPig->UnLease();
    }
    if (m_IsamGi.NotEmpty()) {
        m_IsamGi->UnLease();
    }
    if (m_IsamStr.NotEmpty()) {
        m_IsamStr->UnLease();
    }
}

int CSeqDBVol::GetOidAtOffset(int              first_seq,
                              Uint8            residue,
                              CSeqDBLockHold & locked) const
{
    // This method compensates for representation in two ways.
    //
    // 1. For protein, we subtract the oid to compensate for
    // inter-sequence nulls.
    //
    // 2. For nucleotide, the input value is 0..(num residues).  We
    // scale this value to the length of the byte data.

    int   vol_cnt = GetNumOIDs();
    Uint8 vol_len = GetVolumeLength();

    if (first_seq >= vol_cnt) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "OID not in valid range.");
    }

    if (residue >= vol_len) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Residue offset not in valid range.");
    }

    if ('n' == m_Idx->GetSeqType()) {
        // Input range is from 0 .. total_length
        // Require range from  0 .. byte_length

        Uint8 end_of_bytes = x_GetSeqResidueOffset(vol_cnt);

        double dresidue = (double(residue) * end_of_bytes) / vol_len;

        if (dresidue < 0) {
            residue = 0;
        } else {
            residue = Uint8(dresidue);

            if (residue > (end_of_bytes-1)) {
                residue = end_of_bytes - 1;
            }
        }
    }

    // First seq limit handled right here.
    // oid_end refers to first disincluded oid.

    int oid_beg = first_seq;
    int oid_end = vol_cnt-1;

    // Residue limit we need to search for.

    int oid_mid = (oid_beg + oid_end)/2;

    while(oid_beg < oid_end) {
        Uint8 offset = x_GetSeqResidueOffset(oid_mid);

        if ('p' == m_Idx->GetSeqType()) {
            offset -= oid_mid;
        }

        if (offset >= residue) {
            oid_end = oid_mid;
        } else {
            oid_beg = oid_mid + 1;
        }

        oid_mid = (oid_beg + oid_end)/2;
    }

    return oid_mid;
}

Uint8 CSeqDBVol::x_GetSeqResidueOffset(int oid) const
{
    //m_Atlas.Lock(locked);

    TIndx start_offset = 0;
    m_Idx->GetSeqStart(oid, start_offset);
    return start_offset;
}

CRef<CSeq_data>
CSeqDBVol::GetSeqData(int              oid,
                      TSeqPos          begin,
                      TSeqPos          end,
                      CSeqDBLockHold & locked) const
{
    // This design was part of the BlastDbDataLoader code.

    //m_Atlas.Lock(locked);

    if (!m_SeqFileOpened) x_OpenSeqFile();

    CRef<CSeq_data> seq_data(new CSeq_data);

    if (m_IsAA) {
        const char * buffer(0);
        TSeqPos      length(0);

        length = x_GetSequence(oid, & buffer);

        if ((begin >= end) || (end > length)) {
            NCBI_THROW(CSeqDBException,
                       eArgErr,
                       "Begin and end offsets are not valid.");
        }

        seq_data->SetNcbistdaa().Set().assign(buffer + begin, buffer + end);
    } else {
        // This code builds an array and packs the output in 4 bit
        // format for NA.  No attempt is made to find an optimal
        // packing for the data.

        int nucl_code(kSeqDBNuclNcbiNA8);

        SSeqDBSlice slice(begin, end);

        char    * buffer(0);
        TSeqPos   length(0);

        length = x_GetAmbigSeq(oid,
                               & buffer,
                               nucl_code,
                               eNew,
                               & slice,
                               NULL);

        // validity of begin, end, and length has already been checked by
        // overloaded x_GetSequence()
        // note: length has been redefined to be end - begin

        vector<char> v4;
        v4.reserve((length+1)/2);

        // (this is an attempt to stop a warning message.)
        TSeqPos length_whole = TSeqPos(length & (TSeqPos(0)-TSeqPos(2)));

        for(TSeqPos i = 0; i < length_whole; i += 2) {
            v4.push_back((buffer[i] << 4) | buffer[i+1]);
        }

        if (length_whole != length) {
            _ASSERT((length_whole) == (length-1));
            v4.push_back(buffer[length_whole] << 4);
        }

        seq_data->SetNcbi4na().Set().swap(v4);
        delete [] buffer;
    }

    return seq_data;
}

void
CSeqDBVol::GetRawSeqAndAmbig(int              oid,
                             const char    ** buffer,
                             int            * seq_length,
                             int            * amb_length ) const
{
    if (seq_length)
        *seq_length = 0;

    if (amb_length)
        *amb_length = 0;

    if (buffer)
        *buffer = 0;

    TIndx start_S   = 0;
    TIndx end_S     = 0;
    TIndx start_A   = 0;
    TIndx end_A     = 0;
    TIndx map_begin = 0;
    TIndx map_end   = 0;

    //m_Atlas.Lock(locked);
    if (!m_SeqFileOpened) x_OpenSeqFile();

    m_Idx->GetSeqStartEnd(oid, start_S, end_S);
    bool amb_ok = true;

    if (m_IsAA) {
        // No ambiguities in protein dbs, but there is a NUL between
        // sequences, so we subtract one to remove that.

        end_A = start_A = --end_S;

        _ASSERT(start_S > 0);

        map_begin = start_S - 1;
        map_end   = end_A + 1;
    } else {
        amb_ok = m_Idx->GetAmbStartEnd(oid, start_A, end_A);

        map_begin = start_S;
        map_end   = end_A;
    }

    int s_len = int(end_S - start_S);
    int a_len = int(end_A - start_A);

    if (! (s_len && amb_ok)) {
        NCBI_THROW(CSeqDBException, eFileErr,
                   "File error: could not get sequence data.");
    }

    if (amb_length) {
        *amb_length = a_len;
    }

    if (seq_length) {
        *seq_length = s_len;
    }

    if (buffer) {        
        *buffer = m_Seq->GetFileDataPtr(map_begin);
        *buffer += (start_S - map_begin);
    }

    if (buffer && *buffer) {
        if (! *seq_length) {
            NCBI_THROW(CSeqDBException,
                       eArgErr,
                       "Could not get sequence data.");
        }
    } else {
        if (((buffer && *buffer) || a_len) && (! *seq_length)) {
            NCBI_THROW(CSeqDBException, eArgErr, CSeqDB::kOidNotFound);
        }
    }
}

template<class T>
static void
s_SeqDBFitsInFour(T id)
{
    if (id >= (static_cast<T>(1) << 32)) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "ID overflows range of specified type.");
    }
}

void CSeqDBVol::GetGiBounds(TGi            & low_id,
                            TGi            & high_id,
                            int            & count,
                            CSeqDBLockHold & locked) const
{
    //m_Atlas.Lock(locked);
    x_OpenGiFile();
    low_id = ZERO_GI;
    high_id = ZERO_GI;
    count = 0;

    if (m_IsamGi.NotEmpty()) {
        Int8 L(0), H(0);

        m_IsamGi->GetIdBounds(L, H, count);

        low_id = GI_FROM(Int8, L);
        high_id = GI_FROM(Int8, H);

        s_SeqDBFitsInFour(L);
        s_SeqDBFitsInFour(H);
        x_UnleaseGiFile();
    }
}

void CSeqDBVol::GetPigBounds(int            & low_id,
                             int            & high_id,
                             int            & count,
                             CSeqDBLockHold & locked) const
{
    //m_Atlas.Lock(locked);
    x_OpenPigFile();
    low_id = high_id = count = 0;

    if (m_IsamPig.NotEmpty()) {
        Int8 L(0), H(0);

        m_IsamPig->GetIdBounds(L, H, count);

        low_id = (int) L;
        high_id = (int) H;

        s_SeqDBFitsInFour(L);
        s_SeqDBFitsInFour(H);
        x_UnleasePigFile();
    }
}

void CSeqDBVol::GetStringBounds(string         & low_id,
                                string         & high_id,
                                int            & count) const
{
    x_OpenStrFile();
    count = 0;
    low_id.erase();
    high_id.erase();

    if (m_IsamStr.NotEmpty()) {
        m_IsamStr->GetIdBounds(low_id, high_id, count);
        x_UnleaseStrFile();
    }
}

void CSeqDBVol::SetOffsetRanges(int                oid,
                                const TRangeList & offset_ranges,
                                bool               append_ranges,
                                bool               cache_data) const
{

	 CFastMutexGuard mtx_gurad(m_MtxCachedRange);
    if (offset_ranges.empty() && (! cache_data) && (! append_ranges)) {
        // Specifying no-cache plus an empty offset range list, means
        // that we are clearing out this sequence.  In this case, just
        // free the relevant element and be done.

        m_RangeCache.erase(oid);
        return;
    }

    // This adds the range cache object to the map.

    CRef<CSeqDBRangeList> & R = m_RangeCache[oid];

    if (R.Empty() || R->GetRanges().empty()) {
        // In this case, we are disabling caching, and no ranges
        // exist.  There is nothing to do, and no need to keep the
        // element around, so once again we erase + exit.

        if (offset_ranges.empty() && (! cache_data)) {
            m_RangeCache.erase(oid);
            return;
        }

        if (R.Empty()) {
            R.Reset(new CSeqDBRangeList());
        }
    }

    // We should flush the sequence if:
    //
    // 1. We are not keeping the old ranges (1).
    // 2. There are new ranges to add (2).
    // 3. We are clearing the 'cache data' flag.

    bool flush_sequence = ((! append_ranges) ||         // (1)
                           (! offset_ranges.empty()) || // (2)
                           (! cache_data));             // (3)

    if (flush_sequence) {
        R->FlushSequence();
    }

    R->SetRanges(offset_ranges, append_ranges, cache_data);
}

void CSeqDBVol::FlushOffsetRangeCache()
{
	CFastMutexGuard mtx_gurad(m_MtxCachedRange);
    m_RangeCache.clear();
}

void CSeqDBRangeList::SetRanges(const TRangeList & offset_ranges,
                                bool               append_ranges,
                                bool               cache_data)
{
    if (append_ranges) {
        m_Ranges.insert(offset_ranges.begin(), offset_ranges.end());
    } else {
        m_Ranges = offset_ranges;
    }

    // Note that actual caching is not currently done.
    m_CacheData = cache_data;
}

void CSeqDBVol::OptimizeGiLists() const
{
    if (m_UserGiList.Empty() ||
        m_VolumeGiLists.empty() ||
        m_UserGiList->GetNumSis() ||
        m_UserGiList->GetNumTis()) {

        return;
    }

    NON_CONST_ITERATE(TGiLists, gilist, m_VolumeGiLists) {
        if ((**gilist).GetNumSis() != 0)
            return;

        if ((**gilist).GetNumTis() != 0)
            return;
    }

    // If we have volume GI lists, and a user gi list, and neither of
    // these uses Seq-ids, then we can detach the user gi list (from
    // the volume) because it is redundant with the volume GI lists.
    // The opposite is not true -- we could not simply remove the
    // volume GI lists and rely on the user gi list.  This is because
    // each volume GI list is translated in terms of the user GI list,
    // which means that only the intersection of the two lists is left
    // in the volume GI list.

    m_UserGiList.Reset();
}

void CSeqDBVol::HashToOids(unsigned         hash,
                           vector<int>    & oids,
                           CSeqDBLockHold & locked) const
{
    // It has not been decided whether sequence hash lookups are of
    // long term interest or whether standard databases will be built
    // with these indices, but it should not cause any harm to support
    // them for databases where the files do exist.

    // Since it is normal for a hash lookup to fail (the user of this
    // feature generally does not know if the sequence will be found),
    // the lack of hash indexing is reported by throwing an exception.

    if (!m_HashFileOpened) x_OpenHashFile();
    if (m_IsamHash.Empty()) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Hash lookup requested but no hash ISAM file found.");
    }

    m_IsamHash->HashToOids(hash, oids);
}

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
void CSeqDBVol::GetColumnBlob(int              col_id,
                              int              oid,
                              CBlastDbBlob   & blob,
                              bool             keep,
                              CSeqDBLockHold & locked)
{
    if (! m_HaveColumns) {
        x_OpenAllColumns(locked);
    }

    if ((int)m_Columns.size() == 0 || m_Columns[col_id].Empty())
	return;

    _ASSERT(col_id >= 0);

    m_Columns[col_id]->GetBlob(oid, blob, keep, & locked);
}

const map<string,string> &
CSeqDBVol::GetColumnMetaData(int              col_id,
                             CSeqDBLockHold & locked)
{
    //m_Atlas.Lock(locked);

    if (! m_HaveColumns) {
        x_OpenAllColumns(locked);
    }

    _ASSERT(col_id >= 0);
    _ASSERT(col_id < (int)m_Columns.size());
    _ASSERT(m_Columns[col_id].NotEmpty());

    return m_Columns[col_id]->GetMetaData();
}

void CSeqDBVol::ListColumns(set<string>    & titles,
                            CSeqDBLockHold & locked)
{
    //m_Atlas.Lock(locked);

    if (! m_HaveColumns) {
        x_OpenAllColumns(locked);
    }

    ITERATE(vector< CRef<CSeqDBColumn> >, iter, m_Columns) {
        titles.insert((**iter).GetTitle());
    }
}

void CSeqDBVol::x_OpenAllColumns(CSeqDBLockHold & locked)
{
    //m_Atlas.Lock(locked);

    if (m_HaveColumns) {
        return;
    }

    string alpha("abcdefghijklmnopqrstuvwxyz");
    string ei("??a"), ed("??b"), ed2("??c");

    ei[0] = ed[0] = ed2[0] = (m_IsAA ? 'p' : 'n');

    map<string,int> unique_titles;

    for(size_t i = 0; i < alpha.size(); i++) {
        ei[1] = ed[1] = ed2[1] = alpha[i];

        if (CSeqDBColumn::ColumnExists(m_VolName, ei, m_Atlas)) {

            bool big   = CSeqDBColumn::ColumnExists(m_VolName, ed, m_Atlas);
            bool small = CSeqDBColumn::ColumnExists(m_VolName, ed2, m_Atlas);

            if ( ! (big || small)) continue;

            CRef<CSeqDBColumn> col;

            const Int2 bytetest = 0x0011;
            const char * ptr = (const char *) &bytetest;
            if (ptr[0] == 0x11 && small) {
                col.Reset(new CSeqDBColumn(m_VolName, ei, ed2, & locked));
            } else {
                col.Reset(new CSeqDBColumn(m_VolName, ei, ed, & locked));
            }

            string errmsg, errarg;

            string title = col->GetTitle();

            if (unique_titles[title]) {
                errmsg = "duplicate column title";
                errarg = title;
            } else {
                unique_titles[title] = 1;
            }

            int noidc(col->GetNumOIDs()), noidv(m_Idx->GetNumOIDs());

            if (noidc != noidv) {
                errmsg = "column has wrong #oids";
                errarg = NStr::IntToString(noidc) + " vs "
                    + NStr::IntToString(noidv);
            }

            if (errmsg.size()) {
                if (errarg.size()) {
                    errmsg += string(" [") + errarg + "].";
                }
                NCBI_THROW(CSeqDBException, eFileErr,
                           string("Error: ") + errmsg);
            }

            m_Columns.push_back(col);
        }
    }

    m_HaveColumns = true;
}

int CSeqDBVol::GetColumnId(const string   & title,
                           CSeqDBLockHold & locked)
{
    //m_Atlas.Lock(locked);

    if (! m_HaveColumns) {
        x_OpenAllColumns(locked);
    }

    for(size_t i = 0; i < m_Columns.size(); i++) {
        if (m_Columns[i]->GetTitle() == title) {
            return static_cast<int>(i);
        }
    }

    return -1;
}
#endif


END_NCBI_SCOPE

