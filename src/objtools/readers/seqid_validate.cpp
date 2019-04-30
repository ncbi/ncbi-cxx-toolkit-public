/*
 * $Id$
 *
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
 * Authors:  Frank Ludwig
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <objtools/readers/reader_error_codes.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/alnread.hpp>
#include <objtools/readers/reader_error_codes.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include "seqid_validate.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

bool CSeqIdValidate::operator()(const CSeq_id& seqId, string& description, EDiagSev& severity) 
{
    description.clear();

    if (seqId.IsLocal() &&
        seqId.GetLocal().IsStr()) {
        const auto idString = seqId.GetLocal().GetStr();

        if (idString.empty()) {
            description = "Empty local ID.";
            severity = eDiag_Error;
            return false;
        }

        if (idString.size() > 50) {
            description = "Local ID \"" + 
                          idString +
                          " \" exceeds 50 character limit.";
            severity = eDiag_Error;
            return false;
        }

        if (CSeq_id::CheckLocalID(idString) & CSeq_id::fInvalidChar) {
            description = "Local ID \"" + 
                          idString +
                          "\" contains invalid characters.";
            severity = eDiag_Error;
            return false;
        }
    }
    // default implementation only checks local IDs
    return true;
}

END_SCOPE(objects)
END_NCBI_SCOPE
