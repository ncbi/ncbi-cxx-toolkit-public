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
 * Revision 6.4  2001/10/17 13:04:30  clausen
 * Fixed InitFastNcbi2naIupacna to remove hardware dependency
 *
 * Revision 6.3  2001/09/07 14:16:50  ucko
 * Cleaned up external interface.
 *
 * Revision 6.2  2001/09/06 20:43:32  ucko
 * Fix iterator types (caught by gcc 3.0.1).
 *
 * Revision 6.1  2001/08/24 00:34:23  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include <objects/seq/seqport_util.hpp>

#include <serial/serial.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>

#include <objects/seq/NCBI2na.hpp>
#include <objects/seq/NCBI4na.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/IUPACaa.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/NCBIstdaa.hpp>

#include <objects/seqcode/Seq_code_set.hpp>
#include <objects/seqcode/Seq_code_table.hpp>
#include <objects/seqcode/Seq_code_type.hpp>
#include <objects/seqcode/Seq_map_table.hpp>

#include <algorithm>
#include <string.h>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


// CSeqportUtil_implementation is a singleton.

class CSeqportUtil_implementation {
public:
    CSeqportUtil_implementation();
    ~CSeqportUtil_implementation();

    unsigned int Convert
    (const CSeq_data&       in_seq,
     CSeq_data*             out_seq,
     CSeq_data::E_Choice    to_code,
     unsigned int           uBeginIdx,
     unsigned int           uLength,
     bool                   bAmbig,
     CRandom::TValue        seed)
        const;

    unsigned int Pack
    (CSeq_data*     in_seq,
     unsigned int   uBeginidx,
     unsigned int   uLength)
        const;

    bool FastValidate
    (const CSeq_data&   in_seq,
     unsigned int       uBeginIdx,
     unsigned int       uLength)
        const;

    void Validate
    (const CSeq_data&       in_seq,
     vector<unsigned int>*  badIdx,
     unsigned int           uBeginIdx,
     unsigned int           uLength)
        const;

    unsigned int GetAmbigs
    (const CSeq_data&       in_seq,
     CSeq_data*             out_seq,
     vector<unsigned int>*  out_indices,
     CSeq_data::E_Choice    to_code,
     unsigned int           uBeginIdx,
     unsigned int           uLength)
        const;

    unsigned int GetCopy
    (const CSeq_data&       in_seq,
     CSeq_data*             out_seq,
     unsigned int           uBeginIdx,
     unsigned int           uLength)
        const;

    unsigned int Keep
    (CSeq_data*      in_seq,
     unsigned int     uBeginIdx,
     unsigned int     uLength)
        const;

    unsigned int Append
    (CSeq_data*             out_seq,
     const CSeq_data&       in_seq1,
     unsigned int           uBeginIdx1,
     unsigned int           uLength1,
     const CSeq_data&       in_seq2,
     unsigned int           uBeginIdx2,
     unsigned int           uLength2)
        const;

    unsigned int Complement
    (CSeq_data*       in_seq,
     unsigned int     uBeginIdx,
     unsigned int     uLength)
        const;

    unsigned int Complement
    (const CSeq_data&       in_seq,
     CSeq_data*             out_seq,
     unsigned int           uBeginIdx,
     unsigned int           uLength)
        const;

    unsigned int Reverse
    (CSeq_data*       in_seq,
     unsigned int     uBeginIdx,
     unsigned int     uLength)
        const;

    unsigned int Reverse
    (const CSeq_data&  in_seq,
     CSeq_data*        out_seq,
     unsigned int      uBeginIdx,
     unsigned int      uLength)
        const;

    unsigned int ReverseComplement
    (CSeq_data*       in_seq,
     unsigned int     uBeginIdx,
     unsigned int     uLength)
        const;

    unsigned int ReverseComplement
    (const CSeq_data&   in_seq,
     CSeq_data*         out_seq,
     unsigned int       uBeginIdx,
     unsigned int       uLength)
        const;

private:

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
};

static CSeqportUtil_implementation s_Implementation;



/////////////////////////////////////////////////////////////////////////////
//  PUBLIC (static wrappers to CSeqportUtil_implementation public methods)::
//


unsigned int CSeqportUtil::Convert
(const CSeq_data&        in_seq,
 CSeq_data*              out_seq,
 CSeq_data::E_Choice     to_code,
 unsigned int            uBeginIdx,
 unsigned int            uLength,
 bool                    bAmbig,
 CRandom::TValue         seed)
{
    return s_Implementation.Convert
        (in_seq, out_seq, to_code, uBeginIdx, uLength, bAmbig, seed);
}


unsigned int CSeqportUtil::Pack
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
{
    return s_Implementation.Pack
        (in_seq, uBeginIdx, uLength);
}


bool CSeqportUtil::FastValidate
(const CSeq_data&        in_seq,
 unsigned int            uBeginIdx,
 unsigned int            uLength)
{
    return s_Implementation.FastValidate
        (in_seq, uBeginIdx, uLength);
}


void CSeqportUtil::Validate
(const CSeq_data&        in_seq,
 vector<unsigned int>*   badIdx,
 unsigned int            uBeginIdx,
 unsigned int            uLength)
{
    s_Implementation.Validate
        (in_seq, badIdx, uBeginIdx, uLength);
}


unsigned int CSeqportUtil::GetAmbigs
(const CSeq_data&        in_seq,
 CSeq_data*              out_seq,
 vector<unsigned int>*   out_indices,
 CSeq_data::E_Choice     to_code,
 unsigned int            uBeginIdx,
 unsigned int            uLength)
{
    return s_Implementation.GetAmbigs
        (in_seq, out_seq, out_indices, to_code, uBeginIdx, uLength);
}


unsigned int CSeqportUtil::GetCopy
(const CSeq_data&        in_seq,
 CSeq_data*              out_seq,
 unsigned int            uBeginIdx,
 unsigned int            uLength)
{
    return s_Implementation.GetCopy
        (in_seq, out_seq, uBeginIdx, uLength);
}



unsigned int CSeqportUtil::Keep
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
{
    return s_Implementation.Keep
        (in_seq, uBeginIdx, uLength);
}


unsigned int CSeqportUtil::Append
(CSeq_data*              out_seq,
 const CSeq_data&        in_seq1,
 unsigned int            uBeginIdx1,
 unsigned int            uLength1,
 const CSeq_data&        in_seq2,
 unsigned int            uBeginIdx2,
 unsigned int            uLength2)
{
    return s_Implementation.Append
        (out_seq,
         in_seq1, uBeginIdx1, uLength1, in_seq2, uBeginIdx2, uLength2);
}


unsigned int CSeqportUtil::Complement
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
{
    return s_Implementation.Complement
        (in_seq, uBeginIdx, uLength);
}


unsigned int CSeqportUtil::Complement
(const CSeq_data&        in_seq,
 CSeq_data*              out_seq,
 unsigned int            uBeginIdx,
 unsigned int            uLength)
{
    return s_Implementation.Complement
        (in_seq, out_seq, uBeginIdx, uLength);
}


unsigned int CSeqportUtil::Reverse
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
{
    return s_Implementation.Reverse
        (in_seq, uBeginIdx, uLength);
}


unsigned int CSeqportUtil::Reverse
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
{
    return s_Implementation.Reverse
        (in_seq, out_seq, uBeginIdx, uLength);
}


unsigned int CSeqportUtil::ReverseComplement
(CSeq_data*    in_seq,
 unsigned int  uBeginIdx,
 unsigned int  uLength)
{
    return s_Implementation.ReverseComplement
        (in_seq, uBeginIdx, uLength);
}


unsigned int CSeqportUtil::ReverseComplement
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
{
    return s_Implementation.ReverseComplement
        (in_seq, out_seq, uBeginIdx, uLength);
}



CSeqportUtil_implementation::CSeqportUtil_implementation()
{

    // Initialize m_SeqCodeSet
    m_SeqCodeSet = Init();

    // Initialize code tables
    m_Iupacna = InitCodes(eSeq_code_type_iupacna);

    m_Ncbieaa = InitCodes(eSeq_code_type_ncbieaa);

    m_Ncbistdaa = InitCodes(eSeq_code_type_ncbistdaa);

    m_Iupacaa = InitCodes(eSeq_code_type_iupacaa);


    // Initialize na complement tables
    m_Iupacna_complement = InitIupacnaComplement();

    m_Ncbi2naComplement = InitNcbi2naComplement();

    m_Ncbi4naComplement = InitNcbi4naComplement();



    // Initialize na reverse tables
    m_Ncbi2naRev = InitNcbi2naRev();

    m_Ncbi4naRev = InitNcbi4naRev();


    // Initialize map tables
    m_Ncbi2naIupacna = InitMaps(eSeq_code_type_ncbi2na,
                                eSeq_code_type_iupacna);

    m_Ncbi2naNcbi4na = InitMaps(eSeq_code_type_ncbi2na,
                                eSeq_code_type_ncbi4na);

    m_Ncbi4naIupacna = InitMaps(eSeq_code_type_ncbi4na,
                                eSeq_code_type_iupacna);

    m_IupacnaNcbi2na = InitMaps(eSeq_code_type_iupacna,
                                eSeq_code_type_ncbi2na);

    m_IupacnaNcbi4na = InitMaps(eSeq_code_type_iupacna,
                                eSeq_code_type_ncbi4na);

    m_Ncbi4naNcbi2na = InitMaps(eSeq_code_type_ncbi4na,
                                eSeq_code_type_ncbi2na);

    m_IupacaaNcbieaa = InitMaps(eSeq_code_type_iupacaa,
                                eSeq_code_type_ncbieaa);

    m_NcbieaaIupacaa = InitMaps(eSeq_code_type_ncbieaa,
                                eSeq_code_type_iupacaa);

    m_IupacaaNcbistdaa = InitMaps(eSeq_code_type_iupacaa,
                                  eSeq_code_type_ncbistdaa);

    m_NcbieaaNcbistdaa = InitMaps(eSeq_code_type_ncbieaa,
                                  eSeq_code_type_ncbistdaa);

    m_NcbistdaaNcbieaa = InitMaps(eSeq_code_type_ncbistdaa,
                                  eSeq_code_type_ncbieaa);

    m_NcbistdaaIupacaa = InitMaps(eSeq_code_type_ncbistdaa,
                                  eSeq_code_type_iupacaa);

    // Initialize fast conversion tables
    m_FastNcbi2naIupacna = InitFastNcbi2naIupacna();
    m_FastNcbi2naNcbi4na = InitFastNcbi2naNcbi4na();
    m_FastNcbi4naIupacna = InitFastNcbi4naIupacna();
    m_FastIupacnaNcbi2na = InitFastIupacnaNcbi2na();
    m_FastIupacnaNcbi4na = InitFastIupacnaNcbi4na();
    m_FastNcbi4naNcbi2na = InitFastNcbi4naNcbi2na();

    // Initialize m_Masks used for random ambiguity resolution
    m_Masks = CSeqportUtil_implementation::InitMasks();

    // Initialize m_DetectAmbigNcbi4naNcbi2na used for ambiguity
    // detection and reporting
    m_DetectAmbigNcbi4naNcbi2na = InitAmbigNcbi4naNcbi2na();

    // Initialize m_DetectAmbigIupacnaNcbi2na used for ambiguity detection
    // and reporting
    m_DetectAmbigIupacnaNcbi2na = InitAmbigIupacnaNcbi2na();

}

// Destructor. All memory allocated on the
// free store is wrapped in smart pointers.
// Therefore, the destructor does not need
// to deallocate memory.
CSeqportUtil_implementation::~CSeqportUtil_implementation()
{
    return;
}


/////////////////////////////////////////////////////////////////////////////
//  PRIVATE::
//


// Helper function to initialize m_SeqCodeSet from sm_StrAsnData
CRef<CSeq_code_set> CSeqportUtil_implementation::Init()
{
    // Compose a long-long string
    string str;
    for (size_t i = 0;  sm_StrAsnData[i];  i++) {
        str += sm_StrAsnData[i];
    }

    // Create an in memory stream on sm_StrAsnData
    CNcbiIstrstream is(str.c_str(), strlen(str.c_str()));

    auto_ptr<CObjectIStream>
        asn_codes_in(CObjectIStream::Open(eSerial_AsnText, is));

    // Create a CSeq_code_set
    CRef<CSeq_code_set> ptr_seq_code_set(new CSeq_code_set());

    // Initialize the newly created CSeq_code_set
    *asn_codes_in >> *ptr_seq_code_set;

    // Return a newly created CSeq_code_set
    return ptr_seq_code_set;
}


// Function to initialize code tables
CRef<CSeqportUtil_implementation::CCode_table>
CSeqportUtil_implementation::InitCodes(ESeq_code_type code_type)
{
    // Get list of code tables
    const list<CRef<CSeq_code_table> >& code_list = m_SeqCodeSet->GetCodes();

    // Get table for code_type
    list<CRef<CSeq_code_table> >::const_iterator i_ct;
    for(i_ct = code_list.begin(); i_ct != code_list.end(); ++i_ct)
        if((*i_ct)->GetCode() == code_type)
            break;


    if(i_ct == code_list.end())
        throw runtime_error("Requested code table not found");

    // Get table data
    const list<CRef<CSeq_code_table::C_E> >& table_data = (*i_ct)->GetTable();
    int size = table_data.size();
    int start_at = (*i_ct)->GetStart_at();
    CRef<CCode_table> codeTable(new CCode_table(size, start_at));

    // Initialize codeTable to 255
    for(int i=0; i<256; i++)
        codeTable->m_Table[i] = '\xff';

    // Copy table data to codeTable
    int nIdx = start_at;
    list<CRef<CSeq_code_table::C_E> >::const_iterator i_td;
    for(i_td = table_data.begin(); i_td != table_data.end(); ++i_td) {
        codeTable->m_Table[nIdx] =  *((*i_td)->GetSymbol().c_str());
        if(codeTable->m_Table[nIdx] == '\x00')
            codeTable->m_Table[nIdx++] = '\xff';
        else
            nIdx++;
    }

    // Return codeTable
    return codeTable;
}


// Function to initialize iupacna complement table
CRef<CSeqportUtil_implementation::CCode_comp>
CSeqportUtil_implementation::InitIupacnaComplement()
{

    // Get list of code tables
    const list<CRef<CSeq_code_table> >& code_list = m_SeqCodeSet->GetCodes();

    // Get table for code_type iupacna
    list<CRef<CSeq_code_table> >::const_iterator i_ct;
    for(i_ct = code_list.begin(); i_ct != code_list.end(); ++i_ct)
        if((*i_ct)->GetCode() == eSeq_code_type_iupacna)
            break;


    if(i_ct == code_list.end())
        throw runtime_error("Code table for Iupacna not found");

    // Check that complements are set
    if(!(*i_ct)->IsSetComps())
        throw runtime_error("Complement data is not set for iupacna table");

    // Get complement data, start at and size of complement data
    const list<int>& comp_data = (*i_ct)->GetComps();
    int start_at = (*i_ct)->GetStart_at();

    // Allocate memory for complement data
    CRef<CCode_comp> compTable(new CCode_comp(256, start_at));

    // Initialize compTable to 255 for illegal codes
    for(unsigned int i = 0; i<256; i++)
        compTable->m_Table[i] = (char) 255;

    // Loop trhough the complement data and set compTable
    list<int>::const_iterator i_comp;
    unsigned int nIdx = start_at;
    for(i_comp = comp_data.begin(); i_comp != comp_data.end(); ++i_comp)
        compTable->m_Table[nIdx++] = (*i_comp);

    // Return the complement data
    return compTable;

}


// Function to initialize ncbi2na complement table
CRef<CSeqportUtil_implementation::CCode_comp>
CSeqportUtil_implementation::InitNcbi2naComplement()
{

    // Get list of code tables
    const list<CRef<CSeq_code_table> >& code_list = m_SeqCodeSet->GetCodes();

    // Get table for code_type ncbi2na
    list<CRef<CSeq_code_table> >::const_iterator i_ct;
    for(i_ct = code_list.begin(); i_ct != code_list.end(); ++i_ct)
        if((*i_ct)->GetCode() == eSeq_code_type_ncbi2na)
            break;

    if(i_ct == code_list.end())
        throw runtime_error("Code table for Iupacna not found");

    // Check that complements are set
    if(!(*i_ct)->IsSetComps())
        throw runtime_error("Complement data is not set for ncbi2na table");

    // Get complement data, start at and size of complement data
    const list<int>& comp_data = (*i_ct)->GetComps();
    int start_at = (*i_ct)->GetStart_at();

    // Allocate memory for complement data
    CRef<CCode_comp> compTable(new CCode_comp(256, start_at));

    // Put complement data in an array
    char compArray[4];
    int nIdx = start_at;
    list<int>::const_iterator i_comp;
    for(i_comp = comp_data.begin(); i_comp != comp_data.end(); ++i_comp)
        compArray[nIdx++] = (*i_comp);

    // Set compTable
    for(unsigned int i = 0; i < 4; i++)
        for(unsigned int j = 0; j < 4; j++)
            for(unsigned int k = 0; k < 4; k++)
                for(unsigned int l = 0; l < 4; l++)
                    {
                        nIdx = i<<6 | j<<4 | k<<2 | l;
                        char c1 = compArray[i] << 6;
                        char c2 = compArray[j] << 4;
                        char c3 = compArray[k] << 2;
                        char c4 = compArray[l];
                        compTable->m_Table[nIdx] = c1 | c2 | c3 | c4;
                    }

    // Return complement data
    return compTable;

}


// Function to initialize ncbi4na complement table
CRef<CSeqportUtil_implementation::CCode_comp>
CSeqportUtil_implementation::InitNcbi4naComplement()
{

    // Get list of code tables
    const list<CRef<CSeq_code_table> >& code_list = m_SeqCodeSet->GetCodes();

    // Get table for code_type ncbi2na
    list<CRef<CSeq_code_table> >::const_iterator i_ct;
    for(i_ct = code_list.begin(); i_ct != code_list.end(); ++i_ct)
        if((*i_ct)->GetCode() == eSeq_code_type_ncbi4na)
            break;

    if(i_ct == code_list.end())
        throw runtime_error("Code table for Iupacna not found");

    // Check that complements are set
    if(!(*i_ct)->IsSetComps())
        throw runtime_error("Complement data is not set for iupacna table");

    // Get complement data, start at and size of complement data
    const list<int>& comp_data = (*i_ct)->GetComps();
    int start_at = (*i_ct)->GetStart_at();

    // Allocate memory for complement data
    CRef<CCode_comp> compTable(new CCode_comp(256, start_at));


    // Put complement data in an array
    char compArray[16];
    int nIdx = start_at;
    list<int>::const_iterator i_comp;
    for(i_comp = comp_data.begin(); i_comp != comp_data.end(); ++i_comp)
        compArray[nIdx++] = (*i_comp);

    // Set compTable
    for(unsigned int i = 0; i<16; i++)
        for(unsigned int j = 0; j < 16; j++)
            {
                nIdx = i<<4 | j;
                char c1 = compArray[i] << 4;
                char c2 = compArray[j];
                compTable->m_Table[nIdx] = c1 | c2;
            }

    // Return complement data
    return compTable;

}


// Function to initialize m_Ncbi2naRev
CRef<CSeqportUtil_implementation::CCode_rev> CSeqportUtil_implementation::InitNcbi2naRev()
{

    // Allocate memory for reverse table
    CRef<CCode_rev> revTable(new CCode_rev(256, 0));

    // Initialize table used to reverse a byte.
    for(unsigned int i = 0; i < 4; i++)
        for(unsigned int j = 0; j < 4; j++)
            for(unsigned int k = 0; k < 4; k++)
                for(unsigned int l = 0; l < 4; l++)
                    revTable->m_Table[64*i + 16*j + 4*k + l] =
                        64*l + 16*k + 4*j +i;

    // Return the reverse table
    return revTable;
}


// Function to initialize m_Ncbi4naRev
CRef<CSeqportUtil_implementation::CCode_rev> CSeqportUtil_implementation::InitNcbi4naRev()
{

    // Allocate memory for reverse table
    CRef<CCode_rev> revTable(new CCode_rev(256, 0));

    // Initialize table used to reverse a byte.
    for(unsigned int i = 0; i < 16; i++)
        for(unsigned int j = 0; j < 16; j++)
            revTable->m_Table[16*i + j] = 16*j + i;

    // Return the reverse table
    return revTable;
}



// Function to initialize map tables
CRef<CSeqportUtil_implementation::CMap_table> CSeqportUtil_implementation::InitMaps
(ESeq_code_type from_type,
 ESeq_code_type to_type)
{

    // Get list of map tables
    const list< CRef< CSeq_map_table > >& map_list = m_SeqCodeSet->GetMaps();

    // Get requested map table
    list<CRef<CSeq_map_table> >::const_iterator i_mt;
    for(i_mt = map_list.begin(); i_mt != map_list.end(); ++i_mt)
        if((*i_mt)->GetFrom() == from_type && (*i_mt)->GetTo() == to_type)
            break;

    if(i_mt == map_list.end())
        throw runtime_error("Requested map table not found");

    // Get the map table
    const list<int>& table_data = (*i_mt)->GetTable();

    // Create a map table reference
    int size = table_data.size();
    int start_at = (*i_mt)->GetStart_at();
    CRef<CMap_table> mapTable(new CMap_table(size,start_at));

    // Copy the table data to mapTable
    int nIdx = start_at;
    list<int>::const_iterator i_td;
    for(i_td = table_data.begin(); i_td != table_data.end(); ++i_td)
        {
            mapTable->m_Table[nIdx++] = *i_td;
        }

    return mapTable;
}


// Functions to initialize fast conversion tables
// Function to initialize FastNcib2naIupacna
CRef<CSeqportUtil_implementation::CFast_table4> CSeqportUtil_implementation::InitFastNcbi2naIupacna()
{

    CRef<CFast_table4> fastTable(new CFast_table4(256,0));
    unsigned char i,j,k,l;
    for(i = 0; i < 4; i++)
        for(j = 0; j < 4; j++)
            for(k = 0; k < 4; k++)
                for(l = 0; l < 4; l++)
                    {
                        unsigned char aByte = (i<<6) | (j<<4) | (k<<2) | l;
                        char chi = m_Ncbi2naIupacna->m_Table[i];
                        char chj = m_Ncbi2naIupacna->m_Table[j];
                        char chk = m_Ncbi2naIupacna->m_Table[k];
                        char chl = m_Ncbi2naIupacna->m_Table[l];

                        // Note high order bit pair corresponds to low order
                        // byte etc., on Unix machines.
                        char *pt = 
                            reinterpret_cast<char*>(&fastTable->m_Table[aByte]);
                        *(pt++) = chi;
                        *(pt++) = chj;
                        *(pt++) = chk;
                        *(pt) = chl;                       
                     }
    m_Ncbi2naIupacna->drop_table();
    return fastTable;
}


// Function to initialize FastNcib2naNcbi4na
CRef<CSeqportUtil_implementation::CFast_table2> CSeqportUtil_implementation::InitFastNcbi2naNcbi4na()
{

    CRef<CFast_table2> fastTable(new CFast_table2(256,0));
    unsigned char i, j, k, l;

    for(i = 0; i < 4; i++)
        for(j = 0; j < 4; j++)
            for(k = 0; k < 4; k++)
                for(l = 0; l < 4; l++) {
                    unsigned char aByte = (i<<6) | (j<<4) | (k<<2) | l;
                    unsigned char chi = m_Ncbi2naNcbi4na->m_Table[i];
                    unsigned char chj = m_Ncbi2naNcbi4na->m_Table[j];
                    unsigned char chk = m_Ncbi2naNcbi4na->m_Table[k];
                    unsigned char chl = m_Ncbi2naNcbi4na->m_Table[l];
                    fastTable->m_Table[aByte] =
                        (chi << 12)|(chj << 8)|(chk << 4)|chl;
                }
    m_Ncbi2naNcbi4na->drop_table();
    return fastTable;
}


// Function to initialize FastNcib4naIupacna
CRef<CSeqportUtil_implementation::CFast_table2> CSeqportUtil_implementation::InitFastNcbi4naIupacna()
{

    CRef<CFast_table2> fastTable(new CFast_table2(256,0));
    unsigned char i,j;
    for(i = 0; i < 16; i++)
        for(j = 0; j < 16; j++) {
            unsigned char aByte = (i<<4) | j;
            unsigned char chi = m_Ncbi4naIupacna->m_Table[i];
            unsigned char chj = m_Ncbi4naIupacna->m_Table[j];

            // Note high order nible corresponds to low order byte
            // etc., on Unix machines.
            fastTable->m_Table[aByte] = (chi<<8) | chj;
        }
    m_Ncbi4naIupacna->drop_table();
    return fastTable;
}


// Function to initialize m_FastIupacnancbi2na
CRef<CSeqportUtil_implementation::CFast_4_1> CSeqportUtil_implementation::InitFastIupacnaNcbi2na()
{

    int start_at = m_IupacnaNcbi2na->m_StartAt;
    int size = m_IupacnaNcbi2na->m_Size;
    CRef<CFast_4_1> fastTable(new CFast_4_1(4,0,256,0));
    for(int ch = 0; ch < 256; ch++) {
        if((ch >= start_at) && (ch < (start_at + size)))
            {
                unsigned char uch = m_IupacnaNcbi2na->m_Table[ch];
                uch &= '\x03';
                for(unsigned int pos = 0; pos < 4; pos++)
                    fastTable->m_Table[pos][ch] = uch << (6-2*pos);
            }
        else
            for(unsigned int pos = 0; pos < 4; pos++)
                fastTable->m_Table[pos][ch] = '\x00';
    }
    m_IupacnaNcbi2na->drop_table();
    return fastTable;
}


// Function to initialize m_FastIupacnancbi4na
CRef<CSeqportUtil_implementation::CFast_2_1> CSeqportUtil_implementation::InitFastIupacnaNcbi4na()
{

    int start_at = m_IupacnaNcbi4na->m_StartAt;
    int size = m_IupacnaNcbi4na->m_Size;
    CRef<CFast_2_1> fastTable(new CFast_2_1(2,0,256,0));
    for(int ch = 0; ch < 256; ch++) {
        if((ch >= start_at) && (ch < (start_at + size)))
            {
                unsigned char uch = m_IupacnaNcbi4na->m_Table[ch];
                for(unsigned int pos = 0; pos < 2; pos++)
                    fastTable->m_Table[pos][ch] = uch << (4-4*pos);
            }
        else
            {
                fastTable->m_Table[0][ch] = '\xF0';
                fastTable->m_Table[1][ch] = '\x0F';
            }
    }
    m_IupacnaNcbi4na->drop_table();
    return fastTable;
}


// Function to initialize m_FastNcbi4naNcbi2na
CRef<CSeqportUtil_implementation::CFast_2_1> CSeqportUtil_implementation::InitFastNcbi4naNcbi2na()
{

    int start_at = m_Ncbi4naNcbi2na->m_StartAt;
    int size = m_Ncbi4naNcbi2na->m_Size;
    CRef<CFast_2_1> fastTable(new CFast_2_1(2,0,256,0));
    for(int n1 = 0; n1 < 16; n1++)
        for(int n2 = 0; n2 < 16; n2++) {
            int nIdx = 16*n1 + n2;
            unsigned char u1, u2;
            if((n1 >= start_at) && (n1 < start_at + size))
                u1 = m_Ncbi4naNcbi2na->m_Table[n1] & 3;
            else
                u1 = '\x00';
            if((n2 >= start_at) && (n2 < start_at + size))
                u2 = m_Ncbi4naNcbi2na->m_Table[n2] & 3;
            else
                u2 = '\x00';
            fastTable->m_Table[0][nIdx] = (u1<<6) | (u2<<4);
            fastTable->m_Table[1][nIdx] = (u1<<2) | u2;
        }

    m_Ncbi4naNcbi2na->drop_table();
    return fastTable;
}


// Function to initialize m_Masks
CRef<CSeqportUtil_implementation::SMasksArray> CSeqportUtil_implementation::InitMasks()
{

    unsigned int i, j, uCnt;
    unsigned char cVal, cRslt;
    CRef<SMasksArray> aMask(new SMasksArray);

    // Initialize possible masks for converting ambiguous
    // ncbi4na bytes to unambiguous bytes
    unsigned char mask[16] = {
        '\x11', '\x12', '\x14', '\x18',
            '\x21', '\x22', '\x24', '\x28',
            '\x41', '\x42', '\x44', '\x48',
            '\x81', '\x82', '\x84', '\x88'
            };

    unsigned char maskUpper[4] = {'\x10', '\x20', '\x40', '\x80'};
    unsigned char maskLower[4] = {'\x01', '\x02', '\x04', '\x08'};

    // Loop through possible ncbi4na bytes and
    // build masks that convert it to unambiguous na
    for(i = 0; i < 256; i++) {
        cVal = i;
        uCnt = 0;

        // Case where both upper and lower nible > 0
        if(((cVal & '\x0f') != 0) && ((cVal & '\xf0') != 0))
            for(j = 0; j < 16; j++) {
                cRslt = cVal & mask[j];
                if(cRslt == mask[j])
                    aMask->m_Table[i].cMask[uCnt++] = mask[j];
            }

        // Case where upper nible = 0 and lower nible > 0
        else if((cVal & '\x0f') != 0)
            for(j = 0; j < 4; j++)
                {
                    cRslt = cVal & maskLower[j];
                    if(cRslt == maskLower[j])
                        aMask->m_Table[i].cMask[uCnt++] = maskLower[j];
                }


        // Case where lower nible = 0 and upper nible > 0
        else if((cVal & '\xf0') != 0)
            for(j = 0; j < 4; j++)
                {
                    cRslt = cVal & maskUpper[j];
                    if(cRslt == maskUpper[j])
                        aMask->m_Table[i].cMask[uCnt++] = maskUpper[j];
                }

        // Both upper and lower nibles = 0
        else
            aMask->m_Table[i].cMask[uCnt++] = '\x00';

        // Number of distict masks for ncbi4na byte i
        aMask->m_Table[i].nMasks = uCnt;

        // Fill out the remainder of cMask array with copies
        // of first uCnt masks
        for(j = uCnt; j < 16 && uCnt > 0; j++)
            aMask->m_Table[i].cMask[j] = aMask->m_Table[i].cMask[j % uCnt];

    }

    return aMask;
}


// Function to initialize m_DetectAmbigNcbi4naNcbi2na used for
// ambiguity detection
CRef<CSeqportUtil_implementation::CAmbig_detect> CSeqportUtil_implementation::InitAmbigNcbi4naNcbi2na()
{

    unsigned char low, high, ambig;

    // Create am new CAmbig_detect object
    CRef<CAmbig_detect> ambig_detect(new CAmbig_detect(256,0));

    // Loop through low and high order nibles and assign
    // values as follows: 0 - no ambiguity, 1 - low order nible ambigiguous
    // 2 - high order ambiguous, 3 -- both high and low ambiguous.

    // Loop for low order nible
    for(low = 0; low < 16; low++) {
        // Determine if low order nible is ambiguous
        if((low == 1) || (low ==2) || (low == 4) || (low == 8))
            ambig = 0;  // Not ambiguous
        else
            ambig = 1;  // Ambiguous

        // Loop for high order nible
        for(high = 0; high < 16; high++) {

            // Determine if high order nible is ambiguous
            if((high != 1) && (high != 2) && (high != 4) && (high != 8))
                ambig += 2;  // Ambiguous

            // Set ambiguity value
            ambig_detect->m_Table[16*high + low] = ambig;

            // Reset ambig
            ambig &= '\xfd';  // Set second bit to 0
        }
    }

    return ambig_detect;
}


// Function to initialize m_DetectAmbigIupacnaNcbi2na used for ambiguity
// detection
CRef<CSeqportUtil_implementation::CAmbig_detect> CSeqportUtil_implementation::InitAmbigIupacnaNcbi2na()
{

    // Create am new CAmbig_detect object
    CRef<CAmbig_detect> ambig_detect(new CAmbig_detect(256,0));

    // 0 implies no ambiguity. 1 implies ambiguity
    // Initialize to 0
    for(unsigned int i = 0; i<256; i++)
        ambig_detect->m_Table[i] = 0;

    // Set iupacna characters that are ambiguous when converted
    // to ncib2na
    ambig_detect->m_Table[66] = 1; // B
    ambig_detect->m_Table[68] = 1; // D
    ambig_detect->m_Table[72] = 1; // H
    ambig_detect->m_Table[75] = 1; // K
    ambig_detect->m_Table[77] = 1; // M
    ambig_detect->m_Table[78] = 1; // N
    ambig_detect->m_Table[82] = 1; // R
    ambig_detect->m_Table[83] = 1; // S
    ambig_detect->m_Table[86] = 1; // V
    ambig_detect->m_Table[87] = 1; // W
    ambig_detect->m_Table[89] = 1; // Y

    return ambig_detect;
}



// Convert from one coding scheme to another. The following
// 12 conversions are supported: ncbi2na<=>ncbi4na;
// ncbi2na<=>iupacna; ncbi4na<=>iupacna; ncbieaa<=>ncbistdaa;
// ncbieaa<=>iupacaa; ncbistdaa<=>iupacaa. Convert is
// really just a dispatch function--it calls the appropriate
// priviate conversion function.
unsigned int CSeqportUtil_implementation::Convert
(const CSeq_data&        in_seq,
 CSeq_data*              out_seq,
 CSeq_data::E_Choice     to_code,
 unsigned int            uBeginIdx,
 unsigned int            uLength,
 bool                    bAmbig,
 CRandom::TValue         seed)
    const
{
    CSeq_data::E_Choice from_code = in_seq.Which();

    if(to_code == CSeq_data::e_not_set || from_code == CSeq_data::e_not_set)
        throw std::runtime_error("to_code or from_code not set");

    switch (to_code) {
    case CSeq_data::e_Iupacna:
        switch (from_code) {
        case CSeq_data::e_Ncbi2na:
            return MapNcbi2naToIupacna(in_seq, out_seq, uBeginIdx, uLength);
        case CSeq_data::e_Ncbi4na:
            return MapNcbi4naToIupacna(in_seq, out_seq, uBeginIdx, uLength);
        default:
            throw runtime_error("Requested conversion not implemented");
        }
    case CSeq_data::e_Iupacaa:
        switch (from_code) {
        case CSeq_data::e_Ncbieaa:
            return MapNcbieaaToIupacaa(in_seq, out_seq, uBeginIdx, uLength);
        case CSeq_data::e_Ncbistdaa:
            return MapNcbistdaaToIupacaa(in_seq, out_seq, uBeginIdx, uLength);
        default:
            throw runtime_error("Requested conversion not implemented");
        }
    case CSeq_data::e_Ncbi2na:
        switch (from_code) {
        case CSeq_data::e_Iupacna:
            return MapIupacnaToNcbi2na(in_seq, out_seq,
                                       uBeginIdx, uLength, bAmbig, seed);
        case CSeq_data::e_Ncbi4na:
            return MapNcbi4naToNcbi2na(in_seq, out_seq,
                                       uBeginIdx, uLength, bAmbig, seed);
        default:
            throw runtime_error("Requested conversion not implemented");
        }
    case CSeq_data::e_Ncbi4na:
        switch (from_code) {
        case CSeq_data::e_Ncbi2na:
            return MapNcbi2naToNcbi4na(in_seq, out_seq, uBeginIdx, uLength);
        case CSeq_data::e_Iupacna:
            return MapIupacnaToNcbi4na(in_seq, out_seq, uBeginIdx, uLength);
        default:
            throw runtime_error("Requested conversion not implemented");
        }
    case CSeq_data::e_Ncbieaa:
        switch (from_code) {
        case CSeq_data::e_Iupacaa:
            return MapIupacaaToNcbieaa(in_seq, out_seq, uBeginIdx, uLength);
        case CSeq_data::e_Ncbistdaa:
            return MapNcbistdaaToNcbieaa(in_seq, out_seq, uBeginIdx, uLength);
        default:
            throw runtime_error("Requested conversion not implemented");
        }
    case CSeq_data::e_Ncbistdaa:
        switch (from_code) {
        case CSeq_data::e_Iupacaa:
            return MapIupacaaToNcbistdaa(in_seq, out_seq, uBeginIdx, uLength);
        case CSeq_data::e_Ncbieaa:
            return MapNcbieaaToNcbistdaa(in_seq, out_seq, uBeginIdx, uLength);
        default:
            throw runtime_error("Requested conversion not implemented");
        }
    default:
        throw runtime_error("Requested conversion not implemented");
    }
}


// Provide maximum packing without loss of information
unsigned int CSeqportUtil_implementation::Pack
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    CRef<CSeq_data> os(new CSeq_data);
    CSeq_data* pos = os.GetPointer();

    vector<unsigned int> oi;

    switch (in_seq->Which()) {
    case CSeq_data::e_Iupacna:
        if (GetAmbigs_iupacna_ncbi2na(*in_seq, pos, &oi, uBeginIdx, uLength)
            > 0)
            {
                MapIupacnaToNcbi4na(*in_seq, pos, uBeginIdx, uLength);
                return GetNcbi4naCopy(*os, in_seq, 0, 0);
            }
        else
            {
                MapIupacnaToNcbi2na(*in_seq, pos, uBeginIdx, uLength,
                                    false, 1);
                return GetNcbi2naCopy(*os, in_seq, 0, 0);
            }
    case CSeq_data::e_Ncbi4na:
        if (GetAmbigs_ncbi4na_ncbi2na(*in_seq, pos, &oi, uBeginIdx, uLength)
            > 0)
            return 0;

        MapNcbi4naToNcbi2na(*in_seq, pos, uBeginIdx, uLength, false, 1);
        return GetNcbi2naCopy(*pos, in_seq, 0, 0);

    default:
        return 0;
    }
}


// Method to quickly validate that a CSeq_data object contains valid data.
// FastValidate is a dispatch function that calls the appropriate
// private fast validation function.
bool CSeqportUtil_implementation::FastValidate
(const CSeq_data&        in_seq,
 unsigned int            uBeginIdx,
 unsigned int            uLength)
    const
{
    switch (in_seq.Which()) {
    case CSeq_data::e_Ncbi2na:
        return true; // ncbi2na sequences are always valid
    case CSeq_data::e_Ncbi4na:
        return true; // ncbi4na sequences are always valid
    case CSeq_data::e_Iupacna:
        return FastValidateIupacna(in_seq, uBeginIdx, uLength);
    case CSeq_data::e_Ncbieaa:
        return FastValidateNcbieaa(in_seq, uBeginIdx, uLength);
    case CSeq_data::e_Ncbistdaa:
        return FastValidateNcbistdaa(in_seq, uBeginIdx, uLength);
    case CSeq_data::e_Iupacaa:
        return FastValidateIupacaa(in_seq, uBeginIdx, uLength);
    default:
        throw runtime_error("Sequence could not be validated");
    }
}


// Function to perform full validation. Validate is a
// dispatch function that calls the appropriate private
// validation function.
void CSeqportUtil_implementation::Validate
(const CSeq_data&        in_seq,
 vector<unsigned int>*   badIdx,
 unsigned int            uBeginIdx,
 unsigned int            uLength)
    const
{
    switch (in_seq.Which()) {
    case CSeq_data::e_Ncbi2na:
        return; // ncbi2na sequences are always valid
    case CSeq_data::e_Ncbi4na:
        return; // ncbi4na sequences are always valid
    case CSeq_data::e_Iupacna:
        ValidateIupacna(in_seq, badIdx, uBeginIdx, uLength);
        break;
    case CSeq_data::e_Ncbieaa:
        ValidateNcbieaa(in_seq, badIdx, uBeginIdx, uLength);
        break;
    case CSeq_data::e_Ncbistdaa:
        ValidateNcbistdaa(in_seq, badIdx, uBeginIdx, uLength);
        break;
    case CSeq_data::e_Iupacaa:
        ValidateIupacaa(in_seq, badIdx, uBeginIdx, uLength);
        break;
    default:
        throw runtime_error("Sequence could not be validated");
    }
}


// Function to find ambiguous bases and vector of indices of
// ambiguous bases in CSeq_data objects. GetAmbigs is a
// dispatch function that calls the appropriate private get
// ambigs function.
unsigned int CSeqportUtil_implementation::GetAmbigs
(const CSeq_data&        in_seq,
 CSeq_data*              out_seq,
 vector<unsigned int>*   out_indices,
 CSeq_data::E_Choice     to_code,
 unsigned int            uBeginIdx,
 unsigned int            uLength)
    const
{

    // Determine and call applicable GetAmbig method.
    switch (in_seq.Which()) {
    case CSeq_data::e_Ncbi4na:
        switch (to_code) {
        case CSeq_data::e_Ncbi2na:
            return GetAmbigs_ncbi4na_ncbi2na(in_seq, out_seq, out_indices,
                                             uBeginIdx, uLength);
        default:
            return 0;
        }
    case CSeq_data::e_Iupacna:
        switch (to_code) {
        case CSeq_data::e_Ncbi2na:
            return GetAmbigs_iupacna_ncbi2na(in_seq, out_seq, out_indices,
                                             uBeginIdx, uLength);
        default:
            return 0;
        }
    default:
        return 0;
    }
}


// Get a copy of in_seq from uBeginIdx through uBeginIdx + uLength-1
// and put in out_seq. See comments in alphabet.hpp for more information.
// GetCopy is a dispatch function.
unsigned int CSeqportUtil_implementation::GetCopy
(const CSeq_data&        in_seq,
 CSeq_data*              out_seq,
 unsigned int            uBeginIdx,
 unsigned int            uLength)
    const
{
    // Do processing based on in_seq type
    switch (in_seq.Which()) {
    case CSeq_data::e_Ncbi2na:
        return GetNcbi2naCopy(in_seq, out_seq, uBeginIdx, uLength);
    case CSeq_data::e_Ncbi4na:
        return GetNcbi4naCopy(in_seq, out_seq, uBeginIdx, uLength);
    case CSeq_data::e_Iupacna:
        return GetIupacnaCopy(in_seq, out_seq, uBeginIdx, uLength);
    case CSeq_data::e_Ncbieaa:
        return GetNcbieaaCopy(in_seq, out_seq, uBeginIdx, uLength);
    case CSeq_data::e_Ncbistdaa:
        return GetNcbistdaaCopy(in_seq, out_seq, uBeginIdx, uLength);
    case CSeq_data::e_Iupacaa:
        return GetIupacaaCopy(in_seq, out_seq, uBeginIdx, uLength);
    default:
        throw runtime_error
            ("GetCopy() is not implemented for the requested sequence type");
    }
}




// Method to keep only a contiguous piece of a sequence beginning
// at uBeginIdx and uLength residues long. Keep is a
// dispatch function.
unsigned int CSeqportUtil_implementation::Keep
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Do proceessing based upon in_seq type
    switch (in_seq->Which()) {
    case CSeq_data::e_Ncbi2na:
        return KeepNcbi2na(in_seq, uBeginIdx, uLength);
    case CSeq_data::e_Ncbi4na:
        return KeepNcbi4na(in_seq, uBeginIdx, uLength);
    case CSeq_data::e_Iupacna:
        return KeepIupacna(in_seq, uBeginIdx, uLength);
    case CSeq_data::e_Ncbieaa:
        return KeepNcbieaa(in_seq, uBeginIdx, uLength);
    case CSeq_data::e_Ncbistdaa:
        return KeepNcbistdaa(in_seq, uBeginIdx, uLength);
    case CSeq_data::e_Iupacaa:
        return KeepIupacaa(in_seq, uBeginIdx, uLength);
    default:
        throw runtime_error("Cannot perform Keep on in_seq type.");
    }
}


// Append in_seq2 to in_seq1 and put result in out_seq. This
// is a dispatch function.
unsigned int CSeqportUtil_implementation::Append
(CSeq_data*              out_seq,
 const CSeq_data&        in_seq1,
 unsigned int            uBeginIdx1,
 unsigned int            uLength1,
 const CSeq_data&        in_seq2,
 unsigned int            uBeginIdx2,
 unsigned int            uLength2)
    const
{
    // Check that in_seqs or of same type
    if(in_seq1.Which() != in_seq2.Which())
        throw runtime_error("Append in_seq types do not match.");

    // Call applicable append method base on in_seq types
    switch (in_seq1.Which()) {
    case CSeq_data::e_Iupacna:
        return AppendIupacna(out_seq, in_seq1, uBeginIdx1, uLength1,
                             in_seq2, uBeginIdx2, uLength2);
    case CSeq_data::e_Ncbi2na:
        return AppendNcbi2na(out_seq, in_seq1, uBeginIdx1, uLength1,
                             in_seq2, uBeginIdx2, uLength2);
    case CSeq_data::e_Ncbi4na:
        return AppendNcbi4na(out_seq, in_seq1, uBeginIdx1, uLength1,
                             in_seq2, uBeginIdx2, uLength2);
    case CSeq_data::e_Ncbieaa:
        return AppendNcbieaa(out_seq, in_seq1, uBeginIdx1, uLength1,
                             in_seq2, uBeginIdx2, uLength2);
    case CSeq_data::e_Ncbistdaa:
        return AppendNcbistdaa(out_seq, in_seq1, uBeginIdx1, uLength1,
                               in_seq2, uBeginIdx2, uLength2);
    case CSeq_data::e_Iupacaa:
        return AppendIupacaa(out_seq, in_seq1, uBeginIdx1, uLength1,
                             in_seq2, uBeginIdx2, uLength2);
    default:
        throw runtime_error("Append for in_seq type not supported.");
    }
}


// Methods to complement na sequences. These are
// dispatch functions.

// Method to complement na sequence in place
unsigned int CSeqportUtil_implementation::Complement
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Determine type of sequeence and which complement method to call
    switch (in_seq->Which()) {
    case CSeq_data::e_Iupacna:
        return ComplementIupacna(in_seq, uBeginIdx, uLength);
    case CSeq_data::e_Ncbi2na:
        return ComplementNcbi2na(in_seq, uBeginIdx, uLength);
    case CSeq_data::e_Ncbi4na:
        return ComplementNcbi4na(in_seq, uBeginIdx, uLength);
    default:
        throw runtime_error
            ("Complement of requested sequence type not supported.");
    }
}


// Method to complement na sequence in a copy out_seq
unsigned int CSeqportUtil_implementation::Complement
(const CSeq_data&        in_seq,
 CSeq_data*              out_seq,
 unsigned int            uBeginIdx,
 unsigned int            uLength)
    const
{
    // Determine type of sequeence and which complement method to call
    switch (in_seq.Which()) {
    case CSeq_data::e_Iupacna:
        return ComplementIupacna(in_seq, out_seq, uBeginIdx, uLength);
    case CSeq_data::e_Ncbi2na:
        return ComplementNcbi2na(in_seq, out_seq, uBeginIdx, uLength);
    case CSeq_data::e_Ncbi4na:
        return ComplementNcbi4na(in_seq, out_seq, uBeginIdx, uLength);
    default:
        throw runtime_error
            ("Complement of requested sequence type not supported.");
    }
}


// Methods to reverse na sequences. These are
// dispatch functions.

// Method to reverse na sequence in place
unsigned int CSeqportUtil_implementation::Reverse
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Determine type of sequeence and which reverse method to call
    switch (in_seq->Which()) {
    case CSeq_data::e_Iupacna:
        return ReverseIupacna(in_seq, uBeginIdx, uLength);
    case CSeq_data::e_Ncbi2na:
        return ReverseNcbi2na(in_seq, uBeginIdx, uLength);
    case CSeq_data::e_Ncbi4na:
        return ReverseNcbi4na(in_seq, uBeginIdx, uLength);
    default:
        throw runtime_error("Reverse of in_seq type is not supported.");
    }
}


// Method to reverse na sequence in a copy out_seq
unsigned int CSeqportUtil_implementation::Reverse
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Determine type of sequeence and which reverse method to call
    switch (in_seq.Which()) {
    case CSeq_data::e_Iupacna:
        return ReverseIupacna(in_seq, out_seq, uBeginIdx, uLength);
    case CSeq_data::e_Ncbi2na:
        return ReverseNcbi2na(in_seq, out_seq, uBeginIdx, uLength);
    case CSeq_data::e_Ncbi4na:
        return ReverseNcbi4na(in_seq, out_seq, uBeginIdx, uLength);
    default:
        throw runtime_error("Reverse of in_seq type is not supported.");
    }
}



// Methods to reverse-complement a sequence. These are
// dispatch functions.

// Method to reverse-complement na sequence in place
unsigned int CSeqportUtil_implementation::ReverseComplement
(CSeq_data*    in_seq,
 unsigned int  uBeginIdx,
 unsigned int  uLength)
    const
{
    // Determine type of sequeence and which rev_comp method to call
    switch (in_seq->Which()) {
    case CSeq_data::e_Iupacna:
        return ReverseComplementIupacna(in_seq, uBeginIdx, uLength);
    case CSeq_data::e_Ncbi2na:
        return ReverseComplementNcbi2na(in_seq, uBeginIdx, uLength);
    case CSeq_data::e_Ncbi4na:
        return ReverseComplementNcbi4na(in_seq, uBeginIdx, uLength);
    default:
        throw runtime_error
            ("ReverseComplement of in_seq type is not supported.");
    }
}


// Method to reverse-complement na sequence in a copy out_seq
unsigned int CSeqportUtil_implementation::ReverseComplement
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Determine type of sequeence and which rev_comp method to call
    switch (in_seq.Which()) {
    case CSeq_data::e_Iupacna:
        return ReverseComplementIupacna(in_seq, out_seq, uBeginIdx, uLength);
    case CSeq_data::e_Ncbi2na:
        return ReverseComplementNcbi2na(in_seq, out_seq, uBeginIdx, uLength);
    case CSeq_data::e_Ncbi4na:
        return ReverseComplementNcbi4na(in_seq, out_seq, uBeginIdx, uLength);
    default:
        throw runtime_error("ReverseComplement of in_seq type not supported.");
    }
}


// Implement private worker functions called by public
// dispatch functions.

// Methods to convert between coding schemes


// Convert in_seq from ncbi2na (1 byte) to iupacna (4 bytes)
// and put result in out_seq
unsigned int CSeqportUtil_implementation::MapNcbi2naToIupacna
(const CSeq_data&   in_seq,
 CSeq_data*         out_seq,
 unsigned int       uBeginIdx,
 unsigned int       uLength)
    const
{
    // Save uBeginIdx and uLength for later use
    unsigned int uBeginSav = uBeginIdx;
    unsigned int uLenSav = uLength;

    // Get vector holding the in sequence
    const vector<char>& in_seq_data = in_seq.GetNcbi2na().Get();

    // Get string where the out sequence will go
    out_seq->Reset();
    string& out_seq_data = out_seq->SetIupacna().Set();

    // Validate uBeginSav
    if(uBeginSav >= 4*in_seq_data.size())
        return 0;

    // Adjust uLenSav
    if((uLenSav == 0 )|| ((uLenSav + uBeginSav )> 4*in_seq_data.size()))
        uLenSav = 4*in_seq_data.size() - uBeginSav;


    // Adjust uBeginIdx and uLength, if necessary
    Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 4, 1);

    // Declare iterator for in_seq
    vector<char>::const_iterator i_in;

    // Allocate string memory for result of conversion
    out_seq_data.resize(uLenSav);

    // Get pointer to data of out_seq_data (a string)
    string::iterator i_out = out_seq_data.begin()-1;

    // Determine begin and end bytes of in_seq_data
    vector<char>::const_iterator i_in_begin =
        in_seq_data.begin() + uBeginIdx/4;
    vector<char>::const_iterator i_in_end = i_in_begin + uLength/4;
    if((uLength % 4) != 0) ++i_in_end;
    --i_in_end;

    // Handle first input sequence byte
    unsigned int uVal =
        m_FastNcbi2naIupacna->m_Table[static_cast<unsigned char>(*i_in_begin)];
    char *pchar, *pval;
    pval = reinterpret_cast<char*>(&uVal);
    for(pchar = pval + uBeginSav - uBeginIdx; pchar < pval + 4; ++pchar)
        *(++i_out) = *pchar;

    if(i_in_begin == i_in_end)
        return uLenSav;
    ++i_in_begin;

    // Loop through in_seq_data and convert to out_seq
    for(i_in = i_in_begin; i_in != i_in_end; ++i_in) {
        uVal =
            m_FastNcbi2naIupacna->m_Table[static_cast<unsigned char>(*i_in)];
        pchar = reinterpret_cast<char*>(&uVal);
        (*(++i_out)) = (*(pchar++));
        (*(++i_out)) = (*(pchar++));
        (*(++i_out)) = (*(pchar++));
        (*(++i_out)) = (*(pchar++));
    }

    // Handle last byte of input data
    uVal =
        m_FastNcbi2naIupacna->m_Table[static_cast<unsigned char>(*i_in_end)];
    pval = reinterpret_cast<char*>(&uVal);
    unsigned int uOverhang = (uBeginSav + uLenSav) % 4;
    uOverhang = (uOverhang ==0) ? 4 : uOverhang;
    for(pchar = pval; pchar < pval + uOverhang; ++pchar) {
        (*(++i_out)) = *pchar;
    }

    return uLenSav;
}


// Convert in_seq from ncbi2na (1 byte) to ncbi4na (2 bytes)
// and put result in out_seq
unsigned int CSeqportUtil_implementation::MapNcbi2naToNcbi4na
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get vector holding the in sequence
    const vector<char>& in_seq_data = in_seq.GetNcbi2na().Get();

    // Get vector where out sequence will go
    out_seq->Reset();
    vector<char>& out_seq_data = out_seq->SetNcbi4na().Set();

    // Save uBeginIdx and uLength for later use as they
    // are modified below
    unsigned int uBeginSav = uBeginIdx;
    unsigned int uLenSav = uLength;

    // Check that uBeginSav is not beyond end of in_seq_data
    if(uBeginSav >= 4*in_seq_data.size())
        return 0;

    // Adjust uLenSav
    if((uLenSav == 0) || ((uBeginSav + uLenSav) > 4*in_seq_data.size()))
        uLenSav = 4*in_seq_data.size() - uBeginSav;


    // Adjust uBeginIdx and uLength, if necessary
    unsigned int uOverhang =
        Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 4, 2);

    // Declare iterator for in_seq
    vector<char>::const_iterator i_in;

    // Allocate memory for out_seq_data
    unsigned int uInBytes = (uLength + uOverhang)/4;
    if(((uLength + uOverhang) % 4) != 0) uInBytes++;
    vector<char>::size_type nOutBytes = 2*uInBytes;
    out_seq_data.resize(nOutBytes);

    // Get an iterator of out_seq_data
    vector<char>::iterator i_out = out_seq_data.begin()-1;

    // Determine begin and end bytes of in_seq_data
    vector<char>::const_iterator i_in_begin =
        in_seq_data.begin() + uBeginIdx/4;
    vector<char>::const_iterator i_in_end = i_in_begin + uInBytes;
    if(((uLength % 4) != 0) || (uOverhang != 0)) ++i_in_end;

    // Loop through in_seq_data and convert to out_seq_data
    for(i_in = i_in_begin; i_in != i_in_end; ++i_in) {
        unsigned short uVal =
            m_FastNcbi2naNcbi4na->m_Table[static_cast<unsigned char>(*i_in)];
        char* pch = reinterpret_cast<char*>(&uVal);
        (*(++i_out)) = (*(pch++));
        (*(++i_out)) = (*(pch++));
    }
    unsigned int keepidx = uBeginSav - uBeginIdx;
    KeepNcbi4na(out_seq, keepidx, uLenSav);

    return uLenSav;
}


// Convert in_seq from ncbi4na (1 byte) to iupacna (2 bytes)
// and put result in out_seq
unsigned int CSeqportUtil_implementation::MapNcbi4naToIupacna
(const CSeq_data& in_seq,
 CSeq_data*       out_seq,
 unsigned int     uBeginIdx,
 unsigned int     uLength)
    const
{
    // Save uBeginIdx and uLength for later use
    unsigned int uBeginSav = uBeginIdx;
    unsigned int uLenSav = uLength;

    // Get vector holding the in sequence
    const vector<char>& in_seq_data = in_seq.GetNcbi4na().Get();

    // Get string where the out sequence will go
    out_seq->Reset();
    string& out_seq_data = out_seq->SetIupacna().Set();

    // Validate uBeginSav
    if(uBeginSav >= 2*in_seq_data.size())
        return 0;

    // Adjust uLenSav
    if((uLenSav == 0 )|| ((uLenSav + uBeginSav )> 2*in_seq_data.size()))
        uLenSav = 2*in_seq_data.size() - uBeginSav;


    // Adjust uBeginIdx and uLength, if necessary
    Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 2, 1);

    // Declare iterator for in_seq
    vector<char>::const_iterator i_in;

    // Allocate string memory for result of conversion
    out_seq_data.resize(uLenSav);

    // Get pointer to data of out_seq_data (a string)
    string::iterator i_out = out_seq_data.begin() - 1;

    // Determine begin and end bytes of in_seq_data
    vector<char>::const_iterator i_in_begin =
        in_seq_data.begin() + uBeginIdx/2;
    vector<char>::const_iterator i_in_end = i_in_begin + uLength/2;
    if((uLength % 2) != 0) ++i_in_end;
    --i_in_end;

    // Handle first input sequence byte
    unsigned short uVal =
        m_FastNcbi4naIupacna->m_Table[static_cast<unsigned char>(*i_in_begin)];
    char *pchar, *pval;
    pval = reinterpret_cast<char*>(&uVal);
    for(pchar = pval + uBeginSav - uBeginIdx; pchar < pval + 2; ++pchar)
        *(++i_out) = *pchar;

    if(i_in_begin == i_in_end)
        return uLenSav;
    ++i_in_begin;

    // Loop through in_seq_data and convert to out_seq
    for(i_in = i_in_begin; i_in != i_in_end; ++i_in) {
        uVal =
            m_FastNcbi4naIupacna->m_Table[static_cast<unsigned char>(*i_in)];
        pchar = reinterpret_cast<char*>(&uVal);
        (*(++i_out)) = (*(pchar++));
        (*(++i_out)) = (*(pchar++));
    }

    // Handle last byte of input data
    uVal =
        m_FastNcbi4naIupacna->m_Table[static_cast<unsigned char>(*i_in_end)];
    pval = reinterpret_cast<char*>(&uVal);
    unsigned int uOverhang = (uBeginSav + uLenSav) % 2;
    uOverhang = (uOverhang ==0) ? 2 : uOverhang;
    for(pchar = pval; pchar < pval + uOverhang; ++pchar)
        (*(++i_out)) = *pchar;

    return uLenSav;
}


// Function to convert iupacna (4 bytes) to ncbi2na (1 byte)
unsigned int CSeqportUtil_implementation::MapIupacnaToNcbi2na
(const CSeq_data& in_seq,
 CSeq_data*       out_seq,
 unsigned int     uBeginIdx,
 unsigned int     uLength,
 bool             bAmbig,
 CRandom::TValue  seed)
    const
{
    // Get string holding the in_seq
    const string& in_seq_data = in_seq.GetIupacna().Get();

    // Get vector where the out sequence will go
    out_seq->Reset();
    vector<char>& out_seq_data = out_seq->SetNcbi2na().Set();

    // If uBeginIdx is after end of in_seq, return
    if(uBeginIdx >= in_seq_data.size())
        return 0;

    // Determine return value
    unsigned int uLenSav = uLength;
    if((uLenSav == 0) || ((uLenSav + uBeginIdx)) > in_seq_data.size())
        uLenSav = in_seq_data.size() - uBeginIdx;


    // Adjust uBeginIdx and uLength, if necessary and get uOverhang
    unsigned int uOverhang =
        Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 1, 4);

    // Allocate vector memory for result of conversion
    // Note memory for overhang is added below.
    vector<char>::size_type nBytes = uLength/4;
    out_seq_data.resize(nBytes);

    // Declare iterator for out_seq_data and determine begin and end
    vector<char>::iterator i_out;
    vector<char>::iterator i_out_begin = out_seq_data.begin();
    vector<char>::iterator i_out_end = i_out_begin + uLength/4;

    // Determine begin of in_seq_data
    string::const_iterator i_in = in_seq_data.begin() + uBeginIdx;

    if(bAmbig)
        {
            // Do random disambiguation
            unsigned char c1, c2;
            CRandom::TValue rv;

            // Declare a random number generator and set seed
            CRandom rg;
            rg.SetSeed(seed);

	    // Do disambiguation by converting Iupacna to Ncbi4na
	    // deterministically and then converting from Ncbi4na to Ncbi2na
	    // with random disambiguation

	    // Loop through the out_seq_data converting 4 Iupacna bytes to
	    // one Ncbi2na byte. in_seq_data.size() % 4 bytes at end of
	    // input handled separately below.
            for(i_out = i_out_begin; i_out != i_out_end; ++i_out)
                {
                    // Determine first Ncbi4na byte from 1st two Iupacna bytes
                    c1 =
                        m_FastIupacnaNcbi4na->m_Table
                        [0][static_cast<unsigned char>(*i_in)] |
                        m_FastIupacnaNcbi4na->m_Table
                        [1][static_cast<unsigned char>(*(i_in+1))];

		    // Determine second Ncbi4na byte from 2nd two Iupacna bytes
                    c2 =
                        m_FastIupacnaNcbi4na->m_Table
                        [0][static_cast<unsigned char>(*(i_in+2))]|
                        m_FastIupacnaNcbi4na->m_Table
                        [1][static_cast<unsigned char>(*(i_in+3))];

		    // Randomly pick disambiguated Ncbi4na bytes
                    rv = rg.GetRand() % 16L;
                    c1 &= m_Masks->m_Table[c1].cMask[rv];
                    rv = rg.GetRand() % 16L;
                    c2 &= m_Masks->m_Table[c2].cMask[rv];

		    // Convert from Ncbi4na to Ncbi2na
                    (*i_out) = m_FastNcbi4naNcbi2na->m_Table[0][c1] |
                        m_FastNcbi4naNcbi2na->m_Table[1][c2];

		    // Increment input sequence iterator.
                    i_in+=4;
                }

            // Handle overhang at end of in_seq
            switch (uOverhang) {
            case 1:
                c1 =
                    m_FastIupacnaNcbi4na->m_Table
                    [0][static_cast<unsigned char>(*i_in)];
                rv = rg.GetRand() % 16L;
                c1 &= m_Masks->m_Table[c1].cMask[rv];
                out_seq_data.push_back(m_FastNcbi4naNcbi2na->m_Table[0][c1]);
                break;
            case 2:
                c1 =
                    m_FastIupacnaNcbi4na->m_Table
                    [0][static_cast<unsigned char>(*i_in)] |
                    m_FastIupacnaNcbi4na->m_Table
                    [1][static_cast<unsigned char>(*(i_in+1))];
                rv = rg.GetRand() % 16L;
                c1 &= m_Masks->m_Table[c1].cMask[rv];
                out_seq_data.push_back(m_FastNcbi4naNcbi2na->m_Table[0][c1]);
                break;
            case 3:
                c1 =
                    m_FastIupacnaNcbi4na->m_Table
                    [0][static_cast<unsigned char>(*i_in)] |
                    m_FastIupacnaNcbi4na->m_Table
                    [1][static_cast<unsigned char>(*(i_in+1))];
                c2 =
                    m_FastIupacnaNcbi4na->m_Table
                    [0][static_cast<unsigned char>(*(i_in+2))];
                rv = rg.GetRand() % 16L;
                c2 &= m_Masks->m_Table[c1].cMask[rv];
                rv = rg.GetRand() % 16L;
                c2 &= m_Masks->m_Table[c2].cMask[rv];
                out_seq_data.push_back(m_FastNcbi4naNcbi2na->m_Table[0][c1] |
                                       m_FastNcbi4naNcbi2na->m_Table[1][c2]);
            }
        }
    else
        {
            // Pack uLength input characters into out_seq_data
            for(i_out = i_out_begin; i_out != i_out_end; ++i_out)
                {
                    (*i_out) =
                        m_FastIupacnaNcbi2na->m_Table
                        [0][static_cast<unsigned char>(*(i_in))] |
                        m_FastIupacnaNcbi2na->m_Table
                        [1][static_cast<unsigned char>(*(i_in+1))] |
                        m_FastIupacnaNcbi2na->m_Table
                        [2][static_cast<unsigned char>(*(i_in+2))] |
                        m_FastIupacnaNcbi2na->m_Table
                        [3][static_cast<unsigned char>(*(i_in+3))];
                    i_in+=4;
                }

            // Handle overhang
            char ch = '\x00';
            for(unsigned int i = 0; i < uOverhang; i++)
                ch |=
                    m_FastIupacnaNcbi2na->m_Table
                    [i][static_cast<unsigned char>(*(i_in+i))];
            if(uOverhang > 0)
                out_seq_data.push_back(ch);
        }

    return uLenSav;
}


// Function to convert iupacna (2 bytes) to ncbi4na (1 byte)
unsigned int CSeqportUtil_implementation::MapIupacnaToNcbi4na
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get string holding the in_seq
    const string& in_seq_data = in_seq.GetIupacna().Get();

    // Get vector where the out sequence will go
    out_seq->Reset();
    vector<char>& out_seq_data = out_seq->SetNcbi4na().Set();

    // If uBeginIdx beyond end of in_seq, return
    if(uBeginIdx >= in_seq_data.size())
        return 0;

    // Determine return value
    unsigned int uLenSav = uLength;
    if((uLenSav == 0) || (uLenSav + uBeginIdx) > in_seq_data.size())
        uLenSav = in_seq_data.size() - uBeginIdx;

    // Adjust uBeginIdx and uLength and get uOverhang
    unsigned int uOverhang =
        Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 1, 2);

    // Allocate vector memory for result of conversion
    // Note memory for overhang is added below.
    vector<char>::size_type nBytes = uLength/2;
    out_seq_data.resize(nBytes);

    // Declare iterator for out_seq_data and determine begin and end
    vector<char>::iterator i_out;
    vector<char>::iterator i_out_begin = out_seq_data.begin();
    vector<char>::iterator i_out_end = i_out_begin + uLength/2;

    // Determine begin of in_seq_data offset by 1
    string::const_iterator i_in = in_seq_data.begin() + uBeginIdx;

    // Pack uLength input characters into out_seq_data
    for(i_out = i_out_begin; i_out != i_out_end; ++i_out) {
        (*i_out) =
            m_FastIupacnaNcbi4na->m_Table
            [0][static_cast<unsigned char>(*(i_in))] |
            m_FastIupacnaNcbi4na->m_Table
            [1][static_cast<unsigned char>(*(i_in+1))];
        i_in+=2;
    }

    // Handle overhang
    char ch = '\x00';
    if (uOverhang > 0) {
        ch |=
            m_FastIupacnaNcbi4na->
            m_Table[0][static_cast<unsigned char>(*i_in)];
        out_seq_data.push_back(ch);
    }

    return uLenSav;
}


// Function to convert ncbi4na (2 bytes) to ncbi2na (1 byte)
unsigned int CSeqportUtil_implementation::MapNcbi4naToNcbi2na
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength,
 bool              bAmbig,
 CRandom::TValue   seed)
    const
{
    // Get vector holding the in_seq
    const vector<char>& in_seq_data = in_seq.GetNcbi4na().Get();

    // Get vector where the out sequence will go
    out_seq->Reset();
    vector<char>& out_seq_data = out_seq->SetNcbi2na().Set();

    // Save uBeginIdx and uLength as they will be modified below
    unsigned int uBeginSav = uBeginIdx;
    unsigned int uLenSav = uLength;


    // Check that uBeginSav is not beyond end of in_seq
    if(uBeginSav >= 2*in_seq_data.size())
        return 0;

    // Adjust uLenSav if needed
    if((uLenSav == 0) || ((uBeginSav + uLenSav) > 2*in_seq_data.size()))
        uLenSav = 2*in_seq_data.size() - uBeginSav;

    // Adjust uBeginIdx and uLength and get uOverhang
    unsigned int uOverhang =
        Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 2, 4);

    // Allocate vector memory for result of conversion
    // Note memory for overhang is added below.
    vector<char>::size_type nBytes = uLength/4;
    out_seq_data.resize(nBytes);

    // Declare iterator for out_seq_data and determine begin and end
    vector<char>::iterator i_out;
    vector<char>::iterator i_out_begin = out_seq_data.begin();
    vector<char>::iterator i_out_end = i_out_begin + uLength/4;

    // Determine begin of in_seq_data
    vector<char>::const_iterator i_in = in_seq_data.begin() + uBeginIdx/2;

    // Do random disambiguation
    if(bAmbig)
        {
            // Declare a random number generator and set seed
            CRandom rg;
            rg.SetSeed(seed);

            // Pack uLength input bytes into out_seq_data
            for(i_out = i_out_begin; i_out != i_out_end; ++i_out)
                {
                    // Disambiguate
                    unsigned char c1 = static_cast<unsigned char>(*i_in);
                    unsigned char c2 = static_cast<unsigned char>(*(i_in+1));
                    CRandom::TValue rv = rg.GetRand() % 16L;
                    c1 &= m_Masks->m_Table[c1].cMask[rv];
                    rv = rg.GetRand() % 16L;
                    c2 &= m_Masks->m_Table[c2].cMask[rv];

                    // Convert
                    (*i_out) =
                        m_FastNcbi4naNcbi2na->m_Table[0][c1] |
                        m_FastNcbi4naNcbi2na->m_Table[1][c2];
                    i_in+=2;
                }

            // Handle overhang
            char ch = '\x00';
            if(uOverhang > 0)
                {
                    // Disambiguate
                    unsigned char c1 = static_cast<unsigned char>(*i_in);
                    CRandom::TValue rv = rg.GetRand() % 16L;
                    c1 &= m_Masks->m_Table[c1].cMask[rv];

            // Convert
                    ch |= m_FastNcbi4naNcbi2na->m_Table[0][c1];
                }

            if(uOverhang == 3)
                {
                    // Disambiguate
                    unsigned char c1 = static_cast<unsigned char>(*(++i_in));
                    CRandom::TValue rv = rg.GetRand() % 16L;
                    c1 &= m_Masks->m_Table[c1].cMask[rv];

            // Convert
                    ch |= m_FastNcbi4naNcbi2na->m_Table[1][c1];
                }

            if(uOverhang > 0)
                {
                    out_seq_data.push_back(ch);
                }
        }
    // Do not do random disambiguation
    else
        {
            // Pack uLength input bytes into out_seq_data
            for(i_out = i_out_begin; i_out != i_out_end; ++i_out) {
                (*i_out) =
                    m_FastNcbi4naNcbi2na->m_Table
                    [0][static_cast<unsigned char>(*i_in)] |
                    m_FastNcbi4naNcbi2na->m_Table
                    [1][static_cast<unsigned char>(*(i_in+1))];
                i_in+=2;
            }

            // Handle overhang
            char ch = '\x00';
            if(uOverhang > 0)
                ch |= m_FastNcbi4naNcbi2na->m_Table
                    [0][static_cast<unsigned char>(*i_in)];

            if(uOverhang == 3)
                ch |= m_FastNcbi4naNcbi2na->m_Table
                    [1][static_cast<unsigned char>(*(++i_in))];

            if(uOverhang > 0)
                out_seq_data.push_back(ch);

        }
    unsigned int keepidx = uBeginSav - uBeginIdx;
    KeepNcbi2na(out_seq, keepidx, uLenSav);

    return uLenSav;
}


// Function to convert iupacaa (byte) to ncbieaa (byte)
unsigned int CSeqportUtil_implementation::MapIupacaaToNcbieaa
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get read-only reference to in_seq data
    const string& in_seq_data = in_seq.GetIupacaa().Get();

    // Get read & write reference to out_seq data
    out_seq->Reset();
    string& out_seq_data = out_seq->SetNcbieaa().Set();

    // If uBeginIdx beyond end of in_seq, return
    if(uBeginIdx >= in_seq_data.size())
        return 0;

    // Adjust uBeginIdx and uLength, if necessary
    Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 1, 1);

    // Allocate memory for out_seq
    out_seq_data.resize(uLength);

    // Declare iterator for out_seq_data
    string::iterator i_out = out_seq_data.begin();

    // Declare iterator for in_seq_data and determine begin and end
    string::const_iterator i_in;
    string::const_iterator i_in_begin = in_seq_data.begin() + uBeginIdx;
    string::const_iterator i_in_end = i_in_begin + uLength;

    // Loop through input and convert to output
    for(i_in = i_in_begin; i_in != i_in_end; ++i_in)
        (*(i_out++)) =
            m_IupacaaNcbieaa->m_Table[static_cast<unsigned char>(*i_in)];

    return uLength;
}


// Function to convert ncbieaa (byte) to iupacaa (byte)
unsigned int CSeqportUtil_implementation::MapNcbieaaToIupacaa
(const CSeq_data&   in_seq,
 CSeq_data*         out_seq,
 unsigned int       uBeginIdx,
 unsigned int       uLength)
    const
{
    // Get read-only reference to in_seq data
    const string& in_seq_data = in_seq.GetNcbieaa().Get();

    // Get read & write reference to out_seq data
    out_seq->Reset();
    string& out_seq_data = out_seq->SetIupacaa().Set();

    // If uBeginIdx beyond end of in_seq, return
    if(uBeginIdx >= in_seq_data.size())
        return 0;

    // Adjust uBeginIdx and uLength, if necessary
    Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 1, 1);

    // Allocate memory for out_seq
    out_seq_data.resize(uLength);

    // Declare iterator for out_seq_data
    string::iterator i_out = out_seq_data.begin();

    // Declare iterator for in_seq_data and determine begin and end
    string::const_iterator i_in;
    string::const_iterator i_in_begin = in_seq_data.begin() + uBeginIdx;
    string::const_iterator i_in_end = i_in_begin + uLength;

    // Loop through input and convert to output
    for(i_in = i_in_begin; i_in != i_in_end; ++i_in)
        (*(i_out++)) =
            m_NcbieaaIupacaa->m_Table[static_cast<unsigned char>(*i_in)];

    return uLength;
}


// Function to convert iupacaa (byte) to ncbistdaa (byte)
unsigned int CSeqportUtil_implementation::MapIupacaaToNcbistdaa
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get read-only reference to in_seq data
    const string& in_seq_data = in_seq.GetIupacaa().Get();

    // Get read & write reference to out_seq data
    out_seq->Reset();
    vector<char>& out_seq_data = out_seq->SetNcbistdaa().Set();

    // If uBeginIdx beyond end of in_seq, return
    if(uBeginIdx >= in_seq_data.size())
        return 0;

    // Adjust uBeginIdx and uLength, if necessary
    Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 1, 1);

    // Allocate memory for out_seq
    out_seq_data.resize(uLength);

    // Declare iterator for out_seq_data
    vector<char>::iterator i_out = out_seq_data.begin();

    // Declare iterator for in_seq_data and determine begin and end
    string::const_iterator i_in;
    string::const_iterator i_in_begin = in_seq_data.begin() + uBeginIdx;
    string::const_iterator i_in_end = i_in_begin + uLength;

    // Loop through input and convert to output
    for(i_in = i_in_begin; i_in != i_in_end; ++i_in)
        (*(i_out++)) =
            m_IupacaaNcbistdaa->m_Table[static_cast<unsigned char>(*i_in)];

    return uLength;
}





// Function to convert ncbieaa (byte) to ncbistdaa (byte)
unsigned int CSeqportUtil_implementation::MapNcbieaaToNcbistdaa
(const CSeq_data&   in_seq,
 CSeq_data*         out_seq,
 unsigned int       uBeginIdx,
 unsigned int       uLength)
    const
{
    // Get read-only reference to in_seq data
    const string& in_seq_data = in_seq.GetNcbieaa().Get();

    // Get read & write reference to out_seq data
    out_seq->Reset();
    vector<char>& out_seq_data = out_seq->SetNcbistdaa().Set();

    // If uBeginIdx beyond end of in_seq, return
    if(uBeginIdx >= in_seq_data.size())
        return 0;

    // Adjust uBeginIdx and uLength, if necessary
    Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 1, 1);

    // Allocate memory for out_seq
    out_seq_data.resize(uLength);

    // Declare iterator for out_seq_data
    vector<char>::iterator i_out = out_seq_data.begin();

    // Declare iterator for in_seq_data and determine begin and end
    string::const_iterator i_in;
    string::const_iterator i_in_begin = in_seq_data.begin() + uBeginIdx;
    string::const_iterator i_in_end = i_in_begin + uLength;

    // Loop through input and convert to output
    for(i_in = i_in_begin; i_in != i_in_end; ++i_in)
        (*(i_out++)) =
            m_NcbieaaNcbistdaa->m_Table[static_cast<unsigned char>(*i_in)];

    return uLength;
}


// Function to convert ncbistdaa (byte) to ncbieaa (byte)
unsigned int CSeqportUtil_implementation::MapNcbistdaaToNcbieaa
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get read-only reference to in_seq data
    const vector<char>& in_seq_data = in_seq.GetNcbistdaa().Get();

    // Get read & write reference to out_seq data
    out_seq->Reset();
    string& out_seq_data = out_seq->SetNcbieaa().Set();

    // If uBeginIdx beyond end of in_seq, return
    if(uBeginIdx >= in_seq_data.size())
        return 0;

    // Adjust uBeginIdx and uLength if necessary
    Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 1, 1);

    // Allocate memory for out_seq
    out_seq_data.resize(uLength);

    // Get iterator for out_seq_data
    string::iterator i_out = out_seq_data.begin();

    // Declare iterator for in_seq_data and determine begin and end
    vector<char>::const_iterator i_in;
    vector<char>::const_iterator i_in_begin = in_seq_data.begin() + uBeginIdx;
    vector<char>::const_iterator i_in_end = i_in_begin + uLength;

    // Loop through input and convert to output
    for(i_in = i_in_begin; i_in != i_in_end; ++i_in)
        *(i_out++) =
            m_NcbistdaaNcbieaa->m_Table[static_cast<unsigned char>(*i_in)];

    return uLength;
}


// Function to convert ncbistdaa (byte) to iupacaa (byte)
unsigned int CSeqportUtil_implementation::MapNcbistdaaToIupacaa
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get read-only reference to in_seq data
    const vector<char>& in_seq_data = in_seq.GetNcbistdaa().Get();

    // Get read & write reference to out_seq data
    out_seq->Reset();
    string& out_seq_data = out_seq->SetIupacaa().Set();

    // If uBeginIdx beyond end of in_seq, return
    if(uBeginIdx >= in_seq_data.size())
        return 0;

    // Adjust uBeginIdx and uLength
    Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 1, 1);

    // Allocate memory for out_seq
    out_seq_data.resize(uLength);

    // Get iterator for out_seq_data
    string::iterator i_out = out_seq_data.begin();

    // Declare iterator for in_seq_data and determine begin and end
    vector<char>::const_iterator i_in;
    vector<char>::const_iterator i_in_begin = in_seq_data.begin() + uBeginIdx;
    vector<char>::const_iterator i_in_end = i_in_begin + uLength;

    // Loop through input and convert to output
    for(i_in = i_in_begin; i_in != i_in_end; ++i_in)
        (*(i_out++)) =
            m_NcbistdaaIupacaa->m_Table[static_cast<unsigned char>(*i_in)];

    return uLength;
}


// Fast validation of iupacna sequence
bool CSeqportUtil_implementation::FastValidateIupacna
(const CSeq_data&  in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get read-only reference to in_seq data
    const string& in_seq_data = in_seq.GetIupacna().Get();

    // Check that uBeginIdx is not beyond end of in_seq
    if(uBeginIdx >= in_seq_data.size())
        return true;

    // Adjust uBeginIdx, uLength
    Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 1, 1);

    // Declare in iterator on in_seq and determine begin and end
    string::const_iterator itor;
    string::const_iterator b_itor = in_seq_data.begin() + uBeginIdx;
    string::const_iterator e_itor = b_itor + uLength;

    // Perform Fast Validation
    unsigned char ch = '\x00';
    for(itor = b_itor; itor != e_itor; ++itor)
        ch |= m_Iupacna->m_Table[static_cast<unsigned char>(*itor)];

    // Return true if valid, otherwise false
    return (ch != 255);
}


bool CSeqportUtil_implementation::FastValidateNcbieaa
(const CSeq_data&  in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get read-only reference to in_seq data
    const string& in_seq_data = in_seq.GetNcbieaa().Get();

    // Check that uBeginIdx is not beyond end of in_seq
    if(uBeginIdx >= in_seq_data.size())
        return true;

    // Check that uBeginIdx is not beyond end of in_seq
    if(uBeginIdx >= in_seq_data.size())
        return true;

    // Adjust uBeginIdx, uLength
    Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 1, 1);

    // Declare in iterator on in_seq and determine begin and end
    string::const_iterator itor;
    string::const_iterator b_itor = in_seq_data.begin() + uBeginIdx;
    string::const_iterator e_itor = b_itor + uLength;

    // Perform Fast Validation
    unsigned char ch = '\x00';
    for(itor = b_itor; itor != e_itor; ++itor)
        ch |= m_Ncbieaa->m_Table[static_cast<unsigned char>(*itor)];

    // Return true if valid, otherwise false
    return (ch != 255);

}


bool CSeqportUtil_implementation::FastValidateNcbistdaa
(const CSeq_data&  in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get read-only reference to in_seq data
    const vector<char>& in_seq_data = in_seq.GetNcbistdaa().Get();

    // Check that uBeginIdx is not beyond end of in_seq
    if(uBeginIdx >= in_seq_data.size())
        return true;

    // Adjust uBeginIdx, uLength
    Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 1, 1);

    // Declare in iterator on in_seq and determine begin and end
    vector<char>::const_iterator itor;
    vector<char>::const_iterator b_itor = in_seq_data.begin() + uBeginIdx;
    vector<char>::const_iterator e_itor = b_itor + uLength;

    // Perform Fast Validation
    unsigned char ch = '\x00';
    for(itor = b_itor; itor != e_itor; ++itor)
        ch |= m_Ncbistdaa->m_Table[static_cast<unsigned char>(*itor)];

    // Return true if valid, otherwise false
    return (ch != 255);

}


bool CSeqportUtil_implementation::FastValidateIupacaa
(const CSeq_data&  in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get read-only reference to in_seq data
    const string& in_seq_data = in_seq.GetIupacaa().Get();

    // Check that uBeginIdx is not beyond end of in_seq
    if(uBeginIdx >= in_seq_data.size())
        return true;

    // Adjust uBeginIdx, uLength
    Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 1, 1);

    // Declare in iterator on in_seq and determine begin and end
    string::const_iterator itor;
    string::const_iterator b_itor = in_seq_data.begin() + uBeginIdx;
    string::const_iterator e_itor = b_itor + uLength;

    // Perform Fast Validation
    unsigned char ch = '\x00';
    for(itor=b_itor; itor!=e_itor; ++itor)
        ch |= m_Iupacaa->m_Table[static_cast<unsigned char>(*itor)];

    // Return true if valid, otherwise false
    return (ch != 255);
}


void CSeqportUtil_implementation::ValidateIupacna
(const CSeq_data&        in_seq,
 vector<unsigned int>*   badIdx,
 unsigned int            uBeginIdx,
 unsigned int            uLength)
    const
{
    // Get read-only reference to in_seq data
    const string& in_seq_data = in_seq.GetIupacna().Get();

    // clear out_indices
    badIdx->clear();

    // Check that uBeginIdx is not beyond end of in_seq
    if(uBeginIdx >= in_seq_data.size())
        return;

    // Adjust uBeginIdx, uLength
    Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 1, 1);

    // Declare in iterator on in_seq and determine begin and end
    string::const_iterator itor;
    string::const_iterator b_itor = in_seq_data.begin() + uBeginIdx;
    string::const_iterator e_itor = b_itor + uLength;

    // Perform  Validation
    unsigned int nIdx = uBeginIdx;
    for(itor = b_itor; itor != e_itor; ++itor)
        if(m_Iupacna->m_Table[static_cast<unsigned char>(*itor)] == char(255))
            badIdx->push_back(nIdx++);
        else
            nIdx++;

    // Return list of bad indices
    return;
}


void CSeqportUtil_implementation::ValidateNcbieaa
(const CSeq_data&        in_seq,
 vector<unsigned int>*   badIdx,
 unsigned int            uBeginIdx,
 unsigned int            uLength)
    const
{
    // Get read-only reference to in_seq data
    const string& in_seq_data = in_seq.GetNcbieaa().Get();

    // clear badIdx
    badIdx->clear();

    // Check that uBeginIdx is not beyond end of in_seq
    if(uBeginIdx >= in_seq_data.size())
        return;

    // Adjust uBeginIdx, uLength
    Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 1, 1);

    // Declare in iterator on in_seq and determine begin and end
    string::const_iterator itor;
    string::const_iterator b_itor = in_seq_data.begin() + uBeginIdx;
    string::const_iterator e_itor = b_itor + uLength;

    // Perform  Validation
    unsigned int nIdx = uBeginIdx;
    for(itor = b_itor; itor != e_itor; ++itor)
        if(m_Ncbieaa->m_Table[static_cast<unsigned char>(*itor)] == char(255))
            badIdx->push_back(nIdx++);
        else
            nIdx++;

    // Return vector of bad indices
    return;
}


void CSeqportUtil_implementation::ValidateNcbistdaa
(const CSeq_data&        in_seq,
 vector<unsigned int>*   badIdx,
 unsigned int            uBeginIdx,
 unsigned int            uLength)
    const
{
    // Get read-only reference to in_seq data
    const vector<char>& in_seq_data = in_seq.GetNcbistdaa().Get();

    // Create a vector to return
    badIdx->clear();

    // Check that uBeginIdx is not beyond end of in_seq
    if(uBeginIdx >= in_seq_data.size())
        return;

    // Adjust uBeginIdx, uLength
    Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 1, 1);

    // Declare in iterator on in_seq and determine begin and end
    vector<char>::const_iterator itor;
    vector<char>::const_iterator b_itor = in_seq_data.begin() + uBeginIdx;
    vector<char>::const_iterator e_itor = b_itor + uLength;

    // Perform  Validation
    unsigned int nIdx = uBeginIdx;
    for(itor=b_itor; itor!=e_itor; ++itor)
        if(m_Ncbistdaa->m_Table[static_cast<unsigned char>(*itor)]==char(255))
            badIdx->push_back(nIdx++);
        else
            nIdx++;

    // Return vector of bad indices
    return;
}


void CSeqportUtil_implementation::ValidateIupacaa
(const CSeq_data&        in_seq,
 vector<unsigned int>*   badIdx,
 unsigned int            uBeginIdx,
 unsigned int            uLength)
    const
{
    // Get read-only reference to in_seq data
    const string& in_seq_data = in_seq.GetIupacaa().Get();

    // Create a vector to return
    badIdx->clear();

    // Check that uBeginIdx is not beyond end of in_seq
    if(uBeginIdx >= in_seq_data.size())
        return;

    // Adjust uBeginIdx, uLength
    Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 1, 1);

    // Declare in iterator on in_seq and determine begin and end
    string::const_iterator itor;
    string::const_iterator b_itor = in_seq_data.begin() + uBeginIdx;
    string::const_iterator e_itor = b_itor + uLength;

    // Perform  Validation
    unsigned int nIdx = uBeginIdx;
    for(itor=b_itor; itor!=e_itor; ++itor)
        if(m_Iupacaa->m_Table[static_cast<unsigned char>(*itor)] == char(255))
            badIdx->push_back(nIdx++);
        else
            nIdx++;

    // Return vector of bad indices
    return;
}


// Function to make copy of ncbi2na type sequences
unsigned int CSeqportUtil_implementation::GetNcbi2naCopy
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get reference to out_seq data
    out_seq->Reset();
    vector<char>& out_seq_data = out_seq->SetNcbi2na().Set();

    // Get reference to in_seq data
    const vector<char>& in_seq_data = in_seq.GetNcbi2na().Get();

    // Return if uBeginIdx is after end of in_seq
    if(uBeginIdx >= 4 * in_seq_data.size())
        return 0;

    // Set uLength to actual valid length in out_seq
    if( (uLength ==0) || ((uBeginIdx + uLength) > (4*in_seq_data.size() )) )
        uLength = 4*in_seq_data.size() - uBeginIdx;

    // Allocate memory for out_seq data
    if((uLength % 4) == 0)
        out_seq_data.resize(uLength/4);
    else
        out_seq_data.resize(uLength/4 + 1);

    // Get iterator on out_seq_data
    vector<char>::iterator i_out = out_seq_data.begin() - 1;

    // Calculate amounts to shift bits
    unsigned int lShift, rShift;
    lShift = 2*(uBeginIdx % 4);
    rShift = 8 - lShift;

    // Get interators on in_seq
    vector<char>::const_iterator i_in;
    vector<char>::const_iterator i_in_begin =
        in_seq_data.begin() + uBeginIdx/4;

    // Determine number of input bytes to process
    unsigned int uNumBytes = uLength/4;
    if((uLength % 4) != 0)
        ++uNumBytes;

    // Prevent access beyond end of in_seq_data
    bool bDoLastByte = false;
    if((uBeginIdx/4 + uNumBytes) >= in_seq_data.size())
        {
            uNumBytes = in_seq_data.size() - uBeginIdx/4 - 1;
            bDoLastByte = true;
        }
    vector<char>::const_iterator i_in_end = i_in_begin + uNumBytes;

    // Loop through input sequence and copy to output sequence
    if(lShift > 0)
        for(i_in = i_in_begin; i_in != i_in_end; ++i_in)
            (*(++i_out)) =
                ((*i_in) << lShift) | (((*(i_in+1)) & 255) >> rShift);
    else
        for(i_in = i_in_begin; i_in != i_in_end; ++i_in)
            (*(++i_out)) = (*i_in);

    // Handle last input byte if necessary
    if(bDoLastByte)
        (*(++i_out)) = (*i_in) << lShift;

    return uLength;
}


// Function to make copy of ncbi4na type sequences
unsigned int CSeqportUtil_implementation::GetNcbi4naCopy
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get reference to out_seq data
    out_seq->Reset();
    vector<char>& out_seq_data = out_seq->SetNcbi4na().Set();

    // Get reference to in_seq data
    const vector<char>& in_seq_data = in_seq.GetNcbi4na().Get();

    // Return if uBeginIdx is after end of in_seq
    if(uBeginIdx >= 2 * in_seq_data.size())
        return 0;

    // Set uLength to actual valid length in out_seq
    if( (uLength ==0) || ((uBeginIdx + uLength) > (2*in_seq_data.size() )) )
        uLength = 2*in_seq_data.size() - uBeginIdx;

    // Allocate memory for out_seq data
    if((uLength % 2) == 0)
        out_seq_data.resize(uLength/2);
    else
        out_seq_data.resize(uLength/2 + 1);


    // Get iterator on out_seq_data
    vector<char>::iterator i_out = out_seq_data.begin() - 1;

    // Calculate amounts to shift bits
    unsigned int lShift, rShift;
    lShift = 4*(uBeginIdx % 2);
    rShift = 8 - lShift;

    // Get interators on in_seq
    vector<char>::const_iterator i_in;
    vector<char>::const_iterator i_in_begin =
        in_seq_data.begin() + uBeginIdx/2;

    // Determine number of input bytes to process
    unsigned int uNumBytes = uLength/2;
    if((uLength % 2) != 0)
        ++uNumBytes;

    // Prevent access beyond end of in_seq_data
    bool bDoLastByte = false;
    if((uBeginIdx/2 + uNumBytes) >= in_seq_data.size())
        {
            uNumBytes = in_seq_data.size() - uBeginIdx/2 - 1;
            bDoLastByte = true;
        }
    vector<char>::const_iterator i_in_end = i_in_begin + uNumBytes;

    // Loop through input sequence and copy to output sequence
    if(lShift > 0)
        for(i_in = i_in_begin; i_in != i_in_end; ++i_in)
            (*(++i_out)) =
                ((*i_in) << lShift) | (((*(i_in+1)) & 255) >> rShift);
    else
        for(i_in = i_in_begin; i_in != i_in_end; ++i_in)
            (*(++i_out)) = (*i_in);

    // Handle last input byte
    if(bDoLastByte)
        (*(++i_out)) = (*i_in) << lShift;

    return uLength;
}


// Function to make copy of iupacna type sequences
unsigned int CSeqportUtil_implementation::GetIupacnaCopy
(const CSeq_data& in_seq,
 CSeq_data*       out_seq,
 unsigned int     uBeginIdx,
 unsigned int     uLength)
    const
{
    // Get reference to out_seq data
    out_seq->Reset();
    string& out_seq_data = out_seq->SetIupacna().Set();

    // Get reference to in_seq data
    const string& in_seq_data = in_seq.GetIupacna().Get();

    // Return if uBeginIdx is after end of in_seq
    if(uBeginIdx >= in_seq_data.size())
        return 0;

    // Set uLength to actual valid length in out_seq
    if( (uLength ==0) || ((uBeginIdx + uLength) > (in_seq_data.size() )) )
        uLength = in_seq_data.size() - uBeginIdx;

    // Allocate memory for out_seq data
    out_seq_data.resize(uLength);

    // Get iterator on out_seq_data
    string::iterator i_out = out_seq_data.begin() - 1;

    // Get interators on in_seq
    string::const_iterator i_in;
    string::const_iterator i_in_begin = in_seq_data.begin() + uBeginIdx;
    string::const_iterator i_in_end = i_in_begin + uLength;

    // Loop through input sequence and copy to output sequence
    for(i_in = i_in_begin; i_in != i_in_end; ++i_in)
        (*(++i_out)) = (*i_in);

    return uLength;
}


// Function to make copy of ncbieaa type sequences
unsigned int CSeqportUtil_implementation::GetNcbieaaCopy
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get reference to out_seq data
    out_seq->Reset();
    string& out_seq_data = out_seq->SetNcbieaa().Set();

    // Get reference to in_seq data
    const string& in_seq_data = in_seq.GetNcbieaa().Get();

    // Return if uBeginIdx is after end of in_seq
    if(uBeginIdx >= in_seq_data.size())
        return 0;

    // Set uLength to actual valid length in out_seq
    if( (uLength ==0) || ((uBeginIdx + uLength) > (in_seq_data.size() )) )
        uLength = in_seq_data.size() - uBeginIdx;

    // Allocate memory for out_seq data
    out_seq_data.resize(uLength);

    // Get iterator on out_seq_data
    string::iterator i_out = out_seq_data.begin() - 1;

    // Get interators on in_seq
    string::const_iterator i_in;
    string::const_iterator i_in_begin = in_seq_data.begin() + uBeginIdx;
    string::const_iterator i_in_end = i_in_begin + uLength;

    // Loop through input sequence and copy to output sequence
    for(i_in = i_in_begin; i_in != i_in_end; ++i_in)
        (*(++i_out)) = (*i_in);

    return uLength;
}


// Function to make copy of ncbistdaa type sequences
unsigned int CSeqportUtil_implementation::GetNcbistdaaCopy
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get reference to out_seq data
    out_seq->Reset();
    vector<char>& out_seq_data = out_seq->SetNcbistdaa().Set();

    // Get reference to in_seq data
    const vector<char>& in_seq_data = in_seq.GetNcbistdaa().Get();

    // Return if uBeginIdx is after end of in_seq
    if(uBeginIdx >= in_seq_data.size())
        return 0;

    // Set uLength to actual valid length in out_seq
    if( (uLength ==0) || ((uBeginIdx + uLength) > (in_seq_data.size() )) )
        uLength = in_seq_data.size() - uBeginIdx;

    // Allocate memory for out_seq data
    out_seq_data.resize(uLength);

    // Get iterator on out_seq_data
    vector<char>::iterator i_out = out_seq_data.begin() - 1;

    // Get interators on in_seq
    vector<char>::const_iterator i_in;
    vector<char>::const_iterator i_in_begin = in_seq_data.begin() + uBeginIdx;
    vector<char>::const_iterator i_in_end = i_in_begin + uLength;

    // Loop through input sequence and copy to output sequence
    for(i_in = i_in_begin; i_in != i_in_end; ++i_in)
        (*(++i_out)) = (*i_in);

    return uLength;
}


// Function to make copy of iupacaa type sequences
unsigned int CSeqportUtil_implementation::GetIupacaaCopy
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get reference to out_seq data
    out_seq->Reset();
    string& out_seq_data = out_seq->SetIupacaa().Set();

    // Get reference to in_seq data
    const string& in_seq_data = in_seq.GetIupacaa().Get();

    // Return if uBeginIdx is after end of in_seq
    if(uBeginIdx >= in_seq_data.size())
        return 0;

    // Set uLength to actual valid length in out_seq
    if( (uLength ==0) || ((uBeginIdx + uLength) > (in_seq_data.size() )) )
        uLength = in_seq_data.size() - uBeginIdx;

    // Allocate memory for out_seq data
    out_seq_data.resize(uLength);

    // Get iterator on out_seq_data
    string::iterator i_out = out_seq_data.begin() - 1;

    // Get interators on in_seq
    string::const_iterator i_in;
    string::const_iterator i_in_begin = in_seq_data.begin() + uBeginIdx;
    string::const_iterator i_in_end = i_in_begin + uLength;

    // Loop through input sequence and copy to output sequence
    for(i_in = i_in_begin; i_in != i_in_end; ++i_in)
        (*(++i_out)) = (*i_in);

    return uLength;
}


// Function to adjust uBeginIdx to lie on an in_seq byte boundary
// and uLength to lie on on an out_seq byte boundary. Returns
// overhang
unsigned int CSeqportUtil_implementation::Adjust
(unsigned int*  uBeginIdx,
 unsigned int*  uLength,
 unsigned int   uInSeqBytes,
 unsigned int   uInSeqsPerByte,
 unsigned int   uOutSeqsPerByte)
    const
{
    // Adjust uBeginIdx and uLength to acceptable values

    // If uLength = 0, assume convert to end of sequence
    if(*uLength == 0)
        *uLength = uInSeqsPerByte * uInSeqBytes;

    // Ensure that uBeginIdx does not start at or after end of in_seq_data
    if(*uBeginIdx >= uInSeqsPerByte * uInSeqBytes)
        *uBeginIdx = uInSeqsPerByte * uInSeqBytes - uInSeqsPerByte;

    // Ensure that uBeginIdx is a multiple of uInSeqsPerByte and adjust uLength
    *uLength += *uBeginIdx % uInSeqsPerByte;
    *uBeginIdx = uInSeqsPerByte * (*uBeginIdx/uInSeqsPerByte);

    // Adjust uLength so as not to go beyond end of in_seq_data
    if(*uLength > uInSeqsPerByte * uInSeqBytes - *uBeginIdx)
        *uLength = uInSeqsPerByte * uInSeqBytes - *uBeginIdx;

    // Adjust uLength down to multiple of uOutSeqsPerByte
    // and calculate overhang (overhang handled separately at end)
    unsigned int uOverhang = *uLength % uOutSeqsPerByte;
    *uLength = uOutSeqsPerByte * (*uLength / uOutSeqsPerByte);

    return uOverhang;

}


// Loops through an ncbi4na input sequence and determines
// the ambiguities that would result from conversion to an ncbi2na sequence
// On return, out_seq contains the ncbi4na bases that become ambiguous and
// out_indices contains the indices of the abiguous bases in in_seq
unsigned int CSeqportUtil_implementation::GetAmbigs_ncbi4na_ncbi2na
(const CSeq_data&        in_seq,
 CSeq_data*              out_seq,
 vector<unsigned int>*   out_indices,
 unsigned int            uBeginIdx,
 unsigned int            uLength)
    const
{
    // Get read-only reference to in_seq data
    const vector<char>& in_seq_data = in_seq.GetNcbi4na().Get();

    // Get read & write reference to out_seq data
    out_seq->Reset();
    vector<char>& out_seq_data = out_seq->SetNcbi4na().Set();

    // Adjust uBeginIdx and uLength, if necessary
    if(uBeginIdx >= 2*in_seq_data.size())
        return 0;

    if((uLength == 0) || (((uBeginIdx + uLength) > 2*in_seq_data.size())))
        uLength = 2*in_seq_data.size() - uBeginIdx;

    // Save uBeginIdx and adjust uBeginIdx = 0 mod 2
    unsigned int uBeginSav = uBeginIdx;
    unsigned int uLenSav = uLength;
    uLength += uBeginIdx % 2;
    uBeginIdx = 2*(uBeginIdx/2);

    // Allocate memory for out_seq_data and out_indices
    // Note, these will be shrunk at the end to correspond
    // to actual memory needed.  Note, in test cases, over 50% of the
    // time spent in this method is spent in the next two
    // statements and 3/4 of that is spent in the second statement.
    out_seq_data.resize(uLength/2 + (uLength % 2));
    out_indices->resize(uLength);

    // Variable to track number of ambigs
    unsigned int uNumAmbigs = 0;

    // Get iterators to input sequence
    vector<char>::const_iterator i_in;
    vector<char>::const_iterator i_in_begin =
        in_seq_data.begin() + uBeginIdx/2;
    vector<char>::const_iterator i_in_end =
        i_in_begin + uLength/2 + (uLength % 2);

    // Get iterators to out_seq_data and out_indices
    vector<char>::iterator i_out_seq = out_seq_data.begin();
    vector<unsigned int>::iterator i_out_idx = out_indices->begin();

    // Index of current input seq base
    unsigned int uIdx = uBeginIdx;

    // Loop through input sequence looking for ambiguities
    for(i_in = i_in_begin; i_in != i_in_end; ++i_in) {
        switch (m_DetectAmbigNcbi4naNcbi2na->m_Table
                [static_cast<unsigned char>(*i_in)]) {

        case 1:    // Low order input nible ambiguous

            // Put low order input nible in low order output nible
            if(uNumAmbigs & 1)
                {
                    (*i_out_seq) |= (*i_in) & '\x0f';
                    ++i_out_seq;
                }

            // Put low order input nible in high order output nible
            else
                (*i_out_seq) = (*i_in) << 4;

            // Record input index that was ambiguous
            (*i_out_idx) = uIdx + 1;
            ++i_out_idx;

            // Increment number of ambiguities
            uNumAmbigs++;
            break;

        case 2:    // High order input nible ambiguous

            // Put high order input nible in low order output nible
            if(uNumAmbigs & 1)
                {
                    (*i_out_seq) |= ((*i_in) >> 4) & '\x0f';
                    ++i_out_seq;
                }

            // Put high order input nible in high order output nible
            else
                (*i_out_seq) = (*i_in) & '\xf0';

            // Record input index that was ambiguous
            (*i_out_idx) = uIdx;
            ++i_out_idx;

            // Increment number of ambiguities
            uNumAmbigs++;
            break;

        case 3:    // Both input nibles ambiguous

            // Put high order input nible in low order
            // output nible, move to the next output byte
            // and put the low order input nibble in the
            // high order output nible.
            if(uNumAmbigs & 1)
                {
                    (*i_out_seq) |= ((*i_in) >> 4) & '\x0f';
                    (*(++i_out_seq)) = (*i_in) << 4;
                }

            // Put high order input nible in high order
            // output nible, put low order input nible
            // in low order output nible, and move to
            // next output byte
            else
                {
                    (*i_out_seq) = (*i_in);
                    ++i_out_seq;
                }

            // Record indices that were ambiguous
            (*i_out_idx) = uIdx;
            (*(++i_out_idx)) = uIdx + 1;
            ++i_out_idx;

            // Increment the number of ambiguities
            uNumAmbigs+=2;
            break;
        }

        // Increment next input byte.
        uIdx += 2;
    }

    // Shrink out_seq_data and out_indices to actual sizes needed
    out_indices->resize(uNumAmbigs);
    out_seq_data.resize(uNumAmbigs/2 + uNumAmbigs % 2);

    // Check to ensure that ambigs outside of requested range are not included
    unsigned int uKeepBeg = 0;
    unsigned int uKeepLen = 0;
    if((*out_indices)[0] < uBeginSav)
        {
            uKeepBeg = 1;
            out_indices->erase(out_indices->begin(), out_indices->begin() + 1);
        }

    if((*out_indices)[out_indices->size()-1] >= uBeginSav + uLenSav)
        {
            out_indices->pop_back();
            uKeepLen = out_indices->size();
        }

    if((uKeepBeg != 0) || (uKeepLen != 0))
        uNumAmbigs = KeepNcbi4na(out_seq, uKeepBeg, uKeepLen);

    return uNumAmbigs;
}


// Loops through an iupacna input sequence and determines
// the ambiguities that would result from conversion to an ncbi2na sequence.
// On return, out_seq contains the iupacna bases that become ambiguous and
// out_indices contains the indices of the abiguous bases in in_seq. The
// return is the number of ambiguities found.
unsigned int CSeqportUtil_implementation::GetAmbigs_iupacna_ncbi2na
(const CSeq_data&        in_seq,
 CSeq_data*              out_seq,
 vector<unsigned int>*   out_indices,
 unsigned int            uBeginIdx,
 unsigned int            uLength)
    const
{
    // Get read-only reference to in_seq data
    const string& in_seq_data = in_seq.GetIupacna().Get();

    // Get read & write reference to out_seq data
    out_seq->Reset();
    string& out_seq_data = out_seq->SetIupacna().Set();

    // Validate/adjust uBeginIdx and uLength
    if(uBeginIdx >= in_seq_data.size())
        return 0;

    if((uLength == 0) || ((uBeginIdx + uLength) > in_seq_data.size()))
        uLength = in_seq_data.size() - uBeginIdx;

    // Allocate memory for out_seq_data and out_indices
    // Note, these will be shrunk at the end to correspond
    // to actual memory needed.
    out_seq_data.resize(uLength);
    out_indices->resize(uLength);

    // Variable to track number of ambigs
    unsigned int uNumAmbigs = 0;

    // Get iterators to input sequence
    string::const_iterator i_in;
    string::const_iterator i_in_begin = in_seq_data.begin() + uBeginIdx;
    string::const_iterator i_in_end = i_in_begin + uLength;

    // Get iterators to out_seq_data and out_indices
    string::iterator i_out_seq = out_seq_data.begin();
    vector<unsigned int>::iterator i_out_idx = out_indices->begin();

    // Index of current input seq base
    unsigned int uIdx = uBeginIdx;

    // Loop through input sequence looking for ambiguities
    for(i_in = i_in_begin; i_in != i_in_end; ++i_in)
        {
            if(m_DetectAmbigIupacnaNcbi2na->m_Table
               [static_cast<unsigned char>(*i_in)] == 1)
                {
                    (*i_out_seq) = (*i_in);
                    ++i_out_seq;
                    (*i_out_idx) = uIdx;
                    ++i_out_idx;
                    ++uNumAmbigs;
                }

            ++uIdx;
        }

    out_seq_data.resize(uNumAmbigs);
    out_indices->resize(uNumAmbigs);

    return uNumAmbigs;
}


// Method to implement Keep for Ncbi2na. Returns length of
// kept sequence
unsigned int CSeqportUtil_implementation::KeepNcbi2na
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get a reference to in_seq
    vector<char>& in_seq_data = in_seq->SetNcbi2na().Set();

    // If uBeginIdx past the end of in_seq, return empty in_seq
    if(uBeginIdx >= in_seq_data.size()*4)
        {
            in_seq_data.clear();
            return 0;
        }

    // If uLength == 0, Keep from uBeginIdx to end of in_seq
    if(uLength == 0)
        uLength = 4*in_seq_data.size() - uBeginIdx;


    // If uLength goes beyond the end of the sequence, trim
    // it back to the end of the sequence
    if(uLength > (4*in_seq_data.size() - uBeginIdx))
        uLength = 4*in_seq_data.size() - uBeginIdx;

    // If entire sequence is being requested, just return
    if((uBeginIdx == 0) && (uLength >= 4*in_seq_data.size()))
        return uLength;

    // Determine index in in_seq_data that holds uBeginIdx residue
    unsigned int uStart = uBeginIdx/4;

    // Determine index within start byte
    unsigned int uStartInByte = 2 * (uBeginIdx % 4);

    // Calculate masks
    unsigned char rightMask = 0xff << uStartInByte;
    unsigned char leftMask = ~rightMask;

    // Determine index in in_seq_data that holds uBeginIdx + uLength
    // residue
    unsigned int uEnd = (uBeginIdx + uLength - 1)/4;

    // Get iterator for writting
    vector<char>::iterator i_write;

    // Determine begin and end of read
    vector<char>::iterator i_read = in_seq_data.begin() + uStart;
    vector<char>::iterator i_read_end = in_seq_data.begin() + uEnd;

    // Loop through in_seq_data and copy data of desire
    // sub sequence to begining of in_seq_data
    for(i_write = in_seq_data.begin(); i_read != i_read_end; ++i_write) {
        (*i_write) = (((*i_read) << uStartInByte) | leftMask) &
            (((*(i_read+1)) >> (8-uStartInByte)) | rightMask);
        ++i_read;
    }

    // Handle last byte
    (*i_write) = (*i_read) << uStartInByte;

    // Shrink in_seq to to size needed
    unsigned int uSize = uLength/4;
    if((uLength % 4) != 0)
        uSize++;
    in_seq_data.resize(uSize);

    return uLength;
}


// Method to implement Keep for Ncbi4na. Returns length of
// kept sequence.
unsigned int CSeqportUtil_implementation::KeepNcbi4na
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get a reference to in_seq
    vector<char>& in_seq_data = in_seq->SetNcbi4na().Set();

    // If uBeginIdx past the end of in_seq, return empty in_seq
    if(uBeginIdx >= in_seq_data.size()*2)
        {
            in_seq_data.clear();
            return 0;
        }

    // If uLength == 0, Keep from uBeginIdx to end of in_seq
    if(uLength == 0)
        uLength = 2*in_seq_data.size() - uBeginIdx;


    // If uLength goes beyond the end of the sequence, trim
    // it back to the end of the sequence
    if(uLength > (2*in_seq_data.size() - uBeginIdx))
        uLength = 2*in_seq_data.size() - uBeginIdx;

    // If entire sequence is being requested, just return
    if((uBeginIdx == 0) && (uLength >= 2*in_seq_data.size()))
        return uLength;

    // Determine index in in_seq_data that holds uBeginIdx residue
    unsigned int uStart = uBeginIdx/2;

    // Determine index within start byte
    unsigned int uStartInByte = 4 * (uBeginIdx % 2);

    // Calculate masks
    unsigned char rightMask = 0xff << uStartInByte;
    unsigned char leftMask = ~rightMask;

    // Determine index in in_seq_data that holds uBeginIdx + uLength
    // residue
    unsigned int uEnd = (uBeginIdx + uLength - 1)/2;

    // Get iterator for writting
    vector<char>::iterator i_write;

    // Determine begin and end of read
    vector<char>::iterator i_read = in_seq_data.begin() + uStart;
    vector<char>::iterator i_read_end = in_seq_data.begin() + uEnd;

    // Loop through in_seq_data and copy data of desire
    // sub sequence to begining of in_seq_data
    for(i_write = in_seq_data.begin(); i_read != i_read_end; ++i_write) {
        (*i_write) = (((*i_read) << uStartInByte) | leftMask) &
            (((*(i_read+1)) >> (8-uStartInByte)) | rightMask);
        ++i_read;
    }

    // Handle last byte
    (*i_write) = (*i_read) << uStartInByte;

    // Shrink in_seq to to size needed
    unsigned int uSize = uLength/2;
    if((uLength % 2) != 0)
        uSize++;
    in_seq_data.resize(uSize);

    return uLength;
}


// Method to implement Keep for Iupacna. Return length
// of kept sequence
unsigned int CSeqportUtil_implementation::KeepIupacna
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get a reference to in_seq
    string& in_seq_data = in_seq->SetIupacna().Set();


    // If uBeginIdx past end of in_seq, return empty in_seq
    if(uBeginIdx >= in_seq_data.size())
        {
            in_seq_data.erase();
            return 0;
        }

    // If uLength is 0, Keep from uBeginIdx to end of in_seq
    if(uLength == 0)
        uLength = in_seq_data.size() - uBeginIdx;

    // Check that uLength does not go beyond end of in_seq
    if((uBeginIdx + uLength) > in_seq_data.size())
        uLength = in_seq_data.size() - uBeginIdx;

    // If uBeginIdx == 0 and uLength == in_seq_data.size()
    // just return as the entire sequence is being requested
    if((uBeginIdx == 0) && (uLength >= in_seq_data.size()))
        return uLength;

    // Get two iterators on in_seq, one read and one write
    string::iterator i_read;
    string::iterator i_write;

    // Determine begin and end of read
    i_read = in_seq_data.begin() + uBeginIdx;
    string::iterator i_read_end = i_read + uLength;

    // Loop through in_seq for uLength bases
    // and shift uBeginIdx to beginning
    for(i_write = in_seq_data.begin(); i_read != i_read_end; ++i_write)
        {
            (*i_write) = (*i_read);
            ++i_read;
        }

    // Resize in_seq_data to uLength
    in_seq_data.resize(uLength);

    return uLength;
}


// Method to implement Keep for Ncbieaa
unsigned int CSeqportUtil_implementation::KeepNcbieaa
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get a reference to in_seq
    string& in_seq_data = in_seq->SetNcbieaa().Set();


    // If uBeginIdx past end of in_seq, return empty in_seq
    if(uBeginIdx >= in_seq_data.size())
        {
            in_seq_data.erase();
            return 0;
        }

    // If uLength is 0, Keep from uBeginIdx to end of in_seq
    if(uLength == 0)
        uLength = in_seq_data.size() - uBeginIdx;

    // Check that uLength does not go beyond end of in_seq
    if((uBeginIdx + uLength) > in_seq_data.size())
        uLength = in_seq_data.size() - uBeginIdx;

    // If uBeginIdx == 0 and uLength == in_seq_data.size()
    // just return as the entire sequence is being requested
    if((uBeginIdx == 0) && (uLength >= in_seq_data.size()))
        return uLength;

    // Get two iterators on in_seq, one read and one write
    string::iterator i_read;
    string::iterator i_write;

    // Determine begin and end of read
    i_read = in_seq_data.begin() + uBeginIdx;
    string::iterator i_read_end = i_read + uLength;

    // Loop through in_seq for uLength bases
    // and shift uBeginIdx to beginning
    for(i_write = in_seq_data.begin(); i_read != i_read_end; ++i_write) {
        (*i_write) = (*i_read);
        ++i_read;
    }

    // Resize in_seq_data to uLength
    in_seq_data.resize(uLength);

    return uLength;
}


// Method to implement Keep for Ncbistdaa
unsigned int CSeqportUtil_implementation::KeepNcbistdaa
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get a reference to in_seq
    vector<char>& in_seq_data = in_seq->SetNcbistdaa().Set();

    // If uBeginIdx past end of in_seq, return empty in_seq
    if(uBeginIdx >= in_seq_data.size())
        {
            in_seq_data.clear();
            return 0;
        }

    // If uLength is 0, Keep from uBeginIdx to end of in_seq
    if(uLength == 0)
        uLength = in_seq_data.size() - uBeginIdx;

    // Check that uLength does not go beyond end of in_seq
    if((uBeginIdx + uLength) > in_seq_data.size())
        uLength = in_seq_data.size() - uBeginIdx;

    // If uBeginIdx == 0 and uLength == in_seq_data.size()
    // just return as the entire sequence is being requested
    if((uBeginIdx == 0) && (uLength >= in_seq_data.size()))
        return uLength;

    // Get two iterators on in_seq, one read and one write
    vector<char>::iterator i_read;
    vector<char>::iterator i_write;

    // Determine begin and end of read
    i_read = in_seq_data.begin() + uBeginIdx;
    vector<char>::iterator i_read_end = i_read + uLength;

    // Loop through in_seq for uLength bases
    // and shift uBeginIdx to beginning
    for(i_write = in_seq_data.begin(); i_read != i_read_end; ++i_write) {
        (*i_write) = (*i_read);
        ++i_read;
    }

    // Resize in_seq_data to uLength
    in_seq_data.resize(uLength);

    return uLength;
}


// Method to implement Keep for Iupacaa
unsigned int CSeqportUtil_implementation::KeepIupacaa
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get a reference to in_seq
    string& in_seq_data = in_seq->SetIupacaa().Set();


    // If uBeginIdx past end of in_seq, return empty in_seq
    if (uBeginIdx >= in_seq_data.size()) {
        in_seq_data.erase();
        return 0;
    }

    // If uLength is 0, Keep from uBeginIdx to end of in_seq
    if(uLength == 0)
        uLength = in_seq_data.size() - uBeginIdx;

    // Check that uLength does not go beyond end of in_seq
    if((uBeginIdx + uLength) > in_seq_data.size())
        uLength = in_seq_data.size() - uBeginIdx;

    // If uBeginIdx == 0 and uLength == in_seq_data.size()
    // just return as the entire sequence is being requested
    if((uBeginIdx == 0) && (uLength >= in_seq_data.size()))
        return uLength;

    // Get two iterators on in_seq, one read and one write
    string::iterator i_read;
    string::iterator i_write;

    // Determine begin and end of read
    i_read = in_seq_data.begin() + uBeginIdx;
    string::iterator i_read_end = i_read + uLength;

    // Loop through in_seq for uLength bases
    // and shift uBeginIdx to beginning
    for(i_write = in_seq_data.begin(); i_read != i_read_end; ++i_write) {
        (*i_write) = (*i_read);
        ++i_read;
    }

    // Resize in_seq_data to uLength
    in_seq_data.resize(uLength);

    return uLength;
}



// Methods to complement na sequences

// In place methods
unsigned int CSeqportUtil_implementation::ComplementIupacna
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Keep just the part of in_seq that will be complemented
    unsigned int uKept = KeepIupacna(in_seq, uBeginIdx, uLength);

    // Get in_seq data
    string& in_seq_data = in_seq->SetIupacna().Set();

    // Get an iterator to in_seq_data
    string::iterator i_data;

    // Get end of iteration--needed for performance
    string::iterator i_data_end = in_seq_data.end();

    // Loop through the input sequence and complement it
    for(i_data = in_seq_data.begin(); i_data != i_data_end; ++i_data)
        (*i_data) =
            m_Iupacna_complement->m_Table[static_cast<unsigned char>(*i_data)];

    return uKept;
}


unsigned int CSeqportUtil_implementation::ComplementNcbi2na
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Keep just the part of in_seq that will be complemented
    unsigned int uKept = KeepNcbi2na(in_seq, uBeginIdx, uLength);

    // Get in_seq data
    vector<char>& in_seq_data = in_seq->SetNcbi2na().Set();

    // Get an iterator to in_seq_data
    vector<char>::iterator i_data;

    // Get end of iteration
    vector<char>::iterator i_data_end = in_seq_data.end();

    // Loop through the input sequence and complement it
    for(i_data = in_seq_data.begin(); i_data != i_data_end; ++i_data)
        (*i_data) =
            m_Ncbi2naComplement->m_Table[static_cast<unsigned char>(*i_data)];

    return uKept;
}


unsigned int CSeqportUtil_implementation::ComplementNcbi4na
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Keep just the part of in_seq that will be complemented
    unsigned int uKept = KeepNcbi4na(in_seq, uBeginIdx, uLength);

    // Get in_seq data
    vector<char>& in_seq_data = in_seq->SetNcbi4na().Set();

    // Get an iterator to in_seq_data
    vector<char>::iterator i_data;

    // Get end of iteration--done for performance
    vector<char>::iterator i_data_end = in_seq_data.end();

    // Loop through the input sequence and complement it
    for(i_data = in_seq_data.begin(); i_data != i_data_end; ++i_data)
        (*i_data) =
            m_Ncbi4naComplement->m_Table[static_cast<unsigned char>(*i_data)];

    return uKept;
}


// Complement in copy methods
unsigned int CSeqportUtil_implementation::ComplementIupacna
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    unsigned int uKept = GetIupacnaCopy(in_seq, out_seq, uBeginIdx, uLength);
    unsigned int uIdx1 = 0, uIdx2 = 0;
    ComplementIupacna(out_seq, uIdx1, uIdx2);
    return uKept;
}


unsigned int CSeqportUtil_implementation::ComplementNcbi2na
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    unsigned int uKept = GetNcbi2naCopy(in_seq, out_seq, uBeginIdx, uLength);
    unsigned int uIdx1 = 0, uIdx2 = 0;
    ComplementNcbi2na(out_seq, uIdx1, uIdx2);
    return uKept;
}


unsigned int CSeqportUtil_implementation::ComplementNcbi4na
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    unsigned int uKept = GetNcbi4naCopy(in_seq, out_seq, uBeginIdx, uLength);
    unsigned int uIdx1 = 0, uIdx2 = 0;
    ComplementNcbi4na(out_seq, uIdx1, uIdx2);
    return uKept;
}


// Methods to reverse na sequences

// In place methods
unsigned int CSeqportUtil_implementation::ReverseIupacna
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Keep just the part of in_seq that will be reversed
    unsigned int uKept = KeepIupacna(in_seq, uBeginIdx, uLength);

    // Get in_seq data
    string& in_seq_data = in_seq->SetIupacna().Set();

    // Reverse the order of the string
    reverse(in_seq_data.begin(), in_seq_data.end());

    return uKept;
}


unsigned int CSeqportUtil_implementation::ReverseNcbi2na
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get a reference to in_seq data
    vector<char>& in_seq_data = in_seq->SetNcbi2na().Set();

    // Validate and adjust uBeginIdx and uLength
    if(uBeginIdx >= 4*in_seq_data.size())
        {
            in_seq_data.erase(in_seq_data.begin(), in_seq_data.end());
            return 0;
        }

    // If uLength is zero, set to end of sequence
    if(uLength == 0)
        uLength = 4*in_seq_data.size() - uBeginIdx;

    // Ensure that uLength not beyond end of sequence
    if((uBeginIdx + uLength) > (4 * in_seq_data.size()))
        uLength = 4*in_seq_data.size() - uBeginIdx;

    // Determine start and end bytes
    unsigned int uStart = uBeginIdx/4;
    unsigned int uEnd = uStart + (uLength - 1 +(uBeginIdx % 4))/4 + 1;

    // Declare an iterator and get end of sequence
    vector<char>::iterator i_in;
    vector<char>::iterator i_in_begin = in_seq_data.begin() + uStart;
    vector<char>::iterator i_in_end = in_seq_data.begin() + uEnd;

    // Loop through in_seq_data and reverse residues in each byte
    for(i_in = i_in_begin; i_in != i_in_end; ++i_in)
        (*i_in) = m_Ncbi2naRev->m_Table[static_cast<unsigned char>(*i_in)];

    // Reverse the bytes in the sequence
    reverse(i_in_begin, i_in_end);

    // Keep just the requested part of the sequence
    unsigned int uJagged = 3 - ((uBeginIdx + uLength - 1) % 4) + 4*uStart;
    return KeepNcbi2na(in_seq, uJagged, uLength);
}


unsigned int CSeqportUtil_implementation::ReverseNcbi4na
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    // Get a reference to in_seq data
    vector<char>& in_seq_data = in_seq->SetNcbi4na().Set();

    // Validate and adjust uBeginIdx and uLength
    if(uBeginIdx >= 2*in_seq_data.size())
        {
            in_seq_data.erase(in_seq_data.begin(), in_seq_data.end());
            return 0;
        }

    // If uLength is zero, set to end of sequence
    if(uLength == 0)
        uLength = 2*in_seq_data.size() - uBeginIdx;

    // Ensure that uLength not beyond end of sequence
    if((uBeginIdx + uLength) > (2 * in_seq_data.size()))
        uLength = 2*in_seq_data.size() - uBeginIdx;

    // Determine start and end bytes
    unsigned int uStart = uBeginIdx/2;
    unsigned int uEnd = uStart + (uLength - 1 +(uBeginIdx % 2))/2 + 1;

    // Declare an iterator and get end of sequence
    vector<char>::iterator i_in;
    vector<char>::iterator i_in_begin = in_seq_data.begin() + uStart;
    vector<char>::iterator i_in_end = in_seq_data.begin() + uEnd;

    // Loop through in_seq_data and reverse residues in each byte
    for(i_in = i_in_begin; i_in != i_in_end; ++i_in)
        (*i_in) = m_Ncbi4naRev->m_Table[static_cast<unsigned char>(*i_in)];

    // Reverse the bytes in the sequence
    reverse(i_in_begin, i_in_end);

    // Keep just the requested part of the sequence
    unsigned int uJagged = 1 - ((uBeginIdx + uLength - 1) % 2) + 2*uStart;
    return KeepNcbi4na(in_seq, uJagged, uLength);
}


// Reverse in copy methods
unsigned int CSeqportUtil_implementation::ReverseIupacna
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    GetIupacnaCopy(in_seq, out_seq, uBeginIdx, uLength);

    unsigned int uIdx1 = 0, uIdx2 = uLength;
    return ReverseIupacna(out_seq, uIdx1, uIdx2);
}


unsigned int CSeqportUtil_implementation::ReverseNcbi2na
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    GetNcbi2naCopy(in_seq, out_seq, uBeginIdx, uLength);

    unsigned int uIdx1 = 0, uIdx2 = uLength;
    return ReverseNcbi2na(out_seq, uIdx1, uIdx2);
}


unsigned int CSeqportUtil_implementation::ReverseNcbi4na
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    GetNcbi4naCopy(in_seq, out_seq, uBeginIdx, uLength);

    unsigned int uIdx1 = 0, uIdx2 = uLength;
    return ReverseNcbi4na(out_seq, uIdx1, uIdx2);
}


// Methods to reverse-complement an na sequences

// In place methods
unsigned int CSeqportUtil_implementation::ReverseComplementIupacna
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    ReverseIupacna(in_seq, uBeginIdx, uLength);

    unsigned int uIdx = 0;
    return ComplementIupacna(in_seq, uIdx, uLength);
}


unsigned int CSeqportUtil_implementation::ReverseComplementNcbi2na
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    ReverseNcbi2na(in_seq, uBeginIdx, uLength);

    unsigned int uIdx = 0;
    return ComplementNcbi2na(in_seq, uIdx, uLength);
}


unsigned int CSeqportUtil_implementation::ReverseComplementNcbi4na
(CSeq_data*        in_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    ReverseNcbi4na(in_seq, uBeginIdx, uLength);

    unsigned int uIdx = 0;
    return ComplementNcbi4na(in_seq, uIdx, uLength);
}


// Reverse in copy methods
unsigned int CSeqportUtil_implementation::ReverseComplementIupacna
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    ReverseIupacna(in_seq, out_seq, uBeginIdx, uLength);

    unsigned int uIdx = 0;
    return ComplementIupacna(out_seq, uIdx, uLength);
}


unsigned int CSeqportUtil_implementation::ReverseComplementNcbi2na
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    ReverseNcbi2na(in_seq, out_seq, uBeginIdx, uLength);

    unsigned int uIdx = 0;
    return ComplementNcbi2na(out_seq, uIdx, uLength);
}


unsigned int CSeqportUtil_implementation::ReverseComplementNcbi4na
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 unsigned int      uBeginIdx,
 unsigned int      uLength)
    const
{
    ReverseNcbi4na(in_seq, out_seq, uBeginIdx, uLength);

    unsigned int uIdx = 0;
    return ComplementNcbi4na(out_seq, uIdx, uLength);
}


// Append methods
unsigned int CSeqportUtil_implementation::AppendIupacna
(CSeq_data*        out_seq,
 const CSeq_data&  in_seq1,
 unsigned int      uBeginIdx1,
 unsigned int      uLength1,
 const CSeq_data&  in_seq2,
 unsigned int      uBeginIdx2,
 unsigned int      uLength2)
    const
{
    // Get references to in_seqs
    const string& in_seq1_data = in_seq1.GetIupacna().Get();
    const string& in_seq2_data = in_seq2.GetIupacna().Get();

    // Get a reference to out_seq
    out_seq->Reset();
    string& out_seq_data = out_seq->SetIupacna().Set();

    // Validate and Adjust uBeginIdx_ and uLength_
    if((uBeginIdx1 >= in_seq1_data.size()) &&
       (uBeginIdx2 >= in_seq2_data.size()))
        return 0;

    if(((uBeginIdx1 + uLength1) > in_seq1_data.size()) || uLength1 == 0)
        uLength1 = in_seq1_data.size() - uBeginIdx1;

    if(((uBeginIdx2 + uLength2) > in_seq2_data.size()) || uLength2 == 0)
        uLength2 = in_seq2_data.size() - uBeginIdx2;

    // Append the strings
    out_seq_data.append(in_seq1_data.substr(uBeginIdx1,uLength1));
    out_seq_data.append(in_seq2_data.substr(uBeginIdx2,uLength2));

    return uLength1 + uLength2;
}


unsigned int CSeqportUtil_implementation::AppendNcbi2na
(CSeq_data*        out_seq,
 const CSeq_data&  in_seq1,
 unsigned int      uBeginIdx1,
 unsigned int      uLength1,
 const CSeq_data&  in_seq2,
 unsigned int      uBeginIdx2,
 unsigned int      uLength2)
    const
{
    // Get references to in_seqs
    const vector<char>& in_seq1_data = in_seq1.GetNcbi2na().Get();
    const vector<char>& in_seq2_data = in_seq2.GetNcbi2na().Get();

    // Get a reference to out_seq
    out_seq->Reset();
    vector<char>& out_seq_data = out_seq->SetNcbi2na().Set();

    // Handle case where both uBeginidx go beyond in_seq
    if((uBeginIdx1 >= 4*in_seq1_data.size()) &&
       (uBeginIdx2 >= 4*in_seq2_data.size()))
        return 0;

    // Handle case where uBeginIdx1 goes beyond end of in_seq1
    if(uBeginIdx1 >= 4*in_seq1_data.size())
        return GetNcbi2naCopy(in_seq2, out_seq, uBeginIdx2, uLength2);

    // Handle case where uBeginIdx2 goes beyond end of in_seq2
    if(uBeginIdx2 >= 4*in_seq2_data.size())
        return GetNcbi2naCopy(in_seq1, out_seq, uBeginIdx1, uLength1);

    // Validate and Adjust uBeginIdx_ and uLength_
    if(((uBeginIdx1 + uLength1) > 4*in_seq1_data.size()) || uLength1 == 0)
        uLength1 = 4*in_seq1_data.size() - uBeginIdx1;

    if(((uBeginIdx2 + uLength2) > 4*in_seq2_data.size()) || uLength2 == 0)
        uLength2 = 4*in_seq2_data.size() - uBeginIdx2;


    // Resize out_seq_data to hold appended sequence
    unsigned int uTotalLength = uLength1 + uLength2;
    if((uTotalLength % 4) == 0)
        out_seq_data.resize(uTotalLength/4);
    else
        out_seq_data.resize(uTotalLength/4 + 1);

    // Calculate bit shifts required for in_seq1
    unsigned int lShift1 = 2*(uBeginIdx1 % 4);
    unsigned int rShift1 = 8 - lShift1;

    // Calculate bit shifts required for in_seq2
    unsigned int lShift2, rShift2, uCase;
    unsigned int uVacantIdx = 2*(uLength1 % 4);
    unsigned int uStartIdx = 2*(uBeginIdx2 % 4);
    if((uVacantIdx < uStartIdx) && (uVacantIdx > 0))
        {
            uCase = 0;
            lShift2 = uStartIdx - uVacantIdx;
            rShift2 = 8 - lShift2;
        }
    else if((uVacantIdx < uStartIdx) && (uVacantIdx == 0))
        {
            uCase = 1;
            lShift2 = uStartIdx;
            rShift2 = 8 - lShift2;
        }
    else if((uVacantIdx == uStartIdx) && (uVacantIdx > 0))
        {
            uCase = 2;
            lShift2 = 0;
            rShift2 = 8;
        }
    else if((uVacantIdx == uStartIdx) && (uVacantIdx == 0))
        {
            uCase = 3;
            lShift2 = 0;
            rShift2 = 8;
        }
    else
        {
            uCase = 4;
            rShift2 = uVacantIdx - uStartIdx;
            lShift2 = 8 - rShift2;
        }


    // Determine begin and end points for iterators.
    unsigned int uStart1 = uBeginIdx1/4;
    unsigned int uEnd1;
    if(((uBeginIdx1 + uLength1) % 4) == 0)
        uEnd1 = (uBeginIdx1 + uLength1)/4;
    else
        uEnd1 = (uBeginIdx1 + uLength1)/4 + 1;

    unsigned int uStart2 = uBeginIdx2/4;
    unsigned int uEnd2;
    if(((uBeginIdx2 + uLength2) % 4) == 0)
        uEnd2 = (uBeginIdx2 + uLength2)/4;
    else
        uEnd2 = (uBeginIdx2 + uLength2)/4 + 1;

    // Get begin and end positions on in_seqs
    vector<char>::const_iterator i_in1_begin = in_seq1_data.begin() + uStart1;
    vector<char>::const_iterator i_in1_end = in_seq1_data.begin() + uEnd1 - 1;
    vector<char>::const_iterator i_in2_begin = in_seq2_data.begin() + uStart2;
    vector<char>::const_iterator i_in2_end = in_seq2_data.begin() + uEnd2;

    // Declare iterators
    vector<char>::iterator i_out = out_seq_data.begin() - 1;
    vector<char>::const_iterator i_in1;
    vector<char>::const_iterator i_in2;

    // Insert in_seq1 into out_seq
    for(i_in1 = i_in1_begin; i_in1 != i_in1_end; ++i_in1)
        (*(++i_out)) = ((*i_in1) << lShift1) | ((*(i_in1+1) & 255) >> rShift1);

    // Handle last byte for in_seq1 if necessary
    unsigned int uEndOutByte;
    if((uLength1 % 4) == 0)
        uEndOutByte = uLength1/4 - 1;
    else
        uEndOutByte = uLength1/4;
    if(i_out != (out_seq_data.begin() + uEndOutByte))
        (*(++i_out)) = (*i_in1) << lShift1;

    // Connect in_seq1 and in_seq2
    unsigned char uMask1 = 255 << (8 - 2*(uLength1 % 4));
    unsigned char uMask2 = 255 >> (2*(uBeginIdx2 % 4));
    unsigned int uSeq2Inc = 1;

    switch (uCase) {
    case 0: // 0 < uVacantIdx < uStartIdx
        if((i_in2_begin + 1) == i_in2_end)
            {
                (*i_out) &= uMask1;
                (*i_out) |= ((*i_in2_begin) & uMask2) << lShift2;
                return uTotalLength;
            }
        else
            {
                (*i_out) &= uMask1;
                (*i_out) |=
                    (((*i_in2_begin) & uMask2) << lShift2) |
                    (((*(i_in2_begin+1)) & 255) >> rShift2);
            }
        break;
    case 1: // 0 == uVacantIdx < uStartIdx
        if((i_in2_begin + 1) == i_in2_end)
            {
                (*(++i_out)) = (*i_in2_begin) << lShift2;
                return uTotalLength;
            }
        else
            {
                (*(++i_out)) =
                    ((*i_in2_begin) << lShift2) |
                    (((*(i_in2_begin+1)) & 255) >> rShift2);
            }
        break;
    case 2: // uVacantIdx == uStartIdx > 0
        (*i_out) &= uMask1;
        (*i_out) |= (*i_in2_begin) & uMask2;
        if((i_in2_begin + 1) == i_in2_end)
            return uTotalLength;
        break;
    case 3: // uVacantIdx == uStartIdx == 0
        (*(++i_out)) = (*i_in2_begin);
        if((i_in2_begin + 1) == i_in2_end)
            return uTotalLength;
        break;
    case 4: // uVacantIdx > uStartIdx
        if((i_in2_begin + 1) == i_in2_end)
            {
                (*i_out) &= uMask1;
                (*i_out) |= ((*i_in2_begin) & uMask2) >> rShift2;
                if(++i_out != out_seq_data.end())
                    (*i_out) = (*i_in2_begin) << lShift2;
                return uTotalLength;
            }
        else
            {
                (*i_out) &= uMask1;
                (*i_out) |=
                    (((*i_in2_begin) & uMask2) >> rShift2) |
                    ((*(i_in2_begin+1) & ~uMask2) << lShift2);
                uSeq2Inc = 0;
            }

    }

    // Insert in_seq2 into out_seq
    for(i_in2 = i_in2_begin+uSeq2Inc; (i_in2 != i_in2_end) &&
            ((i_in2+1) != i_in2_end); ++i_in2) {
        (*(++i_out)) = ((*i_in2) << lShift2) | ((*(i_in2+1) & 255) >> rShift2);
    }

    // Handle last byte for in_seq2, if there is one
    if((++i_out != out_seq_data.end()) && (i_in2 != i_in2_end))
        (*i_out) = (*i_in2) << lShift2;

    return uLength1 + uLength2;
}


unsigned int CSeqportUtil_implementation::AppendNcbi4na
(CSeq_data*        out_seq,
 const CSeq_data&  in_seq1,
 unsigned int      uBeginIdx1,
 unsigned int      uLength1,
 const CSeq_data&  in_seq2,
 unsigned int      uBeginIdx2,
 unsigned int      uLength2)
    const
{
    // Get references to in_seqs
    const vector<char>& in_seq1_data = in_seq1.GetNcbi4na().Get();
    const vector<char>& in_seq2_data = in_seq2.GetNcbi4na().Get();

    // Get a reference to out_seq
    out_seq->Reset();
    vector<char>& out_seq_data = out_seq->SetNcbi4na().Set();

    // Handle both uBeginidx go beyond end of in_seq
    if((uBeginIdx1 >= 4*in_seq1_data.size()) &&
       (uBeginIdx2 >= 4*in_seq2_data.size()))
        return 0;

    // Handle case where uBeginIdx1 goes beyond end of in_seq1
    if(uBeginIdx1 >= 4*in_seq1_data.size())
        return GetNcbi4naCopy(in_seq2, out_seq, uBeginIdx2, uLength2);

    // Handle case where uBeginIdx2 goes beyond end of in_seq2
    if(uBeginIdx2 >= 4*in_seq2_data.size())
        return GetNcbi4naCopy(in_seq1, out_seq, uBeginIdx1, uLength1);

    // Validate and Adjust uBeginIdx_ and uLength_
    if(((uBeginIdx1 + uLength1) > 2*in_seq1_data.size()) || uLength1 == 0)
        uLength1 = 2*in_seq1_data.size() - uBeginIdx1;

    if(((uBeginIdx2 + uLength2) > 2*in_seq2_data.size()) || uLength2 == 0)
        uLength2 = 2*in_seq2_data.size() - uBeginIdx2;

    // Resize out_seq_data to hold appended sequence
    unsigned int uTotalLength = uLength1 + uLength2;
    if((uTotalLength % 2) == 0)
        out_seq_data.resize(uTotalLength/2);
    else
        out_seq_data.resize(uTotalLength/2 + 1);

    // Calculate bit shifts required for in_seq1
    unsigned int lShift1 = 4*(uBeginIdx1 % 2);
    unsigned int rShift1 = 8 - lShift1;

    // Calculate bit shifts required for in_seq2
    unsigned int lShift2, rShift2, uCase;
    unsigned int uVacantIdx = 4*(uLength1 % 2);
    unsigned int uStartIdx = 4*(uBeginIdx2 % 2);
    if((uVacantIdx < uStartIdx))
        {
            uCase = 1;
            lShift2 = uStartIdx;
            rShift2 = 8 - lShift2;
        }
    else if((uVacantIdx == uStartIdx) && (uVacantIdx > 0))
        {
            uCase = 2;
            lShift2 = 0;
            rShift2 = 8;
        }
    else if((uVacantIdx == uStartIdx) && (uVacantIdx == 0))
        {
            uCase = 3;
            lShift2 = 0;
            rShift2 = 8;
        }
    else
        {
            uCase = 4;
            rShift2 = uVacantIdx - uStartIdx;
            lShift2 = 8 - rShift2;
        }


    // Determine begin and end points for iterators.
    unsigned int uStart1 = uBeginIdx1/2;
    unsigned int uEnd1;
    if(((uBeginIdx1 + uLength1) % 2) == 0)
        uEnd1 = (uBeginIdx1 + uLength1)/2;
    else
        uEnd1 = (uBeginIdx1 + uLength1)/2 + 1;

    unsigned int uStart2 = uBeginIdx2/2;
    unsigned int uEnd2;
    if(((uBeginIdx2 + uLength2) % 2) == 0)
        uEnd2 = (uBeginIdx2 + uLength2)/2;
    else
        uEnd2 = (uBeginIdx2 + uLength2)/2 + 1;

    // Get begin and end positions on in_seqs
    vector<char>::const_iterator i_in1_begin = in_seq1_data.begin() + uStart1;
    vector<char>::const_iterator i_in1_end = in_seq1_data.begin() + uEnd1 - 1;
    vector<char>::const_iterator i_in2_begin = in_seq2_data.begin() + uStart2;
    vector<char>::const_iterator i_in2_end = in_seq2_data.begin() + uEnd2;

    // Declare iterators
    vector<char>::iterator i_out = out_seq_data.begin() - 1;
    vector<char>::const_iterator i_in1;
    vector<char>::const_iterator i_in2;

    // Insert in_seq1 into out_seq
    for(i_in1 = i_in1_begin; i_in1 != i_in1_end; ++i_in1)
        (*(++i_out)) = ((*i_in1) << lShift1) | ((*(i_in1+1) & 255) >> rShift1);

    // Handle last byte for in_seq1 if necessary
    unsigned int uEndOutByte;
    if((uLength1 % 2) == 0)
        uEndOutByte = uLength1/2 - 1;
    else
        uEndOutByte = uLength1/2;
    if(i_out != (out_seq_data.begin() + uEndOutByte))
        (*(++i_out)) = (*i_in1) << lShift1;

    // Connect in_seq1 and in_seq2
    unsigned char uMask1 = 255 << (8 - 4*(uLength1 % 2));
    unsigned char uMask2 = 255 >> (4*(uBeginIdx2 % 2));
    unsigned int uSeq2Inc = 1;

    switch (uCase) {
    case 1: // 0 == uVacantIdx < uStartIdx
        if((i_in2_begin+1) == i_in2_end)
            {
                (*(++i_out)) = (*i_in2_begin) << lShift2;
                return uTotalLength;
            }
        else
            {
                (*(++i_out)) =
                    ((*i_in2_begin) << lShift2) |
                    (((*(i_in2_begin+1)) & 255) >> rShift2);
            }
        break;
    case 2: // uVacantIdx == uStartIdx > 0
        (*i_out) &= uMask1;
        (*i_out) |= (*i_in2_begin) & uMask2;
        if((i_in2_begin+1) == i_in2_end)
            return uTotalLength;
        break;
    case 3: // uVacantIdx == uStartIdx == 0
        (*(++i_out)) = (*i_in2_begin);
        if((i_in2_begin+1) == i_in2_end)
            return uTotalLength;
        break;
    case 4: // uVacantIdx > uStartIdx
        if((i_in2_begin+1) == i_in2_end)
            {
                (*i_out) &= uMask1;
                (*i_out) |= ((*i_in2_begin) & uMask2) >> rShift2;
                if(++i_out != out_seq_data.end())
                    (*i_out) = (*i_in2_begin) << lShift2;
                return uTotalLength;
            }
        else
            {
                (*i_out) &= uMask1;
                (*i_out) |=
                    (((*i_in2_begin) & uMask2) >> rShift2) |
                    ((*(i_in2_begin+1) & ~uMask2) << lShift2);
                uSeq2Inc = 0;
            }

    }

    // Insert in_seq2 into out_seq
    for(i_in2 = i_in2_begin+uSeq2Inc; (i_in2 != i_in2_end) &&
            ((i_in2+1) != i_in2_end); ++i_in2) {
        (*(++i_out)) =
            ((*i_in2) << lShift2) | ((*(i_in2+1) & 255) >> rShift2);
    }

    // Handle last byte for in_seq2, if there is one
    if((++i_out != out_seq_data.end()) && (i_in2 != i_in2_end))
        (*i_out) = (*i_in2) << lShift2;

    return uTotalLength;
}


unsigned int CSeqportUtil_implementation::AppendNcbieaa
(CSeq_data*        out_seq,
 const CSeq_data&  in_seq1,
 unsigned int      uBeginIdx1,
 unsigned int      uLength1,
 const CSeq_data&  in_seq2,
 unsigned int      uBeginIdx2,
 unsigned int      uLength2)
    const
{
    // Get references to in_seqs
    const string& in_seq1_data = in_seq1.GetNcbieaa().Get();
    const string& in_seq2_data = in_seq2.GetNcbieaa().Get();

    // Get a reference to out_seq
    out_seq->Reset();
    string& out_seq_data = out_seq->SetNcbieaa().Set();

    // Validate and Adjust uBeginIdx_ and uLength_
    if((uBeginIdx1 >= in_seq1_data.size()) &&
       (uBeginIdx2 >= in_seq2_data.size()))
        {
            return 0;
        }

    if(((uBeginIdx1 + uLength1) > in_seq1_data.size()) || uLength1 == 0)
        uLength1 = in_seq1_data.size() - uBeginIdx1;

    if(((uBeginIdx2 + uLength2) > in_seq2_data.size()) || uLength2 == 0)
        uLength2 = in_seq2_data.size() - uBeginIdx2;

    // Append the strings
    out_seq_data.append(in_seq1_data.substr(uBeginIdx1,uLength1));
    out_seq_data.append(in_seq2_data.substr(uBeginIdx2,uLength2));

    return uLength1 + uLength2;
}


unsigned int CSeqportUtil_implementation::AppendNcbistdaa
(CSeq_data*          out_seq,
 const CSeq_data&    in_seq1,
 unsigned int        uBeginIdx1,
 unsigned int        uLength1,
 const CSeq_data&    in_seq2,
 unsigned int        uBeginIdx2,
 unsigned int        uLength2)
    const
{
    // Get references to in_seqs
    const vector<char>& in_seq1_data = in_seq1.GetNcbistdaa().Get();
    const vector<char>& in_seq2_data = in_seq2.GetNcbistdaa().Get();

    // Get a reference to out_seq
    out_seq->Reset();
    vector<char>& out_seq_data = out_seq->SetNcbistdaa().Set();

    // Validate and Adjust uBeginIdx_ and uLength_
    if((uBeginIdx1 >= in_seq1_data.size()) &&
       (uBeginIdx2 >= in_seq2_data.size()))
        return 0;

    if(((uBeginIdx1 + uLength1) > in_seq1_data.size()) || uLength1 == 0)
        uLength1 = in_seq1_data.size() - uBeginIdx1;

    if(((uBeginIdx2 + uLength2) > in_seq2_data.size()) || uLength2 == 0)
        uLength2 = in_seq2_data.size() - uBeginIdx2;

    // Get begin and end positions on in_seqs
    vector<char>::const_iterator i_in1_begin =
        in_seq1_data.begin() + uBeginIdx1;
    vector<char>::const_iterator i_in1_end = i_in1_begin + uLength1;
    vector<char>::const_iterator i_in2_begin =
        in_seq2_data.begin() + uBeginIdx2;
    vector<char>::const_iterator i_in2_end = i_in2_begin + uLength2;

    // Insert the in_seqs into out_seq
    out_seq_data.insert(out_seq_data.end(), i_in1_begin, i_in1_end);
    out_seq_data.insert(out_seq_data.end(), i_in2_begin, i_in2_end);

    return uLength1 + uLength2;
}


unsigned int CSeqportUtil_implementation::AppendIupacaa
(CSeq_data*          out_seq,
 const CSeq_data&    in_seq1,
 unsigned int        uBeginIdx1,
 unsigned int        uLength1,
 const CSeq_data&    in_seq2,
 unsigned int        uBeginIdx2,
 unsigned int        uLength2)
    const
{
    // Get references to in_seqs
    const string& in_seq1_data = in_seq1.GetIupacaa().Get();
    const string& in_seq2_data = in_seq2.GetIupacaa().Get();

    // Get a reference to out_seq
    out_seq->Reset();
    string& out_seq_data = out_seq->SetIupacaa().Set();

    // Validate and Adjust uBeginIdx_ and uLength_
    if((uBeginIdx1 >= in_seq1_data.size()) &&
       (uBeginIdx2 >= in_seq2_data.size()))
        {
            return 0;
        }

    if(((uBeginIdx1 + uLength1) > in_seq1_data.size()) || uLength1 == 0)
        uLength1 = in_seq1_data.size() - uBeginIdx1;

    if(((uBeginIdx2 + uLength2) > in_seq2_data.size()) || uLength2 == 0)
        uLength2 = in_seq2_data.size() - uBeginIdx2;

    // Append the strings
    out_seq_data.append(in_seq1_data.substr(uBeginIdx1,uLength1));
    out_seq_data.append(in_seq2_data.substr(uBeginIdx2,uLength2));

    return uLength1 + uLength2;
}




/////////////////////////////////////////////////////////////////////////////
//  CSeqportUtil_implementation::sm_StrAsnData  --  some very long and ugly string
//

const char* CSeqportUtil_implementation::sm_StrAsnData[] =
{
    " -- This is the set of NCBI sequence code tables\n-- J.Ostell  10/18/91\n--\n\nSeq-code-set ::= {\n codes {                              -- codes\n {                                -- IUPACna\n code iupacna ,\n num 25 ,                     -- continuous 65-89\n one-letter TRUE ,            -- all one letter codes\n start-at 65 ,                -- starts with A, ASCII 65\n table {\n { symbol \"A\", name \"Adenine\" },\n { symbol \"B\" , name \"G or T or C\" },\n { symbol \"C\", name \"Cytosine\" },\n { symbol \"D\", name \"G or A or T\" },\n                { symbol \"\", name \"\" },\n                { symbol \"\", name \"\" },\n                { symbol \"G\", name \"Guanine\" },\n                { symbol \"H\", name \"A or C or T\" } ,\n                { symbol \"\", name \"\" },\n                { symbol \"\", name \"\" },\n                { symbol \"K\", name \"G or T\" },\n                { symbol \"\", name \"\"},\n                { symbol \"M\", name \"A or C\" },\n                { symbol \"N\", name \"A or G or C or T\" } ,\n                { symbol \"\", name \"\" },\n                { symbol \"\", name \"\" },\n                { symbol \"\", name \"\"},\n                { symbol \"R\", name \"G or A\"},\n                { symbol \"S\", name \"G or C\"},\n                { symbol \"T\", name \"Thymine\"},\n                { symbol \"\", name \"\"},\n                { symbol \"V\", name \"G or C or A\"},\n                { symbol \"W\", name \"A or T\" },\n                { symbol \"\", name \"\"},\n                { symbol \"Y\", name \"T or C\"}\n            } ,                           -- end of table\n            comps {                      -- complements\n                84,\n                86,\n                71,\n                72,\n                69,\n                70,\n                67,\n                68,\n                73,\n                74,",
        "\n                77,\n                76,\n                75,\n                78,\n                79,\n                80,\n                81,\n                89,\n                87,\n                65,\n                85,\n                66,\n                83,\n                88,\n                82\n            }\n        },\n        {                                -- IUPACaa\n            code iupacaa ,\n            num 26 ,                     -- continuous 65-90\n            one-letter TRUE ,            -- all one letter codes\n            start-at 65 ,                -- starts with A, ASCII 65\n            table {\n                { symbol \"A\", name \"Alanine\" },\n                { symbol \"B\" , name \"Asp or Asn\" },\n                { symbol \"C\", name \"Cysteine\" },\n                { symbol \"D\", name \"Aspartic Acid\" },\n                { symbol \"E\", name \"Glutamic Acid\" },\n                { symbol \"F\", name \"Phenylalanine\" },\n                { symbol \"G\", name \"Glycine\" },\n                { symbol \"H\", name \"Histidine\" } ,\n                { symbol \"I\", name \"Isoleucine\" },\n                { symbol \"\", name \"\" },\n                { symbol \"K\", name \"Lysine\" },\n                { symbol \"L\", name \"Leucine\" },\n                { symbol \"M\", name \"Methionine\" },\n                { symbol \"N\", name \"Asparagine\" } ,\n                { symbol \"\", name \"\" },\n                { symbol \"P\", name \"Proline\" },\n                { symbol \"Q\", name \"Glutamine\"},\n                { symbol \"R\", name \"Arginine\"},\n                { symbol \"S\", name \"Serine\"},\n                { symbol \"T\", name \"Threonine\"},\n                { symbol \"\", name \"\"},\n                { symbol \"V\", name \"Valine\"},\n                { symbol \"W\", name \"Tryptophan\" },\n",
        "                { symbol \"X\", name \"Undetermined or atypical\"},\n                { symbol \"Y\", name \"Tyrosine\"},\n                { symbol \"Z\", name \"Glu or Gln\" }\n            }                            -- end of table   \n          },         \n     {                                -- IUPACeaa\n            code ncbieaa ,\n            num 49 ,                     -- continuous 42-90\n            one-letter TRUE ,            -- all one letter codes\n            start-at 42 ,                -- starts with *, ASCII 42\n            table {\n                { symbol \"*\", name \"Termination\" } ,\n                { symbol \"\", name \"\" } ,\n                { symbol \"\", name \"\" } ,\n                { symbol \"-\", name \"Gap\" } ,\n                { symbol \"\", name \"\" } ,\n                { symbol \"\", name \"\" } ,\n                { symbol \"\", name \"\" } ,\n                { symbol \"\", name \"\" } ,\n                { symbol \"\", name \"\" } ,\n                { symbol \"\", name \"\" } ,\n                { symbol \"\", name \"\" } ,\n                { symbol \"\", name \"\" } ,\n                { symbol \"\", name \"\" } ,\n                { symbol \"\", name \"\" } ,\n                { symbol \"\", name \"\" } ,\n                { symbol \"\", name \"\" } ,\n                { symbol \"\", name \"\" } ,\n                { symbol \"\", name \"\" } ,\n                { symbol \"\", name \"\" } ,\n                { symbol \"\", name \"\" } ,\n                { symbol \"\", name \"\" } ,\n                { symbol \"\", name \"\" } ,\n                { symbol \"\", name \"\" } ,\n                { symbol \"A\", name \"Alanine\" },\n                { symbol \"B\" , name \"Asp or Asn\" },\n                { symbol \"C\", name \"Cysteine\" },\n                { symbol \"D\", name \"Aspartic Acid\" },\n",
        "                { symbol \"E\", name \"Glutamic Acid\" },\n                { symbol \"F\", name \"Phenylalanine\" },\n                { symbol \"G\", name \"Glycine\" },\n                { symbol \"H\", name \"Histidine\" } ,\n                { symbol \"I\", name \"Isoleucine\" },\n                { symbol \"\", name \"\" },\n                { symbol \"K\", name \"Lysine\" },\n                { symbol \"L\", name \"Leucine\" },\n                { symbol \"M\", name \"Methionine\" },\n                { symbol \"N\", name \"Asparagine\" } ,\n                { symbol \"\", name \"\" },\n                { symbol \"P\", name \"Proline\" },\n                { symbol \"Q\", name \"Glutamine\"},\n                { symbol \"R\", name \"Arginine\"},\n                { symbol \"S\", name \"Serine\"},\n                { symbol \"T\", name \"Threonine\"},\n                { symbol \"U\", name \"Selenocysteine\"},\n                { symbol \"V\", name \"Valine\"},\n                { symbol \"W\", name \"Tryptophan\" },\n                { symbol \"X\", name \"Undetermined or atypical\"},\n                { symbol \"Y\", name \"Tyrosine\"},\n                { symbol \"Z\", name \"Glu or Gln\" }\n            }                            -- end of table            \n        }, \n      {                                -- IUPACaa3\n            code iupacaa3 ,\n            num 26 ,                     -- continuous 0-25\n            one-letter FALSE ,            -- all 3 letter codes\n            table {\n                { symbol \"---\", name \"Gap\" } ,\n                { symbol \"Ala\", name \"Alanine\" },\n                { symbol \"Asx\" , name \"Asp or Asn\" },\n                { symbol \"Cys\", name \"Cysteine\" },\n                { symbol \"Asp\", name \"Aspartic Acid\" },\n                { symbol \"Glu\", name \"Glutamic Acid\" },\n",
        "                { symbol \"Phe\", name \"Phenylalanine\" },\n                { symbol \"Gly\", name \"Glycine\" },\n                { symbol \"His\", name \"Histidine\" } ,\n                { symbol \"Ile\", name \"Isoleucine\" },\n                { symbol \"Lys\", name \"Lysine\" },\n                { symbol \"Leu\", name \"Leucine\" },\n                { symbol \"Met\", name \"Methionine\" },\n                { symbol \"Asn\", name \"Asparagine\" } ,\n                { symbol \"Pro\", name \"Proline\" },\n                { symbol \"Gln\", name \"Glutamine\"},\n                { symbol \"Arg\", name \"Arginine\"},\n                { symbol \"Ser\", name \"Serine\"},\n                { symbol \"Thr\", name \"Threonine\"},\n                { symbol \"Val\", name \"Valine\"},\n                { symbol \"Trp\", name \"Tryptophan\" },\n                { symbol \"Xxx\", name \"Undetermined or atypical\"},\n                { symbol \"Tyr\", name \"Tyrosine\"},\n                { symbol \"Glx\", name \"Glu or Gln\" },\n                { symbol \"Sec\", name \"Selenocysteine\"},\n                { symbol \"Ter\", name \"Termination\" } \n            }                            -- end of table            \n        }, \n        {                                -- NCBIstdaa\n            code ncbistdaa ,\n            num 26 ,                     -- continuous 0-25\n            one-letter TRUE ,            -- all one letter codes\n            table {\n                { symbol \"-\", name \"Gap\" } ,                -- 0\n                { symbol \"A\", name \"Alanine\" },             -- 1\n                { symbol \"B\" , name \"Asp or Asn\" },         -- 2\n                { symbol \"C\", name \"Cysteine\" },            -- 3\n                { symbol \"D\", name \"Aspartic Acid\" },       -- 4\n                { symbol \"E\", name \"Glutamic Acid\" },       -- 5\n",
        "                { symbol \"F\", name \"Phenylalanine\" },       -- 6\n                { symbol \"G\", name \"Glycine\" },             -- 7\n                { symbol \"H\", name \"Histidine\" } ,          -- 8\n                { symbol \"I\", name \"Isoleucine\" },          -- 9\n                { symbol \"K\", name \"Lysine\" },              -- 10\n                { symbol \"L\", name \"Leucine\" },             -- 11\n                { symbol \"M\", name \"Methionine\" },          -- 12\n                { symbol \"N\", name \"Asparagine\" } ,         -- 13\n                { symbol \"P\", name \"Proline\" },             -- 14\n                { symbol \"Q\", name \"Glutamine\"},            -- 15\n                { symbol \"R\", name \"Arginine\"},             -- 16\n                { symbol \"S\", name \"Serine\"},               -- 17\n                { symbol \"T\", name \"Threoine\"},             -- 18\n                { symbol \"V\", name \"Valine\"},               -- 19\n                { symbol \"W\", name \"Tryptophan\" },          -- 20\n                { symbol \"X\", name \"Undetermined or atypical\"},  -- 21\n                { symbol \"Y\", name \"Tyrosine\"},             -- 22\n                { symbol \"Z\", name \"Glu or Gln\" },          -- 23\n                { symbol \"U\", name \"Selenocysteine\"},       -- 24 \n                { symbol \"*\", name \"Termination\" }          -- 25\n            }                            -- end of table            \n        },\n        {                                -- NCBI2na\n            code ncbi2na ,\n            num 4 ,                     -- continuous 0-3\n            one-letter TRUE ,            -- all one letter codes\n            table {\n                { symbol \"A\", name \"Adenine\" },\n",
        "                { symbol \"C\", name \"Cytosine\" },\n                { symbol \"G\", name \"Guanine\" },\n                { symbol \"T\", name \"Thymine/Uracil\"}\n            } ,                          -- end of table            \n            comps {                      -- complements\n                3,\n                2,\n                1,\n                0\n            }\n        }, \n         {                                -- NCBI4na\n            code ncbi4na ,\n            num 16 ,                     -- continuous 0-15\n            one-letter TRUE ,            -- all one letter codes\n            table {\n                { symbol \"-\", name \"Gap\" } ,\n                { symbol \"A\", name \"Adenine\" },\n                { symbol \"C\", name \"Cytosine\" },\n                { symbol \"M\", name \"A or C\" },\n                { symbol \"G\", name \"Guanine\" },\n                { symbol \"R\", name \"G or A\"},\n                { symbol \"S\", name \"G or C\"},\n                { symbol \"V\", name \"G or C or A\"},\n                { symbol \"T\", name \"Thymine/Uracil\"},\n                { symbol \"W\", name \"A or T\" },\n                { symbol \"Y\", name \"T or C\"} ,\n                { symbol \"H\", name \"A or C or T\" } ,\n                { symbol \"K\", name \"G or T\" },\n                { symbol \"D\", name \"G or A or T\" },\n                { symbol \"B\" , name \"G or T or C\" },\n                { symbol \"N\", name \"A or G or C or T\" }\n            } ,                           -- end of table            \n            comps {                       -- complements\n",
        "                0 ,\n                8 ,\n                4 ,\n                12,\n                2 ,\n                10,\n                9 ,\n                14,\n                1 ,\n                6 ,\n                5 ,\n                13,\n                3 ,\n                11,\n                7 ,\n                15\n            }\n        } \n      },                                -- end of codes\n      maps {\n        {\n            from iupacna ,\n            to ncbi2na ,\n            num 25 ,\n            start-at 65 ,\n            table {\n                0,     -- A -> A\n                1,     -- B -> C\n                1,     -- C -> C\n                2,     -- D -> G\n                255,\n                255,\n                2,     -- G -> G\n                0,     -- H -> A\n                255,\n                255,\n                2,     -- K -> G\n                255,\n                1,     -- M -> C\n                0,     -- N -> A\n                255,\n                255,\n                255,\n                2,     -- R -> G\n                1,     -- S -> C\n                3,     -- T -> T\n                255,\n                0,     -- V -> A\n                3,     -- W -> T\n                255,\n                3 }    -- Y -> T\n        }, \n        {\n            from iupacna ,\n            to ncbi4na ,\n            num 25 ,\n            start-at 65 ,\n",
        "            table {\n                1,     -- A\n                14,    -- B\n                2,     -- C\n                13,    -- D\n                255,\n                255,\n                4,     -- G\n                11,    -- H\n                255,\n                255,\n                12,    -- K\n                255,\n                3,     -- M\n                15,    -- N\n                255,\n                255,\n                255,\n                5,     -- R\n                6,     -- S\n                8,     -- T\n                255,\n                7,     -- V\n                9,     -- W\n                255,\n                10 }   -- Y\n        }, \n        {\n            from ncbi2na ,\n            to iupacna ,\n            num 4 ,\n            table {\n                65,     -- A\n                67,     -- C\n                71,     -- G\n                84 }    -- T\n        } ,\n        {\n            from ncbi2na ,\n            to ncbi4na ,\n            num 4 ,\n            table {\n                1,     -- A\n                2,     -- C\n                4,     -- G\n                8 }    -- T\n        } , \n        {\n            from ncbi4na ,\n            to iupacna ,\n            num 16 ,\n            table {\n                78,    -- gap -> N\n                65,    -- A\n                67,    -- C\n                77,    -- M\n                71,    -- G\n                82,    -- R\n                83,    -- S\n                86,    -- V\n                84,    -- T\n                87,    -- W\n                89,    -- Y\n                72,    -- H\n                75,    -- K\n                68,    -- D\n                66,    -- B\n                78 }   -- N\n        } ,\n        {\n            from ncbi4na ,\n            to ncbi2na ,\n            num 16 ,\n            table {\n                3,    -- gap -> T\n",
        "                0,    -- A -> A\n                1,    -- C -> C\n                1,    -- M -> C\n                2,    -- G -> G\n                2,    -- R -> G\n                1,    -- S -> C\n                0,    -- V -> A\n                3,    -- T -> T\n                3,    -- W -> T\n                3,    -- Y -> T\n                0,    -- H -> A\n                2,    -- K -> G\n                2,    -- D -> G\n                1,    -- B -> C\n                0 }   -- N -> A\n        }  ,\n        {\n            from iupacaa ,\n            to ncbieaa ,\n            num 26 ,\n            start-at 65 ,\n            table {\n                65 ,    -- they map directly\n                66 ,\n                67 ,\n                68,\n                69,\n                70,\n                71,\n                72,\n                73,\n                255,  -- J\n                75,\n                76,\n                77,\n                78,\n                255,  -- O\n                80,\n                81,\n                82,\n                83,\n                84,\n                255,  -- U\n                86,\n                87,\n                88,\n                89,\n                90 }\n        }  ,\n         {\n            from ncbieaa ,\n            to iupacaa ,\n            num 49 ,\n            start-at 42 ,\n            table {\n                88 ,   -- termination -> X\n                255,\n                255,\n                88,    -- Gap -> X\n                255,\n                255,\n                255,\n                255,\n                255,\n                255,\n                255,\n                255,\n                255,\n                255,\n                255,\n                255,\n                255,\n                255,\n                255,\n                255,\n                255,\n",
        "                255,\n                255,\n                65 ,    -- from here they map directly\n                66 ,\n                67 ,\n                68,\n                69,\n                70,\n                71,\n                72,\n                73,\n                255,  -- J\n                75,\n                76,\n                77,\n                78,\n                255,  -- O\n                80,\n                81,\n                82,\n                83,\n                84,\n                67,  -- U -> C\n                86,\n                87,\n                88,\n                89,\n                90 }\n        } ,\n        {\n            from iupacaa ,\n            to ncbistdaa ,\n            num 26 ,\n            start-at 65 ,\n            table {\n                1 ,    -- they map directly\n                2 ,\n                3 ,\n                4,\n                5,\n                6,\n                7,\n                8,\n                9,\n                255,  -- J\n                10,\n                11,\n                12,\n                13,\n                255,  -- O\n                14,\n                15,\n                16,\n                17,\n                18,\n                255,  -- U\n                19,\n                20,\n                21,\n				22,\n                23 }\n        } ,\n        {\n            from ncbieaa ,\n            to ncbistdaa ,\n            num 49 ,\n            start-at 42 ,\n            table {\n                25,   -- termination\n                255,\n                255,\n                0,    -- Gap\n                255,\n                255,\n                255,\n                255,\n                255,\n                255,\n                255,\n                255,\n                255,\n                255,\n                255,\n",
        "                255,\n                255,\n                255,\n                255,\n                255,\n                255,\n                255,\n                255,\n                1 ,    -- they map directly\n                2 ,\n                3 ,\n                4,\n                5,\n                6,\n                7,\n                8,\n                9,\n                255,  -- J\n                10,\n                11,\n                12,\n                13,\n                255,  -- O\n                14,\n                15,\n                16,\n                17,\n                18,\n                24,  -- U\n                19,\n                20,\n                21,\n                22,\n                23 }\n        }  ,\n        {\n            from ncbistdaa ,\n            to ncbieaa ,\n            num 26 ,\n            table {\n                45 ,  --   \"-\"\n                65 ,    -- they map directly with holes for O and J\n                66 ,\n                67 ,\n                68,\n                69,\n                70,\n                71,\n                72,\n                73,\n                75,\n                76,\n                77,\n                78,\n                80,\n                81,\n                82,\n                83,\n                84,\n                86,\n                87,\n                88,\n                89,\n                90,\n                85,	 -- U\n                42}  -- *\n        } ,\n        {\n            from ncbistdaa ,\n            to iupacaa ,\n            num 26 ,\n            table {\n                255 ,  --   \"-\"\n                65 ,    -- they map directly with holes for O and J\n                66 ,\n                67 ,\n                68,\n                69,\n                70,\n                71,\n",
        "                72,\n                73,\n                75,\n                76,\n                77,\n                78,\n                80,\n                81,\n                82,\n                83,\n                84,\n                86,\n                87,\n                88,\n                89,\n                90,\n                255,  -- U\n                255}  -- *\n        } \n \n      }                                  -- end of maps\n}                                        -- end of seq-code-set\n\n\n",
        0  // to indicate that there is no more data
};


END_objects_SCOPE
END_NCBI_SCOPE
