#ifndef OBJTOOLS_READERS_SEQDB__SEQDBIMPL_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBIMPL_HPP

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


/// @file seqdbimpl.hpp
/// The top level of the private implementation layer for SeqDB.
/// 
/// Defines classes:
///     CSeqDBImplFlush
///     CSeqDBImpl
/// 
/// Implemented for: UNIX, MS-Windows

#include "seqdbvolset.hpp"
#include "seqdbalias.hpp"
#include "seqdboidlist.hpp"

BEGIN_NCBI_SCOPE

using namespace ncbi::objects;


/// CSeqDBImplFlush class
/// 
/// This functor like object provides a call back mechanism to return
/// memory holds to the atlas, from lease objects in the other objects
/// under CSeqDBImpl.  Without this class, CSeqDBAtlas and CSeqDBImpl
/// would be codependant, which is somewhat bad style, and creates
/// annoying cyclical dependencies among the include files.

class CSeqDBImplFlush : public CSeqDBFlushCB {
public:
    CSeqDBImplFlush(void)
        : m_Impl(0)
    {
    }
    
    virtual void SetImpl(class CSeqDBImpl * impl)
    {
        m_Impl = impl;
    }
    
    virtual void operator()(void);
    
private:
    CSeqDBImpl * m_Impl;
};


/// CSeqDBImpl class
/// 
/// This is the main implementation layer of the CSeqDB class.  This
/// is seperated from that class so that various implementation
/// details of CSeqDB are kept from the public interface.

class CSeqDBImpl {
public:
    /// Numeric type that can span all OIDs for one instance of SeqDB.
    typedef Uint4 TOID;
    
    /// Constructor
    ///
    /// This builds a CSeqDBImpl object from the provided parameters,
    /// which correspond to those of the longer CSeqDB constructor.
    ///
    /// @param db_name_list
    ///   A list of database or alias names, seperated by spaces.
    /// @param prot_nucl
    ///   Either kSeqTypeProt for a protein database, or kSeqTypeNucl
    ///   for nucleotide.  These can also be specified as 'p' or 'n'.
    /// @param oid_begin
    ///   Iterator will skip OIDs less than this value.  Only OIDs
    ///   found in the OID lists (if any) will be returned.
    /// @param oid_end
    ///   Iterator will return up to (but not including) this OID.
    /// @param use_mmap
    ///   If kSeqDBMMap is specified (the default), memory mapping is
    ///   attempted.  If kSeqDBNoMMap is specified, memory mapping
    ///   fails, or this platform does not support it, the less
    ///   efficient read and write calls are used instead.
    CSeqDBImpl(const string & db_name_list,
               char           prot_nucl,
               Uint4          oid_begin,
               Uint4          oid_end,
               bool           use_mmap);
    
    /// Destructor
    ~CSeqDBImpl();
    
    /// Get the sequence length.
    ///
    /// This computes and returns the exact sequence length for the
    /// indicated sequence.
    ///
    /// @param oid
    ///   The ordinal id of the sequence.
    /// @return
    ///   The length of the sequence in bases.
    Uint4 GetSeqLength(Uint4 oid) const;
    
    /// Get the approximate sequence length.
    ///
    /// This computes and returns the approximate sequence length for
    /// the indicated sequence.
    ///
    /// @param oid
    ///   The ordinal id of the sequence.
    /// @return
    ///   The approximate length of the sequence in bases.
    Uint4 GetSeqLengthApprox(Uint4 oid) const;
    
    /// Get the sequence header data.
    ///
    /// This builds and returns the header data corresponding to the
    /// indicated sequence as a Blast-def-line-set.
    ///
    /// @param oid
    ///   The ordinal id of the sequence.
    /// @return
    ///   The length of the sequence in bases.
    CRef<CBlast_def_line_set> GetHdr(Uint4 oid) const;
    
    /// Get the sequence type.
    ///
    /// Return an enumerated value indicating which type of sequence
    /// data this instance of SeqDB stores.
    ///
    /// @return
    ///   The type of sequences stored here, either kSeqTypeProt or kSeqTypeNucl.
    char GetSeqType() const;
    
    /// Get a CBioseq for a sequence.
    ///
    /// This builds and returns the header and sequence data
    /// corresponding to the indicated sequence as a CBioseq.  If
    /// target_gi is non-null, the header information will be filtered
    /// to only include the defline associated with that gi.
    ///
    /// @param oid
    ///   The ordinal id of the sequence.
    /// @param target_gi
    ///   The target gi to filter the header information by.
    /// @return
    ///   A CBioseq object corresponding to the sequence.
    CRef<CBioseq> GetBioseq(Uint4 oid, Uint4 target_gi) const;
    
    /// Get the sequence data for a sequence.
    ///
    /// Get the raw sequence (strand data).  When done, resources
    /// should be returned with RetSequence.  This data pointed to
    /// by *buffer is in read-only memory (where supported).
    /// @param oid
    ///   The ordinal id of the sequence.
    /// @param buffer
    ///   A returned pointer to the data in the sequence.
    /// @return
    ///   The return value is the sequence length (in base pairs or
    ///   residues).  In case of an error, an exception is thrown.
    Uint4 GetSequence(Uint4 oid, const char ** buffer) const;
    
    /// Get a pointer to sequence data with embedded ambiguities.
    ///
    /// This is like GetAmbigSeq(), but the allocated object should be
    /// deleted by the caller.  This is intended for users who are
    /// going to modify the sequence data, or are going to mix the
    /// data into a container with other data, and who are mixing data
    /// from multiple sources and want to free the data in the same
    /// way.  The fourth parameter should be given one of the values
    /// from EAllocStrategy; the corresponding method should be used
    /// to delete the object.  Note that "delete[]" should be used
    /// instead of "delete"
    ///
    /// @param oid
    ///   The ordinal id of the sequence.
    /// @param buffer
    ///   A returned pointer to the data in the sequence.
    /// @param nucl_code
    ///   The encoding to use for the returned sequence data.
    /// @param strategy
    ///   Selects which function is used to allocate the buffer.
    /// @return
    ///   The return value is the sequence length (in base pairs or
    ///   residues).  In case of an error, an exception is thrown.
    Uint4 GetAmbigSeq(Uint4           oid,
                      char         ** buffer,
                      Uint4           nucl_code,
                      ESeqDBAllocType strategy) const;
    
    void RetSequence(const char ** buffer) const;
    
    list< CRef<CSeq_id> > GetSeqIDs(Uint4 oid) const;
    
    string GetTitle() const;
    
    string GetDate() const;
    
    /// Returns the number of sequences available.
    Uint4 GetNumSeqs() const;
    
    /// Returns the size of the (possibly sparse) OID range.
    Uint4 GetNumOIDs(void) const;
    
    /// Returns the sum of the lengths of all available sequences.
    ///
    /// This uses summary information stored in the database volumes
    /// or alias files.  It provides an exact value, without iterating
    /// over individual sequences.
    Uint8 GetTotalLength() const;
    
    /// Returns the sum of the lengths of all volumes.
    ///
    /// This uses summary information stored in the database volumes
    /// (but not the alias files).  It provides an exact value,
    /// without iterating over individual sequences.  It includes all
    /// OIDs regardless of inclusion by the filtering mechanisms of
    /// the alias files.
    Uint8 GetVolumeLength(void) const;
    
    Uint4 GetMaxLength() const;
    
    bool CheckOrFindOID(Uint4 & next_oid) const;
    
    CSeqDB::EOidListType
    GetNextOIDChunk(TOID         & begin_chunk,
                    TOID         & end_chunk,
                    vector<TOID> & oid_list,
                    Uint4        * oid_state);
    
    const string & GetDBNameList() const;
    
    void SetMemoryBound(Uint8 membound, Uint8 slicesize)
    {
        m_Atlas.SetMemoryBound(membound, slicesize);
    }
    
    void FlushSeqMemory(void);
    
    /// Translate a PIG to an OID.
    bool PigToOid(Uint4 pig, Uint4 & oid) const;
    
    /// Translate a PIG to an OID.
    bool OidToPig(Uint4 oid, Uint4 & pig) const;
    
    /// Translate a GI to an OID.
    bool GiToOid(Uint4 gi, Uint4 & oid) const;
    
    /// Translate a GI to an OID.
    bool OidToGi(Uint4 oid, Uint4 & gi) const;
    
    /// Find OIDs matching the specified string.
    void AccessionToOids(const string & acc, vector<TOID> & oids) const;
    
    /// Translate a CSeq-id to a list of OIDs.
    void SeqidToOids(const CSeq_id & seqid, vector<TOID> & oids) const;
    
    /// Find the OID corresponding to the offset given in residues,
    /// into the database as a whole.
    Uint4 GetOidAtOffset(Uint4 first_seq, Uint8 residue) const;
    
private:
    string x_FixString(const string &) const;
    
    Uint4 x_GetNumSeqs() const;
    
    Uint4 x_GetNumOIDs() const;
    
    Uint8 x_GetTotalLength() const;
    
    Uint8 x_GetVolumeLength() const;
    
    /// Build the OID list
    ///
    /// OID list setup is done once, but not until needed.
    /// 
    /// @param locked
    ///   The lock hold object for this thread.
    void x_GetOidList(CSeqDBLockHold & locked) const;
    
    /// Get the next included oid
    ///
    /// This method checks if the OID list has the specified OID, and
    /// if not, finds the next one it does have, if any.
    /// 
    /// @param next_oid
    ///   The next oid to check and to return to the user.
    /// @param locked
    ///   The lock hold object for this thread.
    bool x_CheckOrFindOID(Uint4 & next_oid, CSeqDBLockHold & locked) const;
    
    CSeqDBImplFlush       m_FlushCB;
    mutable CSeqDBAtlas   m_Atlas;
    string                m_DBNames;
    CSeqDBAliasFile       m_Aliases;
    CSeqDBVolSet          m_VolSet;
    
    /// The list of included OIDs
    mutable CRef<CSeqDBOIDList> m_OIDList;
    CRef<CSeqDBTaxInfo>   m_TaxInfo;
    Uint4                 m_RestrictBegin;
    Uint4                 m_RestrictEnd;
    CFastMutex            m_OIDLock;
    Uint4                 m_NextChunkOID;
    
    Uint4                 m_NumSeqs;
    Uint4                 m_NumOIDs;
    Uint8                 m_TotalLength;
    Uint8                 m_VolumeLength;
    char                  m_SeqType;
    
    /// True if OID list setup is done.
    mutable bool          m_OidListSetup;
};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBIMPL_HPP

