#ifndef OBJTOOLS_DATA_LOADERS_BLASTDB___LOCAL_BLASTDB_ADAPTER__HPP
#define OBJTOOLS_DATA_LOADERS_BLASTDB___LOCAL_BLASTDB_ADAPTER__HPP

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
 *  ===========================================================================
 *
 *  Author: Christiam Camacho
 *
 * ===========================================================================
 */

/** @file local_blastdb_adapter.hpp
  * Declaration of the CLocalBlastDbAdapter class.
  */

#include <objtools/data_loaders/blastdb/blastdb_adapter.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/** This class allows retrieval of sequence data from locally installed BLAST
 * databases via CSeqDB
 */
class NCBI_XLOADER_BLASTDB_EXPORT CLocalBlastDbAdapter : public IBlastDbAdapter
{
public:
    /// Constructor with a CSeqDB instance
    /// @param seqdb CSeqDB object to initialize this object with [in]
    CLocalBlastDbAdapter(CRef<CSeqDB> seqdb) : m_SeqDB(seqdb) {}

    /// Constructor with a CSeqDB instance
    /// @param db_name database name [in]
    /// @param db_type database molecule type [in]
    CLocalBlastDbAdapter(const string& db_name, CSeqDB::ESeqType db_type)
        : m_SeqDB(new CSeqDB(db_name, db_type)) {}

	/** @inheritDoc */
    virtual CSeqDB::ESeqType GetSequenceType();
	/** @inheritDoc */
    virtual int GetSeqLength(int oid);
	/** @inheritDoc */
    virtual TSeqIdList GetSeqIDs(int oid);
	/** @inheritDoc */
    virtual CRef<CBioseq> GetBioseqNoData(int oid, TGi target_gi = ZERO_GI, const CSeq_id * target_id = NULL);
	/** @inheritDoc */
    virtual CRef<CSeq_data> GetSequence(int oid, int begin = 0, int end = 0);
	/** @inheritDoc */
    virtual bool SeqidToOid(const CSeq_id & id, int & oid);
	/** @inheritDoc */
    virtual int GetTaxId(const CSeq_id_Handle& id);
    
private:
    /// The BLAST database handle
    CRef<CSeqDB> m_SeqDB;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif /* OBJTOOLS_DATA_LOADERS_BLASTDB___LOCAL_BLASTDB_ADAPTER__HPP */
