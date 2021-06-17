#ifndef OBJTOOLS_ALNTEXT__HPP
#define OBJTOOLS_ALNTEXT__HPP

/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE                          
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
* Author:  Eyal Mozes
*
* File Description:
*   Text representation of protein alignment
*   refactored from algo/prosplign
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <util/tables/raw_scoremat.h>

#include <list>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CScope;
    class CSeqVector_CI;
    class CTrans_table;
END_SCOPE(objects)

/// Text representation of ProSplign alignment
// dna        : GATGAAACAGCACTAGTGACAGGTAAA----GATCTAAATATCGTTGA<skip>GGAAGACATCCATTGGCAATGGCAATGGCAT
// translation:  D  E  T  A  L  V  T  G  K        S  K  Y h                hh I  H       
// match      :  |  |     +        |  |  |        |  |  | +                ++ +  | XXXXXbad partXXXXX
// protein    :  D  E  Q  S  F --- T  G  K  E  Y  S  K  Y y.....intron.....yy L  H  D  T  S  T  E  G 
//
// there are no "<skip>", "intron", or "bad part" in actual values
class NCBI_XALNMGR_EXPORT CProteinAlignText {
public:
    static const char GAP_CHAR; // used in dna and protein text
    static const char SPACE_CHAR; // translation and protein
    static const char INTRON_CHAR; // protein
    static const char INTRON_OR_GAP[];

    // used in match text
    static const char BAD_PIECE_CHAR;
    static const char MISMATCH_CHAR;
    static const char BAD_OR_MISMATCH[];
    static const char MATCH_CHAR;
    static const char POSIT_CHAR;

    CProteinAlignText(objects::CScope& scope, const objects::CSeq_align& seqalign, const string& matrix_name = "BLOSUM62");
    ~CProteinAlignText();

    const string& GetDNA() const { return m_dna; }
    const string& GetTranslation() const { return m_translation; }
    const string& GetMatch() const { return m_match; }
    const string& GetProtein() const { return m_protein; }

    static CRef<objects::CSeq_loc> GetGenomicBounds(objects::CScope& scope,
                                           const objects::CSeq_align& seqalign);

    static char TranslateTriplet(const objects::CTrans_table& table,
                                 const string& triplet);

private:
    string m_dna;
    string m_translation;
    string m_match;
    string m_protein;

    const objects::CTrans_table* m_trans_table;
    SNCBIFullScoreMatrix m_matrix;

    void AddDNAText(objects::CSeqVector_CI& genomic_ci, int& nuc_prev, size_t len);
    void TranslateDNA(int phase, size_t len, bool is_insertion);
    void AddProtText(objects::CSeqVector_CI& protein_ci, int& prot_prev, size_t len);
    void MatchText(size_t len, bool is_match=false);
    char MatchChar(size_t i);
    void AddHoleText(bool prev_3_prime_splice, bool cur_5_prime_splice,
                     objects::CSeqVector_CI& genomic_ci, objects::CSeqVector_CI& protein_ci,
                     int& nuc_prev, int& prot_prev,
                     int nuc_cur_start, int prot_cur_start);
    void AddSpliceText(objects::CSeqVector_CI& genomic_ci, int& nuc_prev, char match);
};

END_NCBI_SCOPE


#endif
