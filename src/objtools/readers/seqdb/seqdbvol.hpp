#ifndef OBJTOOLS_READERS_SEQDB__SEQDBVOL_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBVOL_HPP

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

/// @file seqdbvol.hpp
/// Defines database volume access classes.
///
/// Defines classes:
///     CSeqDBVol
///
/// Implemented for: UNIX, MS-Windows

#include "seqdbatlas.hpp"
#include "seqdbgeneral.hpp"
#include "seqdbtax.hpp"
#include <objects/seq/seq__.hpp>

BEGIN_NCBI_SCOPE

using namespace ncbi::objects;

/// CSeqDBVol class
/// 
/// This object defines access to one database volume.

class CSeqDBVol {
public:
    typedef CSeqDBAtlas::TIndx TIndx;
    
    CSeqDBVol(CSeqDBAtlas   & atlas,
              const string  & name,
              char            prot_nucl);
    
    Int4 GetSeqLengthProt(Uint4 oid) const;
    
    Int4 GetSeqLengthApprox(Uint4 oid) const;
    
    // Assumes locked.
    
    Int4 GetSeqLengthExact(Uint4 oid) const;
    
    CRef<CBlast_def_line_set> GetHdr(Uint4 oid, CSeqDBLockHold & locked) const;
    
    char GetSeqType() const;
    
    CRef<CBioseq>
    GetBioseq(Uint4                 oid,
              bool                  have_oidlist,
              Uint4                 memb_bit,
              Uint4                 pref_gi,
              CRef<CSeqDBTaxInfo>   tax_info,
              CSeqDBLockHold      & locked) const;
    
    // Will get (and release) the lock as needed.
    
    Int4 GetSequence(Int4 oid, const char ** buffer, CSeqDBLockHold & locked) const
    {
        return x_GetSequence(oid, buffer, true, locked, true);
    }
    
    Int4 GetAmbigSeq(Int4              oid,
                     char           ** buffer,
                     Uint4             nucl_code,
                     ESeqDBAllocType   alloc_type,
                     CSeqDBLockHold  & locked) const;
    
    list< CRef<CSeq_id> > GetSeqIDs(Uint4 oid, CSeqDBLockHold & locked) const;
    
    string GetTitle() const;
    
    string GetDate() const;
    
    Uint4  GetNumSeqs() const;
    
    Uint8  GetTotalLength() const;
    
    Uint4  GetMaxLength() const;
    
    string GetVolName() const
    {
        return m_VolName;
    }
    
    void UnLease();
    
    bool PigToOid(Uint4 pig, Uint4 & oid, CSeqDBLockHold & locked) const;
    
    bool GetPig(Uint4 oid, Uint4 & pig, CSeqDBLockHold & locked) const;
    
    bool GiToOid(Uint4 gi, Uint4 & oid, CSeqDBLockHold & locked) const;
    
    bool GetGi(Uint4 oid, Uint4 & gi, CSeqDBLockHold & locked) const;
    
private:
    CRef<CBlast_def_line_set>
    x_GetHdrText(Uint4 oid,
                 CSeqDBLockHold & locked) const;
    
    void x_GetHdrBinary(Uint4            oid,
                        vector<char>   & hdr_data,
                        CSeqDBLockHold & locked) const;
    
    CRef<CSeqdesc> x_GetAsnDefline(Uint4 oid, CSeqDBLockHold & locked) const;
    
    char   x_GetSeqType() const;
    
    bool   x_GetAmbChar(Uint4            oid,
                        vector<Int4>   & ambchars,
                        CSeqDBLockHold & locked) const;
    
    Int4   x_GetAmbigSeq(Int4               oid,
                         char            ** buffer,
                         Uint4              nucl_code,
                         ESeqDBAllocType    alloc_type,
                         CSeqDBLockHold   & locked) const;
    
    char * x_AllocType(Uint4             length,
                       ESeqDBAllocType   alloc_type,
                       CSeqDBLockHold  & locked) const;
    
    // can_release: Specify 'true' here if this function is permitted
    // to release the atlas lock.  The nucleotide length computation
    // has a memory access which will often trigger a soft page fault.
    // This allows the code to release the lock prior to the memory
    // access, which results in a noticeable performance benefit.
    
    Int4 x_GetSequence(Int4             oid,
                       const char    ** buffer,
                       bool             keep,
                       CSeqDBLockHold & locked,
                       bool             can_release) const;
    
    CRef<CBlast_def_line_set>
    x_GetTaxDefline(Uint4 oid, CSeqDBLockHold & locked) const;
    
    CRef<CBlast_def_line_set>
    x_GetTaxDefline(Uint4            oid,
                    bool             have_oidlist,
                    Uint4            membership_bit,
                    Uint4            preferred_gi,
                    CSeqDBLockHold & locked) const;
    
    CRef<CSeqdesc>
    x_GetTaxonomy(Uint4                 oid,
                  bool                  have_oidlist,
                  Uint4                 membership_bit,
                  Uint4                 preferred_gi,
                  CRef<CSeqDBTaxInfo>   tax_info,
                  CSeqDBLockHold      & locked) const;
    
    bool x_SeqIdIn(const list< CRef<CSeq_id> > & seqids,
                   CSeq_id                     & seqid) const;
    
    CSeqDBAtlas      & m_Atlas;
    string             m_VolName;
    CSeqDBIdxFile      m_Idx;
    CSeqDBSeqFile      m_Seq;
    CSeqDBHdrFile      m_Hdr;
    
    // These are mutable because they defer initialization.
    mutable CRef<CSeqDBIsam> m_IsamPig;
    mutable CRef<CSeqDBIsam> m_IsamGi;
    mutable CRef<CSeqDBIsam> m_IsamStr;
};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBVOL_HPP


