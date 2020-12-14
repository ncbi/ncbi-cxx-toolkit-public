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
#include <objtools/blast/seqdb_reader/impl/seqdbtax.hpp>

/// Tell the error reporting framework what part of the code we're in.
#define NCBI_USE_ERRCODE_X   Objtools_SeqDBTax

BEGIN_NCBI_SCOPE


/// CSeqDBTaxId class
///
/// This is a memory overlay class.  Do not change the size or layout
/// of this class unless corresponding changes happen to the taxonomy
/// database file format.  This class's constructor and destructor are
/// not called; instead, a pointer to mapped memory is cast to a
/// pointer to this type, and the access methods are used to examine
/// the fields.

class CSeqDBTaxId {
public:
    /// Constructor
    ///
    /// This class is a read-only memory overlay and is not expected
    /// to ever be constructed.
    CSeqDBTaxId()
    {
        _ASSERT(0);
    }

    /// Return the taxonomic identifier field (in host order)
    TTaxId GetTaxId()const
    {
        return TAX_ID_FROM(Int4, SeqDB_GetStdOrd(& m_Taxid));
    }

    /// Return the offset field (in host order)
    Int4 GetOffset() const
    {
        return SeqDB_GetStdOrd(& m_Offset);
    }

private:
    /// This structure should not be copy constructed
    CSeqDBTaxId(const CSeqDBTaxId &);

    /// The taxonomic identifier
    Uint4 m_Taxid;

    /// The offset of the start of the taxonomy data.
    Uint4 m_Offset;
};



class CTaxDBFileInfo
{
public:
	CTaxDBFileInfo();
    ~CTaxDBFileInfo();

    const char* GetDataPtr() {return m_DataPtr;}
    const CSeqDBTaxId* GetIndexPtr() {return m_IndexPtr;}
    bool IsMissingTaxInfo() {return m_MissingDB;}
    const Int4 GetTaxidCount() { return m_AllTaxidCount;}
    size_t GetDataFileSize() { return m_DataFileSize;}

private:
    /// The filename of the taxonomic db index file
    string m_IndexFN;

    /// The filename of the taxnomoic db data file
    string m_DataFN;

    /// Total number of taxids in the database
    Int4 m_AllTaxidCount;

    /// Memory map of the index file
    unique_ptr<CMemoryFile> m_IndexFileMap;
    unique_ptr<CMemoryFile> m_DataFileMap;
    CSeqDBTaxId* m_IndexPtr;
    char * m_DataPtr;

    size_t m_DataFileSize;


    /// Indicator if tax db files are missing
    bool m_MissingDB;
};



CTaxDBFileInfo::CTaxDBFileInfo()
    : m_AllTaxidCount(0),
      m_IndexPtr(NULL),
      m_DataPtr(NULL),
      m_DataFileSize(0),
      m_MissingDB(false)
{

    // It is reasonable for this database to not exist.
    m_IndexFN =  SeqDB_ResolveDbPath("taxdb.bti");

    if (m_IndexFN.size()) {
        m_DataFN = m_IndexFN;
        m_DataFN[m_DataFN.size()-1] = 'd';
    }
    
    if (! (m_IndexFN.size() &&
           m_DataFN.size()  &&
           CFile(m_IndexFN).Exists() &&
           CFile(m_DataFN).Exists())) {
        m_MissingDB = true;
        return;
    }
    
    // Size for header data plus one taxid object.
    
    Uint4 data_start = (4 +    // magic
                        4 +    // taxid count
                        16);   // 4 reserved fields
    
    Uint4 idx_file_len = (Uint4) CFile(m_IndexFN).GetLength();
    
    if (idx_file_len < (data_start + sizeof(CSeqDBTaxId))) {
        m_MissingDB = true;
        return;
    }
    
    m_IndexFileMap.reset(new CMemoryFile(m_IndexFN));
    
    m_IndexFileMap->Map();

    // Last check-up of the database validity
    
    
    Uint4 * magic_num_ptr = (Uint4 *)m_IndexFileMap->GetPtr();
    
    const unsigned TAX_DB_MAGIC_NUMBER = 0x8739;
    
    if (TAX_DB_MAGIC_NUMBER != SeqDB_GetStdOrd(magic_num_ptr ++)) {
        m_MissingDB = true;
        m_IndexFileMap.reset();
        ERR_POST("Error: Tax database file has wrong magic number.");
        return;
    }
    
    m_AllTaxidCount = SeqDB_GetStdOrd(magic_num_ptr ++);
    
    // Skip the four reserved fields
    magic_num_ptr += 4;
    
    int taxid_array_size = int((idx_file_len - data_start)/sizeof(CSeqDBTaxId));
    
    if (taxid_array_size != m_AllTaxidCount) {
        m_MissingDB = true;
        m_IndexFileMap.reset();
        ERR_POST("SeqDB: Taxid metadata indicates (" << m_AllTaxidCount
                   << ") entries but file has room for (" << taxid_array_size
                   << ").");
        
        if (taxid_array_size < m_AllTaxidCount) {
            m_AllTaxidCount = taxid_array_size;
        }
        return;
    }
    
    m_DataFileMap.reset(new CMemoryFile(m_DataFN));

    m_DataPtr = (char *) (m_DataFileMap->GetPtr());
    m_DataFileSize = m_DataFileMap->GetSize();
    m_IndexPtr = (CSeqDBTaxId*) magic_num_ptr;
    
}

CTaxDBFileInfo::~CTaxDBFileInfo()
{
    if (!m_MissingDB) {
	m_IndexFileMap->Unmap();
	m_IndexFileMap.reset();
	m_DataFileMap->Unmap();
	m_DataFileMap.reset();
    }
}


bool CSeqDBTaxInfo::GetTaxNames(TTaxId           tax_id,
                                SSeqDBTaxInfo  & info )
{
	static CTaxDBFileInfo t;
    if (t.IsMissingTaxInfo()) return false;

    Int4 low_index  = 0;
    Int4 high_index = t.GetTaxidCount() - 1;
    
    const char * Data = t.GetDataPtr();
    const CSeqDBTaxId*  Index = t.GetIndexPtr();
    TTaxId low_taxid  = Index[low_index ].GetTaxId();
    TTaxId high_taxid = Index[high_index].GetTaxId();

    if((tax_id < low_taxid) || (tax_id > high_taxid))
        return false;
    
    Int4 new_index =  (low_index+high_index)/2;
    Int4 old_index = new_index;
    
    while(1) {
        TTaxId curr_taxid = Index[new_index].GetTaxId();
        
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
    
    if (tax_id == Index[new_index].GetTaxId()) {
        info.taxid = tax_id;
        
        Uint4 begin_data(Index[new_index].GetOffset());
        Uint4 end_data(0);
        
        if (new_index == high_index) {
            // Last index is special...
            end_data = Uint4(t.GetDataFileSize());
            
            if (end_data < begin_data) {
                // Should not happen.
                ERR_POST( "Error: Offset error at end of taxdb file.");
                return false;
            }
        } else {
            end_data = (Index[new_index+1].GetOffset());
        }
        
        const char * start_ptr = &Data[begin_data];
        
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

