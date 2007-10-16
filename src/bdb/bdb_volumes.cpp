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
 * Author: Anatoliy Kuznetsov
 *
 * File Description: Volumes management
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>

#include <bdb/bdb_volumes.hpp>
#include <bdb/bdb_cursor.hpp>
#include <bdb/bdb_trans.hpp>
#include <bdb/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Bdb_Volumes

BEGIN_NCBI_SCOPE

/// Transaction class for volumes management
///
/// @internal
class CBDB_VolumesTransaction : public CBDB_Transaction
{
public:
    CBDB_VolumesTransaction(CBDB_Volumes& volumes)
        : CBDB_Transaction(*(volumes.m_Env),
                           CBDB_Transaction::eTransSync)
    {
        volumes.m_VolumesDB->SetTransaction(this);
    }
};




CBDB_Volumes::CBDB_Volumes()
{
}

CBDB_Volumes::~CBDB_Volumes()
{
    try {
        Close();
    } 
    catch (std::exception& ex)
    {
        ERR_POST_X(1, "Exception in ~CBDB_Volumes() " << ex.what());
    }    
}

void CBDB_Volumes::Close()
{
    m_VolumesDB.reset(0);
    m_Env.reset(0);
}

void CBDB_Volumes::Open(const string& dir_path)
{
    Close();
    m_Path = CDirEntry::AddTrailingPathSeparator(dir_path);
    // Make sure our directory exists
    {{
        CDir dir(m_Path);
        if ( !dir.Exists() ) {
            dir.Create();
        }
    }}
    m_Env.reset(new CBDB_Env());
    string err_file = m_Path + "err.log";
    m_Env->OpenErrFile(err_file.c_str());
    m_Env->SetLogFileMax(20 * 1024 * 1024);

    CDir dir(m_Path);
    CDir::TEntries fl = dir.GetEntries("__db.*", CDir::eIgnoreRecursive);
    if (fl.empty()) {
        m_Env->OpenWithTrans(m_Path, CBDB_Env::eThreaded);
    } else {
        try {
            m_Env->JoinEnv(m_Path, CBDB_Env::eThreaded);
            if (!m_Env->IsTransactional()) {
                m_Env.reset(0);
                NCBI_THROW(CBDB_VolumesException, eTransactionsNotAvailable,
                           "Joined non-transactional environment");
            }
        }
        catch (CBDB_ErrnoException& err_ex)
        {
            if (err_ex.IsRecovery()) {
                LOG_POST_X(2, Warning <<
                           "'Warning: DB_ENV returned DB_RUNRECOVERY code."
                           " Running the recovery procedure.");
            }
            m_Env->OpenWithTrans(m_Path,
                                 CBDB_Env::eThreaded | CBDB_Env::eRunRecovery);
        }
        catch (CBDB_Exception&)
        {
            m_Env->OpenWithTrans(m_Path,
                                CBDB_Env::eThreaded | CBDB_Env::eRunRecovery);
        }
    }
    //m_Env->SetDirectDB(true);
    //m_Env->SetDirectLog(true);
    m_Env->SetLogAutoRemove(true);

    m_Env->SetLockTimeout(30 * 1000000); // 30 sec

    m_VolumesDB.reset(new SVolumesDB());
    m_VolumesDB->SetEnv(*m_Env);
    m_VolumesDB->Open("volumes.db", CBDB_RawFile::eReadWriteCreate);
}

const SVolumesDB& CBDB_Volumes::FetchVolumeRec(unsigned volume_id)
{
    m_VolumesDB->volume_id = volume_id;
    EBDB_ErrCode err = m_VolumesDB->Fetch();
    if (err != eBDB_Ok) {
        NCBI_THROW(CBDB_VolumesException, eVolumeNotFound,
            string("Cannot find volume=") + NStr::UIntToString(volume_id));
    }
    return *m_VolumesDB;
}

unsigned CBDB_Volumes::AddVolume(const string& location,
                                 unsigned      type,
                                 unsigned      version,
                                 EVolumeStatus status)
{
    m_VolumesDB->type = type;
    m_VolumesDB->status = status;
    m_VolumesDB->raw_status = 0;
    m_VolumesDB->lock = 0;
    m_VolumesDB->version = version;
    m_VolumesDB->date_from = 0;
    m_VolumesDB->date_to = 0;
    m_VolumesDB->mtimestamp = (Uint4)time(0);
    m_VolumesDB->relo_volume_id = 0;
    m_VolumesDB->location = location;
    m_VolumesDB->backup_loc = "";
    
    CBDB_VolumesTransaction trans(*this);
    unsigned volume_id = m_VolumesDB->Append();
    trans.Commit();
    return volume_id;
}

void CBDB_Volumes::SetBackupLocation(unsigned      volume_id, 
                                     const string& backup_loc)
{
    CBDB_VolumesTransaction trans(*this);
    m_VolumesDB->volume_id = volume_id;
    EBDB_ErrCode err = m_VolumesDB->FetchForUpdate();
    if (err != eBDB_Ok) {
        NCBI_THROW(CBDB_VolumesException, eVolumeNotFound,
            string("Cannot find volume=") + NStr::UIntToString(volume_id));
    }
    m_VolumesDB->backup_loc = backup_loc;
    m_VolumesDB->UpdateInsert();
    trans.Commit();
}

void CBDB_Volumes::LockVolume(unsigned volume_id)
{
    CBDB_VolumesTransaction trans(*this);
    m_VolumesDB->volume_id = volume_id;
    EBDB_ErrCode err = m_VolumesDB->FetchForUpdate();
    if (err != eBDB_Ok) {
        NCBI_THROW(CBDB_VolumesException, eVolumeNotFound,
            string("Cannot find volume=") + NStr::UIntToString(volume_id));
    }
    unsigned lock = m_VolumesDB->lock;
    
    m_VolumesDB->lock = ++lock;
    m_VolumesDB->UpdateInsert();
    trans.Commit();
}

void CBDB_Volumes::UnLockVolume(unsigned volume_id)
{
    CBDB_VolumesTransaction trans(*this);
    m_VolumesDB->volume_id = volume_id;
    EBDB_ErrCode err = m_VolumesDB->FetchForUpdate();
    if (err != eBDB_Ok) {
        NCBI_THROW(CBDB_VolumesException, eVolumeNotFound,
            string("Cannot find volume=") + NStr::UIntToString(volume_id));
    }
    unsigned lock = m_VolumesDB->lock;
    if (lock == 0) {
        NCBI_THROW(CBDB_VolumesException, eVolumeNotLocked,
            string("Cannot unlock (lock count == 0) volume=") 
            + NStr::UIntToString(volume_id));
    }
    
    m_VolumesDB->lock = ++lock;
    m_VolumesDB->UpdateInsert();
    trans.Commit();

}


void CBDB_Volumes::ChangeStatus(unsigned volume_id, EVolumeStatus status)
{
    CBDB_VolumesTransaction trans(*this);

    x_ChangeCurrentStatus(volume_id, status);

    trans.Commit();
}

void CBDB_Volumes::SwitchVolumes(unsigned volume_id_old, 
                                 unsigned volume_id_new,
                                 EVolumeStatus new_status)
{
    CBDB_VolumesTransaction trans(*this);
    
    x_ChangeCurrentStatus(volume_id_old, eOffline);
    x_ChangeCurrentStatus(volume_id_new, new_status);

    trans.Commit();
}

void CBDB_Volumes::Split(unsigned volume_id_old, 
                         unsigned volume_id_new1,
                         unsigned volume_id_new2,
                         EVolumeStatus new_status)
{
    CBDB_VolumesTransaction trans(*this);
    
    x_ChangeCurrentStatus(volume_id_old,  eOffline);
    x_ChangeCurrentStatus(volume_id_new1, new_status);
    x_ChangeCurrentStatus(volume_id_new2, new_status);

    trans.Commit();
}

void CBDB_Volumes::Merge(unsigned volume_id_new,
                         const vector<unsigned>& merge_list,
                         EVolumeStatus new_status)
{
    CBDB_VolumesTransaction trans(*this);

    x_ChangeCurrentStatus(volume_id_new,  new_status);

    for (size_t i = 0; i < merge_list.size(); ++i) {
        unsigned v_id = merge_list[i];
        x_ChangeCurrentStatus(v_id,  eOffline);
    } // for

    trans.Commit();
}

void CBDB_Volumes::EnumerateVolumes(vector<unsigned>& vlist, bool avail)
{
    vlist.resize(0);
    CBDB_FileCursor cur(*m_VolumesDB);
    while (cur.Fetch() == eBDB_Ok) {
        unsigned volume_id = m_VolumesDB->volume_id;
        if (avail) {
            EVolumeStatus status = 
                (EVolumeStatus)(unsigned)m_VolumesDB->status;
            switch (status) {
            case eOnlinePassive:
            case eOnlineActive:
            case eOnlineMaintenance:
                break;
            default:
                continue;
            } // switch
        }
        vlist.push_back(volume_id);
    }
}

SVolumesDB& CBDB_Volumes::GetVolumeDB()
{
    return *m_VolumesDB;
}

void CBDB_Volumes::SetDateRange(unsigned volume_id,
                                unsigned from, 
                                unsigned to)
{
    CBDB_VolumesTransaction trans(*this);
    m_VolumesDB->volume_id = volume_id;
    EBDB_ErrCode err = m_VolumesDB->FetchForUpdate();
    if (err != eBDB_Ok) {
        NCBI_THROW(CBDB_VolumesException, eVolumeNotFound,
            string("Cannot find volume=") + NStr::UIntToString(volume_id));
    }
    unsigned lock = m_VolumesDB->lock;
    if (lock == 0) {
        NCBI_THROW(CBDB_VolumesException, eVolumeNotLocked,
            string("Cannot unlock (lock count == 0) volume=") 
            + NStr::UIntToString(volume_id));
    }
    m_VolumesDB->date_from = from;
    m_VolumesDB->date_to = to;
    
    m_VolumesDB->UpdateInsert();
    trans.Commit();
}


void CBDB_Volumes::x_ChangeCurrentStatus(unsigned volume_id,
                                         EVolumeStatus status)
{
    m_VolumesDB->volume_id = volume_id;
    EBDB_ErrCode err = m_VolumesDB->FetchForUpdate();
    if (err != eBDB_Ok) {
        NCBI_THROW(CBDB_VolumesException, eVolumeNotFound,
            string("Cannot find volume=") + NStr::UIntToString(volume_id));
    }
    unsigned lock = m_VolumesDB->lock;
    if (lock) {
        NCBI_THROW(CBDB_VolumesException, eVolumeLocked,
            string("Volume locked volume=") + NStr::UIntToString(volume_id));
    }
    EVolumeStatus old_status = (EVolumeStatus)(unsigned)m_VolumesDB->status;
    bool can_do = x_CheckStatusChange(old_status, status);
    if (!can_do) {
        NCBI_THROW(CBDB_VolumesException, eVolumeStatusIncorrect,
            string("Illegal volume status switch volume=") 
            + NStr::UIntToString(volume_id)
            + string(" old status = ") + StatusToString(old_status) 
            + string(" new status = ") + StatusToString(status) 
            );
    }
    m_VolumesDB->status = (unsigned)status;
    m_VolumesDB->UpdateInsert();

}


string CBDB_Volumes::StatusToString(EVolumeStatus status)
{
    switch (status)
    {
    case eOnlinePassive:         return "OnlinePassive";
    case eOnlineActive:          return "OnlineActive";
    case eOnlineMaintenance:     return "OnlineMaintenance";
    case eOffline:               return "Offline";
    case eOfflineRelocated:      return "OfflineRelocated";
    case eOfflineArchived:       return "OfflineArchived";
    case eOfflineRestore:        return "OfflineRestore";
    default:
        _ASSERT(0);
        return "Unknown status";
    } // switch
}


bool CBDB_Volumes::x_CheckStatusChange(EVolumeStatus old_status, 
                                       EVolumeStatus new_status)
{
    if (new_status == old_status) return true;

    // FSM rules are hard coded here...

    switch (new_status)
    {
    case eOnlinePassive:
        switch (old_status)
        {
        case eOnlineActive:
        case eOnlineMaintenance:
        case eOffline:
        case eOfflineArchived:
        case eOfflineRestore:
            return true;
        default:
            break;
        } // switch
        break;
    case eOnlineActive:
        switch (old_status)
        {
        case eOnlinePassive:
        case eOnlineMaintenance:
        case eOffline:
        case eOfflineRestore:
            return true;
        default:
            break;
        } // switch
        break;
    case eOnlineMaintenance:
        switch (old_status)
        {
        case eOnlinePassive:
        case eOnlineActive:
        case eOffline:
        case eOfflineRestore:
            return true;
        default:
            break;
        } // switch
        break;
    case eOffline:
        switch (old_status)
        {
        case eOnlinePassive:
        case eOnlineActive:
        case eOnlineMaintenance:
        case eOfflineRelocated:
        case eOfflineArchived:
        case eOfflineRestore:
            return true;
        default:
            break;
        } // switch
        break;
    case eOfflineRelocated:
        switch (old_status)
        {
        case eOnlinePassive:
        case eOnlineActive:
        case eOnlineMaintenance:
        case eOffline:
        case eOfflineRelocated:
        case eOfflineArchived:
        case eOfflineRestore:
            return true;
        default:
            break;
        } // switch
        break;
    case eOfflineArchived:
        switch (old_status)
        {
        case eOnlinePassive:
        case eOffline:
        case eOfflineRestore:
            return true;
        default:
            _ASSERT(0);
        } // switch
        break;
    case eOfflineRestore:
        switch (old_status)
        {
        case eOfflineArchived:
            return true;
        default:
            break;
        } // switch
        break;
    default:
        break;
    } // switch
    return false;
}



END_NCBI_SCOPE


