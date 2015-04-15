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

/// Get bioseqs from a sequence fetching reply.
///
/// This method reads the reply from a sequence fetching request
/// and extracts the bioseqs, errors and warnings from it.
///
/// @param reply
///     The reply from a sequence fetching request.
/// @param bioseqs
///     The returned list of bioseqs from the request.
/// @param errors
///     Returned string containing any errors encountered.
/// @param warnings
///     Returned string containing any warnigns encountered.
static void
s_GetSeqsFromReply(CRef<objects::CBlast4_reply> reply,
                                 CBlastServices::TBioseqVector                & bioseqs,  // out
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

/// Build Sequence Fetching Request
///
/// This method builds a blast4 request designed to fetch a list
/// of bioseqs from the blast4 server.
///
/// @param seqids
///     The seqids of the sequences to fetch.
/// @param database
///     The database or databases containing the desired sequences.
/// @param seqtype
///     Either 'p' or 'n' for protein or nucleotide.
/// @param errors
///     Returned string containing any errors encountered.
/// @param skip_seq_data
///     If true, the sequence data will NOT be fetched
/// @param target_only
///     If true, only requested seq_id will be returned
/// @return
///     The blast4 sequence fetching request object.
static CRef<objects::CBlast4_request>
s_BuildGetSeqRequest(CBlastServices::TSeqIdVector& seqids,   // in
                     const string& database, // in
                     char          seqtype,  // 'p' or 'n'
                     bool    skip_seq_data,  // in
                     bool    target_only,    // in
                     string&       errors)   // out
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
    body->SetGet_sequences().SetTarget_only(target_only);

    // Fill in db values

    db->SetName(database);
    db->SetType(rtype);

    // Link in the list of requests.

    list< CRef< CSeq_id > > & seqid_list =
        body->SetGet_sequences().SetSeq_ids();

    ITERATE(CBlastServices::TSeqIdVector, iter, seqids) {
        seqid_list.push_back(*iter);
    }

    return request;
}

/// Main function to issue a Blast4-get-sequences-request and collect its
/// results from the remote BLAST server.
///
/// @param seqids
///     The seqids of the sequences to fetch. [in]
/// @param database
///     The database or databases containing the desired sequences. [in]
/// @param seqtype
///     Either 'p' or 'n' for protein or nucleotide. [in]
/// @param bioseqs
///     The vector used to return the requested Bioseqs. [out]
/// @param errors
///     Returned string containing any errors encountered. [out]
/// @param warnings
///     A null-separated list of warning. [out]
/// @param skip_seq_data
///     If true, the sequence data will NOT be fetched [in]
/// @param verbose  Produce verbose output. [in]
static void
s_GetSequences(CBlastServices::TSeqIdVector & seqids,
                             const string & database,
                             char           seqtype,
                             bool           skip_seq_data,
                             bool           target_only,
                             CBlastServices::TBioseqVector& bioseqs,
                             string       & errors,
                             string       & warnings,
                             bool           verbose)
{
    // Build the request

    CRef<CBlast4_request> request =
        s_BuildGetSeqRequest(seqids, database, seqtype, skip_seq_data, target_only, errors);

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
    s_GetSeqsFromReply(reply, bioseqs, errors, warnings);
}

/// Build Sequence Parts Fetching Request
///
/// This method builds a blast4 request designed to fetch sequence
/// data
///
/// @param seqids
///     The seqids and ranges of the sequences to fetch.
/// @param database
///     The database or databases containing the desired sequences.
/// @param seqtype
///     Either 'p' or 'n' for protein or nucleotide.
/// @param errors
///     Returned string containing any errors encountered.
/// @return
///     The blast4 sequence fetching request object.
static CRef<objects::CBlast4_request> 
s_BuildGetSeqPartsRequest(const CBlastServices::TSeqIntervalVector & seqids,    // in
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


bool
CBlastServices::IsValidBlastDb(const string& dbname, bool is_protein)
{
    if (dbname.empty())
       return false;

    bool found_all = false;
    vector< CRef<objects::CBlast4_database_info> > result =
          GetDatabaseInfo(dbname, is_protein, &found_all);

    if (found_all && !result.empty())
       return true;
    else
       return false;
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


vector< CRef<objects::CBlast4_database_info> >
CBlastServices::GetDatabaseInfoLegacy(const string& dbname, bool is_protein,
	bool *found_all,
	vector<string> *missing_names)
{
    vector<CRef<objects::CBlast4_database_info> > retval;
    vector<string> dbs;
    NStr::Tokenize(dbname, " \n\t", dbs);

    if (dbs.empty())
      *found_all = false; // Loop did not run.
    else 
      *found_all = true; // Set to false if one missing

    ITERATE(vector<string>, i, dbs) {
       const string kDbName = NStr::TruncateSpaces(*i);
       if (kDbName.empty())
             continue;

       CRef<CBlast4_database> blastdb(new CBlast4_database);
       blastdb->SetName(kDbName);
       blastdb->SetType(is_protein 
                     ? eBlast4_residue_type_protein 
                     : eBlast4_residue_type_nucleotide);
       CRef<CBlast4_database_info> result = GetDatabaseInfo(blastdb);
       if (result){
          retval.push_back(result);
       }
       else{
          *found_all = false;
	  if( missing_names ) missing_names->push_back(  blastdb->GetName() );
       }
    }
    return retval;
}

vector< CRef<objects::CBlast4_database_info> >
CBlastServices::GetDatabaseInfo(const string& dbname, bool is_protein,
	bool *found_all,
	vector<string> *missing_names)
{
    vector<CRef<objects::CBlast4_database_info> > retval;
    CRef<CBlast4_request> request;
    CRef<CBlast4_reply> reply(new CBlast4_reply);
    vector<string> all_db_names;
    vector<string>::iterator  it_db;
    bool l_multiple_db = false;
    string local_db_name = NStr::TruncateSpaces( dbname );

    if( found_all ){
	*found_all = false;
    }
    NStr::Tokenize(local_db_name, " \n\t", all_db_names);
    l_multiple_db = ( all_db_names.size() > 1 );

    request.Reset(new CBlast4_request);
    CRef<CBlast4_request_body> body(new CBlast4_request_body);
    CRef<CBlast4_get_databases_ex_request> db_ex_req(new CBlast4_get_databases_ex_request);

    body->SetGet_databases_ex( *db_ex_req );
    request->SetBody(*body);

    db_ex_req->SetParams().Add("FILTER_TYPE",string("EXACT"));
    db_ex_req->SetParams().Add("DBNAME",dbname);
    if( is_protein ) 
	db_ex_req->SetParams().Add("DBTYPE",string("prot"));
    else
	db_ex_req->SetParams().Add("DBTYPE",string("nucl"));


    try {
        CBlast4Client().Ask(*request, *reply);
    }
    catch(const CEofException &) {
        NCBI_THROW(CBlastServicesException, eRequestErr,
                   "No response from server, cannot complete request.");
    }
    // if no answer, call legacy method
    if( reply->GetBody().GetGet_databases_ex().Get().empty() ){
	return GetDatabaseInfoLegacy(dbname,is_protein,found_all,missing_names);
    }

    if( !reply->CanGetBody() ||  !reply->GetBody().IsGet_databases_ex() ) {
	if(found_all ) *found_all = false;
        NCBI_THROW(CBlastServicesException, eRequestErr,
                   "Unexpected response from server, cannot complete request. (GetDatabaseInfoEx)");
    }

    list< CRef< CBlast4_database_info > >::const_iterator it;
    it = reply->GetBody().GetGet_databases_ex().Get().begin();
    for( ; it != reply->GetBody().GetGet_databases_ex().Get().end(); it++){
	retval.push_back( *it );
	if( found_all ) {
	    if( l_multiple_db ) {
		string current_dbname = (*(*it)).GetDatabase().GetName();
		it_db = find(all_db_names.begin(),all_db_names.end(),current_dbname);
		if( it_db != all_db_names.end() ){
		    all_db_names.erase( it_db);
		}
	    }
	    else{
		//single db lookup
		*found_all = true;
		all_db_names.clear(); 
	    }
	}
    }

    if( found_all ){
	if( all_db_names.empty() ) {
	    *found_all = true;  // all resolved
	}
	else{
	    if( missing_names ) missing_names->assign(all_db_names.begin(),all_db_names.end()); 
	}
    }
    return retval;
}

void
CBlastServices::GetSequencesInfo(TSeqIdVector & seqids,   // in
                               const string & database, // in
                               char           seqtype,  // 'p' or 'n'
                               TBioseqVector& bioseqs,  // out
                               string       & errors,   // out
                               string       & warnings, // out
                               bool           verbose,  // in
                               bool           target_only)  // in
{
    s_GetSequences(seqids, database, seqtype, true, target_only, bioseqs, 
                   errors, warnings, verbose);
}

void                      
CBlastServices::GetSequences(TSeqIdVector & seqids,   // in
                           const string & database, // in
                           char           seqtype,  // 'p' or 'n'
                           TBioseqVector& bioseqs,  // out
                           string       & errors,   // out
                           string       & warnings, // out
                           bool           verbose,  // in
                           bool           target_only)  // in
{
    s_GetSequences(seqids, database, seqtype, false, target_only, bioseqs, 
                   errors, warnings, verbose);
}   


/// Extract information from the get-seq-parts reply object.
/// @param reply The reply object from blast4.
/// @param ids All Seq-ids for the requested sequences.
/// @param seq_data Seq_data for the sequences in question.
/// @param errors Any error messages found in the reply.
/// @param warnings Any warnings found in the reply.
static void
s_GetPartsFromReply(CRef<objects::CBlast4_reply>   reply,    // in
                    CBlastServices::TSeqIdVector & ids,      // out
                    CBlastServices::TSeqDataVector  & seq_data, // out
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
            s_BuildGetSeqPartsRequest(seqids, database, seqtype, errors);

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
    s_GetPartsFromReply(reply, ids, seq_data, errors, warnings);
}

objects::CBlast4_get_windowmasked_taxids_reply::Tdata
CBlastServices::GetTaxIdWithWindowMaskerSupport()
{
    if (m_WindowMaskedTaxIds.empty()) {
        CBlast4Client client;
        CRef<CBlast4_get_windowmasked_taxids_reply> reply;
        try { 
            reply = client.AskGet_windowmasked_taxids(); 
            if (m_Verbose) {
                NcbiCout << MSerial_AsnText << *reply << endl;
            }
            m_WindowMaskedTaxIds = reply->Set();
        }
        catch (const CEofException &) {
            NCBI_THROW(CBlastServicesException, eRequestErr,
                       "No response from server, cannot complete request.");
        }
    }
    return m_WindowMaskedTaxIds;
}

END_NCBI_SCOPE

/* @} */

