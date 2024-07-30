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
 * Author:  Justin Foley
 *
 * File Description:
 *   asnvalidate output formatters
 *
 */

#ifndef _ASNVAL_FORMATTERS_HPP_
#define _ASNVAL_FORMATTERS_HPP_

#include <corelib/ncbistd.hpp>

class CAppConfig; // Why is this outside NCBI scope?
BEGIN_NCBI_SCOPE

namespace objects {
class CValidErrItem;
}

class IFormatter {
public:
    IFormatter(CNcbiOstream& ostr) : m_Ostr(ostr) {}
    virtual ~IFormatter(){}

    virtual void Start(){}
    virtual void Finish(){}

    virtual void operator()(const objects::CValidErrItem& item, bool ignoreInferences = false) = 0;

protected:
    CNcbiOstream& m_Ostr;
};


unique_ptr<IFormatter> g_CreateFormatter(const CAppConfig& config, CNcbiOstream& ostr);



END_NCBI_SCOPE

#endif

