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
#include <algo/blast/api/psiblast_options.hpp>
#include <algo/blast/api/blast_psi.hpp>
//#include <algo/blast/blast_psi_priv.h>
#include <algo/structure/cd_utils/cuSequence.hpp>
#include <algo/structure/cd_utils/cuPssmMaker.hpp>
#include <objects/cdd/Cdd_id.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

static void
printMsa(const char* filename, const PSIMsa* msa)
{
    Uint4 i, j;
    FILE* fp = NULL;

    ASSERT(msa);
    ASSERT(filename);

    fp = fopen(filename, "w");
	 

    for (i = 0; i < msa->dimensions->num_seqs + 1; i++) {
        /*fprintf(fp, "%3d\t", i);*/
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
    gaplessColumnWeights(false)
{
};

//consensus is not in profiles
//row 0 if profiles is the master
CdPssmInput::CdPssmInput(ResidueProfiles& profiles, PssmMakerOptions& config, bool useConsensus)
: m_profiles(profiles),m_options(0), m_useConsensus(useConsensus), m_currentCol(0),
	m_diagRequest()
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
	m_matrixName = config.matrixName;
	m_options->impala_scaling_factor = config.scalingFactor;
}

void CdPssmInput::read(ColumnResidueProfile& crp)
{
	
	int conIndex = crp.getIndexByConsensus();
	/*
	if (m_currentCol == 89)
				bool check = true;*/
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
		//m_msa->data[row+startingRow][m_currentCol].is_aligned = crp.isAligned(residuesOnColumn[row], row);
		//m_msa->data[row+startingRow][m_currentCol].is_aligned = (residuesOnColumn[row] != gap);
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
		for (int i = 0; i < m_msaDimensions.query_length; i++)
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
	for (int row = 1; row <= m_msaDimensions.num_seqs; row++)
	{
		int i = 0;
		for (; i < m_msaDimensions.query_length; i++)
		{
			if (m_msa->data[row][i].letter == gap)
				m_msa->data[row][i].is_aligned = false;
			else
				break;
		}
		for (int j = m_msaDimensions.query_length - 1; j > i; j--)
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

	for (int row = 2; row <= m_msaDimensions.num_seqs; row++)
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
	for (int i = 0; i < m_msaDimensions.query_length; i++)
	{
		dest[i].is_aligned = src[i].is_aligned;
		dest[i].letter = src[i].letter;
		//memcpy(&(dest[i]), &(src[i]), sizeof(PSIMsaCell));
	}
}

int CdPssmInput::countResiduesInRow(int row)
{
	int count = 0;
	for (int i = 0; i < m_msaDimensions.query_length; i++)
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
	: m_conMaker(cd), m_useConsensus(useConsensus), m_trunctMaster(),
	m_masterSeqEntry(), m_addQuery(addQueryToPssm), m_cd(cd)
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

}

CRef<CPssmWithParameters> PssmMaker::make()
{
	CdPssmInput pssmInput(m_conMaker.getResidueProfiles(), m_config,m_useConsensus);
	if (!m_useConsensus)
		for(int i = 0 ; i < pssmInput.GetQueryLength(); i++)
			m_trunctMaster.push_back(pssmInput.GetQuery()[i]);
	CPssmEngine pssmEngine(&pssmInput);
	m_pseudoCount = pssmInput.GetOptions()->pseudo_count;
	/*
	if (m_identityFilterThreshold > 0)
		pssmInput.SetOptions()->nsg_identity_threshold = m_identityFilterThreshold;
		*/
    CRef<CPssmWithParameters> pssmRef = pssmEngine.Run();
	if (m_addQuery)
	{
		CRef< CSeq_entry > query;
		if(m_useConsensus)
			query = m_conMaker.getConsensusSeqEntry();
		else
		{
			query = new CSeq_entry;
			query->Assign(*m_masterSeqEntry);
			getTrunctMaster(query);
		}
		modifyQuery(query); 
		pssmRef->SetPssm().SetQuery(*query);
	}
	return pssmRef;
}

BLAST_Matrix*  PssmMaker::makeBlastMatrix()
{
	return PssmToBLAST_Matrix(*make());
}

void PssmMaker::modifyQuery(CRef< CSeq_entry > query)
{
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
	if (cit != cdids.end())
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
	string title(m_cd->GetAccession());
	title += ", ";
	title += m_cd->GetName();
	list< CRef< CCdd_descr > >& cddescList = m_cd->SetDescription().Set();
	list< CRef< CCdd_descr > >::iterator lit = cddescList.begin();
	
	for (; lit != cddescList.end(); lit++)
	{
		if ((*lit)->IsComment())
		{
			title += ", ";
			title += (*lit)->GetComment();
			title += '.';
			break; //only take the first comment
		}
	}
	desc->SetTitle(title);
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
	return m_conMaker.getGuideAlignment();
}

const string& PssmMaker::getConsensus()
{
	return m_conMaker.getConsensus();
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

