#ifndef BDB_VOLUMES_HPP__
#define BDB_VOLUMES_HPP__

/* $Id$
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
 * Author:  Anatoliy Kuznetsov
 *   
 * File Description: Volumes management
 *                   
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbimisc.hpp>
#include <bdb/bdb_file.hpp>
#include <bdb/bdb_env.hpp>

BEGIN_NCBI_SCOPE

/// BDB file for storing volumes registry 
///
struct NCBI_BDB_EXPORT SVolumesDB : public CBDB_File
{
    CBDB_FieldUint4        volume_id;  ///< Volume ID

    CBDB_FieldUint4        type;           ///< Volume type
    CBDB_FieldUint4        status;         ///< Volume status
    CBDB_FieldUint4        raw_status;     ///< Raw data status
    CBDB_FieldUint4        lock;           ///< Volume lock counter
    CBDB_FieldUint4        version;        ///< Volume version
    CBDB_FieldUint4        date_from;      ///< Date interval
    CBDB_FieldUint4        date_to;        ///< Date interval
    CBDB_FieldUint4        mtimestamp;     ///< Modification date
    CBDB_FieldUint4        relo_volume_id; ///< Modification date
    CBDB_FieldString       location;       ///< Mount point for the volume
    CBDB_FieldString       backup_loc;     ///< Location of volume backup


    SVolumesDB() 
        : CBDB_File(CBDB_File::eDuplicatesDisable, 
                    CBDB_File::eQueue)
    {
        DisableNull();

        BindKey ("volume_id",      &volume_id);

        BindData("type",           &type);
        BindData("status",         &status);
        BindData("raw_status",     &raw_status);
        BindData("lock",           &lock);
        BindData("version",        &version);
        BindData("date_from",      &date_from);
        BindData("date_to",        &date_to);
        BindData("mtimestamp",     &mtimestamp);
        BindData("relo_volume_id", &relo_volume_id);
        BindData("location",       &location, 1024);
        BindData("backup_loc",     &backup_loc, 1024);
    }

private:
    SVolumesDB(const SVolumesDB&);
    SVolumesDB& operator=(const SVolumesDB&);
};

/// Volumes management
///
/// Stores all volume registration as a transactional BDB database.
/// All operations are implemented as atomic transactions. 
/// The transactional database can be used across process boundaries
/// (but not across network).
///
class NCBI_BDB_EXPORT CBDB_Volumes
{
public:
    /// Volume status codes
    enum EVolumeStatus {
        eOnlinePassive = 0, ///< Online, read-only closed for updates
        eOnlineActive,      ///< Online, new updates can come
        eOnlineMaintenance, ///< Online, readable, under background processing
        eOffline,           ///< Offline, unmounted
        eOfflineRelocated,  ///< Offline, data moved to another volume
        eOfflineArchived,   ///< Offline, data moved to archive
        eOfflineRestore     ///< Offline, restore request received
    };
public:
    CBDB_Volumes();
    ~CBDB_Volumes();

    /// Open (mount) volume management database.
    /// Volume management uses a dedicated directory to store volume management 
    /// database, which is always opens as transactional BDB environment.
    /// Do NOT keep any other databases or environments at the same location
    ///
    /// @param dir_path
    ///    Location of volume management database.
    ///
    void Open(const string& dir_path);

    /// Close volume management database
    ///
    void Close();

    /// Get volume record
    /// (throws an exception if record not found)
    ///
    const SVolumesDB& FetchVolumeRec(unsigned volume_id);

    /// Get low level access to volumes database (be careful will you!)
    SVolumesDB& GetVolumeDB();


    /// Register a new volume 
    ///
    /// @return volume id
    unsigned AddVolume(const string& location,
                       unsigned      type,
                       unsigned      version,
                       EVolumeStatus status = eOffline);

    /// Set backup token (location) for volume
    void SetBackupLocation(unsigned volume_id, const string& backup_loc);

    /// Change volume status
    void ChangeStatus(unsigned volume_id, EVolumeStatus status);

    /// Increment volume lock counter
    /// Volume locking sets restrictions on some operations 
    /// (like status change)
    ///
    void LockVolume(unsigned volume_id);

    /// Decrement volume lock counter
    void UnLockVolume(unsigned volume_id);

    /// Transactional volume switch: old volume goes offline
    /// new volume comes online
    ///
    void SwitchVolumes(unsigned volume_id_old, 
                       unsigned volume_id_new,
                       EVolumeStatus new_status = eOnlineActive);

    /// Old volume goes offline, new volumes online
    /// All volume records should be created upfront
    void Split(unsigned volume_id_old, 
               unsigned volume_id_new1,
               unsigned volume_id_new2,
               EVolumeStatus new_status = eOnlineActive);

    /// Delete volume
    void Delete(const vector<unsigned>& remove_list);

    /// Sort volumes
    void SortVolumes();

    /// Merge volumes into one
    void Merge(unsigned volume_id_new,
               const vector<unsigned>& merge_list,
               EVolumeStatus new_status = eOnlineActive);

    /// Update date range
    void SetDateRange(unsigned volume_id,
                      unsigned from, 
                      unsigned to);

    /// Get list of all available volumes 
    /// @param vlist  
    ///     List of volumes
    /// @param avail
    ///     When TRUE returns only online volumes
    ///     (FALSE - all volumes)
    void EnumerateVolumes(vector<unsigned>& vlist, bool avail = false);

    /// Utility to convert status to string
    static string StatusToString(EVolumeStatus status);
protected:
    /// Check if status change is possible
    bool x_CheckStatusChange(EVolumeStatus old_status, 
                             EVolumeStatus new_status);
    void x_ChangeCurrentStatus(unsigned volume_id,
                               EVolumeStatus status);
private:
    CBDB_Volumes(const CBDB_Volumes&);
    CBDB_Volumes& operator=(const CBDB_Volumes&);
    friend class CBDB_VolumesTransaction;
private:
    auto_ptr<CBDB_Env>      m_Env;
    auto_ptr<SVolumesDB>    m_VolumesDB;
    string                  m_Path;
};

/// Exceptions specific to volumes management
///
class CBDB_VolumesException : public CBDB_Exception
{
public:
    enum EErrCode {
        eTransactionsNotAvailable,
        eVolumeNotFound,
        eVolumeLocked,
        eVolumeNotLocked,
        eVolumeStatusIncorrect
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eTransactionsNotAvailable: return "eTransactionsNotAvailable";
        case eVolumeNotFound:           return "eVolumeNotFound";
        case eVolumeLocked:             return "eVolumeLocked";
        case eVolumeNotLocked:          return "eVolumeNotLocked";
        case eVolumeStatusIncorrect:    return "eVolumeStatusIncorrect";

        default: return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CBDB_VolumesException, CBDB_Exception);
};



END_NCBI_SCOPE

#endif // BDB_VOLUMES_HPP__

