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

/// @file seqdbtax.cpp
/// Implementation for the CSeqDBVol class, which provides an
/// interface for all functionality of one database volume.

#include <ncbi_pch.hpp>
#include <objtools/error_codes.hpp>
#include "seqdbtax.hpp"

/// Tell the error reporting framework what part of the code we're in.
#define NCBI_USE_ERRCODE_X   Objtools_SeqDBTax

BEGIN_NCBI_SCOPE

CSeqDBTaxInfo::CSeqDBTaxInfo(CSeqDBAtlas & atlas)
    : m_Atlas        (atlas),
      m_Lease        (atlas),
      m_AllTaxidCount(0),
      m_TaxData      (0)
{
    typedef CSeqDBAtlas::TIndx TIndx;
    
    // It is reasonable for this database to not exist.
    
    CSeqDBLockHold locked(m_Atlas);
    
    m_IndexFN =
        SeqDB_FindBlastDBPath("taxdb.bti", '-', 0, true, m_Atlas, locked);

    if (m_IndexFN.size()) {
        m_DataFN = m_IndexFN;
        m_DataFN[m_DataFN.size()-1] = 'd';
    }
    
    if (! (m_IndexFN.size() &&
           m_DataFN.size()  &&
           CFile(m_IndexFN).Exists() &&
           CFile(m_DataFN).Exists())) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Error: Tax database file not found.");
    }
    
    // Size for header data plus one taxid object.
    
    Uint4 data_start = (4 +    // magic
                        4 +    // taxid count
                        16);   // 4 reserved fields
    
    Uint4 idx_file_len = (Uint4) CFile(m_IndexFN).GetLength();
    
    if (idx_file_len < (data_start + sizeof(CSeqDBTaxId))) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Error: Tax database file not found.");
    }
    
    CSeqDBMemLease lease(m_Atlas);
    
    // Last check-up of the database validity
    
    m_Atlas.Lock(locked);
    m_Atlas.GetRegion(lease, m_IndexFN, 0, data_start);
    
    Uint4 * magic_num_ptr = (Uint4 *) lease.GetPtr(0);
    
    const unsigned TAX_DB_MAGIC_NUMBER = 0x8739;
    
    if (TAX_DB_MAGIC_NUMBER != SeqDB_GetStdOrd(magic_num_ptr ++)) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Error: Tax database file has wrong magic number.");
    }
    
    m_AllTaxidCount = SeqDB_GetStdOrd(magic_num_ptr ++);
    
    // Skip the four reserved fields
    magic_num_ptr += 4;
    
    int taxid_array_size = int((idx_file_len - data_start)/sizeof(CSeqDBTaxId));
    
    if (taxid_array_size != m_AllTaxidCount) {
        ERR_POST_X(1, "SeqDB: Taxid metadata indicates (" << m_AllTaxidCount
                   << ") entries but file has room for (" << taxid_array_size
                   << ").");
        
        if (taxid_array_size < m_AllTaxidCount) {
            m_AllTaxidCount = taxid_array_size;
        }
    }
    
    m_TaxData = (CSeqDBTaxId*)
        m_Atlas.GetRegion(m_IndexFN, data_start, idx_file_len, locked);
    
    m_Atlas.RetRegion(lease);
}

CSeqDBTaxInfo::~CSeqDBTaxInfo()
{
    if (! m_Lease.Empty()) {
        m_Atlas.RetRegion(m_Lease);
    }
    if (m_TaxData != 0) {
        m_Atlas.RetRegion((const char*) m_TaxData);
        m_TaxData = 0;
    }
}

bool CSeqDBTaxInfo::GetTaxNames(Int4             tax_id,
                                SSeqDBTaxInfo  & info,
                                CSeqDBLockHold & locked)
{
    Int4 low_index  = 0;
    Int4 high_index = m_AllTaxidCount - 1;
    
    Int4 low_taxid  = m_TaxData[low_index ].GetTaxId();
    Int4 high_taxid = m_TaxData[high_index].GetTaxId();
    
    if((tax_id < low_taxid) || (tax_id > high_taxid))
        return false;
    
    Int4 new_index =  (low_index+high_index)/2;
    Int4 old_index = new_index;
    
    while(1) {
        Int4 curr_taxid = m_TaxData[new_index].GetTaxId();
        
        if (tax_id < curr_taxid) {
            high_index = new_index;
        } else if (tax_id > curr_taxid){
            low_index = new_index;
        } else { /* Got it ! */
            break;
        }
        
        new_index = (low_index+high_index)/2;
        if (new_index == old_index) {
            if (tax_id > curr_taxid) {
                new_index++;
            }
            break;
        }
        old_index = new_index;
    }
    
    if (tax_id == m_TaxData[new_index].GetTaxId()) {
        info.taxid = tax_id;
        
        m_Atlas.Lock(locked);
        
        Uint4 begin_data(m_TaxData[new_index].GetOffset());
        Uint4 end_data(0);
        
        if (new_index == high_index) {
            // Last index is special...
            CSeqDBAtlas::TIndx fsize(0);
            
            if (! m_Atlas.GetFileSizeL(m_DataFN, fsize)) {
                // Should not happen.
                NCBI_THROW(CSeqDBException,
                           eFileErr,
                           "Error: Cannot get tax database file length.");
            }
            
            end_data = Uint4(fsize);
            
            if (end_data < begin_data) {
                // Should not happen.
                NCBI_THROW(CSeqDBException,
                           eFileErr,
                           "Error: Offset error at end of taxdb file.");
            }
        } else {
            end_data = (m_TaxData[new_index+1].GetOffset());
        }
        
        if (! m_Lease.Contains(begin_data, end_data)) {
            m_Atlas.GetRegion(m_Lease, m_DataFN, begin_data, end_data);
        }
        
        const char * start_ptr = m_Lease.GetPtr(begin_data);
        
        CSeqDB_Substring buffer(start_ptr, start_ptr + (end_data - begin_data));
        CSeqDB_Substring sci, com, blast, king;
        bool rc1, rc2, rc3;
        
        rc1 = SeqDB_SplitString(buffer, sci, '\t');
        rc2 = SeqDB_SplitString(buffer, com, '\t');
        rc3 = SeqDB_SplitString(buffer, blast, '\t');
        king = buffer;
        
        if (rc1 && rc2 && rc3 && buffer.Size()) {
            sci   .GetString(info.scientific_name);
            com   .GetString(info.common_name);
            blast .GetString(info.blast_name);
            king  .GetString(info.s_kingdom);
            
            return true;
        }
    }
    
    return false;
}

END_NCBI_SCOPE

