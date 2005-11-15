#ifndef __SAMPLE_DATA_PATCHER__HPP
#define __SAMPLE_DATA_PATCHER__HPP

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
* Author: Maxim Didenko
*
* File Description:
*
*/

#include <corelib/ncbiobj.hpp>

#include <objmgr/impl/tse_assigner.hpp>
#include <objtools/data_loaders/patcher/datapatcher_iface.hpp>


#include "asn_db.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSampleAssigner : public CTSE_Default_Assigner
{
public:
    CSampleAssigner(const CAsnDB& db);
    virtual ~CSampleAssigner() {}


    virtual void LoadBioseq(CTSE_Info&, const TPlace& place, 
                            CRef<CSeq_entry> entry);

private:
    const CAsnDB& m_AsnDB;
};


class CSampleDataPatcher : public IDataPatcher
{
public:

    CSampleDataPatcher(const string& db_path);
    CSampleDataPatcher(const CAsnDB& db);
    virtual ~CSampleDataPatcher() {}
    
    virtual CRef<ITSE_Assigner> GetAssigner();
    virtual CRef<ISeq_id_Translator> GetSeqIdTranslator() 
    { return CRef<ISeq_id_Translator>(); }
    virtual bool IsPatchNeeded(const CTSE_Info& tse);

    virtual void Patch(const CTSE_Info& tse, CSeq_entry& entry);
private:

    void DoPatch(const CTSE_Info& tse, CBioseq& bioseq);
    void DoPatch(const CTSE_Info& tse, CSeq_entry& entry);
    CRef<ITSE_Assigner> m_Assigner;

    auto_ptr<CAsnDB> m_AsnDBGuard;
    const CAsnDB&    m_AsnDB;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/11/15 19:25:22  didenko
 * Added bioseq_edit_sample sample
 *
 * ===========================================================================
 */

#endif // __SAMPLE_DATA_PATCHER__HPP
