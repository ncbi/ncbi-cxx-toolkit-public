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
 * Author:  Charlie Liu
 *
 * File Description:
 *
 *       Make PSSM from a CD
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <algo/blast/api/psiblast_options.hpp>
#include <algo/blast/api/pssm_engine.hpp>
#include <algo/structure/cd_utils/cuSequence.hpp>
#include <algo/structure/cd_utils/cuPssmMaker.hpp>
#include <algo/structure/cd_utils/cuPssmScorer.hpp>
#include <algo/structure/cd_utils/cuScoringMatrix.hpp>
#include <objects/scoremat/PssmFinalData.hpp>
#include <objects/scoremat/PssmIntermediateData.hpp>
#include <objects/cdd/Cdd_id.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

static void
printMsa(const char* filename, const PSIMsa* msa, vector<string>& seqIds)
{
    Uint4 i, j;
    FILE* fp = NULL;

    ASSERT(msa);
    ASSERT(filename);

    fp = fopen(filename, "w");
	int startRow = msa->dimensions->num_seqs + 1 - seqIds.size();
	//if startRow == 1, this means row 0 is the consensus and should be ignored
	ASSERT(startRow >= 0);
    for (i = startRow; i < msa->dimensions->num_seqs + 1; i++) {
        fprintf(fp, ">%s\n", seqIds[i-startRow].c_str());
        for (j = 0; j < msa->dimensions->query_length; j++) {
            if (msa->data[i][j].is_aligned) {
				fprintf(fp, "%c", ColumnResidueProfile::getEaaCode(msa->data[i][j].letter));
            } else {
                fprintf(fp, ".");
            }
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
}


PssmMakerOptions::PssmMakerOptions()
:	pseudoCount(-1),
	scalingFactor(1.0),
	matrixName("BLOSUM62"),
	requestInformationContent(false),            
    requestResidueFrequencies(false),            
    requestWeightedResidueFrequencies(false),   
    requestFrequencyRatios(false),
    requestNumIndepObs(false),
    gaplessColumnWeights(false),
	unalignedSegThreshold(-1),
	inclusionThreshold(0.5),
	reuseUid(false)
{
};

//consensus is not in profiles
//row 0 if profiles is the master
CdPssmInput::CdPssmInput(ResidueProfiles& profiles, PssmMakerOptions& config, bool useConsensus)
  : m_profiles(profiles),m_options(0), m_useConsensus(useConsensus), m_diagRequest(),
    m_currentCol(0)
{
	PSIBlastOptionsNew(&m_options);
	if (m_useConsensus)
	{
		m_msaDimensions.num_seqs = m_profiles.getNumRows();
		m_msaDimensions.query_length = m_profiles.getConsensus(false).size();
		m_query = new unsigned char[m_msaDimensions.query_length];
		memcpy(m_query, m_profiles.getConsensus(false).data(), m_msaDimensions.query_length);
	}
	else
	{
		m_msaDimensions.num_seqs = m_profiles.getNumRows() - 1;
		string trunctMaster;
		m_msaDimensions.query_length = m_profiles.countColumnsOnMaster(trunctMaster);
		m_query = new unsigned char[m_msaDimensions.query_length];
	  	memcpy(m_query, trunctMaster.data(), m_msaDimensions.query_length);
	}
	m_msa = PSIMsaNew(&m_msaDimensions);
	//m_options.inclusion_ethresh = PSI_INCLUSION_ETHRESH;
	//determine pseudo count with information content
	if ( config.pseudoCount > 0 ) {
		m_options->pseudo_count = config.pseudoCount;
	}
	else
	{
		double SumAInf = profiles.calcInformationContent(m_useConsensus);
		int iPseudo = 1;
		if      (SumAInf > 84  ) iPseudo = 10;
    	else if (SumAInf > 55  ) iPseudo =  7;
    	else if (SumAInf > 43  ) iPseudo =  5;
    	else if (SumAInf > 41.5) iPseudo =  4;
    	else if (SumAInf > 40  ) iPseudo =  3;
    	else if (SumAInf > 39  ) iPseudo =  2;
    	else                     iPseudo =  1;
		m_options->pseudo_count = iPseudo;
	}
	m_options->nsg_compatibility_mode = true;
	//m_options.pseudo_count = PSI_PSEUDO_COUNT_CONST;
	//m_options.use_best_alignment = false;
	//m_options.nsg_ignore_consensus = m_useConsensus;
	//m_options.nsg_identity_threshold = 1.0;
	m_diagRequest.frequency_ratios = config.requestFrequencyRatios;
	m_diagRequest.gapless_column_weights = config.gaplessColumnWeights;
	m_diagRequest.information_content = config.requestInformationContent;
	m_diagRequest.residue_frequencies = config.requestResidueFrequencies;
	m_diagRequest.weighted_residue_frequencies = config.requestWeightedResidueFrequencies;
    m_diagRequest.independent_observations = config.requestNumIndepObs;
	m_matrixName = config.matrixName;
	m_options->impala_scaling_factor = config.scalingFactor;
}

void CdPssmInput::read(ColumnResidueProfile& crp)
{
	
	crp.getIndexByConsensus();
	int startingRow = 0;
	if (m_useConsensus)
		startingRow = 1;
	vector<char> residuesOnColumn;
	char gap = ColumnResidueProfile::getNcbiStdCode('-');
	residuesOnColumn.assign(m_profiles.getNumRows(), gap);
	crp.getResiduesByRow(residuesOnColumn);
	for (int row = 0; row < m_profiles.getNumRows(); row++)
	{
		m_msa->data[row+startingRow][m_currentCol].letter = residuesOnColumn[row];
		m_msa->data[row+startingRow][m_currentCol].is_aligned = true;
	}
	m_currentCol++;
}

CdPssmInput::~CdPssmInput()
{
	PSIMsaFree(m_msa);
	PSIBlastOptionsFree(m_options);
}

void CdPssmInput::Process()
{
	if (m_useConsensus)
	{
		//add consensus
		for (unsigned int i = 0; i < m_msaDimensions.query_length; i++)
		{
			m_msa->data[0][i].letter = m_query[i];
			m_msa->data[0][i].is_aligned = true;
		}
		m_profiles.traverseColumnsOnConsensus(*this);
	}
	else
		m_profiles.traverseColumnsOnMaster(*this);
	
	//printMsa("msaBefore.txt", m_msa);
	unalignLeadingTrailingGaps();
	//move the row with the most aligned residues to row 1
	//because row 1 will be used to filter out most identical sequences
	//moveUpLongestRow(); 	
  //	LOG_POST("num_seq="<<m_msaDimensions.num_seqs<<"query_len="<<m_msaDimensions.query_length);
	 
	//printMsa("msa.txt", m_msa);
}

void CdPssmInput::unalignLeadingTrailingGaps()
{
	char gap = ColumnResidueProfile::getNcbiStdCode('-');
	//row 0 is the query; does not have gaps
	//so we start at row 1
	for (unsigned int row = 1; row <= m_msaDimensions.num_seqs; row++)
	{
		unsigned int i = 0;
		for (; i < m_msaDimensions.query_length; i++)
		{
			if (m_msa->data[row][i].letter == gap)
				m_msa->data[row][i].is_aligned = false;
			else
				break;
		}
		for (unsigned int j = m_msaDimensions.query_length - 1; j > i; j--)
		{
			if (m_msa->data[row][j].letter == gap)
				m_msa->data[row][j].is_aligned = false;
			else
				break;
		}
	}
}

void CdPssmInput::moveUpLongestRow()
{
	int longestRow = 1;
	int maxLen = countResiduesInRow(longestRow);

	for (int row = 2; row <= (int) m_msaDimensions.num_seqs; row++)
	{
		int len = countResiduesInRow(row);
		if (len > maxLen)
		{
			maxLen = len;
			longestRow = row;
		}
	}
	if (longestRow != 1)
	{
		PSIMsaCell* tmp = (PSIMsaCell*)calloc(m_msaDimensions.query_length, sizeof(PSIMsaCell));;
		copyRow(m_msa->data[1], tmp);
		copyRow(m_msa->data[longestRow], m_msa->data[1]);
		copyRow(tmp, m_msa->data[longestRow]);
		free(tmp);
	}
}

void CdPssmInput::copyRow(PSIMsaCell* src, PSIMsaCell* dest)
{
	for (unsigned int i = 0; i < m_msaDimensions.query_length; i++)
	{
		dest[i].is_aligned = src[i].is_aligned;
		dest[i].letter = src[i].letter;
		//memcpy(&(dest[i]), &(src[i]), sizeof(PSIMsaCell));
	}
}

int CdPssmInput::countResiduesInRow(int row)
{
	int count = 0;
	for (unsigned int i = 0; i < m_msaDimensions.query_length; i++)
	{
		if (m_msa->data[row][i].is_aligned)
			count++;
	}
	return count;
}

    /// Get the query sequence used as master for the multiple sequence
    /// alignment in ncbistdaa encoding.
unsigned char* CdPssmInput::GetQuery()
{
	return m_query;
}

    /// Get the query's length
unsigned int CdPssmInput::GetQueryLength()
{
	return m_msaDimensions.query_length;
}

    /// Obtain the multiple sequence alignment structure
PSIMsa* CdPssmInput::GetData()
{
	return m_msa; 
}

    /// Obtain the options for the PSSM engine
const PSIBlastOptions* CdPssmInput::GetOptions()
{
	return m_options;
}

 /// Obtain the options for the PSSM engine
PSIBlastOptions* CdPssmInput::SetOptions()
{
	return m_options;
}

const char* CdPssmInput::GetMatrixName()
{
	return m_matrixName.c_str();
}

    /// Obtain the diagnostics data that is requested from the PSSM engine
    /// Its results will be populated in the PssmWithParameters ASN.1 object
const PSIDiagnosticsRequest* CdPssmInput::GetDiagnosticsRequest()
{
	return &m_diagRequest;
}

//------------------------- PssmMaker ---------------------
PssmMaker::PssmMaker(CCdCore* cd, bool useConsensus, bool addQueryToPssm) 
	: m_conMaker(0), m_useConsensus(useConsensus), m_addQuery(addQueryToPssm), 
	m_masterSeqEntry(), m_trunctMaster(), m_cd(cd), m_pssmInput(0)
	//m_identityFilterThreshold(0.94)
{
	CRef< CSeq_id > seqId;
	cd->GetSeqIDFromAlignment(0, seqId);
	if (!IsConsensus(seqId))
		cd->GetSeqEntryForRow(0, m_masterSeqEntry);
	else //if consensus is master
	{
		//use master because it is already a consensus
		//note this override the input useConsensus
		m_useConsensus = false; 
		vector<int> seqIndice;
		cd->FindConsensusInSequenceList(&seqIndice);
		if (seqIndice.size() > 0)
			cd->GetSeqEntryForIndex(seqIndice[0], m_masterSeqEntry);
	}
}

void PssmMaker::setOptions(const PssmMakerOptions& option)
{
	m_config = option;
}

PssmMaker::~PssmMaker()
{
	if (m_pssmInput)
		delete m_pssmInput;
	if (m_conMaker)
		delete m_conMaker;
}

CRef<CPssmWithParameters> PssmMaker::make()
{
	m_conMaker = new ConsensusMaker(m_cd, m_config.inclusionThreshold);
	if (m_config.unalignedSegThreshold >= 0)
	{
		m_conMaker->skipUnalignedSeg(m_config.unalignedSegThreshold);
	}
	m_pssmInput = new CdPssmInput (m_conMaker->getResidueProfiles(), m_config,m_useConsensus);
	if (!m_useConsensus)
		for(unsigned int i = 0 ; i < m_pssmInput->GetQueryLength(); i++)
			m_trunctMaster.push_back(m_pssmInput->GetQuery()[i]);
	CPssmEngine pssmEngine(m_pssmInput);
	m_pseudoCount = m_pssmInput->GetOptions()->pseudo_count;
	/*
	if (m_identityFilterThreshold > 0)
		pssmInput.SetOptions()->nsg_identity_threshold = m_identityFilterThreshold;
		*/
	CRef<CPssmWithParameters> pssmRef;
	try {
		pssmRef = pssmEngine.Run();
	}catch (...)
	{
		pssmRef.Reset();
	};
	if (pssmRef.Empty())
	{
		pssmRef = makeDefaultPssm();
	}
	if (m_addQuery)
	{
		CRef< CSeq_entry > query;
		if(m_useConsensus)
			query = m_conMaker->getConsensusSeqEntry();
		else
		{
			query = new CSeq_entry;
			query->Assign(*m_masterSeqEntry);
			getTrunctMaster(query);
		}
		modifyQuery(query); 
		pssmRef->SetPssm().SetQuery(*query);
	}
	m_pssmMade = pssmRef;
	return pssmRef;
}

CRef<CPssmWithParameters> PssmMaker::makeDefaultPssm()
{
	EScoreMatrixType emt;
	bool found = false;
	int i = 0;
	for (i = eBlosum45; i <= ePam250 ; i++)
	{
		if (NStr::CompareNocase(m_config.matrixName, 
			GetScoringMatrixName((EScoreMatrixType)i)) == 0)
		{
			found = true;
			break;
		}
	}
	if (found )
		emt = (EScoreMatrixType)i;
	else
		emt = eBlosum62;
	ScoreMatrix sm(emt);
	string consensus;
	if(m_useConsensus)
		consensus = m_conMaker->getConsensus();
	else
	{
		NcbistdaaToNcbieaaString(m_trunctMaster, &consensus);
	}
	CRef<CPssmWithParameters> pssmPara(new CPssmWithParameters);
	CPssm& pssm = pssmPara->SetPssm();
	pssm.SetNumColumns(consensus.size());
	int numRows = ColumnResidueProfile::m_residues.size();
	pssm.SetNumRows(numRows);
	list< double >* freqs = 0;
	if (m_config.requestFrequencyRatios)
	{
		freqs = &(pssm.SetIntermediateData().SetFreqRatios());
	}
	list< int > & scores = pssm.SetFinalData().SetScores();
	for (unsigned int col = 0; col < consensus.size(); col++)
	{
		char c1 = consensus.at(col);
		for (char row = 0; row < numRows; row++)
		{
			char c2 =  ColumnResidueProfile::getEaaCode(row);
			int score = m_config.scalingFactor * sm.GetScore(c1, c2);
			scores.push_back(score);
			if (freqs)
				freqs->push_back(0.0);
		}
	}
	pssm.SetFinalData().SetLambda(0.267);
	pssm.SetFinalData().SetKappa(0.0447);
	pssm.SetFinalData().SetH(0.140);
	if (m_config.scalingFactor > 1)
		pssm.SetFinalData().SetScalingFactor((int)m_config.scalingFactor);
	return pssmPara;
}

void PssmMaker::modifyQuery(CRef< CSeq_entry > query)
{
    static const string commaSpace(", ");
    static const string periodSpaceSpace(".  ");
	CBioseq& bioseq = query->SetSeq();
	bioseq.ResetId();
	list< CRef< CSeq_id > > & ids = bioseq.SetId();
	CRef< CSeq_id > seqId(new CSeq_id);
	CDbtag& dbtag = seqId->SetGeneral();
	//dbtag.SetDb("CDD");
	CObject_id& obj = dbtag.SetTag();
	list< CRef< CCdd_id > >& cdids = m_cd->SetId().Set();
	int uid = -1;
	list< CRef< CCdd_id > >::iterator cit = cdids.begin();
	for (; cit != cdids.end(); cit++)
	{
		if ((*cit)->IsUid())
		{
			uid = (*cit)->GetUid();
			break;
		}
	}
	if (cit != cdids.end() && m_config.reuseUid)
	{
		obj.SetId(uid);
		dbtag.SetDb("CDD");
	}
	else
	{
		obj.SetStr(m_cd->GetAccession());
		dbtag.SetDb("Cdd");
	}
	ids.push_back(seqId);
	//add a decr field
	list< CRef< CSeqdesc > >& descList = bioseq.SetDescr().Set();
	CRef< CSeqdesc > desc(new CSeqdesc);

    //  Get the CD title.  
    //  Chop leading/trailing spaces, and terminating '.' characters.
    string cdTitle(m_cd->GetTitle());
    NStr::TruncateSpacesInPlace(cdTitle);
    if (cdTitle.length() > 0) {
        while (NStr::EndsWith(cdTitle, '.')) {
            cdTitle = cdTitle.substr(0, cdTitle.length() - 1);
        }
    }

	string seqDescTitle(m_cd->GetAccession());
	seqDescTitle += commaSpace;
	seqDescTitle += m_cd->GetName();

    //  If this is a consensus sequence, prepend the title to any comment.
    //  Do this here in case there are no comments.
    if (m_useConsensus && cdTitle.length() > 0) {
        seqDescTitle += commaSpace + cdTitle + periodSpaceSpace;
    }

	list< CRef< CCdd_descr > >& cddescList = m_cd->SetDescription().Set();
	list< CRef< CCdd_descr > >::iterator lit = cddescList.begin();
	
	for (; lit != cddescList.end(); lit++)
	{
		if ((*lit)->IsComment())
		{
            if (!m_useConsensus || cdTitle.length() == 0) {
                seqDescTitle += commaSpace;
            }
			seqDescTitle += (*lit)->GetComment();
            if (!NStr::EndsWith(seqDescTitle, '.')) {
                seqDescTitle += '.';
            }

            //  only take the first comment
			break; 
		}
	}
	desc->SetTitle(seqDescTitle);
	list< CRef< CSeqdesc > >::iterator it = descList.begin();
	for(; it != descList.end(); it++)
		if ( (*it)->IsTitle() ) {
			descList.erase(it);
			break;
		}
	descList.push_back(desc);
}

const BlockModelPair& PssmMaker::getGuideAlignment()
{
	BlockModelPair& bmp = m_conMaker->getGuideAlignment();
	CRef< CSeq_id > seqId = *(m_pssmMade->SetPssm().SetQuery().SetSeq().SetId().begin());
	bmp.getSlave().setSeqId(seqId);
	return bmp;
}

const string& PssmMaker::getConsensus()
{
	return m_conMaker->getConsensus();
}

//seqId in seqEntry is kept.
//seqInst is replaced with trunct master.
bool PssmMaker::getTrunctMaster(CRef< CSeq_entry >& seqEntry)
{
	if (m_useConsensus)
		return false;
	CBioseq& bioseq = seqEntry->SetSeq();
	CSeq_inst& seqInst = bioseq.SetInst();
	seqInst.SetLength(m_trunctMaster.size());
	seqInst.ResetSeq_data();
	string eaa;
	NcbistdaaToNcbieaaString(m_trunctMaster, &eaa);
	seqInst.SetSeq_data(*(new CSeq_data(eaa, CSeq_data::e_Ncbieaa)));
	//CSeq_data& seqData = seqInst.SetSeq_data();
	//seqData.SetNcbieaa(*(new CSeq_data::Ncbistdaa(m_trunctMaster)));
	return true;
}

void PssmMaker::printAlignment(string& fileName)
{
	vector<string> seqIdStr;
	const vector< CRef< CSeq_id > >& seqIds = m_conMaker->getResidueProfiles().getSeqIdsByRow();
	if (!IsConsensus(seqIds[0]))
		seqIdStr.push_back(seqIds[0]->AsFastaString());
	for (unsigned int i = 1; i < seqIds.size(); i++)
	{
		seqIdStr.push_back(seqIds[i]->AsFastaString());
	}

	printMsa(fileName.c_str(), m_pssmInput->GetData(), seqIdStr);
}

void PssmMaker::printAlignmentByColumn(string& fileName)
{
    if (fileName.length() == 0) {
        return;
    }

    Uint4 i, j;
    unsigned int nRows, nCols;
	vector<string> seqIdStr;
	const vector< CRef< CSeq_id > >& seqIds = m_conMaker->getResidueProfiles().getSeqIdsByRow();
    const PSIMsa& msa = *m_pssmInput->GetData();

    FILE* fp = fopen(fileName.c_str(), "w");

    nCols = msa.dimensions->query_length;
    nRows = msa.dimensions->num_seqs;

	if (!IsConsensus(seqIds[0]))
		seqIdStr.push_back(seqIds[0]->AsFastaString());
	for (unsigned int k = 1; k < seqIds.size(); k++)
	{
		seqIdStr.push_back(seqIds[k]->AsFastaString());
	}
    for (i = 0; i < seqIdStr.size(); ++i) {
        fprintf(fp, "row %d:  %s\n", i,seqIdStr[i].c_str());
    }

//    int startRow = 0; //msa->dimensions->num_seqs + 1 - seqIds.size();
	//if startRow == 1, this means row 0 is the consensus and should be ignored
//	ASSERT(startRow >= 0);
    static const string dash("-");
    fprintf(fp, "Query length:  %d;  Number of rows:  %d\n", nCols, nRows);
    for (j = 0; j < nCols; j++) {
        fprintf(fp, ">column %d\n", j+1);
        for (i = 0; i < nRows + 1; i++) {
            if (msa.data[i][j].is_aligned) {
				fprintf(fp, "%c", ColumnResidueProfile::getEaaCode(msa.data[i][j].letter));
            } else {
                fprintf(fp, "-");
            }
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
}

void PssmMaker::getPssmColumnResidues(map<unsigned int, string>& columnMap)
{
    unsigned int i, j, nRows, nCols;
    string colResidues;

    const PSIMsa& msa = *m_pssmInput->GetData();
    nCols = msa.dimensions->query_length;
    nRows = msa.dimensions->num_seqs;

    columnMap.clear();

    for (j = 0; j < nCols; j++) {
        colResidues.clear();
        colResidues.assign(nRows + 1, '-');
        for (i = 0; i < nRows + 1; i++) {
            if (msa.data[i][j].is_aligned) {
                colResidues[i] = ColumnResidueProfile::getEaaCode(msa.data[i][j].letter);
            } else {
                colResidues[i] = '-';
            }
        }
        columnMap[j] = colResidues;
    }
}

int findHighestScoringRowByPssm(CCdCore* ccd)
{
	cd_utils::PssmMaker pm(ccd,true,true);   // 2rd param is useConsensus.  generally "true".
	cd_utils::PssmMakerOptions config;
	config.requestFrequencyRatios = false;
	pm.setOptions(config);
	CRef<CPssmWithParameters> pssm = pm.make();
	const BlockModelPair& guide = pm.getGuideAlignment();
	int max = 0;
	int maxRow = 0;
	PssmScorer ps(pssm);
	CRef<CBioseq> bioseq;
	for (int i = 0; i < ccd->GetNumRows(); i++)
	{
		ccd->GetBioseqForRow(i, bioseq);
		BlockModelPair bmp(ccd->GetSeqAlign(i));
		if (i==0) //score the master
			bmp.getSlave() = bmp.getMaster();
		bmp.remaster(guide);
		int score = ps.score(bmp,bioseq);
		if (score > max)
		{
			max = score;
			maxRow = i;
		}
	}
	return maxRow;
}


END_SCOPE(cd_utils)
END_NCBI_SCOPE
