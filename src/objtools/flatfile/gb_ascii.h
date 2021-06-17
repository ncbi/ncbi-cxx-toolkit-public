/* gb_ascii.h
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
 * File Name:  gb_ascii.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *      Build GenBank format entry block.
 *
 */

#ifndef _GBASCII_
#define _GBASCII_

BEGIN_NCBI_SCOPE

bool GetGenBankInstContig(DataBlkPtr entry, objects::CBioseq& bsp, ParserPtr pp);

/* routines for checking the feature location has join or order
* among other segment
*/
// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
void CheckFeatSeqLoc(TEntryList& seq_entries);
// LCOV_EXCL_STOP
bool GenBankAscii(ParserPtr pp);

END_NCBI_SCOPE

#endif
