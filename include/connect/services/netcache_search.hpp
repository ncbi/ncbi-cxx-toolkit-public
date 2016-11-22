#ifndef CONNECT_SERVICES___NETCACHE_SEARCH__HPP
#define CONNECT_SERVICES___NETCACHE_SEARCH__HPP

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
 * Author: Rafael Sadyrov
 *
 */


#include <connect/connect_export.h>
#include <memory>
#include <chrono>


namespace ncbi {
namespace grid {
namespace netcache {
namespace search {


/// @internal
/// @{

struct SExpressionImpl;
struct SBlobInfoImpl;
struct SExpression { shared_ptr<SExpressionImpl> impl; ~SExpression(); };
struct SBlobInfo   { shared_ptr<SBlobInfoImpl>   impl; ~SBlobInfo();   };

/// @}


using time_point = chrono::system_clock::time_point;
using duration = chrono::system_clock::duration;


/// @defgroup NetCacheSearch
/// @{


/// Search and output fields
namespace fields {

const struct KEY             {KEY(){}}              key;
const struct SUBKEY          {SUBKEY(){}}           subkey;
const struct CREATED         {CREATED(){}}          created;
const struct EXPIRES         {EXPIRES(){}}          expires;
const struct VERSION_EXPIRES {VERSION_EXPIRES(){}}  version_expires;
const struct SIZE            {SIZE(){}}             size;

}


using namespace fields;

/// Search operators
/// @{

struct NCBI_XCONNECT_EXPORT CExpression { SExpression base; };

NCBI_XCONNECT_EXPORT CExpression operator==(KEY,             string);
NCBI_XCONNECT_EXPORT CExpression operator==(string,          KEY);

NCBI_XCONNECT_EXPORT CExpression operator>=(CREATED,         time_point);
NCBI_XCONNECT_EXPORT CExpression operator< (CREATED,         time_point);
NCBI_XCONNECT_EXPORT CExpression operator<=(time_point,      CREATED);
NCBI_XCONNECT_EXPORT CExpression operator> (time_point,      CREATED);

NCBI_XCONNECT_EXPORT CExpression operator>=(CREATED,         duration);
NCBI_XCONNECT_EXPORT CExpression operator< (CREATED,         duration);
NCBI_XCONNECT_EXPORT CExpression operator<=(duration,        CREATED);
NCBI_XCONNECT_EXPORT CExpression operator> (duration,        CREATED);

NCBI_XCONNECT_EXPORT CExpression operator>=(EXPIRES,         time_point);
NCBI_XCONNECT_EXPORT CExpression operator< (EXPIRES,         time_point);
NCBI_XCONNECT_EXPORT CExpression operator<=(time_point,      EXPIRES);
NCBI_XCONNECT_EXPORT CExpression operator> (time_point,      EXPIRES);

NCBI_XCONNECT_EXPORT CExpression operator>=(EXPIRES,         duration);
NCBI_XCONNECT_EXPORT CExpression operator< (EXPIRES,         duration);
NCBI_XCONNECT_EXPORT CExpression operator<=(duration,        EXPIRES);
NCBI_XCONNECT_EXPORT CExpression operator> (duration,        EXPIRES);

NCBI_XCONNECT_EXPORT CExpression operator>=(VERSION_EXPIRES, time_point);
NCBI_XCONNECT_EXPORT CExpression operator< (VERSION_EXPIRES, time_point);
NCBI_XCONNECT_EXPORT CExpression operator<=(time_point,      VERSION_EXPIRES);
NCBI_XCONNECT_EXPORT CExpression operator> (time_point,      VERSION_EXPIRES);

NCBI_XCONNECT_EXPORT CExpression operator>=(VERSION_EXPIRES, duration);
NCBI_XCONNECT_EXPORT CExpression operator< (VERSION_EXPIRES, duration);
NCBI_XCONNECT_EXPORT CExpression operator<=(duration,        VERSION_EXPIRES);
NCBI_XCONNECT_EXPORT CExpression operator> (duration,        VERSION_EXPIRES);

NCBI_XCONNECT_EXPORT CExpression operator>=(SIZE,            size_t);
NCBI_XCONNECT_EXPORT CExpression operator< (SIZE,            size_t);
NCBI_XCONNECT_EXPORT CExpression operator<=(size_t,          SIZE);
NCBI_XCONNECT_EXPORT CExpression operator> (size_t,          SIZE);

NCBI_XCONNECT_EXPORT CExpression operator&&(CExpression,     CExpression);

/// @}


/// Output fields specification
/// @{

struct NCBI_XCONNECT_EXPORT CFields
{
    SExpression base;

    CFields();
    CFields(CREATED);
    CFields(EXPIRES);
    CFields(VERSION_EXPIRES);
    CFields(SIZE);
};

NCBI_XCONNECT_EXPORT CFields operator|(CFields, CFields);

/// @}


/// Blob info
struct NCBI_XCONNECT_EXPORT CBlobInfo
{
    SBlobInfo base;

    string     operator[](KEY)             const;
    string     operator[](SUBKEY)          const;
    time_point operator[](CREATED)         const;
    time_point operator[](EXPIRES)         const;
    time_point operator[](VERSION_EXPIRES) const;
    size_t     operator[](SIZE)            const;
};


/// @}


}
}
}
}


#endif
