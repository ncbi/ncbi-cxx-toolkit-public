#ifndef OBJECTS_SEQ___SEQPORT_UTIL__HPP
#define OBJECTS_SEQ___SEQPORT_UTIL__HPP

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
 * Author:  Clifford Clausen
 *          (also reviewed/fixed/groomed by Denis Vakatov)
 *
 * File Description:
 *   
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2001/08/24 00:34:22  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include <objects/seq/Seq_data.hpp>
#include <objects/seqcode/Seq_code_type.hpp>
#include <util/random_gen.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <vector>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


class CSeq_code_set;


// CSeqportUtil is a singleton.

class CSeqportUtil
{
public:
    // Alphabet conversion function. Function returns the
    // number of converted codes.
    static unsigned int Convert(const CSeq_data&       in_seq,
                                CSeq_data*             out_seq,
                                CSeq_data::E_Choice    to_code,
                                unsigned int           uBeginIdx = 0,
                                unsigned int           uLength   = 0,
                                bool                   bAmbig    = false,
                                CRandom::TValue        seed      = 17734276);

    // Function to provide maximum in-place packing of na
    // sequences without loss of information. Iupacna
    // can always be packed to ncbi4na without loss. Iupacna
    // can sometimes be packed to ncbi2na. Ncbi4na can
    // sometimes be packed to ncbi2na. Returns number of
    // residues packed. If in_seq cannot be packed, the
    // original in_seq is returned unchanged and the return value
    // from Pack is 0
    static unsigned int Pack(CSeq_data*     in_seq,
                             unsigned int   uBeginidx = 0,
                             unsigned int   uLength   = 0);

    // Performs fast validation of CSeq_data. If all data in the
    // sequence represent valid elements of a biological sequence, then
    // FastValidate returns true. Otherwise it returns false
    static bool FastValidate(const CSeq_data&   in_seq,
                             unsigned int       uBeginIdx = 0,
                             unsigned int       uLength   = 0);

    // Performs validation of CSeq_data. Returns a list of indices
    // corresponding to data that does not represent a valid element
    // of a biological sequence.
    static void Validate(const CSeq_data&       in_seq,
                         vector<unsigned int>*  badIdx,
                         unsigned int           uBeginIdx = 0,
                         unsigned int           uLength   = 0);

    // Get ambiguous bases. out_indices returns
    // the indices relative to in_seq of ambiguous bases.
    // out_seq returns the ambiguous bases. Note, there are
    // only ambiguous bases for iupacna->ncib2na and
    // ncib4na->ncbi2na coversions.
    static unsigned int GetAmbigs(const CSeq_data&       in_seq,
                                  CSeq_data*             out_seq,
                                  vector<unsigned int>*  out_indices,
                                  CSeq_data::E_Choice    to_code,
                                  unsigned int           uBeginIdx = 0,
                                  unsigned int           uLength   = 0);

    // Get a copy of CSeq_data. No conversion is done. uBeginIdx of the
    // biological sequence in in_seq will be in position
    // 0 of out_seq. Usually, uLength bases will be copied
    // from in_seq to out_seq. If uLength goes beyond the end of
    // in_seq, it will be shortened to go to the end of in_seq.
    // For packed sequence formats (ncbi2na and ncbi4na),
    // only uLength bases are valid copies. For example,
    // in an ncbi4na encoded sequence, if uLength is odd, the last
    // sequence returned will be uLength+1 because 2 bases are encoded
    // per byte in ncbi4na. However, in this case, uLength will be returned
    // unchanged (it will remain odd unless it goes beyond the end
    // of in_seq). If uLength=0, then a copy from uBeginIdx to the end
    // of in_seq is returned.
    static unsigned int GetCopy(const CSeq_data&       in_seq,
                                CSeq_data*             out_seq,
                                unsigned int           uBeginIdx = 0,
                                unsigned int           uLength   = 0);

    // Method to keep only a contiguous piece of a sequence beginning
    // at uBeginIdx and uLength residues long. Does bit shifting as
    // needed to put uBeginIdx of original sequence at position zero on output.
    // Similar to GetCopy(), but done in place.  Returns length of
    // kept sequence.
    static unsigned int Keep(CSeq_data*      in_seq,
                             unsigned int     uBeginIdx = 0,
                             unsigned int     uLength   = 0);

    // Append in_seq2 to to end of in_seq1. Both in seqs must be
    // in the same alphabet or this method will throw a runtime_error.
    // The result of the append will be put into out_seq.
    // For packed sequences ncbi2na and ncbi4na, Append will shift and
    // append so as to remove any jaggedness at the append point.
    static unsigned int Append(CSeq_data*             out_seq,
                               const CSeq_data&       in_seq1,
                               unsigned int           uBeginIdx1,
                               unsigned int           uLength1,
                               const CSeq_data&       in_seq2,
                               unsigned int           uBeginIdx2,
                               unsigned int           uLength2);

    // Create a biological complement of an na sequence.
    // Attempts to complement an aa sequence will throw
    // a runtime_error. Returns length of complemented sequence.

    // Complement the input sequence in place
    static unsigned int Complement(CSeq_data*       in_seq,
                                   unsigned int     uBeginIdx = 0,
                                   unsigned int     uLength   = 0);

    // Complement the input sequence and put the result in
    // the output sequence
    static unsigned int Complement(const CSeq_data&       in_seq,
                                   CSeq_data*             out_seq,
                                   unsigned int           uBeginIdx = 0,
                                   unsigned int           uLength   = 0);

    // Create a biological sequence that is the reversse
    // of an input sequence. Attempts to reverse an aa
    // sequence will throw a runtime_error. Returns length of
    // reversed sequence.

    // Reverse a sequence in place
    static unsigned int Reverse(CSeq_data*       in_seq,
                                unsigned int     uBeginIdx = 0,
                                unsigned int     uLength   = 0);

    // Reverse an input sequence and put result in output sequence.
    // Reverses packed bytes as necessary.
    static unsigned int Reverse(const CSeq_data&  in_seq,
                                CSeq_data*        out_seq,
                                unsigned int      uBeginIdx = 0,
                                unsigned int      uLength   = 0);


    // Create the reverse complement of an input sequence. Attempts
    // to reverse-complement an aa sequence will throw a
    // runtime_error.

    // Reverse complement a sequence in place
    static unsigned int ReverseComplement(CSeq_data*       in_seq,
                                          unsigned int     uBeginIdx = 0,
                                          unsigned int     uLength   = 0);

    // Reverse complmenet a sequence and put result in output sequence
    static unsigned int ReverseComplement(const CSeq_data&   in_seq,
                                          CSeq_data*         out_seq,
                                          unsigned int       uBeginIdx = 0,
                                          unsigned int       uLength   = 0);


private:
    // The following private "x_ZZZ()" methods implement the functionality of
    // the above public static "ZZZ" methods.

    unsigned int x_Convert
    (const CSeq_data&       in_seq,
     CSeq_data*             out_seq,
     CSeq_data::E_Choice    to_code,
     unsigned int           uBeginIdx,
     unsigned int           uLength,
     bool                   bAmbig,
     CRandom::TValue        seed)
        const;

    unsigned int x_Pack
    (CSeq_data*     in_seq,
     unsigned int   uBeginidx,
     unsigned int   uLength)
        const;

    bool x_FastValidate
    (const CSeq_data&   in_seq,
     unsigned int       uBeginIdx,
     unsigned int       uLength)
        const;

    void x_Validate
    (const CSeq_data&       in_seq,
     vector<unsigned int>*  badIdx,
     unsigned int           uBeginIdx,
     unsigned int           uLength)
        const;

    unsigned int x_GetAmbigs
    (const CSeq_data&       in_seq,
     CSeq_data*             out_seq,
     vector<unsigned int>*  out_indices,
     CSeq_data::E_Choice    to_code,
     unsigned int           uBeginIdx,
     unsigned int           uLength)
        const;

    unsigned int x_GetCopy
    (const CSeq_data&       in_seq,
     CSeq_data*             out_seq,
     unsigned int           uBeginIdx,
     unsigned int           uLength)
        const;

    unsigned int x_Keep
    (CSeq_data*      in_seq,
     unsigned int     uBeginIdx,
     unsigned int     uLength)
        const;

    unsigned int x_Append
    (CSeq_data*             out_seq,
     const CSeq_data&       in_seq1,
     unsigned int           uBeginIdx1,
     unsigned int           uLength1,
     const CSeq_data&       in_seq2,
     unsigned int           uBeginIdx2,
     unsigned int           uLength2)
        const;

    unsigned int x_Complement
    (CSeq_data*       in_seq,
     unsigned int     uBeginIdx,
     unsigned int     uLength)
        const;

    unsigned int x_Complement
    (const CSeq_data&       in_seq,
     CSeq_data*             out_seq,
     unsigned int           uBeginIdx,
     unsigned int           uLength)
        const;

    unsigned int x_Reverse
    (CSeq_data*       in_seq,
     unsigned int     uBeginIdx,
     unsigned int     uLength)
        const;

    unsigned int x_Reverse
    (const CSeq_data&  in_seq,
     CSeq_data*        out_seq,
     unsigned int      uBeginIdx,
     unsigned int      uLength)
        const;

    unsigned int x_ReverseComplement
    (CSeq_data*       in_seq,
     unsigned int     uBeginIdx,
     unsigned int     uLength)
        const;

    unsigned int x_ReverseComplement
    (const CSeq_data&   in_seq,
     CSeq_data*         out_seq,
     unsigned int       uBeginIdx,
     unsigned int       uLength)
        const;


    // Template wrapper class used to create data type specific
    // classes to delete code tables on exit from main
    template <class T>
    class CWrapper_table : public CObject
    {
    public:
        CWrapper_table(int size, int start)
        {
            m_Table   = new T[256];
            m_StartAt = start;
            m_Size    = size;
        }
        ~CWrapper_table() {
            drop_table();
        }
        void drop_table()
        {
            delete[] m_Table;
            m_Table = 0;
        }

        T*  m_Table;
        int m_StartAt;
        int m_Size;
    };

    // Template wrapper class used for two-dimensional arrays.
    template <class T>
    class CWrapper_2D : public CObject
    {
    public:
        CWrapper_2D(int size1, int start1, int size2, int start2)
        {
            m_Size_D1 = size1;
            m_Size_D2 = size2;
            m_StartAt_D1 = start1;
            m_StartAt_D2 = start2;
            m_Table = new T*[size1];
            for(int i=0; i<size1; i++)
                {
                    m_Table[i] = new T[size2] - start2;
                }
            m_Table -= start1;
        }
        ~CWrapper_2D()
        {
            m_Table += m_StartAt_D1;
            for(int i=0; i<m_Size_D1; i++)
                {
                    delete[](m_Table[i] + m_StartAt_D2);
                }
            delete[] m_Table;
        }

        T** m_Table;
        int m_Size_D1;
        int m_Size_D2;
        int m_StartAt_D1;
        int m_StartAt_D2;
    };

    // Typedefs making use of wrapper classes above.
    typedef CWrapper_table<char>           CCode_table;
    typedef CWrapper_table<string>         CCode_table_str;
    typedef CWrapper_table<int>            CMap_table;
    typedef CWrapper_table<unsigned int>   CFast_table4;
    typedef CWrapper_table<unsigned short> CFast_table2;
    typedef CWrapper_table<unsigned char>  CAmbig_detect;
    typedef CWrapper_table<char>           CCode_comp;
    typedef CWrapper_table<char>           CCode_rev;

    typedef CWrapper_2D<unsigned char>     CFast_4_1;
    typedef CWrapper_2D<unsigned char>     CFast_2_1;

    // String to initialize CSeq_code_set
    // This string is initialized in seqport_util.h
    static const char* sm_StrAsnData[];

    // CSeq_code_set member holding code and map table data
    CRef<CSeq_code_set> m_SeqCodeSet;

    // Helper function used internally to initialize m_SeqCodeSet
    CRef<CSeq_code_set> Init();

    // Member variables holding code tables
    CRef<CCode_table> m_Iupacna;
    CRef<CCode_table> m_Ncbieaa;
    CRef<CCode_table> m_Ncbistdaa;
    CRef<CCode_table> m_Iupacaa;

    // Helper function to initialize code tables
    CRef<CCode_table> InitCodes(ESeq_code_type code_type);

    // Member variables holding na complement information
    CRef<CCode_comp> m_Iupacna_complement;
    CRef<CCode_comp> m_Ncbi2naComplement;
    CRef<CCode_comp> m_Ncbi4naComplement;

    // Helper functions to initialize complement tables
    CRef<CCode_comp> InitIupacnaComplement();
    CRef<CCode_comp> InitNcbi2naComplement();
    CRef<CCode_comp> InitNcbi4naComplement();

    // Member variables holding na reverse information
    // Used to reverse residues packed within a byte.
    CRef<CCode_rev> m_Ncbi2naRev;
    CRef<CCode_rev> m_Ncbi4naRev;

    // Helper functions to initialize reverse tables
    CRef<CCode_rev> InitNcbi2naRev();
    CRef<CCode_rev> InitNcbi4naRev();

    // Member variables holding map tables
    CRef<CMap_table> m_Ncbi2naIupacna;
    CRef<CMap_table> m_Ncbi2naNcbi4na;
    CRef<CMap_table> m_Ncbi4naIupacna;
    CRef<CMap_table> m_IupacnaNcbi2na;
    CRef<CMap_table> m_IupacnaNcbi4na;
    CRef<CMap_table> m_Ncbi4naNcbi2na;
    CRef<CMap_table> m_IupacaaNcbieaa;
    CRef<CMap_table> m_NcbieaaIupacaa;
    CRef<CMap_table> m_IupacaaNcbistdaa;
    CRef<CMap_table> m_NcbieaaNcbistdaa;
    CRef<CMap_table> m_NcbistdaaNcbieaa;
    CRef<CMap_table> m_NcbistdaaIupacaa;

    // Helper function to initialize map tables
    CRef<CMap_table> InitMaps(ESeq_code_type from_type,
                              ESeq_code_type to_type);

    // Member variables holding fast conversion tables

    // Takes a byte as an index and returns a unsigned int with
    // 4 characters, each character being one of ATGC
    CRef<CFast_table4> m_FastNcbi2naIupacna;

    // Takes a byte (each byte with 4 Ncbi2na codes) as an index and
    // returns a Unit2 with 2 bytes, each byte formated as 2 Ncbi4na codes
    CRef<CFast_table2> m_FastNcbi2naNcbi4na;

    // Takes a byte (each byte with 2 Ncbi4na codes) as an index and
    // returns a 2 byte string, each byte with an Iupacna code.
    CRef<CFast_table2> m_FastNcbi4naIupacna;

    // Table used for fast compression from Iupacna to Ncbi2na (4 bytes to 1
    // byte). This table is a 2 dimensional table. The first dimension
    // corresponds to the iupacna position modulo 4 (0-3). The second dimension
    //  is the value of the iupacna byte (0-255). The 4 resulting values from 4
    // iupancna bytes are bitwise or'd to produce 1 byte.
    CRef<CFast_4_1> m_FastIupacnaNcbi2na;

    // Table used for fast compression from Iupacna to Ncbi4na
    // (2 bytes to 1 byte). Similar to m_FastIupacnaNcbi2na
    CRef<CFast_2_1> m_FastIupacnaNcbi4na;

    // Table used for fast compression from Ncbi4na to Ncbi2na
    // (2 bytes to 1 byte). Similar to m_FastIupacnaNcbi4na
    CRef<CFast_2_1> m_FastNcbi4naNcbi2na;

    // Helper function to initialize fast conversion tables
    CRef<CFast_table4> InitFastNcbi2naIupacna();
    CRef<CFast_table2> InitFastNcbi2naNcbi4na();
    CRef<CFast_table2> InitFastNcbi4naIupacna();

    CRef<CFast_4_1>    InitFastIupacnaNcbi2na();
    CRef<CFast_2_1>    InitFastIupacnaNcbi4na();
    CRef<CFast_2_1>    InitFastNcbi4naNcbi2na();

    // Data members and functions used for random disambiguation

    // structure used for ncbi4na --> ncbi2na
    struct SMasksArray : public CObject
    {
        // Structure to hold all masks applicable to an input byte
        struct SMasks {
            int           nMasks;
            unsigned char cMask[16];
        };
        SMasks m_Table[256];
    };

    CRef<SMasksArray> m_Masks;

    // Helper function to initialize m_Masks
    CRef<SMasksArray> InitMasks();

    // Data members used for detecting ambiguities

    // Data members used by GetAmbig methods to get a list of
    // ambiguities resulting from alphabet conversions
    CRef<CAmbig_detect> m_DetectAmbigNcbi4naNcbi2na;
    CRef<CAmbig_detect> m_DetectAmbigIupacnaNcbi2na;

    // Helper functiond to initialize m_Detect_Ambig_ data members
    CRef<CAmbig_detect> InitAmbigNcbi4naNcbi2na();
    CRef<CAmbig_detect> InitAmbigIupacnaNcbi2na();

    // Alphabet conversion functions. Functions return
    // the number of converted codes.

    // Fuction to convert ncbi2na (1 byte) to iupacna (4 bytes)
    unsigned int MapNcbi2naToIupacna(const CSeq_data&  in_seq,
                                     CSeq_data*        out_seq,
                                     unsigned int      uBeginIdx,
                                     unsigned int      uLength)
        const;

    // Function to convert ncbi2na (1 byte) to ncbi4na (2 bytes)
    unsigned int MapNcbi2naToNcbi4na(const CSeq_data&  in_seq,
                                     CSeq_data*        out_seq,
                                     unsigned int      uBeginIdx,
                                     unsigned int      uLength)
        const;

    // Function to convert ncbi4na (1 byte) to iupacna (2 bytes)
    unsigned int MapNcbi4naToIupacna(const CSeq_data& in_seq,
                                     CSeq_data*       out_seq,
                                     unsigned int     uBeginIdx,
                                     unsigned int     uLength)
        const;

    // Function to convert iupacna (4 bytes) to ncbi2na (1 byte)
    unsigned int MapIupacnaToNcbi2na(const CSeq_data& in_seq,
                                     CSeq_data*       out_seq,
                                     unsigned int     uBeginIdx,
                                     unsigned int     uLength,
                                     bool             bAmbig,
                                     CRandom::TValue  seed)
        const;

    // Function to convert iupacna (2 bytes) to ncbi4na (1 byte)
    unsigned int MapIupacnaToNcbi4na(const CSeq_data& in_seq,
                                     CSeq_data*       out_seq,
                                     unsigned int     uBeginIdx,
                                     unsigned int     uLength)
        const;

    // Function to convert ncbi4na (2 bytes) to ncbi2na (1 byte)
    unsigned int MapNcbi4naToNcbi2na(const CSeq_data& in_seq,
                                     CSeq_data*       out_seq,
                                     unsigned int     uBeginIdx,
                                     unsigned int     uLength,
                                     bool             bAmbig,
                                     CRandom::TValue  seed)
        const;

    // Function to convert iupacaa (byte) to ncbieaa (byte)
    unsigned int MapIupacaaToNcbieaa(const CSeq_data& in_seq,
                                     CSeq_data*       out_seq,
                                     unsigned int     uBeginIdx,
                                     unsigned int     uLength) const;

    // Function to convert ncbieaa (byte) to iupacaa (byte)
    unsigned int MapNcbieaaToIupacaa(const CSeq_data& in_seq,
                                     CSeq_data*       out_seq,
                                     unsigned int     uBeginIdx,
                                     unsigned int     uLength)
        const;

    // Function to convert iupacaa (byte) to ncbistdaa (byte)
    unsigned int MapIupacaaToNcbistdaa(const CSeq_data& in_seq,
                                       CSeq_data*       out_seq,
                                       unsigned int     uBeginIdx,
                                       unsigned int     uLength)
        const;

    // Function to convert ncbieaa (byte) to ncbistdaa (byte)
    unsigned int MapNcbieaaToNcbistdaa(const CSeq_data& in_seq,
                                       CSeq_data*       out_seq,
                                       unsigned int     uBeginIdx,
                                       unsigned int     uLength)
        const;

    // Function to convert ncbistdaa (byte) to ncbieaa (byte)
    unsigned int MapNcbistdaaToNcbieaa(const CSeq_data& in_seq,
                                       CSeq_data*       out_seq,
                                       unsigned int     uBeginIdx,
                                       unsigned int     uLength)
        const;

    // Function to convert ncbistdaa (byte) to iupacaa (byte)
    unsigned int MapNcbistdaaToIupacaa(const CSeq_data& in_seq,
                                       CSeq_data*       out_seq,
                                       unsigned int     uBeginIdx,
                                       unsigned int     uLength)
        const;

    // Fast Validation functions
    bool FastValidateIupacna(const CSeq_data& in_seq,
                             unsigned int     uBeginIdx,
                             unsigned int     uLength)
        const;

    bool FastValidateNcbieaa(const CSeq_data& in_seq,
                             unsigned int     uBeginIdx,
                             unsigned int     uLength)
        const;


    bool FastValidateNcbistdaa(const CSeq_data& in_seq,
                               unsigned int     uBeginIdx,
                               unsigned int     uLength)
        const;


    bool FastValidateIupacaa(const CSeq_data& in_seq,
                             unsigned int     uBeginIdx,
                             unsigned int     uLength)
        const;

    // Full Validation functions
    void ValidateIupacna(const CSeq_data&       in_seq,
                         vector<unsigned int>*  badIdx,
                         unsigned int           uBeginIdx,
                         unsigned int           uLength)
        const;

    void ValidateNcbieaa(const CSeq_data&       in_seq,
                         vector<unsigned int>*  badIdx,
                         unsigned int           uBeginIdx,
                         unsigned int           uLength)
        const;

    void ValidateNcbistdaa(const CSeq_data&       in_seq,
                           vector<unsigned int>*  badIdx,
                           unsigned int           uBeginIdx,
                           unsigned int           uLength)
        const;

    void ValidateIupacaa(const CSeq_data&       in_seq,
                         vector<unsigned int>*  badIdx,
                         unsigned int           uBeginIdx,
                         unsigned int           uLength)
        const;

    // Functions to make copies of the different types of sequences
    unsigned int GetNcbi2naCopy(const CSeq_data& in_seq,
                                CSeq_data*       out_seq,
                                unsigned int     uBeginIdx,
                                unsigned int     uLength)
        const;

    unsigned int GetNcbi4naCopy(const CSeq_data& in_seq,
                                CSeq_data*       out_seq,
                                unsigned int     uBeginIdx,
                                unsigned int     uLength)
        const;

    unsigned int GetIupacnaCopy(const CSeq_data& in_seq,
                                CSeq_data*       out_seq,
                                unsigned int     uBeginIdx,
                                unsigned int     uLength)
        const;

    unsigned int GetNcbieaaCopy(const CSeq_data& in_seq,
                                CSeq_data*       out_seq,
                                unsigned int     uBeginIdx,
                                unsigned int     uLength)
        const;

    unsigned int GetNcbistdaaCopy(const CSeq_data& in_seq,
                                  CSeq_data*       out_seq,
                                  unsigned int     uBeginIdx,
                                  unsigned int     uLength)
        const;

    unsigned int GetIupacaaCopy(const CSeq_data& in_seq,
                                CSeq_data*       out_seq,
                                unsigned int     uBeginIdx,
                                unsigned int     uLength)
        const;

    // Function to adjust uBeginIdx to lie on an in_seq byte boundary
    // and uLength to lie on on an out_seq byte boundary. Returns
    // overhang, the number of out seqs beyond byte boundary determined
    // by uBeginIdx + uLength
    unsigned int Adjust(unsigned int* uBeginIdx,
                        unsigned int* uLength,
                        unsigned int  uInSeqBytes,
                        unsigned int  uInSeqsPerByte,
                        unsigned int  uOutSeqsPerByte)
        const;

    // GetAmbig methods

    // Loops through an ncbi4na input sequence and determines
    // the ambiguities that would result from conversion to an ncbi2na sequence
    // On return, out_seq contains the ncbi4na bases that become ambiguous and
    // out_indices contains the indices of the abiguous bases in in_seq
    unsigned int GetAmbigs_ncbi4na_ncbi2na(const CSeq_data&       in_seq,
                                           CSeq_data*             out_seq,
                                           vector<unsigned int>*  out_indices,
                                           unsigned int           uBeginIdx,
                                           unsigned int           uLength)
        const;

    // Loops through an iupacna input sequence and determines
    // the ambiguities that would result from conversion to an ncbi2na sequence
    // On return, out_seq contains the iupacna bases that become ambiguous and
    // out_indices contains the indices of the abiguous bases in in_seq. The
    // return is the number of ambiguities found.
    unsigned int GetAmbigs_iupacna_ncbi2na(const CSeq_data&       in_seq,
                                           CSeq_data*             out_seq,
                                           vector<unsigned int>*  out_indices,
                                           unsigned int           uBeginIdx,
                                           unsigned int           uLength)
        const;

    // Methods to perform Keep on specific seq types. Methods
    // return length of kept sequence.
    unsigned int KeepNcbi2na(CSeq_data*       in_seq,
                             unsigned int     uBeginIdx,
                             unsigned int     uLength)
        const;

    unsigned int KeepNcbi4na(CSeq_data*       in_seq,
                             unsigned int     uBeginIdx,
                             unsigned int     uLength)
        const;

    unsigned int KeepIupacna(CSeq_data*       in_seq,
                             unsigned int     uBeginIdx,
                             unsigned int     uLength)
        const;

    unsigned int KeepNcbieaa(CSeq_data*       in_seq,
                             unsigned int     uBeginIdx,
                             unsigned int     uLength)
        const;

    unsigned int KeepNcbistdaa(CSeq_data*     in_seq,
                               unsigned int   uBeginIdx,
                               unsigned int   uLength)
        const;

    unsigned int KeepIupacaa(CSeq_data*       in_seq,
                             unsigned int     uBeginIdx,
                             unsigned int     uLength)
        const;

    // Methods to complement na sequences

    // In place methods. Return number of complemented residues.
    unsigned int ComplementIupacna(CSeq_data*    in_seq,
                                   unsigned int  uBeginIdx,
                                   unsigned int  uLength)
        const;

    unsigned int ComplementNcbi2na(CSeq_data*    in_seq,
                                   unsigned int  uBeginIdx,
                                   unsigned int  uLength)
        const;

    unsigned int ComplementNcbi4na(CSeq_data*    in_seq,
                                   unsigned int  uBeginIdx,
                                   unsigned int  uLength)
        const;


    // Complement in copy methods
    unsigned int ComplementIupacna(const CSeq_data&  in_seq,
                                   CSeq_data*        out_seq,
                                   unsigned int      uBeginIdx,
                                   unsigned int      uLength)
        const;

    unsigned int ComplementNcbi2na(const CSeq_data&  in_seq,
                                   CSeq_data*        out_seq,
                                   unsigned int      uBeginIdx,
                                   unsigned int      uLength)
        const;

    unsigned int ComplementNcbi4na(const CSeq_data&  in_seq,
                                   CSeq_data*        out_seq,
                                   unsigned int      uBeginIdx,
                                   unsigned int      uLength)
        const;


    // Methods to reverse na sequences

    // In place methods
    unsigned int ReverseIupacna(CSeq_data*    in_seq,
                                unsigned int  uBeginIdx,
                                unsigned int  uLength)
        const;

    unsigned int ReverseNcbi2na(CSeq_data*    in_seq,
                                unsigned int  uBeginIdx,
                                unsigned int  uLength)
        const;

    unsigned int ReverseNcbi4na(CSeq_data*    in_seq,
                                unsigned int  uBeginIdx,
                                unsigned int  uLength)
        const;

    // Reverse in copy methods
    unsigned int ReverseIupacna(const CSeq_data&  in_seq,
                                CSeq_data*        out_seq,
                                unsigned int      uBeginIdx,
                                unsigned int      uLength)
        const;

    unsigned int ReverseNcbi2na(const CSeq_data&  in_seq,
                                CSeq_data*        out_seq,
                                unsigned int      uBeginIdx,
                                unsigned int      uLength)
        const;

    unsigned int ReverseNcbi4na(const CSeq_data&  in_seq,
                                CSeq_data*        out_seq,
                                unsigned int      uBeginIdx,
                                unsigned int      uLength)
        const;

    // Methods to reverse-complement an na sequences

    // In place methods
    unsigned int ReverseComplementIupacna(CSeq_data*    in_seq,
                                          unsigned int  uBeginIdx,
                                          unsigned int  uLength)
        const;

    unsigned int ReverseComplementNcbi2na(CSeq_data*    in_seq,
                                          unsigned int  uBeginIdx,
                                          unsigned int  uLength)
        const;

    unsigned int ReverseComplementNcbi4na(CSeq_data*    in_seq,
                                          unsigned int  uBeginIdx,
                                          unsigned int  uLength)
        const;

    // Reverse in copy methods
    unsigned int ReverseComplementIupacna(const CSeq_data& in_seq,
                                          CSeq_data*       out_seq,
                                          unsigned int     uBeginIdx,
                                          unsigned int     uLength)
        const;

    unsigned int ReverseComplementNcbi2na(const CSeq_data& in_seq,
                                          CSeq_data*       out_seq,
                                          unsigned int     uBeginIdx,
                                          unsigned int     uLength)
        const;

    unsigned int ReverseComplementNcbi4na(const CSeq_data& in_seq,
                                          CSeq_data*       out_seq,
                                          unsigned int     uBeginIdx,
                                          unsigned int     uLength)
        const;

    // Append methods
    unsigned int AppendIupacna(CSeq_data*          out_seq,
                               const CSeq_data&    in_seq1,
                               unsigned int        uBeginIdx1,
                               unsigned int        uLength1,
                               const CSeq_data&    in_seq2,
                               unsigned int        uBeginIdx2,
                               unsigned int        uLength2)
        const;

    unsigned int AppendNcbi2na(CSeq_data*          out_seq,
                               const CSeq_data&    in_seq1,
                               unsigned int        uBeginIdx1,
                               unsigned int        uLength1,
                               const CSeq_data&    in_seq2,
                               unsigned int        uBeginIdx2,
                               unsigned int        uLength2)
        const;

    unsigned int AppendNcbi4na(CSeq_data*          out_seq,
                               const CSeq_data&    in_seq1,
                               unsigned int        uBeginIdx1,
                               unsigned int        uLength1,
                               const CSeq_data&    in_seq2,
                               unsigned int        uBeginIdx2,
                               unsigned int        uLength2)
        const;

    unsigned int AppendNcbieaa(CSeq_data*          out_seq,
                               const CSeq_data&    in_seq1,
                               unsigned int        uBeginIdx1,
                               unsigned int        uLength1,
                               const CSeq_data&    in_seq2,
                               unsigned int        uBeginIdx2,
                               unsigned int        uLength2)
        const;

    unsigned int AppendNcbistdaa(CSeq_data*        out_seq,
                                 const CSeq_data&  in_seq1,
                                 unsigned int      uBeginIdx1,
                                 unsigned int      uLength1,
                                 const CSeq_data&  in_seq2,
                                 unsigned int      uBeginIdx2,
                                 unsigned int      uLength2)
        const;

    unsigned int AppendIupacaa(CSeq_data*          out_seq,
                               const CSeq_data&    in_seq1,
                               unsigned int        uBeginIdx1,
                               unsigned int        uLength1,
                               const CSeq_data&    in_seq2,
                               unsigned int        uBeginIdx2,
                               unsigned int        uLength2)
        const;


    // Prevent construction and assigment.
    CSeqportUtil();
    CSeqportUtil(const CSeqportUtil&);
    CSeqportUtil& operator= (const CSeqportUtil&);
    ~CSeqportUtil();

    // The unique instance of CSeqportUtil
    static CSafeStaticPtr<CSeqportUtil> sm_pInstance;

    // Make CSafeStaticPtr<CSeqportUtil> a friend so it can
    // call constructor
    friend class CSafeStaticPtr<CSeqportUtil>;
};


END_objects_SCOPE
END_NCBI_SCOPE

#endif  /* OBJECTS_SEQ___SEQPORT_UTIL__HPP */
