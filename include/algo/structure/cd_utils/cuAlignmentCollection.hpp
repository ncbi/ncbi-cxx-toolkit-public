#ifndef CU_ALIGNMENT_COLLECTION_HPP
#define CU_ALIGNMENT_COLLECTION_HPP
#include <algo/structure/cd_utils/cuCdCore.hpp>
#include <algo/structure/cd_utils/cuCppNCBI.hpp>
#include <algo/structure/cd_utils/cuCdFamily.hpp>
#include <algo/structure/cd_utils/cuRowSourceTable.hpp>
#include <algo/structure/cd_utils/cuBlock.hpp>
#include <algo/structure/cd_utils/cuSequenceTable.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)
//encapsulate the operations on a collection of alignments from normal or pending
//alignments of multiple CDs

class MultipleAlignment;

//row=0 is master
class NCBI_CDUTILS_EXPORT AlignmentCollection
{
public:
	AlignmentCollection(CCdCore* cd, CCdCore::AlignmentUsage alignUse=CCdCore::USE_PENDING_ALIGNMENT, bool uniqueSeqId=false, bool scoped=true);
	AlignmentCollection();
	void AddAlignment(CCdCore* cd, CCdCore::AlignmentUsage alignUse=CCdCore::USE_PENDING_ALIGNMENT, bool uniqueSeqId=false,bool scoped=true);
	void AddAlignment(const AlignmentCollection& ac);
	
	void AddSequence(CCdCore* cd);
	void AddSequence(const AlignmentCollection& ac);

	int getNumFamilies() const { return m_numFamilies;}
	void setNumFamilies(int num) { m_numFamilies = num;}
	void clear() {m_firstCd = 0; m_seqAligns.clear(); m_rowSources.clear();}
	CCdCore* getFirstCD() const {return m_firstCd;}
	int GetNumRows() const;
    int GetNumPendingRows() const;
	int GetAlignmentLength(int row=0) const;
	bool  Get_GI_or_PDB_String_FromAlignment(int  row, std::string& result) const ;
	bool GetSeqIDForRow(int row, CRef< CSeq_id >& SeqID) const ;
	bool GetSeqEntryForRow(int row, CRef<CSeq_entry>& seqEntry)const;
	string GetDefline(int row) const;
	bool GetGI(int row, int& gi, bool ignorePDBs = true) const;
    int GetLowerBound(int row) const;
	int GetUpperBound(int row) const;
	int FindSeqInterval (const CSeq_interval& seqLoc) const;
	bool IsPending(int row)const ;
	bool wasMaster(int row) const;
	void removeRowSourcesForCD(CCdCore* cd);
	void addRowSources(const vector<int>& rows, CCdCore* cd, bool scoped=true);
	const RowSource& GetRowSource(int row) const ;
	RowSource& GetRowSource(int row);
	CRef<CSeq_align> getSeqAlign(int row) const{return m_seqAligns[row];}
	const RowSourceTable& GetRowSourceTable() const {return m_rowSources;};
	//get all CDs involved in this AlignmentCollection
	int getCDs(vector<CCdCore*>& cds);
	//get only CDs whose alignment rows are indlued
	int getCDsInScope(vector<CCdCore*>& cds);
	//get CDs which are only used to map row membership of duplicated SeqLoc
	int getCDsOutofScope(vector<CCdCore*>& cds);
	bool isCDInScope(CCdCore* cd)const;
	bool isRowInScope(int row)const;
	CCdCore* GetLeafDescendentCD(int row, bool includeSelf=false)const;
	CCdCore* GetScopedLeafCD(int row)const;
	CCdCore* GetSeniorMemberCD(int row, bool scopedOnly=true)const;
	void convertFromCDRows(CCdCore* cd, const vector<int>& cdRows, set<int>& colRows)const;
	void convertToCDRows(const vector<int>& colRows, CDRowsMap& cdRows) const;
	void mapRows(const AlignmentCollection& ac, const set<int>& rows, set<int>& convertedRows) const;
	int mapRow(const AlignmentCollection& ac, int row) const;
	void getAllRowsForCD(CCdCore* cd, vector<int>& colRows)const;
	bool GetSpeciesForRow(int row, string& species) const;
	bool  GetBioseqForRow(int rowId, CRef< CBioseq >& bioseq);
	int GetRowsWithSameSeqID(int rowToMatch, vector<int>& rows, bool inclusive = true) const;
	int GetRowsWithSeqID(const CRef< CSeq_id >& SeqID, vector<int>& rows) const;
	bool IsStruct(int row);
    bool IsPdb(int row)const;
	void GetAllSequences(vector<string>& sequences);
	string GetSequenceForRow(int row);
	void GetAlignedResiduesForRow(int row, char*&);
	void GetAlignedResiduesForAll(char** & ppAlignedResidues, bool forceRecompute) ;
	bool isInstanceOf(MultipleAlignment* ma);
	void getNormalRowsNotInChild(vector<int>& childless, bool excludeMaster=false) const;
	//void makeGuideAlignment(int row, Multiple
	virtual ~AlignmentCollection();

    // dih -- I added these for debugging
    bool AreNonOverlapping() const;
    bool IsNonOverlapping(const CRef< CSeq_align >& align, int& blockIndex) const;
	string& getLastError() {return m_err;}
    //static void EraseRows(CCdCore * pCD,vector < int > & rows) ;
    //static void CopyRows(CCdCore * pSrCCdCore,CCdCore * pDstCD,vector < int > & rows) ;
	//static bool GetDenDiagSet(CCdCore * pCD,int row, ncbi::objects::TDendiag* & pDenDiagSet);

protected:
	vector< CRef< CSeq_align > > m_seqAligns; //row=0 holds master
	//CCdCore::AlignmentUsage m_alignUse;
	//int m_pendingStart;
	RowSourceTable m_rowSources;
	CCdCore* m_firstCd;
	int m_numFamilies;
	string m_err;
	
	//buffer sequence data to speed up
	vector<CRef< CBioseq > > m_bioseqs;
	SequenceTable m_seqTable;

	void addPendingAlignment(CCdCore* cd, bool uniqueSeqId=false, bool scoped=true);
	void addNormalAlignment(CCdCore* cd, bool uniqueSeqId=false, bool scoped=true);
};

//all rows in Multiple Alignment should have the same block structure.

class NCBI_CDUTILS_EXPORT MultipleAlignment : public AlignmentCollection
{
public:
	MultipleAlignment(CCdCore* cd, bool scoped=true);
	MultipleAlignment(const CDFamily& family);
	MultipleAlignment();

	bool setAlignment(const CDFamily& family);
	bool setAlignment(const CDFamily& family, CDFamilyIterator& start);
	void setAlignment(CCdCore* cd, bool scoped=true);
	void setAlignment(CRef<CSeq_align>& seqAlign);
	bool setAlignment(const AlignmentCollection& ac, int row);
	bool isBlockAligned() const;
	//void setAlignment(const MultipleAlignment& malign);
	int appendAlignment(MultipleAlignment& malign,bool includeUnScoped=true);
	bool findParentalEquivalent(const cd_utils::BlockModel& bar, int& pRow, bool inputAsChild=true) const;
	bool findParentalCastible(const cd_utils::BlockModel& bar, int& pRow) const;
	bool locateChildRow(const cd_utils::BlockModel& childRow, int& pRow) const;
	const cd_utils::BlockModel& getBlockModel(int row) const;
	void copyRowSource(int currentRow, const AlignmentCollection& malign, int rowInAc);
	bool transferToCD(CCdCore* cd =0);  //transfer to the first/root cd;
	bool transferOnlyMastersToCD(bool onlyKeepStructureWithEvidence);  //transfer to the first/root cd;
	int getNonEssentialRows(vector<int>& neRows);
	bool extractAlignedSubset(const AlignmentCollection& ac, const set<int>& normalRows, 
		int master);
	
private:
	//to hide the one in the base class
	void AddAlignment(CCdCore* cd, CCdCore::AlignmentUsage alignUse){ }
	
	void makeBlockTable();
	int transferOneRow(CCdCore* cd, int row);

	vector<cd_utils::BlockModel> m_blockTable;
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE
#endif
