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

/// @file writedb_convert.cpp
/// Data conversion tools for CWriteDB and associated code.
/// class for WriteDB.

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include "writedb_general.hpp"
#include "writedb_convert.hpp"
#include <iostream>

BEGIN_NCBI_SCOPE

USING_SCOPE(std);

/// Ambiguous portion of a sequence.
///
/// This class represents a portion of a sequence that is ambiguous.
/// The ambiguites represented by this region must have the same
/// ambiguity letter, be contiguous, and span at most 4095 letters.
/// If any of these conditions is not met, the issue may be solved by
/// the use of multiple objects of this class.
class CAmbiguousRegion {
public:
    /// Construct a new, empty, ambiguous region.
    CAmbiguousRegion()
        : m_Start (0),
          m_End   (0),
          m_Value (0)
    {
    }
    
    /// Construct a new ambiguous region one letter in length.
    /// @param value Ambiguity letter to use. [in]
    /// @param offset Starting offset of the ambiguity. [in]
    CAmbiguousRegion(int value, int offset)
        : m_Start (offset),
          m_End   (offset+1),
          m_Value (value)
    {
    }
    
    /// Construct a new ambiguous region of a specified length.
    /// @param value Ambiguity letter to use. [in]
    /// @param offset Starting offset of the ambiguity. [in]
    /// @param length Length of the ambiguity. [in]
    CAmbiguousRegion(int value, int offset, int length)
        : m_Start (offset),
          m_End   (offset+length),
          m_Value (value)
    {
    }
    
    /// Try to append a letter to an ambiguous region.
    ///
    /// The provided letter will be appended to this region if the
    /// resulting region would be 4095 or fewer letters in length,
    /// contain only one ambiguity letter, and be contiguous.  If it
    /// would not, false is returned and this region is unchanged.
    ///
    /// @param value Ambiguity letter to use. [in]
    /// @param offset Starting offset of the ambiguity. [in]
    /// @return True if the letter was appended, false otherwise.
    bool Append(int value, int offset)
    {
        _ASSERT(m_End && m_Value);
        
        if (value == m_Value && offset == m_End) {
            if (Length() < eMaxLength) {
                m_End ++;
                return true;
            }
        }
        
        return false;
    }
    
    /// Get the letter value for this region.
    /// @return Letter value.
    int Value() const
    {
        return m_Value;
    }
    
    /// Get the length of this ambiguous region.
    /// @return Length of the region.
    int Length() const
    {
        return m_End - m_Start;
    }
    
    /// Get the starting offset of the region.
    /// @return Starting offset of the region.
    int Offset() const
    {
        return m_Start;
    }
    
private:
    // Maxinum length that we allow for one region of ambiguities;
    // longer regions than this will be packed as multiple regions.
    //
    // Note that in WriteDB, the new format is triggered by long
    // regions OR long sequences.  In formatdb, the new format is
    // triggered only by long sequences, which can result in long
    // ambiguous regions being stored as lists of thousands of lists
    // of short ambiguity runs.
    
    // Old format uses <= 16 bases per Int4, which means that a run of
    // 32 'N' bases results in 32/16 = 2 Int4s, which is 8 bytes,
    // which is the same space as used to stored 16 compressed bases.
    
    enum {
        eMaxLength = 0xFFF ///< Maximum length of a region.
    };
    
    int m_Start; ///< Starting offset (offset of first base).
    int m_End;   ///< End offset (offset of first disincluded base.)
    int m_Value; ///< Value of base (ambiguity letter).
};

/// Encode ambiguities in blast database format.
///
/// This class encodes nucleotide ambiguities in blast database format
/// from a series of ambiguous letter values and offsets.
class CAmbigDataBuilder {
public:
    /// Constructor.
    /// @param sz Size of the sequence in letters. [in]
    CAmbigDataBuilder(int sz)
        : m_Size(sz)
    {
        for(int i = 0; i < 16; i++) {
            m_Log2[i] = -1;
        }
        
        // Only these values should be specified
        m_Log2[1] = 0;
        m_Log2[2] = 1;
        m_Log2[4] = 2;
        m_Log2[8] = 3;
    }

    /// Check (and maybe store) a possibly ambiguous letter.
    ///
    /// If the letter is not an ambiguity, this method converts it to
    /// Blast-NA2 format, and returns it.  If the letter value is an
    /// ambiguity, it is added to the list of ambiguities, and a
    /// randomly selected letter value is returned.  Each ambiguity
    /// letter (there are 12 for nucleotide) represents a possiblity
    /// between two or more nucleotide bases.  The random letter is
    /// always selected from the set of these values corresponding to
    /// the input ambiguity value.
    ///
    /// @param data Letter value in BlastNA8. [in]
    /// @param offset Offset of letter. [in]
    /// @return Value to encode as BlastNA2.
    int Check(int data, int offset)
    {
        // If offset is past the sequence end, return zero.  This can
        // happen since the calling code checks for ambiguities two
        // letters at a time.
        
        if (offset >= m_Size) {
            return 0;
        }
        
        // If we recieve a non-ambiguity, return the normal
        // translation for this letter code.
        
        _ASSERT(data != 0);
        
        int rv = m_Log2[data & 0xF];
        
        if (rv != -1) {
            return rv;
        }
        
        // Bona-fide ambiguity; we need to make up some random junk,
        // and also build an ambiguous region.
        
        x_AddAmbig(data, offset);
        
        return x_Random(data);
    }
    
    // New format: (inclusive ranges)
    //   4 bits  (31-28): value (residue mask, i.e. 0xF for 'N')
    //   12 bits (27-16): length of region
    //   16 bits (15- 0): (unused)
    //   
    //   32 bits: offset
    //
    // Old format:
    //   4 bits  (31-28): value (residue mask, i.e. 0xF for 'N')
    //   4 bits  (27-24): length of region
    //   24 bits (23- 0): offset
    
    /// Compute and return the encoded list of ambiguities.
    ///
    /// The list of ambiguous regions is packed in blast database
    /// format and returned to the user.  If the length of the
    /// sequence is larger than 2^24-1, or any of the ambiguous
    /// regions is larger than 0xF, the 'new' format of ambiguity is
    /// used, which allows for larger ambiguous regions at higher
    /// sequence offsets, but requires 8 bytes per ambiguous region
    /// instead of four bytes required by the 'old' format.
    ///
    /// @param amb The ambiguity data in blast database format. [out]
    void GetAmbig(string & amb)
    {
        // The decision to use the new format should (probably) be
        // selected via an analysis of all ambiguities encoded here.
        
        bool new_format = false;
        
        // Current technique: If the sequence length is longer than
        // 2^24-1, I use the new format.  Else, if any region of
        // ambiguities is longer than 16 bases, I use the new format.
        // Otherwise, I use the old format.
        
        if (m_Size > 0xFFFFFF) {
            new_format = true;
        } else {
            for(unsigned i = 0; i<m_Regions.size(); i++) {
                if (m_Regions[i].Length() > 0xF) {
                    new_format = true;
                    break;
                }
            }
        }
        
        int num_amb = (int) m_Regions.size();
        
        // The size packed here is actually the number of *words* used
        // rather than the number of ambiguous regions; the new format
        // two words per region.
        
        Uint4 amb_words = (new_format
                           ? (0x80000000 | (num_amb * 2))
                           : num_amb);
        
        amb.reserve((1 + m_Regions.size())*8);
        s_AppendInt4(amb, amb_words);
        
        for(int i = 0; i < num_amb; i++) {
            // Regions over 4k are split during ambiguity scanning; at
            // this point in the code, splitting has already happened.
            
            if (new_format) {
                x_PackNewAmbig(amb, m_Regions[i]);
            } else {
                x_PackOldAmbig(amb, m_Regions[i]);
            }
        }
    }
    
    /// Append the 'new' encoding of one ambiguous region to a string.
    /// @param amb String encoding of all ambiguous regions. [in|out]
    /// @param r Ambiguous region. [in]
    void x_PackNewAmbig(string                 & amb,
                        const CAmbiguousRegion & r)
    {
        int length_m1 = r.Length() - 1;
        
        _ASSERT(r.Value() <= 15);
        _ASSERT((length_m1 >> 12) == 0);
        
        // First word - residue value and run length.
        char A1[4];
        
        char ch0 = (r.Value() << 4) | (length_m1 >> 8);
        char ch1 = length_m1 & 0xFF;
        
        A1[0] = ch0;
        A1[1] = ch1;
        A1[2] = A1[3] = 0; // unused space
        
        amb.append(A1, 4);
        
        // Second word - starting offset.
        
        s_AppendInt4(amb, r.Offset());
    }
    
    /// Append the 'old' encoding of one ambiguous region to a string.
    /// @param amb String encoding of all ambiguous regions. [in|out]
    /// @param r Ambiguous region. [in]
    void x_PackOldAmbig(string & amb, CAmbiguousRegion & r)
    {
        int length_m1 = r.Length() - 1;
        int off = r.Offset();
        
        _ASSERT(r.Value() <= 15);
        _ASSERT((length_m1 >> 4) == 0);
        _ASSERT(off <= 0xFFFFFF); // old form uses three byte offsets
        
        // All in one word.
        char A1[4];
        
        char ch0 = (r.Value() << 4) | length_m1;
        
        A1[0] = ch0;
        A1[1] = (off >> 16) & 0xFF;
        A1[2] = (off >>  8) & 0xFF;
        A1[3] = (off      ) & 0xFF;
        
        amb.append(A1, 4);
    }
    
private:
    /// Add an ambiguity letter.
    ///
    /// The internal encoding contains a list of ambiguous ranges.
    /// This method adds the given letter at the given offset to the
    /// most recent region, if possible, or creates a new region for
    /// it.
    /// 
    /// @param value Ambiguous letter to add. [in]
    /// @param offset Offset at which letter occurs. [in]
    void x_AddAmbig(int value, int offset)
    {
        if (m_Regions.size()) {
            if (m_Regions.back().Append(value, offset)) {
                return;
            }
        }
        
        CAmbiguousRegion r(value, offset);
        m_Regions.push_back(r);
    }
    
    /// Pick a random letter from the set represented by an ambiguity.
    /// 
    /// This method takes an ambiguous value as input, and returns a
    /// letter randomly chosen from the set of letters the ambiguity
    /// represents.
    ///
    /// @param value An ambiguous letter. [in]
    /// @return A non-ambiguous letter.
    int x_Random(int value)
    {
        // 0xF is the most common case of ambiguity so it's worth to
        // process it in fastest way, especially because it's very easy.
        
        if (value == 0xF) {
            return rand() & 0x3;
        }
        
        if (value == 0) {
            cerr << "Error: '0' ambiguity code found, changing to 15." << endl;
            return rand() & 0x3;
        }
        
        int bitcount = ((value & 1) +
                        ((value >> 1) & 1) +
                        ((value >> 2) & 1) +
                        ((value >> 3) & 1));
        
        // 1-bit ambiguities here, indicate an error in this class.
        _ASSERT(bitcount >= 2);
        _ASSERT(bitcount <= 3);
        
        int pick = rand() % bitcount;
        
        for(int i = 0; i < 4; i++) {
            // skip 0 bits in input.
            if ((value & (1 << i)) == 0)
                continue;
            
            // If the bitcount is zero, this is the bit we want.
            if (! pick)
                return i;
            
            // Else, decrement.
            pick--;
        }
        
        // This should be unreachable.
        _ASSERT(0);
        return 0;
    }
    
    // Data
    
    /// Table mapping 1248 to 0123.
    int m_Log2[16];
    
    /// Size of the input sequence.
    int m_Size;
    
    /// Ambiguous regions for the sequence.
    vector<CAmbiguousRegion> m_Regions;
};

/// Builds a table from NA4 to NA2 (with ambiguities marked as 0xFF.)
/// @return A vector indexed by NA4 value, with values from 0-3 or 0xFF.
inline vector<unsigned char> s_BuildNa4ToNa2Table()
{
    // ctable takes a byte index and returns 0-16 output from 0-16.
    // Invalid or ambiguous elements return FF instead.
    
    vector<unsigned char> ctable;
    ctable.resize(16, 0xFF);
    
    for(int i = 0; i<4; i++) {
        ctable[1 << i] = i;
    }
    
    return ctable;
}

void WriteDB_Ncbi4naToBinary(const char * ncbi4na,
                             int          byte_length,
                             int          base_length,
                             string     & seq,
                             string     & amb)
{
    typedef unsigned char uchar;
    
    static vector<uchar> ctable = s_BuildNa4ToNa2Table();
    
    // Build the sequence data.
    
    int inp_bytes   = s_DivideRoundUp(base_length, 2);
    int blast_bytes = base_length / 4 + 1;
    int remainder   = base_length & 3;
    int last_byte   = blast_bytes - 1;
    
    // Accumulator for ambiguity data.
    CAmbigDataBuilder ambiguities(base_length);
    
    if (!((int)inp_bytes == (int)byte_length)) {
        cout << "ib=" << inp_bytes << ",n4sz=" << byte_length << endl;
    }
    
    _ASSERT((int)inp_bytes == (int)byte_length);
    
    seq.resize(blast_bytes);
    
    // Fun Fact: If the number of bases is even, we can process two at
    // a time, but if the number of input bases is odd, we want to
    // short circuit the last 'read' from the input array, since that
    // would walk off the end of the array.  So, i2_limit computes the
    // proper limit for i2's input data, where the primary iteration
    // refers to the iteration limit for i1.
    
    for(int i = 0; i < inp_bytes; i++) {
        // one input byte
        uchar inp = ncbi4na[i];
        
        // represents 2 bases
        uchar b1 = inp >>  4;
        uchar b2 = inp & 0xF;
        
        // compress each to 2 bits
        uchar c1 = ctable[b1];
        uchar c2 = ctable[b2];
        
        uchar half = 0;
        
        if (((c1 | c2) & 0x80) == 0) {
            // No ambiguities, so we can do this the easy way.
            
            half = (c1 << 2) | c2;
        } else {
            // Check each element, accumulate ambiguity data.
            
            if (! (b1 && b2)) {
                _ASSERT(b1 != 0);
                
                if ((i*2+1) < base_length) {
                    // the second base is only available if the
                    // sequence contains a whole number of bytes
                    // (i.e. has an even length).
                    _ASSERT(b2 != 0);
                } else {
                    _ASSERT(b2 == 0);
                }
            }
            
            half |= ambiguities.Check(b1, i*2) << 2;
            half |= ambiguities.Check(b2, i*2+1);
        }
        
        seq[i/2] |= (i & 1) ? half : (half << 4);
    }
    seq[last_byte] &= 255-3;
    seq[last_byte] |= remainder;
    
    ambiguities.GetAmbig(amb);
}

void WriteDB_Ncbi4naToBinary(const CSeq_inst & seqinst,
                             string          & seq,
                             string          & amb)
{
    const vector<char> & na4 = seqinst.GetSeq_data().GetNcbi4na().Get();
    int base_length = seqinst.GetLength();
    
    WriteDB_Ncbi4naToBinary(& na4[0], (int) na4.size(), base_length, seq, amb);
}

void WriteDB_StdaaToBinary(const CSeq_inst & si, string & seq)
{
    // No conversion is actually done here.
    const vector<char> & v = si.GetSeq_data().GetNcbistdaa().Get();
    
    _ASSERT(si.GetLength() == v.size());
    seq.assign(& v[0], v.size());
}

void WriteDB_EaaToBinary(const CSeq_inst & si, string & seq)
{
    const string & v = si.GetSeq_data().GetNcbieaa().Get();
    
    _ASSERT(si.GetLength() == v.size());
    
    // convert to string.
    CSeqConvert::Convert(v,
                         CSeqUtil::e_Ncbieaa,
                         0,
                         (int) v.size(),
                         seq,
                         CSeqUtil::e_Ncbistdaa);
}

void WriteDB_IupacaaToBinary(const CSeq_inst & si, string & seq)
{
    const string & v = si.GetSeq_data().GetIupacaa().Get();
    
    _ASSERT(si.GetLength() == v.size());
    
    // convert to string.
    CSeqConvert::Convert(v,
                         CSeqUtil::e_Iupacaa,
                         0,
                         (int) v.size(),
                         seq,
                         CSeqUtil::e_Ncbistdaa);
}

void WriteDB_Ncbi2naToBinary(const CSeq_inst & si,
                             string          & seq)
{
    int base_length = si.GetLength();
    int data_bytes  = s_DivideRoundUp(base_length, 4);
    int blast_bytes = base_length / 4 + 1;
    int remainder   = base_length & 3;
    int last_byte   = blast_bytes - 1;
    
    const vector<char> & v = si.GetSeq_data().GetNcbi2na().Get();
    
    _ASSERT((int)data_bytes == (int)v.size());
    
    seq.reserve(blast_bytes);
    seq.assign(& v[0], data_bytes);
    seq.resize(blast_bytes);
    
    seq[last_byte] &= 255-3;
    seq[last_byte] |= remainder;
}

END_NCBI_SCOPE
