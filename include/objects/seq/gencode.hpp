#ifndef OBJECTS_SEQ___GENCODE_HPP
#define OBJECTS_SEQ___GENCODE_HPP

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
 * Revision 1.1  2001/08/24 00:32:44  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include <objects/seqloc/Na_strand.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <map>
#include <memory>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


class CSeq_data;
class CGenetic_code;
class CGenetic_code_table;


///  CGencode is a Singleton which provides a utility
///  function Translate() supporting na -> aa translations.

class CGencode
{
public:
    // Function to translate na to aa. in_seq must
    // be in Iupacna and out_seq will be in Ncbieaa
    // The key to the code_breaks map is the index
    // in out_seq (sequence of amino acids) where
    // an exception to the genetic code occurs. The
    // char values will replace the genetic coded
    // implied aa at that index.
    static void Translate
    (const CSeq_data&               in_seq,
     CSeq_data*                     out_seq,
     const CGenetic_code&           genetic_code,
     const map<unsigned int, char>& code_breaks,
     unsigned int                   uBeginIdx          = 0,
     unsigned int                   uLength            = 0,
     bool                           bCheck_first       = true,
     bool                           bPartial_start     = false,
     ENa_strand                     eStrand            = eNa_strand_plus,
     bool                           bStop              = true,
     bool                           bRemove_trailing_x = false);

private:
    // Prevent creation and assignment except by call to Instance()
    CGencode();
    CGencode(const CGencode&);
    CGencode& operator= (const CGencode&);
    ~CGencode();

    // see Translate() in the "public" section above
    void x_Translate
    (const CSeq_data&               in_seq,
     CSeq_data*                     out_seq,
     const CGenetic_code&           genetic_code,
     const map<unsigned int, char>& code_breaks,
     unsigned int                   uBeginIdx,
     unsigned int                   uLength,
     bool                           bCheck_first,
     bool                           bPartial_start,
     ENa_strand                     eStrand,
     bool                           bStop,
     bool                           bRemove_trailing_x)
        const;

    // The unique instance of CGencode
    static CSafeStaticPtr<CGencode> sm_pInstance;
    friend class CSafeStaticPtr<CGencode>;

    // Holds gc.prt Genetic-code-table
    static const char* sm_StrGcAsn[];

    // Genetic code table data
    CRef<CGenetic_code_table> m_GcTable;

    // Initialize genetic codes.
    void InitGcTable();

    // Table to ensure that all codon codes are
    // one of TCAGN and to translate these to 0-4,
    // respectively. Result used as an index to second
    // dimension in m_AaIdx
    unsigned char m_Tran[256];

    // Same as m_Tran but for complements
    // AGTCN translate to 0-4, respectively
    unsigned char m_CTran[256];

    // Table used to determine index into genetic code.
    // First dimension is na residue position in triplet
    // Second dimenstion is 0=T, 1=C, 2=A, 3=G, 4=X
    // Three values are bitwise ORed to determine index
    // into Ncbieaa or Sncbieaa string.
    unsigned char m_AaIdx[3][5];

    // Function to initialize m_Tran, m_CTran, and m_AaIdx
    void InitTran();

    // Function to get requested ncbieaa and sncbieaa strings
    void GetGeneticCode
    (const CGenetic_code& genetic_code,
     bool                 bCheck_first, // false => sncbieaa not needed
     string*              gc,           // ncbieaa string
     string*              sc)           // sncbieaa string
        const;

    // Functions to get ncbieaa genetic code string
    const string& GetNcbieaa(int id) const;
    const string& GetNcbieaa(const string& name) const;

    // Functions to get start code
    const string& GetSncbieaa(int id) const;
    const string& GetSncbieaa(const string& name) const;
};


END_objects_SCOPE
END_NCBI_SCOPE

#endif  /* OBJECTS_SEQ___GENCODE_HPP */
