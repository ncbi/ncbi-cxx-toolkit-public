#ifndef ALGO_BLAST_API__CDD_PSSM_INPUT__HPP
#define ALGO_BLAST_API__CDD_PSSM_INPUT__HPP

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
 * Author:  Greg Boratyn
 *
 */

/** @file cdd_pssm_input.hpp
 * Defines a concrete strategy to obtain PSSM input data for PSI-BLAST.
 */

#include <corelib/ncbiobj.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/psi_pssm_input.hpp>
#include <algo/blast/api/rps_aux.hpp>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_psi.h>

#include <objects/seqloc/Seq_id.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>


/// Forward declaration for unit test classes
class CPssmEngineTest;
class CPssmCddInputTest;

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

// Forward declarations in objects scope
BEGIN_SCOPE(objects)
#ifndef SKIP_DOXYGEN_PROCESSING
    class CSeq_align_set;
    class CDense_seg;
    class CScope;
#endif /* SKIP_DOXYGEN_PROCESSING */
END_SCOPE(objects)

BEGIN_SCOPE(blast)


/// Interface for strategy to pre-process multiple alignment of conserved
/// domains matches as input data for PSSM engine
class IPssmInputCdd : public IPssmInput_Base
{
public:
    virtual ~IPssmInputCdd() {}

    /// Get CD data for PSSM computation
    virtual PSICdMsa* GetData(void) = 0;

    /// Get CDD-related PSI-BLAST options
    virtual const PSIBlastOptions* GetOptions(void) = 0;

    /// Get diagnostics options
    virtual const PSIDiagnosticsRequest* GetDiagnosticsRequest(void)
    {return NULL;}

    /// Pre-process CDs used for PSSM computation
    virtual void Process(void) = 0;
};


/// Strategy for pre-processing RPS-BLAST matches for PSSM computation. Builds
/// multuiple alignment of CDs and retrieves weighted residue frequencies and
/// effective number of observations for each CD matching to a query.
class NCBI_XBLAST_EXPORT CCddInputData : public IPssmInputCdd
{
public:
    /// Type used for residue frequencies stored in CDD
    typedef Uint4 TFreqs;

    /// Type used for number of independent observations stored in CDD
    typedef Uint4 TObsr;

public:

    /// Constructor
    /// @param query Query sequence in ncbistdaa encoding [in]
    /// @param query_length Query sequence length [in]
    /// @param seqaligns List of pair-wise alignments of query to CDs.
    /// PSSM will be computed based on those alignments [in]
    /// @param opts CD-related PSSM engine options [in]
    /// @dbname Name of CDD database used for searching matches to query. The
    /// alignments in seqaligns come from searching this database [in]
    /// @param matrix_name Name of the scoring system [in]
    /// @param gap_existence Gap opening cost, if zero default from
    /// IPssmInput_Base will be used [in]
    /// @param gap_extension Gap extension cost, if zero default from
    /// IPssmInput_Base will be used [in]
    /// @param diags Options for pssm engine diagnostics [in]
    ///
    CCddInputData(const Uint1* query, unsigned int query_length,
                  CConstRef<objects::CSeq_align_set> seqaligns,
                  const PSIBlastOptions& opts,
                  const string& dbname,
                  const string& matrix_name = "BLOSUM62",
                  int gap_existence = 0,
                  int gap_extension = 0,
                  PSIDiagnosticsRequest* diags = NULL,
                  const string& query_title = "");

    /// Virtual destructor
    virtual ~CCddInputData();


    /// Get query sequence
    /// @return Query sequence
    ///
    unsigned char* GetQuery(void) {return &m_QueryData[0];}

    /// Get query length
    /// @return Query length
    ///
    unsigned int  GetQueryLength(void) {return m_QueryData.size();}

    /// Get scoring matrix name
    /// @return Scoring matrix name
    ///
    const char* GetMatrixName(void) {return m_MatrixName.c_str();}

    /// Get options for PSSM engine diagnostics
    /// @return PSSM engine diagnostics request
    ///
    const PSIDiagnosticsRequest* GetDiagnosticsRequest(void)
    {return m_DiagnosticsRequest;}

    /// Get data for PSSM computation
    /// @return Data for PSSM computation
    ///
    PSICdMsa* GetData(void) {return &m_CddData;}

    /// Get CDD-related PSSM engine options
    /// @return CDD-related PSSM engine options
    ///
    const PSIBlastOptions* GetOptions(void) {return &m_Opts;}

    /// Pre-process CD matches to query. Make multiple alignment of CDs
    /// matching the query. Get the weghted counts and effective numbers of
    /// observations for each match.
    ///
    void Process(void);


    /// Set minimum e-value threshold for inclusion of CDs in PSSM computation.
    /// The minimum e-value is used only for experimental purposes
    /// hence it does not appear in CddOptions
    /// @param val Minimum e-value threshold [in]
    ///
    void SetMinCddEvalue(double val) {m_MinEvalue = val;}

    /// Get query as Bioseq
    /// @return Query as bioseq
    ///
    CRef<objects::CBioseq> GetQueryForPssm(void) {return m_QueryBioseq;}

    /// Get gap existence to use when building PSSM
    /// @return Gap existence
    ///
    int GetGapExistence(void)
    {return m_GapExistence ? m_GapExistence : IPssmInputCdd::GetGapExistence();}

    /// Get gap extension to use when building PSSM
    /// @return Gap extension
    ///
    int GetGapExtension(void)
    {return m_GapExtension ? m_GapExtension : IPssmInputCdd::GetGapExtension();}
    

protected:

    typedef CRange<int> TRange;

    /// Represents one alignment segment of a RPS-BLAST hit
    ///
    class CHitSegment {
    public:

        /// Constructor
        /// @param q Segment range on query [in]
        /// @param s Segment range on subject [i]
        CHitSegment(TRange q, TRange s)
            : m_QueryRange(q), m_SubjectRange(s) {}

        /// Copy constructor. MSA data is not copied.
        /// @apram seg Hit segment [in]
        CHitSegment(const CHitSegment& seg)
            : m_QueryRange(seg.m_QueryRange),
              m_SubjectRange(seg.m_SubjectRange)
        {}

        /// Allocate and populate arrays for MSA data (weighted residue counts
        /// and effective observations used for PSSM computation)
        /// @param db_oid Subject index in CDD database [in]
        /// @param profile_data Object accessing data int CDD [in]
        void FillData(int db_oid, const CBlastRPSInfo& profile_data);

        /// Validate hit segment
        /// @return True if hit segment is valid, false otherwise
        ///
        /// Check whether range on query and subject have the same lengths,
        /// MSA data is set and corresponds to subject range
        bool Validate(void) const;

        /// Does the hit segment represent an empty range
        /// @return True if hit segment empty, false otherwise
        bool IsEmpty(void) const;

        /// Change ranges on query and subject by given values
        /// @param d_from Change value for the beginning of the segment [in]
        /// @param d_to Change value for the end of the segment [in]
        ///
        /// d_from and d_to are added to begining and end of both query and
        /// subject ranges. d_from and d_to can be negative.
        void AdjustRanges(int d_from, int d_to);

        /// Get length of the hit segment in residues
        /// @return Segment length
        int GetLength(void) const {return m_QueryRange.GetLength();}
        
    private:
        /// Fobidding assignment operator
        CHitSegment& operator=(const CHitSegment& hit);

        /// Populate arrays of weighted residue counts
        /// @param db_oid Subject index in CDD [in]
        /// @param profile_data Object accessing data in CDD [in]
        void x_FillResidueCounts(int db_oid, const CBlastRPSInfo& profile_data);

        /// Populate arrays of effective numbers of observations
        /// @param db_oid Subject index in CDD [in]
        /// @param profile_data Object accessing data in CDD [in]
        void x_FillObservations(int db_oid, const CBlastRPSInfo& profile_data);


    public:
        /// Segment range on query
        TRange m_QueryRange;

        /// Segment range on subject
        TRange m_SubjectRange;

        /// Data used for PSSM computation
        vector<PSICdMsaCellData> m_MsaData;

    private:
        /// Buffer for residue frequencies from CDs
        vector<double> m_WFreqsData;
    };

    
    /// Single RPS-BLAST hit
    class CHit {
    public:
        /// Master selection for operations involving ranges
        enum EApplyTo {eQuery = 0, eSubject};

    public:

        /// Constructor
        /// @param denseg Alignment as Dense_seg [in]
        /// @param evalue E-value [in]
        CHit(const objects::CDense_seg& denseg, double evalue);

        /// Copy constructor
        /// @param hit Hit [in]
        CHit(const CHit& hit);

        /// Destructor
        ~CHit();

        /// Allocate and populate arrays of data for PSSM computation
        /// @param seqdb Object accessing CDD [in]
        /// @param profile_data Object accssing data in CDD [in]
        void FillData(const CSeqDB& seqdb,
                      const CBlastRPSInfo& profile_data);

        /// Intersect hit segments with list of ranges and store result in hit
        /// segments.
        /// @param segments List of ranges [in]
        /// @param app Specifies whether input ranges are to be intersected
        /// with query or subject ranges [in]
        void IntersectWith(const vector<TRange>& segments,
                           EApplyTo app);

        /// Intersect hit with another hit and store result
        /// @param hit Hit [in]
        /// @param app Specifies whether intersection is to be perfromed on
        /// query or subject ranges [in]
        void IntersectWith(const CHit& hit, EApplyTo app);

        /// Subtract from another hit from this hit using query ranges
        /// @param hit Hit to subtract [in]
        ///
        void Subtract(const CHit& hit);

        /// Validate hit
        /// @return True if hit is valid, false otherwise
        bool Validate(void) const;

        /// Is hit empty
        /// @return True if the hit is empty, false otherwise
        ///
        /// The hit is considered non-empty if it contains at least one
        /// non-empty range
        bool IsEmpty(void) const;

        /// Get hit length in residues, counts number of matching residues,
        /// gaps are not counted
        /// @return Length
        int GetLength(void) const;

        /// Get hit segments
        /// @return Reference to hit segments
        vector<CHitSegment*>& GetSegments(void) {return m_SegmentList;}

        /// Get hit segments
        /// @return Reference to hit segments
        const vector<CHitSegment*>& GetSegments(void) const
        {return m_SegmentList;}
        
        // TO DO: Add interface functions for the attributes below and make
        // them private

        /// Seq_id of hit subject
        CConstRef<CSeq_id> m_SubjectId;

        /// E-value
        double m_Evalue;

        /// Hit index in MSA table
        int m_MsaIdx;

    private:
        /// List of hit segments
        vector<CHitSegment*> m_SegmentList;
    };


    /// Class used for sorting hit segments by range
    class compare_hitseg_range {
    public:
        bool operator()(const CHitSegment* a, const CHitSegment* b) const {

            // assuming that ranges are mutually exclusive
            return a->m_SubjectRange.GetFrom() < b->m_SubjectRange.GetFrom();
        }
    };

    /// Class used for sorting ranges
    class compare_range {
    public:
        bool operator()(const TRange& a, const TRange& b) const {
            if (a.GetFrom() == b.GetFrom()) {
                return a.GetTo() < b.GetTo();
            }
            else {
                return a.GetFrom() < b.GetFrom();
            }
        }
    };

    /// Class used for sorting hits by subject seq-id and e-value
    class compare_hits_by_seqid_eval {
    public:
        bool operator()(CHit* const& a, CHit* const& b)
        {
            if (a->m_SubjectId->Match(*b->m_SubjectId)) {
                return a->m_Evalue < b->m_Evalue;
            }
            else {
                return *a->m_SubjectId < *b->m_SubjectId;
            }
        }
    };


protected:

    /// Process RPS-BLAST hits. Convert hits to internal representation.
    /// @param min_evalue Min e-value for inclusion of CD hit in PSSM
    /// calculation [in]
    /// @param max_evalue Max e-value for inclusion of CD hit in PSSM
    /// calculation [in]
    void x_ProcessAlignments(double min_evalue, double max_evalue);

    /// Read data needed for PSSM computation from CDD and populate arrays
    void x_FillHitsData(void);

    /// Create multiple alignment of CDs
    void x_CreateMsa(void);

    /// Remove multiple hits to the same CD
    ///
    /// For each pair of hits to the same CD the intersection between hits 
    /// in CD ranges is subtracted using query ranges from the CD with worse
    /// e-value
    void x_RemoveMultipleCdHits(void);
    
    /// Validate multiple alignment of CDs
    bool x_ValidateMsa(void) const;

    /// Validate internal representation of RPS-BLAST hits
    bool x_ValidateHits(void) const;

    /// Create query as Bioseq
    void x_ExtractQueryForPssm(void);


private:
    /// Query sequence
    vector<Uint1> m_QueryData;
    /// Query title (for PSSM)
    string m_QueryTitle;


    /// CDD database name
    string m_DbName;

    /// RPS-BLAST hits for the query
    CConstRef<objects::CSeq_align_set> m_SeqalignSet;

    /// RPS-BLAST hits in internal representation
    vector<CHit*> m_Hits;


    /// MSA of CDs and CD data
    PSICdMsa m_CddData;
    /// MSA dimensions, used by PSSM engine
    PSIMsaDimensions m_MsaDimensions;

    /// MSA data
    vector<PSICdMsaCell> m_MsaData;
    /// Pointer to MSA
    PSICdMsaCell** m_Msa;


    /// Delta BLAST options for PSSM Engine
    PSIBlastOptions m_Opts;
    /// Scoring matrix name
    string m_MatrixName;
    /// PSSM engine Diagnostics request
    PSIDiagnosticsRequest* m_DiagnosticsRequest;

    /// Min e-value threshold for all hits to be included in PSSM
    /// computation. This is needed mostly for experiments.
    double m_MinEvalue;

    /// Query as Bioseq
    CRef<objects::CBioseq> m_QueryBioseq;

    /// Gap existence for underlying scoring system
    int m_GapExistence;

    /// Gap extension for underlying scoring system
    int m_GapExtension;

    static const int kAlphabetSize = 28;

    /// Scale of residue frequencies and number of independent observations
    /// stored in CDD
    static const int kRpsScaleFactor = 1000;

    // unit test class
    friend class ::CPssmCddInputTest;
};


END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API__CDD_PSSM_INPUT_HPP */
