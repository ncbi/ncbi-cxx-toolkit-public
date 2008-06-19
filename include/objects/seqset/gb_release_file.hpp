#ifndef UTILS___GB_RELEASE_FILE__HPP
#define UTILS___GB_RELEASE_FILE__HPP
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
* Author:  Mati Shomrat
* File Description:
*   Utility class for processing Genbank release files.
*/
#include <corelib/ncbistd.hpp>
#include <objects/seqset/Seq_entry.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

// forward decleration
class CGBReleaseFileImpl;


// CGBReleaseFile is a utility class to ease the processing of Genbank
// release files one Seq-entry at a time.
// It shields the user from the underlying I/O hook mechanism.
// 
// Usage:
// 1. Implement ISeqEntryHandler interface
// 2. Create a CGBReleaseFile with the release file's name
// 3. Register the handler object.
// 4. Call Read(..), the handling method will be called for each Seq-entry.

class NCBI_SEQSET_EXPORT CGBReleaseFile
{
public:
    
    // Interface for handling Seq-entry objects
    class ISeqEntryHandler
    {
    public:
        // user code for handling a Seq-entry goes here.
        // The return value indicates whethear to continue (true),
        // or abort (false) the read.
        virtual bool HandleSeqEntry(CRef<CSeq_entry>& entry) = 0;
        virtual ~ISeqEntryHandler(void) {};
    };
    
    // constructors
    CGBReleaseFile(const string& file_name);
    CGBReleaseFile(CObjectIStream& in);

    // destructor
    virtual ~CGBReleaseFile(void);
    
    // Register handler
    void RegisterHandler(ISeqEntryHandler* handler);

    // Read the release file
    void Read(void);

private:
    
    CGBReleaseFileImpl& x_GetImpl(void);
    CRef<CObject>   m_Impl;
};


END_NCBI_SCOPE

#endif  //  UTILS___GB_RELEASE_FILE__HPP
