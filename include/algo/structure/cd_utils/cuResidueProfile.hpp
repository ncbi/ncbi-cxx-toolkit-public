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
 class ColumnResidueProfile
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
	bool hasRow(int row) const;
	void getResiduesByRow(vector<char>& residues)const;
	unsigned char getResidueByRow(int row);
	bool isAligned(char residue, int row)const;
	bool isAligned(int row)const;
	bool isAllRowsAligned()const;
	void setIndexByConsensus(int col) {m_indexByConsensus = col;};
	int getIndexByConsensus()const {return m_indexByConsensus;};
	int getResidueTypeCount()const { return m_residueTypeCount;}
	typedef pair<int, bool>RowStatusPair;
	typedef multimap<char, RowStatusPair> ResidueRowsMap;

	double calcInformationContent();
 private:
	 ResidueRowsMap::iterator findRow(int row);
	 ResidueRowsMap::const_iterator findRow(int row)const;
	 set<int> m_rows;
	 ResidueRowsMap m_residueRowsMap;

	 static map<char, double> m_backgroundResFreq;
	 static void useDefaultBackgroundResFreq();
	 double getBackgroundResFreq(char res);
	 int m_residueTypeCount;
	 int m_indexByConsensus;
 };

 class ColumnAddress
 {
 public:
	ColumnAddress(int posOnMaster, int gap=0);
	ColumnAddress();
	~ColumnAddress();
	bool operator<(const ColumnAddress& rhs) const;

	int mPos;
	int gap;
 };

 //interface
 class ColumnReader
 {
 public:
	 virtual void read(ColumnResidueProfile& crp) = 0;
 };

class MasterColumnCounter : public ColumnReader
{
public:
	MasterColumnCounter():m_count(0){};
	void read(ColumnResidueProfile& crp) {m_count++; m_seq += crp.getResidueByRow(0); }
	int getCount()const {return m_count;}
	string& getSeq() {return m_seq;} 
private:
	int m_count;
	string m_seq;
};

 class ResidueProfiles
 {
 public:
	static const double FrequencyThreshold;
	ResidueProfiles();
	~ResidueProfiles();
	void addOneRow(BlockModelPair& bmp, const string& mSeq, const string& sSeq);
	void calculateRowWeights();
	const string& makeConsensus();
	//inNcbieaa=false, return string in ncbistdaa
	const string getConsensus(bool inNcbieaa=true) ;
	const BlockModelPair& getGuideAlignment() const;
	BlockModelPair& getGuideAlignment();
	int countColumnsOnMaster(string& seq);
	void traverseAllColumns(ColumnReader& cr);
	void traverseColumnsOnMaster(ColumnReader& cr);
	void traverseColumnsOnConsensus(ColumnReader& cr);
	void traverseAlignedColumns(ColumnReader& cr);
	int getNumRows()const {return m_totalRows;}
	double calcInformationContent(bool byConsensus=true);

 private:
	 
	int m_totalRows;
	//double m_rowWeightsSum;
	//col address vs. ColResidueProfile
	typedef map<ColumnAddress, ColumnResidueProfile> PosProfileMap;
	PosProfileMap m_profiles;
	vector<double> m_rowWeights;

	//results
	string m_consensus;
	BlockModelPair m_guideAlignment;
 };

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/04/19 14:28:01  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */
