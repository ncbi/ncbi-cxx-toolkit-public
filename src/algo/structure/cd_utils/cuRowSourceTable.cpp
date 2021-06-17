#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuRowSourceTable.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

RowSourceTable::RowSourceTable()
    :m_table(), m_masters(), m_cdRowIndexMap()
{

}

    
void RowSourceTable::addEntry(int row, CCdCore* cd, int rowInCD, bool normal, bool scoped)
{
    RowSource rs(cd, rowInCD, normal);
    addEntry(row, rs, scoped);
}

void RowSourceTable::addEntry(int row, RowSource rs, bool scoped)
{
    m_table.insert(RowSourceMap::value_type(row, rs));
    if (scoped)
        m_cdsInScope.insert(rs.cd);
    else
        m_cdsOutofScope.insert(rs.cd);
    if (rs.rowInSrc == 0 && rs.normal)
    {
        m_masters.insert(row);    
    }
    int uniRow = rs.rowInSrc;
    if (!rs.normal)
        uniRow += PENDING_ROW_START;
    m_cdRowIndexMap.insert(CDRowIndexMap::value_type(makeCDRowKey(rs.cd, uniRow), row));
}

void RowSourceTable::removeEntriesForCD(vector<int>& rows, CCdCore* cd)
{
	if (isCDInScope(cd))
		return; // can only do this if cd is not in scope
	m_cdsOutofScope.erase(cd);
	//vector<int> rows;
	//getAllRowsForCD(cd, rows);
	for ( unsigned int i = 0; i < rows.size(); i++)
	{
		m_masters.erase(rows[i]);
		pair<RowSourceMap::iterator, RowSourceMap::iterator> range = m_table.equal_range(rows[i]);
		for (RowSourceMap::iterator rit = range.first; rit != range.second; ++rit)
		{
			RowSource& rs = rit->second;
			int uniRow = rs.rowInSrc;
			if (!rs.normal)
				uniRow += PENDING_ROW_START;
			m_cdRowIndexMap.erase(makeCDRowKey(rs.cd, uniRow));
			if ( rs.cd == cd)
			{
				m_table.erase(rit);
				break;
			}
			else if (rs.wasMaster())
				m_masters.insert(rows[i]);
		}
	}
	return;
}

/* buggy--does not work with pendings.  Replace it with isRowInCD
void RowSourceTable::getAllRowsForCD(CCdCore* cd, vector<int>& rows)const
{
    vector<int> cdRows;
    int num = cd->GetNumRows();
    for ( int i = 0; i < num; i++)
    {
        cdRows.push_back(i);
    }
    set<int> colRows;
    convertFromCDRows(cd, cdRows, colRows); 
    for (set<int>::iterator sit = colRows.begin(); sit != colRows.end(); sit++)
    {
        rows.push_back(*sit);
    }
}*/


bool RowSourceTable::isRowInCD(int row, CCdCore* cd) const
{
	vector<RowSource> rss;
	findEntries(row, rss);
	for(unsigned int i = 0; i < rss.size(); i++)
	{
		if (rss[i].cd == cd)
			return true;
	}
	return false;
}

int RowSourceTable::findEntries(int row, vector<RowSource>& src, bool scopedOnly) const
{
    pair<RowSourceMap::const_iterator, RowSourceMap::const_iterator> range = m_table.equal_range(row);
    for (RowSourceMap::const_iterator rit = range.first; rit != range.second; ++rit)
    {
        if (scopedOnly)
        {
            if (isCDInScope(rit->second.cd))
                src.push_back(rit->second);
        }
        else
            src.push_back(rit->second);
    }
    return src.size();
}

const RowSource& RowSourceTable::findEntry(int row) const 
{
    RowSourceMap::const_iterator rcit = m_table.find(row);
    if (rcit != m_table.end())
    {
        return rcit->second;
    }
    else
        return *(new RowSource());
}

RowSource& RowSourceTable::findEntry(int row) 
{
    RowSourceMap::iterator rcit = m_table.find(row);
    if (rcit != m_table.end())
    {
        return rcit->second;
    }
    else
        return *(new RowSource());
}
    //int findRowBySourceCD(const CCdCore* cd, vector<int> rows)const;
    //int findRowBySource(const RowSource& rs)const;
bool RowSourceTable::isPending(int row)const
{
    const RowSource& rs = findEntry(row);
    if (rs.cd)
    {
        return !rs.normal;
    }
    else
        return false;
}

void RowSourceTable::getMasterRows(vector<int>& masters)
{
    for (set<int>::iterator sit = m_masters.begin(); sit != m_masters.end(); sit++)
        masters.push_back(*sit);    
}

int RowSourceTable::getCDs(vector<CCdCore*>& cds)
{
    getCDsInScope(cds);
    getCDsOutofScope(cds);
    return cds.size();
}

//get only CDs whose alignment rows are included
int RowSourceTable::getCDsInScope(vector<CCdCore*>& cds)
{
    transferCDs(m_cdsInScope, cds);
    return cds.size();
}

//get CDs which are only used to map row membership of duplicated SeqLoc
int RowSourceTable::getCDsOutofScope(vector<CCdCore*>& cds)
{
    transferCDs(m_cdsOutofScope, cds);
    return cds.size();
}

bool RowSourceTable::isCDInScope(CCdCore* cd) const
{
    set<CCdCore*>::const_iterator sit = m_cdsInScope.find(cd);
    return sit != m_cdsInScope.end();
}

bool RowSourceTable::isEntryInScope(const RowSource& rs)const
{
    return isCDInScope(rs.cd);
}

void RowSourceTable::transferCDs(const set<CCdCore*>& cdSet, vector<CCdCore*>& cdVec)
{
    for (set<CCdCore*>::const_iterator sit = cdSet.begin(); sit != cdSet.end(); sit++)
        cdVec.push_back(*sit);
}

void RowSourceTable::clear()
{
    m_table.clear();
    m_masters.clear();
}

//pending row starts at CCdCore::PENDING_ROW_START
string RowSourceTable::makeCDRowKey(CCdCore* cd, int row) const
{
    return cd->GetAccession() + 'r' + NStr::IntToString(row);;
}

void RowSourceTable::convertFromCDRows(CCdCore* cd, const vector<int>& cdRows, set<int>& colRows)const
{
	string acc = cd->GetAccession();
	for (unsigned int i = 0; i < cdRows.size(); i++)
	{
		CDRowIndexMap::const_iterator rowIt = m_cdRowIndexMap.find(makeCDRowKey(cd, cdRows[i]));
		if (rowIt != m_cdRowIndexMap.end())
			colRows.insert(rowIt->second);
	}
}

int RowSourceTable::convertFromCDRow(CCdCore* cd, int cdRow)const
{
    CDRowIndexMap::const_iterator rowIt = m_cdRowIndexMap.find(makeCDRowKey(cd, cdRow));
    if (rowIt != m_cdRowIndexMap.end())
        return rowIt->second;
    else
        return -1;
}

void RowSourceTable::convertToCDRows(const vector<int>& colRows, CDRowsMap& cdRows) const
{
	for (unsigned int i = 0; i < colRows.size(); i++)
	{
		vector<RowSource> entries;
		findEntries(colRows[i], entries);
		for(unsigned int j = 0; j < entries.size(); j++)
		{
			int uniRow = entries[j].normal ? entries[j].rowInSrc : entries[j].rowInSrc + PENDING_ROW_START;
			cdRows[entries[j].cd].push_back(uniRow);
		}
	}
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
