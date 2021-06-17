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

#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <objects/seq/seq_id_handle.hpp>
#include <objects/seqedit/SeqEdit_Cmd.hpp>

#include "map_rw_helper.hpp"
#include "file_db_engine.hpp"

BEGIN_NCBI_SCOPE


template<>
class CContElemConverter<objects::CSeq_id_Handle>
{
public:
    static objects::CSeq_id_Handle FromString(const string& str)  
    {
        CNcbiIstrstream is_str(str);
        unique_ptr<CObjectIStream> is(CObjectIStream::Open(eSerial_AsnText,is_str));
        CRef<objects::CSeq_id> seqid(new objects::CSeq_id);
        *is >> *seqid;
        return objects::CSeq_id_Handle::GetHandle(*seqid);
    }
    static string ToString  (const objects::CSeq_id_Handle& id)  
    {
        CNcbiOstrstream os_str;
        {
        unique_ptr<CObjectOStream> os(CObjectOStream::Open(eSerial_AsnText,os_str));
        *os << *id.GetSeqId();
        }
        return CNcbiOstrstreamToString(os_str);
    }
};


BEGIN_SCOPE(objects)

CFileDBEngine::CFileDBEngine(const string& db_path)
    : m_DBPath(db_path), m_DataFormat(eSerial_AsnText)
{
    CDir dir(m_DBPath);
    if (!dir.Exists()) {
        dir.CreatePath();
    }
    CFile file(m_DBPath + CDirEntry::GetPathSeparator() + "ChandedIds");
    if (file.Exists()) {
        CNcbiIfstream is(file.GetPath().c_str());
        ReadMap(is, m_ChangedIds);
    }
}


CFileDBEngine::~CFileDBEngine()
{
}


void CFileDBEngine::CreateBlob(const string& blobid)
{
    CDir dir(m_DBPath + CDirEntry::GetPathSeparator() + blobid);
    if (!dir.Exists()) {
        dir.CreatePath();   
        m_CmdCount[blobid] = 0;
    }
}


bool CFileDBEngine::HasBlob(const string& blobid) const
{
    CDir dir(m_DBPath + CDirEntry::GetPathSeparator() + blobid);
    return dir.Exists();
}


string CFileDBEngine::x_GetCmdFileName(const string& blobid, int cmdcount)
{
    string fname;
    if (cmdcount < 10)
        fname = "000" + NStr::IntToString(cmdcount);
    else if (cmdcount < 100)
        fname = "00" + NStr::IntToString(cmdcount);
    else if (cmdcount < 1000)
        fname = "0" + NStr::IntToString(cmdcount);
    else 
        fname = NStr::IntToString(cmdcount);

    fname = m_DBPath + CDirEntry::GetPathSeparator() + blobid 
        + CDirEntry::GetPathSeparator() + fname;
    return fname;
}


void CFileDBEngine::SaveCommand( const CSeqEdit_Cmd& cmd )
{
    const string& blob_id = cmd.GetBlobId();
    if (blob_id.empty()) {
        _ASSERT(0);
        return;
    }
    CreateBlob(blob_id);
    
    int& cmdcount = m_CmdCount[blob_id];
    string fname = x_GetCmdFileName(blob_id,cmdcount+1);
    
    unique_ptr<CObjectOStream> os( CObjectOStream::Open(m_DataFormat,fname) );
    *os << cmd;

    ++cmdcount;
}


void CFileDBEngine::GetCommands(const string& blob_id, TCommands& cmds) const
{
    CDir dir(m_DBPath + CDirEntry::GetPathSeparator() + blob_id);
    list<string> fnames;
    if (dir.Exists()) {
        CDir::TEntries files = dir.GetEntries( "", CDir::fIgnoreRecursive);
        for (const auto& f : files) {
            if ( f->IsFile() )
                fnames.push_back(f->GetName());
        }
        if (fnames.empty()) {
            return;
        }
        fnames.sort();
        int& cmdcount = const_cast<CFileDBEngine*>(this)->m_CmdCount[blob_id];
        cmdcount = 0;
        for (const auto& name : fnames) {
            string fname = dir.GetPath() + CDirEntry::GetPathSeparator() + name;
            CRef<CSeqEdit_Cmd> cmd(new CSeqEdit_Cmd);
            unique_ptr<CObjectIStream> is( CObjectIStream::Open(m_DataFormat,fname) );
            *is >> *cmd;
            cmds.push_back(cmd);
            ++cmdcount;
        }
    } 
}


void CFileDBEngine::DropDB()
{
    CDir dir(m_DBPath);
    if (dir.Exists()) {
        dir.Remove();
        dir.CreatePath();
    }
    m_ChangedIds.clear();
    m_CmdCount.clear();        
}


bool CFileDBEngine::FindSeqId(const CSeq_id_Handle& id, string& blobid) const
{
    TChangedIds::const_iterator it = m_TmpIds.find(id);
    if (it != m_TmpIds.end()) {
        blobid = it->second;
        return true;
    }
    it = m_ChangedIds.find(id);
    if (it != m_ChangedIds.end()) {
        blobid = it->second;
        return true;
    }
    return false;
}


void CFileDBEngine::NotifyIdChanged(const CSeq_id_Handle& id, const string& newblobid)
{
    m_TmpIds[id] = newblobid;
}


void CFileDBEngine::BeginTransaction()
{
    m_TmpIds.clear();
    m_TmpCount.clear();
    for (auto const& it : m_CmdCount) {
        m_TmpCount[it.first] = it.second;
    }
}


void CFileDBEngine::CommitTransaction()
{
    for (const auto& it : m_TmpIds) {
        m_ChangedIds[it.first] = it.second;
    }
    m_TmpIds.clear();
    if (m_ChangedIds.size() > 0) {
        string fname = m_DBPath + CDirEntry::GetPathSeparator() + "ChandedIds";
        CNcbiOfstream os(fname.c_str());
        WriteMap(os, m_ChangedIds);    
    }
    m_TmpCount.clear();
}


void CFileDBEngine::RollbackTransaction()
{
    m_TmpIds.clear();

    for (auto const& it : m_CmdCount) {
        if( m_TmpCount.find(it.first) == m_TmpCount.end()) {
            int cmdcount = it.second;
            while (cmdcount > 0)  {
                string fname = x_GetCmdFileName(it.first, cmdcount);
                CFile file(fname);
                if (file.Exists()) {
                    file.Remove();
                }
                --cmdcount;
            }
            m_CmdCount[it.first] = 0;
        }
    }

    for (auto const& it : m_TmpCount) {
        _ASSERT( m_CmdCount.find(it.first) != m_CmdCount.end());
        int& cmdcount = m_CmdCount[it.first];
        while (cmdcount > it.second) {
            string fname = x_GetCmdFileName(it.first, cmdcount);
            CFile file(fname);
            if (file.Exists()) {
                file.Remove();
            }
            --cmdcount;
        }
    }
    m_TmpCount.clear();
}


END_SCOPE(objects)
END_NCBI_SCOPE
