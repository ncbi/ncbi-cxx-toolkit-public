#ifndef SNP_UTIL___SNP_BINS__HPP
#define SNP_UTIL___SNP_BINS__HPP

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
 * Authors:  Melvin Quintos, Dmitry Rudnev
 *
 * File Description:
 *
 */

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <util/range.hpp>
#include <list>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/// objects and functions for dealing with variation (historically called SNP) bins
///
/// a bin is a group of all entries of a certain type covering a range of positions on a
/// sequence object
/// see also https://sp.ncbi.nlm.nih.gov/IEB/pdas/dbsnp/SNP Named Annotation/BinTrackRequirements.docx
class NCBI_SNPUTIL_EXPORT NSnpBins
{
public:
	/// types of supported data in bins
    enum EBinType {
        eUnknown = -1,
        eGAP = 0,   ///< dbGaP analysis files
        eGCAT= 1,   ///< NHGRI GWAS Catalog Track (AKA Association Results)
        eCLIN= 2,   ///< Clinical Variations
        eCITED=3,   ///< Cited Variations
        eIND = 4    ///< individual tracks (Venter, Watson, etc), data same as in eCLIN
    };
	typedef int TBinType;

	/// association result source
	enum ESource {
		eSource_dbGAP		= 1,
		eSource_NHGRI_GWAS	= 2,
		eSource_NHLBI_GRASP	= 3
	};
	typedef int TSource;

	/// a single bin entry
	///
	/// some of the fields are specially formatted, descriptions are in
	/// https://sp.ncbi.nlm.nih.gov/IEB/pdas/dbsnp/SNP Named Annotation/BinTrackRequirements.docx
	struct SBinEntry : public CObject
	{
		//!! arrange member names as in the dumped file
		TSeqPos pos;
		unsigned int snpid;
		double  pvalue;
		string  pmids;			///< comma-delimited list of PubMed IDs
		string  trait;
		string  genes_reported;	///< specially formatted, see document
		string  genes_mapped;	///< specially formatted, see document
		int     ClinSigID;		///< clinical significance ID, @sa NSnp::EClinSigID
		string  sHGVS;
		string  dbgaptext;	///< specially formatted, see document
		string  context;
		TSource source;
		string  trackSubType;    ///< used to further differentiate some GWAS/pha tracks (see SV-2201)
		string  population;      ///< population description for GWAS/pha tracks
		TSeqPos pos_end;         ///< gene end when trackSubType is (Gene association)
		int     geneId;          ///< gene ID when trackSubType is (Gene association)
		string  geneName;        ///< gene name when trackSubType is (Gene association)

	};

	typedef list<CRef<SBinEntry> >  TBinEntryList;
	typedef set<string>        TListNaas;

	/// representation of a bin
	struct SBin : public CObject
	{
		TBinType type;
		int    count;	///< number of entries in this bin
		string title;
		TSeqRange range;

		CRef<SBinEntry>  m_SigEntry;  ///< most significant entry in this bin
		TBinEntryList m_EntryList;
		string signature;
	};

	/// storage for gene maps as seen in strings in SBinEntry::genes_reported and SBinEntry::genes_mapped
	class NCBI_SNPUTIL_EXPORT CGeneMap
	{
	public:
		/// expects a string like "GeneSym1^GeneID1:GeneSym2^geneID2:etc"
		CGeneMap(const string& sSrc = "") { x_Init(sSrc); }
		void Set(const string& sSrc) { x_Init(sSrc); }

		/// recreate the string that was used for creation
		///
		/// - sorting of genes may be different compared to the original
		/// - duplicates will be removed
		string AsString() const;

		/// maps of gene symbols to gene ids
		///
		/// key is gene symbol, value is gene id
		typedef map<string, string> TGeneMap;

		/// masquerading as container
		typedef TGeneMap::const_iterator const_iterator;
		const_iterator begin() const { return m_GeneMap.begin(); }
		const_iterator end() const { return m_GeneMap.end(); }

	private:
		friend bool operator==(const CGeneMap& GeneMap1, const CGeneMap& GeneMap2);
		void x_Init(const string& sSrc);
		TGeneMap m_GeneMap;
	};



public:
	/// get a selector for a bin from a NA track accession
	static void GetBinSelector(const string& sTrackAccession,
	                           bool isAdaptive,
	                           int depth,
	                           objects::SAnnotSelector& sel);

    /// get an annotation handle that is needed to load a singular bin on range
	///
	/// @param scope
	///    active scope
	/// @param sel
	///    selector obtained by GetBinSelector()
	/// @param loc
	///   range that the bin will cover
	/// @param annot
	///   will be filled if a handle can be obtained
	/// @return
	///   false if a handle cannot be obtained
    static bool GetBinHandle(objects::CScope& scope,
                             const objects::SAnnotSelector& sel,
                             const objects::CSeq_loc &loc,
                             objects::CSeq_annot_Handle& annot);

	/// get a singular bin corresponding to a position range
	///
	/// @param annot
	///   annotation handle obtained by GetBinHandle()
	/// @param range
	///   range this bin will cover
    static CRef<SBin> GetBin(const objects::CSeq_annot_Handle& annot, TSeqRange range);

    /// get a bin entry corresponding to a row position in the table presumed contained within the handle
	///
	/// @param annot
	///   annotation handle obtained by GetBinHandle()
	/// @param row
	///   row in the annotation table
	/// @return
	///   null if there is no valid entry for this row
	static CRef<SBinEntry> GetEntry(const objects::CSeq_annot_Handle& annot, int row);

	/// choose a more significant entry of the two offered
	/// @return
	///   - 1 of entry1 is more significant
	///   - 2 if entry2 is more significant
	static int ChooseSignificant(const SBinEntry* entry1, const SBinEntry* entry2, TBinType type);

	/// get title and comment out of annot.desc
	///
	/// @note
	///   if the annotation has several descriptions, will get the last one
	static void ReadAnnotDesc(const objects::CSeq_annot_Handle& handle,
						 string& title,
						 string& comment);

	/// get human-readable text for various source types
	static string SourceAsString(TSource Source);

	/// Perform iterative binary search to find table indexes (rows) 'pos_index_begin' and 'pos_index_end' in a table with
	/// values in "pos" column
	///
	/// @param annot
	///   annotation handle obtained by GetBinHandle()
	/// @param pos_value_from
	///   lower boundary for the "pos" value (inclusive)
	/// @param pos_value_to
	///   upper boundary for the "pos" value (exclusive)
	/// @param pos_index_begin
	///   first index (row) which contains the indicated "pos" values
    /// @param pos_index_end
	///   points at one row greater than the last item found (exclusive)
	/// @note
	///   annot is supposed to contain a table with a sorted "pos" column (but may contain
	///   duplicate entries), otherwise
    ///   an exception will be thrown or the algorithm will fail
    static void FindPosIndexRange(const objects::CSeq_annot_Handle& annot,
                          int pos_value_from, int pos_value_to,
                          int& pos_index_begin, int& pos_index_end);

    /// determine whether a string in TrackSubType describes a Gene Marker ("102_1" or "102_3")
    static bool isGeneMarker(const string& trackSubType) { return trackSubType == "102_1" || trackSubType == "102_3"; }
};

/// compare two GeneMaps, return true if they have the same set of elements
inline bool operator==(const NSnpBins::CGeneMap& GeneMap1, const NSnpBins::CGeneMap& GeneMap2)
	{
    if(GeneMap1.m_GeneMap.size() != GeneMap2.m_GeneMap.size())
        return false;

    return equal(GeneMap1.m_GeneMap.begin(), GeneMap1.m_GeneMap.end(), GeneMap2.m_GeneMap.begin());
}

/// compare two GeneMaps, return true if they do not have the same set of elements
inline bool operator!=(const NSnpBins::CGeneMap& GeneMap1, const NSnpBins::CGeneMap& GeneMap2)
{
    return !(GeneMap1 == GeneMap2);
}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SNP_UTIL___SNP_BINS__HPP

