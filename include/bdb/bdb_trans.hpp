#ifndef BDB_TRANS__HPP
#define BDB_TRANS__HPP
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
 * File Description: Transaction support
 *
 */

/// @file bdb_trans.hpp
/// Wrapper around Berkeley DB transaction structure

#include <bdb/bdb_types.hpp>
#include <util/itransaction.hpp>

#include <vector>

BEGIN_NCBI_SCOPE

class CBDB_Env;

/** @addtogroup BDB
 *
 * @{
 */

class   CBDB_RawFile;

/// BDB transaction object.
///
/// Transaction is associated with files using CBDB_File::SetTransaction()
///
/// @note
///   Transaction is aborted upon the desctruction of a non-commited
///   transaction object.

class NCBI_BDB_EXPORT CBDB_Transaction : public ITransaction,
                                         public ITransactionalRegistry
{
public:
    /// Enum controls if transaction is synchronous or not.
    /// see DB_TXN->commit for more details
    enum ETransSync {
        eEnvDefault, ///< Use default from CBDB_Env
        eTransSync,  ///< Syncronous transaction
        eTransASync  ///< Non-durable asyncronous transaction
    };

    /// When file is connected to transaction using
    /// CBDB_File::SetTransaction() class by default keeps track
    /// of all associated files and then drops association on 
    /// Commit(), Abort() or destruction.
    /// As an option we can create non associated transactions.
    /// We want such if we plan to Commit transactions concurrently
    /// or just looking for some performance optimization.
    ///
    enum EKeepFileAssociation
    {
        eFullAssociation,  ///< Transaction associated with files
        eNoAssociation     ///< No association tracking
    };

    /// Construct transaction
    CBDB_Transaction(CBDB_Env&             env, 
                     ETransSync            tsync = eEnvDefault,
                     EKeepFileAssociation  assoc = eFullAssociation);



    /// Non-commited transaction is aborted upon the destruction
    ///
    /// Destructor automatically detaches transaction from all
    /// connected files (see CBDB_File::SetTransaction())
    ///
    ~CBDB_Transaction();
    
    // ITransaction:

    /// Commit transaction
    virtual void Commit();
    /// Rollback transaction (same as Abort)
    virtual void Rollback() { Abort(); }

    


    /// Abort transaction
    void Abort();
    

    /// Get low level Berkeley DB transaction handle.
    ///
    /// Function uses lazy initialization paradigm, and actual
    /// transaction is created "on demand". First call to GetTxn()
    /// creates the handle which lives until commit or rollback point.
    ///
    /// @return Transaction handle
    ///
    DB_TXN*  GetTxn();

    /// Transaction file association mode
    EKeepFileAssociation GetAssociationMode() const { return m_Assoc; }

    /// Downcast ITransaction
    static
    CBDB_Transaction* CastTransaction(ITransaction* trans);

    // -----------------------------------------------------
    // ITransactionalRegistry
    // -----------------------------------------------------

    /// Add file to the list of connected files
    virtual void Add(ITransactional* dbfile);
    
    /// Remove file from the list of connected files.
    /// Has no effect if file has not been added before by AddFile
    virtual void Remove(ITransactional* dbfile);

protected:


    /// Return TRUE if transaction handle has been requested by some
    /// client (File)
    bool IsInProgress() const { return m_Txn != 0; }

private:
    /// Abort transaction with error checking or without
    void x_Abort(bool ignore_errors);

    /// Forget all
    void x_DetachFromFiles();

private:
    CBDB_Transaction(const CBDB_Transaction& trans);
    CBDB_Transaction& operator=(const CBDB_Transaction& trans);
protected:
    /// Transaction association list
    typedef vector<ITransactional*>  TTransVector;
protected:
    CBDB_Env&               m_Env;        ///< Associated environment
    ETransSync              m_TSync;      ///< Sync. flag
    EKeepFileAssociation    m_Assoc;      ///< Association flag
    DB_TXN*                 m_Txn;        ///< Transaction handle
    TTransVector            m_TransFiles; ///< Files connected to transaction
    
private:
    friend class CBDB_RawFile;
};

/* @} */

END_NCBI_SCOPE

#endif  /* BDB_ENV__HPP */
