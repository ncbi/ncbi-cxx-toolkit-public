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
 * Author:  Vahram Avagyan
 *
 */

#include <ncbi_pch.hpp>
#include <internal/blast/vdb/vdb2blast_util.hpp>
#include <objmgr/impl/data_source.hpp>
#include <sra/data_loaders/csra/impl/csraloader_impl.hpp>
#include "vdb_priv.h"
#include "vdbsequtil.h"

BEGIN_NCBI_SCOPE
USING_SCOPE(blast);
USING_SCOPE(objects);

// ==========================================================================//
// Constants

// ==========================================================================//

/// CVDBSeqInfoSrc
///
/// Implementation of the IBlastSeqInfoSrc interface for SRA databases.
///
/// This class can be used internally by the Blast API classes to generate
/// SeqIDs and other high-level sequence information from ordinal numbers
/// (OIDs), which represent the implementation-level numbering of sequences.
/// This class communicates with the CVDBBlastUtil class to convert
/// OIDs to SRA-specific SeqIDs.

class CVDBSeqInfoSrc : public blast::IBlastSeqInfoSrc
{
public:
    /// Constructor taking a CVDBBlastUtil object.
    /// @param sraBlastUtil Properly initialized CVDBBlastUtil object [in]
    CVDBSeqInfoSrc(CRef<CVDBBlastUtil> sraBlastUtil);

    /// Destructor.
    virtual ~CVDBSeqInfoSrc();

    /// Method to retrieve a sequence identifier given its ordinal number.
    /// @param oid the ordinal number to retrieve [in]
    /// @return list of SeqIDs identifying this sequence.
    virtual list< CRef<objects::CSeq_id> > GetId(Uint4 oid) const;

    /// Method to retrieve the sequence location given its ordinal number.
    /// @param oid the ordinal number to retrieve [in]
    /// @return a SeqLoc identifying this sequence.
    virtual CConstRef<objects::CSeq_loc> GetSeqLoc(Uint4 oid) const;

    /// Method to retrieve a sequence length given its ordinal number.
    /// @param oid the ordinal number to retrieve [in]
    /// @return length of the sequence.
    virtual Uint4 GetLength(Uint4 oid) const;

    /// Returns the size of the underlying container of sequences
    /// @return total size of all the sequences.
    virtual size_t Size() const;

    /// Returns true if the subject is restricted by a GI list,
    /// always returns false in this implementation.
    virtual bool HasGiList() const;

    /// Retrieves the subject masks for the corresponding oid,
    /// always returns false in this implementation.
    virtual bool GetMasks(Uint4 oid, 
                          const vector<TSeqRange>& target_ranges,
                          TMaskedSubjRegions& retval) const;

    /// Retrieves the subject masks for the corresponding oid,
    /// always returns false in this implementation.
    virtual bool GetMasks(Uint4 oid, 
                          const TSeqRange& target_range,
                          TMaskedSubjRegions& retval) const;

    /// Return true if the implementation can return anything besides a seq-loc
    /// for the entire sequence.  If in doubt, the implementation must
    /// return true.
    virtual bool CanReturnPartialSequence() const {return true;}

private:
    /// The CVDBBlastUtil object that takes care of various conversions.
    mutable CRef<CVDBBlastUtil> m_sraBlastUtil;
};

// ==========================================================================//
// CVDBSeqInfoSrc implementation

CVDBSeqInfoSrc::CVDBSeqInfoSrc(CRef<CVDBBlastUtil> sraBlastUtil)
: m_sraBlastUtil(sraBlastUtil)
{
    ASSERT(m_sraBlastUtil.NotEmpty());
}

CVDBSeqInfoSrc::~CVDBSeqInfoSrc()
{
	m_sraBlastUtil.Reset();
}

list< CRef<CSeq_id> > CVDBSeqInfoSrc::GetId(Uint4 oid) const
{
    CRef<CSeq_id> seqIdVDB =
        m_sraBlastUtil->GetVDBSeqIdFromOID(oid);
    ASSERT(seqIdVDB.NotEmpty());

    list< CRef<CSeq_id> > listIds;
    listIds.push_back(seqIdVDB);

    return listIds;
}

CConstRef<CSeq_loc> CVDBSeqInfoSrc::GetSeqLoc(Uint4 oid) const
{
    list< CRef<CSeq_id> > listIds = GetId(oid);
    ASSERT(!listIds.empty());

    CRef<CSeq_loc> seqLoc(new CSeq_loc);
    seqLoc->SetWhole().Assign(**listIds.begin());

    return seqLoc;
}

Uint4 CVDBSeqInfoSrc::GetLength(Uint4 oid) const
{
    BlastSeqSrc* seqSrc = m_sraBlastUtil->GetSRASeqSrc();
    ASSERT(seqSrc);

    return BlastSeqSrcGetSeqLen(seqSrc, (void*) &oid);

}

size_t CVDBSeqInfoSrc::Size() const
{
    BlastSeqSrc* seqSrc = m_sraBlastUtil->GetSRASeqSrc();
    ASSERT(seqSrc);
    return BlastSeqSrcGetNumSeqs(seqSrc);
}

bool CVDBSeqInfoSrc::HasGiList() const
{
    return false;
}

bool CVDBSeqInfoSrc::GetMasks(Uint4 oid,
              const vector<TSeqRange>& target_ranges,
              TMaskedSubjRegions& retval) const
{
    return false;
}

bool CVDBSeqInfoSrc::GetMasks(Uint4 oid,
                          const TSeqRange& target_range,
                          TMaskedSubjRegions& retval) const
{
    return false;
}

// ==========================================================================//
/// CVDBBlastUtil implementation

void
CVDBBlastUtil::x_GetSRARunAccessions(vector<string>& vecSRARunAccessions)
{
    try
    {
        NStr::Split(m_strAllRuns, " ", vecSRARunAccessions, NStr::fSplit_Tokenize);
        // remove any redundancy
        set<string> string_set;
        copy(vecSRARunAccessions.begin(),
             vecSRARunAccessions.end(),
             inserter(string_set, string_set.begin()));
        vecSRARunAccessions.clear();
        copy(string_set.begin(), string_set.end(),
             back_inserter(vecSRARunAccessions));
    }
    catch (...)
    {
        NCBI_THROW(CException, eUnknown,
                   "Failed to process SRA accession list: " + m_strAllRuns);
    }

    if (vecSRARunAccessions.empty())
    {
        NCBI_THROW(CException, eUnknown,
                   "Invalid SRA accession list: " + m_strAllRuns);
    }
}


BlastSeqSrc*
CVDBBlastUtil::x_MakeVDBSeqSrc()
{
    // Parse the SRA run accessions
    vector<string> vecSRARunAccessions;
    x_GetSRARunAccessions(vecSRARunAccessions);

    // Prepare the input data for SRA BlastSeqSrc construction
    Uint4 numRuns = vecSRARunAccessions.size();
    char** vdbRunAccessions = new char*[numRuns];
    for (Uint4 iRun = 0; iRun < numRuns; iRun++) {
        if (!vecSRARunAccessions[iRun].empty()) {
            vdbRunAccessions[iRun] =
                strdup(vecSRARunAccessions[iRun].c_str());
        }
    }

    Boolean * isRunExcluded = new Boolean[numRuns];
    Uint4 rc;
    // Construct the BlastSeqSrc object
    BlastSeqSrc* seqSrc =
        SRABlastSeqSrcInit((const char**)vdbRunAccessions, numRuns, false, isRunExcluded, &rc, m_isCSRAUtil);

    string excluded_runs= kEmptyStr;
    // Clean up
    for (Uint4 iRun = 0; iRun < numRuns; iRun++)
    {
    	if(isRunExcluded[iRun]) {
    		excluded_runs += vdbRunAccessions[iRun];
    		excluded_runs += " ";
    	}
    }
    delete [] vdbRunAccessions;
    delete [] isRunExcluded;

    if(rc > 0 && excluded_runs != kEmptyStr) {
    	if(seqSrc)
    		BlastSeqSrcFree(seqSrc);
    	NCBI_THROW(CException, eUnknown,
    	            "Error opening the following db(s): " + excluded_runs);
    }


    if (!seqSrc) {
        NCBI_THROW(CException, eUnknown,
                   "Failed to construct the VDB BlastSeqSrc object");
    }

    // Check for errors
    char* errMsg = BlastSeqSrcGetInitError(seqSrc);
    if (errMsg) {
        string strErrMsg(errMsg);
        free(errMsg);
        BlastSeqSrcFree(seqSrc);
        NCBI_THROW(CException, eUnknown,
                   "VDB BlastSeqSrc construction failed: " + strErrMsg);
    }


    return seqSrc;
}

CVDBBlastUtil::CVDBBlastUtil(const string& strAllRuns,
                             bool bOwnSeqSrc,
                             bool bCSRA):
                             m_bOwnSeqSrc(bOwnSeqSrc), m_strAllRuns(strAllRuns), m_isCSRAUtil(bCSRA)
{
    m_seqSrc = x_MakeVDBSeqSrc();
    if( VDBSRC_OVERFLOW_RV == BlastSeqSrcGetNumSeqs(m_seqSrc)) {
        NCBI_THROW(CException, eUnknown, "VDB Num of seqs overflow");
    }
}

CVDBBlastUtil::~CVDBBlastUtil()
{
    if (m_bOwnSeqSrc)
    {
        BlastSeqSrcFree(m_seqSrc);
    }
}

BlastSeqSrc*
CVDBBlastUtil::GetSRASeqSrc()
{
    if (!m_seqSrc)
    {
        NCBI_THROW(CException, eUnknown, "VDB BlastSeqSrc is not available");
    }

    return m_seqSrc;
}

CRef<IBlastSeqInfoSrc>
CVDBBlastUtil::GetSRASeqInfoSrc()
{
    if (!m_seqSrc)
    {
        NCBI_THROW(CException, eUnknown, "VDB BlastSeqSrc is not available");
    }

    CRef<CVDBBlastUtil> refThis(this);
    CRef<IBlastSeqInfoSrc> infoSrc(new CVDBSeqInfoSrc(refThis));
    return infoSrc;
}

static bool 
s_IsWGSId(const string & id)
{
	if (CSeq_id::IdentifyAccession(id) & CSeq_id::eAcc_wgs) {
		return true;
	}

    return false;
}

Uint4
CVDBBlastUtil::GetOIDFromVDBSeqId(CRef<CSeq_id> seqId)
{
    if (!m_seqSrc) {
        NCBI_THROW(CException, eUnknown, "VDB BlastSeqSrc is not available");
    }

    // Decode the tag to collect the SRA-specific indices
    const char * readName = NULL;
    string nameStr= kEmptyStr;
    if(seqId->IsGeneral()) {
    	if (!seqId->GetGeneral().CanGetDb() ||
    		!seqId->GetGeneral().CanGetTag() ||
    		!seqId->GetGeneral().GetTag().IsStr()) {
        	NCBI_THROW(CException, eUnknown,
            	       "Incomplete SeqID for SRA sequence");
    	}

    	// Decode the tag to collect the SRA-specific indices
    	nameStr = seqId->GetGeneral().GetTag().GetStr();
    }
    else {
       	nameStr = seqId->GetSeqIdString(true);
    }

    if (nameStr == kEmptyStr) {
        NCBI_THROW(CException, eUnknown,
                   "Empty VDB tag in SeqID");
    }
    readName = nameStr.c_str();

    // Get the VDB Data
    TVDBData* vdbData = (TVDBData*)(_BlastSeqSrcImpl_GetDataStructure(m_seqSrc));
    if (!vdbData) {
        NCBI_THROW(CException, eUnknown, "Invalid VDB BlastSeqSrc");
    }

    // Get the OID
    Int4 oid = 0;
    if (!VDBSRC_GetOIDFromReadName(vdbData, readName, &oid)) {
        NCBI_THROW(CException, eUnknown,
                   "Failed to get the OID for the VDB tag: " + string(readName));
    }
    return Uint4(oid);
}


bool
CVDBBlastUtil::IsSRA(const string & db_name)
{
	if (!db_name.empty())
	{
		size_t last_pos = db_name.find_last_of(CDirEntry::GetPathSeparator());
		string tmp = db_name;
		if(last_pos != string::npos) {
			tmp = db_name.substr(last_pos +1);
		}

		if(tmp.find_first_of("0123456789") == 3) {
            if(tmp.find_first_not_of("0123456789", 4) == std::string::npos)
				return true;
		}
	}
	return false;
}
CRef<CSeq_id>
CVDBBlastUtil::GetVDBSeqIdFromOID(Uint4 oid)
{
    if (!m_seqSrc)
    {
        NCBI_THROW(CException, eUnknown, "SRA BlastSeqSrc is not available");
    }

    CRef<CSeq_id> seqId;
    // Get the SRA Data
    TVDBData* vdbData =
        (TVDBData*)(_BlastSeqSrcImpl_GetDataStructure(m_seqSrc));
    if (!vdbData)
    {
        NCBI_THROW(CException, eUnknown, "Invalid SRA BlastSeqSrc");
    }

    char nameRun[100];
    if(!VDBSRC_GetReadNameForOID(vdbData, oid,nameRun, 100)) {
    	return seqId;
    }

    const string gnl_tag("gnl|SRA|");
    string strId = string(nameRun);
    if(m_isCSRAUtil) {
    	vector<string>  tmp;
    	NStr::Split(strId, "/", tmp, NStr::fSplit_Tokenize);
    	if (NStr::Find(tmp.back(), "|") != NPOS) {
    		 list<CRef<CSeq_id> > ids;
    		 CSeq_id::ParseFastaIds(ids, tmp.back(), true);
    		 CRef<CSeq_id> bs_id = FindBestChoice(ids, CSeq_id::Score);
    		 if(bs_id->IdentifyAccession(CSeq_id::fParse_RawText) == CSeq_id::eAcc_unknown){
    			strId = gnl_tag + strId;
    		 }
    		else {
    			seqId.Reset(bs_id);
    			return seqId;
    		 }
    	}
    	else if(CSeq_id::eAcc_unknown ==  CSeq_id::IdentifyAccession(tmp.back(), CSeq_id::fParse_RawText)) {
    		strId = gnl_tag + strId;
    	}
    	else {
    		strId =tmp.back();
    	}
    }
    else {
    	if (!s_IsWGSId(strId)) {
    		strId = gnl_tag + strId;
    	}
    }
    seqId.Reset(new CSeq_id(strId));

    return seqId;
}

CRef<CBioseq>
CVDBBlastUtil::CreateBioseqFromVDBSeqId(CRef<CSeq_id> seqId)
{
    if (!m_seqSrc)
    {
        NCBI_THROW(CException, eUnknown, "VDB BlastSeqSrc is not available");
    }

    CRef<CBioseq> bioseqResult;

    // Get the VDB Data
    TVDBData* vdbData =
        (TVDBData*)(_BlastSeqSrcImpl_GetDataStructure(m_seqSrc));
    if (!vdbData)
    {
        NCBI_THROW(CException, eUnknown, "Invalid VDB BlastSeqSrc");
    }

    uint64_t oid = (uint64_t) GetOIDFromVDBSeqId(seqId);

    // Read the sequence as string
    char* cstrSeq = 0;
    bool rc = false;
    TVDBErrMsg errMsg;
    VDBSRC_InitEmptyErrorMsg(&errMsg);
   	rc = VDBSRC_Get4naSequenceAsString(vdbData, oid, &cstrSeq, &errMsg);

    if(!rc)
    {
    	char * errString;
    	VDBSRC_FormatErrorMsg(&errString,&errMsg);
    	VDBSRC_ReleaseErrorMsg(&errMsg);
    	//ERR_POST(Error << errString);
        NCBI_THROW(CException, eUnknown,
                   "Failed to read the VDB sequence string for OID=" +
                   NStr::UInt8ToString(oid));
    }
   	VDBSRC_ReleaseErrorMsg(&errMsg);

    if (!cstrSeq || strlen(cstrSeq) == 0)
    {
        NCBI_THROW(CException, eUnknown,
                   "Got an empty VDB sequence string for OID=" +
                   NStr::UInt8ToString(oid));
    }

    // Store the sequence in the Bioseq
    CRef<CSeq_data> seqData(new CSeq_data(cstrSeq, CSeq_data::e_Iupacna));
    CRef<CSeq_inst> seqInst(new CSeq_inst);
    seqInst->SetRepr(CSeq_inst::eRepr_raw);
    seqInst->SetMol(CSeq_inst::eMol_dna);
    seqInst->SetLength(strlen(cstrSeq));
    seqInst->SetSeq_data(*seqData);

    bioseqResult.Reset(new CBioseq);
    bioseqResult->SetInst(*seqInst);

    // Store the Seq ID in the Bioseq
    bioseqResult->SetId().push_back(seqId);

    // Store the spot name as a title in the Bioseq
    CRef<CSeqdesc> descTitle(new CSeqdesc);
    string title = "Length: " + NStr::UIntToString(seqInst->GetLength());
    descTitle->SetTitle(title);
    bioseqResult->SetDescr().Set().push_back(descTitle);

    free(cstrSeq);
    return bioseqResult;
}

CRef<CBioseq>
CVDBBlastUtil::CreateBioseqFromOid(Uint8 oid)
{
    if (!m_seqSrc)
    {
        NCBI_THROW(CException, eUnknown, "VDB BlastSeqSrc is not available");
    }

    CRef<CBioseq> bioseqResult;
    TVDBErrMsg errMsg;
    VDBSRC_InitEmptyErrorMsg(&errMsg);

    // Get the VDB Data
    TVDBData* vdbData =
        (TVDBData*)(_BlastSeqSrcImpl_GetDataStructure(m_seqSrc));
    if (!vdbData)
    {
        NCBI_THROW(CException, eUnknown, "Invalid VDB BlastSeqSrc");
    }

    if(vdbData->reader_2na == NULL)
    {
    	if(errMsg.isError)
    	{
    		char * errString;
    		VDBSRC_FormatErrorMsg(&errString,&errMsg);
    		VDBSRC_ReleaseErrorMsg(&errMsg);
    		ERR_POST(Error << errString);
    		NCBI_THROW(CException, eUnknown,
    		           "2na reader has not been initialized");
    	}
    }

    // Read the sequence as string
    char* cstrSeq = 0;
    bool rc = false;

   	rc = VDBSRC_Get2naSequenceAsString(vdbData, oid, &cstrSeq, &errMsg);

    if((rc == FALSE) && (errMsg.isError))
    {
    	char * errString;
    	VDBSRC_FormatErrorMsg(&errString,&errMsg);
    	VDBSRC_ReleaseErrorMsg(&errMsg);
    	ERR_POST(Error << errString);
        NCBI_THROW(CException, eUnknown,
                   "Failed to read the VDB sequence string for OID=" +
                   NStr::UInt8ToString(oid));
    }
    else if((rc == FALSE) && (!errMsg.isError))
	{
    	ERR_POST(Warning << "All sequences in the set has been read");
    	VDBSRC_ReleaseErrorMsg(&errMsg);
    	return bioseqResult;
	}

   	VDBSRC_ReleaseErrorMsg(&errMsg);
    if (!cstrSeq || strlen(cstrSeq) == 0)
    {
        NCBI_THROW(CException, eUnknown,
                   "Got an empty VDB sequence string for OID=" +
                   NStr::UInt8ToString(oid));
    }

    CRef<CSeq_id> id = GetVDBSeqIdFromOID(oid);
    if (id.Empty())
    {
	        NCBI_THROW(CException, eUnknown,
   	                   "Failed to seq id for OID=" +
   	                   NStr::UInt8ToString(oid));
    }

    // Store the sequence in the Bioseq
    CRef<CSeq_data> seqData(new CSeq_data(cstrSeq, CSeq_data::e_Iupacna));
    CRef<CSeq_inst> seqInst(new CSeq_inst);
    seqInst->SetRepr(CSeq_inst::eRepr_raw);
    seqInst->SetMol(CSeq_inst::eMol_dna);
    seqInst->SetLength(strlen(cstrSeq));
    seqInst->SetSeq_data(*seqData);

    bioseqResult.Reset(new CBioseq);
    bioseqResult->SetInst(*seqInst);

    // Store the Seq ID in the Bioseq
    bioseqResult->SetId().push_back(id);

    // Store the spot name as a title in the Bioseq
    CRef<CSeqdesc> descTitle(new CSeqdesc);
    string title = "Length: " + NStr::UIntToString(seqInst->GetLength());
    descTitle->SetTitle(title);
    bioseqResult->SetDescr().Set().push_back(descTitle);

    return bioseqResult;
}

void CVDBBlastUtil::AddSubjectsToScope(CRef<CScope> scope,
                                       CConstRef<CSeq_align_set> alnSet)
{
    if (alnSet.Empty())
        return;

    CSeq_align_set::Tdata::const_iterator itAln;
    for (itAln = alnSet->Get().begin(); itAln != alnSet->Get().end(); itAln++)
    {
        CRef<CSeq_align> alnCur = *itAln;
        const CSeq_id& subjIdFromAln = alnCur->GetSeq_id(1);
        CRef<CSeq_id> subjId(new CSeq_id);
        subjId->Assign(subjIdFromAln);

        CRef<CBioseq> bioseq = CreateBioseqFromVDBSeqId(subjId);
        scope->AddBioseq(*bioseq, CScope::kPriority_Default,
                                  CScope::eExist_Get);
    }
}

void
CVDBBlastUtil::FillVDBInfo(vector< CBlastFormatUtil::SDbInfo >& vecDbInfo)
{
    if (!m_seqSrc)
    {
        NCBI_THROW(CException, eUnknown, "VDB BlastSeqSrc is not available");
    }

    // Create a DB info structure describing the list of open SRA runs
    CBlastFormatUtil::SDbInfo dbInfo;
    dbInfo.is_protein = false;
    dbInfo.name = NStr::Replace(BlastSeqSrcGetName(m_seqSrc), "|", " ") ;
    dbInfo.definition = dbInfo.name;
    dbInfo.total_length = BlastSeqSrcGetTotLen(m_seqSrc);
    dbInfo.number_seqs = BlastSeqSrcGetNumSeqs(m_seqSrc);
    vecDbInfo.push_back(dbInfo);
}

CVDBBlastUtil::CVDBBlastUtil(bool bCSRA, const string& strAllRuns):
        m_bOwnSeqSrc(true), m_strAllRuns(strAllRuns), m_isCSRAUtil(bCSRA)
{
    m_seqSrc = x_MakeVDBSeqSrc();
}

void CVDBBlastUtil::GetVDBStats(const string & strAllRuns, Uint8 & num_seqs, Uint8 & length, bool getRefStats)
{
	CVDBBlastUtil util(getRefStats, strAllRuns);
	BlastSeqSrc* seq_src = util.GetSRASeqSrc();
    TVDBData* vdbData =
        (TVDBData*)(_BlastSeqSrcImpl_GetDataStructure(seq_src));
    if (!vdbData)
    {
        NCBI_THROW(CException, eUnknown, "Invalid SRA BlastSeqSrc");
    }

    num_seqs = vdbData->numSeqs;
    length = VDBSRC_GetTotSeqLen(vdbData);
}

void CVDBBlastUtil::GetVDBStats(const string & strAllRuns, Uint8 & num_seqs, Uint8 & length,
			     	 	        Uint8 & max_seq_length, Uint8 & av_seq_length, bool getRefStats)
{
	CVDBBlastUtil util(getRefStats, strAllRuns);
	BlastSeqSrc* seq_src = util.GetSRASeqSrc();
    TVDBData* vdbData =
        (TVDBData*)(_BlastSeqSrcImpl_GetDataStructure(seq_src));
    if (!vdbData)
    {
        NCBI_THROW(CException, eUnknown, "Invalid SRA BlastSeqSrc");
    }

    num_seqs = vdbData->numSeqs;
    length = VDBSRC_GetTotSeqLen(vdbData);
    max_seq_length = VDBSRC_GetMaxSeqLen(vdbData);
    av_seq_length = VDBSRC_GetAvgSeqLen(vdbData);
}

void CVDBBlastUtil::CheckVDBs(const vector<string> & vdbs)
{
	unsigned int numRuns = vdbs.size();

	char** vdbRunAccessions = new char*[numRuns];
	for (Uint4 iRun = 0; iRun < numRuns; iRun++) {
		if (!vdbs[iRun].empty()) {
			vdbRunAccessions[iRun] = strdup(vdbs[iRun].c_str());
	    }
	}

    AutoPtr<Boolean, ArrayDeleter<Boolean> > isRunExcluded(new Boolean[numRuns]);
	Uint4 rc;
	// Construct the BlastSeqSrc object
    SRABlastSeqSrcInit((const char**)vdbRunAccessions, numRuns, false, isRunExcluded.get(), &rc, false);

	// Clean up
	string cannot_open= kEmptyStr;
	for (Uint4 iRun = 0; iRun < numRuns; iRun++)
	{
		if(isRunExcluded.get()[iRun]) {
			cannot_open += vdbs[iRun]+ " ";
	    }
	    free(vdbRunAccessions[iRun]);
	}
    delete [] vdbRunAccessions;

    if(cannot_open != kEmptyStr)
    {
        NCBI_THROW(CException, eInvalid, "Invalid vdbs: " + cannot_open);
    }
}

Uint4 CVDBBlastUtil::SetupVDBManager()
{
	Uint4 status = 0;
	VdbBlastMgr*  mgr = VDBSRC_GetVDBManager(&status);
	if(status != 0 || mgr == NULL) {
	        NCBI_THROW(CException, eInvalid, "Fail to setup VDB manager");
	}
	status = VdbBlastMgrKLogLevelSetWarn ( mgr);

	return status;
}

void CVDBBlastUtil::ReleaseVDBManager()
{
	VDBSRC_ReleaseVDBManager();
}

bool CVDBBlastUtil::IsCSRA(const string & db_name)
{
	if(CVDBBlastUtil::IsSRA(db_name)) {
		int rv = VDBSRC_IsCSRA(db_name.c_str());
		if(rv == 1)
			return true;

		if(rv == -1)
	        NCBI_THROW(CException, eInvalid, "Check for csra retruns error");
	}

	return false;
}

void CVDBBlastUtil::GetAllStats(const string & strAllRuns, Uint8 & num_seqs, Uint8 & length,
					        	Uint8 & ref_num_seqs, Uint8 & ref_length)
{
	CVDBBlastUtil util(false, strAllRuns);
	BlastSeqSrc* seq_src = util.GetSRASeqSrc();
	TVDBData* vdbData = (TVDBData*)(_BlastSeqSrcImpl_GetDataStructure(seq_src));
	if (!vdbData) {
	        NCBI_THROW(CException, eUnknown, "Invalid SRA BlastSeqSrc");
    }

    num_seqs = vdbData->numSeqs;
    length = VDBSRC_GetTotSeqLen(vdbData);

    TVDBErrMsg vdbErrMsg;
    VDBSRC_InitEmptyErrorMsg(&vdbErrMsg);
    VDBSRC_MakeCSRASeqSrcFromSRASeqSrc(vdbData, &vdbErrMsg, true);
    if(vdbErrMsg.isError) {
    	ref_num_seqs = 0;
    	ref_length = 0;
    }
    else {
    	ref_num_seqs = vdbData->numSeqs;
    	ref_length = VDBSRC_GetTotSeqLen(vdbData);
    }
}

CVDBBlastUtil::IDType CVDBBlastUtil::VDBIdType(const CSeq_id & seq_id)
{
	if ( seq_id.Which() == CSeq_id::e_General ) {
		const CDbtag& dbtag = seq_id.GetGeneral();
		if ( NStr::EqualNocase(dbtag.GetDb(), "SRA")) {
			if ( dbtag.GetTag().IsStr() ) {
				const string& str = dbtag.GetTag().GetStr();
				SIZE_TYPE srr_len = str.find('/');
			    if ( srr_len != NPOS ) {
			    	return eCSRALocalRefId;
			    }
			    else {
			    	return eSRAId;
			    }
			}
		}
	}
	if (s_IsWGSId(seq_id.GetSeqIdString())) {
		return eWGSId;
	}

	if (CSeq_id::eAcc_unknown !=  seq_id.IdentifyAccession(CSeq_id::fParse_AnyRaw)) {
		return eCSRARefId;
	}
	return eUnknownId;
}

Uint4 CVDBBlastUtil::GetMaxNumCSRAThread(void)
{
	Uint8 mem_size = CSystemInfo::GetTotalPhysicalMemorySize();
	if(mem_size == 0)
		return 0;
	Uint4 num_thread =  (mem_size * 0.5/VDB_2NA_CHUNK_BUF_SIZE);

	return (num_thread == 0? 1: num_thread);
}

// ==========================================================================//

END_NCBI_SCOPE 
