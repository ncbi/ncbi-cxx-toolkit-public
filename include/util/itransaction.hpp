#ifndef UTIL_ITRANSACTION__HPP
#define UTIL_ITRANSACTION__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: ITransaction interface
 *
 */

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup Transact
 *
 * @{
 *
 *  The idea of ITransactional and ITransactionalRegistry
 *  is that both form a pair of associated objects.
 *  When Transaction goes out of scope it informs subjects of
 *  transaction that they are free of the transaction context.
 *
 *  <pre>
 *  {{
 *  File        f1, f2;
 *  Transaction tr;
 *  f1.SetTransaction(tr);
 *  f2.SetTransaction(tr);
 *  ...... // do something with f1,f2
 *  tr.Commit();
 *  }}           <---- here transaction goes out of scope and
 *                     all associated objects(transactional) are 
 *                     free (from transaction)
 *     Transaction in this case is derived from:
 *                    ITransaction + ITransactionalRegistry
 *     File  is derived of ITransactional
 *  </pre>
 *
 */

/// Transaction interface
class NCBI_XUTIL_EXPORT ITransaction
{
public:
    virtual ~ITransaction();
        
    /// Commit transaction
    virtual void Commit() = 0;    
    /// Abort transaction 
    virtual void Rollback() = 0;
};


/// Interface for transactional objects. 
/// Support of transaction association.
///
class NCBI_XUTIL_EXPORT ITransactional
{
public:
    virtual ~ITransactional();

    /// Establish transaction association  
    ///
    virtual void SetTransaction(ITransaction* trans) = 0;

    /// Remove transaction association 
    /// (must be established by  SetTransaction
    ///
    virtual void RemoveTransaction(ITransaction* trans) = 0;
};

/// Registration of transactional objects
///
class NCBI_XUTIL_EXPORT ITransactionalRegistry
{
public:
    virtual ~ITransactionalRegistry();

    /// Register transactional object
    virtual void Add(ITransactional*) = 0;

    /// Forget the transactional object
    virtual void Remove(ITransactional*) = 0;
};

/* @} */


END_NCBI_SCOPE

#endif
