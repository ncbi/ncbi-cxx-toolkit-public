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
 * Author: Frank Ludwig
 *
 * File Description:
 *   Deprecated legacy NCBI_DEPRECATED typedefs for IMessageListener.
 *   This file should eventually be removed.
 *
 */

#ifndef OBJTOOLS_READERS___ERRORCONTAINER__HPP
#define OBJTOOLS_READERS___ERRORCONTAINER__HPP

#include <objtools/readers/message_listener.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects) // namespace ncbi::objects::

// These are essentially typedefs, except that typedefs can't
// be used because old files might have forward declarations
// like "class IErrorContainer;" that would conflict with a typedef.

class NCBI_DEPRECATED IErrorContainer        : public ILineErrorListener      { };
class NCBI_DEPRECATED CErrorContainerBase    : public CMessageListenerBase    { };
class NCBI_DEPRECATED CErrorContainerLenient : public CMessageListenerLenient { };
class NCBI_DEPRECATED CErrorContainerStrict  : public CMessageListenerStrict  { };
class NCBI_DEPRECATED CErrorContainerCount   : public CMessageListenerCount   { 
public:
    CErrorContainerCount(size_t uMaxCount) :
        CMessageListenerCount(uMaxCount) { }
};
class NCBI_DEPRECATED CErrorContainerLevel   : public CMessageListenerLevel   {
public:
    CErrorContainerLevel( int iLevel ) :
        CMessageListenerLevel(iLevel) { }
};
class NCBI_DEPRECATED CErrorContainerWithLog : public CMessageListenerWithLog {
public:
    CErrorContainerWithLog(
    const CDiagCompileInfo& info) :
    CMessageListenerWithLog(info) { }

};

END_SCOPE(objects)

END_NCBI_SCOPE


#endif /* OBJTOOLS_READERS___ERRORCONTAINER__HPP */
