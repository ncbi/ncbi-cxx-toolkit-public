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

/// @file seqdb.cpp
/// Implementation for the CSeqDB class, the top level class for SeqDB.
#include <ncbi_pch.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include "seqdbimpl.hpp"
#include <objtools/blast/seqdb_reader/impl/seqdbgeneral.hpp>
#include <map>
#include <string>

#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/objistrasnb.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>

BEGIN_NCBI_SCOPE

const char* CSeqDB::kOidNotFound = "OID not found";

/// Helper function to translate enumerated type to character.
///
/// @param seqtype
///   The sequence type (eProtein, eNucleotide, or eUnknown).
/// @return
///   The sequence type as a char ('p', 'n', or '-').

static char s_GetSeqTypeChar(CSeqDB::ESeqType seqtype)
{
    switch(seqtype) {
    case CSeqDB::eProtein:
        return 'p';
    case CSeqDB::eNucleotide:
        return 'n';
    case CSeqDB::eUnknown:
        return '-';
    }

    NCBI_THROW(CSeqDBException,
               eArgErr,
               "Invalid sequence type specified.");
}

/// Helper function to build private implementation object.
///
/// This method builds and returns the object which implements the
/// functionality for the CSeqDB API.  If this method is called with
/// '-' for the sequence data type, protein will be tried first, then
/// nucleotide.  The created object will be returned.  Either
/// kSeqTypeProt for a protein database, kSeqTypeNucl for nucleotide,
/// or kSeqTypeUnkn to less this function try one then the other.
///
/// @param dbname
///   A list of database or alias names, seperated by spaces.
/// @param prot_nucl
///   Specify whether to use protein, nucleotide, or either.
/// @param oid_begin
///   Iterator will skip OIDs less than this value.  Only OIDs
///   found in the OID lists (if any) will be returned.
/// @param oid_end
///   Iterator will return up to (but not including) this OID.
/// @param use_mmap
///   If kSeqDBMMap is specified (the default), memory mapping is
///   attempted.  If kSeqDBNoMMap is specified, or memory mapping
///   fails, this platform does not support it, the less efficient
///   read and write calls are used instead.
/// @param gi_list
///   This ID list specifies OIDs and deflines to include.
/// @param neg_list
///   This negative ID list specifies deflines and OIDs to exclude.
/// @param idset
///   If set, this specifies IDs to either include or exclude.
/// @return
///   The CSeqDBImpl object that was created.

static CSeqDBImpl *
s_SeqDBInit(const string       & dbname,
            char                 prot_nucl,
            int                  oid_begin,
            int                  oid_end,
            bool                 use_atlas_lock,
            CSeqDBGiList       * gi_list = NULL,
            CSeqDBNegativeList * neg_list = NULL,
            CSeqDBIdSet          idset = CSeqDBIdSet())
{
    CSeqDBImpl * impl = 0;

    if (prot_nucl == '-') {
        try {
            prot_nucl = 'p';
            impl = new CSeqDBImpl(dbname,
                                  prot_nucl,
                                  oid_begin,
                                  oid_end,
                                  gi_list,
                                  neg_list,
                                  idset,
                                  use_atlas_lock);
        }
        catch(CSeqDBException &) {
            prot_nucl = 'n';
        }
    }

    if (! impl) {
        impl = new CSeqDBImpl(dbname,
                              prot_nucl,
                              oid_begin,
                              oid_end,
                              gi_list,
                              neg_list,
                              idset,
                              use_atlas_lock);
    }

    _ASSERT(impl);

    return impl;
}

CSeqDB::CSeqDB(const string & dbname,
               ESeqType       seqtype,
               CSeqDBGiList * gi_list,
               bool           use_atlas_lock)

{
    if (dbname.size() == 0) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Database name is required.");
    }

    char seq_type = s_GetSeqTypeChar(seqtype);

    m_Impl = s_SeqDBInit(dbname,
                         seq_type,
                         0,
                         0,
                         use_atlas_lock,
                         gi_list);

    ////m_Impl->Verify();
}

CSeqDB::CSeqDB(const string       & dbname,
               ESeqType             seqtype,
               CSeqDBNegativeList * nlist)
{
    if (dbname.size() == 0) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Database name is required.");
    }

    const bool kUseAtlasLock = true;
    m_Impl = s_SeqDBInit(dbname,
                         s_GetSeqTypeChar(seqtype),
                         0,
                         0,
                         kUseAtlasLock,
                         NULL,
                         nlist);

    ////m_Impl->Verify();
}

CSeqDB::CSeqDB(const string & dbname,
               ESeqType       seqtype,
               CSeqDBGiList * gi_list,
               CSeqDBNegativeList * nlist)
{
    if (dbname.size() == 0) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Database name is required.");
    }

    char seq_type = s_GetSeqTypeChar(seqtype);

    m_Impl = s_SeqDBInit(dbname,
                         seq_type,
                         0,
                         0,
                         true,
                         gi_list,
                         nlist);

    ////m_Impl->Verify();
}

CSeqDB::CSeqDB(const string & dbname,
               ESeqType       seqtype,
               int            oid_begin,
               int            oid_end,
               CSeqDBGiList * gi_list,
               CSeqDBNegativeList * nlist)
{
    if (dbname.size() == 0) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Database name is required.");
    }

    char seq_type = s_GetSeqTypeChar(seqtype);

    m_Impl = s_SeqDBInit(dbname,
                         seq_type,
                         oid_begin,
                         oid_end,
                         true,
                         gi_list,
                         nlist);

    ////m_Impl->Verify();
}


void CSeqDB::AccessionsToOids(const vector<string>& accs, vector<blastdb::TOid>& oids) const
{
     m_Impl->AccessionsToOids(accs, oids);
}

void CSeqDB::TaxIdsToOids(set<TTaxId>& tax_ids, vector<blastdb::TOid>& rv) const
{
     m_Impl->TaxIdsToOids(tax_ids, rv);
}

void CSeqDB::GetDBTaxIds(set<TTaxId> & tax_ids) const
{
     m_Impl->GetDBTaxIds(tax_ids);
}

void CSeqDB::GetTaxIdsForOids(const vector<blastdb::TOid> & oids, set<TTaxId> & tax_ids) const
{
	m_Impl->GetTaxIdsForOids(oids, tax_ids);
}

// This could become the primary constructor for SeqDB, and those
// taking positive and negative lists could be deprecated.  This
// implies refactoring of code using SeqDB, addition of the third
// (string/Seq-id) type IDs to the IdSet, and changes to client code.
// Some non-SeqDB code uses FindOID and other methods of the GI list,
// comparable functionality would need to be added to IdSet().
//
// Before any of that is done, all the SeqDB classes should be made to
// use CSeqDBIdSet instead of using positive and negative lists.  This
// implies widespread changes to CSeqDBIdSet and SeqDB internal code.
//
// I'll leave those changes for another time -- for now I'll just add
// the pieces of framework that seem useful and are implied by the
// current design.

CSeqDB::CSeqDB(const string & dbname, ESeqType seqtype, CSeqDBIdSet ids)
{
    if (dbname.size() == 0) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Database name is required.");
    }

    CRef<CSeqDBNegativeList> neg;
    CRef<CSeqDBGiList> pos;

    if (! ids.Blank()) {
        if (ids.IsPositive()) {
            pos = ids.GetPositiveList();
        } else {
            neg = ids.GetNegativeList();
        }
    }

    const bool kUseAtlasLock = true;
    m_Impl = s_SeqDBInit(dbname,
                         s_GetSeqTypeChar(seqtype),
                         0,
                         0,
                         kUseAtlasLock,
                         pos.GetPointerOrNull(),
                         neg.GetPointerOrNull(),
                         ids);

    ////m_Impl->Verify();
}

CSeqDB::CSeqDB(const vector<string> & dbs,
               ESeqType               seqtype,
               CSeqDBGiList         * gi_list)
{
    string dbname;
    SeqDB_CombineAndQuote(dbs, dbname);

    if (dbname.size() == 0) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Database name is required.");
    }

    const bool kUseAtlasLock = true;
    m_Impl = s_SeqDBInit(dbname,
                         s_GetSeqTypeChar(seqtype),
                         0,
                         0,
                         kUseAtlasLock,
                         gi_list);

    ////m_Impl->Verify();
}

CSeqDB::CSeqDB(const string & dbname,
               ESeqType       seqtype,
               int            oid_begin,
               int            oid_end,
               bool           use_mmap,
               CSeqDBGiList * gi_list)
{
    if (dbname.size() == 0) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Database name is required.");
    }

    const bool kUseAtlasLock = true;
    m_Impl = s_SeqDBInit(dbname,
                         s_GetSeqTypeChar(seqtype),
                         oid_begin,
                         oid_end,
                         kUseAtlasLock,
                         gi_list);

    ////m_Impl->Verify();
}

CSeqDB::CSeqDB(const vector<string> & dbs,
               ESeqType               seqtype,
               int                    oid_begin,
               int                    oid_end,
               bool                   use_mmap,
               CSeqDBGiList         * gi_list)
{
    string dbname;
    SeqDB_CombineAndQuote(dbs, dbname);

    if (dbname.size() == 0) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Database name is required.");
    }

    const bool kUseAtlasLock = true;
    m_Impl = s_SeqDBInit(dbname,
                         s_GetSeqTypeChar(seqtype),
                         oid_begin,
                         oid_end,
                         kUseAtlasLock,
                         gi_list);

    ////m_Impl->Verify();
}

CSeqDB::CSeqDB()
{
    m_Impl = new CSeqDBImpl();
    ////m_Impl->Verify();
}

int CSeqDB::GetSeqLength(int oid) const
{
    ////m_Impl->Verify();
    int length = m_Impl->GetSeqLength(oid);
    ////m_Impl->Verify();

    return length;
}

int CSeqDB::GetSeqLengthApprox(int oid) const
{
    ////m_Impl->Verify();
    int length = m_Impl->GetSeqLengthApprox(oid);
    ////m_Impl->Verify();

    return length;
}

CRef<CBlast_def_line_set> CSeqDB::GetHdr(int oid) const
{
    ////m_Impl->Verify();
    CRef<CBlast_def_line_set> rv = m_Impl->GetHdr(oid);
    ////m_Impl->Verify();

    return rv;
}

CSeqDB::ESeqType CSeqDB::GetSequenceType() const
{
    switch(m_Impl->GetSeqType()) {
    case 'p':
        return eProtein;
    case 'n':
        return eNucleotide;
    }

    NCBI_THROW(CSeqDBException,
               eArgErr,
               "Internal sequence type is not valid.");
}

void CSeqDB::GetTaxIDs(int             oid,
                       map<TGi, TTaxId> & gi_to_taxid,
                       bool            persist) const
{
    ////m_Impl->Verify();
    typedef map<TGi, TTaxId> TmpMap;
    TmpMap gi_to_taxid_tmp;
    m_Impl->GetTaxIDs(oid, gi_to_taxid_tmp, persist);
    if ( !persist ) {
        gi_to_taxid.clear();
    }
    ITERATE ( TmpMap, it, gi_to_taxid_tmp ) {
        gi_to_taxid[it->first] = it->second;
    }
    ////m_Impl->Verify();
}

void CSeqDB::GetTaxIDs(int           oid,
                       vector<TTaxId> & taxids,
                       bool          persist) const
{
    ////m_Impl->Verify();
    m_Impl->GetTaxIDs(oid, taxids, persist);
    ////m_Impl->Verify();
}

void CSeqDB::GetAllTaxIDs(int           oid,
                          set<TTaxId> & taxids) const
{
    m_Impl->GetAllTaxIDs(oid, taxids);
}

void CSeqDB::GetLeafTaxIDs(
        int                  oid,
        map<TGi, set<TTaxId> >& gi_to_taxid_set,
        bool                 persist
) const
{
    ////m_Impl->Verify();
    typedef map<TGi, set<TTaxId> > TmpMap;
    TmpMap gi_to_taxid_set_tmp;
    m_Impl->GetLeafTaxIDs(oid, gi_to_taxid_set_tmp, persist);
    if ( !persist ) {
        gi_to_taxid_set.clear();
    }
    ITERATE ( TmpMap, it, gi_to_taxid_set_tmp ) {
        gi_to_taxid_set[it->first] = it->second;
    }
    //m_Impl->Verify();
}

void CSeqDB::GetLeafTaxIDs(
        int          oid,
        vector<TTaxId>& taxids,
        bool         persist
) const
{
    //m_Impl->Verify();
    m_Impl->GetLeafTaxIDs(oid, taxids, persist);
    //m_Impl->Verify();
}

CRef<CBioseq>
CSeqDB::GetBioseq(int oid, TGi target_gi, const CSeq_id * target_id) const
{
    //m_Impl->Verify();
    CRef<CBioseq> rv = m_Impl->GetBioseq(oid, target_gi, target_id, true);
    //m_Impl->Verify();

    return rv;
}

CRef<CBioseq>
CSeqDB::GetBioseqNoData(int oid, TGi target_gi, const CSeq_id * target_id) const
{
    //m_Impl->Verify();
    CRef<CBioseq> rv = m_Impl->GetBioseq(oid, target_gi, target_id, false);
    //m_Impl->Verify();

    return rv;
}

void CSeqDB::RetSequence(const char ** buffer) const
{
    //m_Impl->Verify();
    m_Impl->RetSequence(buffer);
    //m_Impl->Verify();
}

int CSeqDB::GetSequence(int oid, const char ** buffer) const
{
    //m_Impl->Verify();
    int rv = m_Impl->GetSequence(oid, buffer);
    //m_Impl->Verify();

    return rv;
}

CRef<CSeq_data> CSeqDB::GetSeqData(int     oid,
                                   TSeqPos begin,
                                   TSeqPos end) const
{
    //m_Impl->Verify();
    CRef<CSeq_data> rv = m_Impl->GetSeqData(oid, begin, end);
    //m_Impl->Verify();

    return rv;
}

int CSeqDB::GetAmbigSeq(int oid, const char ** buffer, int nucl_code) const
{
    //m_Impl->Verify();
    int rv = m_Impl->GetAmbigSeq(oid,
                                 (char **)buffer,
                                 nucl_code,
                                 0,
                                 (ESeqDBAllocType) 0);
    //m_Impl->Verify();

    return rv;
}

void CSeqDB::RetAmbigSeq(const char ** buffer) const
{
    //m_Impl->Verify();
    m_Impl->RetAmbigSeq(buffer);
    //m_Impl->Verify();
}

int CSeqDB::GetAmbigSeq(int           oid,
                        const char ** buffer,
                        int           nucl_code,
                        int           begin_offset,
                        int           end_offset) const
{
    //m_Impl->Verify();

    SSeqDBSlice region(begin_offset, end_offset);

    int rv = m_Impl->GetAmbigSeq(oid,
                                 (char **)buffer,
                                 nucl_code,
                                 & region,
                                 (ESeqDBAllocType) 0);

    //m_Impl->Verify();

    return rv;
}

int CSeqDB::GetAmbigSeqAlloc(int             oid,
                             char         ** buffer,
                             int             nucl_code,
                             ESeqDBAllocType strategy,
                             TSequenceRanges *masks) const
{
    //m_Impl->Verify();

    if ((strategy != eMalloc) && (strategy != eNew)) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Invalid allocation strategy specified.");
    }

    int rv = m_Impl->GetAmbigSeq(oid, buffer, nucl_code, 0, strategy, masks);

    //m_Impl->Verify();

    return rv;
}

int CSeqDB::GetAmbigPartialSeq(int                oid,
                               char            ** buffer,
                               int                nucl_code,
                               ESeqDBAllocType    strategy,
                               TSequenceRanges  * partial_ranges,
                               TSequenceRanges  * masks) const
{

	if ((strategy != eMalloc) && (strategy != eNew)) {
	        NCBI_THROW(CSeqDBException,
	                   eArgErr,
	                   "Invalid allocation strategy specified.");
	    }

    int rv = m_Impl->GetAmbigPartialSeq(oid, buffer, nucl_code, strategy, partial_ranges, masks);
	return rv;
}

string CSeqDB::GetTitle() const
{
    return m_Impl->GetTitle();
}

string CSeqDB::GetDate() const
{
    return m_Impl->GetDate();
}

CTime
CSeqDB::GetDate(const string   & dbname,
                ESeqType         seqtype)
{
    vector<string> vols;
    CSeqDB::FindVolumePaths(dbname, seqtype, vols);
    CTime retv;
    char date[128];
    ITERATE(vector<string>, vol, vols) {
        string fn = *vol + ((seqtype == CSeqDB::eProtein)? ".pin" : ".nin");
        ifstream f(fn.c_str(), ios::in|ios::binary);
        char s[4];   // size of next chunk
        if (f.is_open()) {
            f.seekg(8, ios::beg);
            f.read(s, 4);
            Uint4 offset = SeqDB_GetStdOrd((Uint4 *) s);
            f.seekg(offset, ios::cur);
            f.read(s, 4);
            offset = SeqDB_GetStdOrd((Uint4 *) s);
            f.read(date, offset);
            CTime d(string(date), kBlastDbDateFormat);
            if (retv.IsEmpty() || d > retv) {
                retv = d;
            }
        }
    }
    return retv;
}

int CSeqDB::GetNumSeqs() const
{
    return m_Impl->GetNumSeqs();
}

int CSeqDB::GetNumSeqsStats() const
{
    return m_Impl->GetNumSeqsStats();
}

int CSeqDB::GetNumOIDs() const
{
    return m_Impl->GetNumOIDs();
}

Uint8 CSeqDB::GetTotalLength() const
{
    return m_Impl->GetTotalLength();
}

Uint8 CSeqDB::GetExactTotalLength()
{
    return m_Impl->GetExactTotalLength();
}

Uint8 CSeqDB::GetTotalLengthStats() const
{
    return m_Impl->GetTotalLengthStats();
}

Uint8 CSeqDB::GetVolumeLength() const
{
    return m_Impl->GetVolumeLength();
}

int CSeqDB::GetMaxLength() const
{
    return m_Impl->GetMaxLength();
}

int CSeqDB::GetMinLength() const
{
    return m_Impl->GetMinLength();
}

CSeqDB::~CSeqDB()
{
    ////m_Impl->Verify();

    if (m_Impl)
        delete m_Impl;
}

CSeqDBIter CSeqDB::Begin() const
{
    return CSeqDBIter(this, 0);
}

bool CSeqDB::CheckOrFindOID(int & oid) const
{
    ////m_Impl->Verify();
    bool rv = m_Impl->CheckOrFindOID(oid);
    ////m_Impl->Verify();

    return rv;
}


CSeqDB::EOidListType
CSeqDB::GetNextOIDChunk(int         & begin,
                        int         & end,
                        int         size,
                        vector<int> & lst,
                        int         * state)
{
    ////m_Impl->Verify();

    CSeqDB::EOidListType rv =
        m_Impl->GetNextOIDChunk(begin, end, size, lst, state);

    ////m_Impl->Verify();

    return rv;
}

void CSeqDB::ResetInternalChunkBookmark()
{
    m_Impl->ResetInternalChunkBookmark();
}

const string & CSeqDB::GetDBNameList() const
{
    return m_Impl->GetDBNameList();
}

list< CRef<CSeq_id> > CSeqDB::GetSeqIDs(int oid) const
{
    ////m_Impl->Verify();

    list< CRef<CSeq_id> > rv = m_Impl->GetSeqIDs(oid);

    ////m_Impl->Verify();

    return rv;
}

TGi CSeqDB::GetSeqGI(int oid) const
{
    return m_Impl->GetSeqGI(oid);
}

bool CSeqDB::PigToOid(int pig, int & oid) const
{
    ////m_Impl->Verify();
    bool rv = m_Impl->PigToOid(pig, oid);
    ////m_Impl->Verify();

    return rv;
}

bool CSeqDB::OidToPig(int oid, int & pig) const
{
    ////m_Impl->Verify();
    bool rv = m_Impl->OidToPig(oid, pig);
    ////m_Impl->Verify();

    return rv;
}

bool CSeqDB::TiToOid(Int8 ti, int & oid) const
{
    ////m_Impl->Verify();
    bool rv = m_Impl->TiToOid(ti, oid);
    ////m_Impl->Verify();

    return rv;
}

bool CSeqDB::GiToOid(TGi gi, int & oid) const
{
    ////m_Impl->Verify();
    bool rv = m_Impl->GiToOid(gi, oid);
    ////m_Impl->Verify();

    return rv;
}

bool CSeqDB::GiToOidwFilterCheck(TGi gi, int & oid) const
{
    ////m_Impl->Verify();
    bool rv = m_Impl->GiToOidwFilterCheck(gi, oid);
    ////m_Impl->Verify();

    return rv;
}

bool CSeqDB::OidToGi(int oid, TGi & gi) const
{
    ////m_Impl->Verify();
    TGi gi_tmp;
    bool rv = m_Impl->OidToGi(oid, gi_tmp);
    gi = gi_tmp;
    ////m_Impl->Verify();

    return rv;
}

bool CSeqDB::PigToGi(int pig, TGi & gi) const
{
    ////m_Impl->Verify();
    bool rv = false;

    int oid(0);

    if (m_Impl->PigToOid(pig, oid)) {
        TGi gi_tmp;
        rv = m_Impl->OidToGi(oid, gi_tmp);
        gi = gi_tmp;
    }
    ////m_Impl->Verify();

    return rv;
}

bool CSeqDB::GiToPig(TGi gi, int & pig) const
{
    ////m_Impl->Verify();
    bool rv = false;
    pig = -1;

    int oid(0);

    if (m_Impl->GiToOid(gi, oid)) {
        rv = m_Impl->OidToPig(oid, pig);
    }

    ////m_Impl->Verify();

    return rv;
}

void CSeqDB::AccessionToOids(const string & acc, vector<int> & oids) const
{
    ////m_Impl->Verify();
    m_Impl->AccessionToOids(acc, oids);

    // If we have a numeric ID and the search failed, try to look it
    // up as a GI (but not as a PIG or TI).  Due to the presence of
    // PDB ids like "pdb|1914|a", the faster GitToOid is not done
    // first (unless the caller does so.)

    if (oids.empty()) {
        try {
            TGi gi = NStr::StringToNumeric<TGi>(acc, NStr::fConvErr_NoThrow);
            int oid(-1);

            if (gi > ZERO_GI  &&  m_Impl->GiToOidwFilterCheck(gi, oid)) {
                    oids.push_back(oid);
            }
        }
        catch(...) {
        }
    }

    ////m_Impl->Verify();
}

void CSeqDB::SeqidToOids(const CSeq_id & seqid, vector<int> & oids) const
{
    ////m_Impl->Verify();
    m_Impl->SeqidToOids(seqid, oids, true);
    ////m_Impl->Verify();
}

bool CSeqDB::SeqidToOid(const CSeq_id & seqid, int & oid) const
{
    ////m_Impl->Verify();
    bool rv = false;

    oid = -1;

    vector<int> oids;
    m_Impl->SeqidToOids(seqid, oids, false);

    if (! oids.empty()) {
        rv = true;
        oid = oids[0];
    }

    ////m_Impl->Verify();

    return rv;
}

int CSeqDB::GetOidAtOffset(int first_seq, Uint8 residue) const
{
    ////m_Impl->Verify();
    int rv = m_Impl->GetOidAtOffset(first_seq, residue);
    ////m_Impl->Verify();

    return rv;
}

CSeqDBIter::CSeqDBIter(const CSeqDB * db, int oid)
    : m_DB    (db),
      m_OID   (oid),
      m_Data  (0),
      m_Length((int) -1)
{
    if (m_DB->CheckOrFindOID(m_OID)) {
        x_GetSeq();
    }
}

CSeqDBIter::CSeqDBIter(const CSeqDBIter & other)
    : m_DB    (other.m_DB),
      m_OID   (other.m_OID),
      m_Data  (0),
      m_Length((int) -1)
{
    if (m_DB->CheckOrFindOID(m_OID)) {
        x_GetSeq();
    }
}

/// Copy one iterator to another.
CSeqDBIter & CSeqDBIter::operator =(const CSeqDBIter & other)
{
    x_RetSeq();

    m_DB = other.m_DB;
    m_OID = other.m_OID;
    m_Data = 0;
    m_Length = -1;

    if (m_DB->CheckOrFindOID(m_OID)) {
        x_GetSeq();
    }

    return *this;
}

CSeqDBIter & CSeqDBIter::operator++()
{
    x_RetSeq();

    ++m_OID;

    if (m_DB->CheckOrFindOID(m_OID)) {
        x_GetSeq();
    } else {
        m_Length = -1;
    }

    return *this;
}

CRef<CBioseq>
CSeqDB::GiToBioseq(TGi gi) const
{
    ////m_Impl->Verify();

    CRef<CBioseq> bs;
    int oid(0);

    if (m_Impl->GiToOid(gi, oid)) {
        bs = m_Impl->GetBioseq(oid, gi, NULL, true);
    }

    ////m_Impl->Verify();

    return bs;
}

CRef<CBioseq>
CSeqDB::PigToBioseq(int pig) const
{
    ////m_Impl->Verify();

    int oid(0);
    CRef<CBioseq> bs;

    if (m_Impl->PigToOid(pig, oid)) {
        bs = m_Impl->GetBioseq(oid, ZERO_GI, NULL, true);
    }

    ////m_Impl->Verify();

    return bs;
}

CRef<CBioseq>
CSeqDB::SeqidToBioseq(const CSeq_id & seqid) const
{
    ////m_Impl->Verify();

    vector<int> oids;
    CRef<CBioseq> bs;

    m_Impl->SeqidToOids(seqid, oids, false);

    if (! oids.empty()) {
        bs = m_Impl->GetBioseq(oids[0], ZERO_GI, &seqid, true);
    }

    ////m_Impl->Verify();

    return bs;
}

void
CSeqDB::FindVolumePaths(const string   & dbname,
                        ESeqType         seqtype,
                        vector<string> & paths,
                        vector<string> * alias_paths,
                        bool             recursive,
                        bool             expand_links)
{
    if (seqtype == CSeqDB::eProtein) {
        CSeqDBImpl::FindVolumePaths(dbname, 'p', paths, alias_paths, recursive, expand_links);
    } else if (seqtype == CSeqDB::eNucleotide) {
        CSeqDBImpl::FindVolumePaths(dbname, 'n', paths, alias_paths, recursive, expand_links);
    } else {
        try {
            CSeqDBImpl::FindVolumePaths(dbname, 'p', paths, alias_paths, recursive, expand_links);
        }
        catch(...) {
            CSeqDBImpl::FindVolumePaths(dbname, 'n', paths, alias_paths, recursive, expand_links);
        }
    }
}

void
CSeqDB::FindVolumePaths(vector<string> & paths, bool recursive) const
{
    ////m_Impl->Verify();
    m_Impl->FindVolumePaths(paths, recursive);
    ////m_Impl->Verify();
}

void
CSeqDB::GetGis(int oid, vector<TGi> & gis, bool append) const
{
    ////m_Impl->Verify();

    // This could be done a little faster at a lower level, but not
    // necessarily by too much.  If this operation is important to
    // performance, that decision can be revisited.

    list< CRef<CSeq_id> > seqids = GetSeqIDs(oid);

    if (! append) {
        gis.clear();
    }

    ITERATE(list< CRef<CSeq_id> >, seqid, seqids) {
        if ((**seqid).IsGi()) {
            gis.push_back((**seqid).GetGi());
        }
    }

    ////m_Impl->Verify();
}

void CSeqDB::SetIterationRange(int oid_begin, int oid_end)
{
    m_Impl->SetIterationRange(oid_begin, oid_end);
}

void CSeqDB::GetAliasFileValues(TAliasFileValues & afv)
{
    ////m_Impl->Verify();
    m_Impl->GetAliasFileValues(afv);
    ////m_Impl->Verify();
}

void CSeqDB::GetTaxInfo(TTaxId taxid, SSeqDBTaxInfo & info)
{
    CSeqDBImpl::GetTaxInfo(taxid, info);
}

void CSeqDB::GetTotals(ESummaryType   sumtype,
                       int          * oid_count,
                       Uint8        * total_length,
                       bool           use_approx) const
{
    ////m_Impl->Verify();
    m_Impl->GetTotals(sumtype, oid_count, total_length, use_approx);
    ////m_Impl->Verify();
}

const CSeqDBGiList * CSeqDB::GetGiList() const
{
    return m_Impl->GetGiList();
}

CSeqDBIdSet CSeqDB::GetIdSet() const
{
    return m_Impl->GetIdSet();
}

void CSeqDB::GetSequenceAsString(int      oid,
                                 string & output,
                                 TSeqRange range /* = TSeqRange() */) const
{
    CSeqUtil::ECoding code_to = ((GetSequenceType() == CSeqDB::eProtein)
                                 ? CSeqUtil::e_Iupacaa
                                 : CSeqUtil::e_Iupacna);

    GetSequenceAsString(oid, code_to, output, range);
}

void CSeqDB::GetSequenceAsString(int                 oid,
                                 CSeqUtil::ECoding   coding,
                                 string            & output,
                                 TSeqRange range /* = TSeqRange() */) const
{
    output.erase();

    string raw;
    const char * buffer = 0;
    int length = 0;

    // Protein dbs ignore encodings, always returning ncbistdaa.
    if (range.NotEmpty()) {
        length = GetAmbigSeq(oid, & buffer, kSeqDBNuclNcbiNA8,
                             range.GetFrom(), range.GetToOpen());
    } else {
        length = GetAmbigSeq(oid, & buffer, kSeqDBNuclNcbiNA8);
    }

    try {
        raw.assign(buffer, length);
    }
    catch(...) {
        RetAmbigSeq(& buffer);
        throw;
    }
    RetAmbigSeq(& buffer);

    CSeqUtil::ECoding code_from = ((GetSequenceType() == CSeqDB::eProtein)
                                   ? CSeqUtil::e_Ncbistdaa
                                   : CSeqUtil::e_Ncbi8na);

    string result;

    if (code_from == coding) {
        result.swap(raw);
    } else {
        CSeqConvert::Convert(raw,
                             code_from,
                             0,
                             length,
                             result,
                             coding);
    }

    output.swap(result);
}

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
void CSeqDB::ListColumns(vector<string> & titles)
{
    m_Impl->ListColumns(titles);
}

int CSeqDB::GetColumnId(const string & title)
{
    return m_Impl->GetColumnId(title);
}

const map<string,string> &
CSeqDB::GetColumnMetaData(int column_id)
{
    return m_Impl->GetColumnMetaData(column_id);
}

const string & CSeqDB::GetColumnValue(int column_id, const string & key)
{
    static string mt;
    return SeqDB_MapFind(GetColumnMetaData(column_id), key, mt);
}

const map<string,string> &
CSeqDB::GetColumnMetaData(int            column_id,
                          const string & volname)
{
    return m_Impl->GetColumnMetaData(column_id, volname);
}

void CSeqDB::GetColumnBlob(int            col_id,
                           int            oid,
                           CBlastDbBlob & blob)
{
    m_Impl->GetColumnBlob(col_id, oid, true, blob);
}

void CSeqDB::GetAvailableMaskAlgorithms(vector<int> & algorithms)
{
    m_Impl->GetAvailableMaskAlgorithms(algorithms);
}

int CSeqDB::GetMaskAlgorithmId(const string &algo_name) const
{
    return m_Impl->GetMaskAlgorithmId(algo_name);
}

string CSeqDB::GetAvailableMaskAlgorithmDescriptions()
{
    return m_Impl->GetAvailableMaskAlgorithmDescriptions();
}

vector<int> CSeqDB::ValidateMaskAlgorithms(const vector<int>& algorithm_ids)
{
    vector<int> invalid_algo_ids, available_algo_ids;
    GetAvailableMaskAlgorithms(available_algo_ids);
    invalid_algo_ids.reserve(algorithm_ids.size());
    if (available_algo_ids.empty()) {
        copy(algorithm_ids.begin(), algorithm_ids.end(),
             back_inserter(invalid_algo_ids));
        return invalid_algo_ids;
    }

    ITERATE(vector<int>, itr, algorithm_ids) {
        vector<int>::const_iterator pos = find(available_algo_ids.begin(),
                                               available_algo_ids.end(), *itr);
        if (pos == available_algo_ids.end()) {
            invalid_algo_ids.push_back(*itr);
        }
    }
    return invalid_algo_ids;
}

void CSeqDB::GetMaskAlgorithmDetails(int                 algorithm_id,
                                     objects::EBlast_filter_program & program,
                                     string            & program_name,
                                     string            & algo_opts)
{
    string sid;
    m_Impl->GetMaskAlgorithmDetails(algorithm_id, sid, program_name,
                                    algo_opts);
    Int4 id(0);
    NStr::StringToNumeric(sid, &id, NStr::fConvErr_NoThrow, 10);
    program = (objects::EBlast_filter_program)id;
}

void CSeqDB::GetMaskAlgorithmDetails(int                 algorithm_id,
                                     string            & program,
                                     string            & program_name,
                                     string            & algo_opts)
{
    m_Impl->GetMaskAlgorithmDetails(algorithm_id, program, program_name,
                                    algo_opts);
}

void CSeqDB::GetMaskData(int                 oid,
                         int                 algo_id,
                         TSequenceRanges   & ranges)
{
    m_Impl->GetMaskData(oid, algo_id, ranges);
}

#endif


void CSeqDB::SetOffsetRanges(int                        oid,
                             const CSeqDB::TRangeList & offset_ranges,
                             bool                       append_ranges,
                             bool                       cache_data)
{
    ////m_Impl->Verify();

    m_Impl->SetOffsetRanges(oid,
                            offset_ranges,
                            append_ranges,
                            cache_data);

    ////m_Impl->Verify();
}

void CSeqDB::RemoveOffsetRanges(int oid)
{
    static TRangeList empty;
    SetOffsetRanges(oid, empty, false, false);
}

void CSeqDB::FlushOffsetRangeCache()
{
    m_Impl->FlushOffsetRangeCache();
}

void CSeqDB::SetNumberOfThreads(int num_threads, bool force_mt)
{
    ////m_Impl->Verify();

    m_Impl->SetNumberOfThreads(num_threads, force_mt);
}

string CSeqDB::ESeqType2String(ESeqType type)
{
    string retval("Unknown");
    switch (type) {
    case eProtein: retval.assign("Protein"); break;
    case eNucleotide: retval.assign("Nucleotide"); break;
    case eUnknown:
    default: break;
    }
    return retval;
}

string CSeqDB::GenerateSearchPath()
{
    return CSeqDBAtlas::GenerateSearchPath();
}

void CSeqDB::SetVolsMemBit(int mbit)
{
    m_Impl->SetVolsMemBit(mbit);
}

/// Functor class for FindFilesInDir
class CBlastDbFinder {
public:
    void operator() (CDirEntry& de) {
        const string& extn = de.GetPath().substr(de.GetPath().length() - 3, 1);
        SSeqDBInitInfo value;
        // rm extension
        value.m_BlastDbName = de.GetPath().substr(0, de.GetPath().length() - 4);
        CNcbiOstrstream oss;
        // Needed for escaping spaces
        oss << "\"" << value.m_BlastDbName << "\"";
        value.m_BlastDbName = CNcbiOstrstreamToString(oss);
        value.m_MoleculeType =
            (extn == "n" ? CSeqDB::eNucleotide : CSeqDB::eProtein);
        m_DBs.push_back(value);
    }

    vector<SSeqDBInitInfo> m_DBs;

    /// Auxiliary function to get the original file name found by this object
    string GetFileName(size_t idx) {
        SSeqDBInitInfo& info = m_DBs[idx];
        string retval = NStr::Replace(info.m_BlastDbName, "\"", kEmptyStr);
        if (info.m_MoleculeType == CSeqDB::eNucleotide) {
            string alias = retval + ".nal", index = retval + ".nin";
            retval = (CFile(alias).Exists() ? alias : index);
        } else {
            string alias = retval + ".pal", index = retval + ".pin";
            retval = (CFile(alias).Exists() ? alias : index);
        }
        return retval;
    }
};

/** Functor object for s_RemoveAliasComponents where the path name is matched
 * in SSeqDBInitInfo */
class PathFinder {
public:
    PathFinder(const string& p) : m_Path(p) {}
    bool operator() (const SSeqDBInitInfo& value) const {
        return (NStr::Find(value.m_BlastDbName, m_Path) != NPOS);
    }

private:
    string m_Path;
};

static void s_RemoveAliasComponents(CBlastDbFinder& finder)
{
    set<string> dbs2remove;
    for (size_t i = 0; i < finder.m_DBs.size(); i++) {
        string path = finder.GetFileName(i);
        if (path[path.size()-1] != 'l') { // not an alias file
            continue;
        }
        CNcbiIfstream in(path.c_str());
        if (!in) {
            continue;
        }
        string line;
        while (getline(in, line)) {
            if (NStr::StartsWith(line, "DBLIST")) {
                vector<string> tokens;
                NStr::Split(line, " ", tokens, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
                for (size_t j = 1; j < tokens.size(); j++) {
                    dbs2remove.insert(tokens[j]);
                }
            }
        }
    }

    ITERATE(set<string>, i, dbs2remove) {
        finder.m_DBs.erase(remove_if(finder.m_DBs.begin(), finder.m_DBs.end(),
                                     PathFinder(*i)),
                           finder.m_DBs.end());
    }
}

vector<SSeqDBInitInfo>
FindBlastDBs(const string& path, const string& dbtype, bool recurse,
             bool include_alias_files /* = false */,
             bool remove_redundant_dbs /* = false */)
{
    // 1. Find every database volume (but not alias files etc).
    vector<string> fmasks, dmasks;

    // If the type is 'guess' we do both types of databases.

    if (dbtype != "nucl") {
        fmasks.push_back("*.pin");
        if (include_alias_files) {
            fmasks.push_back("*.pal");
        }
    }
    if (dbtype != "prot") {
        fmasks.push_back("*.nin");
        if (include_alias_files) {
            fmasks.push_back("*.nal");
        }
    }
    dmasks.push_back("*");

    EFindFiles flags = (EFindFiles)
        (fFF_File | (recurse ? fFF_Recursive : 0));

    CBlastDbFinder dbfinder;
    FindFilesInDir(CDir(path), fmasks, dmasks, dbfinder, flags);
    if (remove_redundant_dbs) {
        s_RemoveAliasComponents(dbfinder);
    }
    sort(dbfinder.m_DBs.begin(), dbfinder.m_DBs.end());
    return dbfinder.m_DBs;
}

Int8 CSeqDB::GetDiskUsage() const
{
    vector<string> paths;
    FindVolumePaths(paths);
    _ASSERT( !paths.empty() );

    Int8 retval = 0;

    vector<string> extn;
    const bool is_protein(GetSequenceType() == CSeqDB::eProtein);
    SeqDB_GetFileExtensions(is_protein, extn, GetBlastDbVersion());
    string blastdb_dirname;

    ITERATE(vector<string>, path, paths) {
        ITERATE(vector<string>, ext, extn) {
            CFile file(*path + "." + *ext);
            if (file.Exists()) {
                Int8 length = file.GetLength();
                if (length != -1) {
                    retval += length;
                    LOG_POST(Trace << "File " << file.GetPath() << " " << length << " bytes");
                    blastdb_dirname = file.GetDir();
                } else {
                    ERR_POST(Error << "Error retrieving file size for "
                                   << file.GetPath());
                }
            }
        }
    }
    // For multi-volume databases, take into account files that apply to the
    // entire BLASTDB
    if (paths.size() > 1) {
        _ASSERT( !blastdb_dirname.empty() );
        auto dbname = GetDBNameList();
        vector<string> dblist;
        NStr::Split(dbname, " ", dblist, NStr::fSplit_Tokenize);
        if (dblist.size() > 1) {
            CNcbiOstrstream oss;
            oss << "Cannot compute disk usage for multiple BLASTDBs (i.e.: '"
                << dbname << "') at once. Please try again using one BLASTDB "
                << "at a time.";
            NCBI_THROW(CSeqDBException, eArgErr, CNcbiOstrstreamToString(oss));
        }

        for (const auto& ext: extn) {
            CFile file(CDirEntry::MakePath(blastdb_dirname, dbname, ext));
            if (file.Exists()) {
                Int8 length = file.GetLength();
                if (length != -1) {
                    retval += length;
                    LOG_POST(Trace << "File " << file.GetPath() << " " << length << " bytes");
                } else {
                    ERR_POST(Error << "Error retrieving file size for "
                                   << file.GetPath());
                }
            }
        }
    }
    return retval;
}


CSeqDB::ESeqType
ParseMoleculeTypeString(const string& s)
{
    CSeqDB::ESeqType retval = CSeqDB::eUnknown;
    if (NStr::StartsWith(s, "prot", NStr::eNocase)) {
        retval = CSeqDB::eProtein;
    } else if (NStr::StartsWith(s, "nucl", NStr::eNocase)) {
        retval = CSeqDB::eNucleotide;
    } else if (NStr::StartsWith(s, "guess", NStr::eNocase)) {
        retval = CSeqDB::eUnknown;
    } else {
        _ASSERT("Unknown molecule for BLAST DB" != 0);
    }
    return retval;
}

bool DeleteBlastDb(const string& dbpath, CSeqDB::ESeqType seq_type)
{
    int num_files_removed = 0;
    vector<string> db_files, alias_files;
    bool is_protein = (seq_type == CSeqDB::eProtein);

    vector<string> extn;
    SeqDB_GetFileExtensions( is_protein, extn, eBDB_Version4);
    vector<string> lmdb_extn;
    SeqDB_GetLMDBFileExtensions(is_protein, lmdb_extn);
    ITERATE(vector<string>, lmdb, lmdb_extn) {
    	CNcbiOstrstream oss;
    	oss << dbpath << "." << *lmdb;
        const string fname = CNcbiOstrstreamToString(oss);
        if (CFile(fname).Remove()) {
        	LOG_POST(Info << "Deleted " << fname);
            num_files_removed++;
        }
        else {
        	unsigned int index = 0;
        	string vfname = dbpath + "." + NStr::IntToString(index/10) +
        			        NStr::IntToString(index%10) + "." + *lmdb;
        	while (CFile(vfname).Remove()) {
        		index++;
        		vfname = dbpath + "." + NStr::IntToString(index/10) +
        	    	     NStr::IntToString(index%10) + "." + *lmdb;

        	}
        }
    }

    try { CSeqDB::FindVolumePaths(dbpath, seq_type, db_files, &alias_files); }
    catch (...) {}    // ignore any errors from the invocation above
    ITERATE(vector<string>, f, db_files) {
        ITERATE(vector<string>, e, extn) {
            CNcbiOstrstream oss;
            oss << *f << "." << *e;
            const string fname = CNcbiOstrstreamToString(oss);
            if (CFile(fname).Remove()) {
                LOG_POST(Info << "Deleted " << fname);
                num_files_removed++;
            }
        }
    }
    ITERATE(vector<string>, f, alias_files) {
        if (CFile(*f).Remove()) {
            LOG_POST(Info << "Deleted " << *f);
            num_files_removed++;
        }
    }
    return static_cast<bool>(num_files_removed != 0);
}

const char* CSeqDB::kBlastDbDateFormat = "b d, Y  H:m P";

void CSeqDB::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CSeqDB");
    CObject::DebugDump(ddc, depth);
    ddc.Log("m_Impl", m_Impl, depth);
}

EBlastDbVersion CSeqDB::GetBlastDbVersion() const
{
	 return m_Impl->GetBlastDbVersion();
}


void CSeqDB::x_GetDBFilesMetaData(Int8 & disk_bytes, Int8 & cached_bytes, vector<string> & db_files, const string & user_path) const
{
    vector<string> paths;
    vector<string> alias;
    m_Impl->FindVolumePaths(paths, alias, true);
    _ASSERT( !paths.empty() );

    db_files.clear();
    cached_bytes = 0;
    disk_bytes = 0;

    ITERATE(vector<string>, a, alias) {
   		CFile af(*a);
   		if (af.Exists()) {
   			string fn = user_path + af.GetName();
   			db_files.push_back(fn);
   			Int8 afl = af.GetLength();
   			if (afl != -1) {
   				disk_bytes += afl;
   			} else {
   				ERR_POST(Error << "Error retrieving file size for " << af.GetPath());
   			}
   		}
    }

    vector<string> extn;
    const bool is_protein(GetSequenceType() == CSeqDB::eProtein);
    SeqDB_GetFileExtensions(is_protein, extn, EBlastDbVersion::eBDB_Version4);

    const string kExtnMol(1, is_protein ? 'p' : 'n');
    const string index_ext = kExtnMol + "in";
    const string seq_ext = kExtnMol + "sq";

    ITERATE(vector<string>, path, paths) {
        ITERATE(vector<string>, ext, extn) {
            CFile file(*path + "." + *ext);
            if (file.Exists()) {
            	string f = user_path + file.GetName();
            	db_files.push_back(f);
                Int8 length = file.GetLength();
                if (length != -1) {
                    disk_bytes += length;
                    if((*ext == index_ext) || (*ext == seq_ext)) {
                    	cached_bytes += length;
                    }
                } else {
                    ERR_POST(Error << "Error retrieving file size for "
                                   << file.GetPath());
                }
            }
        }
    }

    if (GetBlastDbVersion() == EBlastDbVersion::eBDB_Version5) {
    	vector<string> lmdb_list;
    	m_Impl->GetLMDBFileNames(lmdb_list);

     	ITERATE(vector<string>, l, lmdb_list) {
     		CFile file(*l);
     		if (file.Exists()) {
     		   string f = user_path + file.GetName();
      	 	   db_files.push_back(f);
      	 	   Int8 length = file.GetLength();
       	   	   if (length != -1) {
       	   		   disk_bytes += length;
       	   	   } else {
       	   		   ERR_POST(Error << "Error retrieving file size for " << file.GetPath());
       	   	   }
       	   	   static const char * v5_exts[]={"os", "ot", "tf", "to", NULL};
       	   	   for(const char ** p=v5_exts; *p != NULL; p++) {
       	    		CFile v(file.GetDir() + file.GetBase() + "." + kExtnMol + (*p));
       	    		if (v.Exists()) {
       	    			string vf = user_path + v.GetName();
       	    			db_files.push_back(vf);
       	    			Int8 vl = v.GetLength();
       	    			if (vl != -1) {
       	    				disk_bytes += vl;
       	    			} else {
       	    				ERR_POST(Error << "Error retrieving file size for " << v.GetPath());
       	    			}
       	    		}
       	   	   }
     	   }
    	}
    }

    // FIXME: increase the version for this new file type
    //string ext;
    //SeqDB_GetMetadataFileExtension(is_protein, ext);
    //const CFile dbfile(paths.front());
    //db_files.push_back(user_path + dbfile.GetBase() + "." + ext);

    sort(db_files.begin(), db_files.end());
}

CRef<CBlast_db_metadata> CSeqDB::GetDBMetaData(string user_path)
{
	CRef<CBlast_db_metadata> m (new CBlast_db_metadata());
	int num_seqs = 0;
	Uint8 total_length = 0;

	GetTotals(CSeqDB::eFilteredAll, &num_seqs, &total_length, true);
	vector<string> dblist;
    NStr::Split(GetDBNameList(), " ", dblist, NStr::fSplit_Tokenize);
    NON_CONST_ITERATE(vector<string>, itr, dblist) {
        size_t off = (*itr).find_last_of(CFile::GetPathSeparator());
        if (off != string::npos ) {
        	(*itr).erase(0, off+1);
        }
    }

	string dbnames = NStr::Join(dblist, " ");
	m->SetDbname(dbnames);

    m->SetDbtype(GetSequenceType() == CSeqDB::eProtein ? "Protein" : "Nucleotide" );
	m->SetDb_version(GetBlastDbVersion() == EBlastDbVersion::eBDB_Version5?5:4);
	m->SetDescription(GetTitle());
	m->SetNumber_of_letters(total_length);
	m->SetNumber_of_sequences(num_seqs);

	CTimeFormat timeFmt = CTimeFormat::GetPredefined(CTimeFormat::eISO8601_DateTimeSec);
	CTime date(GetDate(), kBlastDbDateFormat);
	m->SetLast_updated(date.AsString(timeFmt));

	Int8 disk_bytes(0), cached_bytes(0);
	x_GetDBFilesMetaData(disk_bytes, cached_bytes, m->SetFiles(), user_path);
	m->SetBytes_total(disk_bytes);
	m->SetBytes_to_cache(cached_bytes);

	m->SetNumber_of_volumes(m_Impl->GetNumOfVols());

	if (GetBlastDbVersion() == EBlastDbVersion::eBDB_Version5) {
		set<TTaxId> tax_ids;
		GetDBTaxIds(tax_ids);
		if((tax_ids.size() > 1) || ((tax_ids.size() == 1) && (0 != *tax_ids.begin()))){
			m->SetNumber_of_taxids(static_cast<int>(tax_ids.size()));
		}
	}
	return m;
}

void CSeqDB::GetTaxIdsForAccession(const string & accs, vector<TTaxId> & taxids)
{
	CSeq_id seqid(accs, CSeq_id::fParse_RawText | CSeq_id::fParse_ValidLocal);
	m_Impl->GetTaxIdsForSeqId(seqid, taxids);
}

void CSeqDB::GetTaxIdsForSeqId(const CSeq_id & seq_id, vector<TTaxId> & taxids)
{
	m_Impl->GetTaxIdsForSeqId(seq_id, taxids);
}


END_NCBI_SCOPE

