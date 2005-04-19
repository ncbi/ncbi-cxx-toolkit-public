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
 */
#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuResidueProfile.hpp>
#include <algo/structure/cd_utils/cuUtils.hpp>
#include <algo/blast/core/blast_util.h>
#include  <math.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

// define ColumnResidueProfile
const string ColumnResidueProfile::m_residues = "-ABCDEFGHIKLMNPQRSTVWXYZU*"; //ncbieaa

unsigned char ColumnResidueProfile::getNcbiStdCode(char eaa)
{
	unsigned char  ret = m_residues.find(eaa);
	assert(ret < m_residues.size() && ret >= 0);
	return ret;
}

ColumnResidueProfile::ColumnResidueProfile()
: m_residueTypeCount(0), m_residueRowsMap(), m_rows(), m_indexByConsensus(-1)
 //m_backgroundResFreq()
{
}

ColumnResidueProfile::~ColumnResidueProfile()
{
}

void ColumnResidueProfile::addOccurence(char residue, int row, bool aligned)
{
	//this deals with columns on the master which can be added more than once
	 if (hasRow(row))
	 {
		 /*if (aligned)
		 {
			ResidueRowsMap::iterator rit = findRow(row);
			if (rit != m_residueRowsMap.end())
			{
				rit->second.second = aligned;
			}
		 }*/
		 return;
	 }
	 if (m_residueRowsMap.count(residue) == 0) //first time to see this residue
		 m_residueTypeCount++;
	 m_residueRowsMap.insert(ResidueRowsMap::value_type(residue, RowStatusPair(row, aligned)));
	 m_rows.insert(row);
}

bool ColumnResidueProfile::hasRow(int row) const
{
	return m_rows.find(row) != m_rows.end();
}

ColumnResidueProfile::ResidueRowsMap::iterator ColumnResidueProfile::findRow(int row)
{
	ResidueRowsMap::iterator it = m_residueRowsMap.begin();
	for (; it != m_residueRowsMap.end(); it++)
	{
		if (it->second.first == row)
			return it;
	}
	return m_residueRowsMap.end();
}

ColumnResidueProfile::ResidueRowsMap::const_iterator ColumnResidueProfile::findRow(int row)const
{
	ResidueRowsMap::const_iterator it = m_residueRowsMap.begin();
	for (; it != m_residueRowsMap.end(); it++)
	{
		if (it->second.first == row)
			return it;
	}
	return m_residueRowsMap.end();
}

int ColumnResidueProfile::getSumCount() const
{
	return m_residueRowsMap.size();
}

char ColumnResidueProfile::getMostFrequentResidue(int& count) const
{
	int max = 0;
	count = m_residueRowsMap.count(m_residues[max]);
	for (int i = 1; i < m_residues.size(); i++)
	{
		if (m_residueRowsMap.count(m_residues[i]) > count)
		{
			max = i;
			count = m_residueRowsMap.count(m_residues[i]);
		}
	}
	return m_residues[max];
}
	 
inline double ColumnResidueProfile::calculateColumnWeight(char residue, bool countGap, int numRows)const
{
	if (m_residueRowsMap.count(residue) == 0)
		return 0;
	else
	{
		if (!countGap)
		{
			double  denom = (double)(m_residueTypeCount *  (m_residueRowsMap.count(residue)));
			double w = 1.0/denom;
			return w;
		}
		else
		{
			int numGap = numRows - getSumCount();
			double denom = 0.0;
			if (numGap > 0)
				denom = (double)((m_residueTypeCount+1) *  (m_residueRowsMap.count(residue)));
			else
				denom = (double)(m_residueTypeCount *  (m_residueRowsMap.count(residue)));
				double w = 1.0/denom;
			return w;
		}
	}
}

//return the total weights for this column, which should be 1
double ColumnResidueProfile::sumUpColumnWeightsByRow(vector<double>& rowWeights, bool countGap, int numRows) const
{	
	double total = 0.0;
	ResidueRowsMap::const_iterator cit = m_residueRowsMap.begin();
	set<int> rowsUsed;
	for (; cit != m_residueRowsMap.end(); cit++)
	{
		double colResWeight = calculateColumnWeight(cit->first, countGap, numRows);
		rowWeights[cit->second.first] += colResWeight;
		total += colResWeight;
		rowsUsed.insert(cit->second.first);
	}
	if (countGap && (rowsUsed.size() < numRows))
	{
		double gapWeight = (1.0 - total) / (numRows - rowsUsed.size());
		for (int row = 0; row < numRows; row++)
		{
			if(rowsUsed.find(row) != rowsUsed.end())
				rowWeights[row] += gapWeight;
		}
	}
	return 1.0;
}

inline double ColumnResidueProfile::reweightColumnByRowWeights(const vector<double>& rowWeights, char& heaviestResidue)const
{
	double totalWeight = 0;
	double maxResWeight = 0;
	double resWeight = 0;
	for (int i = 0; i < m_residues.size(); i++)
	{
		//debug
		char aa = m_residues[i];
		int count = m_residueRowsMap.count(aa);
		pair <ResidueRowsMap::const_iterator, ResidueRowsMap::const_iterator> range =
			m_residueRowsMap.equal_range(m_residues[i]);
		resWeight = 0;
		for (ResidueRowsMap::const_iterator cit = range.first; cit != range.second; cit++)
		{
			int row = cit->second.first;
			resWeight += rowWeights[row];
		}
		if (resWeight > maxResWeight)
		{
			heaviestResidue = m_residues[i];
			maxResWeight = resWeight;
		}
		totalWeight += resWeight;
	}
	return totalWeight;
}

//residues will be in Ncbistd
void ColumnResidueProfile::getResiduesByRow(vector<char>& residues)const
{
	ResidueRowsMap::const_iterator rit = m_residueRowsMap.begin();
	for (; rit != m_residueRowsMap.end(); rit++)
	{
		residues[rit->second.first] = getNcbiStdCode(rit->first);
	}
}

//residues will be in Ncbistd
unsigned char ColumnResidueProfile::getResidueByRow(int row)
{
	ResidueRowsMap::iterator rit = findRow(row);
	if (rit !=  m_residueRowsMap.end())
	{
		return getNcbiStdCode(rit->first);
	}
	else
		return getNcbiStdCode('-');
}


bool ColumnResidueProfile::isAligned(char residue, int row)const
{
	pair <ResidueRowsMap::const_iterator, ResidueRowsMap::const_iterator> range =
		m_residueRowsMap.equal_range(m_residues[residue]);
	for (ResidueRowsMap::const_iterator cit = range.first; cit != range.second; cit++)
	{
		if (cit->second.first == row)
			return cit->second.second;
	}
	return false;
}

bool ColumnResidueProfile::isAligned(int row)const
{
	ResidueRowsMap::const_iterator cit = findRow(row);
	if (cit != m_residueRowsMap.end())
		return cit->second.second;
	else
		return false;
}

bool ColumnResidueProfile::isAllRowsAligned()const
{
	for (ResidueRowsMap::const_iterator cit = m_residueRowsMap.begin();
		cit != m_residueRowsMap.end(); cit++)
	{
		if (!(cit->second.second))
			return false;
	}
	return true;
}

 map<char, double> ColumnResidueProfile::m_backgroundResFreq;
 double* m_backgroundResFreqArray = 0;

void ColumnResidueProfile::useDefaultBackgroundResFreq()
{
	//-------------------------------------------------------------------------
	//
	// residue frequencies, SWISS-PROT, Release 40.20, 06-Jun-2002:
	//   Ala (A) 7.67   Gln (Q) 3.93   Leu (L) 9.56   Ser (S) 7.04
	//   Arg (R) 5.21   Glu (E) 6.48   Lys (K) 5.95   Thr (T) 5.55
	//   Asn (N) 4.32   Gly (G) 6.88   Met (M) 2.37   Trp (W) 1.20
	//   Asp (D) 5.25   His (H) 2.25   Phe (F) 4.08   Tyr (Y) 3.14
	//   Cys (C) 1.62   Ile (I) 5.85   Pro (P) 4.89   Val (V) 6.63
	//
	//-------------------------------------------------------------------------
	// these are the only residues used for calculating information content
	m_backgroundResFreq['A'] = 7.67/100.0;  // Ala
	m_backgroundResFreq['R'] = 5.21/100.0;  // Arg
	m_backgroundResFreq['N'] = 4.32/100.0;  // Asn
	m_backgroundResFreq['D'] = 5.25/100.0;  // Asp
	m_backgroundResFreq['C'] = 1.62/100.0;  // Cys
	m_backgroundResFreq['Q'] = 3.93/100.0;  // Gln
	m_backgroundResFreq['E'] = 6.48/100.0;  // Glu
	m_backgroundResFreq['G'] = 6.88/100.0;  // Gly
	m_backgroundResFreq['H'] = 2.25/100.0;  // His
	m_backgroundResFreq['I'] = 5.85/100.0;  // Ile
	m_backgroundResFreq['L'] = 9.56/100.0;  // Leu
	m_backgroundResFreq['K'] = 5.95/100.0;  // Lys
	m_backgroundResFreq['M'] = 2.37/100.0;  // Met
	m_backgroundResFreq['F'] = 4.08/100.0;  // Phe
	m_backgroundResFreq['P'] = 4.89/100.0;  // Pro
	m_backgroundResFreq['S'] = 7.04/100.0;  // Ser
	m_backgroundResFreq['T'] = 5.55/100.0;  // Thr
	m_backgroundResFreq['W'] = 1.20/100.0;  // Trp
	m_backgroundResFreq['Y'] = 3.14/100.0;  // Tyr
	m_backgroundResFreq['V'] = 6.63/100.0;  // Val
}

double ColumnResidueProfile::getBackgroundResFreq(char res)
{
	if (m_backgroundResFreqArray == 0)
		m_backgroundResFreqArray = BLAST_GetStandardAaProbabilities();
	
	return m_backgroundResFreqArray[getNcbiStdCode(res)];
	/*
	if (m_backgroundResFreq.size() == 0)
		useDefaultBackgroundResFreq();
	if (m_backgroundResFreq.find(res) == m_backgroundResFreq.end())
		return 0.0;
	else
		return m_backgroundResFreq[res];
	*/
}

double ColumnResidueProfile::calcInformationContent()
{
	double info = 0;
	double freqThreshold = 0.0001f;
	double total = (double) m_residueRowsMap.size();
	static const double ln2 = log(2.0f);
	for (int i = 0; i < m_residues.size(); i++)
	{

		int count = m_residueRowsMap.count(m_residues[i]);
		if (count > 0)
		{
			//double standardFreq = GetStandardProbability(m_residues[i]);
			double standardFreq = getBackgroundResFreq(m_residues[i]);
			if ( standardFreq > freqThreshold)
			{
				double freq = double(count)/total;
				double freqRatio = freq/standardFreq;
				if (freqRatio > freqThreshold) 
					info += freq * (log(freqRatio))/ln2;
			}
		}
	}
		return info;
}

//---------------------------ColumnAddress--------------------------------

ColumnAddress::ColumnAddress(int posOnMaster, int aGap)
: mPos(posOnMaster), gap(aGap)
{
}

ColumnAddress::ColumnAddress()
: mPos(0), gap(0)
{
}

ColumnAddress::~ColumnAddress()
{
}

bool ColumnAddress::operator<(const ColumnAddress& rhs) const
{
	if (mPos == rhs.mPos)
		return gap < rhs.gap;
	else
		return mPos < rhs.mPos;
}


//-------------------------ResidueProfiles----------------------------------------

const double ResidueProfiles::FrequencyThreshold = 0.5; //50-50 rule

ResidueProfiles::ResidueProfiles()
: m_profiles(), m_rowWeights(),
	m_consensus(), m_guideAlignment(),
	m_totalRows(1) //starting at 1 because 0 is reserved for master
{
}

ResidueProfiles::~ResidueProfiles()
{
}

void ResidueProfiles::addOneRow(BlockModelPair& bmp, const string& mSeq, const string& sSeq)
{
	//master row = 0
	int masterRow = 0;
	const vector<Block>& mBlocks = bmp.getMaster().getBlocks();
	const vector<Block>& sBlocks = bmp.getSlave().getBlocks();
	for (int bn = 0; bn < mBlocks.size(); bn++)
	{
		for (int i = 0; i < mBlocks[bn].getLen(); i++)
		{
			int mPos = mBlocks[bn].getStart() + i;
			int sPos = sBlocks[bn].getStart() + i;
			ColumnAddress col(mPos);
			m_profiles[col].addOccurence(sSeq[sPos], m_totalRows, true);
			//if (!(m_profiles[col].hasRow(masterRow)))
			m_profiles[col].addOccurence(mSeq[mPos], masterRow, true);
		}
		//add the unaligned region to the c-term unless it is the last block
		if (bn != (mBlocks.size() -1))
		{
			int mPos = mBlocks[bn].getEnd();
			int sPos = sBlocks[bn].getEnd();
			//on the slave
			int sGapLen = bmp.getSlave().getGapToCTerminal(bn);
			int mGapLen = bmp.getMaster().getGapToCTerminal(bn);
			//assume the input alignment is degapped, one of above must be 0
			//assert((sGapLen == 0) || (mGapLen == 0));
			//add cols on master
			for (int gap =1; gap <= mGapLen; gap++)
			{
				ColumnAddress col(mPos + gap);
				m_profiles[col].addOccurence(mSeq[mPos+gap], masterRow, false);
				
			}
			//add cols on slave
			//split the gap in the middle
			int midOnMaster = mGapLen/2 + mGapLen%2;
			int mid = sGapLen/2 + sGapLen%2;
			for (int gap =1; gap <= sGapLen; gap++)
			{
				ColumnAddress col; 
				if ( gap <= mid)
				{
					if (gap <= midOnMaster)
					{
						col.gap = 0;
						col.mPos = mPos + gap;
					}
					else // gap > midOnMaster
					{
						col.mPos = mPos + midOnMaster;
						col.gap = gap - midOnMaster;
					}
				}
				else // (gap > mid) //attach to the next block
				{
					int mPosNext = mBlocks[bn+1].getStart();
					int sPosNext = sBlocks[bn+1].getStart();
					int delta = sPosNext - (sPos + gap);
					if (delta <= (mGapLen/2))
					{
						col.mPos = mPosNext - delta;
						col.gap = 0;
					}
					else
					{
						col.mPos = mPosNext - (mGapLen/2);
						col.gap = (mGapLen/2) - delta;
					}
				}
				m_profiles[col].addOccurence(sSeq[sPos+gap], m_totalRows, false);
			}
		}	
	}
	m_totalRows++;
}


void ResidueProfiles::calculateRowWeights()
{
	bool countGap = false;
	//iterate through all column Profile to add up the row weights
	m_rowWeights.assign(m_totalRows, 0.0);
	double weightsSum = 0;
	int colUsed = 0;
	PosProfileMap::const_iterator cit = m_profiles.begin();

	//get highest ungapped coun
	int highestCount = 0;
	for (; cit != m_profiles.end(); cit++)
	{
		int aCount = cit->second.getSumCount();
		if (aCount > highestCount)
			highestCount = aCount;
	}
	cit = m_profiles.begin();
	for (; cit != m_profiles.end(); cit++)
	{
		const ColumnResidueProfile& colProfile = cit->second;
		if(cit->first.mPos == 1498)
			bool gotit = true;
		if (!countGap)
		{
			//only use columns that has all rows to calculate the row weight
			if (colProfile.getSumCount() >= highestCount)
			{
				weightsSum += colProfile.sumUpColumnWeightsByRow(m_rowWeights, countGap, m_totalRows);
				colUsed++;
			}
		}
		//count gap
		//but ignore columns with identical residues
		else if (colProfile.getResidueTypeCount() > 1 )
		{
			weightsSum += colProfile.sumUpColumnWeightsByRow(m_rowWeights, countGap, m_totalRows);
			colUsed++;
		}
	}
	//debug
	//check weightsSum should round up to colUsed
	//printf("Column with all rows:%d. Total row Weight: %.2f\n", colUsed, weightsSum);
	//if a row did not get a weight, give it a default
	double defaultWeight = 0.0;
	if (colUsed != 0)
		defaultWeight = 1.0f * double(colUsed)/double(m_totalRows);
	else
		defaultWeight = 1.0f * double(m_profiles.size())/double(m_totalRows);
	double minW = 0.0, maxW = 0.0;
	int noWeightRows = 0;
	/*
	for ( int i = 0; i < m_rowWeights.size(); i++)
	{
		if (m_rowWeights[i] > maxW)
			maxW = m_rowWeights[i];
		if (m_rowWeights[i] != 0.00)
		{
			if (minW == 0.0)
				minW = m_rowWeights[i];
			else if (m_rowWeights[i] < minW)
				minW = m_rowWeights[i];
		}
		
	}*/
	/*
	printf("default weight=%.4f; minW=%.4f;maxW=%.4f;colUsed=%d; wieghtSum=%.4f, totalRow=%d\n", defaultWeight, 
		 minW, maxW, colUsed, weightsSum, m_totalRows);*/
	for ( int i = 0; i < m_rowWeights.size(); i++)
	{
		//normalize the weight by weightsSum
		if (m_rowWeights[i] == 0.0)
		{
			m_rowWeights[i] = defaultWeight;
			weightsSum += m_rowWeights[i];
			noWeightRows++;
		}
	}
	for ( int i = 0; i < m_rowWeights.size(); i++)
	{
		m_rowWeights[i] = m_rowWeights[i]/weightsSum;
	}
	//printf("number of no weight columns=%d\n", noWeightRows);
	//debug
	/*
	for ( int i = 0; i < m_rowWeights.size(); i++)
	{
		printf("Row: %d | Weight: %.3f\n", i, m_rowWeights[i]);
	}*/
}

const string& ResidueProfiles::makeConsensus()
{
	vector<Block>& blocksOnMaster = m_guideAlignment.getMaster().getBlocks();
	vector<Block>& blocksOnConsensus = m_guideAlignment.getSlave().getBlocks();
	blocksOnMaster.clear();
	blocksOnConsensus.clear();
	m_consensus.erase();

	bool inBlock = false;
	int startM = 0, endM = 0;
	int startC = 0;
	int blockId = 0;
	double threshold = FrequencyThreshold;

	PosProfileMap::iterator cit = m_profiles.begin();
	for (; cit != m_profiles.end(); cit++)
	{
		const ColumnAddress& col = cit->first;
		
		char res = 0;
		double weight = (cit->second).reweightColumnByRowWeights(m_rowWeights, res);
		
		bool qualifiedForConsensus = (weight >= threshold && res );
		bool qualifiedForGuide = qualifiedForConsensus && ((cit->second).isAligned(0)); //is aligned on master
		//bool qualifiedForGuide = qualifiedForConsensus && (col.gap == 0); //not a gap on the master
		
		if (!inBlock)
		{
			if (qualifiedForGuide)
			{
				startM = col.mPos;
				endM = startM;
				startC = m_consensus.size();
				//m_consensus += res;
				inBlock = true;
			}
		}
		else
		{
			if (qualifiedForGuide)
			{
				assert(col.mPos > endM);
				if (col.mPos == (endM + 1)) //continue on the previous block
				{
					endM++;
				}
				else  
				{	
					//save the last block
					blocksOnMaster.push_back(Block(startM, endM - startM + 1, blockId));
					blocksOnConsensus.push_back(Block(startC, endM - startM + 1, blockId));
					//start a new block
					blockId++;
					startM = col.mPos;
					endM = startM;
					startC = m_consensus.size();
				}
			}
			else //ending this block
			{
				inBlock = false;
				blocksOnMaster.push_back(Block(startM, endM - startM + 1, blockId));
				blocksOnConsensus.push_back(Block(startC, endM - startM + 1, blockId));
				blockId++;
			}
		}
		if (qualifiedForConsensus)
		{
			cit->second.setIndexByConsensus(m_consensus.size());
			m_consensus += res;
		}
	}
	if (inBlock) //block goes to the end of the sequence
	{
		blocksOnMaster.push_back(Block(startM, endM - startM + 1, blockId));
		blocksOnConsensus.push_back(Block(startC, endM - startM + 1, blockId));
	}
	assert(m_guideAlignment.isValid());
	return m_consensus;
}

const string ResidueProfiles::getConsensus(bool inNcbieaa)
{
	if (inNcbieaa)
		return m_consensus;
	else
	{
		string ncbistd;
		for (int i = 0; i < m_consensus.size(); i++)
		{
			ncbistd += ColumnResidueProfile::getNcbiStdCode(m_consensus[i]);
		}
		return ncbistd;
	}
}

const BlockModelPair& ResidueProfiles::getGuideAlignment()const
{
	return m_guideAlignment;
}

BlockModelPair& ResidueProfiles::getGuideAlignment()
{
	return m_guideAlignment;
}

int ResidueProfiles::countColumnsOnMaster(string& seq)
{
	MasterColumnCounter mcc;
	traverseColumnsOnMaster(mcc);
	seq.assign(mcc.getSeq());
	return mcc.getCount();
}

void ResidueProfiles::traverseAllColumns(ColumnReader& cr)
{
	PosProfileMap::iterator pit = m_profiles.begin();
	for (; pit != m_profiles.end(); pit++)
	{
		cr.read(pit->second);
	}
}

void ResidueProfiles::traverseColumnsOnMaster(ColumnReader& cr)
{
	PosProfileMap::iterator pit = m_profiles.begin();
	for (; pit != m_profiles.end(); pit++)
	{
		if (pit->first.gap == 0)
			cr.read(pit->second);
	}
}

void ResidueProfiles::traverseColumnsOnConsensus(ColumnReader& cr)
{
	PosProfileMap::iterator pit = m_profiles.begin();
	for (; pit != m_profiles.end(); pit++)
	{
		if (pit->second.getIndexByConsensus() >= 0)
			cr.read(pit->second);
	}
}

void ResidueProfiles::traverseAlignedColumns(ColumnReader& cr)
{
	PosProfileMap::iterator pit = m_profiles.begin();
	for (; pit != m_profiles.end(); pit++)
	{
		if (pit->second.isAllRowsAligned())
			cr.read(pit->second);
	}
}

double ResidueProfiles::calcInformationContent(bool byConsensus)
{
	double info = 0;
	
	PosProfileMap::iterator cit = m_profiles.begin();
	for (; cit != m_profiles.end(); cit++)
	{
		ColumnResidueProfile& colProfile = cit->second;
		//if (colProfile.isAllRowsAligned())
		//if (colProfile.getSumCount() == m_totalRows)
		bool useCol = false;
		if ( byConsensus ) {
			useCol = colProfile.getIndexByConsensus() >= 0;
		}
		else		 //by master
			useCol = (cit->first.gap == 0);
			
		if (useCol)
		{
			info += colProfile.calcInformationContent();
		}
	}
	return info;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/04/19 14:27:18  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */

