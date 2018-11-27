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
 * Author: Tom Madden
 *
 * File Description:
 *   Access minhash files.
 *
 */

#ifndef MHFILE_HEADER__HPP
#define MHFILE_HEADER__HPP

#include <corelib/ncbifile.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/core/blast_export.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

class NCBI_XBLAST_EXPORT CMinHashException : public CException {
public:
     /// Error types
     enum EErrCode {
          /// Argument validation failed.
          eArgErr, 

          /// Zero length files.
          eFileEmpty,

          /// Memory allocation failed.
          eMemErr
     };

    /// Get a message describing the situation leading to the throw.
    virtual const char* GetErrCodeString() const override
    {
           switch ( GetErrCode() ) {
           case eArgErr:  return "eArgErr";
           case eFileEmpty: return "eFileErr";
           default:       return CException::GetErrCodeString();
           }
    }

    /// Include standard NCBI exception behavior.
    NCBI_EXCEPTION_DEFAULT(CMinHashException,CException);

};

#define KMER_LSH_ARRAY_SIZE 0x1000001
#define KMER_RANDOM_NUM_OFFSET 48 // 48 bytes after start of file.

/// Structure of file on disk
struct MinHashIndexHeader {
	/// Version of the index
	int version;
	/// Number of sequences represented in the index.
	int num_seqs;
	/// Number of hash functions
	int num_hashes;
	/// Should seg be run?
	int do_seg;
	/// Size of KMER used.
	int kmerNum;
	/// How many hash functions per band
	int rows_per_band;
	/// Width of the data (1 is one byte, 2 is a short, 0 or 4 is an integer)
	int dataWidth;
	/// zero is 15 letter, 1 is 10 letter alphabet.
	int Alphabet;
	/// Starts of the LSH matches.
	int LSHStart;
	/// How many are there (is this number of integers or 4*number)?
	int LSHSize;
	/// End of the LSH matches.
	uint64_t LSHMatchEnd;
	/// Residues in one Chunk (version 3 or greater)
	int chunkSize;
	/// For use in future.
	int futureUse;
};



/// Access data in Minhash files
class NCBI_XBLAST_EXPORT CMinHashFile : public CObject {
public:
    /// parameterized constructor
    CMinHashFile(const string& indexname);

    int GetVersion(void) const { return m_Data->version;}

    int GetNumSeqs(void) const { return m_Data->num_seqs;}

    /// Returns the number of values in an array of hashes (probably 32)
    int GetNumHashes(void) const { return m_Data->num_hashes;}

    int GetSegStatus(void) const { return m_Data->do_seg;}

    /// Returns the length of the KMER
    int GetKmerSize(void) const { return m_Data->kmerNum;}

    int GetRows(void) const { return m_Data->rows_per_band;}

    int GetDataWidth(void) const { return (m_Data->dataWidth == 0 ? 4 : m_Data->dataWidth);}

    /// One of two alphabets from Shiryev et al.(2007),  Bioinformatics, 23:2949-2951
    /// 0 means 15 letters (based on SE_B(14)), 1 means 10 letters (based on SE-V(10))
    int GetAlphabet(void) const { return m_Data->Alphabet;}

    int GetLSHSize(void) const { return m_Data->LSHSize;}

    int GetLSHStart(void) const {return m_Data->LSHStart;}
    // Needed?
    uint64_t GetLSHMatchEnd(void) const {return m_Data->LSHMatchEnd;}

    /// Get number of letters in a chunk (version 3 or higher)
    int GetChunkSize(void) const { return m_Data->chunkSize;}

    // Random numbers come after the LSH matches.
    uint32_t* GetRandomNumbers(void) const;

    /// Overrepresented KMERs
    void GetBadMers(vector<int> &badMers) const;

    /// LSH points for Buhler approach.
    unsigned char* GetKValues(void) const;

    uint64_t* GetLSHArray(void) const { return ((uint64_t*)m_MmappedIndex->GetPtr()) + (uint64_t)GetLSHStart()/8;}

    int* GetHits(uint64_t offset) const {return ((int*) m_MmappedIndex->GetPtr()) + offset/4;}

    uint32_t* GetMinHits(int oid) const {return (uint32_t*) (m_MinHitsData + (uint64_t)4*(GetNumHashes()+1)*oid); }

    /// Gets the database OID and vector of hash values for entry given by oid.
    /// @param oid Entry to fetch.
    /// @param subjectOid OID of the BLAST database (for current volume)
    /// @param hits Vector of the hash values read from disk.
    void GetMinHits(int oid, int& subjectOid, vector<uint32_t>& hits) const;

    /// Returns the number of hash arrays.
    int GetNumSignatures() const {return (m_DataFileSize/(GetDataWidth()*GetNumHashes()+4));}

private:

    uint32_t* x_GetMinHits32(int oid, int& subjectOid) const;

    uint16_t* x_GetMinHits16(int oid, int& subjectOid) const;

    unsigned char* x_GetMinHits8(int oid, int& subjectOid) const;

    // pointer to memory mapped index file.
    auto_ptr<CMemoryFile> m_MmappedIndex;

    // pointer to memory mapped data file.
    auto_ptr<CMemoryFile> m_MmappedData;

    MinHashIndexHeader* m_Data;

    /// Pointer to start of min-hits arrays.
    unsigned char* m_MinHitsData;

    void x_Init();

    /// m_MmappedData File size 
    Int8 m_DataFileSize;

    /// Name of the index file.
    string m_IndexName;

};

END_SCOPE(blast)
END_NCBI_SCOPE
#endif /* MHFILE_HEADER__HPP */
