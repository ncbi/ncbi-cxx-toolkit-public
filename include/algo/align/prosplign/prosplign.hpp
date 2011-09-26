#ifndef ALGO_ALIGN_PROSPLIGN__HPP
#define ALGO_ALIGN_PROSPLIGN__HPP

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
* Author:  Boris Kiryutin (prosplign algorithm and implementation)
* Author:  Vyacheslav Chetvernin (this adapter)
*
* File Description:
*   CProSplign class definition
*   spliced protein to genomic sequence alignment
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objmgr/seq_vector_ci.hpp>

#include <list>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CScope;
END_SCOPE(objects)

/// Scoring parameters object
class NCBI_XALGOALIGN_EXPORT CProSplignOptions_Base: public CObject
{
public:
    static void SetupArgDescriptions(CArgDescriptions* argdescr);

    /// creates scoring parameter object with default values
    CProSplignOptions_Base();
    CProSplignOptions_Base(const CArgs& args);

    CProSplignOptions_Base& SetScoreMatrix(const string& matrix_name);
    const string& GetScoreMatrix() const;

    CProSplignOptions_Base& SetAltStarts(bool allow_alt_start);
    bool GetAltStarts() const;

    static const string default_score_matrix_name; // BLOSUM62
    static const bool default_allow_alt_starts;

private:
    string score_matrix_name;
    bool allow_alt_starts;
};

class NCBI_XALGOALIGN_EXPORT CProSplignScoring: public CProSplignOptions_Base
{
public:
    static void SetupArgDescriptions(CArgDescriptions* argdescr);

    /// creates scoring parameter object with default values
    CProSplignScoring();

    CProSplignScoring(const CArgs& args);


    CProSplignScoring& SetMinIntronLen(int);
    int GetMinIntronLen() const;


    ///  in addition to ScoreMatrix prosplign uses following costs (negate to get a score)

    CProSplignScoring& SetGapOpeningCost(int);
    int GetGapOpeningCost() const;

    /// Gap Extension Cost for one aminoacid (three bases)
    CProSplignScoring& SetGapExtensionCost(int);
    int GetGapExtensionCost() const;

    CProSplignScoring& SetFrameshiftOpeningCost(int);
    int GetFrameshiftOpeningCost() const;

    /// GT/AG intron opening cost
    CProSplignScoring& SetGTIntronCost(int);
    int GetGTIntronCost() const;
    /// GC/AG intron opening cost
    CProSplignScoring& SetGCIntronCost(int);
    int GetGCIntronCost() const;
    ///AT/AC intron opening cost
    CProSplignScoring& SetATIntronCost(int);
    int GetATIntronCost() const;

    /// Non Consensus Intron Cost
    /// should not exceed a sum of lowest two intron opening costs,
    /// i.e. intron_non_consensus cost <= intron_GT cost + intron_GC cost
    CProSplignScoring& SetNonConsensusIntronCost(int);
    int GetNonConsensusIntronCost() const;            

    /// Inverted Intron Extension Cost 
    /// intron_extension cost for 1 base = 1/(inverted_intron_extension*3)
    CProSplignScoring& SetInvertedIntronExtensionCost(int);
    int GetInvertedIntronExtensionCost() const;

public:
    static const int default_min_intron_len = 30;

    static const int default_gap_opening  = 10;
    static const int default_gap_extension = 1;
    static const int default_frameshift_opening = 30;

    static const int default_intron_GT = 15;
    static const int default_intron_GC = 20;
    static const int default_intron_AT = 25;
    static const int default_intron_non_consensus = 34;
    static const int default_inverted_intron_extension = 1000;

private:
    int min_intron_len;
    int gap_opening;
    int gap_extension;
    int frameshift_opening;
    int intron_GT;
    int intron_GC;
    int intron_AT;
    int intron_non_consensus;
    int inverted_intron_extension;
};

/// Output filtering parameters
///
/// ProSplign always makes a global alignment,
/// i.e. it aligns the whole protein no matter how bad some parts of this alignment might be.
/// Usually we don't want the bad pieces and remove them.
/// The following parameters define good parts.
class NCBI_XALGOALIGN_EXPORT CProSplignOutputOptions: public CProSplignOptions_Base
{
public:
    enum EMode {
        /// default filtering parameters
        eWithHoles,
        /// all zeroes - no filtering
        ePassThrough,
    };

    static void SetupArgDescriptions(CArgDescriptions* argdescr);

    CProSplignOutputOptions(EMode mode = eWithHoles);
    CProSplignOutputOptions(const CArgs& args);

    bool IsPassThrough() const;

    /// cut partial codons and adjecent at the beginning and at the end of an alignment
    /// called at the end of post processing
    CProSplignOutputOptions& SetCutFlankPartialCodons(bool);
    bool GetCutFlankPartialCodons() const;

    /// any length flank of a good piece should not be worse than this percentage threshold
    CProSplignOutputOptions& SetFlankPositives(int);
    int GetFlankPositives() const;
    /// good piece total percentage threshold
    CProSplignOutputOptions& SetTotalPositives(int);
    int GetTotalPositives() const;

    /// any part of a good piece longer than max_bad_len should not be worse than min_positives
    CProSplignOutputOptions& SetMaxBadLen(int);
    int GetMaxBadLen() const;
    CProSplignOutputOptions& SetMinPositives(int);
    int GetMinPositives() const;

    /// minimum exon identity
    CProSplignOutputOptions& SetMinExonId(int);
    int GetMinExonId() const;
    /// minimum exon positives percentage
    CProSplignOutputOptions& SetMinExonPos(int);
    int GetMinExonPos() const;

    /// minimum number of bases in the first and last exon
    CProSplignOutputOptions& SetMinFlankingExonLen(int);
    int GetMinFlankingExonLen() const;
    /// good piece should not be shorter than that 
    CProSplignOutputOptions& SetMinGoodLen(int);
    int GetMinGoodLen() const;



    /// reward (in # of positives?) for start codon match. 
    CProSplignOutputOptions& SetStartBonus(int);
    int GetStartBonus() const;
    /// reward for stop codon at the end. Not implemented yet
    CProSplignOutputOptions& SetStopBonus(int);
    int GetStopBonus() const;

public:
    static const bool default_cut_flank_partial_codons = true;

    static const int default_flank_positives = 55;
    static const int default_total_positives = 70;

    static const int default_max_bad_len = 45;
    static const int default_min_positives = 15;

    static const int default_min_exon_id = 30;
    static const int default_min_exon_pos = 55;

    static const int default_min_flanking_exon_len = 15;
    static const int default_min_good_len = 59;

    static const int default_start_bonus = 8; /// ???
    static const int default_stop_bonus = 8; /// ???

private:
    bool cut_flank_partial_codons;
    int flank_positives;
    int total_positives;
    int max_bad_len;
    int min_positives;
    int min_exon_id;
    int min_exon_pos;
    int min_flanking_exon_len;
    int min_good_len;
    int start_bonus;
    int stop_bonus;
};

class CProSplignText;

/// spliced protein to genomic alignment
///
class NCBI_XALGOALIGN_EXPORT CProSplign: public CObject
{
public:

    /// By default ProSplign looks for introns.
    /// Set intronless mode for protein to mRNA alignments, many viral genomes, etc.
    CProSplign( CProSplignScoring scoring = CProSplignScoring(), bool intronless=false );
    ~CProSplign();

    /// Aligns protein to a region on genomic sequence.
    /// genomic seq_loc should be a continuous region - an interval or a whole sequence
    ///
    /// Returns Spliced-seg
    CRef<objects::CSeq_align>
    FindAlignment(objects::CScope& scope,
                  const objects::CSeq_id& protein,
                  const objects::CSeq_loc& genomic, 
                  CProSplignOutputOptions output_options = CProSplignOutputOptions())
    {
        CRef<objects::CSeq_align> align_ref;
        align_ref = FindGlobalAlignment(scope, protein, genomic);
        align_ref = RefineAlignment(scope, *align_ref, output_options);
        return align_ref;
    }

    /// Globally aligns protein to a region on genomic sequence.
    /// genomic seq_loc should be a continuous region - an interval or a whole sequence
    ///
    /// Returns Spliced-seg
    CRef<objects::CSeq_align>
    FindGlobalAlignment(objects::CScope& scope,
                        const objects::CSeq_id& protein,
                        const objects::CSeq_loc& genomic);

    /// Refines Spliced-seg alignment by removing bad pieces according to output_options.
    /// This is irreversible action - more relaxed parameters will not change the alignment back
    CRef<objects::CSeq_align>
    RefineAlignment(objects::CScope& scope,
                    const objects::CSeq_align& seq_align,
                    CProSplignOutputOptions output_options = CProSplignOutputOptions());

    /// TEMPRARY TEST VERSION. BASED ON SCORES, BLAST STYLE
    /// Refines Spliced-seg alignment by removing bad pieces according to output_options.
    /// This is irreversible action - more relaxed parameters will not change the alignment back
    CRef<objects::CSeq_align>
    BlastAlignment(objects::CScope& scope,
                   const objects::CSeq_align& seq_align, int score_cutoff, int score_dropoff);


    /// deprecated internals
    CProSplign( CProSplignScoring scoring, bool intronless, bool one_stage, bool just_second_stage, bool old);
    const vector<pair<int, int> >& GetExons() const;
    vector<pair<int, int> >& SetExons();
    void GetFlanks(bool& lgap, bool& rgap) const;
    void SetFlanks(bool lgap, bool rgap);

public:
    class CImplementation;
private:
    auto_ptr<CImplementation> m_implementation;
    
private:
    /// forbidden
    CProSplign(const CProSplign&);
    CProSplign& operator=(const CProSplign&);
};

/// Text representation of ProSplign alignment
// dna        : GATGAAACAGCACTAGTGACAGGTAAA----GATCTAAATATCGTTGA<skip>GGAAGACATCCATTGGCAATGGCAATGGCAT
// translation:  D  E  T  A  L  V  T  G  K        S  K  Y h                hh I  H       
// match      :  |  |     +        |  |  |        |  |  | +                ++ +  | XXXXXbad partXXXXX
// protein    :  D  E  Q  S  F --- T  G  K  E  Y  S  K  Y y.....intron.....yy L  H  D  T  S  T  E  G 
//
// there are no "<skip>", "intron", or "bad part" in actual values
class CProSplignText {
public:
    /// Outputs formatted text
    static void Output(const objects::CSeq_align& seqalign, objects::CScope& scope, ostream& out, int width, const string& matrix_name = "BLOSUM62");
};

END_NCBI_SCOPE


#endif
