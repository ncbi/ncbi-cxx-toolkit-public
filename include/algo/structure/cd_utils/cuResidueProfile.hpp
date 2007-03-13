/* $Id$
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
 * Author:  Charlie
 *
 * File Description:
 *
 *       Make consensus and remaster with it
 *
 * ===========================================================================
 */

#ifndef CU_RESIDUE_PROFILE_HPP
#define CU_RESIDUE_PROFILE_HPP

#include <algo/structure/cd_utils/cuCppNCBI.hpp>
#include <algo/structure/cd_utils/cuBlock.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

//on one column
 class NCBI_CDUTILS_EXPORT ColumnResidueProfile
 {
 public:
	static const string m_residues;
	static unsigned char getNcbiStdCode(char eaa);
	static char getEaaCode(char stdCode) {return m_residues[stdCode];}

	 ColumnResidueProfile(); //init occurence to 0
	 ~ColumnResidueProfile();

	 void addOccurence(char residue, int row, bool aligned);
	 inline double calculateColumnWeight(char residue, bool countGap, int numRows)const;
	 //return the total weights for this column, which should be 1
	 double sumUpColumnWeightsByRow(vector<double>& rowWeights, bool countGap, int numRows) const;
	 inline double reweightColumnByRowWeights(const vector<double>& rowWeights, char& heaviestResidue)const;
	 int getSumCount() const;
	char getMostFrequentResidue(int& count) const ;
	//bool hasRow(int row) const;
	void getResiduesByRow(vector<char>& residues, bool byNcbiStd=true)const;
	//residues will be in Ncbistd
	unsigned char getResidueByRow(int row);
	bool isAligned(char residue, int row)const;
	bool isAligned(int row);
	bool isAllRowsAligned()const;
	void setIndexByConsensus(int col) {m_indexByConsensus = col;};
	int getIndexByConsensus()const {return m_indexByConsensus;};
	int getResidueTypeCount()const { return m_residueTypeCount;}
	typedef pair<int, bool>RowStatusPair;
	typedef multimap<char, RowStatusPair> ResidueRowsMap;

	double calcInformationContent();
 private:
	 inline ResidueRowsMap::iterator* findRow(int row);
	// inline ResidueRowsMap::const_iterator* findRow(int row)const;
	 //set<int> m_rows;
	 bool m_masterIn;
	 ResidueRowsMap m_residueRowsMap;
	 //to speed up findRow
     vector<ResidueRowsMap::iterator*> m_residuesByRow;
	 static map<char, double> m_backgroundResFreq;
	 static void useDefaultBackgroundResFreq();
	 double getBackgroundResFreq(char res);
	 int m_residueTypeCount;
	 int m_indexByConsensus;
 };

 class NCBI_CDUTILS_EXPORT ColumnAddress
 {
 public:
	ColumnAddress(int posOnMaster, int gap=0);
	ColumnAddress();
	~ColumnAddress();
	inline bool operator<(const ColumnAddress& rhs) const;

	int mPos;
	int gap;
 };

 //interface
 class NCBI_CDUTILS_EXPORT ColumnReader
 {
 public:
	 virtual void read(ColumnResidueProfile& crp) = 0;
     virtual ~ColumnReader() {};
 };

class NCBI_CDUTILS_EXPORT MasterColumnCounter : public ColumnReader
{
public:
	MasterColumnCounter():m_count(0){};
    virtual ~MasterColumnCounter() {};
	virtual void read(ColumnResidueProfile& crp) {m_count++; m_seq += crp.getResidueByRow(0); }
	int getCount()const {return m_count;}
	string& getSeq() {return m_seq;} 
private:
	int m_count;
	string m_seq;
};

//forward del
class NCBI_CDUTILS_EXPORT UnalignedSegReader : public ColumnReader
{
public:
	typedef pair<int,int> Seg;

	UnalignedSegReader();
	
	void setIndexSequence(string& seq);
	string getIndexSequence();
	void read(ColumnResidueProfile& crp);
	Seg getLongestSeg(); 
	int getLenOfLongestSeg(); 
	int getTotalUnaligned() {return m_totalUnaligned;}
	int getTotal() {return m_pos;}
	int getLongUnalignedSegs(int length, vector<Seg>& segs);
	string subtractLongestSeg(int threshold);
	//string subtractLongSeg(int length);
	string subtractSeg(Seg seg, string& in);
private:
	Seg m_maxSeg;
	Seg m_curSeg;
	vector<Seg> m_unalignedSegs;
	int m_totalUnaligned;
	int m_pos;
	string m_indexSeq;

	int getLen(Seg seg);
};

 class NCBI_CDUTILS_EXPORT ResidueProfiles
 {
 public:
	
	ResidueProfiles();
	~ResidueProfiles();
	void setInclusionThreshold(double th){m_frequencyThreshold = th;};
	void addOneRow(BlockModelPair& bmp, const string& mSeq, const string& sSeq);
	void calculateRowWeights();
	const string& makeConsensus();
	//inNcbieaa=false, return string in ncbistdaa
	const string getConsensus(bool inNcbieaa=true) ;
	const BlockModelPair& getGuideAlignment() const;
	BlockModelPair& getGuideAlignment();
	int countColumnsOnMaster(string& seq);

	void countUnalignedConsensus(UnalignedSegReader& ucr );
	bool skipUnalignedSeg(UnalignedSegReader& ucr, int len);
	void adjustConsensusAndGuide();

	void traverseAllColumns(ColumnReader& cr);
	void traverseColumnsOnMaster(ColumnReader& cr);
	void traverseColumnsOnConsensus(ColumnReader& cr);
	void traverseAlignedColumns(ColumnReader& cr);
	int getNumRows()const {return m_totalRows;}
	double calcInformationContent(bool byConsensus=true);
	const vector< CRef< CSeq_id> > getSeqIdsByRow() const { return m_seqIds;}

     //  Keep track of how many candidate columns failed the weight check against m_frequencyThreshold.
     //  This is useful to know when trying to decide if two adjacent consensus positions are able
     //  to be included in the same block.  If there were failures of the weight check between
     //  those two positions, that means a block can't safely span those two consensus positions
     //  because a) at least one row in the alignment has residues in the failed columns and b) this 
     //  requires a block break to avoid erroneously including those skipped residues (which do not 
     //  map to any column in the consensus) in the block.
     typedef map<int, unsigned int> UnqualForConsMap;
     typedef UnqualForConsMap::iterator UnqualForConsIt;
     typedef UnqualForConsMap::const_iterator UnqualForConsCit;
     unsigned int GetNumUnqualAfterIndex(int index) const;
     bool HasUnqualAfterIndex(int index) const;

 private:
	double m_frequencyThreshold;
	int m_totalRows;
	//double m_rowWeightsSum;
	//col address vs. ColResidueProfile
	typedef map<ColumnAddress, ColumnResidueProfile> PosProfileMap;
	PosProfileMap m_profiles;
	vector<double> m_rowWeights;
	vector< CRef< CSeq_id> > m_seqIds; 

	set<int> m_colsToSkipOnMaster;
	set<int> m_colsToSkipOnConsensus;

     //  Number of candidate columns that did not pass m_frequencyThreshold (map value) 
     //  between consensus index i (map key) and i+1.  Filled in during 'makeConsensus';
     //  adjusted if 'adjustConsensusAndGuide' called after 'makeConsensus'.
     //  (index -1 == before first consensus residue)
     UnqualForConsMap m_numUnqualAfterConsIndex;

	void segsToSet(vector<UnalignedSegReader::Seg>& segs,set<int>& cols);

	//results
	string m_consensus;
	BlockModelPair m_guideAlignment;
 };


END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif
