#ifndef OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__SI2CSI_TASK__DELETE_HPP_
#define OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__SI2CSI_TASK__DELETE_HPP_

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
 * Authors: Dmitrii Saprykin
 *
 * File Description:
 *
 * Cassandra blob storage task to delete Si2Csi record
 *
 */

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/si2csi/record.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CCassSI2CSITaskDelete
    : public CCassBlobWaiter
{
    enum EBlobInserterState {
        eInit = 0,
        eWaitingPropsDeleted,
        eDone = CCassBlobWaiter::eDone,
        eError = CCassBlobWaiter::eError
    };

public:
    CCassSI2CSITaskDelete(
        unsigned int op_timeout_ms,
        shared_ptr<CCassConnection> conn,
        const string & keyspace,
        CSI2CSIRecord::TSecSeqId sec_seq_id,
        CSI2CSIRecord::TSecSeqIdType sec_seq_id_type,
        unsigned int max_retries,
        TDataErrorCallback data_error_cb
    );

protected:
    virtual void Wait1(void) override;

private:
    CSI2CSIRecord::TSecSeqId m_SecSeqId;
    CSI2CSIRecord::TSecSeqIdType m_SecSeqIdType;
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__SI2CSI_TASK__UPDATE_HPP_
