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
#include "seqdbvol.hpp"
#include "seqdboidlist.hpp"

#include <objects/general/general__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>

#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>
#include <corelib/ncbimtx.hpp>

#include <sstream>

BEGIN_NCBI_SCOPE

CSeqDBVol::CSeqDBVol(CSeqDBAtlas    & atlas,
                     const string   & name,
                     char             prot_nucl,
                     CSeqDBLockHold & locked)
    : m_Atlas   (atlas),
      m_VolName (name),
      m_Idx     (atlas, name, prot_nucl, locked),
      m_Seq     (atlas, name, prot_nucl, locked),
      m_Hdr     (atlas, name, prot_nucl, locked)
{
    // ISAM files are optional
    
    try {
        m_IsamPig =
            new CSeqDBIsam(m_Atlas,
                           name,
                           prot_nucl,
                           'p',
                           CSeqDBIsam::ePig);
    }
    catch(CSeqDBException &) {
    }
    
    try {
        m_IsamGi =
            new CSeqDBIsam(m_Atlas,
                           name,
                           prot_nucl,
                           'n',
                           CSeqDBIsam::eGi);
    }
    catch(CSeqDBException &) {
    }
    
    try {
        m_IsamStr =
            new CSeqDBIsam(m_Atlas,
                           name,
                           prot_nucl,
                           's',
                           CSeqDBIsam::eStringID);
    }
    catch(CSeqDBException &) {
    }
}

char CSeqDBVol::GetSeqType() const
{
    return x_GetSeqType();
}

char CSeqDBVol::x_GetSeqType() const
{
    return m_Idx.GetSeqType();
}

int CSeqDBVol::GetSeqLengthProt(int oid) const
{
    TIndx start_offset = 0;
    TIndx end_offset   = 0;
    
    m_Idx.GetSeqStartEnd(oid, start_offset, end_offset);
    
    _ASSERT(kSeqTypeProt == m_Idx.GetSeqType());
    
    // Subtract one, for the inter-sequence null.
    return int(end_offset - start_offset - 1);
}


// Assumes locked.

int CSeqDBVol::GetSeqLengthExact(int oid) const
{
    TIndx start_offset = 0;
    TIndx end_offset   = 0;
    
    m_Idx.GetSeqStartEnd(oid, start_offset, end_offset);
    
    _ASSERT(m_Idx.GetSeqType() == kSeqTypeNucl);
    
    int whole_bytes = int(end_offset - start_offset - 1);
    
    // The last byte is partially full; the last two bits of
    // the last byte store the number of nucleotides in the
    // last byte (0 to 3).
    
    char amb_char = 0;
    
    m_Seq.ReadBytes(& amb_char, end_offset - 1, end_offset);
    
    int remainder = amb_char & 3;
    return (whole_bytes * 4) + remainder;
}


int CSeqDBVol::GetSeqLengthApprox(int oid) const
{
    TIndx start_offset = 0;
    TIndx end_offset   = 0;
    
    m_Idx.GetSeqStartEnd(oid, start_offset, end_offset);
    
    _ASSERT(m_Idx.GetSeqType() == kSeqTypeNucl);
    
    int whole_bytes = int(end_offset - start_offset - 1);
    
    // Same principle as below - but use lower bits of oid
    // instead of fetching the actual last byte.  this should
    // correct for bias, unless sequence length modulo 4 has a
    // significant statistical bias, which seems unlikely to
    // me.
    
    return (whole_bytes * 4) + (oid & 0x03);
}

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
static vector<Uint1>
s_SeqDBMapNA2ToNA4Setup()
{
    vector<Uint1> translated;
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
    
    return translated;
}

/// Convert sequence data from NA2 to NA4 format
///
/// This uses a translation table to convert nucleotide data.  The
/// input data is in NA2 format, the output data will be in NcbiNA4
/// format.
///
/// @param buf2bit
///    The NA2 input data.
/// @param buf4bit
///    The NcbiNA4 output data.
/// @param base_length
///    The length (in bases) of the input data.
static void
s_SeqDBMapNA2ToNA4(const char   * buf2bit,
                   vector<char> & buf4bit,
                   int            base_length)
{
    static vector<Uint1> expanded = s_SeqDBMapNA2ToNA4Setup();
    
    int estimated_length = (base_length + 1)/2;
    buf4bit.reserve(estimated_length);
    
    int inp_chars = base_length/4;
    
    for(int i=0; i<inp_chars; i++) {
        Uint4 inp_char = (buf2bit[i] & 0xFF);
        
        buf4bit.push_back(expanded[ (inp_char*2)     ]);
        buf4bit.push_back(expanded[ (inp_char*2) + 1 ]);
    }
    
    int bases_remain = base_length - (inp_chars*4);
    
    if (bases_remain) {
        Uint1 remainder_bits = 2 * bases_remain;
        Uint1 remainder_mask = (0xFF << (8 - remainder_bits)) & 0xFF;
        Uint4 last_masked = buf2bit[inp_chars] & remainder_mask;
        
        buf4bit.push_back(expanded[ (last_masked*2) ]);
        
        if (bases_remain > 2) {
            buf4bit.push_back(expanded[ (last_masked*2)+1 ]);
        }
    }
    
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
static vector<Uint1>
s_SeqDBMapNA2ToNA8Setup()
{
    // Builds a table; each two bit slice holds 0,1,2 or 3.  These are
    // converted to whole bytes containing 1,2,4, or 8, respectively.
    
    vector<Uint1> translated;
    translated.reserve(1024);
    
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
    
    return translated;
}

/// Convert sequence data from NA2 to NA8 format
///
/// This uses a translation table to convert nucleotide data.  The
/// input data is in NA2 format, the output data will be in Ncbi-NA8
/// format.  This function also optionally adds sentinel bytes to the
/// start and end of the data (needed by some applications).
///
/// @param buf2bit
///    The NA2 input data
/// @param buf8bit
///    The Ncbi-NA8 output data
/// @param base_length
///    The length (in bases) of the input data
/// @param sentinel_bytes
///    Specify true if sentinel bytes should be included
static void
s_SeqDBMapNA2ToNA8(const char   * buf2bit,
                   vector<char> & buf8bit,
                   int            base_length,
                   bool           sentinel_bytes)
{
    static vector<Uint1> expanded = s_SeqDBMapNA2ToNA8Setup();
    
    int sreserve = 0;
    
    if (sentinel_bytes) {
        sreserve = 2;
        buf8bit.push_back((unsigned char)(0));
    }
    
    buf8bit.reserve(base_length + sreserve);
    
    int whole_input_chars = base_length/4;
    
    // In a nucleotide search, this loop is probably a noticeable time
    // consumer, at least relative to the CSeqDB universe.  Each input
    // byte is used to look up a 4 byte output translation.  That four
    // byte section is copied to the output vector.  By pre-processing
    // the arithmetic in the ~Setup() function, we can just pull bytes
    // from a vector.
    
    for(int i = 0; i < whole_input_chars; i++) {
        int table_offset = (buf2bit[i] & 0xFF) * 4;
        
        buf8bit.push_back(expanded[ table_offset ]);
        buf8bit.push_back(expanded[ table_offset + 1 ]);
        buf8bit.push_back(expanded[ table_offset + 2 ]);
        buf8bit.push_back(expanded[ table_offset + 3 ]);
    }
    
    int bases_remain = base_length & 0x3;
    
    if (bases_remain) {
        // We use the last byte of the input to look up a four byte
        // translation; but we only use the bytes of it that we need.
        
        int table_offset = (buf2bit[whole_input_chars] & 0xFF) * 4;
        
        while(bases_remain--) {
            buf8bit.push_back(expanded[ table_offset++ ]);
        }
    }
    
    if (sentinel_bytes) {
        buf8bit.push_back((unsigned char)(0));
    }
    
    _ASSERT(base_length == int(buf8bit.size() - sreserve));
}

/// Convert sequence data from Ncbi-NA8 to Blast-NA8 format
///
/// This uses a translation table to convert nucleotide data.  The
/// input data is in Ncbi-NA8 format, the output data will be in
/// Blast-NA8 format.  The data is converted in-place.
///
/// @param buf
///    The array of nucleotides to convert
static void
s_SeqDBMapNcbiNA8ToBlastNA8(vector<char> & buf)
{
    Uint1 trans_ncbina8_to_blastna8[] = {
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
    
    for(int i = 0; i < (int)buf.size(); i++) {
        buf[i] = trans_ncbina8_to_blastna8[ (size_t) buf[i] ];
    }
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
///     The packed ambiguity data
/// @param i
///     The index into the ambiguity data
/// @return
///     The region length
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
///     The packed ambiguity data
/// @param i
///     The index into the ambiguity data
/// @return
///     The region length
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
///     The packed ambiguity data
/// @param i
///     The index into the ambiguity data
/// @return
///     The residue value
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
///     The packed ambiguity data
/// @param i
///     The index into the ambiguity data
/// @return
///     The residue value
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
///     The packed ambiguity data
/// @param i
///     The index into the ambiguity data
/// @return
///     The residue value
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
///     Sequence data for a sequence
/// @param amb_chars
///     Corresponding ambiguous data
static void
s_SeqDBRebuildDNA_NA4(vector<char>       & buf4bit,
                      const vector<Int4> & amb_chars)
{
    if (amb_chars.empty()) 
        return;
    
    Uint4 amb_num = amb_chars[0];
    
    // Check if highest order bit set.
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
/// @param buf8bit
///     Sequence data for a sequence
/// @param amb_chars
///     Corresponding ambiguous data
/// @param sentinel
///     True if sentinel bytes are used
static void
s_SeqDBRebuildDNA_NA8(vector<char>       & buf8bit,
                      const vector<Int4> & amb_chars,
                      bool                 sentinel)
{
    if (buf8bit.empty())
        return;
    
    if (amb_chars.empty()) 
        return;
    
    Uint4 amb_num = amb_chars[0];
    
    /* Check if highest order bit set. */
    bool new_format = (amb_num & 0x80000000) != 0;
    
    if (new_format) {
	amb_num &= 0x7FFFFFFF;
    }
    
    Int4 sentinel_adjustment = sentinel ? 1 : 0;
    
    for(Uint4 i = 1; i < amb_num+1; i++) {
        Int4  row_len  = 0;
        Int4  position = 0;
        Uint1 trans_ch = 0;
        
	if (new_format) {
            trans_ch  = s_ResVal   (amb_chars, i);
            row_len   = s_ResLenNew(amb_chars, i); 
            position  = s_ResPosNew(amb_chars, i);
	} else {
            trans_ch  = s_ResVal   (amb_chars, i);
            row_len   = s_ResLenOld(amb_chars, i); 
            position  = s_ResPosOld(amb_chars, i);
	}
        
        Int4 index = position + sentinel_adjustment;
        
        // This could be made slightly faster for long runs.
        
        for(Int4 j = 0; j <= row_len; j++) {
            buf8bit[index++] = trans_ch;
    	}
        
	if (new_format) /* for new format we have 8 bytes for each element. */
            i++;
    }
}

/// Store protein sequence data in a Seq-inst
///
/// This function reads length elements from seq_buffer and stores
/// them in a Seq-inst object.  It also sets appropriate encoding
/// information in that object.
///
/// @param seqinst
///     The Seq-inst to return the data in
/// @param seq_buffer
///     The input sequence data
/// @param length
///     The length (in bases) of the input data
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
///     The Seq-inst to return the data in
/// @param seq_buffer
///     The input sequence data
/// @param length
///     The length (in bases) of the input data
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
///     The Seq-inst to return the data in
/// @param seq_buffer
///     The input sequence data in Ncbi-NA4 format
/// @param length
///     The length (in bases) of the input data
/// @param amb_chars
///     The ambiguity data for this sequence
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
///   The set of deflines for this sequence.
/// @param title
///   The returned title string
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
///     A list of Seq-ids to search
/// @param target
///     The Seq-id to search for
/// @return
///     True if the Seq-id was found
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
CSeqDBVol::x_GetTaxDefline(int              oid,
                           bool             have_oidlist,
                           int              membership_bit,
                           int              preferred_gi,
                           CSeqDBLockHold & locked) const
{
    typedef list< CRef<CBlast_def_line> > TBDLL;
    typedef TBDLL::iterator               TBDLLIter;
    typedef TBDLL::const_iterator         TBDLLConstIter;
    
    // 1. read a defline set w/ gethdr, filtering by membership bit
    
    CRef<CBlast_def_line_set> BDLS =
        x_GetHdrMembBit(oid, have_oidlist, membership_bit, locked);
    
    // 2. if there is a preferred gi, bump it to the top.
    
    if (preferred_gi != 0) {
        CSeq_id seqid(CSeq_id::e_Gi, preferred_gi);
        
        TBDLL & dl = BDLS->Set();
        
        CRef<CBlast_def_line> moved;
        
        for(TBDLLIter iter = dl.begin(); iter != dl.end(); iter++) {
            if (s_SeqDB_SeqIdIn((**iter).GetSeqid(), seqid)) {
                moved = *iter;
                dl.erase(iter);
                break;
            }
        }
        
        if (! moved.Empty()) {
            dl.push_front(moved);
        }
    }
    
    return BDLS;
}

list< CRef<CSeqdesc> >
CSeqDBVol::x_GetTaxonomy(int                   oid,
                         bool                  have_oidlist,
                         int                   membership_bit,
                         int                   preferred_gi,
                         CRef<CSeqDBTaxInfo>   tax_info,
                         CSeqDBLockHold      & locked) const
{
    const bool provide_old_taxonomy_info = false;
    const bool provide_new_taxonomy_info = true;
    const bool use_taxinfo_cache         = false;
    const int  max_taxcache_size         = 200;
    
    const char * TAX_DATA_OBJ_LABEL = "TaxNamesData";
    const char * TAX_ORGREF_DB_NAME = "taxon";
    
    list< CRef<CSeqdesc> > taxonomy;
    
    CRef<CBlast_def_line_set> bdls =
        x_GetTaxDefline(oid,
                        have_oidlist,
                        membership_bit,
                        preferred_gi,
                        locked);
    
    if (bdls.Empty()) {
        return taxonomy;
    }
    
    typedef list< CRef<CBlast_def_line> > TBDLL;
    typedef TBDLL::iterator               TBDLLIter;
    typedef TBDLL::const_iterator         TBDLLConstIter;
    
    const TBDLL & dl = bdls->Get();
    
    CRef<CBioSource>   source;
    CRef<CUser_object> uobj;
    CRef<CObject_id>   uo_oi;
    
    if (provide_new_taxonomy_info) {
        source.Reset(new CBioSource);
    }
    
    if (provide_old_taxonomy_info) {
        uobj  .Reset(new CUser_object);
        uo_oi .Reset(new CObject_id);
        uo_oi->SetStr(TAX_DATA_OBJ_LABEL);
        uobj->SetType(*uo_oi);
    }
    
    bool found = false;
    
    // Lock for sake of tax cache
    
    m_Atlas.Lock(locked);
    
    for(TBDLLConstIter iter = dl.begin(); iter != dl.end(); iter ++) {
        int taxid = (*iter)->GetTaxid();
        
        bool have_org_desc = false;
        
        if (use_taxinfo_cache && m_TaxCache[taxid].NotEmpty()) {
            have_org_desc = true;
        }
        
        CSeqDBTaxNames tnames;
        
        if (tax_info.Empty()) {
            continue;
        }
        
        bool worked = true;
        
        if (provide_old_taxonomy_info || ((! have_org_desc) && provide_new_taxonomy_info)) {
            worked = tax_info->GetTaxNames(taxid, tnames, locked);
        }
        
        if (! worked) {
            continue;
        }
        
        found = true;
        
        if (provide_old_taxonomy_info) {
            CRef<CUser_field> uf(new CUser_field);
            
            CRef<CObject_id> uf_oi(new CObject_id);
            uf_oi->SetId(taxid);
            uf->SetLabel(*uf_oi);
            
            vector<string> & strs = uf->SetData().SetStrs();
            
            uf->SetNum(4);
            strs.resize(4);
            
            strs[0] = tnames.GetSciName();
            strs[1] = tnames.GetCommonName();
            strs[2] = tnames.GetBlastName();
            strs[3] = tnames.GetSKing();
            
            uobj->SetData().push_back(uf);
        }
        
        if (provide_new_taxonomy_info) {
            if (have_org_desc) {
                taxonomy.push_back(m_TaxCache[taxid]);
            } else {
                CRef<CDbtag> org_tag(new CDbtag);
                org_tag->SetDb(TAX_ORGREF_DB_NAME);
                org_tag->SetTag().SetId(taxid);
            
                CRef<COrg_ref> org(new COrg_ref);
                org->SetTaxname(tnames.GetSciName());
                org->SetCommon(tnames.GetCommonName());
                org->SetDb().push_back(org_tag);
                
                source->SetOrg(*org);
                
                CRef<CSeqdesc> desc(new CSeqdesc);
                desc->SetSource(*source);
                
                taxonomy.push_back(desc);
                
                if (use_taxinfo_cache) {
                    // Simple memory usage limitation.  This could
                    // probably be profiled to determine the best max
                    // size to use.  If you use more than 200 or so,
                    // the cache may be hurting more than helping.  It
                    // would be easy to make this more sophisticated.
                    
                    if ((int) m_TaxCache.size() > max_taxcache_size) {
                        m_TaxCache.clear();
                    }
                    
                    m_TaxCache[taxid] = desc;
                }
            }
        }
    }
    
    if (found && provide_old_taxonomy_info) {
        CRef<CSeqdesc> desc(new CSeqdesc);
        desc->SetUser(*uobj);
        taxonomy.push_back(desc);
    }
    
    return taxonomy;
}

CRef<CSeqdesc>
CSeqDBVol::x_GetAsnDefline(int              oid,
                           bool             have_oidlist,
                           int              memb_bit,
                           CSeqDBLockHold & locked) const
{
    const char * ASN1_DEFLINE_LABEL = "ASN1_BlastDefLine";
    
    CRef<CSeqdesc> asndef;
    
    vector<char> hdr_data;
    x_GetHdrBinaryMembBit(oid, hdr_data, have_oidlist, memb_bit, locked);
    
    if (! hdr_data.empty()) {
        CRef<CUser_object> uobj(new CUser_object);
    
        CRef<CObject_id> uo_oi(new CObject_id);
        uo_oi->SetStr(ASN1_DEFLINE_LABEL);
        uobj->SetType(*uo_oi);
        
        CRef<CUser_field> uf(new CUser_field);
        
        CRef<CObject_id> uf_oi(new CObject_id);
        uf_oi->SetStr(ASN1_DEFLINE_LABEL);
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
CSeqDBVol::GetBioseq(int                   oid,
                     bool                  have_oidlist,
                     int                   memb_bit,
                     int                   target_gi,
                     CRef<CSeqDBTaxInfo>   tax_info,
                     CSeqDBLockHold      & locked) const
{
    typedef list< CRef<CBlast_def_line> > TDeflines;
    CRef<CBioseq> null_result;
    
    CRef<CBlast_def_line_set> defline_set;
    CRef<CBlast_def_line>     defline;
    list< CRef< CSeq_id > >   seqids;
    
    // Get the defline set
    
    defline_set = x_GetHdrMembBit(oid, have_oidlist, memb_bit, locked);
    
    if (target_gi != 0) {
        CSeq_id seqid(CSeq_id::e_Gi, target_gi);
        
        TDeflines & dl = defline_set->Set();
        
        CRef<CBlast_def_line> filt_dl;
        
        ITERATE(TDeflines, iter, dl) {
            if (s_SeqDB_SeqIdIn((**iter).GetSeqid(), seqid)) {
                filt_dl = *iter;
                break;
            }
        }
        
        if (filt_dl.Empty()) {
            NCBI_THROW(CSeqDBException, eArgErr,
                       "Error: oid headers do not contain target gi.");
        } else {
            dl.clear();
            dl.push_back(filt_dl);
        }
    }
    
    if (defline_set.Empty() ||
        (! defline_set->CanGet())) {
        return null_result;
    }
        
    defline = defline_set->Get().front();
        
    if (! defline->CanGetSeqid()) {
        return null_result;
    }
        
    seqids = defline->GetSeqid();
    
    // Get length & sequence.
    
    const char * seq_buffer = 0;
    
    int length = x_GetSequence(oid, & seq_buffer, false, locked, false);
    
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
    
    CRef<CBioseq> bioseq(new CBioseq);
    
    CSeq_inst & seqinst = bioseq->SetInst();
    
    bool is_prot = (x_GetSeqType() == kSeqTypeProt);
    
    if (is_prot) {
        s_SeqDBWriteSeqDataProt(seqinst, seq_buffer, length);
    } else {
        // nucl
        vector<Int4> ambchars;
        
        x_GetAmbChar(oid, ambchars, locked);
        
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
    
    // Set the length, id (Seq_id), and repr (== raw).
    
    seqinst.SetLength(length);
    seqinst.SetRepr(CSeq_inst::eRepr_raw);
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
        
        CRef<CSeqdesc> desc2( x_GetAsnDefline(oid, have_oidlist, memb_bit, locked) );
        
        CSeq_descr & seq_desc_set = bioseq->SetDescr();
        seq_desc_set.Set().push_back(desc1);
        
        if (! desc2.Empty()) {
            seq_desc_set.Set().push_back(desc2);
        }
    }
    
    list< CRef<CSeqdesc> > tax =
        x_GetTaxonomy(oid,
                      have_oidlist,
                      memb_bit,
                      target_gi,
                      tax_info,
                      locked);
    
    ITERATE(list< CRef<CSeqdesc> >, iter, tax) {
        bioseq->SetDescr().Set().push_back(*iter);
    }
    
    return bioseq;
}

char * CSeqDBVol::x_AllocType(size_t length, ESeqDBAllocType alloc_type, CSeqDBLockHold & locked) const
{
    // Allocation using the atlas is not intended for the end user.
    
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
        retval = m_Atlas.Alloc(length, locked);
    }
    
    return retval;
}

int CSeqDBVol::GetAmbigSeq(int                oid,
                           char            ** buffer,
                           int                nucl_code,
                           ESeqDBAllocType    alloc_type,
                           CSeqDBLockHold   & locked) const
{
    char * buf1 = 0;
    int baselen = x_GetAmbigSeq(oid, & buf1, nucl_code, alloc_type, locked);
    
    *buffer = buf1;
    return baselen;
}

int CSeqDBVol::x_GetAmbigSeq(int                oid,
                             char            ** buffer,
                             int                nucl_code,
                             ESeqDBAllocType    alloc_type,
                             CSeqDBLockHold   & locked) const
{
    int base_length = -1;
    
    if (kSeqTypeProt == m_Idx.GetSeqType()) {
        if (alloc_type == eAtlas) {
            base_length = x_GetSequence(oid, (const char**) buffer, true, locked, false);
        } else {
            const char * buf2(0);
            
            base_length = x_GetSequence(oid, & buf2, false, locked, false);
            
            char * obj = x_AllocType(base_length, alloc_type, locked);
            
            memcpy(obj, buf2, base_length);
            
            *buffer = obj;
        }
    } else {
        vector<char> buffer_na8;
        
        {
            // The code in this block is derived from GetBioseq() and
            // s_SeqDBWriteSeqDataNucl().
            
            // Get the length and the (probably mmapped) data.
            
            const char * seq_buffer = 0;
            
            base_length = x_GetSequence(oid, & seq_buffer, false, locked, false);
            
            if (base_length < 1) {
                NCBI_THROW(CSeqDBException, eFileErr,
                           "Error: could not get sequence.");
            }
            
            // Get ambiguity characters.
            
            vector<Int4> ambchars;
            x_GetAmbChar(oid, ambchars, locked);
            
            // Combine and translate to 4 bits-per-character encoding.
            
            bool sentinel = (nucl_code == kSeqDBNuclBlastNA8);
            
            s_SeqDBMapNA2ToNA8(seq_buffer, buffer_na8, base_length, sentinel);
            s_SeqDBRebuildDNA_NA8(buffer_na8, ambchars, sentinel);
            
            if (sentinel) {
                // Translate bytewise, in place.
                s_SeqDBMapNcbiNA8ToBlastNA8(buffer_na8);
            }
        }
        
        int bytelen = (int) buffer_na8.size();
        char * uncomp_buf = x_AllocType(bytelen, alloc_type, locked);
        
        for(int i = 0; i < bytelen; i++) {
            uncomp_buf[i] = buffer_na8[i];
        }
        
        *buffer = uncomp_buf;
    }
    
    return base_length;
}

int CSeqDBVol::x_GetSequence(int              oid,
                             const char    ** buffer,
                             bool             keep,
                             CSeqDBLockHold & locked,
                             bool             can_release) const
{
    TIndx start_offset = 0;
    TIndx end_offset   = 0;
    
    int length = -1;
    
    m_Idx.GetSeqStartEnd(oid, start_offset, end_offset);
    
    char seqtype = m_Idx.GetSeqType();
    
    if (kSeqTypeProt == seqtype) {
        // Subtract one, for the inter-sequence null.
        
        end_offset --;
        
        length = int(end_offset - start_offset);
        
        *buffer = m_Seq.GetRegion(start_offset, end_offset, keep, locked);
    } else if (kSeqTypeNucl == seqtype) {
        // The last byte is partially full; the last two bits of
        // the last byte store the number of nucleotides in the
        // last byte (0 to 3).
        
        *buffer = m_Seq.GetRegion(start_offset, end_offset, keep, locked);
        
        // If we are returning a hold on the sequence (keep), and the
        // caller does not need the lock after this (can_release) we
        // can let go of the lock (because the hold will prevent GC of
        // the underlying data).  This will allow the following data
        // access to occur outside of the locked duration - lowering
        // contention in the nucleotide case.
        
        if (keep && can_release) {
            m_Atlas.Unlock(locked);
        }
        
        int whole_bytes = int(end_offset - start_offset - 1);
        
        char last_char = (*buffer)[whole_bytes];
        
        int remainder = last_char & 3;
        length = (whole_bytes * 4) + remainder;
    }
    
    return length;
}

list< CRef<CSeq_id> > CSeqDBVol::GetSeqIDs(int              oid,
                                           bool             have_oidlist,
                                           int              membership_bit,
                                           CSeqDBLockHold & locked) const
{
    list< CRef< CSeq_id > > seqids;
    
    CRef<CBlast_def_line_set> defline_set =
        x_GetHdrMembBit(oid, have_oidlist, membership_bit, locked);
    
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

Uint8 CSeqDBVol::GetVolumeLength() const
{
    return m_Idx.GetVolumeLength();
}

CRef<CBlast_def_line_set>
CSeqDBVol::GetHdr(int oid, CSeqDBLockHold & locked) const
{
    return x_GetHdrAsn1(oid, locked);
}

CRef<CBlast_def_line_set>
CSeqDBVol::x_GetHdrMembBit(int              oid,
                           bool             have_oidlist,
                           int              membership_bit,
                           CSeqDBLockHold & locked) const
{
    typedef list< CRef<CBlast_def_line> > TBDLL;
    typedef TBDLL::iterator               TBDLLIter;
    
    CRef<CBlast_def_line_set> BDLS = x_GetHdrAsn1(oid, locked);
    
    // 2. filter based on "rdfp->membership_bit" or similar.
    
    if (have_oidlist && (membership_bit != 0)) {
        // Create the memberships mask (should this be fixed to allow
        // membership bits greater than 32?)
        
        int memb_mask = 0x1 << (membership_bit-1);
        
        TBDLL & dl = BDLS->Set();
        
        for(TBDLLIter iter = dl.begin(); iter != dl.end(); ) {
            const CBlast_def_line & defline = **iter;
            
            bool have_memb =
                defline.CanGetMemberships() &&
                defline.IsSetMemberships() &&
                (! defline.GetMemberships().empty());
            
            if (have_memb) {
                int bits = defline.GetMemberships().front();
                
                if ((bits & memb_mask) == 0) {
                    have_memb = false;
                }
            }
            
            if (! have_memb) {
                TBDLLIter eraseme = iter++;
                dl.erase(eraseme);
            } else {
                iter++;
            }
        }
    }
    
    return BDLS;
}

CRef<CBlast_def_line_set>
CSeqDBVol::x_GetHdrAsn1(int oid, CSeqDBLockHold & locked) const
{
    CRef<CBlast_def_line_set> nullret;
    
    TIndx hdr_start = 0;
    TIndx hdr_end   = 0;
    
    m_Idx.GetHdrStartEnd(oid, hdr_start, hdr_end);
    
    const char * asn_region = m_Hdr.GetRegion(hdr_start, hdr_end, locked);
    
    // Now; create an ASN.1 object from the memory chunk provided
    // here.
        
    if (! asn_region) {
        return nullret;
    }
    
    // Get the data as a string, then as a stringstream, then build
    // the actual asn.1 object.  How much 'extra' work is done here?
    // Perhaps a dedicated ASN.1 memory-stream object would be of some
    // benefit, particularly in the "called 100000 times" cases.
    
    istringstream asndata( string(asn_region, asn_region + (hdr_end - hdr_start)) );
    
    auto_ptr<CObjectIStream> inpstr(CObjectIStream::Open(eSerial_AsnBinary, asndata));
    
    CRef<objects::CBlast_def_line_set> phil(new objects::CBlast_def_line_set);
    
    *inpstr >> *phil;
    
    
    return phil;
}

void
CSeqDBVol::x_GetHdrBinaryMembBit(int              oid,
                                 vector<char>   & hdr_data,
                                 bool             have_oidlist,
                                 int              membership_bit,
                                 CSeqDBLockHold & locked) const
{
    CRef<CBlast_def_line_set> dls =
        x_GetHdrMembBit(oid, have_oidlist, membership_bit, locked);
    
    // Get the data as a string, then as a stringstream, then build
    // the actual asn.1 object.  How much 'extra' work is done here?
    // Perhaps a dedicated ASN.1 memory-stream object would be of some
    // benefit, particularly in the "called 100000 times" cases.
    
    ostringstream asndata;
    
    auto_ptr<CObjectOStream> outpstr(CObjectOStream::Open(eSerial_AsnBinary, asndata));
    
    *outpstr << *dls;
    
    hdr_data.clear();
    string str = asndata.str();
    
    hdr_data.assign(str.data(), str.data() + str.length());
}

void CSeqDBVol::x_GetAmbChar(int oid, vector<Int4> & ambchars, CSeqDBLockHold & locked) const
{
    TIndx start_offset = 0;
    TIndx end_offset   = 0;
    
    bool ok = m_Idx.GetAmbStartEnd(oid, start_offset, end_offset);
    
    if (! ok) {
        NCBI_THROW(CSeqDBException, eFileErr,
                   "File error: could not get ambiguity data.");
    }
    
    int length = int(end_offset - start_offset);
    
    if (length) {
        int total = length / 4;
        
        Int4 * buffer =
            (Int4*) m_Seq.GetRegion(start_offset,
                                    start_offset + (total * 4),
                                    false,
                                    locked);
        
        // This makes no sense...
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
    return m_Idx.GetNumOIDs();
}

string CSeqDBVol::GetTitle() const
{
    return m_Idx.GetTitle();
}

string CSeqDBVol::GetDate() const
{
    return m_Idx.GetDate();
}

int CSeqDBVol::GetMaxLength() const
{
    return m_Idx.GetMaxLength();
}

bool CSeqDBVol::PigToOid(int pig, int & oid, CSeqDBLockHold & locked) const
{
    if (m_IsamPig.Empty()) {
        return false;
    }
    
    return m_IsamPig->PigToOid(pig, oid, locked);
}

bool CSeqDBVol::GetPig(int oid, int & pig, CSeqDBLockHold & locked) const
{
    pig = -1;
    
    if (m_IsamPig.Empty()) {
        return false;
    }
    
    CRef<CBlast_def_line_set> BDLS = x_GetHdrAsn1(oid, locked);
    
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

bool CSeqDBVol::GiToOid(int gi, int & oid, CSeqDBLockHold & locked) const
{
    if (m_IsamGi.Empty()) {
        return false;
    }
    
    return m_IsamGi->GiToOid(gi, oid, locked);
}

bool CSeqDBVol::GetGi(int oid, int & gi, CSeqDBLockHold & locked) const
{
    gi = -1;
    
    if (m_IsamGi.Empty()) {
        return false;
    }
    
    CRef<CBlast_def_line_set> BDLS = x_GetHdrAsn1(oid, locked);
    
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

void CSeqDBVol::AccessionToOids(const string   & acc,
                                vector<int>    & oids,
                                CSeqDBLockHold & locked) const
{
    bool   simpler (false);
    int    ident   (-1);
    string str_id;
    
    switch(CSeqDBIsam::TryToSimplifyAccession(acc, ident, str_id, simpler)) {
    case CSeqDBIsam::eString:
        if (! m_IsamStr.Empty()) {
            // Not simplified
            m_IsamStr->StringToOids(str_id, oids, simpler, locked);
        }
        break;
        
    case CSeqDBIsam::ePig:
        // Converted to PIG type.
        if (! m_IsamPig.Empty()) {
            int oid(-1);
            
            if (m_IsamPig->PigToOid(ident, oid, locked)) {
                oids.push_back(oid);
            }
        }
        break;
        
    case CSeqDBIsam::eGi:
        // Converted to GI type.
        if (! m_IsamGi.Empty()) {
            int oid(-1);
            
            if (m_IsamGi->GiToOid(ident, oid, locked)) {
                oids.push_back(oid);
            }
        }
        break;
        
    case CSeqDBIsam::eOID:
        // Converted to OID directly.
        oids.push_back(ident);
        break;
    }
}

void CSeqDBVol::SeqidToOids(CSeq_id        & seqid,
                            vector<int>    & oids,
                            CSeqDBLockHold & locked) const
{
    bool simpler (false);
    int  ident   (-1);
    string str_id;
    
    switch(CSeqDBIsam::SimplifySeqid(seqid, 0, ident, str_id, simpler)) {
    case CSeqDBIsam::eString:
        if (! m_IsamStr.Empty()) {
            // Not simplified
            m_IsamStr->StringToOids(str_id, oids, simpler, locked);
        }
        break;
        
    case CSeqDBIsam::ePig:
        // Converted to PIG type.
        if (! m_IsamPig.Empty()) {
            int oid(-1);
            
            if (m_IsamPig->PigToOid(ident, oid, locked)) {
                oids.push_back(oid);
            }
        }
        break;
        
    case CSeqDBIsam::eGi:
        // Converted to GI type.
        if (! m_IsamGi.Empty()) {
            int oid(-1);
            
            if (m_IsamGi->GiToOid(ident, oid, locked)) {
                oids.push_back(oid);
            }
        }
        break;
        
    case CSeqDBIsam::eOID:
        // Converted to OID directly.
        oids.push_back(ident);
        break;
    }
}

void CSeqDBVol::UnLease()
{
    m_Idx.UnLease();
    m_Seq.UnLease();
    m_Hdr.UnLease();
    
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

int CSeqDBVol::GetOidAtOffset(int first_seq, Uint8 residue) const
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
    
    if (kSeqTypeNucl == m_Idx.GetSeqType()) {
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
        
        if (kSeqTypeProt == m_Idx.GetSeqType()) {
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
    TIndx start_offset = 0;
    m_Idx.GetSeqStart(oid, start_offset);
    return start_offset;
}

END_NCBI_SCOPE

