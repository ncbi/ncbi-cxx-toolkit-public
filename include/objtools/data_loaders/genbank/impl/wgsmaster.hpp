#ifndef WGSMASTER__HPP_INCLUDED
#define WGSMASTER__HPP_INCLUDED
/*  $Id$
* ===========================================================================
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
* ===========================================================================
*
*  Author:  Eugene Vasilchenko
*
*  File Description: various blob stream processors
*
*/

#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objects/seq/Seqdesc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CBioseq_Info;
class CTSE_Chunk_Info;

class NCBI_XREADER_EXPORT CWGSMasterSupport
{
public:
    static const int kForceDescrMask = ((1<<CSeqdesc::e_Pub) |
                                        (1<<CSeqdesc::e_Comment) |
                                        (1<<CSeqdesc::e_User));
    static const int kOptionalDescrMask = ((1<<CSeqdesc::e_Source) |
                                           (1<<CSeqdesc::e_Molinfo) |
                                           (1<<CSeqdesc::e_Create_date) |
                                           (1<<CSeqdesc::e_Update_date) |
                                           (1<<CSeqdesc::e_Genbank) |
                                           (1<<CSeqdesc::e_Embl));
    static const int kGoodDescrMask = kForceDescrMask | kOptionalDescrMask;
    
    static CSeq_id_Handle GetWGSMasterSeq_id(const CSeq_id_Handle& idh);
    static bool HasMasterId(const CBioseq_Info& seq, const CSeq_id_Handle& master_idh);
    static void AddMasterDescr(CBioseq_Info& seq, const CSeq_descr& src);
    static CRef<CSeq_descr> GetWGSMasterDescr(CDataLoader* loader,
                                              const CSeq_id_Handle& master_idh,
                                              int mask, TUserObjectTypesSet& uo_types);
    static void LoadWGSMaster(CDataLoader* loader,
                              CRef<CTSE_Chunk_Info> chunk);
    static void AddWGSMaster(CTSE_LoadLock& lock);
};


class CWGSBioseqUpdater_Base : public CBioseqUpdater,
                               public CWGSMasterSupport
{
public:
    CWGSBioseqUpdater_Base(const CSeq_id_Handle& master_idh);
    virtual ~CWGSBioseqUpdater_Base();

    bool HasMasterId(const CBioseq_Info& seq) const {
        return CWGSMasterSupport::HasMasterId(seq, m_MasterId);
    }
    
private:
    CSeq_id_Handle m_MasterId;
};


class NCBI_XREADER_EXPORT CWGSBioseqUpdaterChunk : public CWGSBioseqUpdater_Base
{
public:
    CWGSBioseqUpdaterChunk(const CSeq_id_Handle& master_idh);
    virtual ~CWGSBioseqUpdaterChunk();

    virtual void Update(CBioseq_Info& seq);
};


class NCBI_XREADER_EXPORT CWGSBioseqUpdaterDescr : public CWGSBioseqUpdater_Base
{
public:
    CWGSBioseqUpdaterDescr(const CSeq_id_Handle& master_idh,
                           CRef<CSeq_descr> descr);
    virtual ~CWGSBioseqUpdaterDescr();

    virtual void Update(CBioseq_Info& seq);

private:
    CRef<CSeq_descr> m_Descr;
};


class NCBI_XREADER_EXPORT CWGSMasterChunkInfo : public CTSE_Chunk_Info
{
public:
    CWGSMasterChunkInfo(const CSeq_id_Handle& master_idh,
                        int mask, TUserObjectTypesSet& uo_types);
    virtual ~CWGSMasterChunkInfo();

    CSeq_id_Handle m_MasterId;
    int m_DescrMask;
    TUserObjectTypesSet m_UserObjectTypes;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//WGSMASTER__HPP_INCLUDED
