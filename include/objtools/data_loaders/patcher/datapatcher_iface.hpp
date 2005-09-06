#ifndef __DATA_PATCHER_IFACE_HPP__
#define __DATA_PATCHER_IFACE_HPP__

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
*  ===========================================================================
*
*  Author: Maxim Didenko
*
*  File Description:
*
* ===========================================================================
*/

#include <corelib/plugin_manager.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class ITSE_Assigner;
class ISeq_id_Translator;
class CSeq_entry;
class CTSE_Info;

/////////////////////////////////////////////////////////////////////////////////
//
// IDataPatcher
//

class NCBI_XLOADER_PATCHER_EXPORT IDataPatcher : public CObject
{
public:

    virtual ~IDataPatcher();

    virtual CRef<ITSE_Assigner> GetAssigner() = 0;
    virtual CRef<ISeq_id_Translator> GetSeqIdTranslator() = 0;

    virtual void Patch(CSeq_entry& entry);
    virtual bool IsPatchNeeded(const CTSE_Info& ) = 0;
};


END_SCOPE(objects)

NCBI_DECLARE_INTERFACE_VERSION(objects::IDataPatcher,  "xdatapatcher", 1, 0, 0);
 
template<>
class CDllResolver_Getter<objects::IDataPatcher>
{
public:
    CPluginManager_DllResolver* operator()(void)
    {
        CPluginManager_DllResolver* resolver =
            new CPluginManager_DllResolver
            (CInterfaceVersion<objects::IDataPatcher>::GetName(),
             kEmptyStr,
             CVersionInfo::kAny,
             CDll::eAutoUnload);
        resolver->SetDllNamePrefix("ncbi");
        return resolver;
    }
};

END_NCBI_SCOPE

/* ========================================================================== 
 * $Log$
 * Revision 1.1  2005/09/06 13:22:11  didenko
 * IDataPatcher interface moved to a separate file
 *
 * ========================================================================== */

#endif  // __DATA_PATCHER_IFACE_HPP__
