#ifndef PSG_CACHE_SI2CSI__HPP
#define PSG_CACHE_SI2CSI__HPP

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
 * Authors: Dmitri Dmitrienko
 *
 * File Description: si2csi table cache
 *
 */

#include "psg_cache_base.hpp"
 
BEGIN_NCBI_SCOPE

class CPubseqGatewayCacheSi2Csi : public CPubseqGatewayCacheBase
{
public:
	CPubseqGatewayCacheSi2Csi(const string& file_name);
    virtual ~CPubseqGatewayCacheSi2Csi() override;
    void Open();
    bool LookupBySeqId(const string& sec_seqid, int& sec_seq_id_type, string& data);
    bool LookupBySeqIdIdType(const string& sec_seqid, int sec_seq_id_type, string& data);

    static string PackKey(const string& sec_seqid, int sec_seq_id_type);
    static bool UnpackKey(const char* key, size_t key_sz, int& sec_seq_id_type);
private:
    unique_ptr<lmdb::dbi> m_Dbi;
};

END_NCBI_SCOPE


#endif
