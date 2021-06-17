#ifndef LDS2_HPP__
#define LDS2_HPP__
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
 * Author: Aleksey Grichenko
 *
 * File Description: LDS v.2 data manager.
 *
 */

#include <corelib/ncbiobj.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/lds2/lds2_db.hpp>
#include <objtools/lds2/lds2_handlers.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/// Class for managing LDS2 database and related data files.
class NCBI_LDS2_EXPORT CLDS2_Manager : public CObject
{
public:
    /// Create LDS2 manager for the specified db file.
    /// If the file does not exist, it will be created only after adding
    /// at least one data file and indexing it.
    CLDS2_Manager(const string& db_file);

    virtual ~CLDS2_Manager(void);

    /// Get currently selected database name.
    const string& GetDbFile(void) const;

    /// Get the current database object.
    CLDS2_Database* GetDatabase(void) { return m_Db.GetPointerOrNull(); }

    /// Select new database. If the database does not yet exist,
    /// it is not created immediately. The list of data files is
    /// cleared.
    void SetDbFile(const string& db_file);

    /// Add new data file to the list. This will not parse and index the
    /// new file - call UpdateData().
    void AddDataFile(const string& data_file);

    /// Directory parsing mode while indexing files
    enum EDirMode {
        eDir_NoRecurse, ///< Do not parse sub-dirs automatically.
        eDir_Recurse    ///< Automatically scan sub-directories (default).
    };

    /// Add data directory. All files in the directory are added to the list.
    /// If the mode is eDir_Recurse, also adds all subdirectories.
    /// Call UpdateData to parse and index the files.
    void AddDataDir(const string& data_dir, EDirMode mode = eDir_Recurse);

    /// Register a URL handler. Using handlers allows to use special
    /// storage types like compressed files, ftp or http locations etc.
    /// The same handler must be registered in the data loader
    /// when using LDS2 to fetch data. The default handlers "file" and
    /// "gzipfile" for local files are registered automatically.
    void RegisterUrlHandler(CLDS2_UrlHandler_Base* handler);

    /// Add a URL. The handler is used to access the URL and must be
    /// registered in the manager before adding the URL.
    void AddDataUrl(const string& url, const string& handler_name);

    /// Remove all data from the database
    void ResetData(void);

    /// Rescan all indexed files, check for modifications, update the database.
    void UpdateData(void);

    /// Control indexing of GB releases (bioseq-sets).
    enum EGBReleaseMode {
        eGB_Ignore, ///< Do not split bioseq-sets (default)
        eGB_Guess,  ///< Try to autodetect and split GB release bioseq-sets
        eGB_Force   ///< Split all top-level bioseq-sets into seq-entries
    };

    EGBReleaseMode GetGBReleaseMode(void) const { return m_GBReleaseMode; }
    void SetGBReleaseMode(EGBReleaseMode mode) { m_GBReleaseMode = mode; }

    /// Control seq-id conflict resolving during file parsing.
    enum EDuplicateIdMode {
        /// Ignore bioseqs with duplicate ids, store just the first one.
        eDuplicate_Skip,
        /// Store all bioseqs regardless of seq-id conflicts (defalut).
        /// The conflict may be resolved later by data loader.
        eDuplicate_Store,
        /// Throw exception on bioseqs with duplicate seq-ids.
        eDuplicate_Throw
    };

    EDuplicateIdMode GetDuplicateIdMode(void) const { return m_DupIdMode; }
    void SetDuplicateIdMode(EDuplicateIdMode mode) { m_DupIdMode = mode; }

    /// Control grouping of standalone seq-aligns into bigger blobs.
    /// If set to 0 or 1, no grouping is performed, each seq-align
    /// becomes a separate blob.
    int GetSeqAlignGroupSize(void) const { return m_SeqAlignGroupSize; }
    void SetSeqAlignGroupSize(int sz) { m_SeqAlignGroupSize = sz; }

    /// Error handling while indexing files.
    /// NOTE: Only a few kinds of errors can be ignored (unsupported
    /// file format or object type, broken data file etc.).
    enum EErrorMode {
        eError_Silent, ///< Try to ignore errors, continue indexing.
        eError_Report, ///< Print error messages, but do not fail (default).
        eError_Throw   ///< Throw exceptions on errors.
    };

    EErrorMode GetErrorMode(void) const { return m_ErrorMode; }
    void SetErrorMode(EErrorMode mode) { m_ErrorMode = mode; }

    /// Fasta reader settings
    CFastaReader::TFlags GetFastaFlags(void) const { return m_FastaFlags; }
    void SetFastaFlags(CFastaReader::TFlags flags) { m_FastaFlags = flags; }

private:
    typedef CLDS2_Database::TStringSet TFiles;

    // Check for gzip file.
    bool x_IsGZipFile(const SLDS2_File& file_info);

    // Find handler for the file.
    CLDS2_UrlHandler_Base* x_GetUrlHandler(const SLDS2_File& file_info);
    // Get file info and handler
    SLDS2_File x_GetFileInfo(const string&                file_name,
                             CRef<CLDS2_UrlHandler_Base>& handler);
    void x_ParseFile(const SLDS2_File&      info,
                     CLDS2_UrlHandler_Base& handler);

    // All registered handlers by name.
    typedef map<string, CRef<CLDS2_UrlHandler_Base> > THandlers;
    // List of URLs which require special handlers.
    typedef map<string, string> THandlersByUrl;

    CRef<CLDS2_Database> m_Db;
    TFiles               m_Files;
    THandlersByUrl       m_HandlersByUrl;
    EGBReleaseMode       m_GBReleaseMode;
    EDuplicateIdMode     m_DupIdMode;
    EErrorMode           m_ErrorMode;
    CFastaReader::TFlags m_FastaFlags;
    THandlers            m_Handlers;
    int                  m_SeqAlignGroupSize;
};


inline
const string& CLDS2_Manager::GetDbFile(void) const
{
    _ASSERT(m_Db);
    return m_Db->GetDbFile();
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // LDS2_HPP__
