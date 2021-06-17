#ifndef SRA__READER__SRA__WGSRESOLVER__HPP
#define SRA__READER__SRA__WGSRESOLVER__HPP
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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Resolve WGS accessions
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <vector>

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);

class CVDBMgr;
class CID2ProcessorResolver;

class NCBI_SRAREAD_EXPORT CWGSResolver : public CObject
{
public:
    CWGSResolver(void);
    virtual ~CWGSResolver(void);

    // return all WGS prefixes that could contain gi or accession
    typedef vector<string> TWGSPrefixes;
    virtual TWGSPrefixes GetPrefixes(TGi gi) = 0;
    virtual TWGSPrefixes GetPrefixes(const string& acc) = 0;

    // remember that the id is finally found in a specific WGS prefix
    // the info can be used to return smaller candidates set in the future
    virtual void SetWGSPrefix(TGi gi,
                              const TWGSPrefixes& prefixes,
                              const string& prefix);
    virtual void SetWGSPrefix(const string& acc,
                              const TWGSPrefixes& prefixes,
                              const string& prefix);
    // remember that the id finally happened to be non-WGS
    // the info can be used to return empty candidates set in the future
    virtual void SetNonWGS(TGi gi,
                           const TWGSPrefixes& prefixes);
    virtual void SetNonWGS(const string& acc,
                           const TWGSPrefixes& prefixes);

    // force update of indexes from files
    virtual bool Update(void);

    // create best resolver for environment
    // it can use direct access to a full WGS index file,
    // or use ID2 connection for resolution requests.
    static CRef<CWGSResolver> CreateResolver(const CVDBMgr& mgr);

    // create recursive resolver
    static CRef<CWGSResolver> CreateResolver(CID2ProcessorResolver* resolver);

private:
    // to prevent copying
    CWGSResolver(const CWGSResolver&);
    void operator=(const CWGSResolver&);
};

    
END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // SRA__READER__SRA__WGSRESOLVER__HPP
