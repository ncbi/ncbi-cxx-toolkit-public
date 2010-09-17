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
 * Author:  Christiam Camacho
 *
 */

/// @file linkoutdb.cpp
/// Implementation for the CLinkoutDB_Impl and CLinkoutDB classes

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include "seqdbatlas.hpp"
#include "linkoutdb_impl.hpp"
#include <objtools/blast/seqdb_reader/linkoutdb.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>

BEGIN_NCBI_SCOPE

bool CLinkoutDB::UseLinkoutDB()
{
    bool retval = false;
    CNcbiApplication* app = CNcbiApplication::Instance();
    if (app && !app->GetEnvironment().Get("LINKOUTDB").empty()) {
        retval = true;
    }
#ifdef NCBI_OS_UNIX
    if (app == NULL) {
        if (getenv("LINKOUTDB") != 0) {
            retval = true;
        }
    }
#endif
    return retval;
}

static const string kDefaultLinkoutDBName("linkouts");

map<string, CLinkoutDB*> CLinkoutDB::sm_LinkoutDBs;
CLinkoutDBDestroyer CLinkoutDB::sm_LinkoutDBDestroyer;

CLinkoutDBDestroyer::~CLinkoutDBDestroyer()
{
    ITERATE(set<CLinkoutDB*>, itr, m_Instances) {
        delete *itr;
    }
    m_Instances.clear();
}

CLinkoutDB& CLinkoutDB::GetInstance(const string& dbname /* = kEmptyStr */)
{
    CLinkoutDB* retval = NULL;
    map<string, CLinkoutDB*>::iterator pos = sm_LinkoutDBs.find(dbname);
    if (pos == sm_LinkoutDBs.end()) {
        retval = (dbname == kEmptyStr 
                        ? new CLinkoutDB()
                        : new CLinkoutDB(dbname));
        sm_LinkoutDBDestroyer.AddLinkoutDBSingleton(retval);
        sm_LinkoutDBs[dbname] = retval;
    } else {
        retval = pos->second;
    }
    _ASSERT(retval != NULL);
    return *retval;
}

CLinkoutDB::CLinkoutDB()
{
    const string dbname(SeqDB_ResolveDbPathForLinkoutDB(kDefaultLinkoutDBName));
    if (dbname.empty()) {
        string msg("Linkout database '");
        msg += kDefaultLinkoutDBName + "' not found";
        NCBI_THROW(CSeqDBException, eFileErr, msg);
    }
    m_Impl = new CLinkoutDB_Impl(dbname);
}

CLinkoutDB::CLinkoutDB(const string& dbname) 
    : m_Impl(new CLinkoutDB_Impl(dbname))
{}

CLinkoutDB::~CLinkoutDB()
{
    delete m_Impl;
}

int CLinkoutDB::GetLinkout(int gi)
{
    return m_Impl->GetLinkout(gi);
}

int CLinkoutDB::GetLinkout(const CSeq_id& id)
{
    return m_Impl->GetLinkout(id); 
}

CLinkoutDB_Impl::CLinkoutDB_Impl(const string& dbname) 
    : m_AtlasHolder(true, 0, 0), m_Atlas(m_AtlasHolder.Get())
{
    x_Init(dbname);
}

void CLinkoutDB_Impl::x_Init(const string& dbname)
{
    // This is determined in the blastdb_links application
    const char kProtNucl('p');

    // At least one of the indices must exist (probably the numeric one for
    // GIs)
    try {
        m_Accession2Linkout = new CSeqDBIsam(m_Atlas, dbname, kProtNucl, 's',
                                             eStringId);
    } catch (const CSeqDBException& e) {
        if (e.GetErrCode() != CSeqDBException::eFileErr) {
            throw;
        }
    }
    try {
        m_Gi2Linkout = new CSeqDBIsam(m_Atlas, dbname, kProtNucl, 'n', eGiId);
    } catch (const CSeqDBException& e) {
        if (e.GetErrCode() != CSeqDBException::eFileErr) {
            throw;
        }
    }

    if (m_Gi2Linkout.Empty() && m_Accession2Linkout.Empty()) {
        string msg("Linkout database '");
        msg += dbname + "' not found";
        NCBI_THROW(CSeqDBException, eFileErr, msg);
    }
}

int CLinkoutDB_Impl::GetLinkout(int gi)
{
    CSeqDBLockHold lock(m_Atlas);
    CSeqDBIsam::TOid retval = 0;
    return (m_Gi2Linkout->IdToOid(gi, retval, lock) ? retval : 0);
}

int CLinkoutDB_Impl::GetLinkout(const CSeq_id& id)
{
    CSeqDBLockHold lock(m_Atlas);
    CSeqDBIsam::TOid retval = 0;
    if (id.IsGi()) {
        return (m_Gi2Linkout->IdToOid(id.GetGi(), retval, lock) ? retval: 0);
    } else {
        Int8 ident(0);
        string str_id;
        bool simpler(false), version_check(true);
        SeqDB_SimplifySeqid(const_cast<CSeq_id&>(id), 0, ident, str_id, simpler);
        vector<CSeqDBIsam::TOid> oids;
        m_Accession2Linkout->StringToOids(str_id, oids, simpler, version_check, 
                                          lock);
        if ( !oids.empty() ) {
            retval = oids.front();
        }
    }
    return retval; 
}

void CLinkoutDB::GetLinkoutTypes(vector<CLinkoutDB::TLinkoutTypeString>& rv)
{
    rv.clear();
    rv.push_back(make_pair(eLocuslink, string("eLocuslink")));
    rv.push_back(make_pair(eUnigene, string("eUnigene")));
    rv.push_back(make_pair(eStructure, string("eStructure")));
    rv.push_back(make_pair(eGeo, string("eGeo")));
    rv.push_back(make_pair(eGene, string("eGene")));
    rv.push_back(make_pair(eHitInMapviewer, string("eHitInMapviewer")));
    rv.push_back(make_pair(eAnnotatedInMapviewer, string("eAnnotatedInMapviewer")));
    rv.push_back(make_pair(eGenomicSeq, string("eGenomicSeq")));
    rv.push_back(make_pair(eBioAssay, string("eBioAssay")));
}

END_NCBI_SCOPE

