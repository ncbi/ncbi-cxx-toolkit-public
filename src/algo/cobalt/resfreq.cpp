static char const rcsid[] = "$Id$";

/*
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: resfreq.cpp

Author: Jason Papadopoulos

Contents: Implementation of CProfileData

******************************************************************************/

#include <ncbi_pch.hpp>
#include <algo/blast/api/seqsrc_seqdb.hpp>
#include <algo/blast/core/blast_rps.h>
#include <algo/cobalt/resfreq.hpp>

/// @file resfreq.cpp
/// Implementation of CProfileData

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

void
CProfileData::Load(EMapChoice choice, 
                   string dbname,
                   string resfreq_file)
{
    vector<string> dbpath;
    CSeqDB::FindVolumePaths(dbname, CSeqDB::eProtein, dbpath);

    m_ResFreqMmap = 0;
    m_PssmMmap = new CMemoryFile(dbpath[0] + ".rps");

    // Point to important locations in the PSSM file. Note that 
    // because only the PSSM file contains a list of where PSSMs
    // start, the file must be opened even if residue freqs and
    // not PSSMs are desired
   
    BlastRPSProfileHeader * profile_header = (BlastRPSProfileHeader *)
                                             m_PssmMmap->GetPtr();
    int num_db_seqs = profile_header->num_profiles;
    int num_rows = profile_header->start_offsets[num_db_seqs];
    Int4 *pssm_start = (Int4 *)profile_header->start_offsets + num_db_seqs + 1;

    switch (choice) {
    case eGetPssm:

        // make a 2-D array pointing to the memory-mapped
        // PSSM data

        m_SeqOffsets = profile_header->start_offsets;
        m_PssmRows = new Int4 * [num_rows];
        for (int i = 0; i < num_rows; i++) {
            m_PssmRows[i] = pssm_start + kAlphabetSize * i;
        }
        break;

    case eGetResFreqs:

        // to avoid exhausting virtual memory, do not keep
        // the residue frequency file and the PSSM file 
        // simultaneously open. Instead, copy the list of
        // offsets from the PSSM file, close the file, and
        // then memory map the residue frequencies

        m_SeqOffsets = new Int4 [num_db_seqs + 1];
        memcpy(m_SeqOffsets, profile_header->start_offsets,
                                (num_db_seqs + 1) * sizeof(Int4));
        delete m_PssmMmap;
        m_PssmMmap = 0;

        m_ResFreqMmap = new CMemoryFile(resfreq_file);
        m_ResFreqRows = new double * [num_rows];
        double *resfreq_start = (double *)(m_ResFreqMmap->GetPtr());

        for (int i = 0; i < num_rows; i++) {
            m_ResFreqRows[i] = resfreq_start + kAlphabetSize * i;
        }
        break;
    }
}

void
CProfileData::Clear()
{
    if (m_PssmMmap) {
        delete m_PssmMmap;
        delete [] m_PssmRows;
        m_PssmMmap = 0;
    }
    if (m_ResFreqMmap) {
        delete m_ResFreqMmap;
        delete [] m_SeqOffsets;
        delete [] m_ResFreqRows;
        m_ResFreqMmap = 0;
    }
}

END_SCOPE(cobalt)
END_NCBI_SCOPE
