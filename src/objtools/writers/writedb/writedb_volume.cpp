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
 * Author:  Kevin Bealer
 *
 */

/// @file writedb_volume.cpp
/// Implementation for the CWriteDB_Volume class.
/// class for WriteDB.

#include <ncbi_pch.hpp>
#include "writedb_volume.hpp"
#include <objtools/writers/writedb/writedb_error.hpp>
#include <iostream>

BEGIN_NCBI_SCOPE

/// Include C++ std library symbols.
USING_SCOPE(std);

CWriteDB_Volume::CWriteDB_Volume(const string & dbname,
                                 bool           protein,
                                 const string & title,
                                 const string & date,
                                 int            index,
                                 Uint8          max_file_size,
                                 Uint8          max_letters,
                                 EIndexType     indices)
    : m_DbName      (dbname),
      m_Protein     (protein),
      m_Title       (title),
      m_Date        (date),
      m_Index       (index),
      m_Indices     (indices),
      m_OID         (0),
      m_Open        (true)
{
    m_VolName = CWriteDB_File::MakeShortName(m_DbName, m_Index);
    
    m_Idx.Reset(new CWriteDB_IndexFile(dbname,
                                       protein,
                                       title,
                                       date,
                                       index,
                                       max_file_size));
    
    m_Hdr.Reset(new CWriteDB_HeaderFile(dbname,
                                        protein,
                                        index,
                                        max_file_size));
    
    m_Seq.Reset(new CWriteDB_SequenceFile(dbname,
                                          protein,
                                          index,
                                          max_file_size,
                                          max_letters));
    
    if (m_Indices != CWriteDB::eNoIndex) {
        bool sparse =
            (m_Indices & CWriteDB::eSparseIndex) == CWriteDB::eSparseIndex;
        
        if (m_Protein) {
            m_PigIsam.Reset(new CWriteDB_Isam(ePig,
                                              dbname,
                                              protein,
                                              index,
                                              max_file_size,
                                              false));
        }
        
        m_GiIsam.Reset(new CWriteDB_Isam(eGi,
                                         dbname,
                                         protein,
                                         index,
                                         max_file_size,
                                         false));
        
        m_AccIsam.Reset(new CWriteDB_Isam(eAcc,
                                          dbname,
                                          protein,
                                          index,
                                          max_file_size,
                                          sparse));
        
        if (m_Indices & CWriteDB::eAddTrace) {
            m_TraceIsam.Reset(new CWriteDB_Isam(eTrace,
                                                dbname,
                                                protein,
                                                index,
                                                max_file_size,
                                                false));
        }
        
        if (m_Indices & CWriteDB::eAddHash) {
            m_HashIsam.Reset(new CWriteDB_Isam(eHash,
                                               dbname,
                                               protein,
                                               index,
                                               max_file_size,
                                               false));
        }
    }
}

CWriteDB_Volume::~CWriteDB_Volume()
{
    if (m_Open) {
        Close();
    }
}

bool CWriteDB_Volume::WriteSequence(const string  & seq,
                                    const string  & ambig,
                                    const string  & binhdr,
                                    const TIdList & idlist,
                                    int             pig,
                                    int             hash)
{
    // Zero is a legal hash value, but we should not be computing the
    // hash value if there is no corresponding ISAM file.
    
    _ASSERT((! hash) || m_HashIsam.NotEmpty());
    
    if (! (seq.size() && binhdr.size())) {
            NCBI_THROW(CWriteDBException,
                       eArgErr,
                       "Error: Cannot find CBioseq or deflines.");
    }
    
    _ASSERT(m_Open);
    
    int length = (m_Protein
                  ? (int) seq.size()
                  : x_FindNuclLength(seq));
    
    bool overfull = false;
    
    if (! (m_Idx->CanFit() &&
           m_Hdr->CanFit((int)binhdr.size()) &&
           m_Seq->CanFit((int)(seq.size() + ambig.size()), length))) {
        overfull = true;
    }
    
    if (m_Indices != CWriteDB::eNoIndex) {
        int num = (int)idlist.size();
        
        if (! (m_AccIsam->CanFit(num) &&
               m_GiIsam->CanFit(num) &&
               (m_TraceIsam.Empty() || m_TraceIsam->CanFit(num)))) {
            overfull = true;
        }
        
        if (m_Protein && (! m_PigIsam->CanFit(1))) {
            overfull = true;
        }
        
        if (m_HashIsam.NotEmpty() && (! m_HashIsam->CanFit(1))) {
            overfull = true;
        }
    }
    
    // Exception - if volume has no data, ignore limits.
    
    if (m_OID && overfull) {
        return false;
    }
    
    int off_hdr(0), off_seq(0), off_amb(0);
    
    m_Hdr->AddSequence(binhdr, off_hdr);
    
    if (m_Protein) {
        m_Seq->AddSequence(seq, off_seq, length);
        m_Idx->AddSequence((int) seq.size(), off_hdr, off_seq);
    } else {
        m_Seq->AddSequence(seq, ambig, off_seq, off_amb, length);
        m_Idx->AddSequence(length, off_hdr, off_seq, off_amb);
    }
    
    if (m_Indices != CWriteDB::eNoIndex) {
        m_AccIsam->AddIds(m_OID, idlist);
        m_GiIsam->AddIds(m_OID, idlist);
        
        if (m_Protein && pig) {
            m_PigIsam->AddPig(m_OID, pig);
        }
        
        if (m_TraceIsam.NotEmpty()) {
            m_TraceIsam->AddIds(m_OID, idlist);
        }
        
        if (m_HashIsam.NotEmpty()) {
            m_HashIsam->AddHash(m_OID, hash);
        }
    }
    
    m_OID ++;
    
    return true;
}

int CWriteDB_Volume::x_FindNuclLength(const string & seq)
{
    _ASSERT(! m_Protein);
    _ASSERT(seq.size());
    
    int wholebytes = (int) seq.size() - 1;
    return (wholebytes << 2) + (seq[wholebytes] & 0x3);
}

void CWriteDB_Volume::Close()
{
    if (m_Open) {
        m_Open = false;
        
        // close each file.
        m_Idx->Close();
        m_Hdr->Close();
        m_Seq->Close();
        
        if (m_Indices != CWriteDB::eNoIndex) {
            if (m_Protein) {
                m_PigIsam->Close();
            }
            m_GiIsam->Close();
            m_AccIsam->Close();
            
            if (m_TraceIsam.NotEmpty()) {
                m_TraceIsam->Close();
            }
            
            if (m_HashIsam.NotEmpty()) {
                m_HashIsam->Close();
            }
        }
    }
}

void CWriteDB_Volume::RenameSingle()
{
    _ASSERT(! m_Open);
    m_VolName = m_DbName;
    
    // rename all files to 'single volume' notation.
    m_Idx->RenameSingle();
    m_Hdr->RenameSingle();
    m_Seq->RenameSingle();
    
    if (m_Indices != CWriteDB::eNoIndex) {
        if (m_Protein) {
            m_PigIsam->RenameSingle();
        }
        m_GiIsam->RenameSingle();
        m_AccIsam->RenameSingle();
        
        if (m_TraceIsam.NotEmpty()) {
            m_TraceIsam->RenameSingle();
        }
        
        if (m_HashIsam.NotEmpty()) {
            m_HashIsam->RenameSingle();
        }
    }
}

void CWriteDB_Volume::ListFiles(vector<string> & files) const
{
    files.push_back(m_Idx->GetFilename());
    files.push_back(m_Hdr->GetFilename());
    files.push_back(m_Seq->GetFilename());
    
    if (m_AccIsam.NotEmpty()) {
        m_AccIsam->ListFiles(files);
    }
    
    if (m_GiIsam.NotEmpty()) {
        m_GiIsam->ListFiles(files);
    }
    
    if (m_PigIsam.NotEmpty()) {
        m_PigIsam->ListFiles(files);
    }
    
    if (m_TraceIsam.NotEmpty()) {
        m_TraceIsam->ListFiles(files);
    }
    
    if (m_HashIsam.NotEmpty()) {
        m_HashIsam->ListFiles(files);
    }
}

END_NCBI_SCOPE

