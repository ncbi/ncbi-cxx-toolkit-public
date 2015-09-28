#ifndef OBJECTS_ID2_ID2PROCESSOR_INTERFACE__HPP_INCLUDED
#define OBJECTS_ID2_ID2PROCESSOR_INTERFACE__HPP_INCLUDED
/*  $Id$
* ===========================================================================
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
* ===========================================================================
*
*  Author:  Eugene Vasilchenko
*
*  File Description: IID2Processor plugin interface
*
*/

#include <corelib/plugin_manager.hpp>


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)

class CID2Processor;

END_SCOPE(objects)

NCBI_DECLARE_INTERFACE_VERSION(objects::CID2Processor,  "xid2proc", 1, 0, 0);

template<>
class NCBI_ID2_EXPORT CDllResolver_Getter<objects::CID2Processor>
{
public:
    CPluginManager_DllResolver* operator()(void);
};

END_NCBI_SCOPE


#endif//OBJECTS_ID2_ID2PROCESSOR_INTERFACE__HPP_INCLUDED
