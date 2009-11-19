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
* Author:  Christiam Camacho, Kevin Bealer
*
* ===========================================================================
*/

/// @file blast_services.cpp
/// Implementation of CBlastServices class

#include <ncbi_pch.hpp>
#include <corelib/ncbi_system.hpp>
#include <serial/iterator.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objtools/blast/services/blast_services.hpp>
#include <objects/blast/blastclient.hpp>
#include <util/util_exception.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

bool
CBlastServices::IsValidBlastDb(const string& dbname, bool is_protein)
{
    CRef<CBlast4_database> blastdb(new CBlast4_database);
    blastdb->SetName(dbname);
    blastdb->SetType(is_protein 
                     ? eBlast4_residue_type_protein 
                     : eBlast4_residue_type_nucleotide);
    CRef<CBlast4_database_info> result = GetDatabaseInfo(blastdb);
    return result.NotEmpty();
}

CRef<objects::CBlast4_database_info>
CBlastServices::x_FindDbInfoFromAvailableDatabases
    (CRef<objects::CBlast4_database> blastdb)
{
    _ASSERT(blastdb.NotEmpty());

    CRef<CBlast4_database_info> retval;

    ITERATE(CBlast4_get_databases_reply::Tdata, dbinfo, m_AvailableDatabases) {
        if ((*dbinfo)->GetDatabase() == *blastdb) {
            retval = *dbinfo;
            break;
        }
    }

    return retval;
}

vector< CRef<objects::CBlast4_database_info> >
CBlastServices::GetOrganismSpecificRepeatsDatabases()
{
    if (m_AvailableDatabases.empty()) {
        x_GetAvailableDatabases();
    }
    vector< CRef<objects::CBlast4_database_info> > retval;

    ITERATE(CBlast4_get_databases_reply::Tdata, dbinfo, m_AvailableDatabases) {
        if ((*dbinfo)->GetDatabase().GetName().find("repeat_") != NPOS) {
            retval.push_back(*dbinfo);
        }
    }

    return retval;
}

void
CBlastServices::x_GetAvailableDatabases()
{
    CBlast4Client client;
    CRef<CBlast4_get_databases_reply> databases;
    try { 
        databases = client.AskGet_databases(); 
        m_AvailableDatabases = databases->Set();
    }
    catch (const CEofException &) {
        NCBI_THROW(CBlastServicesException, eRequestErr,
                   "No response from server, cannot complete request.");
    }
}


CRef<objects::CBlast4_database_info>
CBlastServices::GetDatabaseInfo(CRef<objects::CBlast4_database> blastdb)
{
    if (blastdb.Empty()) {
        NCBI_THROW(CBlastServicesException, eArgErr,
                   "NULL argument specified: blast database description");
    }

    if (m_AvailableDatabases.empty()) {
        x_GetAvailableDatabases();
    }

    return x_FindDbInfoFromAvailableDatabases(blastdb);
}

void
CBlastServices::GetSequencesInfo(TSeqIdVector & seqids,   // in
                               const string & database, // in
                               char           seqtype,  // 'p' or 'n'
                               TBioseqVector& bioseqs,  // out
                               string       & errors,   // out
                               string       & warnings, // out
                               bool           verbose)  // in
{
    x_GetSequences(seqids, database, seqtype, true, bioseqs, errors, warnings,
                   verbose);
}

void                      
CBlastServices::GetSequences(TSeqIdVector & seqids,   // in
                           const string & database, // in
                           char           seqtype,  // 'p' or 'n'
                           TBioseqVector& bioseqs,  // out
                           string       & errors,   // out
                           string       & warnings, // out
                           bool           verbose)  // in
{
    x_GetSequences(seqids, database, seqtype, false, bioseqs, errors, warnings,
                   verbose);
}   

void CBlastServices::
GetSequenceParts(const TSeqIntervalVector  & seqids,    // in
                 const string              & database,  // in
                 char                        seqtype,   // 'p' or 'n'
                 TSeqIdVector              & ids,       // out
                 TSeqDataVector            & seq_data,  // out
                 string                    & errors,    // out
                 string                    & warnings,  // out
                 bool                        verbose)   // in
{
    // Build the request

    CRef<CBlast4_request> request =
            x_BuildGetSeqPartsRequest(seqids, database, seqtype, errors);

    if (request.Empty()) {
        return;
    }
    if (verbose) {
        NcbiCout << MSerial_AsnText << *request << endl;
    }

    CRef<CBlast4_reply> reply(new CBlast4_reply);

    try {
        // Send request.
        CBlast4Client().Ask(*request, *reply);
    }
    catch(const CEofException &) {
        NCBI_THROW(CBlastServicesException, eRequestErr,
                   "No response from server, cannot complete request.");
    }

    if (verbose) {
        NcbiCout << MSerial_AsnText << *reply << endl;
    }
    x_GetPartsFromReply(reply, ids, seq_data, errors, warnings);
}

/// Process error messages from a reply object.
///
/// Every reply object from blast4 has a space for error and warning
/// messages.  This function extracts such messages and returns them
/// to the user in two strings.  All warnings are returned in one
/// string, concatenated together with a newline as the delimiter, and
/// all error messages are concatenated together in another string in
/// the same way.  If there are no warnings or errors, the resulting
/// strings will be empty.
///
/// @param reply The reply object from blast4.
/// @param errors Concatenated error messages (if any).
/// @param warnings Concatenated warning messages (if any).
static void
s_ProcessErrorsFromReply(CRef<objects::CBlast4_reply> reply,
                         string& errors,
                         string& warnings)
{
    static const string no_msg("<no message>");

    if (reply->CanGetErrors() && (! reply->GetErrors().empty())) {
        ITERATE(list< CRef< CBlast4_error > >, iter, reply->GetErrors()) {

            // Determine the message source and destination.

            const string & message((*iter)->CanGetMessage()
                                   ? (*iter)->GetMessage()
                                   : no_msg);

            string & dest
                (((*iter)->GetCode() & eBlast4_error_flags_warning)
                 ? warnings
                 : errors);

            // Attach the message (and possibly delimiter) to dest.

            if (! dest.empty()) {
                dest += "\n";
            }

            dest += message;
        }
    }
}

void
CBlastServices::
x_GetPartsFromReply(CRef<objects::CBlast4_reply>   reply,    // in
                    TSeqIdVector                 & ids,      // out
                    TSeqDataVector               & seq_data, // out
                    string                       & errors,   // out
                    string                       & warnings) // out
{
    seq_data.clear();
    ids.clear();

    s_ProcessErrorsFromReply(reply, errors, warnings);

    if (reply->CanGetBody() && reply->GetBody().IsGet_sequence_parts()) {
        CBlast4_get_seq_parts_reply::Tdata& parts_rep =
            reply->SetBody().SetGet_sequence_parts().Set();
        ids.reserve(parts_rep.size());
        seq_data.reserve(parts_rep.size());

        NON_CONST_ITERATE(CBlast4_get_seq_parts_reply::Tdata, itr, parts_rep) {
            ids.push_back(CRef<CSeq_id>(&(*itr)->SetId()));
            seq_data.push_back(CRef<CSeq_data>(&(*itr)->SetData()));
        }
    }
}

void
CBlastServices::x_GetSeqsFromReply(CRef<objects::CBlast4_reply> reply,
                                 TBioseqVector                & bioseqs,  // out
                                 string                       & errors,   // out
                                 string                       & warnings) // out
{
    // Read the data from the reply into the output arguments.

    bioseqs.clear();

    s_ProcessErrorsFromReply(reply, errors, warnings);

    if (reply->CanGetBody() && reply->GetBody().IsGet_sequences()) {
        list< CRef<CBioseq> > & bslist =
            reply->SetBody().SetGet_sequences().Set();

        bioseqs.reserve(bslist.size());

        ITERATE(list< CRef<CBioseq> >, iter, bslist) {
            bioseqs.push_back(*iter);
        }
    }
}

static EBlast4_residue_type
s_SeqTypeToResidue(char p, string & errors)
{
    EBlast4_residue_type retval = eBlast4_residue_type_unknown;

    switch(p) {
    case 'p':
        retval = eBlast4_residue_type_protein;
        break;

    case 'n':
        retval = eBlast4_residue_type_nucleotide;
        break;

    default:
        errors = "Error: invalid residue type specified.";
    }

    return retval;
}

CRef<objects::CBlast4_request> CBlastServices::
x_BuildGetSeqRequest(TSeqIdVector& seqids,   // in
                     const string& database, // in
                     char          seqtype,  // 'p' or 'n'
                     bool    skip_seq_data,  // in
                     string&       errors)  // out
{
    // This will be returned in an Empty() state if an error occurs.
    CRef<CBlast4_request> request;

    EBlast4_residue_type rtype = s_SeqTypeToResidue(seqtype, errors);

    if (database.empty()) {
        errors = "Error: database name may not be blank.";
        return request;
    }

    if (seqids.empty()) {
        errors = "Error: no sequences requested.";
        return request;
    }

    // Build ASN.1 request objects and link them together.

    request.Reset(new CBlast4_request);

    CRef<CBlast4_request_body> body(new CBlast4_request_body);
    CRef<CBlast4_database>     db  (new CBlast4_database);

    request->SetBody(*body);
    body->SetGet_sequences().SetDatabase(*db);
    body->SetGet_sequences().SetSkip_seq_data(skip_seq_data);

    // Fill in db values

    db->SetName(database);
    db->SetType(rtype);

    // Link in the list of requests.

    list< CRef< CSeq_id > > & seqid_list =
        body->SetGet_sequences().SetSeq_ids();

    ITERATE(TSeqIdVector, iter, seqids) {
        seqid_list.push_back(*iter);
    }

    return request;
}

void
CBlastServices::x_GetSequences(TSeqIdVector & seqids,
                             const string & database,
                             char           seqtype,
                             bool           skip_seq_data,
                             TBioseqVector& bioseqs,
                             string       & errors,
                             string       & warnings,
                             bool           verbose)
{
    // Build the request

    CRef<CBlast4_request> request =
        x_BuildGetSeqRequest(seqids, database, seqtype, skip_seq_data, errors);

    if (request.Empty()) {
        return;
    }
    if (verbose) {
        NcbiCout << MSerial_AsnText << *request << endl;
    }

    CRef<CBlast4_reply> reply(new CBlast4_reply);

    try {
        // Send request
        CBlast4Client().Ask(*request, *reply);
    }
    catch(const CEofException &) {
        NCBI_THROW(CBlastServicesException, eRequestErr,
                   "No response from server, cannot complete request.");
    }

    if (verbose) {
        NcbiCout << MSerial_AsnText << *reply << endl;
    }
    x_GetSeqsFromReply(reply, bioseqs, errors, warnings);
}

CRef<objects::CBlast4_request> CBlastServices::
x_BuildGetSeqPartsRequest(const TSeqIntervalVector & seqids,    // in
                          const string             & database,  // in
                          char                       seqtype,   // 'p' or 'n'
                          string                   & errors)    // out
{
    errors.erase();

    // This will be returned in an Empty() state if an error occurs.
    CRef<CBlast4_request> request;

    EBlast4_residue_type rtype = s_SeqTypeToResidue(seqtype, errors);

    if (errors.size()) {
        return request;
    }

    if (database.empty()) {
        errors = "Error: database name may not be blank.";
        return request;
    }
    if (seqids.empty()) {
        errors = "Error: no sequences requested.";
        return request;
    }

    // Build ASN.1 request objects and link them together.

    request.Reset(new CBlast4_request);

    CRef<CBlast4_request_body> body(new CBlast4_request_body);
    CRef<CBlast4_database>     db  (new CBlast4_database);

    request->SetBody(*body);

    CBlast4_get_seq_parts_request & req =
        body->SetGet_sequence_parts();
    copy(seqids.begin(), seqids.end(), back_inserter(req.SetSeq_locations()));

    req.SetDatabase(*db);

    // Fill in db values
    db->SetName(database);
    db->SetType(rtype);
    return request;
}


END_NCBI_SCOPE

/* @} */

