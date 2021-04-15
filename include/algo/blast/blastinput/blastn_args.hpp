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
 * Author: Christiam Camacho
 *
 */

/** @file blastn_args.hpp
 * Main argument class for BLASTN application
 */

#ifndef ALGO_BLAST_BLASTINPUT___BLASTN_ARGS__HPP
#define ALGO_BLAST_BLASTINPUT___BLASTN_ARGS__HPP

#include <algo/blast/blastinput/blast_args.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handles command line arguments for blastn binary
class NCBI_BLASTINPUT_EXPORT CBlastnAppArgs : public CBlastAppArgs
{
public:
    /// Constructor
    CBlastnAppArgs();

    /// @inheritDoc
    virtual int GetQueryBatchSize() const;

    virtual ~CBlastnAppArgs() {}
protected:
    /// @inheritDoc
    virtual CRef<CBlastOptionsHandle>
    x_CreateOptionsHandle(CBlastOptions::EAPILocality locality,
                          const CArgs& args);

};

class NCBI_BLASTINPUT_EXPORT CBlastnNodeArgs : public CBlastnAppArgs
{
public:
    /// Constructor
    CBlastnNodeArgs(const string & input);

    /// @inheritDoc
    virtual int GetQueryBatchSize() const;

    /// Get the input stream
    virtual CNcbiIstream& GetInputStream();

    /// Get the output stream
    virtual CNcbiOstream& GetOutputStream();

    CNcbiOstrstream & GetOutputStrStream() { return m_OutputStream; }

    virtual ~CBlastnNodeArgs();

protected:
    /// @inheritDoc
    virtual CRef<CBlastOptionsHandle>
    x_CreateOptionsHandle(CBlastOptions::EAPILocality locality, const CArgs& args);

private :
    CNcbiOstrstream m_OutputStream;
    CNcbiIstrstream * m_InputStream;
};


END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_BLASTINPUT___BLASTN_ARGS__HPP */
