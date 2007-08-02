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
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CScope;
    class CSeq_id;
    class CSeq_loc;
    class CSeq_align;
END_SCOPE(objects)
USING_SCOPE(ncbi::objects);

class NCBI_XALGOALIGN_EXPORT CProSplignScoring: public CObject
{
public:
    /// creates scoring parameter object with default values
    CProSplignScoring();


    CProSplignScoring& SetMinIntronLen(int);
    int GetMinIntronLen() const;

    ///  in addition to BLOSSOM62 prosplign uses following costs (negate to get a score)

    CProSplignScoring& SetGapOpeningCost(int);
    int GetGapOpeningCost() const;

    CProSplignScoring& SetGapExtensionCost(int); // for one aminoacid (three bases)
    int GetGapExtensionCost() const;

    CProSplignScoring& SetFrameshiftOpeningCost(int);
    int GetFrameshiftOpeningCost() const;

    CProSplignScoring& SetGTIntronCost(int); //GT/AG intron opening cost
    int GetGTIntronCost() const;
    CProSplignScoring& SetGCIntronCost(int); //GC/AG intron opening cost
    int GetGCIntronCost() const;
    CProSplignScoring& SetATIntronCost(int); //AT/AC intron opening cost
    int GetATIntronCost() const;

    CProSplignScoring& SetNonConsensusIntronCost(int); // should not exceed a sum of lowest two intron opening costs,
    int GetNonConsensusIntronCost() const;             // i.e. intron_non_consensus<=intron_GT+intron_GC

    CProSplignScoring& SetInvertedIntronExtensionCost(int); // intron_extension cost for 1 base = 1/(inverted_intron_extension*3)
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

/// ProSplign always makes a global alignment,
/// i.e. it aligns the whole protein no matter how bad some parts of this alignment might be.
/// Usually we don't want the bad pieces and remove them.
/// The following parameters define good parts.
class NCBI_XALGOALIGN_EXPORT CProSplignOutputOptions: public CObject
{
public:
    enum EMode {
        eWithHoles, // default filtering parameters
        ePassThrough, // all zeroes - no filtering
    };

    CProSplignOutputOptions(EMode mode = eWithHoles);

    bool IsPassThrough() const;

    CProSplignOutputOptions& SetEatGaps(bool); // if possible, do not output frame-preserving gaps, output frameshifts only
    bool GetEatGaps() const;

    CProSplignOutputOptions& SetFlankPositives(int); // any length flank of a good piece should not be worse than this percentage threshold
    int GetFlankPositives() const;
    CProSplignOutputOptions& SetTotalPositives(int); // good piece total percentage threshold
    int GetTotalPositives() const;

    /// any part of a good piece longer than max_bad_len should not be worse than min_positives
    CProSplignOutputOptions& SetMaxBadLen(int); // maximum 'bad' part length (in nucleotide bases) inside 'good' piece
    int GetMaxBadLen() const;
    CProSplignOutputOptions& SetMinPositives(int); // internal bad part percentage threshold
    int GetMinPositives() const;

    CProSplignOutputOptions& SetMinFlankingExonLen(int); // minimum number of bases in the first and last exon
    int GetMinFlankingExonLen() const;
    CProSplignOutputOptions& SetMinGoodLen(int); // good piece should not be shorter than that 
    int GetMinGoodLen() const;

    CProSplignOutputOptions& SetStartBonus(int); // reward (in # of positives?) for start codon match
    int GetStartBonus() const;
    CProSplignOutputOptions& SetStopBonus(int); // reward for stop codon at the end
    int GetStopBonus() const;

public:
    static const bool default_eat_gaps = true;

    static const int default_flank_positives = 55;
    static const int default_total_positives = 70;

    static const int default_max_bad_len = 45;
    static const int default_min_positives = 15;

    static const int default_min_flanking_exon_len = 10;
    static const int default_min_good_len = 59;

    static const int default_start_bonus = 8; /// ???
    static const int default_stop_bonus = 8; /// ???

private:
    bool eat_gaps;
    int flank_positives;
    int total_positives;
    int max_bad_len;
    int min_positives;
    int min_flanking_exon_len;
    int min_good_len;
    int start_bonus;
    int stop_bonus;
};

class NCBI_XALGOALIGN_EXPORT CProSplign: public CObject
{
public:

    CProSplign( CProSplignScoring scoring = CProSplignScoring() );
    ~CProSplign();

    /// By default ProSplign looks for introns.
    /// Set intronless mode for protein to mRNA alignments, many viral genomes, etc.
    void SetIntronlessMode(bool intronless);
    bool GetIntronlessMode() const;

    /// Returns Spliced-seg
    CRef<CSeq_align> FindAlignment(CScope& scope, const CSeq_id& protein, const CSeq_loc& genomic, 
                                   CProSplignOutputOptions output_options = CProSplignOutputOptions());

    // deprecated internals
    void SetMode(bool one_stage, bool just_second_stage, bool old);
    const vector<pair<int, int> >& GetExons() const;
    vector<pair<int, int> >& SetExons();
    void GetFlanks(bool& lgap, bool& rgap) const;
    void SetFlanks(bool lgap, bool rgap);

private:
    struct SImplData;
    auto_ptr<SImplData> m_data;
    
    /// forbidden
    CProSplign(const CProSplign&);
    CProSplign& operator=(const CProSplign&);
};


END_NCBI_SCOPE


#endif
