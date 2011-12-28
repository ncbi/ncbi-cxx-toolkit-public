#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuAlignmentCollection.hpp>
#include <algo/structure/cd_utils/cuAlign.hpp>
#include <algo/structure/cd_utils/cuUtils.hpp>
#include <algo/structure/cd_utils/cuSequence.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)
//AlignmentCollection--------------------------------------------------

AlignmentCollection::AlignmentCollection(CCdCore* cd, CCdCore::AlignmentUsage alignUse, bool uniqueSeqId,bool scoped)
 : m_seqAligns(), m_rowSources(), m_firstCd(0), m_seqTable()
{
	AddAlignment(cd,alignUse, uniqueSeqId, scoped);
    m_numFamilies = 1;

}

AlignmentCollection::AlignmentCollection()
: m_seqAligns(), m_rowSources(), m_firstCd(0), m_seqTable()
{
    m_numFamilies = 1;
}

void AlignmentCollection::AddAlignment(CCdCore* cd, CCdCore::AlignmentUsage alignUse, bool uniqueSeqId,bool scoped)
{
	if(m_firstCd==0)
		m_firstCd = cd;
	//m_alignUse = alignUse;
	bool useNormal = (alignUse == CCdCore::USE_NORMAL_ALIGNMENT) || (alignUse == CCdCore::USE_ALL_ALIGNMENT);
	bool usePending = (alignUse == CCdCore::USE_PENDING_ALIGNMENT) || (alignUse == CCdCore::USE_ALL_ALIGNMENT);
	if(useNormal)
		addNormalAlignment(cd, uniqueSeqId,scoped);
	if(usePending)
		addPendingAlignment(cd, uniqueSeqId,scoped);
	AddSequence(cd);
}

void AlignmentCollection::AddAlignment(const AlignmentCollection& ac)
{
	if(m_firstCd == 0)
		m_firstCd = ac.getFirstCD();
	for (int i = 0; i < (int) ac.m_seqAligns.size(); i++)
	{
		m_seqAligns.push_back(ac.m_seqAligns[i]);
		vector<RowSource> srcs;
		ac.m_rowSources.findEntries(i, srcs);
		for (unsigned int j = 0; j < srcs.size(); j++)
			m_rowSources.addEntry(m_seqAligns.size() - 1, srcs[j], ac.isCDInScope(srcs[j].cd));
	}
	AddSequence(ac);
}

void AlignmentCollection::AddSequence(CCdCore* cd)
{
	m_seqTable.addSequences(cd->SetSequences());
}

void AlignmentCollection::AddSequence(const AlignmentCollection& ac)
{
	m_seqTable.addSequences(ac.m_seqTable);
}

void AlignmentCollection::addPendingAlignment(CCdCore* cd, bool uniqueSeqId, bool scoped)
{
//	m_pendingStart = m_seqAligns.size();
	if (cd->IsSetPending())
	{
		list< CRef< CUpdate_align > >& pending = cd->SetPending();
		list< CRef< CUpdate_align > >::iterator lit = pending.begin();
		int rowInCD = 0;
		int rowInCol = 0;
		for(; lit != pending.end(); ++lit)
		{
			if ((*lit)->IsSetSeqannot())
			{
				CSeq_annot& seqannot = (*lit)->SetSeqannot();
				if (seqannot.GetData().IsAlign())
				{
					/* no need to keep master for pending
					if (m_seqAligns.size() ==0)
						m_seqAligns.push_back( *(seqannot.SetData().SetAlign().begin()) ); 
					**********/
					if (uniqueSeqId)
					{
						CRef<CSeq_id> seqId;
						GetSeqID(*(seqannot.SetData().SetAlign().begin()) , seqId, true);
						vector<int> rows;
						if (GetRowsWithSeqID(seqId, rows) <= 0)
						{
							m_seqAligns.push_back( *(seqannot.SetData().SetAlign().begin()) );
							rowInCol = m_seqAligns.size() - 1;
						}
						else
							rowInCol = rows[0];
					}
					else
					{
						m_seqAligns.push_back( *(seqannot.SetData().SetAlign().begin()) );
						rowInCol = m_seqAligns.size() - 1;
					}
					m_rowSources.addEntry(rowInCol, cd, rowInCD, false, scoped);
					rowInCD++;
				}
			}
		}
	}
}

void AlignmentCollection::addNormalAlignment(CCdCore* cd, bool uniqueSeqId, bool scoped)
{
	if(cd->IsSeqAligns())
	{
		list< CRef< CSeq_align > >& alignList = cd->GetSeqAligns();
		list< CRef< CSeq_align > >::iterator lit = alignList.begin();
		//int nextDestRow = m_seqAligns.size();
		int rowInCD = 0;
		int rowInCol = 0;
		//place holder for master
		if (uniqueSeqId)
		{
			CRef<CSeq_id> seqId;
			GetSeqID(*(lit) , seqId, false);
			vector<int> rows;
			if (GetRowsWithSeqID(seqId, rows) <= 0)
			{
				m_seqAligns.push_back( *(lit) );
				rowInCol = m_seqAligns.size() - 1;
			}
			else
				rowInCol = rows[0];
		}
		else
		{
			m_seqAligns.push_back(*lit); 
			rowInCol = m_seqAligns.size() - 1;
		}
		m_rowSources.addEntry(rowInCol, cd, rowInCD,true, scoped);
		
		for (; lit != alignList.end(); ++lit)
		{
			rowInCD++;
			if (uniqueSeqId)
			{
					CRef<CSeq_id> seqId;
					GetSeqID(*(lit) , seqId, true);
					vector<int> rows;
					if (GetRowsWithSeqID(seqId, rows) <= 0)
					{
						m_seqAligns.push_back( *(lit) );
						rowInCol = m_seqAligns.size() - 1;
					}
					else
						rowInCol = rows[0];
			}
			else
			{
				m_seqAligns.push_back(*lit);
				rowInCol = m_seqAligns.size() - 1;
			}
			m_rowSources.addEntry(rowInCol, cd, rowInCD,true, scoped);
		}
	}
}

AlignmentCollection::~AlignmentCollection()
{
}

bool AlignmentCollection::isInstanceOf(MultipleAlignment* ma)
{
	//ma = dynamic_cast<MultipleAlignment*>(this);
	try { 
		ma = dynamic_cast<MultipleAlignment*> (this);
	}catch (...){
		ma=0;
	}
	return ma != NULL;
}


int AlignmentCollection::GetNumPendingRows() const
{
    int Count = 0;
    for (int i=0; i<GetNumRows(); i++) {
        if (IsPending(i)) {
            Count++;
        }
    }
    return(Count);
}


int AlignmentCollection::GetNumRows() const
{
	return m_seqAligns.size();
}

int AlignmentCollection::GetAlignmentLength(int row) const
{
        return GetNumAlignedResidues(m_seqAligns[row]);
}

bool  AlignmentCollection::Get_GI_or_PDB_String_FromAlignment(int  row, std::string& result) const
{
	//-------------------------------------------------------------------
	// get seq-id string for row'th of alignment
	//-------------------------------------------------------------------
	CRef< CSeq_id > SeqID;

	GetSeqIDForRow(row, SeqID);
//	Make_GI_or_PDB_String(SeqID, result, false, 0);
    if (SeqID->IsGi() || SeqID->IsPdb()) {
        result += GetSeqIDStr(SeqID);
    } else {
        result += "<Non-gi/pdb Sequence Types Unsupported>";
    }

	return(true);
}


bool AlignmentCollection::GetGI(int row, int& gi, bool ignorePDBs) const
{
	CRef< CSeq_id >  SeqID;
	GetSeqIDForRow(row, SeqID);
	if (SeqID->IsGi()) 
	{
		gi = SeqID->GetGi();
		return true;
    } else if (SeqID->IsPdb() && !ignorePDBs) {   //  cjl; 10/20/03
        //  shouldn't matter if row is normal or not
        const RowSource& rs = GetRowSource(row);
        gi = rs.cd->GetGIFromSequenceList(rs.cd->GetSeqIndex(SeqID));
        return true;
    }
	return false;
}

bool AlignmentCollection::GetSeqIDForRow(int row, CRef< CSeq_id >& SeqID) const 
{
	bool getSlave = true;
	//if (wasMaster(row))
	if ((row==0) || wasMaster(row))
		getSlave = false;
	return GetSeqID(m_seqAligns[row], SeqID, getSlave);
}

//assume noraml row starts at 0 and pending rows are after normal rows.
bool AlignmentCollection::IsPending(int row) const
{
	//return !m_rowSources.findEntry(row).normal;
	return GetRowSource(row).isPending();
}

bool AlignmentCollection::wasMaster(int row) const
{
	//return !m_rowSources.findEntry(row).normal;
	return GetRowSource(row).wasMaster();
}

//rows are the rows in the collection
void AlignmentCollection::addRowSources(const vector<int>& rows, CCdCore* cd, bool scoped)
{
	MultipleAlignment ma (cd);
	for ( unsigned int i = 0; i < rows.size(); i++)
	{
		RowSource rs = GetRowSource(rows[i]);
		rs.cd = cd;
		if(rs.normal)
		{
			CRef<CSeq_align> saRef = getSeqAlign(rows[i]);
			BlockModel bm(saRef);
			int rowInCd = -1;
			if (ma.findParentalCastible(bm, rowInCd))
			{
				rs.rowInSrc = rowInCd;
				m_rowSources.addEntry(rows[i], rs, scoped);
			}
		}
	}
}


void AlignmentCollection::removeRowSourcesForCD(CCdCore* cd)
{
	if (isCDInScope(cd))
		return;
	vector<int> rows;
	getAllRowsForCD(cd, rows);
	m_rowSources.removeEntriesForCD(rows,cd);
}

const RowSource& AlignmentCollection::GetRowSource(int row) const 
{
	return m_rowSources.findEntry(row);
}

RowSource& AlignmentCollection::GetRowSource(int row)
{
	return m_rowSources.findEntry(row);
}

int AlignmentCollection::getCDs(vector<CCdCore*>& cds)
{
	return m_rowSources.getCDs(cds);
}

int AlignmentCollection::getCDsInScope(vector<CCdCore*>& cds)
{
	return m_rowSources.getCDsInScope(cds);
}
	//get CDs which are only used to map row membership of duplicated SeqLoc
int AlignmentCollection::getCDsOutofScope(vector<CCdCore*>& cds)
{
	return m_rowSources.getCDsOutofScope(cds);
}

bool AlignmentCollection::isCDInScope(CCdCore* cd) const
{
	return m_rowSources.isCDInScope(cd);
}
//includeSelf=false
CCdCore* AlignmentCollection::GetLeafDescendentCD(int row, bool includeSelf)const
{
	vector<RowSource> src;
	m_rowSources.findEntries(row, src);
	if (src.size() < 1) //no descenddent
		return 0;
	else if (src.size() ==1)
	{
		if ((src[0].cd == m_firstCd) && (!includeSelf))
			return 0;
	}
	return (src.rbegin())->cd;
}

CCdCore* AlignmentCollection::GetScopedLeafCD(int row)const
{
	vector<RowSource> src;
	m_rowSources.findEntries(row, src);
	if (src.size() < 1) //no descenddent
		return 0;
	vector<RowSource>::reverse_iterator rit = src.rbegin();
	for (; rit != src.rend(); rit++)
		if (isCDInScope(rit->cd)) 
			return rit->cd;
	return 0;
}

bool AlignmentCollection::isRowInScope(int row)const
{
	return GetScopedLeafCD(row)  != 0;
}

//scopedOnly=true
CCdCore* AlignmentCollection::GetSeniorMemberCD(int row, bool scopedOnly)const
{
	vector<RowSource> src;
	m_rowSources.findEntries(row, src);
	if (src.size() < 1) //no descenddent
		return 0;
	CCdCore* cd = src[0].cd;
	if (scopedOnly)
		if (!isCDInScope(cd))
			cd = 0;
	return cd;
}

void AlignmentCollection::getNormalRowsNotInChild(vector<int>& childless, bool excludeMaster) const
{

	int num = GetNumRows();
	int i = 0;
	if (excludeMaster)
		i = 1;
	for (; i < num; i++)
	{
		if (!IsPending(i))
		{
			CCdCore* cd = GetLeafDescendentCD(i,true);	
			if (cd == m_firstCd)
				childless.push_back(i);
		}
	}
}

void AlignmentCollection::convertFromCDRows(CCdCore* cd, const vector<int>& cdRows, set<int>& colRows)const
{
	m_rowSources.convertFromCDRows(cd, cdRows, colRows);
}

void AlignmentCollection::convertToCDRows(const vector<int>& colRows, CDRowsMap& cdRows) const
{
	m_rowSources.convertToCDRows(colRows,cdRows);
}

void AlignmentCollection::mapRows(const AlignmentCollection& ac, const set<int>& rows, set<int>& convertedRows) const
{
	CDRowsMap cdRowsMap;
	vector<int> rowVec;
	for (set<int>::const_iterator sit = rows.begin(); sit != rows.end(); sit++)
		rowVec.push_back(*sit);
	ac.convertToCDRows(rowVec, cdRowsMap);
	CDRowsMap::iterator cit = cdRowsMap.begin();
	for(; cit != cdRowsMap.end(); ++cit)
	{
		CCdCore* cd = cit->first;
		if (isCDInScope(cd))
		{
			vector<int>& cdRows = cit->second;
			convertFromCDRows(cd, cdRows, convertedRows);
		}
	}
}

int AlignmentCollection::mapRow(const AlignmentCollection& ac, int row) const
{
	vector<RowSource> src;
	ac.GetRowSourceTable().findEntries(row, src);
	for(unsigned int i = 0; i < src.size(); i++)
	{
		if (isCDInScope(src[i].cd))
		{
			return m_rowSources.convertFromCDRow(src[i].cd, src[i].rowInSrc);
		}
	}
	return -1;
}

void AlignmentCollection::getAllRowsForCD(CCdCore* cd, vector<int>& colRows)const
{
	int num = GetNumRows();
	for (int i = 0; i < num; i++)
	{
		if (m_rowSources.isRowInCD(i, cd))
			colRows.push_back(i);
	}
}

//  get the bioseq for the designated alignment row (for editing)
bool   AlignmentCollection::GetBioseqForRow(int row, CRef< CBioseq >& bioseq)
{
	if(m_bioseqs.size() == 0)
		m_bioseqs.assign(GetNumRows(), CRef< CBioseq >());
	if (m_bioseqs[row].Empty())
	{
		CRef< CSeq_id >  SeqID;
		GetSeqIDForRow(row, SeqID);
		bool gotit = m_seqTable.findSequence(SeqID, bioseq);
		if(!gotit)
		{
			CCdCore* myCd = m_rowSources.findEntry(row).cd;
			int seqIndex = myCd->GetSeqIndex(SeqID);
			gotit = myCd->GetBioseqForIndex(seqIndex, bioseq);
		}
		m_bioseqs[row] = bioseq;
		return gotit;
	}
	else
	{
		bioseq = m_bioseqs[row];
		return true;
	}
}

bool AlignmentCollection::GetSpeciesForRow(int row, string& species) const
{
	CRef< CSeq_id > SeqID;
	CCdCore* myCd = m_rowSources.findEntry(row).cd;
	if (GetSeqIDForRow(row, SeqID)) 
	{
		species = myCd->GetSpeciesForIndex(myCd->GetSeqIndex(SeqID));
		return true;
	}
	else
		return false;
}

string AlignmentCollection::GetDefline(int row) const
{
	CRef<CSeq_entry> seqEntry;
	 list< CRef< CSeqdesc > >::const_iterator  j;
	if (GetSeqEntryForRow(row, seqEntry))
	{
		if (seqEntry->IsSeq()) {
            if (seqEntry->GetSeq().IsSetDescr()) {
              // look through the sequence descriptions
              for (j=seqEntry->GetSeq().GetDescr().Get().begin();
                   j!=seqEntry->GetSeq().GetDescr().Get().end(); j++) {
					// if there's a title, return that description
					if ((*j)->IsTitle()) {
						return((*j)->GetTitle());
					}
					// if there's a pdb description, return it
					if ((*j)->IsPdb()) {
						if ((*j)->GetPdb().GetCompound().size() > 0) {
							return((*j)->GetPdb().GetCompound().front());
						}
					}
				}
			}
		}
	}
	return string();
}

bool AlignmentCollection::GetSeqEntryForRow(int row, CRef<CSeq_entry>& seqEntry) const
{
	CRef<CSeq_id> seqID;
	GetSeqIDForRow(row, seqID);
	return m_seqTable.findSequence(seqID, seqEntry);
}

//  Return all rows with the same Seq_id; if 'inclusive' = true, the
//  query row number is included in the returned vector, otherwise not.  
//  Number of entries in 'rows' is returned, regardless of value of 'inclusive'.
int AlignmentCollection::GetRowsWithSameSeqID(int rowToMatch, vector<int>& rows, bool inclusive) const {
    int i;
    int result = -1, index = -1;
    vector<int>::iterator vit, vend = rows.end(), vrow = vend;
    CRef<CSeq_id> sID;

    rows.clear();
    if (GetSeqIDForRow(rowToMatch, sID) && GetRowsWithSeqID(sID, rows) > 0) {
        for (vit = rows.begin(), i = 0; vit != vend; ++vit, ++i) {
            if (rowToMatch == *vit) {
                vrow = vit;
                index = i;
            }
        }
        if (!inclusive && vrow != vend) {
            rows.erase(vit);
        }
        result = (index != -1) ? (int) rows.size() : -1;
    }
    return result;
}

int AlignmentCollection::GetRowsWithSeqID(const CRef< CSeq_id >& SeqID, vector<int>& rows) const
{
	int num = GetNumRows();
	CRef<CSeq_id> sID;
	for (int i = 0; i < num; i++)
	{
		GetSeqIDForRow(i, sID);
		if (SeqIdsMatch(sID, SeqID))
			rows.push_back(i);
	}
	return rows.size();
}

int AlignmentCollection::FindSeqInterval (const CSeq_interval& seqLoc) const
{
	CSeq_id& seqId = const_cast< CSeq_id& > (seqLoc.GetId());
	CRef<CSeq_id> seqIdRef(&seqId);
	vector<int> rows;
	GetRowsWithSeqID(seqIdRef, rows);
	for (unsigned int i = 0; i < rows.size(); i++)
	{
		if ((int)seqLoc.GetFrom() >= GetLowerBound(rows[i]) 
			&& (int)seqLoc.GetTo() <= GetUpperBound(rows[i]))
			return rows[i];
	}
	return -1;	
}


int AlignmentCollection::GetLowerBound(int row) const
{
	CRef< CDense_diag > DenDiag;

	const CRef< CSeq_align >& seqAlign = m_seqAligns[row];
	if (seqAlign.NotEmpty() && GetFirstOrLastDenDiag(seqAlign, true, DenDiag)) {
		CDense_diag::TStarts::const_iterator  pos=DenDiag->GetStarts().begin();
		if (row !=0) //slave
			pos++;
		return (*pos);
	}
	return -1;
}

int AlignmentCollection::GetUpperBound(int row) const
{
	CRef< CDense_diag > DenDiag;

	const CRef< CSeq_align >& seqAlign = m_seqAligns[row];
	if (seqAlign.NotEmpty() && GetFirstOrLastDenDiag(seqAlign, false, DenDiag)) {
		CDense_diag::TStarts::const_iterator  pos=DenDiag->GetStarts().begin();
		if (row != 0) //slave
			pos++;
		TSeqPos len=DenDiag->GetLen();
		return (*pos)+(len) - 1;
	}
	return -1;
}

/*
void AlignmentCollection::EraseRows(CCdCore * pCD,vector < int > & rows) 
{
    int i,j;
    list< CRef< CUpdate_align > >::iterator  lit;

    for(j=rows.size()-1;j>=0;j--){
        //int jj=rows[j]-CCdCore::PENDING_ROW_START;
        for(i=0,lit = pCD->SetPending().begin(); lit != pCD->SetPending().end(); ++lit , i++) {
            if(i==rows[j]-CCdCore::PENDING_ROW_START){
                pCD->SetPending().erase(lit);
                break;
            }
        }
    }
}*/

void AlignmentCollection::GetAllSequences(vector<string>& sequences)
{
    sequences.clear();
	int numRow = GetNumRows();
    for (int i = 0; i < numRow; ++i) 
	{
        sequences.push_back(GetSequenceForRow(i));
    }
}

string AlignmentCollection::GetSequenceForRow(int row)
{
	string seq;
	CRef< CBioseq > bioseq;
	if (GetBioseqForRow(row, bioseq))
	{
		GetNcbieaaString(*bioseq, seq);
	}
	return seq;
}

void AlignmentCollection::GetAlignedResiduesForRow(int row, char*& aseq)
{
	string seq = GetSequenceForRow(row);
	if(aseq == 0)
		aseq = new char[GetAlignmentLength()];
	if (seq.size() > 0) 
	{
		bool isMaster = (row==0) && !IsPending(row);
        RowSource rs = GetRowSource(row);
        int gi; GetGI(row, gi, false);
        string s = rs.cd->GetAccession();
        //int r = rs.rowInSrc;
        //bool n = rs.normal;
		SetAlignedResiduesOnSequence(m_seqAligns[row], seq, aseq, isMaster);
     }
	return;
}


bool AlignmentCollection::AreNonOverlapping() const {
//---------------------------------------------------------------------
// check if all rows of m_seqAligns have non-overlapping den-diags
//---------------------------------------------------------------------
    int dummy;
    for (int i=0; i<GetNumRows(); i++) {
        if (!IsNonOverlapping(m_seqAligns[i], dummy)) {
            return(false);
        }
    }
    return(true);
}


bool AlignmentCollection::IsNonOverlapping(const CRef< CSeq_align >& align, int& blockIndex) const {
//---------------------------------------------------------------------
// check if den-diags of a single seq-align are non-overlapping
//---------------------------------------------------------------------
    TDendiag_cit iter;
    const TDendiag* pDenDiagSet;
    int Start, Start2, Stop=-1, Stop2=-1, Len;

    blockIndex = 0;
    if (GetDDSetFromSeqAlign(*align, pDenDiagSet)) {
        for (iter=pDenDiagSet->begin(); iter!=pDenDiagSet->end(); iter++) {
            Len = (*iter)->GetLen();
            Start = (*iter)->GetStarts().front();
            Start2 = (*iter)->GetStarts().back();
            // if start overlaps preceeding den-diag, return false
            if ((Stop >= 0) && ((Start <= Stop) || (Start2 <= Stop2))) {
                return(false);
            }
            Stop = Start + Len - 1;
            Stop2 = Start2 + Len - 1;
            blockIndex++;
        }
    }
    blockIndex = -1;
    return(true);
}


void AlignmentCollection::GetAlignedResiduesForAll(char** & ppAlignedResidues, bool forceRecompute)
{
//-------------------------------------------------------------------------
// allocate space for, and make, an array of all aligned residues
//-------------------------------------------------------------------------    
    string s;
    int numRows = GetNumRows();

    CRef< CSeq_align > seqAlign;

    // if space isn't allocated yet, allocate space for array of aligned residues
    if (ppAlignedResidues == NULL) 
	{
        ppAlignedResidues = new char*[numRows];
    } 
	else if (!forceRecompute) 
        return;

    // sanity check on all den-diags of all seqAligns
    //bool RetVal = AreNonOverlapping();

    for (int i = 0; i < numRows; i++) 
	{
		GetAlignedResiduesForRow(i, ppAlignedResidues[i] ); 
    }
}

bool AlignmentCollection::IsStruct(int row)
{
    bool result = false;
	CRef< CBioseq > bioSeq;
	if (GetBioseqForRow(row, bioSeq) && bioSeq.NotEmpty()) {
        //  Return -1 on failure; was FindMMDBIdInBioseq
        result = (GetMMDBId (*bioSeq) > 0);
    }
    return result;
}

bool AlignmentCollection::IsPdb(int row) const
{
	CRef< CSeq_id > seqId;
    return (GetSeqIDForRow(row, seqId) && seqId->IsPdb());
}

//MultipleAlignment Definition-----------------------------------------------------------------------

//create a MultipleAlignment with a CD
MultipleAlignment::MultipleAlignment(CCdCore* cd, bool scoped)
	: AlignmentCollection()
{
	setAlignment(cd, scoped);
}

//create a MultipleAlignment with a CD family
MultipleAlignment::MultipleAlignment(const CDFamily& family)
	: AlignmentCollection()
{
	setAlignment(family);
}

MultipleAlignment::MultipleAlignment() 
	: AlignmentCollection()
{
}

bool MultipleAlignment::setAlignment(const CDFamily& family)
{
	vector<CCdCore*> cds;
	family.getAllCD(cds);
	for (unsigned int i = 0; i < cds.size(); i++)
		AddSequence(cds[i]);
	CDFamilyIterator cit = family.begin();
	return setAlignment(family, cit);
}

bool MultipleAlignment::setAlignment(const CDFamily& family, CDFamilyIterator& start)
{
	bool result = true;
	// add root cd
	setAlignment(start->cd, start->selected);
	//add rows from children cds
	vector<CDFamilyIterator> cdits;
	family.getChildren(cdits, start);
	for (unsigned int i = 0; i < cdits.size(); i++)
	{
		MultipleAlignment tmp;
		tmp.setAlignment(family, cdits[i]); 
		int added = 0;
		if (start == family.begin()) //root
			added = appendAlignment(tmp,false);
		else
			added = appendAlignment(tmp,true);
		if (added <= 0)
		{
			result = false;
		}
		m_err += tmp.m_err;

	}
	return result;
}

void MultipleAlignment::setAlignment(CCdCore* cd, bool scoped)
{
	clear();
	AlignmentCollection::AddAlignment(cd, CCdCore::USE_NORMAL_ALIGNMENT,false,scoped);  //rowSourceTable need to be added
	makeBlockTable();
}

void MultipleAlignment::setAlignment(CRef<CSeq_align>& seqAlign)
{
	clear();
	BlockModel bmMaster(seqAlign, false);
	BlockModel bmSlave(seqAlign);
	m_blockTable.push_back(bmMaster);
	m_blockTable.push_back(bmSlave);
	m_seqAligns.push_back(seqAlign);//for master
	m_seqAligns.push_back(seqAlign);//for slave
}

bool MultipleAlignment::setAlignment(const AlignmentCollection& ac, int row)
{
	CRef<CSeq_align> saRef = ac.getSeqAlign(row);
	setAlignment(saRef);
	CRef<CSeq_id> seqID;
	GetSeqIDForRow(0, seqID);
	vector<int> matches;
	ac.GetRowsWithSeqID(seqID, matches);
	if (matches.size() <= 0)
		return false;
	copyRowSource(0, ac, matches[0]);
	copyRowSource(1, ac, row);
	GetRowSource(0).normal = true;
	GetRowSource(0).master = true;
	GetRowSource(1).normal = true;
	return true;
}

bool MultipleAlignment::isBlockAligned() const
{
	const BlockModel masterBM = getBlockModel(0);
	for (unsigned int i = 1; i < m_blockTable.size(); i++)
	{
		if (!masterBM.blockMatch(m_blockTable[i]))
			return false;
	}
	return true;
}

//-1, no child master in the parent
int MultipleAlignment::appendAlignment(MultipleAlignment& malign, bool includeUnScoped)
{
	int added = 0;
	int num = malign.m_blockTable.size();
	if (num <= 0)
		return 0;
	//find the master of malign in this
	int newMaster =0;
	
	if (!locateChildRow(malign.m_blockTable[0], newMaster))
	{
			if (newMaster >= 0)
				LOG_POST("The alignment of the master in cd "<<malign.getFirstCD()->GetAccession()<<" does not match with that of its parent and its content is not used.\n");
			else
				LOG_POST("The master of cd "<<malign.getFirstCD()->GetAccession()<<" is not found in its parent and its content is not used.");
			m_err += "error";
		return -1;
	}
	copyRowSource(newMaster, malign, 0);
	pair<DeltaBlockModel*, bool> deltaStatus = m_blockTable[newMaster] - malign.m_blockTable[0]; 
	if(!deltaStatus.second) //incomplete casting
	{	
		delete deltaStatus.first;
		LOG_POST("The master of cd "<<malign.getFirstCD()->GetAccession()<<" is not properly aligned to its parent and its content is not used.");
		m_err += "error";
		return -1;
	}
	for (int i = 1; i < num; i++)
	{
		/*
		//debug
		if (malign.GetLeafDescendentCD(i,true)->GetAccession() == "loc_28")
			bool guder =true;
		int gi;
		malign.GetGI(i, gi, false);
		if (gi ==9947913)
		{
			bool debug =true;
			//debug
			string cdacc = getFirstCD()->GetAccession();
			string childcdacc = malign.getFirstCD()->GetAccession();
			bool childrowScope = malign.isRowInScope(i);
		}*/
		int parentRow = -1;
		int seqLen = malign.GetSequenceForRow(i).size();
		pair<BlockModel*, bool> resultStatus = malign.m_blockTable[i] + *(deltaStatus.first);
		if (resultStatus.second) //complete cast
		{
            bool IsLegit = true;
            int blockIndex;
			bool shouldAdd = true;
			if (!includeUnScoped && !malign.isRowInScope(i))
				shouldAdd = false;
			if ( !findParentalEquivalent(*(resultStatus.first), parentRow) && shouldAdd) //is a new row, add it
			{
				//make a new CSeq_align
				CRef<CSeq_align> seqAlign = (resultStatus.first)->toSeqAlign(m_blockTable[0]);
                // check that seqAlign is legit
                //IsLegit = IsNonOverlapping(seqAlign, blockIndex);
				IsLegit = (resultStatus.first)->isValid(seqLen, blockIndex);
                if (IsLegit && !seqAlign.Empty()) {
				    added++;
				    //add the casted row blockModel to the block table
				    m_blockTable.push_back(*(resultStatus.first));
				    parentRow = m_blockTable.size() - 1;
				    //add it
				    assert(m_blockTable.size() == (m_seqAligns.size()+1));
				    m_seqAligns.push_back(seqAlign);
                }
                else {
                    LOG_POST("cd " <<malign.getFirstCD()->GetAccession()<<", row "<<i+1<<", does not have enough residues to shift block: "<<
						blockIndex+1<<" in order to fit it into its parental block.  This row is not used." );
					m_err += "error";
                }
			}
            if (IsLegit && (parentRow >= 0)) {
                copyRowSource(parentRow, malign, i);
            }
		}
		else //this row can not be cast to the parent's master blocks
		{
			LOG_POST("The blocks of "<<malign.getFirstCD()->GetAccession()<<", row "<<i<<" can not be fit into the blocks of "<<getFirstCD()->GetAccession()
				<<". It is thus not used to make the tree.");  
			m_err += "error";
		}
	}
	return added;
}

bool MultipleAlignment::locateChildRow(const BlockModel& childRow, int& pRow) const
{
	vector<int> seqIdMatches;
	GetRowsWithSeqID(childRow.getSeqId(), seqIdMatches);
	pRow = -1;

	for ( unsigned int i = 0; i < seqIdMatches.size(); i ++)
	{
		BlockModel* casted = childRow.completeCastTo(m_blockTable[seqIdMatches[i]]);
		pRow = seqIdMatches[i];
		if (casted != 0)
		{
			delete casted;
			return true;
		}
	}
	return false;
}

bool MultipleAlignment::findParentalEquivalent(const BlockModel& bar, int& pRow, bool inputAsChild) const
{
	for ( unsigned int i = 0; i < m_blockTable.size(); i ++)
	{
		bool found = false;
		if (inputAsChild)
			found = bar.contain(m_blockTable[i]);
		else
			found = m_blockTable[i].contain(bar);
		if (found)
		{
			pRow = (int) i;
			return true;
		}
	}
	return false;
}

bool MultipleAlignment::findParentalCastible(const BlockModel& bar, int& pRow) const
{
	for ( unsigned int i = 0; i < m_blockTable.size(); i ++)
	{
		bool found = false;
		BlockModel* bmp = m_blockTable[i].completeCastTo(bar);
		if (bmp)
			found = bmp->contain(bar);
		if (found)
		{
			pRow = (int) i;
			return true;
		}
	}
	return false;
}

void MultipleAlignment::copyRowSource(int parentRow, const AlignmentCollection& malign, int row)
{
	vector<RowSource> sources;
	malign.GetRowSourceTable().findEntries(row, sources);
	for (unsigned int i = 0; i < sources.size(); i++)
	{
		m_rowSources.addEntry(parentRow, *(new RowSource(sources[i])), 
			malign.GetRowSourceTable().isEntryInScope(sources[i]) );
	}
}

void MultipleAlignment::makeBlockTable()
{
	int i = 0; //master
	m_blockTable.push_back( *(new BlockModel(m_seqAligns[i], false)) );
	for (i = 1; i < GetNumRows(); i++)
	{
		m_blockTable.push_back( *(new BlockModel(m_seqAligns[i])) );
	}
}

const BlockModel& MultipleAlignment::getBlockModel(int row) const
{
	return m_blockTable[row];
}

bool MultipleAlignment::transferToCD(CCdCore* cd)
{
	if (cd ==0)
		cd = getFirstCD();
	int endRow = cd->GetNumRows();
	if (endRow <= 1) //empty cd
	{
		//add master sequence
		CRef<CSeq_entry> seqEntry;
		GetSeqEntryForRow(0, seqEntry);
		cd->AddSequence(seqEntry);
		endRow = 1;  //no need to add master alignment
	}
	for (int i = endRow; i < GetNumRows(); i ++)
	{
		transferOneRow(cd, i);
	}
	return true;
}

//return the row number of the newly-added row
int MultipleAlignment::transferOneRow(CCdCore* cd, int row)
{
	//do alignment
	if(IsPending(row))
		cd->AddPendingSeqAlign(m_seqAligns[row]);
	else
		cd->AddSeqAlign(m_seqAligns[row]);
	//do sequence
	CRef<CSeq_id> seqID;
	GetSeqIDForRow(row, seqID);
	if(cd->GetSeqIndex(seqID) < 0)  //there is no such sequence already in cd
	{
		CRef<CSeq_entry> seqEntry;
		GetSeqEntryForRow(row, seqEntry);
		cd->AddSequence(seqEntry);
	}
	return cd->GetNumRows() - 1;
}

bool MultipleAlignment::transferOnlyMastersToCD(bool onlyKeepStructureWithEvidence)
{
	CCdCore* cd = getFirstCD();
	vector<int> keepRows, masters;
	m_rowSources.getMasterRows(masters);
	int endRow = cd->GetNumRows();
	//add masters that are in cd yet
	for (unsigned int i = 0; i < masters.size(); i ++)
	{
		if (masters[i] >= endRow)
		{
			int row = transferOneRow(cd, masters[i]);
			keepRows.push_back(row);
		}
		else
			keepRows.push_back(masters[i]);
	}
	//also keep rows that are not in any child
	getNormalRowsNotInChild(keepRows,true);
	if (onlyKeepStructureWithEvidence)
		cd->GetStructuralRowsWithEvidence(keepRows);
	else //keep all structures
		cd->GetRowsWithMmdbId(keepRows);
	if (keepRows.size() < 2)
		keepRows.push_back(1);
	   // erase rows that weren't masters
    cd->EraseOtherRows(keepRows);
    // erase sequences no longer in alignment
    cd->EraseSequences();
    // erase structure evidence no longer in alignment
    //cd->EraseStructureEvidence();
	return true;
}

int MultipleAlignment::getNonEssentialRows(vector<int>& neRows)
{
	CCdCore* cd = getFirstCD();
	vector<int> masters, childless;
	set<int> keepRows;
	m_rowSources.getMasterRows(masters);
	int endRow = cd->GetNumRows();
	//add masters that are in cd yet
	for (unsigned int i = 0; i < masters.size(); i ++)
	{
		if (masters[i] < endRow)
			keepRows.insert(masters[i]);
	}
	//also keep rows that are not in any child
	getNormalRowsNotInChild(childless,true);
	cd->GetStructuralRowsWithEvidence(childless);
	for (unsigned int i = 0; i < childless.size(); i ++)
	{
		if (childless[i] < endRow)
			keepRows.insert(childless[i]);
	}
	for (int row = 0; row < endRow; row++)
	{
		set<int>::iterator sit = keepRows.find(row);
		if (sit == keepRows.end())
			neRows.push_back(row);
	}
	return neRows.size();
}

bool MultipleAlignment::extractAlignedSubset(const AlignmentCollection& ac, const set<int>& normalRows, 
								int master)
{
	
	//make a new SeqAlign
	BlockModel bmMaster(ac.getSeqAlign(master), !ac.wasMaster(master));
	int slave = 0;
	set<int>::const_iterator sit = normalRows.begin();
	// find the first slave
	for (; sit != normalRows.end(); ++sit)
	{
		slave  = *sit;
		if (slave!= master) 
		{
			sit++;
			break;
		}
	}
	BlockModel bmSlave(ac.getSeqAlign(slave), !ac.wasMaster(slave));
	if (!bmMaster.blockMatch(bmSlave))
		return false;
	m_blockTable.push_back(bmMaster);
	m_blockTable.push_back(bmSlave);
	//make a new CSeq_align and add it
	CRef<CSeq_align> seqAlign = bmSlave.toSeqAlign(bmMaster);
	m_seqAligns.push_back(seqAlign);//for master
	m_seqAligns.push_back(seqAlign);//for slave
	copyRowSource(0, ac, master);
	GetRowSource(0).master = true;
	copyRowSource(1, ac, slave);
	GetRowSource(1).master = false;

	//add the rest
	for (; sit != normalRows.end(); ++sit)
	{
		slave  = *sit;
		if (slave != master) 
		{	
			BlockModel bmSlave(ac.getSeqAlign(slave), !ac.wasMaster(slave));
			if (!bmMaster.blockMatch(bmSlave))
				return false;
			m_blockTable.push_back(bmSlave);
			CRef<CSeq_align> seqAlign = bmSlave.toSeqAlign(bmMaster);
			m_seqAligns.push_back(seqAlign);//for master
			int currRow = m_seqAligns.size() - 1;
			copyRowSource(currRow, ac, slave);
			GetRowSource(currRow).master = false;
		}
	}
	
	return true;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
