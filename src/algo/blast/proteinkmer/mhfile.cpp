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


#include <ncbi_pch.hpp>
#include <algo/blast/proteinkmer/mhfile.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast);



/// Parameterized constructor
CMinHashFile::CMinHashFile(const string& indexname) : m_IndexName(indexname)
{
	if (indexname == "")
		NCBI_THROW(CMinHashException, eArgErr, "Indexname empty");
	
	m_MmappedIndex.reset(new CMemoryFile(indexname + ".pki")); 
	m_MmappedData.reset(new CMemoryFile(indexname + ".pkd")); 
	x_Init();
}

void 
CMinHashFile::x_Init(void)
{
	m_Data = (MinHashIndexHeader*) m_MmappedIndex->GetPtr();
	if (!m_Data)
	{
		string errmsg = m_IndexName + ".pki has zero length";
		NCBI_THROW(CMinHashException, eFileEmpty, errmsg);
	}
	
	m_MinHitsData = (unsigned char*) (m_MmappedData->GetPtr());
	m_DataFileSize = m_MmappedData->GetFileSize();
	if (!m_MinHitsData || m_DataFileSize == 0)
	{
		string errmsg = m_IndexName + ".pkd has zero length";
		NCBI_THROW(CMinHashException, eFileEmpty, errmsg);
	}
}

void 
CMinHashFile::GetMinHits(int oid, int& subject_oid, vector<uint32_t>& hits) const
{
	int width = GetDataWidth();
	int numHashes = GetNumHashes();
	if (hits.size() < numHashes)
		hits.resize(numHashes);

	uint32_t* a = &(hits[0]);
	if (width == 4)
	{
		uint32_t* array = x_GetMinHits32(oid, subject_oid);
#pragma ivdep
		for (int index=0; index<numHashes; index++)
			a[index] = array[index];
	}
	else if (width == 2)
	{
		uint16_t* array = x_GetMinHits16(oid, subject_oid);
#pragma ivdep
		for (int index=0; index<numHashes; index++)
			a[index] = array[index];
	} 
	else if (width == 1)
	{
		uint8_t* array = x_GetMinHits8(oid, subject_oid);
#pragma ivdep
		for (int index=0; index<numHashes; index++)
			a[index] = array[index];
	}
}

uint32_t* 
CMinHashFile::GetRandomNumbers(void) const 
{
	int extraOffset=0;
	if (GetVersion() > 1)
		extraOffset = 2; // Two four-byte integers.

	return ((uint32_t*)m_MmappedIndex->GetPtr()) + KMER_RANDOM_NUM_OFFSET/4 + extraOffset;
}

void
CMinHashFile::GetBadMers(vector<int> &badMers) const
{
	if (GetVersion() < 3)
		return;

	int extraOffset = 2;
	
	uint32_t* array = ((uint32_t*)m_MmappedIndex->GetPtr()) + KMER_RANDOM_NUM_OFFSET/4 + extraOffset;
	int num = array[0];

	for (int i=1; i<=num; i++)
		badMers.push_back(array[i]);
	return;
}

unsigned char* 
CMinHashFile::GetKValues(void) const 
{
	int extraOffset=0;
	if (GetVersion() > 1)
		extraOffset = 8; // Eight bytes.

	return ((unsigned char*)m_MmappedIndex->GetPtr()) + KMER_RANDOM_NUM_OFFSET + 2*4*GetNumHashes() + extraOffset;
}

inline uint32_t*
CMinHashFile::x_GetMinHits32(int oid, int& subject_oid) const
{
	int num_hashes = GetNumHashes();
	uint32_t* array = (uint32_t*) (m_MinHitsData + (uint64_t)(GetDataWidth()*num_hashes+4)*oid);
	subject_oid = array[num_hashes];
	
	return array;
}

inline uint16_t*
CMinHashFile::x_GetMinHits16(int oid, int& subject_oid) const
{
	int num_hashes = GetNumHashes();
	uint16_t* array = (uint16_t*) (m_MinHitsData + (uint64_t)(GetDataWidth()*num_hashes+4)*oid);
	subject_oid = *(uint32_t*) &(array[num_hashes]);
	// subject_oid = *(uint32_t*) ((m_MinHitsData + (uint64_t)((num_hashes+2)*oid + num_hashes)));
	
	return array;
}

inline unsigned char*
CMinHashFile::x_GetMinHits8(int oid, int& subject_oid) const
{
	int num_hashes = GetNumHashes();
	unsigned char* array = (unsigned char*) (m_MinHitsData + (uint64_t)(GetDataWidth()*num_hashes+4)*oid);
	subject_oid = *(uint32_t*) &(array[num_hashes]);
	// subject_oid = *(uint32_t*) ((m_MinHitsData + (uint64_t)((num_hashes+4)*oid + num_hashes))/4);
	
	return array;
}

END_SCOPE(blast)
END_NCBI_SCOPE
