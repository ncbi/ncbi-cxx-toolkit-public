#ifndef CU_ROW_SOURCE_TABLE_HPP
#define CU_ROW_SOURCE_TABLE_HPP

//#include <algo/structure/cd_utils/cuCppNCBI.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>
BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

struct RowSource
{
	RowSource() : cd(0), normal(false), rowInSrc(-1), master(false){}
	RowSource(CCdCore* cd, int row, bool norm):cd(cd), normal(norm), rowInSrc(row)
		{master = (row==0) && normal;}
	RowSource(const RowSource& rhs):cd(rhs.cd), normal(rhs.normal), rowInSrc(rhs.rowInSrc), master(rhs.master){}

	bool isPending() const { return !normal;}
	bool wasMaster() const {return master;}
	CCdCore* cd;
	bool normal; //normal or pending
	int rowInSrc;  // row in the source cd
	bool master;
};

typedef map<CCdCore*, vector<int> >CDRowsMap;
class NCBI_CDUTILS_EXPORT RowSourceTable
{
public:
	RowSourceTable();
	
	void addEntry(int row, CCdCore* cd, int rowInCD, bool normal=true, bool scoped = true);
	void addEntry(int row, RowSource rs, bool scoped = true);
	int findEntries(int row, vector<RowSource>& src, bool scopedOnly=false)const;
	void removeEntriesForCD(vector<int>& colRows, CCdCore* cd);
	const RowSource& findEntry(int row)const ;
	RowSource& findEntry(int row);
	//int findEntriesBySourceCD(const CCdCore* cd, vector<int> rows)const;
	//int findRowBySource(const RowSource& rs)const;
	bool isPending(int row)const;
	void getMasterRows(vector<int>& masters);
	//get all CDs involved in this AlignmentCollection
	int getCDs(vector<CCdCore*>& cds);
	//get only CDs whose alignment rows are included
	int getCDsInScope(vector<CCdCore*>& cds);
	//get CDs which are only used to map row membership of duplicated SeqLoc
	int getCDsOutofScope(vector<CCdCore*>& cds);
	bool isCDInScope(CCdCore* cd)const;
	bool isEntryInScope(const RowSource& rs)const;
	void clear();
	void convertFromCDRows(CCdCore* cd, const vector<int>& cdRows, set<int>& colRows)const;
	int convertFromCDRow(CCdCore* cd, int cdRows)const;
	void convertToCDRows(const vector<int>& colRows, CDRowsMap& cdRows) const;

	//void getAllRowsForCD(CCdCore* cd, vector<int>& colRows)const;
	bool isRowInCD(int row, CCdCore* cd)const;
private:
	typedef multimap<int, RowSource> RowSourceMap;
	RowSourceMap m_table;
	set<int> m_masters;
	set<CCdCore*> m_cdsInScope;
	set<CCdCore*> m_cdsOutofScope;
	//pending row starts at CCdCore::PENDING_ROW_START
	string makeCDRowKey(CCdCore* cd, int row)const ;
	//necessary indice to speed lookup
	typedef map<string, int>CDRowIndexMap;
	CDRowIndexMap m_cdRowIndexMap;

	void transferCDs(const set<CCdCore*>& cdSet, vector<CCdCore*>& cdVec);
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE
#endif        
        

