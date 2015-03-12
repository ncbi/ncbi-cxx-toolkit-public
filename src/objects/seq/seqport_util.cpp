 /*$Id$
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
 *          (also reviewed/fixed/groomed by Denis Vakatov and Aaron Ucko)
 *
 * File Description:
 */  

#include <ncbi_pch.hpp>
#include <objects/seq/seqport_util.hpp>

#include <serial/serial.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>

#include <objects/seq/NCBI2na.hpp>
#include <objects/seq/NCBI4na.hpp>
#include <objects/seq/NCBI8na.hpp>
#include <objects/seq/NCBI8aa.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/IUPACaa.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/NCBIstdaa.hpp>
#include <objects/seq/NCBIpaa.hpp>

#include <objects/seqcode/Seq_code_set.hpp>
#include <objects/seqcode/Seq_code_table.hpp>
#include <objects/seqcode/Seq_code_type.hpp>
#include <objects/seqcode/Seq_map_table.hpp>

#include <util/sequtil/sequtil.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include <util/sequtil/sequtil_manip.hpp>
#include <util/random_gen.hpp>
#include <corelib/ncbi_safe_static.hpp>

#include <algorithm>
#include <string.h>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

static const bool kSymbol = true;
static const bool kName = false;
static const unsigned int kNumCodes = 11;

static inline ESeq_code_type EChoiceToESeq (CSeq_data::E_Choice from_type)
{
    switch (from_type) {
    case CSeq_data::e_Iupacaa:
        return eSeq_code_type_iupacaa;
    case CSeq_data::e_Ncbi2na:
        return eSeq_code_type_ncbi2na;
    case CSeq_data::e_Ncbi4na:
        return eSeq_code_type_ncbi4na;
    case CSeq_data::e_Iupacna:
        return eSeq_code_type_iupacna;
    case CSeq_data::e_Ncbieaa:
        return eSeq_code_type_ncbieaa;
    case CSeq_data::e_Ncbistdaa:
        return eSeq_code_type_ncbistdaa;
    case CSeq_data::e_Ncbi8na:
        return eSeq_code_type_ncbi8na;
    case CSeq_data::e_Ncbipna:
        return eSeq_code_type_ncbipna;
    case CSeq_data::e_Ncbi8aa:
        return eSeq_code_type_ncbi8aa;
    case CSeq_data::e_Ncbipaa:
        return eSeq_code_type_ncbipaa;
    default:
        throw CSeqportUtil::CBadType("EChoiceToESeq");
    }
}    

// CSeqportUtil_implementation is a singleton.

class CSeqportUtil_implementation {
public:
    CSeqportUtil_implementation();
    ~CSeqportUtil_implementation();
    
    typedef CSeqportUtil::TIndex TIndex;
    typedef CSeqportUtil::TPair  TPair;

    TSeqPos Convert
    (const CSeq_data&      in_seq,
     CSeq_data*            out_seq,
     CSeq_data::E_Choice   to_code,
     TSeqPos               uBeginIdx,
     TSeqPos               uLength,
     bool                  bAmbig,
     CRandom::TValue       seed,
     TSeqPos               total_length = 0,
     TSeqPos*              out_seq_length = 0,
     vector<Uint4>*        blast_ambig = 0)
        const;

    TSeqPos Pack
    (CSeq_data*   in_seq,
     TSeqPos      uLength)
        const;

    bool FastValidate
    (const CSeq_data&   in_seq,
     TSeqPos            uBeginIdx,
     TSeqPos            uLength)
        const;

    void Validate
    (const CSeq_data&   in_seq,
     vector<TSeqPos>*   badIdx,
     TSeqPos            uBeginIdx,
     TSeqPos            uLength)
        const;

    TSeqPos GetAmbigs
    (const CSeq_data&      in_seq,
     CSeq_data*            out_seq,
     vector<TSeqPos>*      out_indices,
     CSeq_data::E_Choice   to_code,
     TSeqPos               uBeginIdx,
     TSeqPos               uLength)
        const;

    TSeqPos GetCopy
    (const CSeq_data&   in_seq,
     CSeq_data*         out_seq,
     TSeqPos            uBeginIdx,
     TSeqPos            uLength)
        const;

    TSeqPos Keep
    (CSeq_data*   in_seq,
     TSeqPos      uBeginIdx,
     TSeqPos      uLength)
        const;

    TSeqPos Append
    (CSeq_data*         out_seq,
     const CSeq_data&   in_seq1,
     TSeqPos            uBeginIdx1,
     TSeqPos            uLength1,
     const CSeq_data&   in_seq2,
     TSeqPos            uBeginIdx2,
     TSeqPos            uLength2)
        const;

    TSeqPos Complement
    (CSeq_data*   in_seq,
     TSeqPos      uBeginIdx,
     TSeqPos      uLength)
        const;

    TSeqPos Complement
    (const CSeq_data&   in_seq,
     CSeq_data*         out_seq,
     TSeqPos            uBeginIdx,
     TSeqPos            uLength)
        const;

    TSeqPos Reverse
    (CSeq_data*   in_seq,
     TSeqPos      uBeginIdx,
     TSeqPos      uLength)
        const;

    TSeqPos Reverse
    (const CSeq_data&  in_seq,
     CSeq_data*        out_seq,
     TSeqPos           uBeginIdx,
     TSeqPos           uLength)
        const;

    TSeqPos ReverseComplement
    (CSeq_data*   in_seq,
     TSeqPos      uBeginIdx,
     TSeqPos      uLength)
        const;

    TSeqPos ReverseComplement
    (const CSeq_data&   in_seq,
     CSeq_data*         out_seq,
     TSeqPos            uBeginIdx,
     TSeqPos            uLength)
        const;
        
    const string& GetIupacaa3(TIndex ncbistdaa);
    
    bool IsCodeAvailable(CSeq_data::E_Choice code_type);
    
    bool IsCodeAvailable(ESeq_code_type code_type);
    
    TPair GetCodeIndexFromTo(CSeq_data::E_Choice code_type);
    
    TPair GetCodeIndexFromTo(ESeq_code_type code_type);
    
    const string& GetCodeOrName(CSeq_data::E_Choice code_type, 
                                TIndex              idx,
                                bool                get_code); 
                 
    const string& GetCodeOrName(ESeq_code_type code_type, 
                                TIndex         idx,
                                bool           get_code); 
                 
    TIndex GetIndex(CSeq_data::E_Choice code_type,
                    const string&       code);

    TIndex GetIndex(ESeq_code_type code_type,
                    const string&       code);
                  
    TIndex GetIndexComplement(CSeq_data::E_Choice code_type,
                              TIndex              idx);
                           
    TIndex GetIndexComplement(ESeq_code_type code_type,
                              TIndex         idx);

    TIndex GetMapToIndex(CSeq_data::E_Choice from_type,
                         CSeq_data::E_Choice to_type,
                         TIndex              from_idx);

    TIndex GetMapToIndex(ESeq_code_type from_type,
                         ESeq_code_type to_type,
                         TIndex         from_idx);
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

private:
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
    

    TSeqPos x_ConvertAmbig
    (const CSeq_data&      in_seq,
     CSeq_data*            out_seq,
     CSeq_data::E_Choice   to_code,
     TSeqPos               uBeginIdx,
     TSeqPos               uLength,
     CRandom::TValue       seed,
     TSeqPos               total_length = 0,
     TSeqPos*              out_seq_length = 0,
     vector<Uint4>*        blast_ambig = 0)
        const;

    // Helper function to initialize map tables
    CRef<CMap_table> InitMaps(ESeq_code_type from_type,
                              ESeq_code_type to_type);

    // Member variables holding fast conversion tables

    // Takes a byte as an index and returns a unsigned int with
    // 4 characters, each character being one of ATGC
    //CRef<CFast_table4> m_FastNcbi2naIupacna;

    // Takes a byte (each byte with 4 Ncbi2na codes) as an index and
    // returns a Unit2 with 2 bytes, each byte formated as 2 Ncbi4na codes
    //CRef<CFast_table2> m_FastNcbi2naNcbi4na;

    // Takes a byte (each byte with 2 Ncbi4na codes) as an index and
    // returns a 2 byte string, each byte with an Iupacna code.
    //CRef<CFast_table2> m_FastNcbi4naIupacna;

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
    
    // Tables used to convert an index for a code type to a symbol or name
    // for the same code type
    vector<vector<string> > m_IndexString[2];
    vector<vector<TIndex> > m_IndexComplement;
    vector<map<string, TIndex> > m_StringIndex;
    vector<TIndex> m_StartAt;
        
    // Helper function to initialize fast conversion tables
    //CRef<CFast_table4> InitFastNcbi2naIupacna();
    CRef<CFast_table2> InitFastNcbi2naNcbi4na();
    CRef<CFast_table2> InitFastNcbi4naIupacna();
    CRef<CFast_4_1>    InitFastIupacnaNcbi2na();
    CRef<CFast_2_1>    InitFastIupacnaNcbi4na();
    CRef<CFast_2_1>    InitFastNcbi4naNcbi2na();
    
    // Helper functions to initialize Index to/from code/name conversion tables
    // and complement tables 
    void               InitIndexCodeName();
    
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

    /*
    // Fuction to convert ncbi2na (1 byte) to iupacna (4 bytes)
    TSeqPos MapNcbi2naToIupacna(const CSeq_data&  in_seq,
                                CSeq_data*        out_seq,
                                TSeqPos           uBeginIdx,
                                TSeqPos           uLength)
        const;

    // Function to convert ncbi2na (1 byte) to ncbi4na (2 bytes)
    TSeqPos MapNcbi2naToNcbi4na(const CSeq_data&  in_seq,
                                CSeq_data*        out_seq,
                                TSeqPos           uBeginIdx,
                                TSeqPos           uLength)
        const;

    // Function to convert ncbi4na (1 byte) to iupacna (2 bytes)
    TSeqPos MapNcbi4naToIupacna(const CSeq_data& in_seq,
                                CSeq_data*       out_seq,
                                TSeqPos          uBeginIdx,
                                TSeqPos          uLength)
        const;
    */
    // Function to convert iupacna (4 bytes) to ncbi2na (1 byte)
    TSeqPos MapIupacnaToNcbi2na(const CSeq_data& in_seq,
                                CSeq_data*       out_seq,
                                TSeqPos          uBeginIdx,
                                TSeqPos          uLength,
                                bool             bAmbig,
                                CRandom::TValue  seed,
                                TSeqPos          total_length,
                                TSeqPos*         out_seq_length, 
                                vector<Uint4>*   blast_ambig)
        const;
    /*

    // Function to convert iupacna (2 bytes) to ncbi4na (1 byte)
    TSeqPos MapIupacnaToNcbi4na(const CSeq_data& in_seq,
                                CSeq_data*       out_seq,
                                TSeqPos          uBeginIdx,
                                TSeqPos          uLength)
        const;
    */
    // Function to convert ncbi4na (2 bytes) to ncbi2na (1 byte)
    TSeqPos MapNcbi4naToNcbi2na(const CSeq_data& in_seq,
                                CSeq_data*      out_seq,
                                TSeqPos         uBeginIdx,
                                TSeqPos         uLength,
                                bool            bAmbig,
                                CRandom::TValue seed,
                                TSeqPos         total_length,
                                TSeqPos*        out_seq_length,
                                vector<Uint4>*  blast_ambig)
        const;
    /*

    // Function to convert iupacaa (byte) to ncbieaa (byte)
    TSeqPos MapIupacaaToNcbieaa(const CSeq_data& in_seq,
                                CSeq_data*       out_seq,
                                TSeqPos          uBeginIdx,
                                TSeqPos          uLength) const;

    // Function to convert ncbieaa (byte) to iupacaa (byte)
    TSeqPos MapNcbieaaToIupacaa(const CSeq_data& in_seq,
                                CSeq_data*       out_seq,
                                TSeqPos          uBeginIdx,
                                TSeqPos          uLength)
        const;

    // Function to convert iupacaa (byte) to ncbistdaa (byte)
    TSeqPos MapIupacaaToNcbistdaa(const CSeq_data& in_seq,
                                  CSeq_data*       out_seq,
                                  TSeqPos          uBeginIdx,
                                  TSeqPos          uLength)
        const;

    // Function to convert ncbieaa (byte) to ncbistdaa (byte)
    TSeqPos MapNcbieaaToNcbistdaa(const CSeq_data& in_seq,
                                  CSeq_data*       out_seq,
                                  TSeqPos          uBeginIdx,
                                  TSeqPos          uLength)
        const;

    // Function to convert ncbistdaa (byte) to ncbieaa (byte)
    TSeqPos MapNcbistdaaToNcbieaa(const CSeq_data& in_seq,
                                  CSeq_data*       out_seq,
                                  TSeqPos          uBeginIdx,
                                  TSeqPos          uLength)
        const;

    // Function to convert ncbistdaa (byte) to iupacaa (byte)
    TSeqPos MapNcbistdaaToIupacaa(const CSeq_data& in_seq,
                                  CSeq_data*       out_seq,
                                  TSeqPos          uBeginIdx,
                                  TSeqPos          uLength)
        const;
        */

    // Fast Validation functions
    bool FastValidateIupacna(const CSeq_data& in_seq,
                             TSeqPos          uBeginIdx,
                             TSeqPos          uLength)
        const;

    bool FastValidateNcbieaa(const CSeq_data& in_seq,
                             TSeqPos          uBeginIdx,
                             TSeqPos          uLength)
        const;


    bool FastValidateNcbistdaa(const CSeq_data& in_seq,
                               TSeqPos          uBeginIdx,
                               TSeqPos          uLength)
        const;


    bool FastValidateIupacaa(const CSeq_data& in_seq,
                             TSeqPos          uBeginIdx,
                             TSeqPos          uLength)
        const;

    // Full Validation functions
    void ValidateIupacna(const CSeq_data&       in_seq,
                         vector<TSeqPos>*       badIdx,
                         TSeqPos                uBeginIdx,
                         TSeqPos                uLength)
        const;

    void ValidateNcbieaa(const CSeq_data&       in_seq,
                         vector<TSeqPos>*       badIdx,
                         TSeqPos                uBeginIdx,
                         TSeqPos                uLength)
        const;

    void ValidateNcbistdaa(const CSeq_data&       in_seq,
                           vector<TSeqPos>*       badIdx,
                           TSeqPos                uBeginIdx,
                           TSeqPos                uLength)
        const;

    void ValidateIupacaa(const CSeq_data&       in_seq,
                         vector<TSeqPos>*       badIdx,
                         TSeqPos                uBeginIdx,
                         TSeqPos                uLength)
        const;

    // Functions to make copies of the different types of sequences
    TSeqPos GetNcbi2naCopy(const CSeq_data& in_seq,
                           CSeq_data*       out_seq,
                           TSeqPos          uBeginIdx,
                           TSeqPos          uLength)
        const;

    TSeqPos GetNcbi4naCopy(const CSeq_data& in_seq,
                           CSeq_data*       out_seq,
                           TSeqPos          uBeginIdx,
                           TSeqPos          uLength)
        const;

    TSeqPos GetIupacnaCopy(const CSeq_data& in_seq,
                           CSeq_data*       out_seq,
                           TSeqPos          uBeginIdx,
                           TSeqPos          uLength)
        const;

    TSeqPos GetNcbieaaCopy(const CSeq_data& in_seq,
                           CSeq_data*       out_seq,
                           TSeqPos          uBeginIdx,
                           TSeqPos          uLength)
        const;

    TSeqPos GetNcbistdaaCopy(const CSeq_data& in_seq,
                             CSeq_data*       out_seq,
                             TSeqPos          uBeginIdx,
                             TSeqPos          uLength)
        const;

    TSeqPos GetIupacaaCopy(const CSeq_data& in_seq,
                           CSeq_data*       out_seq,
                           TSeqPos          uBeginIdx,
                           TSeqPos          uLength)
        const;

    // Function to adjust uBeginIdx to lie on an in_seq byte boundary
    // and uLength to lie on on an out_seq byte boundary. Returns
    // overhang, the number of out seqs beyond byte boundary determined
    // by uBeginIdx + uLength
    TSeqPos Adjust(TSeqPos* uBeginIdx,
                   TSeqPos* uLength,
                   TSeqPos  uInSeqBytes,
                   TSeqPos  uInSeqsPerByte,
                   TSeqPos  uOutSeqsPerByte)
        const;

    // GetAmbig methods

    // Loops through an ncbi4na input sequence and determines
    // the ambiguities that would result from conversion to an ncbi2na sequence
    // On return, out_seq contains the ncbi4na bases that become ambiguous and
    // out_indices contains the indices of the abiguous bases in in_seq
    TSeqPos GetAmbigs_ncbi4na_ncbi2na(const CSeq_data&  in_seq,
                                      CSeq_data*        out_seq,
                                      vector<TSeqPos>*  out_indices,
                                      TSeqPos           uBeginIdx,
                                      TSeqPos           uLength)
        const;

    // Loops through an iupacna input sequence and determines
    // the ambiguities that would result from conversion to an ncbi2na sequence
    // On return, out_seq contains the iupacna bases that become ambiguous and
    // out_indices contains the indices of the abiguous bases in in_seq. The
    // return is the number of ambiguities found.
    TSeqPos GetAmbigs_iupacna_ncbi2na(const CSeq_data&  in_seq,
                                      CSeq_data*        out_seq,
                                      vector<TSeqPos>*  out_indices,
                                      TSeqPos           uBeginIdx,
                                      TSeqPos           uLength)
        const;

    // Methods to perform Keep on specific seq types. Methods
    // return length of kept sequence.
    TSeqPos KeepNcbi2na(CSeq_data*  in_seq,
                        TSeqPos     uBeginIdx,
                        TSeqPos     uLength)
        const;

    TSeqPos KeepNcbi4na(CSeq_data*  in_seq,
                        TSeqPos     uBeginIdx,
                        TSeqPos     uLength)
        const;

    TSeqPos KeepIupacna(CSeq_data*  in_seq,
                        TSeqPos     uBeginIdx,
                        TSeqPos     uLength)
        const;

    TSeqPos KeepNcbieaa(CSeq_data*  in_seq,
                        TSeqPos     uBeginIdx,
                        TSeqPos     uLength)
        const;

    TSeqPos KeepNcbistdaa(CSeq_data*  in_seq,
                          TSeqPos     uBeginIdx,
                          TSeqPos     uLength)
        const;

    TSeqPos KeepIupacaa(CSeq_data*  in_seq,
                        TSeqPos     uBeginIdx,
                        TSeqPos     uLength)
        const;

    // Methods to complement na sequences

    // In place methods. Return number of complemented residues.
    TSeqPos ComplementIupacna(CSeq_data*  in_seq,
                              TSeqPos     uBeginIdx,
                              TSeqPos    uLength)
        const;

    TSeqPos ComplementNcbi2na(CSeq_data*  in_seq,
                              TSeqPos     uBeginIdx,
                              TSeqPos     uLength)
        const;

    TSeqPos ComplementNcbi4na(CSeq_data*  in_seq,
                              TSeqPos     uBeginIdx,
                              TSeqPos     uLength)
        const;


    // Complement in copy methods
    TSeqPos ComplementIupacna(const CSeq_data&  in_seq,
                              CSeq_data*        out_seq,
                              TSeqPos           uBeginIdx,
                              TSeqPos           uLength)
        const;

    TSeqPos ComplementNcbi2na(const CSeq_data&  in_seq,
                              CSeq_data*        out_seq,
                              TSeqPos           uBeginIdx,
                              TSeqPos           uLength)
        const;

    TSeqPos ComplementNcbi4na(const CSeq_data&  in_seq,
                              CSeq_data*        out_seq,
                              TSeqPos           uBeginIdx,
                              TSeqPos           uLength)
        const;


    // Methods to reverse na sequences

    // In place methods
    TSeqPos ReverseIupacna(CSeq_data*  in_seq,
                           TSeqPos     uBeginIdx,
                           TSeqPos     uLength)
        const;

    TSeqPos ReverseNcbi2na(CSeq_data*  in_seq,
                           TSeqPos     uBeginIdx,
                           TSeqPos     uLength)
        const;

    TSeqPos ReverseNcbi4na(CSeq_data*  in_seq,
                           TSeqPos     uBeginIdx,
                           TSeqPos     uLength)
        const;

    // Reverse in copy methods
    TSeqPos ReverseIupacna(const CSeq_data&  in_seq,
                           CSeq_data*        out_seq,
                           TSeqPos           uBeginIdx,
                           TSeqPos           uLength)
        const;

    TSeqPos ReverseNcbi2na(const CSeq_data&  in_seq,
                           CSeq_data*        out_seq,
                           TSeqPos           uBeginIdx,
                           TSeqPos           uLength)
        const;

    TSeqPos ReverseNcbi4na(const CSeq_data&  in_seq,
                           CSeq_data*        out_seq,
                           TSeqPos           uBeginIdx,
                           TSeqPos           uLength)
        const;
 
    // Methods to reverse-complement an na sequences

    // In place methods
    TSeqPos ReverseComplementIupacna(CSeq_data*  in_seq,
                                     TSeqPos     uBeginIdx,
                                     TSeqPos     uLength)
        const;

    TSeqPos ReverseComplementNcbi2na(CSeq_data*  in_seq,
                                     TSeqPos     uBeginIdx,
                                     TSeqPos     uLength)
        const;

    TSeqPos ReverseComplementNcbi4na(CSeq_data*  in_seq,
                                     TSeqPos     uBeginIdx,
                                     TSeqPos     uLength)
        const;

    // Reverse in copy methods
    TSeqPos ReverseComplementIupacna(const CSeq_data& in_seq,
                                     CSeq_data*       out_seq,
                                     TSeqPos          uBeginIdx,
                                     TSeqPos          uLength)
        const;

    TSeqPos ReverseComplementNcbi2na(const CSeq_data& in_seq,
                                     CSeq_data*       out_seq,
                                     TSeqPos          uBeginIdx,
                                     TSeqPos          uLength)
        const;

    TSeqPos ReverseComplementNcbi4na(const CSeq_data& in_seq,
                                     CSeq_data*       out_seq,
                                     TSeqPos          uBeginIdx,
                                     TSeqPos          uLength)
        const;

    // Append methods
    TSeqPos AppendIupacna(CSeq_data*        out_seq,
                          const CSeq_data&  in_seq1,
                          TSeqPos           uBeginIdx1,
                          TSeqPos           uLength1,
                          const CSeq_data&  in_seq2,
                          TSeqPos           uBeginIdx2,
                          TSeqPos           uLength2)
        const;

    TSeqPos AppendNcbi2na(CSeq_data*          out_seq,
                          const CSeq_data&  in_seq1,
                          TSeqPos           uBeginIdx1,
                          TSeqPos           uLength1,
                          const CSeq_data&  in_seq2,
                          TSeqPos           uBeginIdx2,
                          TSeqPos           uLength2)
        const;

    TSeqPos AppendNcbi4na(CSeq_data*          out_seq,
                          const CSeq_data&  in_seq1,
                          TSeqPos           uBeginIdx1,
                          TSeqPos           uLength1,
                          const CSeq_data&  in_seq2,
                          TSeqPos           uBeginIdx2,
                          TSeqPos           uLength2)
        const;

    TSeqPos AppendNcbieaa(CSeq_data*          out_seq,
                          const CSeq_data&  in_seq1,
                          TSeqPos           uBeginIdx1,
                          TSeqPos           uLength1,
                          const CSeq_data&  in_seq2,
                          TSeqPos           uBeginIdx2,
                          TSeqPos           uLength2)
        const;

    TSeqPos AppendNcbistdaa(CSeq_data*        out_seq,
                          const CSeq_data&  in_seq1,
                          TSeqPos           uBeginIdx1,
                          TSeqPos           uLength1,
                          const CSeq_data&  in_seq2,
                          TSeqPos           uBeginIdx2,
                          TSeqPos           uLength2)
        const;

    TSeqPos AppendIupacaa(CSeq_data*          out_seq,
                          const CSeq_data&  in_seq1,
                          TSeqPos           uBeginIdx1,
                          TSeqPos           uLength1,
                          const CSeq_data&  in_seq2,
                          TSeqPos           uBeginIdx2,
                          TSeqPos           uLength2)
        const;

    void x_GetSeqFromSeqData(const CSeq_data& data, 
                             const string** str,
                             const vector<char>** vec)
        const;
    void x_GetSeqFromSeqData(CSeq_data& data, 
                             string** str,
                             vector<char>** vec)
        const;
};


static CSafeStatic<CSeqportUtil_implementation> sx_Implementation;

CSeqportUtil_implementation& CSeqportUtil::x_GetImplementation(void)
{
    return *sx_Implementation;
}




/////////////////////////////////////////////////////////////////////////////
//  PUBLIC (static wrappers to CSeqportUtil_implementation public methods)::
//


TSeqPos CSeqportUtil::Convert
(const CSeq_data&     in_seq,
 CSeq_data*           out_seq,
 CSeq_data::E_Choice  to_code,
 TSeqPos              uBeginIdx,
 TSeqPos              uLength,
 bool                 bAmbig,
 CRandom::TValue      seed)
{
    return x_GetImplementation().Convert
        (in_seq, out_seq, to_code, uBeginIdx, uLength, bAmbig, seed, 
         0, 0, 0);
}

TSeqPos CSeqportUtil::ConvertWithBlastAmbig
(const CSeq_data&     in_seq,
 CSeq_data*           out_seq,
 TSeqPos              uBeginIdx,
 TSeqPos              uLength,
 TSeqPos              total_length,
 TSeqPos*             out_seq_length,
 vector<Uint4>*       blast_ambig)
{
    return x_GetImplementation().Convert
        (in_seq, out_seq, CSeq_data::e_Ncbi2na, uBeginIdx, uLength, true,
         17734276, total_length, out_seq_length, blast_ambig);
}

TSeqPos CSeqportUtil::Pack
(CSeq_data*   in_seq,
 TSeqPos uLength)
{
    return x_GetImplementation().Pack
        (in_seq, uLength);
}


bool CSeqportUtil::FastValidate
(const CSeq_data&   in_seq,
 TSeqPos            uBeginIdx,
 TSeqPos            uLength)
{
    return x_GetImplementation().FastValidate
        (in_seq, uBeginIdx, uLength);
}


void CSeqportUtil::Validate
(const CSeq_data&   in_seq,
 vector<TSeqPos>*   badIdx,
 TSeqPos            uBeginIdx,
 TSeqPos            uLength)
{
    x_GetImplementation().Validate
        (in_seq, badIdx, uBeginIdx, uLength);
}


TSeqPos CSeqportUtil::GetAmbigs
(const CSeq_data&     in_seq,
 CSeq_data*           out_seq,
 vector<TSeqPos>*     out_indices,
 CSeq_data::E_Choice  to_code,
 TSeqPos              uBeginIdx,
 TSeqPos              uLength)
{
    return x_GetImplementation().GetAmbigs
        (in_seq, out_seq, out_indices, to_code, uBeginIdx, uLength);
}


TSeqPos CSeqportUtil::GetCopy
(const CSeq_data&   in_seq,
 CSeq_data*         out_seq,
 TSeqPos            uBeginIdx,
 TSeqPos            uLength)
{
    return x_GetImplementation().GetCopy
        (in_seq, out_seq, uBeginIdx, uLength);
}



TSeqPos CSeqportUtil::Keep
(CSeq_data*   in_seq,
 TSeqPos      uBeginIdx,
 TSeqPos      uLength)
{
    return x_GetImplementation().Keep
        (in_seq, uBeginIdx, uLength);
}


TSeqPos CSeqportUtil::Append
(CSeq_data*         out_seq,
 const CSeq_data&   in_seq1,
 TSeqPos            uBeginIdx1,
 TSeqPos            uLength1,
 const CSeq_data&   in_seq2,
 TSeqPos            uBeginIdx2,
 TSeqPos            uLength2)
{
    return x_GetImplementation().Append
        (out_seq,
         in_seq1, uBeginIdx1, uLength1, in_seq2, uBeginIdx2, uLength2);
}


TSeqPos CSeqportUtil::Complement
(CSeq_data*   in_seq,
 TSeqPos      uBeginIdx,
 TSeqPos      uLength)
{
    return x_GetImplementation().Complement
        (in_seq, uBeginIdx, uLength);
}


TSeqPos CSeqportUtil::Complement
(const CSeq_data&   in_seq,
 CSeq_data*         out_seq,
 TSeqPos            uBeginIdx,
 TSeqPos            uLength)
{
    return x_GetImplementation().Complement
        (in_seq, out_seq, uBeginIdx, uLength);
}


TSeqPos CSeqportUtil::Reverse
(CSeq_data*   in_seq,
 TSeqPos      uBeginIdx,
 TSeqPos      uLength)
{
    return x_GetImplementation().Reverse
        (in_seq, uBeginIdx, uLength);
}


TSeqPos CSeqportUtil::Reverse
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
{
    return x_GetImplementation().Reverse
        (in_seq, out_seq, uBeginIdx, uLength);
}


TSeqPos CSeqportUtil::ReverseComplement
(CSeq_data*  in_seq,
 TSeqPos     uBeginIdx,
 TSeqPos     uLength)
{
    return x_GetImplementation().ReverseComplement
        (in_seq, uBeginIdx, uLength);
}


TSeqPos CSeqportUtil::ReverseComplement
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
{
    return x_GetImplementation().ReverseComplement
        (in_seq, out_seq, uBeginIdx, uLength);
}


const string& CSeqportUtil::GetIupacaa3(TIndex ncbistdaa)
{
    return x_GetImplementation().GetIupacaa3(ncbistdaa);
}

bool CSeqportUtil::IsCodeAvailable(CSeq_data::E_Choice code_type)
{
    return x_GetImplementation().IsCodeAvailable(code_type);
}

bool CSeqportUtil::IsCodeAvailable(ESeq_code_type code_type)
{
    return x_GetImplementation().IsCodeAvailable(code_type);
}

CSeqportUtil::TPair CSeqportUtil::GetCodeIndexFromTo
(CSeq_data::E_Choice code_type)
{
    return x_GetImplementation().GetCodeIndexFromTo(code_type);
}

CSeqportUtil::TPair CSeqportUtil::GetCodeIndexFromTo
(ESeq_code_type code_type)
{
    return x_GetImplementation().GetCodeIndexFromTo(code_type);
}

const string& CSeqportUtil::GetCode
(CSeq_data::E_Choice code_type, 
 TIndex              idx) 
{
    return x_GetImplementation().GetCodeOrName(code_type, idx, true);
}

const string& CSeqportUtil::GetCode
(ESeq_code_type code_type, 
 TIndex         idx) 
{
    return x_GetImplementation().GetCodeOrName(code_type, idx, true);
}

const string& CSeqportUtil::GetName
(CSeq_data::E_Choice code_type, 
 TIndex              idx) 
{
    return x_GetImplementation().GetCodeOrName(code_type, idx, false);
}

const string& CSeqportUtil::GetName
(ESeq_code_type code_type, 
 TIndex         idx) 
{
    return x_GetImplementation().GetCodeOrName(code_type, idx, false);
}

CSeqportUtil::TIndex CSeqportUtil::GetIndex
(CSeq_data::E_Choice code_type,
 const string&       code)
{
    return x_GetImplementation().GetIndex(code_type, code);
}

CSeqportUtil::TIndex CSeqportUtil::GetIndex
(ESeq_code_type code_type,
 const string&  code)
{
    return x_GetImplementation().GetIndex(code_type, code);
}

CSeqportUtil::TIndex CSeqportUtil::GetIndexComplement
(CSeq_data::E_Choice code_type,
 TIndex        idx)
{
    return x_GetImplementation().GetIndexComplement(code_type, idx);
}

CSeqportUtil::TIndex CSeqportUtil::GetIndexComplement
(ESeq_code_type code_type,
 TIndex         idx)
{
    return x_GetImplementation().GetIndexComplement(code_type, idx);
}

CSeqportUtil::TIndex CSeqportUtil::GetMapToIndex
(CSeq_data::E_Choice from_type,
 CSeq_data::E_Choice to_type,
 TIndex              from_idx)
{
    return x_GetImplementation().GetMapToIndex(from_type, to_type, from_idx);
}

CSeqportUtil::TIndex CSeqportUtil::GetMapToIndex
(ESeq_code_type from_type,
 ESeq_code_type to_type,
 TIndex         from_idx)
{
    return x_GetImplementation().GetMapToIndex(from_type, to_type, from_idx);
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
    //m_FastNcbi2naIupacna = InitFastNcbi2naIupacna();
    //m_FastNcbi2naNcbi4na = InitFastNcbi2naNcbi4na();
    //m_FastNcbi4naIupacna = InitFastNcbi4naIupacna();
    m_FastIupacnaNcbi2na = InitFastIupacnaNcbi2na();
    m_FastIupacnaNcbi4na = InitFastIupacnaNcbi4na();
    m_FastNcbi4naNcbi2na = InitFastNcbi4naNcbi2na();
    
    // Initialize tables for conversion of index to codes or names
    InitIndexCodeName();

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
    CNcbiIstrstream is(str.c_str(), str.length());

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
    SIZE_TYPE size = table_data.size();
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
CRef<CSeqportUtil_implementation::CMap_table> 
CSeqportUtil_implementation::InitMaps
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
    SIZE_TYPE size = table_data.size();
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
/*
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
    return fastTable;
}
*/

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
                    char *pt = 

                        reinterpret_cast<char*>(&fastTable->m_Table[aByte]);
                    *(pt++) = (chi << 4) | chj;
                    *pt = (chk << 4) | chl;
                }
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
            char *pt = reinterpret_cast<char*>(&fastTable->m_Table[aByte]);
            *(pt++) = chi;
            *pt = chj;
        }
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
                fastTable->m_Table[0][ch] = 0xF0;
                fastTable->m_Table[1][ch] = 0x0F;
            }
    }
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

    return fastTable;
}


// Function to initialize m_IndexString and m_StringIndex
void CSeqportUtil_implementation::InitIndexCodeName()
{
    m_IndexString[kName].resize(kNumCodes);
    m_IndexString[kSymbol].resize(kNumCodes);
    m_IndexComplement.resize(kNumCodes);
    m_StringIndex.resize(kNumCodes);
    m_StartAt.resize(kNumCodes);

    bool found[kNumCodes];
    for (unsigned int ii = 0; ii < kNumCodes; ii++) {
        found[ii] = false;
    }
    ITERATE (CSeq_code_set::TCodes, it, m_SeqCodeSet->GetCodes()) {
        const ESeq_code_type& code = (*it)->GetCode();
        if (!found[code-1]) {
            found[code-1] = true;
            m_StartAt[code-1] = (*it)->IsSetStart_at() ?
                (*it)->GetStart_at() : 0;
            TIndex i = m_StartAt[code-1];
            ITERATE(CSeq_code_table::TTable, is, (*it)->GetTable()) {
                m_IndexString[kSymbol][code-1].push_back((*is)->GetSymbol());
                m_IndexString[kName][code-1].push_back((*is)->GetName());
                m_StringIndex[code-1].insert
                    (make_pair((*is)->GetSymbol(), i++));
            }
            if ( (*it)->IsSetComps() ) {
                ITERATE (list<int>, ic, (*it)->GetComps()) {
                    m_IndexComplement[code-1].push_back(*ic);
                }
            }
        }
    }
    
     
}


// Function to initialize m_Masks
CRef<CSeqportUtil_implementation::SMasksArray> CSeqportUtil_implementation::InitMasks()
{

    unsigned int i, j, uCnt;
    unsigned char cVal, cRslt;
    CRef<SMasksArray> aMask(new SMasksArray);

    // Initialize possible masks for converting ambiguous
    // ncbi4na bytes to unambiguous bytes
    static const unsigned char mask[16] = {
        0x11, 0x12, 0x14, 0x18,
        0x21, 0x22, 0x24, 0x28,
        0x41, 0x42, 0x44, 0x48,
        0x81, 0x82, 0x84, 0x88
    };

    static const unsigned char maskUpper[4] = { 0x10, 0x20, 0x40, 0x80 };
    static const unsigned char maskLower[4] = { 0x01, 0x02, 0x04, 0x08 };

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

/*
struct SSeqDataToSeqUtil
{
    CSeq_data::E_Choice  seq_data_coding;
    CSeqConvert::TCoding seq_convert_coding;
};


static SSeqDataToSeqUtil s_SeqDataToSeqUtilMap[] = {
    { CSeq_data::e_Iupacna,   CSeqUtil::e_Iupacna },
    { CSeq_data::e_Iupacaa,   CSeqUtil::e_Iupacna },
    { CSeq_data::e_Ncbi2na,   CSeqUtil::e_Ncbi2na },
    { CSeq_data::e_Ncbi4na,   CSeqUtil::e_Ncbi4na },
    { CSeq_data::e_Ncbi8na,   CSeqUtil::e_Ncbi8na },
    { CSeq_data::e_Ncbi8aa,   CSeqUtil::e_Ncbi8aa },
    { CSeq_data::e_Ncbieaa,   CSeqUtil::e_Ncbieaa },
    { CSeq_data::e_Ncbistdaa, CSeqUtil::e_Ncbistdaa }
};
*/

static CSeqUtil::TCoding s_SeqDataToSeqUtil[] = {
    CSeqUtil::e_not_set,
    CSeqUtil::e_Iupacna,
    CSeqUtil::e_Iupacaa,
    CSeqUtil::e_Ncbi2na,
    CSeqUtil::e_Ncbi4na,
    CSeqUtil::e_Ncbi8na,
    CSeqUtil::e_not_set,
    CSeqUtil::e_Ncbi8aa,
    CSeqUtil::e_Ncbieaa,
    CSeqUtil::e_not_set,
    CSeqUtil::e_Ncbistdaa
};


// Convert from one coding scheme to another. The following
// 12 conversions are supported: ncbi2na<=>ncbi4na;
// ncbi2na<=>iupacna; ncbi4na<=>iupacna; ncbieaa<=>ncbistdaa;
// ncbieaa<=>iupacaa; ncbistdaa<=>iupacaa. Convert is
// really just a dispatch function--it calls the appropriate
// priviate conversion function.
TSeqPos CSeqportUtil_implementation::x_ConvertAmbig
(const CSeq_data&      in_seq,
 CSeq_data*            out_seq,
 CSeq_data::E_Choice   to_code,
 TSeqPos               uBeginIdx,
 TSeqPos               uLength,
 CRandom::TValue       seed,
 TSeqPos               total_length,
 TSeqPos*              out_seq_length,
 vector<Uint4>*        blast_ambig)
    const
{
    CSeq_data::E_Choice from_code = in_seq.Which();

    if(to_code == CSeq_data::e_not_set || from_code == CSeq_data::e_not_set)
        throw std::runtime_error("to_code or from_code not set");

    if ( to_code != CSeq_data::e_Ncbi2na ) {
        throw std::runtime_error("to_code is not Ncbi2na");
    }

    switch (from_code) {
    case CSeq_data::e_Iupacna:
        return MapIupacnaToNcbi2na(in_seq, out_seq, uBeginIdx, uLength, true,
                                   seed, total_length, out_seq_length, 
                                   blast_ambig);
    case CSeq_data::e_Ncbi4na:
        return MapNcbi4naToNcbi2na(in_seq, out_seq, uBeginIdx, uLength, true, 
                                   seed, total_length, out_seq_length, 
                                   blast_ambig);
    default:
        throw runtime_error("Requested conversion not implemented");
    }
}

// Convert from one coding scheme to another. The following
// 12 conversions are supported: ncbi2na<=>ncbi4na;
// ncbi2na<=>iupacna; ncbi4na<=>iupacna; ncbieaa<=>ncbistdaa;
// ncbieaa<=>iupacaa; ncbistdaa<=>iupacaa. Convert is
// really just a dispatch function--it calls the appropriate
// priviate conversion function.
TSeqPos CSeqportUtil_implementation::Convert
(const CSeq_data&      in_seq,
 CSeq_data*            out_seq,
 CSeq_data::E_Choice   to_code,
 TSeqPos               uBeginIdx,
 TSeqPos               uLength,
 bool                  bAmbig,
 CRandom::TValue       seed,
 TSeqPos               total_length,
 TSeqPos*              out_seq_length,
 vector<Uint4>*        blast_ambig)
    const
{
    CSeq_data::E_Choice from_code = in_seq.Which();

    // adjust uLength
    if ( uLength == 0 ) {
        uLength = numeric_limits<TSeqPos>::max();
    }

    if(to_code == CSeq_data::e_not_set || from_code == CSeq_data::e_not_set) {
        throw std::runtime_error("to_code or from_code not set");
    }
    if ( s_SeqDataToSeqUtil[to_code]  == CSeqUtil::e_not_set  ||
         s_SeqDataToSeqUtil[from_code] == CSeqUtil::e_not_set ) {
        throw runtime_error("Requested conversion not implemented");
    }

    // Note: for now use old code to convert to ncbi2na with random
    // conversion of ambiguous characters.
    if ( (to_code == CSeq_data::e_Ncbi2na)  &&  (bAmbig == true) ) {
        return x_ConvertAmbig(in_seq, out_seq, to_code, uBeginIdx, uLength, 
                              seed, total_length, out_seq_length, blast_ambig);
    }

    const string* in_str = 0;
    const vector<char>* in_vec = 0;

    x_GetSeqFromSeqData(in_seq, &in_str, &in_vec);
    
    TSeqPos retval = 0;
    if ( in_str != 0 ) {
        string result;
        retval = CSeqConvert::Convert(*in_str, s_SeqDataToSeqUtil[from_code],
                                      uBeginIdx, uLength,
                                      result, s_SeqDataToSeqUtil[to_code]);
        CSeq_data temp(result, to_code);
        out_seq->Assign(temp);
    } else if ( in_vec != 0 ) {
        vector<char> result;
        retval = CSeqConvert::Convert(*in_vec, s_SeqDataToSeqUtil[from_code],
                                      uBeginIdx, uLength,
                                      result, s_SeqDataToSeqUtil[to_code]);
        CSeq_data temp(result, to_code);
        out_seq->Assign(temp);
    }
    return retval;
}


// Provide maximum packing without loss of information
TSeqPos CSeqportUtil_implementation::Pack
(CSeq_data*   in_seq,
 TSeqPos      uLength)
    const
{
    _ASSERT(in_seq != 0);

    CSeq_data::E_Choice from_code = in_seq->Which();
    _ASSERT(from_code != CSeq_data::e_not_set);
    
    if ( s_SeqDataToSeqUtil[from_code] == CSeqUtil::e_not_set ) {
        throw runtime_error("Unable tp pack requested coding");
    }

    
    // nothing to pack for proteins
    switch ( from_code ) {
    case CSeq_data::e_Iupacaa:
        return in_seq->GetIupacaa().Get().size();
    case CSeq_data::e_Ncbi8aa:
        return in_seq->GetNcbi8aa().Get().size();
    case CSeq_data::e_Ncbieaa:
        return in_seq->GetNcbieaa().Get().size();
    case CSeq_data::e_Ncbipaa:
        return in_seq->GetNcbipaa().Get().size();
    case CSeq_data::e_Ncbistdaa:
        return in_seq->GetNcbistdaa().Get().size();
    default:
        break;
    }
    // nothing to convert
    if ( from_code == CSeq_data::e_Ncbi2na  &&
         in_seq->GetNcbi2na().Get().size() * 4 <= uLength ) {
        return in_seq->GetNcbi2na().Get().size() * 4;
    }

    const string*       in_str = 0;
    const vector<char>* in_vec = 0;

    x_GetSeqFromSeqData(*in_seq, &in_str, &in_vec);

    vector<char> out_vec;
    CSeqUtil::TCoding coding = CSeqUtil::e_not_set;

    TSeqPos retval = 0;
    if ( in_str != 0 ) {
        retval =
            CSeqConvert::Pack(*in_str, s_SeqDataToSeqUtil[from_code],
                              out_vec, coding, uLength);
    } else if ( in_vec != 0 ) {
        retval = 
            CSeqConvert::Pack(*in_vec, s_SeqDataToSeqUtil[from_code],
                              out_vec, coding, uLength);
    }

    switch (coding) {
    case CSeqUtil::e_Ncbi2na:
        in_seq->SetNcbi2na().Set(out_vec);
        break;
    case CSeqUtil::e_Ncbi4na:
        in_seq->SetNcbi4na().Set(out_vec);
        break;
    default:
        _TROUBLE;
    }

    return retval;
}


// Method to quickly validate that a CSeq_data object contains valid data.
// FastValidate is a dispatch function that calls the appropriate
// private fast validation function.
bool CSeqportUtil_implementation::FastValidate
(const CSeq_data&   in_seq,
 TSeqPos            uBeginIdx,
 TSeqPos            uLength)
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
(const CSeq_data&   in_seq,
 vector<TSeqPos>*   badIdx,
 TSeqPos            uBeginIdx,
 TSeqPos            uLength)
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
TSeqPos CSeqportUtil_implementation::GetAmbigs
(const CSeq_data&     in_seq,
 CSeq_data*           out_seq,
 vector<TSeqPos>*     out_indices,
 CSeq_data::E_Choice  to_code,
 TSeqPos              uBeginIdx,
 TSeqPos              uLength)
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
TSeqPos CSeqportUtil_implementation::GetCopy
(const CSeq_data&   in_seq,
 CSeq_data*         out_seq,
 TSeqPos            uBeginIdx,
 TSeqPos            uLength)
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
TSeqPos CSeqportUtil_implementation::Keep
(CSeq_data*   in_seq,
 TSeqPos      uBeginIdx,
 TSeqPos      uLength)
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
TSeqPos CSeqportUtil_implementation::Append
(CSeq_data*         out_seq,
 const CSeq_data&   in_seq1,
 TSeqPos            uBeginIdx1,
 TSeqPos            uLength1,
 const CSeq_data&   in_seq2,
 TSeqPos            uBeginIdx2,
 TSeqPos            uLength2)
    const
{
    // Check that in_seqs or of same type
    if(in_seq1.Which() != in_seq2.Which())
        throw runtime_error("Append in_seq types do not match.");
        
    // Check that out_seq is not null
    if(!out_seq) {
        return 0;
    }

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
TSeqPos CSeqportUtil_implementation::Complement
(CSeq_data*   in_seq,
 TSeqPos      uBeginIdx,
 TSeqPos      uLength)
    const
{
    _ASSERT(in_seq != 0);

    CSeq_data complement;
    TSeqPos retval = Complement(*in_seq, &complement, uBeginIdx, uLength);
    in_seq->Assign(complement);

    return retval;
}


// Method to complement na sequence in a copy out_seq
TSeqPos CSeqportUtil_implementation::Complement
(const CSeq_data&   in_seq,
 CSeq_data*         out_seq,
 TSeqPos            uBeginIdx,
 TSeqPos            uLength)
    const
{
    _ASSERT(out_seq != 0);

    if ( uLength == 0 ) {
        uLength = numeric_limits<TSeqPos>::max();
    }
    CSeq_data::E_Choice in_code = in_seq.Which();
    _ASSERT(in_code != CSeq_data::e_not_set);

    const string* in_str = 0;
    const vector<char>* in_vec = 0;
    x_GetSeqFromSeqData(in_seq, &in_str, &in_vec);

    TSeqPos retval = 0;
    if ( in_str ) {
        string out_str;
        retval = CSeqManip::Complement(*in_str, s_SeqDataToSeqUtil[in_code], uBeginIdx, uLength, out_str);
        CSeq_data temp(out_str, in_code);
        out_seq->Assign(temp);
    } else if (in_vec != 0) {
        vector<char> out_vec;
        retval = CSeqManip::Complement(*in_vec, s_SeqDataToSeqUtil[in_code], uBeginIdx, uLength, out_vec);
        CSeq_data temp(out_vec, in_code);
        out_seq->Assign(temp);
    }
    return retval;
}


// Methods to reverse na sequences. These are
// dispatch functions.

// Method to reverse na sequence in place
TSeqPos CSeqportUtil_implementation::Reverse
(CSeq_data*   in_seq,
 TSeqPos      uBeginIdx,
 TSeqPos      uLength)
    const
{
    CSeq_data temp;
    TSeqPos retval = Reverse(*in_seq, &temp, uBeginIdx, uLength);
    in_seq->Assign(temp);

    return retval;
}


// Method to reverse na sequence in a copy out_seq
TSeqPos CSeqportUtil_implementation::Reverse
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
    const
{
    _ASSERT(out_seq != 0);

    if ( uLength == 0 ) {
        uLength = numeric_limits<TSeqPos>::max();
    }

    CSeq_data::E_Choice in_code = in_seq.Which();
    _ASSERT(in_code != CSeq_data::e_not_set);

    const string* in_str = 0;
    const vector<char>* in_vec = 0;
    x_GetSeqFromSeqData(in_seq, &in_str, &in_vec);

    TSeqPos retval = 0;
    if ( in_str ) {
        string out_str;
        retval = CSeqManip::Reverse(*in_str, s_SeqDataToSeqUtil[in_code], uBeginIdx, uLength, out_str);
        CSeq_data temp(out_str, in_code);
        out_seq->Assign(temp);
    } else if (in_vec != NULL) {
        vector<char> out_vec;
        retval = CSeqManip::Reverse(*in_vec, s_SeqDataToSeqUtil[in_code], uBeginIdx, uLength, out_vec);
        CSeq_data temp(out_vec, in_code);
        out_seq->Assign(temp);
    }
    return retval;
}



// Methods to reverse-complement a sequence. These are
// dispatch functions.

// Method to reverse-complement na sequence in place
TSeqPos CSeqportUtil_implementation::ReverseComplement
(CSeq_data*  in_seq,
 TSeqPos     uBeginIdx,
 TSeqPos     uLength)
    const
{
    _ASSERT(in_seq != 0);

    CSeq_data::E_Choice in_code = in_seq->Which();
    _ASSERT(in_code != CSeq_data::e_not_set);

    string* in_str = 0;
    vector<char>* in_vec = 0;
    x_GetSeqFromSeqData(*in_seq, &in_str, &in_vec);

    if ( in_str ) {
        return CSeqManip::ReverseComplement(*in_str, s_SeqDataToSeqUtil[in_code], uBeginIdx, uLength);
    } else if (in_vec != NULL) {
        return CSeqManip::ReverseComplement(*in_vec, s_SeqDataToSeqUtil[in_code], uBeginIdx, uLength);
    }
}


// Method to reverse-complement na sequence in a copy out_seq
TSeqPos CSeqportUtil_implementation::ReverseComplement
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
    const
{
    _ASSERT(out_seq != 0);

    if ( uLength == 0 ) {
        uLength = numeric_limits<TSeqPos>::max();
    }

    CSeq_data::E_Choice in_code = in_seq.Which();
    _ASSERT(in_code != CSeq_data::e_not_set);

    const string* in_str = 0;
    const vector<char>* in_vec = 0;
    x_GetSeqFromSeqData(in_seq, &in_str, &in_vec);

    TSeqPos retval = 0;
    if ( in_str ) {
        string out_str;
        retval = CSeqManip::ReverseComplement(*in_str, s_SeqDataToSeqUtil[in_code], uBeginIdx, uLength, out_str);
        CSeq_data temp(out_str, in_code);
        out_seq->Assign(temp);
    } else if (in_vec != NULL) {
        vector<char> out_vec;
        retval = CSeqManip::ReverseComplement(*in_vec, s_SeqDataToSeqUtil[in_code], uBeginIdx, uLength, out_vec);
        CSeq_data temp(out_vec, in_code);
        out_seq->Assign(temp);
    }

    return retval;
}


// Implement private worker functions called by public
// dispatch functions.

// Methods to convert between coding schemes

/*
// Convert in_seq from ncbi2na (1 byte) to iupacna (4 bytes)
// and put result in out_seq
TSeqPos CSeqportUtil_implementation::MapNcbi2naToIupacna
(const CSeq_data&   in_seq,
 CSeq_data*         out_seq,
 TSeqPos            uBeginIdx,
 TSeqPos            uLength)
    const
{
    // Save uBeginIdx and uLength for later use
    TSeqPos uBeginSav = uBeginIdx;
    TSeqPos uLenSav = uLength;

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
    TSeqPos uOverhang = (uBeginSav + uLenSav) % 4;
    uOverhang = (uOverhang ==0) ? 4 : uOverhang;
    for(pchar = pval; pchar < pval + uOverhang; ++pchar) {
        (*(++i_out)) = *pchar;
    }

    return uLenSav;
}


// Convert in_seq from ncbi2na (1 byte) to ncbi4na (2 bytes)
// and put result in out_seq
TSeqPos CSeqportUtil_implementation::MapNcbi2naToNcbi4na
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
    const
{
    // Get vector holding the in sequence
    const vector<char>& in_seq_data = in_seq.GetNcbi2na().Get();

    // Get vector where out sequence will go
    out_seq->Reset();
    vector<char>& out_seq_data = out_seq->SetNcbi4na().Set();

    // Save uBeginIdx and uLength for later use as they
    // are modified below
    TSeqPos uBeginSav = uBeginIdx;
    TSeqPos uLenSav = uLength;

    // Check that uBeginSav is not beyond end of in_seq_data
    if(uBeginSav >= 4*in_seq_data.size())
        return 0;

    // Adjust uLenSav
    if((uLenSav == 0) || ((uBeginSav + uLenSav) > 4*in_seq_data.size()))
        uLenSav = 4*in_seq_data.size() - uBeginSav;


    // Adjust uBeginIdx and uLength, if necessary
    TSeqPos uOverhang =
        Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 4, 2);

    // Declare iterator for in_seq
    vector<char>::const_iterator i_in;

    // Allocate memory for out_seq_data
    TSeqPos uInBytes = (uLength + uOverhang)/4;
    if(((uLength + uOverhang) % 4) != 0) uInBytes++;
    vector<char>::size_type nOutBytes = 2*uInBytes;
    out_seq_data.resize(nOutBytes);

    // Get an iterator of out_seq_data
    vector<char>::iterator i_out = out_seq_data.begin()-1;

    // Determine begin and end bytes of in_seq_data
    vector<char>::const_iterator i_in_begin =
        in_seq_data.begin() + uBeginIdx/4;
    vector<char>::const_iterator i_in_end = i_in_begin + uInBytes;

    // Loop through in_seq_data and convert to out_seq_data
    for(i_in = i_in_begin; i_in != i_in_end; ++i_in) {
        unsigned short uVal =
            m_FastNcbi2naNcbi4na->m_Table[static_cast<unsigned char>(*i_in)];
        char* pch = reinterpret_cast<char*>(&uVal);
        (*(++i_out)) = (*(pch++));
        (*(++i_out)) = (*(pch++));
    }
    TSeqPos keepidx = uBeginSav - uBeginIdx;
    KeepNcbi4na(out_seq, keepidx, uLenSav);

    return uLenSav;
}


// Convert in_seq from ncbi4na (1 byte) to iupacna (2 bytes)
// and put result in out_seq
TSeqPos CSeqportUtil_implementation::MapNcbi4naToIupacna
(const CSeq_data& in_seq,
 CSeq_data*       out_seq,
 TSeqPos          uBeginIdx,
 TSeqPos          uLength)
    const
{
    // Save uBeginIdx and uLength for later use
    TSeqPos uBeginSav = uBeginIdx;
    TSeqPos uLenSav = uLength;

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
    TSeqPos uOverhang = (uBeginSav + uLenSav) % 2;
    uOverhang = (uOverhang ==0) ? 2 : uOverhang;
    for(pchar = pval; pchar < pval + uOverhang; ++pchar)
        (*(++i_out)) = *pchar;

    return uLenSav;
}
*/

// Table for quick check of whether an ncbi4na residue represents an ambiguity.
// The 0 value is not considered an ambiguity, as it represents the end of
// buffer.
static const char kAmbig4na[16] = 
    { 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1 };

class CAmbiguityContext {
public:    
    CAmbiguityContext(vector<Uint4>& amb_buff, int seq_length);
    // Make sure the vector is not freed in the destructor
    ~CAmbiguityContext() {}
    void UpdateBuffer();
    void AddAmbiguity(char in_byte, TSeqPos& seq_pos);
    void Finish();
private:
    vector<Uint4>& m_vAmbBuf; ///< Ambiguity buffer to fill
    char m_LastAmbChar; ///< Last previous ambiguity character
    TSeqPos m_AmbCount;
    TSeqPos m_AmbStart;
    int m_BuffPos;
    bool m_bLongFormat;
    TSeqPos m_MaxAmbCount;
};

CAmbiguityContext::CAmbiguityContext(vector<Uint4>& amb_buff, int seq_length)
    : m_vAmbBuf(amb_buff)
{
    m_AmbCount = 0;
    m_AmbStart = 0;
    m_BuffPos = 0;
    m_LastAmbChar = 0;
    m_bLongFormat = (seq_length >= 0x00ffffff);
    m_MaxAmbCount = (m_bLongFormat ? 0x00000fff : 0x0000000f);
    // If "long format", set the top bit in the length element of the 
    // ambiguity vector, but only if the input vector is empty. Otherwise,
    // assume that this initialization has already been done in the previous 
    // invocation.
    if (m_vAmbBuf.size() == 0) {
        Uint4 amb_len = (m_bLongFormat ? 0x80000000 : 0);
        m_vAmbBuf.push_back(amb_len);
    }
}


void CAmbiguityContext::UpdateBuffer()
{
    // If there are no more unprocessed ambiguities, return.
    if (!m_LastAmbChar)
        return;

    Uint4 amb_element = m_LastAmbChar << 28;
    // In long format length occupies bits 16-27, and sequence position is stored
    // in the next integer element. In short format length occupies bits 24-27,
    // and sequence position is stored in the same integer element. 
    if (m_bLongFormat) {
        amb_element |= (m_AmbCount << 16);
        m_vAmbBuf.push_back(amb_element);
        m_vAmbBuf.push_back(m_AmbStart);
    } else {
        amb_element |= (m_AmbCount << 24);
        amb_element |= m_AmbStart;
        m_vAmbBuf.push_back(amb_element);
    }
}

void CAmbiguityContext::AddAmbiguity(char in_byte, TSeqPos& seq_pos)
{
    char res[2];

    res[0] = (in_byte >> 4) & 0x0f;
    res[1] = in_byte & 0x0f;

    for (int i = 0; i < 2; ++i, ++seq_pos) {
        if (kAmbig4na[(int)res[i]]) {
            if ((res[i] != m_LastAmbChar) || (m_AmbCount >= m_MaxAmbCount)) {
                // Finish the previous ambiguity element, start new; 
                UpdateBuffer();
                m_LastAmbChar = res[i];
                m_AmbCount = 0;
                m_AmbStart = seq_pos;
            } else { 
                // Just increment the count for the last ambiguity
                ++m_AmbCount;
            }
        } else {
            // No ambiguity: finish the previous ambiguity element, if any, 
            // reset the m_LastAmbChar and count.
            UpdateBuffer();
            m_LastAmbChar = 0;
            m_AmbCount = 0;
        }
    }
}

void CAmbiguityContext::Finish()
{
    UpdateBuffer();
    // In the first element of the vector, preserve the top bit, and reset the
    // remainder to the number of ambiguity entries.
    m_vAmbBuf[0] = 
        (m_vAmbBuf[0] & 0x80000000) | ((m_vAmbBuf.size() - 1) & 0x7fffffff);
}

// Function to convert iupacna (4 bytes) to ncbi2na (1 byte)
TSeqPos CSeqportUtil_implementation::MapIupacnaToNcbi2na
(const CSeq_data& in_seq,
 CSeq_data*       out_seq,
 TSeqPos          uBeginIdx,
 TSeqPos          uLength,
 bool             bAmbig,
 CRandom::TValue  seed,
 TSeqPos          total_length,
 TSeqPos*         out_seq_length, 
 vector<Uint4>*   blast_ambig)
    const
{
    // Get string holding the in_seq
    const string& in_seq_data = in_seq.GetIupacna().Get();

    // Out sequence may contain unfinished byte from the previous segment
    if (*out_seq_length == 0)
       out_seq->Reset();
    // Get vector where the out sequence will go
    vector<char>& out_seq_data = out_seq->SetNcbi2na().Set();

    // If uBeginIdx is after end of in_seq, return
    if(uBeginIdx >= in_seq_data.size())
        return 0;

    // Determine return value
    TSeqPos uLenSav = uLength;
    if((uLenSav == 0) || ((uLenSav + uBeginIdx)) > in_seq_data.size())
        uLenSav = in_seq_data.size() - uBeginIdx;


    // Adjust uBeginIdx and uLength, if necessary and get uOverhang
    TSeqPos uOverhang =
        Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 1, 4);

    // Check if the output sequence data has already been filled 
    // with some previous data, e.g. previous segment of a delta 
    // sequence.
    TSeqPos out_seq_pos = 0;
    if (out_seq_length) {
        out_seq_pos = *out_seq_length;
        *out_seq_length += uLenSav;
    }
    TSeqPos rbit = 2*(out_seq_pos % 4);
    TSeqPos lbit = 8 - rbit;

    // Allocate vector memory for result of conversion
    // Note memory for overhang is added below.
    vector<char>::size_type nBytes = (out_seq_pos + uLenSav + 3) / 4;
    out_seq_data.resize(nBytes);

    // Instantiate an ambiguity context object, if BLAST-style 
    // ambiguity output is requested.
    auto_ptr<CAmbiguityContext> amb_context(NULL);
    if (blast_ambig) {
        amb_context.reset(new CAmbiguityContext(*blast_ambig, total_length));
    }

    // Declare iterator for out_seq_data and determine begin and end
    vector<char>::iterator i_out;
    vector<char>::iterator i_out_begin = out_seq_data.begin() + out_seq_pos/4;
    vector<char>::iterator i_out_end = i_out_begin + uLength/4;

    // Determine begin of in_seq_data
    string::const_iterator i_in = in_seq_data.begin() + uBeginIdx;

    char new_byte;
    const int kOneByteMask = 0xff;

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
            for(i_out = i_out_begin; i_out != i_out_end; )
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

                    if (blast_ambig) {
                        amb_context->AddAmbiguity(c1, out_seq_pos);
                        amb_context->AddAmbiguity(c2, out_seq_pos);
                    }

		    // Randomly pick disambiguated Ncbi4na bytes
                    rv = rg.GetRand() % 16;
                    c1 &= m_Masks->m_Table[c1].cMask[rv];
                    rv = rg.GetRand() % 16;
                    c2 &= m_Masks->m_Table[c2].cMask[rv];

		    // Convert from Ncbi4na to Ncbi2na
                    // Calculate the new byte. Assign parts of it to the 
                    // remainder of the current output byte, and the 
                    // front part of the next output byte, advancing the 
                    // output iterator in the process.
                    new_byte = m_FastNcbi4naNcbi2na->m_Table[0][c1] |
                        m_FastNcbi4naNcbi2na->m_Table[1][c2];
                    (*i_out) |= ((new_byte & kOneByteMask) >> rbit);
                    ++i_out;
                    // Fill part of next byte only if it is necessary, i.e. when
                    // rbit is not 0.
                    if (rbit)
                        (*i_out) = ((new_byte & kOneByteMask) << lbit);

		    // Increment input sequence iterator.
                    i_in+=4;
                }

            // Handle overhang at end of in_seq
            switch (uOverhang) {
            case 1:
                c1 =
                    m_FastIupacnaNcbi4na->m_Table
                    [0][static_cast<unsigned char>(*i_in)];
                if (blast_ambig)
                    amb_context->AddAmbiguity(c1, out_seq_pos);
                rv = rg.GetRand() % 16;
                c1 &= m_Masks->m_Table[c1].cMask[rv];
                new_byte = m_FastNcbi4naNcbi2na->m_Table[0][c1] & 0xC0;
                break;
            case 2:
                c1 =
                    m_FastIupacnaNcbi4na->m_Table
                    [0][static_cast<unsigned char>(*i_in)] |
                    m_FastIupacnaNcbi4na->m_Table
                    [1][static_cast<unsigned char>(*(i_in+1))];
                if (blast_ambig)
                    amb_context->AddAmbiguity(c1, out_seq_pos);
                rv = rg.GetRand() % 16;
                c1 &= m_Masks->m_Table[c1].cMask[rv];
                new_byte = m_FastNcbi4naNcbi2na->m_Table[0][c1] & 0xF0;
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
                if (blast_ambig) {
                    amb_context->AddAmbiguity(c1, out_seq_pos);
                    amb_context->AddAmbiguity(c2, out_seq_pos);
                }
                rv = rg.GetRand() % 16;
                c1 &= m_Masks->m_Table[c1].cMask[rv];
                rv = rg.GetRand() % 16;
                c2 &= m_Masks->m_Table[c2].cMask[rv];
                new_byte = (m_FastNcbi4naNcbi2na->m_Table[0][c1] & 0xF0) |
                    (m_FastNcbi4naNcbi2na->m_Table[1][c2] & 0x0C);
                break;
            default:
                // This is a bogus assignment, just to suppress a
                // compiler warning. The value will not actually be
                // used (see the "uOverhang > 0" condition below)
                new_byte = 0;
                break;
            }

            // Assign respective parts of the new byte to the remaining parts
            // of the output sequence. Output iterator only needs to be 
            // incremented if the overhang is greater than the unfilled
            // remainder  of the last output byte.
            if (uOverhang > 0) {
                (*i_out) |= ((new_byte & kOneByteMask) >> rbit);
                if (2*uOverhang > lbit) {
                    ++i_out;
                    (*i_out) = ((new_byte & kOneByteMask) << lbit);
                }
            }

            if (blast_ambig)
                amb_context->Finish();
        }
    else
        {
            // Pack uLength input characters into out_seq_data
            for(i_out = i_out_begin; i_out != i_out_end; )
                {
                    new_byte =
                        m_FastIupacnaNcbi2na->m_Table
                        [0][static_cast<unsigned char>(*(i_in))] |
                        m_FastIupacnaNcbi2na->m_Table
                        [1][static_cast<unsigned char>(*(i_in+1))] |
                        m_FastIupacnaNcbi2na->m_Table
                        [2][static_cast<unsigned char>(*(i_in+2))] |
                        m_FastIupacnaNcbi2na->m_Table
                        [3][static_cast<unsigned char>(*(i_in+3))];
                    (*i_out) |= ((new_byte & kOneByteMask) >> rbit);
                    ++i_out;
                    // Fill part of next byte only if it is necessary, i.e. when
                    // rbit is not 0.
                    if (rbit)
                        (*i_out) = ((new_byte & kOneByteMask) << lbit); 
                    i_in+=4;
                }

            // Handle overhang
            if(uOverhang > 0) {
                new_byte = '\x00';
                for(TSeqPos i = 0; i < uOverhang; i++) {
                    new_byte |=
                        m_FastIupacnaNcbi2na->m_Table
                        [i][static_cast<unsigned char>(*(i_in+i))];
                }
                (*i_out) |= ((new_byte & kOneByteMask) >> rbit);
                if (2*uOverhang > lbit) {
                    ++i_out;
                    (*i_out) = ((new_byte & kOneByteMask) << lbit); 
                }
            }
        }
    return uLenSav;
}
/*

// Function to convert iupacna (2 bytes) to ncbi4na (1 byte)
TSeqPos CSeqportUtil_implementation::MapIupacnaToNcbi4na
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
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
    TSeqPos uLenSav = uLength;
    if((uLenSav == 0) || (uLenSav + uBeginIdx) > in_seq_data.size())
        uLenSav = in_seq_data.size() - uBeginIdx;

    // Adjust uBeginIdx and uLength and get uOverhang
    TSeqPos uOverhang =
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
*/


// Function to convert ncbi4na (2 bytes) to ncbi2na (1 byte)
TSeqPos CSeqportUtil_implementation::MapNcbi4naToNcbi2na(
             const CSeq_data&  in_seq, CSeq_data* out_seq,
             TSeqPos uBeginIdx, TSeqPos uLength, 
             bool bAmbig, CRandom::TValue seed,
             TSeqPos total_length,
             TSeqPos* out_seq_length, vector<Uint4>* blast_ambig)
    const
{
    // Get vector holding the in_seq
    const vector<char>& in_seq_data = in_seq.GetNcbi4na().Get();

    // Out sequence may contain unfinished byte from a previous segment.
    if (*out_seq_length == 0)
       out_seq->Reset();
    // Get vector where the out sequence will go
    vector<char>& out_seq_data = out_seq->SetNcbi2na().Set();

    // Save uBeginIdx and uLength as they will be modified below
    TSeqPos uBeginSav = uBeginIdx;
    TSeqPos uLenSav = uLength;


    // Check that uBeginSav is not beyond end of in_seq
    if(uBeginSav >= 2*in_seq_data.size())
        return 0;

    // Adjust uLenSav if needed
    if((uLenSav == 0) || ((uBeginSav + uLenSav) > 2*in_seq_data.size()))
        uLenSav = 2*in_seq_data.size() - uBeginSav;

    // Adjust uBeginIdx and uLength and get uOverhang
    TSeqPos uOverhang =
        Adjust(&uBeginIdx, &uLength, in_seq_data.size(), 2, 4);

    // Check if the output sequence data has already been filled 
    // with some previous data, e.g. previous segment of a delta 
    // sequence.
    TSeqPos out_seq_pos = 0;
    if (out_seq_length) {
        out_seq_pos = *out_seq_length;
        *out_seq_length += uLenSav;
    }
    TSeqPos rbit = 2*(out_seq_pos % 4);
    TSeqPos lbit = 8 - rbit;

    // Allocate vector memory for result of conversion
    // Note memory for overhang is added below.
    vector<char>::size_type nBytes = (out_seq_pos + uLenSav + 3) / 4;
    out_seq_data.resize(nBytes);

    // Instantiate an ambiguity context object, if BLAST-style 
    // ambiguity output is requested.
    auto_ptr<CAmbiguityContext> amb_context(NULL);
    if (blast_ambig) {
        amb_context.reset(new CAmbiguityContext(*blast_ambig, total_length));
    }

    // Declare iterator for out_seq_data and determine begin and end
    vector<char>::iterator i_out;
    vector<char>::iterator i_out_begin = out_seq_data.begin() + out_seq_pos/4;
    vector<char>::iterator i_out_end = i_out_begin + uLength/4;

    // Make sure that the first byte of out_seq_data does not contain garbage
    // in the bits that are not yet supposed to be filled.
    *i_out_begin &= (0xff << lbit);

    // Determine begin of in_seq_data
    vector<char>::const_iterator i_in = in_seq_data.begin() + uBeginIdx/2;

    char new_byte;
    const int kOneByteMask = 0xff;

    if(bAmbig) {         // Do random disambiguation
        // Declare a random number generator and set seed
        CRandom rg;
        rg.SetSeed(seed);
        
        // Pack uLength input bytes into out_seq_data
        for(i_out = i_out_begin; i_out != i_out_end; ) {
            // Disambiguate
            unsigned char c1 = static_cast<unsigned char>(*i_in);
            unsigned char c2 = static_cast<unsigned char>(*(i_in+1));

            if (blast_ambig) {
                amb_context->AddAmbiguity(c1, out_seq_pos);
                amb_context->AddAmbiguity(c2, out_seq_pos);
            }
            CRandom::TValue rv = rg.GetRand() % 16;
            c1 &= m_Masks->m_Table[c1].cMask[rv];
            rv = rg.GetRand() % 16;
            c2 &= m_Masks->m_Table[c2].cMask[rv];
            
            // Convert
            new_byte = m_FastNcbi4naNcbi2na->m_Table[0][c1] |
                m_FastNcbi4naNcbi2na->m_Table[1][c2];
            (*i_out) |= ((new_byte & kOneByteMask) >> rbit);
            ++i_out;
            // Fill part of next byte only if it is necessary, i.e. when
            // rbit is not 0.
            if (rbit)
                (*i_out) = ((new_byte & kOneByteMask) << lbit);
            i_in+=2;
        }
        
        // Handle overhang
        new_byte = '\x00';

        if(uOverhang > 0) {
            // Disambiguate
            unsigned char c1 = static_cast<unsigned char>(*i_in);
            // If only one residue, make sure that the second half of the byte
            // is 0.
            if (uOverhang == 1)
                c1 &= 0xf0;
            if (blast_ambig)
                amb_context->AddAmbiguity(c1, out_seq_pos);
            CRandom::TValue rv = rg.GetRand() % 16;
            c1 &= m_Masks->m_Table[c1].cMask[rv];
            
            // Convert
            new_byte |= m_FastNcbi4naNcbi2na->m_Table[0][c1];
        }
        
        if(uOverhang == 3) {
            // Disambiguate; make sure that the second half of the byte
            // is 0.
            unsigned char c1 = static_cast<unsigned char>(*(++i_in)) & 0xf0;
            if (blast_ambig)
                amb_context->AddAmbiguity(c1, out_seq_pos);
            CRandom::TValue rv = rg.GetRand() % 16;
            c1 &= m_Masks->m_Table[c1].cMask[rv];
            
            // Convert
            new_byte |= m_FastNcbi4naNcbi2na->m_Table[1][c1];
        }
        
        if(uOverhang > 0) {
            (*i_out) |= ((new_byte & kOneByteMask) >> rbit);
            if (2*uOverhang > lbit) {
                ++i_out;
                (*i_out) = ((new_byte & kOneByteMask) << lbit); 
            }
        }
        
        if (blast_ambig)
            amb_context->Finish();
    } else { // Do not do random disambiguation
        
        // Pack uLength input bytes into out_seq_data
        for(i_out = i_out_begin; i_out != i_out_end; ) {
            new_byte = 
                m_FastNcbi4naNcbi2na->m_Table
                [0][static_cast<unsigned char>(*i_in)] |
                m_FastNcbi4naNcbi2na->m_Table
                [1][static_cast<unsigned char>(*(i_in+1))];
            (*i_out) |= ((new_byte & kOneByteMask) >> rbit);
            ++i_out;
            // Fill part of next byte only if it is necessary, i.e. when
            // rbit is not 0.
            if (rbit)
                (*i_out) = ((new_byte & kOneByteMask) << lbit);
            i_in+=2;
        }
        
        // Handle overhang
        if(uOverhang > 0) {
            new_byte = '\x00';
            new_byte |= m_FastNcbi4naNcbi2na->m_Table
                [0][static_cast<unsigned char>(*i_in)];
        
            if(uOverhang == 3)
                new_byte |= m_FastNcbi4naNcbi2na->m_Table
                    [1][static_cast<unsigned char>(*(++i_in))];
        
            (*i_out) |= ((new_byte & kOneByteMask) >> rbit);
            if (2*uOverhang > lbit) {
                ++i_out;
                (*i_out) = ((new_byte & kOneByteMask) << lbit); 
            }
        }
    }

    TSeqPos keepidx = uBeginSav - uBeginIdx;
    KeepNcbi2na(out_seq, keepidx, uLenSav);

    return uLenSav;
}

/*
// Function to convert iupacaa (byte) to ncbieaa (byte)
TSeqPos CSeqportUtil_implementation::MapIupacaaToNcbieaa
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
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
TSeqPos CSeqportUtil_implementation::MapNcbieaaToIupacaa
(const CSeq_data&   in_seq,
 CSeq_data*         out_seq,
 TSeqPos            uBeginIdx,
 TSeqPos            uLength)
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
TSeqPos CSeqportUtil_implementation::MapIupacaaToNcbistdaa
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
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
TSeqPos CSeqportUtil_implementation::MapNcbieaaToNcbistdaa
(const CSeq_data&   in_seq,
 CSeq_data*         out_seq,
 TSeqPos            uBeginIdx,
 TSeqPos            uLength)
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
TSeqPos CSeqportUtil_implementation::MapNcbistdaaToNcbieaa
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
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
TSeqPos CSeqportUtil_implementation::MapNcbistdaaToIupacaa
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
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
*/

// Fast validation of iupacna sequence
bool CSeqportUtil_implementation::FastValidateIupacna
(const CSeq_data&  in_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
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
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
    const
{
    // Get read-only reference to in_seq data
    const string& in_seq_data = in_seq.GetNcbieaa().Get();

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
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
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
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
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
(const CSeq_data&   in_seq,
 vector<TSeqPos>*   badIdx,
 TSeqPos            uBeginIdx,
 TSeqPos            uLength)
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
    TSeqPos nIdx = uBeginIdx;
    for(itor = b_itor; itor != e_itor; ++itor)
        if(m_Iupacna->m_Table[static_cast<unsigned char>(*itor)] == char(255))
            badIdx->push_back(nIdx++);
        else
            nIdx++;

    // Return list of bad indices
    return;
}


void CSeqportUtil_implementation::ValidateNcbieaa
(const CSeq_data&   in_seq,
 vector<TSeqPos>*   badIdx,
 TSeqPos            uBeginIdx,
 TSeqPos            uLength)
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
    TSeqPos nIdx = uBeginIdx;
    for(itor = b_itor; itor != e_itor; ++itor)
        if(m_Ncbieaa->m_Table[static_cast<unsigned char>(*itor)] == char(255))
            badIdx->push_back(nIdx++);
        else
            nIdx++;

    // Return vector of bad indices
    return;
}


void CSeqportUtil_implementation::ValidateNcbistdaa
(const CSeq_data&   in_seq,
 vector<TSeqPos>*   badIdx,
 TSeqPos            uBeginIdx,
 TSeqPos            uLength)
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
    TSeqPos nIdx = uBeginIdx;
    for(itor=b_itor; itor!=e_itor; ++itor)
        if(m_Ncbistdaa->m_Table[static_cast<unsigned char>(*itor)]==char(255))
            badIdx->push_back(nIdx++);
        else
            nIdx++;

    // Return vector of bad indices
    return;
}


void CSeqportUtil_implementation::ValidateIupacaa
(const CSeq_data&   in_seq,
 vector<TSeqPos>*   badIdx,
 TSeqPos            uBeginIdx,
 TSeqPos            uLength)
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
    TSeqPos nIdx = uBeginIdx;
    for(itor=b_itor; itor!=e_itor; ++itor)
        if(m_Iupacaa->m_Table[static_cast<unsigned char>(*itor)] == char(255))
            badIdx->push_back(nIdx++);
        else
            nIdx++;

    // Return vector of bad indices
    return;
}


// Function to make copy of ncbi2na type sequences
TSeqPos CSeqportUtil_implementation::GetNcbi2naCopy
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
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
    SIZE_TYPE uNumBytes = uLength/4;
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
TSeqPos CSeqportUtil_implementation::GetNcbi4naCopy
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
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
    SIZE_TYPE uNumBytes = uLength/2;
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
TSeqPos CSeqportUtil_implementation::GetIupacnaCopy
(const CSeq_data& in_seq,
 CSeq_data*       out_seq,
 TSeqPos          uBeginIdx,
 TSeqPos          uLength)
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
TSeqPos CSeqportUtil_implementation::GetNcbieaaCopy
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
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
TSeqPos CSeqportUtil_implementation::GetNcbistdaaCopy
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
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
TSeqPos CSeqportUtil_implementation::GetIupacaaCopy
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
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
TSeqPos CSeqportUtil_implementation::Adjust
(TSeqPos*  uBeginIdx,
 TSeqPos*  uLength,
 TSeqPos   uInSeqBytes,
 TSeqPos   uInSeqsPerByte,
 TSeqPos   uOutSeqsPerByte)
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
    TSeqPos uOverhang = *uLength % uOutSeqsPerByte;
    *uLength = uOutSeqsPerByte * (*uLength / uOutSeqsPerByte);

    return uOverhang;

}


// Loops through an ncbi4na input sequence and determines
// the ambiguities that would result from conversion to an ncbi2na sequence
// On return, out_seq contains the ncbi4na bases that become ambiguous and
// out_indices contains the indices of the abiguous bases in in_seq
TSeqPos CSeqportUtil_implementation::GetAmbigs_ncbi4na_ncbi2na
(const CSeq_data&   in_seq,
 CSeq_data*         out_seq,
 vector<TSeqPos>*   out_indices,
 TSeqPos            uBeginIdx,
 TSeqPos            uLength)
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
    TSeqPos uBeginSav = uBeginIdx;
    TSeqPos uLenSav = uLength;
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
    TSeqPos uNumAmbigs = 0;

    // Get iterators to input sequence
    vector<char>::const_iterator i_in;
    vector<char>::const_iterator i_in_begin =
        in_seq_data.begin() + uBeginIdx/2;
    vector<char>::const_iterator i_in_end =
        i_in_begin + uLength/2 + (uLength % 2);

    // Get iterators to out_seq_data and out_indices
    vector<char>::iterator i_out_seq = out_seq_data.begin();
    vector<TSeqPos>::iterator i_out_idx = out_indices->begin();

    // Index of current input seq base
    TSeqPos uIdx = uBeginIdx;

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
    TSeqPos uKeepBeg = 0;
    TSeqPos uKeepLen = 0;
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
TSeqPos CSeqportUtil_implementation::GetAmbigs_iupacna_ncbi2na
(const CSeq_data&   in_seq,
 CSeq_data*         out_seq,
 vector<TSeqPos>*   out_indices,
 TSeqPos            uBeginIdx,
 TSeqPos            uLength)
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
    TSeqPos uNumAmbigs = 0;

    // Get iterators to input sequence
    string::const_iterator i_in;
    string::const_iterator i_in_begin = in_seq_data.begin() + uBeginIdx;
    string::const_iterator i_in_end = i_in_begin + uLength;

    // Get iterators to out_seq_data and out_indices
    string::iterator i_out_seq = out_seq_data.begin();
    vector<TSeqPos>::iterator i_out_idx = out_indices->begin();

    // Index of current input seq base
    TSeqPos uIdx = uBeginIdx;

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
TSeqPos CSeqportUtil_implementation::KeepNcbi2na
(CSeq_data*   in_seq,
 TSeqPos      uBeginIdx,
 TSeqPos      uLength)
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
    TSeqPos uStart = uBeginIdx/4;

    // Determine index within start byte
    TSeqPos uStartInByte = 2 * (uBeginIdx % 4);

    // Calculate masks
    unsigned char rightMask = 0xff << uStartInByte;
    unsigned char leftMask = ~rightMask;

    // Determine index in in_seq_data that holds uBeginIdx + uLength
    // residue
    TSeqPos uEnd = (uBeginIdx + uLength - 1)/4;

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
    TSeqPos uSize = uLength/4;
    if((uLength % 4) != 0)
        uSize++;
    in_seq_data.resize(uSize);

    return uLength;
}


// Method to implement Keep for Ncbi4na. Returns length of
// kept sequence.
TSeqPos CSeqportUtil_implementation::KeepNcbi4na
(CSeq_data*   in_seq,
 TSeqPos      uBeginIdx,
 TSeqPos      uLength)
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
    TSeqPos uStart = uBeginIdx/2;

    // Determine index within start byte
    unsigned int uStartInByte = 4 * (uBeginIdx % 2);

    // Calculate masks
    unsigned char rightMask = 0xff << uStartInByte;
    unsigned char leftMask = ~rightMask;

    // Determine index in in_seq_data that holds uBeginIdx + uLength
    // residue
    TSeqPos uEnd = (uBeginIdx + uLength - 1)/2;

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
    TSeqPos uSize = uLength/2;
    if((uLength % 2) != 0)
        uSize++;
    in_seq_data.resize(uSize);

    return uLength;
}


// Method to implement Keep for Iupacna. Return length
// of kept sequence
TSeqPos CSeqportUtil_implementation::KeepIupacna
(CSeq_data*   in_seq,
 TSeqPos      uBeginIdx,
 TSeqPos      uLength)
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
TSeqPos CSeqportUtil_implementation::KeepNcbieaa
(CSeq_data*   in_seq,
 TSeqPos      uBeginIdx,
 TSeqPos      uLength)
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
TSeqPos CSeqportUtil_implementation::KeepNcbistdaa
(CSeq_data*   in_seq,
 TSeqPos      uBeginIdx,
 TSeqPos      uLength)
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
TSeqPos CSeqportUtil_implementation::KeepIupacaa
(CSeq_data*   in_seq,
 TSeqPos      uBeginIdx,
 TSeqPos      uLength)
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
TSeqPos CSeqportUtil_implementation::ComplementIupacna
(CSeq_data*   in_seq,
 TSeqPos      uBeginIdx,
 TSeqPos      uLength)
    const
{
    // Keep just the part of in_seq that will be complemented
    TSeqPos uKept = KeepIupacna(in_seq, uBeginIdx, uLength);

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


TSeqPos CSeqportUtil_implementation::ComplementNcbi2na
(CSeq_data*   in_seq,
 TSeqPos      uBeginIdx,
 TSeqPos      uLength)
    const
{
    // Keep just the part of in_seq that will be complemented
    TSeqPos uKept = KeepNcbi2na(in_seq, uBeginIdx, uLength);

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


TSeqPos CSeqportUtil_implementation::ComplementNcbi4na
(CSeq_data*   in_seq,
 TSeqPos      uBeginIdx,
 TSeqPos      uLength)
    const
{
    // Keep just the part of in_seq that will be complemented
    TSeqPos uKept = KeepNcbi4na(in_seq, uBeginIdx, uLength);

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
TSeqPos CSeqportUtil_implementation::ComplementIupacna
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
    const
{
    TSeqPos uKept = GetIupacnaCopy(in_seq, out_seq, uBeginIdx, uLength);
    TSeqPos uIdx1 = 0, uIdx2 = 0;
    ComplementIupacna(out_seq, uIdx1, uIdx2);
    return uKept;
}


TSeqPos CSeqportUtil_implementation::ComplementNcbi2na
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
    const
{
    TSeqPos uKept = GetNcbi2naCopy(in_seq, out_seq, uBeginIdx, uLength);
    TSeqPos uIdx1 = 0, uIdx2 = 0;
    ComplementNcbi2na(out_seq, uIdx1, uIdx2);
    return uKept;
}


TSeqPos CSeqportUtil_implementation::ComplementNcbi4na
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
    const
{
    TSeqPos uKept = GetNcbi4naCopy(in_seq, out_seq, uBeginIdx, uLength);
    TSeqPos uIdx1 = 0, uIdx2 = 0;
    ComplementNcbi4na(out_seq, uIdx1, uIdx2);
    return uKept;
}


// Methods to reverse na sequences

// In place methods
TSeqPos CSeqportUtil_implementation::ReverseIupacna
(CSeq_data*  in_seq,
 TSeqPos     uBeginIdx,
 TSeqPos     uLength)
    const
{
    // Keep just the part of in_seq that will be reversed
    TSeqPos uKept = KeepIupacna(in_seq, uBeginIdx, uLength);

    // Get in_seq data
    string& in_seq_data = in_seq->SetIupacna().Set();

    // Reverse the order of the string
    reverse(in_seq_data.begin(), in_seq_data.end());

    return uKept;
}


TSeqPos CSeqportUtil_implementation::ReverseNcbi2na
(CSeq_data*  in_seq,
 TSeqPos     uBeginIdx,
 TSeqPos     uLength)
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
    TSeqPos uStart = uBeginIdx/4;
    TSeqPos uEnd = uStart + (uLength - 1 +(uBeginIdx % 4))/4 + 1;

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
    TSeqPos uJagged = 3 - ((uBeginIdx + uLength - 1) % 4) + 4*uStart;
    return KeepNcbi2na(in_seq, uJagged, uLength);
}


TSeqPos CSeqportUtil_implementation::ReverseNcbi4na
(CSeq_data*   in_seq,
 TSeqPos      uBeginIdx,
 TSeqPos      uLength)
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
    TSeqPos uStart = uBeginIdx/2;
    TSeqPos uEnd = uStart + (uLength - 1 +(uBeginIdx % 2))/2 + 1;

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
    TSeqPos uJagged = 1 - ((uBeginIdx + uLength - 1) % 2) + 2*uStart;
    return KeepNcbi4na(in_seq, uJagged, uLength);
}


// Reverse in copy methods
TSeqPos CSeqportUtil_implementation::ReverseIupacna
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
    const
{
    GetIupacnaCopy(in_seq, out_seq, uBeginIdx, uLength);

    TSeqPos uIdx1 = 0, uIdx2 = uLength;
    return ReverseIupacna(out_seq, uIdx1, uIdx2);
}


TSeqPos CSeqportUtil_implementation::ReverseNcbi2na
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
    const
{
    GetNcbi2naCopy(in_seq, out_seq, uBeginIdx, uLength);

    TSeqPos uIdx1 = 0, uIdx2 = uLength;
    return ReverseNcbi2na(out_seq, uIdx1, uIdx2);
}


TSeqPos CSeqportUtil_implementation::ReverseNcbi4na
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
    const
{
    GetNcbi4naCopy(in_seq, out_seq, uBeginIdx, uLength);

    TSeqPos uIdx1 = 0, uIdx2 = uLength;
    return ReverseNcbi4na(out_seq, uIdx1, uIdx2);
}


// Methods to reverse-complement an na sequences

// In place methods
TSeqPos CSeqportUtil_implementation::ReverseComplementIupacna
(CSeq_data*        in_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
    const
{
    ReverseIupacna(in_seq, uBeginIdx, uLength);

    TSeqPos uIdx = 0;
    return ComplementIupacna(in_seq, uIdx, uLength);
}


TSeqPos CSeqportUtil_implementation::ReverseComplementNcbi2na
(CSeq_data*        in_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
    const
{
    ReverseNcbi2na(in_seq, uBeginIdx, uLength);

    TSeqPos uIdx = 0;
    return ComplementNcbi2na(in_seq, uIdx, uLength);
}


TSeqPos CSeqportUtil_implementation::ReverseComplementNcbi4na
(CSeq_data*        in_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
    const
{
    ReverseNcbi4na(in_seq, uBeginIdx, uLength);

    TSeqPos uIdx = 0;
    return ComplementNcbi4na(in_seq, uIdx, uLength);
}


// Reverse in copy methods
TSeqPos CSeqportUtil_implementation::ReverseComplementIupacna
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
    const
{
    ReverseIupacna(in_seq, out_seq, uBeginIdx, uLength);

    TSeqPos uIdx = 0;
    return ComplementIupacna(out_seq, uIdx, uLength);
}


TSeqPos CSeqportUtil_implementation::ReverseComplementNcbi2na
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
    const
{
    ReverseNcbi2na(in_seq, out_seq, uBeginIdx, uLength);

    TSeqPos uIdx = 0;
    return ComplementNcbi2na(out_seq, uIdx, uLength);
}


TSeqPos CSeqportUtil_implementation::ReverseComplementNcbi4na
(const CSeq_data&  in_seq,
 CSeq_data*        out_seq,
 TSeqPos           uBeginIdx,
 TSeqPos           uLength)
    const
{
    ReverseNcbi4na(in_seq, out_seq, uBeginIdx, uLength);

    TSeqPos uIdx = 0;
    return ComplementNcbi4na(out_seq, uIdx, uLength);
}


// Append methods
TSeqPos CSeqportUtil_implementation::AppendIupacna
(CSeq_data*        out_seq,
 const CSeq_data&  in_seq1,
 TSeqPos           uBeginIdx1,
 TSeqPos           uLength1,
 const CSeq_data&  in_seq2,
 TSeqPos           uBeginIdx2,
 TSeqPos           uLength2)
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


TSeqPos CSeqportUtil_implementation::AppendNcbi2na
(CSeq_data*        out_seq,
 const CSeq_data&  in_seq1,
 TSeqPos           uBeginIdx1,
 TSeqPos           uLength1,
 const CSeq_data&  in_seq2,
 TSeqPos           uBeginIdx2,
 TSeqPos           uLength2)
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
    TSeqPos uTotalLength = uLength1 + uLength2;
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
    TSeqPos uStart1 = uBeginIdx1/4;
    TSeqPos uEnd1;
    if(((uBeginIdx1 + uLength1) % 4) == 0)
        uEnd1 = (uBeginIdx1 + uLength1)/4;
    else
        uEnd1 = (uBeginIdx1 + uLength1)/4 + 1;

    TSeqPos uStart2 = uBeginIdx2/4;
    TSeqPos uEnd2;
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
    TSeqPos uEndOutByte;
    if((uLength1 % 4) == 0)
        uEndOutByte = uLength1/4 - 1;
    else
        uEndOutByte = uLength1/4;
    if(i_out != (out_seq_data.begin() + uEndOutByte))
        (*(++i_out)) = (*i_in1) << lShift1;

    // Connect in_seq1 and in_seq2
    unsigned char uMask1 = 255 << (8 - 2*(uLength1 % 4));
    unsigned char uMask2 = 255 >> (2*(uBeginIdx2 % 4));
    TSeqPos uSeq2Inc = 1;

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


TSeqPos CSeqportUtil_implementation::AppendNcbi4na
(CSeq_data*        out_seq,
 const CSeq_data&  in_seq1,
 TSeqPos           uBeginIdx1,
 TSeqPos           uLength1,
 const CSeq_data&  in_seq2,
 TSeqPos           uBeginIdx2,
 TSeqPos           uLength2)
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
    TSeqPos uTotalLength = uLength1 + uLength2;
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
    TSeqPos uStart1 = uBeginIdx1/2;
    TSeqPos uEnd1;
    if(((uBeginIdx1 + uLength1) % 2) == 0)
        uEnd1 = (uBeginIdx1 + uLength1)/2;
    else
        uEnd1 = (uBeginIdx1 + uLength1)/2 + 1;

    TSeqPos uStart2 = uBeginIdx2/2;
    TSeqPos uEnd2;
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
    TSeqPos uEndOutByte;
    if((uLength1 % 2) == 0)
        uEndOutByte = uLength1/2 - 1;
    else
        uEndOutByte = uLength1/2;
    if(i_out != (out_seq_data.begin() + uEndOutByte))
        (*(++i_out)) = (*i_in1) << lShift1;

    // Connect in_seq1 and in_seq2
    unsigned char uMask1 = 255 << (8 - 4*(uLength1 % 2));
    unsigned char uMask2 = 255 >> (4*(uBeginIdx2 % 2));
    TSeqPos uSeq2Inc = 1;

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


TSeqPos CSeqportUtil_implementation::AppendNcbieaa
(CSeq_data*        out_seq,
 const CSeq_data&  in_seq1,
 TSeqPos           uBeginIdx1,
 TSeqPos           uLength1,
 const CSeq_data&  in_seq2,
 TSeqPos           uBeginIdx2,
 TSeqPos           uLength2)
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


TSeqPos CSeqportUtil_implementation::AppendNcbistdaa
(CSeq_data*          out_seq,
 const CSeq_data&    in_seq1,
 TSeqPos             uBeginIdx1,
 TSeqPos             uLength1,
 const CSeq_data&    in_seq2,
 TSeqPos             uBeginIdx2,
 TSeqPos             uLength2)
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


TSeqPos CSeqportUtil_implementation::AppendIupacaa
(CSeq_data*          out_seq,
 const CSeq_data&    in_seq1,
 TSeqPos             uBeginIdx1,
 TSeqPos             uLength1,
 const CSeq_data&    in_seq2,
 TSeqPos             uBeginIdx2,
 TSeqPos             uLength2)
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

// Returns the 3 letter Iupacaa3 code for an ncbistdaa index
const string& CSeqportUtil_implementation::GetIupacaa3
(TIndex ncbistdaa)
{
    return GetCodeOrName(eSeq_code_type_iupacaa3, ncbistdaa, true);
}

// Returns true if code type is available
bool CSeqportUtil_implementation::IsCodeAvailable
(CSeq_data::E_Choice code_type)
{
    if (code_type == CSeq_data::e_not_set) {
        return false;
    } else {
        return IsCodeAvailable(EChoiceToESeq(code_type));
    }
}

// Return true if code type is available
bool CSeqportUtil_implementation::IsCodeAvailable (ESeq_code_type code_type)
{
    // Iterate through Seq-code-set looking for code type
    ITERATE (CSeq_code_set::TCodes, i_ct, m_SeqCodeSet->GetCodes()) {
        if((*i_ct)->GetCode() == code_type) {
            return true;  
        }
    }
    return false;
}

// Return a pair containing the first index (start-at) and last index 
// for code_type. 
CSeqportUtil::TPair  CSeqportUtil_implementation::GetCodeIndexFromTo
(CSeq_data::E_Choice code_type)
{
    return GetCodeIndexFromTo(EChoiceToESeq(code_type));
}

// Return a pair containing the first index (start-at) and last index 
// for code_type. 
CSeqportUtil::TPair CSeqportUtil_implementation::GetCodeIndexFromTo
(ESeq_code_type code_type)
{
    // Iterate through Seq-code-set looking for code type
    TPair p;
    ITERATE (CSeq_code_set::TCodes, i_ct, m_SeqCodeSet->GetCodes()) {
        if((*i_ct)->GetCode() == code_type) {
            if ( (*i_ct)->IsSetStart_at() ) {
                p.first = static_cast<TIndex>((*i_ct)->GetStart_at());
            } else {
                p.first = 0;
            }
            p.second = p.first + static_cast<TIndex>((*i_ct)->GetNum() - 1);
            return p;  
        }
    }
    throw CSeqportUtil::CBadType("GetCodeIndexFromTo");
}

// Converts CSeq_data::E_Choice type to ESeq_code_type
// and calls overloaded GetCodeOrName()
const string& CSeqportUtil_implementation::GetCodeOrName
(CSeq_data::E_Choice code_type, 
 TIndex              idx,
 bool                get_code) 
{ 
    return GetCodeOrName(EChoiceToESeq(code_type), idx, get_code);   
}

// Returns the code (symbol) of type code_type for index idx. 
const string& CSeqportUtil_implementation::GetCodeOrName
(ESeq_code_type code_type, 
 TIndex         idx,
 bool           get_code) 
{
    if ( !m_IndexString[get_code][code_type-1].size() ) {
        throw CSeqportUtil::CBadType("GetCodeOrName");
    }
    idx -= m_StartAt[code_type-1];
    if (idx >= m_IndexString[get_code][code_type-1].size()) {
        throw CSeqportUtil::CBadIndex(idx, "GetCodeOrName");
    }
    return m_IndexString[get_code][code_type-1][idx];
       
}

// Converts CSeq_data::E_Choice type to ESeq_code_type and call
// overloaded GetIndex();
CSeqportUtil::TIndex CSeqportUtil_implementation::GetIndex
(CSeq_data::E_Choice code_type, 
 const string&       code)
{
    return GetIndex(EChoiceToESeq(code_type), code);
}

// Get the index for code of type code_type. If not found, return -1
CSeqportUtil::TIndex CSeqportUtil_implementation::GetIndex
(ESeq_code_type code_type, 
 const string&  code)
{
    // Iterator to a map mapping a string code to a code index
    map<string, TIndex>::const_iterator pos;
    
    if ( !m_StringIndex[code_type-1].size() ) {
        throw CSeqportUtil::CBadType("GetIndex");
    }
    pos = m_StringIndex[code_type-1].find(code);
    if (pos != m_StringIndex[code_type-1].end()) {
        return pos->second;
    } else {
        throw CSeqportUtil::CBadSymbol(code, "GetIndex");
    }
    
}

// Gets complement of index for code type. Returns -1 if code
// type does not exist
CSeqportUtil::TIndex CSeqportUtil_implementation::GetIndexComplement
(CSeq_data::E_Choice code_type,
 TIndex              idx)
{
    return GetIndexComplement(EChoiceToESeq(code_type), idx);
}

// Returns the complement of the index for code_type. If code_type
// does not exist, or complements for code_type do not exist,
// returns -1
CSeqportUtil::TIndex CSeqportUtil_implementation::GetIndexComplement
(ESeq_code_type code_type,
 TIndex         idx)
{
  
    // Check that code is available
    if (!m_IndexComplement[code_type-1].size()) {
        throw CSeqportUtil::CBadType("GetIndexComplement");
    }
    
    // Check that idx is in range of code indices
    idx -= m_StartAt[code_type-1];
    if ( idx >= m_IndexComplement[code_type-1].size() ) {        
        throw CSeqportUtil::CBadIndex(idx, "GetIndexComplement");
    }
    
    // Return the index of the complement   
    return m_IndexComplement[code_type-1][idx];
 }

CSeqportUtil::TIndex CSeqportUtil_implementation::GetMapToIndex
(CSeq_data::E_Choice from_type,
 CSeq_data::E_Choice to_type,
 TIndex              from_idx)
{
    return GetMapToIndex(EChoiceToESeq(from_type), 
                         EChoiceToESeq(to_type),
                         from_idx);
}

CSeqportUtil::TIndex CSeqportUtil_implementation::GetMapToIndex
(ESeq_code_type from_type,
 ESeq_code_type to_type,
 TIndex            from_idx)
{
    CMap_table* Map = 0;
    
    if (from_type == eSeq_code_type_iupacna) {
        if (to_type == eSeq_code_type_ncbi2na) {
            Map = m_IupacnaNcbi2na.GetPointer();
        } else if (to_type == eSeq_code_type_ncbi4na) {
            Map = m_IupacnaNcbi4na.GetPointer();
        }
    } else if (from_type == eSeq_code_type_ncbi4na) {
        if (to_type == eSeq_code_type_iupacna) {
            Map = m_Ncbi4naIupacna.GetPointer();
        } else if (to_type == eSeq_code_type_ncbi2na) {
            Map = m_Ncbi4naNcbi2na.GetPointer();
        }
    } else if (from_type == eSeq_code_type_ncbi2na) {
        if (to_type == eSeq_code_type_iupacna) {
            Map = m_Ncbi2naIupacna.GetPointer();
        } else if (to_type == eSeq_code_type_ncbi4na) {
            Map = m_Ncbi2naNcbi4na.GetPointer();
        }
    } else if (from_type == eSeq_code_type_iupacaa) {
        if (to_type == eSeq_code_type_ncbieaa) {
            Map = m_IupacaaNcbieaa.GetPointer();
        } else if (to_type == eSeq_code_type_ncbistdaa) {
            Map = m_IupacaaNcbistdaa.GetPointer();
        }
    } else if (from_type == eSeq_code_type_ncbieaa) {
        if (to_type == eSeq_code_type_iupacaa) {
            Map = m_NcbieaaIupacaa.GetPointer();
        } else if (to_type == eSeq_code_type_ncbistdaa) {
            Map = m_NcbieaaNcbistdaa.GetPointer();
        }
    } else if (from_type == eSeq_code_type_ncbistdaa) {
        if (to_type == eSeq_code_type_iupacaa) {
            Map = m_NcbistdaaIupacaa.GetPointer();
        } else if (to_type == eSeq_code_type_ncbieaa) {
            Map = m_NcbistdaaNcbieaa.GetPointer();
        }
    }
    
    // Check that requested map is available
    if (!Map) {
        throw CSeqportUtil::CBadType("GetMapToIndex");
    }
    
    // Check that from_idx is within range of from_type
    if (from_idx - (*Map).m_StartAt >= (TIndex)(*Map).m_Size) {
        throw CSeqportUtil::CBadIndex(from_idx - (*Map).m_StartAt,
            "GetMapToIndex");
    }
    
    // Return map value
    return (*Map).m_Table[from_idx];
    

}


void CSeqportUtil_implementation::x_GetSeqFromSeqData
(const CSeq_data& data, 
 const string** str,
 const vector<char>** vec)
    const
{
    *str = 0;
    *vec = 0;

    switch ( data.Which() ) {
    case CSeq_data::e_Iupacna:
        *str = &(data.GetIupacna().Get());
        break;

    case CSeq_data::e_Ncbi2na:
        *vec = &(data.GetNcbi2na().Get());
        break;

    case CSeq_data::e_Ncbi4na:
        *vec = &(data.GetNcbi4na().Get());
        break;

    case CSeq_data::e_Ncbi8na:
        *vec = &(data.GetNcbi8na().Get());
        break;

    case CSeq_data::e_Iupacaa:
        *str = &(data.GetIupacaa().Get());
        break;

    case CSeq_data::e_Ncbi8aa:
        *vec = &(data.GetNcbi8aa().Get());
        break;

    case CSeq_data::e_Ncbieaa:
        *str = &(data.GetNcbieaa().Get());
        break;

    case CSeq_data::e_Ncbistdaa:
        *vec = &(data.GetNcbistdaa().Get());
        break;

    case CSeq_data::e_not_set:
    case CSeq_data::e_Ncbipna:
    case CSeq_data::e_Ncbipaa:
    case CSeq_data::e_Gap:
        break;
    } // end of switch statement
}


// same as above, but takes a non-const CSeq_data object.
void CSeqportUtil_implementation::x_GetSeqFromSeqData
(CSeq_data& data, 
 string** str,
 vector<char>** vec)
    const
{
    *str = 0;
    *vec = 0;

    switch ( data.Which() ) {
    case CSeq_data::e_Iupacna:
        *str = &(data.SetIupacna().Set());
        break;

    case CSeq_data::e_Ncbi2na:
        *vec = &(data.SetNcbi2na().Set());
        break;

    case CSeq_data::e_Ncbi4na:
        *vec = &(data.SetNcbi4na().Set());
        break;

    case CSeq_data::e_Ncbi8na:
        *vec = &(data.SetNcbi8na().Set());
        break;

    case CSeq_data::e_Iupacaa:
        *str = &(data.SetIupacaa().Set());
        break;

    case CSeq_data::e_Ncbi8aa:
        *vec = &(data.SetNcbi8aa().Set());
        break;

    case CSeq_data::e_Ncbieaa:
        *str = &(data.SetNcbieaa().Set());
        break;

    case CSeq_data::e_Ncbistdaa:
        *vec = &(data.SetNcbistdaa().Set());
        break;

    case CSeq_data::e_not_set:
    case CSeq_data::e_Ncbipna:
    case CSeq_data::e_Ncbipaa:
    case CSeq_data::e_Gap:
        break;
    } // end of switch statement
}


/////////////////////////////////////////////////////////////////////////////
//  CSeqportUtil_implementation::sm_StrAsnData  --  some very long and ugly string
//

// local copy of seqcode.prt sequence alphabet and conversion table ASN.1
const char* CSeqportUtil_implementation::sm_StrAsnData[] =
{
    "-- This is the set of NCBI sequence code tables\n",
    "-- J.Ostell  10/18/91\n",
    "--\n",
    "\n",
    "Seq-code-set ::= {\n",
    " codes {                              -- codes\n",
    " {                                -- IUPACna\n",
    " code iupacna ,\n",
    " num 25 ,                     -- continuous 65-89\n",
    " one-letter TRUE ,            -- all one letter codes\n",
    " start-at 65 ,                -- starts with A, ASCII 65\n",
    " table {\n",
    " { symbol \"A\", name \"Adenine\" },\n",
    " { symbol \"B\" , name \"G or T or C\" },\n",
    " { symbol \"C\", name \"Cytosine\" },\n",
    " { symbol \"D\", name \"G or A or T\" },\n",
    " { symbol \"\", name \"\" },\n",
    " { symbol \"\", name \"\" },\n",
    " { symbol \"G\", name \"Guanine\" },\n",
    " { symbol \"H\", name \"A or C or T\" } ,\n",
    " { symbol \"\", name \"\" },\n",
    " { symbol \"\", name \"\" },\n",
    " { symbol \"K\", name \"G or T\" },\n",
    " { symbol \"\", name \"\"},\n",
    " { symbol \"M\", name \"A or C\" },\n",
    " { symbol \"N\", name \"A or G or C or T\" } ,\n",
    " { symbol \"\", name \"\" },\n",
    " { symbol \"\", name \"\" },\n",
    " { symbol \"\", name \"\"},\n",
    " { symbol \"R\", name \"G or A\"},\n",
    " { symbol \"S\", name \"G or C\"},\n",
    " { symbol \"T\", name \"Thymine\"},\n",
    " { symbol \"\", name \"\"},\n",
    " { symbol \"V\", name \"G or C or A\"},\n",
    " { symbol \"W\", name \"A or T\" },\n",
    " { symbol \"\", name \"\"},\n",
    " { symbol \"Y\", name \"T or C\"}\n",
    " } ,                           -- end of table\n",
    " comps {                      -- complements\n",
    " 84,\n",
    " 86,\n",
    " 71,\n",
    " 72,\n",
    " 69,\n",
    " 70,\n",
    " 67,\n",
    " 68,\n",
    " 73,\n",
    " 74,\n",
    " 77,\n",
    " 76,\n",
    " 75,\n",
    " 78,\n",
    " 79,\n",
    " 80,\n",
    " 81,\n",
    " 89,\n",
    " 83,\n",
    " 65,\n",
    " 85,\n",
    " 66,\n",
    " 87,\n",
    " 88,\n",
    " 82\n",
    " }\n",
    " } ,\n",
    " {                                -- IUPACaa\n",
    " code iupacaa ,\n",
    " num 26 ,                     -- continuous 65-90\n",
    " one-letter TRUE ,            -- all one letter codes\n",
    " start-at 65 ,                -- starts with A, ASCII 65\n",
    " table {\n",
    " { symbol \"A\", name \"Alanine\" },\n",
    " { symbol \"B\" , name \"Asp or Asn\" },\n",
    " { symbol \"C\", name \"Cysteine\" },\n",
    " { symbol \"D\", name \"Aspartic Acid\" },\n",
    " { symbol \"E\", name \"Glutamic Acid\" },\n",
    " { symbol \"F\", name \"Phenylalanine\" },\n",
    " { symbol \"G\", name \"Glycine\" },\n",
    " { symbol \"H\", name \"Histidine\" } ,\n",
    " { symbol \"I\", name \"Isoleucine\" },\n",
    " { symbol \"J\", name \"Leu or Ile\" },\n",
    " { symbol \"K\", name \"Lysine\" },\n",
    " { symbol \"L\", name \"Leucine\" },\n",
    " { symbol \"M\", name \"Methionine\" },\n",
    " { symbol \"N\", name \"Asparagine\" } ,\n",
    " { symbol \"O\", name \"Pyrrolysine\" },\n",
    " { symbol \"P\", name \"Proline\" },\n",
    " { symbol \"Q\", name \"Glutamine\"},\n",
    " { symbol \"R\", name \"Arginine\"},\n",
    " { symbol \"S\", name \"Serine\"},\n",
    " { symbol \"T\", name \"Threonine\"},\n",
    " { symbol \"U\", name \"Selenocysteine\"}, -- was empty\n",
    " { symbol \"V\", name \"Valine\"},\n",
    " { symbol \"W\", name \"Tryptophan\" },\n",
    " { symbol \"X\", name \"Undetermined or atypical\"},\n",
    " { symbol \"Y\", name \"Tyrosine\"},\n",
    " { symbol \"Z\", name \"Glu or Gln\" }\n",
    " }                            -- end of table            \n",
    " } ,\n",
    " {                                -- IUPACeaa\n",
    " code ncbieaa ,\n",
    " num 49 ,                     -- continuous 42-90\n",
    " one-letter TRUE ,            -- all one letter codes\n",
    " start-at 42 ,                -- starts with *, ASCII 42\n",
    " table {\n",
    " { symbol \"*\", name \"Termination\" } ,\n",
    " { symbol \"\", name \"\" } ,\n",
    " { symbol \"\", name \"\" } ,\n",
    " { symbol \"-\", name \"Gap\" } ,\n",
    " { symbol \"\", name \"\" } ,\n",
    " { symbol \"\", name \"\" } ,\n",
    " { symbol \"\", name \"\" } ,\n",
    " { symbol \"\", name \"\" } ,\n",
    " { symbol \"\", name \"\" } ,\n",
    " { symbol \"\", name \"\" } ,\n",
    " { symbol \"\", name \"\" } ,\n",
    " { symbol \"\", name \"\" } ,\n",
    " { symbol \"\", name \"\" } ,\n",
    " { symbol \"\", name \"\" } ,\n",
    " { symbol \"\", name \"\" } ,\n",
    " { symbol \"\", name \"\" } ,\n",
    " { symbol \"\", name \"\" } ,\n",
    " { symbol \"\", name \"\" } ,\n",
    " { symbol \"\", name \"\" } ,\n",
    " { symbol \"\", name \"\" } ,\n",
    " { symbol \"\", name \"\" } ,\n",
    " { symbol \"\", name \"\" } ,\n",
    " { symbol \"\", name \"\" } ,\n",
    " { symbol \"A\", name \"Alanine\" },\n",
    " { symbol \"B\" , name \"Asp or Asn\" },\n",
    " { symbol \"C\", name \"Cysteine\" },\n",
    " { symbol \"D\", name \"Aspartic Acid\" },\n",
    " { symbol \"E\", name \"Glutamic Acid\" },\n",
    " { symbol \"F\", name \"Phenylalanine\" },\n",
    " { symbol \"G\", name \"Glycine\" },\n",
    " { symbol \"H\", name \"Histidine\" } ,\n",
    " { symbol \"I\", name \"Isoleucine\" },\n",
    " { symbol \"J\", name \"Leu or Ile\" },\n",
    " { symbol \"K\", name \"Lysine\" },\n",
    " { symbol \"L\", name \"Leucine\" },\n",
    " { symbol \"M\", name \"Methionine\" },\n",
    " { symbol \"N\", name \"Asparagine\" } ,\n",
    " { symbol \"O\", name \"Pyrrolysine\" },\n",
    " { symbol \"P\", name \"Proline\" },\n",
    " { symbol \"Q\", name \"Glutamine\"},\n",
    " { symbol \"R\", name \"Arginine\"},\n",
    " { symbol \"S\", name \"Serine\"},\n",
    " { symbol \"T\", name \"Threonine\"},\n",
    " { symbol \"U\", name \"Selenocysteine\"},\n",
    " { symbol \"V\", name \"Valine\"},\n",
    " { symbol \"W\", name \"Tryptophan\" },\n",
    " { symbol \"X\", name \"Undetermined or atypical\"},\n",
    " { symbol \"Y\", name \"Tyrosine\"},\n",
    " { symbol \"Z\", name \"Glu or Gln\" }\n",
    " }                            -- end of table            \n",
    " } ,\n",
    " {                                -- IUPACaa3\n",
    " code iupacaa3 ,\n",
    " num 28 ,                     -- continuous 0-27\n",
    " one-letter FALSE ,            -- all 3 letter codes\n",
    " table {\n",
    " { symbol \"---\", name \"Gap\" } ,\n",
    " { symbol \"Ala\", name \"Alanine\" },\n",
    " { symbol \"Asx\" , name \"Asp or Asn\" },\n",
    " { symbol \"Cys\", name \"Cysteine\" },\n",
    " { symbol \"Asp\", name \"Aspartic Acid\" },\n",
    " { symbol \"Glu\", name \"Glutamic Acid\" },\n",
    " { symbol \"Phe\", name \"Phenylalanine\" },\n",
    " { symbol \"Gly\", name \"Glycine\" },\n",
    " { symbol \"His\", name \"Histidine\" } ,\n",
    " { symbol \"Ile\", name \"Isoleucine\" },\n",
    " { symbol \"Lys\", name \"Lysine\" },\n",
    " { symbol \"Leu\", name \"Leucine\" },\n",
    " { symbol \"Met\", name \"Methionine\" },\n",
    " { symbol \"Asn\", name \"Asparagine\" } ,\n",
    " { symbol \"Pro\", name \"Proline\" },\n",
    " { symbol \"Gln\", name \"Glutamine\"},\n",
    " { symbol \"Arg\", name \"Arginine\"},\n",
    " { symbol \"Ser\", name \"Serine\"},\n",
    " { symbol \"Thr\", name \"Threonine\"},\n",
    " { symbol \"Val\", name \"Valine\"},\n",
    " { symbol \"Trp\", name \"Tryptophan\" },\n",
    " { symbol \"Xxx\", name \"Undetermined or atypical\"},\n",
    " { symbol \"Tyr\", name \"Tyrosine\"},\n",
    " { symbol \"Glx\", name \"Glu or Gln\" },\n",
    " { symbol \"Sec\", name \"Selenocysteine\"},\n",
    " { symbol \"Ter\", name \"Termination\" },\n",
    " { symbol \"Pyl\", name \"Pyrrolysine\"},\n",
    " { symbol \"Xle\", name \"Leu or Ile\"}\n",
    " }                            -- end of table            \n",
    " } ,\n",
    " {                                -- NCBIstdaa\n",
    " code ncbistdaa ,\n",
    " num 28 ,                     -- continuous 0-27\n",
    " one-letter TRUE ,            -- all one letter codes\n",
    " table {\n",
    " { symbol \"-\", name \"Gap\" } ,                -- 0\n",
    " { symbol \"A\", name \"Alanine\" },             -- 1\n",
    " { symbol \"B\" , name \"Asp or Asn\" },         -- 2\n",
    " { symbol \"C\", name \"Cysteine\" },            -- 3\n",
    " { symbol \"D\", name \"Aspartic Acid\" },       -- 4\n",
    " { symbol \"E\", name \"Glutamic Acid\" },       -- 5\n",
    " { symbol \"F\", name \"Phenylalanine\" },       -- 6\n",
    " { symbol \"G\", name \"Glycine\" },             -- 7\n",
    " { symbol \"H\", name \"Histidine\" } ,          -- 8\n",
    " { symbol \"I\", name \"Isoleucine\" },          -- 9\n",
    " { symbol \"K\", name \"Lysine\" },              -- 10\n",
    " { symbol \"L\", name \"Leucine\" },             -- 11\n",
    " { symbol \"M\", name \"Methionine\" },          -- 12\n",
    " { symbol \"N\", name \"Asparagine\" } ,         -- 13\n",
    " { symbol \"P\", name \"Proline\" },             -- 14\n",
    " { symbol \"Q\", name \"Glutamine\"},            -- 15\n",
    " { symbol \"R\", name \"Arginine\"},             -- 16\n",
    " { symbol \"S\", name \"Serine\"},               -- 17\n",
    " { symbol \"T\", name \"Threoine\"},             -- 18\n",
    " { symbol \"V\", name \"Valine\"},               -- 19\n",
    " { symbol \"W\", name \"Tryptophan\" },          -- 20\n",
    " { symbol \"X\", name \"Undetermined or atypical\"},  -- 21\n",
    " { symbol \"Y\", name \"Tyrosine\"},             -- 22\n",
    " { symbol \"Z\", name \"Glu or Gln\" },          -- 23\n",
    " { symbol \"U\", name \"Selenocysteine\"},       -- 24 \n",
    " { symbol \"*\", name \"Termination\" },         -- 25\n",
    " { symbol \"O\", name \"Pyrrolysine\" },         -- 26\n",
    " { symbol \"J\", name \"Leu or Ile\" }           -- 27\n",
    " }                            -- end of table            \n",
    " } ,\n",
    " {                                -- NCBI2na\n",
    " code ncbi2na ,\n",
    " num 4 ,                     -- continuous 0-3\n",
    " one-letter TRUE ,            -- all one letter codes\n",
    " table {\n",
    " { symbol \"A\", name \"Adenine\" },\n",
    " { symbol \"C\", name \"Cytosine\" },\n",
    " { symbol \"G\", name \"Guanine\" },\n",
    " { symbol \"T\", name \"Thymine/Uracil\"}\n",
    " } ,                          -- end of table            \n",
    " comps {                      -- complements\n",
    " 3,\n",
    " 2,\n",
    " 1,\n",
    " 0\n",
    " }\n",
    " } ,\n",
    " {                                -- NCBI4na\n",
    " code ncbi4na ,\n",
    " num 16 ,                     -- continuous 0-15\n",
    " one-letter TRUE ,            -- all one letter codes\n",
    " table {\n",
    " { symbol \"-\", name \"Gap\" } ,\n",
    " { symbol \"A\", name \"Adenine\" },\n",
    " { symbol \"C\", name \"Cytosine\" },\n",
    " { symbol \"M\", name \"A or C\" },\n",
    " { symbol \"G\", name \"Guanine\" },\n",
    " { symbol \"R\", name \"G or A\"},\n",
    " { symbol \"S\", name \"G or C\"},\n",
    " { symbol \"V\", name \"G or C or A\"},\n",
    " { symbol \"T\", name \"Thymine/Uracil\"},\n",
    " { symbol \"W\", name \"A or T\" },\n",
    " { symbol \"Y\", name \"T or C\"} ,\n",
    " { symbol \"H\", name \"A or C or T\" } ,\n",
    " { symbol \"K\", name \"G or T\" },\n",
    " { symbol \"D\", name \"G or A or T\" },\n",
    " { symbol \"B\" , name \"G or T or C\" },\n",
    " { symbol \"N\", name \"A or G or C or T\" }\n",
    " } ,                           -- end of table            \n",
    " comps {                       -- complements\n",
    " 0 ,\n",
    " 8 ,\n",
    " 4 ,\n",
    " 12,\n",
    " 2 ,\n",
    " 10,\n",
    " 6 ,\n",
    " 14,\n",
    " 1 ,\n",
    " 9 ,\n",
    " 5 ,\n",
    " 13,\n",
    " 3 ,\n",
    " 11,\n",
    " 7 ,\n",
    " 15\n",
    " }\n",
    " }\n",
    " } ,                                  -- end of codes\n",
    " maps {\n",
    " {\n",
    " from iupacna ,\n",
    " to ncbi2na ,\n",
    " num 25 ,\n",
    " start-at 65 ,\n",
    " table {\n",
    " 0,     -- A -> A\n",
    " 1,     -- B -> C\n",
    " 1,     -- C -> C\n",
    " 2,     -- D -> G\n",
    " 255,\n",
    " 255,\n",
    " 2,     -- G -> G\n",
    " 0,     -- H -> A\n",
    " 255,\n",
    " 255,\n",
    " 2,     -- K -> G\n",
    " 255,\n",
    " 1,     -- M -> C\n",
    " 0,     -- N -> A\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 2,     -- R -> G\n",
    " 1,     -- S -> C\n",
    " 3,     -- T -> T\n",
    " 255,\n",
    " 0,     -- V -> A\n",
    " 3,     -- W -> T\n",
    " 255,\n",
    " 3 }    -- Y -> T\n",
    " } ,\n",
    " {\n",
    " from iupacna ,\n",
    " to ncbi4na ,\n",
    " num 26 ,\n",
    " start-at 64 ,\n",
    " table {\n",
    "		0,     -- @ used by FastaToSeqEntry to convert hyphen to gap\n",
    " 1,     -- A\n",
    " 14,    -- B\n",
    " 2,     -- C\n",
    " 13,    -- D\n",
    " 255,\n",
    " 255,\n",
    " 4,     -- G\n",
    " 11,    -- H\n",
    " 255,\n",
    " 255,\n",
    " 12,    -- K\n",
    " 255,\n",
    " 3,     -- M\n",
    " 15,    -- N\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 5,     -- R\n",
    " 6,     -- S\n",
    " 8,     -- T\n",
    " 255,\n",
    " 7,     -- V\n",
    " 9,     -- W\n",
    " 255,\n",
    " 10 }   -- Y\n",
    " } ,\n",
    " {\n",
    " from ncbi2na ,\n",
    " to iupacna ,\n",
    " num 4 ,\n",
    " table {\n",
    " 65,     -- A\n",
    " 67,     -- C\n",
    " 71,     -- G\n",
    " 84 }    -- T\n",
    " } ,\n",
    " {\n",
    " from ncbi2na ,\n",
    " to ncbi4na ,\n",
    " num 4 ,\n",
    " table {\n",
    " 1,     -- A\n",
    " 2,     -- C\n",
    " 4,     -- G\n",
    " 8 }    -- T\n",
    " } ,\n",
    " {\n",
    " from ncbi4na ,\n",
    " to iupacna ,\n",
    " num 16 ,\n",
    " table {\n",
    " 78,    -- gap -> N\n",
    " 65,    -- A\n",
    " 67,    -- C\n",
    " 77,    -- M\n",
    " 71,    -- G\n",
    " 82,    -- R\n",
    " 83,    -- S\n",
    " 86,    -- V\n",
    " 84,    -- T\n",
    " 87,    -- W\n",
    " 89,    -- Y\n",
    " 72,    -- H\n",
    " 75,    -- K\n",
    " 68,    -- D\n",
    " 66,    -- B\n",
    " 78 }   -- N\n",
    " } ,\n",
    " {\n",
    " from ncbi4na ,\n",
    " to ncbi2na ,\n",
    " num 16 ,\n",
    " table {\n",
    " 3,    -- gap -> T\n",
    " 0,    -- A -> A\n",
    " 1,    -- C -> C\n",
    " 1,    -- M -> C\n",
    " 2,    -- G -> G\n",
    " 2,    -- R -> G\n",
    " 1,    -- S -> C\n",
    " 0,    -- V -> A\n",
    " 3,    -- T -> T\n",
    " 3,    -- W -> T\n",
    " 3,    -- Y -> T\n",
    " 0,    -- H -> A\n",
    " 2,    -- K -> G\n",
    " 2,    -- D -> G\n",
    " 1,    -- B -> C\n",
    " 0 }   -- N -> A\n",
    " } ,\n",
    " {\n",
    " from iupacaa ,\n",
    " to ncbieaa ,\n",
    " num 26 ,\n",
    " start-at 65 ,\n",
    " table {\n",
    " 65 ,    -- they map directly\n",
    " 66 ,\n",
    " 67 ,\n",
    " 68,\n",
    " 69,\n",
    " 70,\n",
    " 71,\n",
    " 72,\n",
    " 73,\n",
    " 74,  -- J - was 255\n",
    " 75,\n",
    " 76,\n",
    " 77,\n",
    " 78,\n",
    " 79,  -- O - was 255\n",
    " 80,\n",
    " 81,\n",
    " 82,\n",
    " 83,\n",
    " 84,\n",
    " 85,  -- U - was 255\n",
    " 86,\n",
    " 87,\n",
    " 88,\n",
    " 89,\n",
    " 90 }\n",
    " } ,\n",
    " {\n",
    " from ncbieaa ,\n",
    " to iupacaa ,\n",
    " num 49 ,\n",
    " start-at 42 ,\n",
    " table {\n",
    " 88 ,   -- termination -> X\n",
    " 255,\n",
    " 255,\n",
    " 88,    -- Gap -> X\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 65 ,    -- from here they map directly\n",
    " 66 ,\n",
    " 67 ,\n",
    " 68,\n",
    " 69,\n",
    " 70,\n",
    " 71,\n",
    " 72,\n",
    " 73,\n",
    " 74,  -- J - was 255\n",
    " 75,\n",
    " 76,\n",
    " 77,\n",
    " 78,\n",
    " 79,  -- O - was 255\n",
    " 80,\n",
    " 81,\n",
    " 82,\n",
    " 83,\n",
    " 84,\n",
    " 85,  -- U was -> X 88\n",
    " 86,\n",
    " 87,\n",
    " 88,\n",
    " 89,\n",
    " 90 }\n",
    " } ,\n",
    " {\n",
    " from iupacaa ,\n",
    " to ncbistdaa ,\n",
    " num 26 ,\n",
    " start-at 65 ,\n",
    " table {\n",
    " 1 ,    -- they map directly\n",
    " 2 ,\n",
    " 3 ,\n",
    " 4,\n",
    " 5,\n",
    " 6,\n",
    " 7,\n",
    " 8,\n",
    " 9,\n",
    " 27,  -- J - was 255\n",
    " 10,\n",
    " 11,\n",
    " 12,\n",
    " 13,\n",
    " 26,  -- O - was 255\n",
    " 14,\n",
    " 15,\n",
    " 16,\n",
    " 17,\n",
    " 18,\n",
    " 24,  -- U - was 255\n",
    " 19,\n",
    " 20,\n",
    " 21,\n",
    "				22,\n",
    " 23 }\n",
    " } ,\n",
    " {\n",
    " from ncbieaa ,\n",
    " to ncbistdaa ,\n",
    " num 49 ,\n",
    " start-at 42 ,\n",
    " table {\n",
    " 25,   -- termination\n",
    " 255,\n",
    " 255,\n",
    " 0,    -- Gap\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 255,\n",
    " 1 ,    -- they map directly\n",
    " 2 ,\n",
    " 3 ,\n",
    " 4,\n",
    " 5,\n",
    " 6,\n",
    " 7,\n",
    " 8,\n",
    " 9,\n",
    " 27,  -- J - was 255\n",
    " 10,\n",
    " 11,\n",
    " 12,\n",
    " 13,\n",
    " 26,  -- O - was 255\n",
    " 14,\n",
    " 15,\n",
    " 16,\n",
    " 17,\n",
    " 18,\n",
    " 24,  -- U\n",
    " 19,\n",
    " 20,\n",
    " 21,\n",
    " 22,\n",
    " 23 }\n",
    " }  ,\n",
    " {\n",
    " from ncbistdaa ,\n",
    " to ncbieaa ,\n",
    " num 28 ,\n",
    " table {\n",
    " 45 ,  --   \"-\"\n",
    " 65 ,    -- they map directly with holes for O and J\n",
    " 66 ,\n",
    " 67 ,\n",
    " 68,\n",
    " 69,\n",
    " 70,\n",
    " 71,\n",
    " 72,\n",
    " 73,\n",
    " 75,\n",
    " 76,\n",
    " 77,\n",
    " 78,\n",
    " 80,\n",
    " 81,\n",
    " 82,\n",
    " 83,\n",
    " 84,\n",
    " 86,\n",
    " 87,\n",
    " 88,\n",
    " 89,\n",
    " 90,\n",
    " 85,	 -- U\n",
    " 42,  -- *\n",
    " 79,	 -- O - new\n",
    " 74}  -- J - new\n",
    " } ,\n",
    " {\n",
    " from ncbistdaa ,\n",
    " to iupacaa ,\n",
    " num 28 ,\n",
    " table {\n",
    " 255 ,  --   \"-\"\n",
    " 65 ,    -- they map directly with holes for O and J\n",
    " 66 ,\n",
    " 67 ,\n",
    " 68,\n",
    " 69,\n",
    " 70,\n",
    " 71,\n",
    " 72,\n",
    " 73,\n",
    " 75,\n",
    " 76,\n",
    " 77,\n",
    " 78,\n",
    " 80,\n",
    " 81,\n",
    " 82,\n",
    " 83,\n",
    " 84,\n",
    " 86,\n",
    " 87,\n",
    " 88,\n",
    " 89,\n",
    " 90,\n",
    " 85,  -- U - was 88\n",
    " 255, -- *\n",
    " 79,	 -- O - new\n",
    " 74}  -- J - new\n",
    " } \n",
    " }\n",
    "-- end of seq-code-set -- }", // make sure '}' is last symbol of ASN text
    0  // to indicate that there is no more data
};


END_objects_SCOPE
END_NCBI_SCOPE
