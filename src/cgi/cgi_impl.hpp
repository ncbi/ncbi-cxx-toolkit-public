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
 * Author:  Denis Vakatov
 *
 */

#include <corelib/ncbi_url.hpp>
#include <cgi/ncbicgi.hpp>


BEGIN_NCBI_SCOPE


class NCBI_XCGI_EXPORT CCgiEntries_Parser : public CUrlArgs_Parser
{
public:
    CCgiEntries_Parser(TCgiEntries* entries,
                       TCgiIndexes* indexes,
                       bool indexes_as_entries);
protected:
    virtual void AddArgument(unsigned int position,
                             const string& name,
                             const string& value,
                             EArgType arg_type);
private:
    TCgiEntries* m_Entries;
    TCgiIndexes* m_Indexes;
    bool         m_IndexesAsEntries;
};


END_NCBI_SCOPE
