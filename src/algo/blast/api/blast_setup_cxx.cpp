#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

/* ===========================================================================
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
 * Author:  Christiam Camacho
 *
 * ===========================================================================
 */

/// @file blast_setup_cxx.cpp
/// Auxiliary setup functions for Blast objects interface.

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/metareg.hpp>

#include "blast_setup.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

Uint1
GetQueryEncoding(EProgram program)
{
    Uint1 retval = 0;

    switch (program) {
    case eBlastn: 
        retval = BLASTNA_ENCODING; 
        break;

    case eBlastp: 
    case eTblastn:
    case eRPSBlast: 
    case ePSIBlast:
        retval = BLASTP_ENCODING; 
        break;

    case eBlastx:
    case eTblastx:
    case eRPSTblastn:
        retval = NCBI4NA_ENCODING;
        break;

    default:
        NCBI_THROW(CBlastException, eBadParameter, "Query Encoding");
    }

    return retval;
}

Uint1
GetSubjectEncoding(EProgram program)
{
    Uint1 retval = 0;

    switch (program) {
    case eBlastn: 
        retval = BLASTNA_ENCODING; 
        break;

    case eBlastp: 
    case eBlastx:
    case ePSIBlast:
        retval = BLASTP_ENCODING; 
        break;

    case eTblastn:
    case eTblastx:
        retval = NCBI4NA_ENCODING;
        break;

    default:
        NCBI_THROW(CBlastException, eBadParameter, "Subject Encoding");
    }

    return retval;
}

TSeqPos CalculateSeqBufferLength(TSeqPos sequence_length, Uint1 encoding,
                                 objects::ENa_strand strand, 
                                 ESentinelType sentinel)
                                 THROWS((CBlastException))
{
    TSeqPos retval = 0;

    if (sequence_length == 0) {
        return retval;
    }

    switch (encoding) {
    // Strand and sentinels are irrelevant in this encoding.
    // Strand is always plus and sentinels cannot be represented
    case NCBI2NA_ENCODING:
        ASSERT(sentinel == eNoSentinels);
        ASSERT(strand == eNa_strand_plus);
        retval = sequence_length / COMPRESSION_RATIO + 1;
        break;

    case NCBI4NA_ENCODING:
        if (sentinel == eSentinels) {
            if (strand == eNa_strand_both) {
                retval = sequence_length * 2;
                retval += 3;
            } else {
                retval = sequence_length + 2;
            }
        } else {
            if (strand == eNa_strand_both) {
                retval = sequence_length * 2;
            } else {
                retval = sequence_length;
            }
        }
        break;

    case BLASTP_ENCODING:
        ASSERT(sentinel == eSentinels);
        ASSERT(strand == eNa_strand_unknown);
        retval = sequence_length + 2;
        break;

    default:
        NCBI_THROW(CBlastException, eBadParameter, "Unsupported encoding");
    }

    return retval;
}

Uint1 GetSentinelByte(Uint1 encoding) THROWS((CBlastException))
{
    switch (encoding) {
    case BLASTP_ENCODING:
        return NULLB;

    case NCBI4NA_ENCODING:
    case BLASTNA_ENCODING:
        return 0xF;

    default:
        NCBI_THROW(CBlastException, eBadParameter, "Unsupported encoding");
    }
}

#if 0
// Not used right now, need to complete implementation
void
BLASTGetTranslation(const Uint1* seq, const Uint1* seq_rev,
        const int nucl_length, const short frame, Uint1* translation)
{
    TSeqPos ni = 0;     // index into nucleotide sequence
    TSeqPos pi = 0;     // index into protein sequence

    const Uint1* nucl_seq = frame >= 0 ? seq : seq_rev;
    translation[0] = NULLB;
    for (ni = ABS(frame)-1; ni < (TSeqPos) nucl_length-2; ni += CODON_LENGTH) {
        Uint1 residue = CGen_code_table::CodonToIndex(nucl_seq[ni+0], 
                                                      nucl_seq[ni+1],
                                                      nucl_seq[ni+2]);
        if (IS_residue(residue))
            translation[pi++] = residue;
    }
    translation[pi++] = NULLB;

    return;
}
#endif

string
FindMatrixPath(const char* matrix_name, bool is_prot)
{
    string retval;
    string full_path;       // full path to matrix file

    if (!matrix_name)
        return retval;

    string mtx(matrix_name);
    mtx = NStr::ToUpper(mtx);

    // Look for matrix file in local directory
    full_path = mtx;
    if (CFile(full_path).Exists()) {
        return retval;
    }
    
    string path("");

    // Obtain the matrix path from the ncbi configuration file
    try {
        CMetaRegistry::SEntry sentry;
        sentry = CMetaRegistry::Load("ncbi", CMetaRegistry::eName_RcOrIni);
        path = sentry.registry ? sentry.registry->Get("NCBI", "Data") : "";
    } catch (const CRegistryException&) { /* ignore */ }
    
    full_path = CFile::MakePath(path, mtx);
    if (CFile(full_path).Exists()) {
        retval = full_path;
        retval.erase(retval.size() - mtx.size());
        return retval;
    }

    // Try appending "aa" or "nt" 
    full_path = path;
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += is_prot ? "aa" : "nt";
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += mtx;
    if (CFile(full_path).Exists()) {
        retval = full_path;
        retval.erase(retval.size() - mtx.size());
        return retval;
    }

    // Try using local "data" directory
    full_path = "data";
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += mtx;
    if (CFile(full_path).Exists()) {
        retval = full_path;
        retval.erase(retval.size() - mtx.size());
        return retval;
    }

    CNcbiApplication* app = CNcbiApplication::Instance();
    if (!app)
        return retval;

    const string& blastmat_env = app->GetEnvironment().Get("BLASTMAT");
    if (CFile(blastmat_env).Exists()) {
        full_path = blastmat_env;
        full_path += CFile::AddTrailingPathSeparator(full_path);
        full_path += is_prot ? "aa" : "nt";
        full_path += CFile::AddTrailingPathSeparator(full_path);
        full_path += mtx;
        if (CFile(full_path).Exists()) {
            retval = full_path;
            retval.erase(retval.size() - mtx.size());
            return retval;
        }
    }

#ifdef OS_UNIX
    full_path = BLASTMAT_DIR;
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += is_prot ? "aa" : "nt";
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += mtx;
    if (CFile(full_path).Exists()) {
        retval = full_path;
        retval.erase(retval.size() - mtx.size());
        return retval;
    }
#endif

    // Try again without the "aa" or "nt"
    if (CFile(blastmat_env).Exists()) {
        full_path = blastmat_env;
        full_path += CFile::AddTrailingPathSeparator(full_path);
        full_path += mtx;
        if (CFile(full_path).Exists()) {
            retval = full_path;
            retval.erase(retval.size() - mtx.size());
            return retval;
        }
    }

#ifdef OS_UNIX
    full_path = BLASTMAT_DIR;
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += mtx;
    if (CFile(full_path).Exists()) {
        retval = full_path;
        retval.erase(retval.size() - mtx.size());
        return retval;
    }
#endif

    return retval;
}

/// Checks if a BLAST database exists at a given file path: looks for 
/// an alias file first, then for an index file
static bool BlastDbFileExists(string& path, bool is_prot)
{
    // Check for alias file first
    string full_path = path + (is_prot ? ".pal" : ".nal");
    if (CFile(full_path).Exists())
        return true;
    // Check for an index file
    full_path = path + (is_prot ? ".pin" : ".nin");
    if (CFile(full_path).Exists())
        return true;
    return false;
}

string
FindBlastDbPath(const char* dbname, bool is_prot)
{
    string retval;
    string full_path;       // full path to matrix file

    if (!dbname)
        return retval;

    string database(dbname);

    // Look for matrix file in local directory
    full_path = database;
    if (BlastDbFileExists(full_path, is_prot)) {
        return retval;
    }

    CNcbiApplication* app = CNcbiApplication::Instance();
    if (app) {
        const string& blastdb_env = app->GetEnvironment().Get("BLASTDB");
        if (CFile(blastdb_env).Exists()) {
            full_path = blastdb_env;
            full_path += CFile::AddTrailingPathSeparator(full_path);
            full_path += database;
            if (BlastDbFileExists(full_path, is_prot)) {
                retval = full_path;
                retval.erase(retval.size() - database.size());
                return retval;
            }
        }
    }

    // Obtain the matrix path from the ncbi configuration file
    CMetaRegistry::SEntry sentry;
    sentry = CMetaRegistry::Load("ncbi", CMetaRegistry::eName_RcOrIni);
    string path = 
        sentry.registry ? sentry.registry->Get("BLAST", "BLASTDB") : "";

    full_path = CFile::MakePath(path, database);
    if (BlastDbFileExists(full_path, is_prot)) {
        retval = full_path;
        retval.erase(retval.size() - database.size());
        return retval;
    }

    return retval;
}

unsigned int
GetNumberOfFrames(EProgram p)
{
    unsigned int retval = 0;

    switch (p) {
    case eBlastn:
    case eMegablast:
    case eDiscMegablast:
        retval = NUM_STRANDS;
        break;
    case eBlastp:
    case eRPSBlast:
    case eTblastn: 
    case ePSIBlast:
        retval = 1;
        break;
    case eBlastx:
    case eTblastx:
    case eRPSTblastn: 
        retval = NUM_FRAMES;
        break;
    default:
        {
            int debug_value = static_cast<int>(p);
            string msg = "Cannot get number of frames for invalid program ";
            msg += "type: " + NStr::IntToString(debug_value);
            NCBI_THROW(CBlastException, eBadParameter, msg);
        }
    }

    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.80  2005/03/04 16:53:27  camacho
 * more doxygen fixes
 *
 * Revision 1.79  2005/01/21 17:38:57  camacho
 * Handle zero-length sequences
 *
 * Revision 1.78  2004/12/29 17:15:42  camacho
 * Use NStr utility functions
 *
 * Revision 1.77  2004/12/20 20:17:00  camacho
 * + PSI-BLAST
 *
 * Revision 1.76  2004/11/12 16:43:34  camacho
 * Add handling of missing EProgram values to GetNumberOfFrames
 *
 * Revision 1.75  2004/09/21 13:50:38  dondosha
 * GetNumberOfFrames now returns 6 for RPS tblastn
 *
 * Revision 1.74  2004/08/17 20:01:57  camacho
 * Handle misconfigured NCBI configuration file
 *
 * Revision 1.73  2004/08/17 15:13:00  ivanov
 * Moved GetProgramFromBlastProgramType() from blast_setup_cxx.cpp
 * to blast_options_cxx.cpp
 *
 * Revision 1.72  2004/08/11 14:24:50  camacho
 * Move FindGeneticCode
 *
 * Revision 1.71  2004/08/02 13:28:28  camacho
 * Minor
 *
 * Revision 1.70  2004/07/06 15:49:30  dondosha
 * Added GetProgramFromBlastProgramType function
 *
 * Revision 1.69  2004/06/02 15:59:14  bealer
 * - Isolate objmgr dependent code.
 *
 * Revision 1.68  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.67  2004/04/19 19:52:02  papadopo
 * correct off-by-one error in sequence size computation
 *
 * Revision 1.66  2004/04/16 14:28:49  papadopo
 * add use of eRPSBlast and eRPSTblastn programs
 *
 * Revision 1.65  2004/04/07 03:06:15  camacho
 * Added blast_encoding.[hc], refactoring blast_stat.[hc]
 *
 * Revision 1.64  2004/04/06 20:45:28  dondosha
 * Added function FindBlastDbPath: should be moved to seqdb library
 *
 * Revision 1.63  2004/03/24 19:14:48  dondosha
 * Fixed memory leaks
 *
 * Revision 1.62  2004/03/19 19:22:55  camacho
 * Move to doxygen group AlgoBlast, add missing CVS logs at EOF
 *
 * Revision 1.61  2004/03/15 19:57:52  dondosha
 * SetupSubjects takes just program argument instead of CBlastOptions*
 *
 * Revision 1.60  2004/03/11 17:26:46  dicuccio
 * Use NStr::ToUpper() instead of transform
 *
 * Revision 1.59  2004/03/09 18:53:25  dondosha
 * Do not set db length and number of sequences options to real values - these are calculated and assigned to parameters structure fields
 *
 * Revision 1.58  2004/03/06 00:39:47  camacho
 * Some refactorings, changed boolen parameter to enum in GetSequence
 *
 * Revision 1.57  2004/02/24 18:14:56  dondosha
 * Set the maximal length in the set of queries, when filling BlastQueryInfo structure
 *
 * Revision 1.56  2004/02/18 15:16:28  camacho
 * Consolidated ncbi2na mask definition
 *
 * Revision 1.55  2004/01/07 17:39:27  vasilche
 * Fixed include path to genbank loader.
 *
 * Revision 1.54  2003/12/29 17:00:57  camacho
 * Update comment
 *
 * Revision 1.53  2003/12/15 19:55:14  camacho
 * Minor fix to ensure exception safety
 *
 * Revision 1.52  2003/12/03 16:43:47  dondosha
 * Renamed BlastMask to BlastMaskLoc, BlastResults to BlastHSPResults
 *
 * Revision 1.51  2003/11/26 18:36:45  camacho
 * Renaming blast_option*pp -> blast_options*pp
 *
 * Revision 1.50  2003/11/26 18:24:00  camacho
 * +Blast Option Handle classes
 *
 * Revision 1.49  2003/11/24 17:12:44  dondosha
 * Query info structure does not have a total_length member any more; use last context offset
 *
 * Revision 1.48  2003/11/06 21:25:37  camacho
 * Revert previous change, add assertions
 *
 * Revision 1.47  2003/11/04 17:14:22  dondosha
 * Length in subject sequence block C structure should not include sentinels
 *
 * Revision 1.46  2003/10/31 19:45:03  camacho
 * Fix setting of subject sequence length
 *
 * Revision 1.45  2003/10/31 16:53:32  dondosha
 * Set length in query sequence block correctly
 *
 * Revision 1.44  2003/10/29 04:46:16  camacho
 * Use fixed AutoPtr for GetSequence return value
 *
 * Revision 1.43  2003/10/27 21:27:36  camacho
 * Remove extra argument to GetSequenceView, minor refactorings
 *
 * Revision 1.42  2003/10/22 14:21:55  camacho
 * Added sanity checking assertions
 *
 * Revision 1.41  2003/10/21 13:04:54  camacho
 * Fix bug in SetupSubjects, use sequence blk set functions
 *
 * Revision 1.40  2003/10/16 13:38:54  coulouri
 * use anonymous exceptions to fix unreferenced variable compiler warning
 *
 * Revision 1.39  2003/10/15 18:18:00  camacho
 * Fix to setup query info structure for proteins
 *
 * Revision 1.38  2003/10/15 15:09:32  camacho
 * Changes from Mike DiCuccio to use GetSequenceView to retrieve sequences.
 *
 * Revision 1.37  2003/10/08 15:13:56  dondosha
 * Test if subject mask is not NULL before converting to a C structure
 *
 * Revision 1.36  2003/10/08 15:05:47  dondosha
 * Test if mask is not NULL before converting to a C structure
 *
 * Revision 1.35  2003/10/07 17:34:05  dondosha
 * Add lower case masks to SSeqLocs forming the vector of sequence locations
 *
 * Revision 1.34  2003/10/03 16:12:18  dondosha
 * Fix in previous change for plus strand search
 *
 * Revision 1.33  2003/10/02 22:10:46  dondosha
 * Corrections for one-strand translated searches
 *
 * Revision 1.32  2003/09/30 03:23:18  camacho
 * Fixes to FindMatrixPath
 *
 * Revision 1.31  2003/09/29 21:38:29  camacho
 * Assign retval only when successfully found path to matrix
 *
 * Revision 1.30  2003/09/29 20:35:03  camacho
 * Replace abort() with exception in GetNumberOfFrames
 *
 * Revision 1.29  2003/09/16 16:48:13  dondosha
 * Use BLAST_PackDNA and BlastSetUp_SeqBlkNew from the core blast library for setting up subject sequences
 *
 * Revision 1.28  2003/09/12 17:52:42  camacho
 * Stop using pair<> as return value from GetSequence
 *
 * Revision 1.27  2003/09/11 20:55:01  camacho
 * Temporary fix for AutoPtr return value
 *
 * Revision 1.26  2003/09/11 17:45:03  camacho
 * Changed CBlastOption -> CBlastOptions
 *
 * Revision 1.25  2003/09/10 04:27:43  camacho
 * 1) Minor change to return type of GetSequence
 * 2) Fix to previous revision
 *
 * Revision 1.24  2003/09/09 22:15:02  dondosha
 * Added cast in return statement in GetSequence method, fixing compiler error
 *
 * Revision 1.23  2003/09/09 15:57:23  camacho
 * Fix indentation
 *
 * Revision 1.22  2003/09/09 14:21:39  coulouri
 * change blastkar.h to blast_stat.h
 *
 * Revision 1.21  2003/09/09 12:57:15  camacho
 * + internal setup functions, use smart pointers to handle memory mgmt
 *
 * Revision 1.20  2003/09/05 19:06:31  camacho
 * Use regular new to allocate genetic code string
 *
 * Revision 1.19  2003/09/03 19:36:27  camacho
 * Fix include path for blast_setup.hpp
 *
 * Revision 1.18  2003/08/28 22:42:54  camacho
 * Change BLASTGetSequence signature
 *
 * Revision 1.17  2003/08/28 15:49:02  madden
 * Fix for packing DNA as well as correct buflen
 *
 * Revision 1.16  2003/08/25 16:24:14  camacho
 * Updated BLASTGetMatrixPath
 *
 * Revision 1.15  2003/08/19 17:39:07  camacho
 * Minor fix to use of metaregistry class
 *
 * Revision 1.14  2003/08/18 20:58:57  camacho
 * Added blast namespace, removed *__.hpp includes
 *
 * Revision 1.13  2003/08/14 13:51:24  camacho
 * Use CMetaRegistry class to load the ncbi config file
 *
 * Revision 1.12  2003/08/11 15:17:39  dondosha
 * Added algo/blast/core to all #included headers
 *
 * Revision 1.11  2003/08/11 14:00:41  dicuccio
 * Indenting changes.  Fixed use of C++ namespaces (USING_SCOPE(objects) inside of
 * BEGIN_NCBI_SCOPE block)
 *
 * Revision 1.10  2003/08/08 19:43:07  dicuccio
 * Compilation fixes: #include file rearrangement; fixed use of 'list' and
 * 'vector' as variable names; fixed missing ostrea<< for __int64
 *
 * Revision 1.9  2003/08/04 15:18:23  camacho
 * Minor fixes
 *
 * Revision 1.8  2003/08/01 22:35:02  camacho
 * Added function to get matrix path (fixme)
 *
 * Revision 1.7  2003/08/01 17:40:56  dondosha
 * Use renamed functions and structures from local blastkar.h
 *
 * Revision 1.6  2003/07/31 19:45:33  camacho
 * Eliminate Ptr notation
 *
 * Revision 1.5  2003/07/30 15:00:01  camacho
 * Do not use Malloc/MemNew/MemFree
 *
 * Revision 1.4  2003/07/25 13:55:58  camacho
 * Removed unnecessary #includes
 *
 * Revision 1.3  2003/07/24 18:22:50  camacho
 * #include blastkar.h
 *
 * Revision 1.2  2003/07/23 21:29:06  camacho
 * Update BLASTFindGeneticCode to get genetic code string with C++ toolkit
 *
 * Revision 1.1  2003/07/10 18:34:19  camacho
 * Initial revision
 *
 *
 * ===========================================================================
 */
