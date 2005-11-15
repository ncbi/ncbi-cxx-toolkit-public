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
 * Author:  Maxim Didenko
 *
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbifile.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include "asn_db.hpp"



BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CAsnDB::CAsnDB( const string& db_path )
    : m_DBPath(db_path)
{
    CDir dir(m_DBPath);
    if (!dir.Exists()) {
        dir.CreatePath();
    }       
}

CAsnDB::~CAsnDB()
{
}

void CAsnDB::CreateBlob(const string& blobid)
{
    CDir dir(m_DBPath + CDirEntry::GetPathSeparator() + blobid);
    if (!dir.Exists())
        dir.CreatePath();   
}

bool CAsnDB::HasBlob(const string& blobid) const
{
    CDir dir(m_DBPath + CDirEntry::GetPathSeparator() + blobid);
    return dir.Exists();
}

bool CAsnDB::Save(const string& blobid, const CBioseq& bioseq)
{
    CreateBlob(blobid);
    ITERATE(CBioseq::TId, it, bioseq.GetId()) {
        if( (*it)->Which() == CSeq_id::e_Gi ) {
            string bsid = (*it)->GetSeqIdString(false);
            string fname = m_DBPath + CDirEntry::GetPathSeparator() + blobid 
                + CDirEntry::GetPathSeparator() + bsid;
            auto_ptr<CObjectOStream> os( CObjectOStream::Open(eSerial_AsnText,fname) );
            *os << bioseq;
            return true;
        }
    }
    return false;
}

CRef<CBioseq> CAsnDB::Restore(const string& blobid, const CSeq_id& id) const
{
    CDir dir(m_DBPath + CDirEntry::GetPathSeparator() + blobid);
    if (dir.Exists()) {
        if (id.Which() == CSeq_id::e_Gi) {
            string bsid = id.GetSeqIdString(false);
            string fname = dir.GetPath() + CDirEntry::GetPathSeparator() + bsid;
            CFile file(fname);
            if (file.Exists()) {
                CRef<CBioseq> ret(new CBioseq);
                auto_ptr<CObjectIStream> is( CObjectIStream::Open(eSerial_AsnText,fname) );
                *is >> *ret;
                return ret;
            }
        }
    }            
    return CRef<CBioseq>();
}

bool CAsnDB::SaveTSE(const string& blobid, const CSeq_entry& entry)
{
    CDir dir(m_DBPath + CDirEntry::GetPathSeparator() + blobid);
    if (dir.Exists())
        dir.Remove(CDirEntry::eNonRecursive);
    dir.CreatePath();
    string fname = dir.GetPath() + CDirEntry::GetPathSeparator() + "TSE";
    auto_ptr<CObjectOStream> os( CObjectOStream::Open(eSerial_AsnText,fname) );            
    *os << entry;
    return true;    
}

CRef<CSeq_entry> CAsnDB::RestoreTSE(const string& blobid) const
{
    CDir dir(m_DBPath + CDirEntry::GetPathSeparator() + blobid);
    if (dir.Exists()) {
        string fname = dir.GetPath() + CDirEntry::GetPathSeparator() + "TSE";
        CFile file(fname);
        if (file.Exists()) {
            CRef<CSeq_entry> ret(new CSeq_entry);
            auto_ptr<CObjectIStream> is( CObjectIStream::Open(eSerial_AsnText,fname) );
            *is >> *ret;
            return ret;
        }
    }
    return CRef<CSeq_entry>();
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/11/15 19:25:21  didenko
 * Added bioseq_edit_sample sample
 *
 * ===========================================================================
 */
