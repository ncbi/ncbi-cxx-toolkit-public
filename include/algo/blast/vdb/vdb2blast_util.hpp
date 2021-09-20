#ifndef ALGO_BLAST_VDB___VDB2BLAST_UTIL__HPP
#define ALGO_BLAST_VDB___VDB2BLAST_UTIL__HPP

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

/// @file blastutil_sra.hpp
/// Class for better integration of SRA BlastSeqSrc with the C++ Blast API.
///
/// The SRA BlastSeqSrc alone is not enough to run Blast through C++ Blast API
/// classes and produce formatted Blast results. One needs some additional
/// high-level functions for converting the SRA sequence data to the objects
/// defined in the C++ toolkit and the Blast API. This file provides a
/// class called CVDBBlastUtil that generates this additional data.

#include "seqsrc_vdb.h"

#include <corelib/ncbiobj.hpp>
#include <common/ncbi_export.h>

#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/blast_seqinfosrc.hpp>

#include <algo/blast/format/blastfmtutil.hpp>

#include <objects/seq/seq__.hpp>

BEGIN_NCBI_SCOPE

// ==========================================================================//
/// CVDBBlastUtil
///
/// High-level functions used for setting up and running Blast with SRAs.
///
/// This class provides several high-level functions for creating and
/// managing the SRA BlastSeqSrc and SRA BlastSeqInfoSrc objects. It also
/// provides conversions between OIDs and high-level sequence objects
/// containing SRA-specific information (SeqIDs, Bioseqs).

class NCBI_VDB2BLAST_EXPORT CVDBBlastUtil : public CObject
{

public:
    /// Constructor that creates and stores the SRA BlastSeqSrc object.
    ///
    /// @param strAllRuns Space-delimited SRA run accessions to open. [in]
    /// @param bOwnSeqSrc Release the BlastSeqSrc object in destructor. [in]
    /// @throws CException
    CVDBBlastUtil(const string& strAllRuns,
                  bool bOwnSeqSrc = false,
                  bool bCSRA = false);

    /// Destructor
    virtual ~CVDBBlastUtil();

    /// *Note* Call this in main thread first, if you are going to instantiate
    /// this object or use any of the static menthod in a thread
    static Uint4 SetupVDBManager();

    /// Call this release vdb manager if SetupManger has been explicitly
    /// called in the main thread
    static void ReleaseVDBManager();

    /// Return the stored SRA BlastSeqSrc object.
    /// @return Pointer to the SRA BlastSeqSrc object.
    /// @throws CException
    BlastSeqSrc* GetSRASeqSrc();

    /// Return the SRA BlastSeqInfoSrc object (create if none exists).
    /// @return CRef to the SRA BlastSeqInfoSrc object.
    /// @throws CException
    CRef<blast::IBlastSeqInfoSrc> GetSRASeqInfoSrc();

    /// Get the ordinal number (OID) for the given SRA sequence.
    ///
    /// This function takes a SeqID identifying an SRA sequence
    /// (a single read, e.g. gnl|SRA|SRR000002.123691.1) and converts
    /// it to the internal ordinal number used by the SRA BlastSeqSrc.
    ///
    /// @param seqId SeqID of the SRA sequence [in]
    /// @return OID corresponding to the given SRA sequence.
    /// @throws CException
    Uint4 GetOIDFromVDBSeqId(CRef<objects::CSeq_id> seqId);

    /// Get the SRA sequence SeqID given its ordinal number (OID).
    ///
    /// This function returns the SeqID identifying an SRA sequence
    /// (a single read, e.g. gnl|SRA|SRR000002.123691.1) given its
    /// internal ordinal number (OID) used by the SRA BlastSeqSrc.
    /// It is guaranteed that one will get the same results by calling
    /// the GetId method of the BlastSeqInfoSrc object and taking the
    /// first General SeqId in the returned list.
    ///
    /// @param oid OID of the SRA sequence [in]
    /// @return corresponding SeqID of the SRA sequence.
    /// @throws CException
    CRef<objects::CSeq_id> GetVDBSeqIdFromOID(Uint4 oid);

    /// Construct a Bioseq object for the given SRA sequence.
    ///
    /// This function takes a SeqID indentifying an SRA sequence
    /// and constructs a Bioseq object with the same SeqID and
    /// the actual sequence data stored in IUPACNA format.
    ///
    /// @param seqId SeqID of the SRA sequence [in]
    /// @return Newly constructed Bioseq for the given SRA sequence.
    /// @throws CException
    CRef<objects::CBioseq>
        CreateBioseqFromVDBSeqId(CRef<objects::CSeq_id> seqId);

    CRef<objects::CBioseq>
    	CreateBioseqFromOid(Uint8 oid);

    /// Populate the CScope object with subject sequence Bioseqs.
    ///
    /// This function must be called to populate the CScope object
    /// with the Bioseqs of every SRA sequence present in the Blast results.
    /// Without these Bioseqs, the Blast formatter would have no way
    /// to access and display the subject sequence data for the alignments.
    ///
    /// @param scope CScope object to populate [in]
    /// @param alnSet Set of alignments representing the Blast results [in]
    /// @throws CException
    void AddSubjectsToScope(CRef<CScope> scope,
                            CConstRef<CSeq_align_set> alnSet);

    /// Populate the DB info list with information on open SRA runs.
    /// @param vecDbInfo DB info list to populate [out]
    /// @throws CException
    void FillVDBInfo(vector< CBlastFormatUtil::SDbInfo >& vecDbInfo);

    // Check if vdb is SRA, can be CSRA
    static bool IsSRA(const string & db_name);
    // Check if vdb is CSRA
    static bool IsCSRA(const string & db_name);

    // Return supported VDB ID Type
    enum IDType {
    	eSRAId = 0,
    	eWGSId,
    	eCSRALocalRefId,
    	eCSRARefId,
    	eUnknownId
    };
    static IDType VDBIdType(const CSeq_id & id);

	/// Fucntion to get around the OID (blastseqsrc) limit
	/// So num of seqs > int4 can be returned
	static void GetVDBStats(const string & strAllRuns, Uint8 & num_seqs, Uint8 & length, bool getRefStats = false);
	static void GetVDBStats(const string & strAllRuns, Uint8 & num_seqs, Uint8 & length,
			                Uint8 & max_seq_length, Uint8 & av_seq_length, bool getRefStats = false);

	// Return reference seqs stats and vdb stats for csra runs .
	static void GetAllStats(const string & strAllRuns, Uint8 & num_seqs, Uint8 & length,
					        Uint8 & ref_num_seqs, Uint8 & ref_length);

	/// Function to check a list of dbs if they can be opened
	/// Throw an exception if any of the db cannot be opened
	static void CheckVDBs(const vector<string> & vdbs);

	bool IsCSRAUtil() { return m_isCSRAUtil; }

	static const Uint8 REF_SEQ_ID_MASK = (0x8000000000000000);

	static Uint4 GetMaxNumCSRAThread(void);

	// This function expect ids in sorted accending order
	void GetOidsFromSeqIds_WGS(const vector<string> & ids , vector<int> & oids);

	bool IsWGS();
private:
	/// Temporary hack to get around oid limit (used by GetVDBStats)
	/// Shoudl be remove when
    CVDBBlastUtil(bool bCSRA, const string& strAllRuns);

    /// Tokenize the stored whitespace-delimited string of SRA runs.
    /// @param vecSRARunAccessions list of individual SRA run accessions [out]
    /// @throws CException
    void x_GetSRARunAccessions(vector<string>& vecSRARunAccessions);


    /// Construct an SRA BlastSeqSrc object from the given strings.
    /// @return Pointer to the constructed SRA BlastSeqSrc object.
    /// @throws CException
    BlastSeqSrc* x_MakeVDBSeqSrc();

    /// Release the BlastSeqSrc object in destructor.
    bool m_bOwnSeqSrc;
    /// Space-delimited list of opened SRA run accessions.
    string m_strAllRuns;
    /// Pointer to a properly initialized SRA BlastSeqSrc.
    BlastSeqSrc* m_seqSrc;
    bool m_isCSRAUtil;

};

// ==========================================================================//

END_NCBI_SCOPE

#endif /* ALGO_BLAST_VDB___VDB2BLAST_UTIL__HPP */
