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

class NCBI_SNPUTIL_EXPORT NSnpBins
{
public:
    enum EBinType {
        eUnknown = -1,
        eGAP = 0,   // dbGaP analysis files
        eGCAT= 1,   // NHGRI GWAS Catalog Track (AKA Association Results)
        eCLIN= 2,   // Clinical Variations
        eCITED=3,   // Cited Variations
        eIND = 4    // individual tracks (Venter, Watson, etc)
    };
	typedef int TBinType;

	// association result source
	enum ESource {
		eSource_dbGAP		= 1,
		eSource_NHGRI_GWAS	= 2,
		eSource_NHLBI_GRASP	= 3
	};
	typedef int TSource;

	struct SBinEntry : public CObject
	{
		//!! arrange member names as in the dumped file
		TSeqPos pos;
		unsigned int snpid;
		double  pvalue;
		string  pmids;
		string  trait;
		string  genes_reported;
		string  genes_mapped;
		int     ClinSigID;
		string  sHGVS;
		string  dbgaptext;
		string  context;
		TSource source;
	};

	typedef list<CRef<SBinEntry> >  TBinEntryList;
	typedef set<string>        TListNaas;

	struct SBin : public CObject
	{
		TBinType type;
		int    count;
		string title;
		TSeqRange range;

		CRef<SBinEntry>  m_SigEntry;  // most significant entry in bin region
		TBinEntryList m_EntryList;
		string signature;
	};

	// storage for gene maps as  in strings in SBinEntry::genes_reported and SBinEntry::genes_mapped
	class NCBI_SNPUTIL_EXPORT CGeneMap
	{
	public:
		// expects a string like "GeneSym1^GeneID1:GeneSym2^geneID2:etc"
		CGeneMap(const string& sSrc = "") { x_Init(sSrc); }
		void Set(const string& sSrc) { x_Init(sSrc); }

		// recreate the string (sorting of genes may be different compared to the original; duplicates will be culled)
		string AsString() const;

		// key is gene symbol, value is gene id
		typedef map<string, string> TGeneMap;
		typedef TGeneMap::const_iterator const_iterator;

		// const iterators
		const_iterator begin() const { return m_GeneMap.begin(); }
		const_iterator end() const { return m_GeneMap.end(); }

	private:
		friend bool operator==(const CGeneMap& GeneMap1, const CGeneMap& GeneMap2);
		void x_Init(const string& sSrc);
		TGeneMap m_GeneMap;
	};



public:
	// get a selector for a bin given a NA track accession with some selector parameters
	static void GetBinSelector(const string& sTrackAccession,
	                           bool isAdaptive,
	                           int depth,
	                           objects::SAnnotSelector& sel);

    // get an annotation handle that is needed to load a bin from an existing selector and loc and bioseq handle
	// returns false if a handle cannot be obtained
    static bool GetBinHandle(objects::CScope& scope,
                             const objects::SAnnotSelector& sel,
                             const objects::CSeq_loc &loc,
                             objects::CSeq_annot_Handle& annot);

	// get a singular bin corresponding to a position range
    static CRef<SBin> GetBin(const objects::CSeq_annot_Handle& annot, TSeqRange range);

    // get a bin entry corresponding to a row position in the table presumed contained within the handle
	// the reference will be null if there is no valid entry for this row
	static CRef<SBinEntry> GetEntry(const objects::CSeq_annot_Handle& annot, int row);

	// choose a more significant entry of the two offered
	// returns 1 of entry1 is more significant or 2 if entry2 is more
	static int ChooseSignificant(const SBinEntry* entry1, const SBinEntry* entry2, TBinType type);

	static void ReadAnnotDesc(const objects::CSeq_annot_Handle& handle,
						 string& title,
						 string& comment);

	// human-readable text for various source types
	static string SourceAsString(TSource Source);

	// Performs iterative binary search to find table indexes 'pos_index_begin' and 'pos_index_end' in a table with
	// "pos" values.
    // pos_index_end points at one index greater than the last item found in range.
	// annot is supposed to contain a table with a sorted "pos" column (may contain
	// duplicate entries), otherwise
    // an exception will be thrown or the algorithm will fail
    static void FindPosIndexRange(const objects::CSeq_annot_Handle& annot,
                          int pos_value_from, int pos_value_to,
                          int& pos_index_begin, int& pos_index_end);
};

// compare two GeneMaps, return true if they have the same set of elements
inline bool operator==(const NSnpBins::CGeneMap& GeneMap1, const NSnpBins::CGeneMap& GeneMap2)
	{
    if(GeneMap1.m_GeneMap.size() != GeneMap2.m_GeneMap.size())
        return false;

    return equal(GeneMap1.m_GeneMap.begin(), GeneMap1.m_GeneMap.end(), GeneMap2.m_GeneMap.begin());
}

inline bool operator!=(const NSnpBins::CGeneMap& GeneMap1, const NSnpBins::CGeneMap& GeneMap2)
{
    return !(GeneMap1 == GeneMap2);
}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SNP_UTIL___SNP_BINS__HPP

