#ifndef SEQ_ID_TRANSLATOR__HPP
#define SEQ_ID_TRANSLATOR__HPP

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
* Author: Maxim Didneko
*
* File Description:
*   Seq-id translator
*
*/

#include <corelib/ncbistd.hpp>

#include <vector>
#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_id_Handle;

class NCBI_XOBJMGR_EXPORT ISeq_id_Translator : public CObject
{
public:
    typedef vector<CSeq_id_Handle> TIds;

    virtual ~ISeq_id_Translator();

    virtual CSeq_id_Handle TranslateToOrig(const CSeq_id_Handle& patched) const = 0;
    virtual CSeq_id_Handle TranslateToPatched(const CSeq_id_Handle& orig) const = 0;
    virtual void TranslateToOrig(const TIds& patched, TIds& orig) const = 0;
    virtual void TranslateToPatched(const TIds& orig, TIds& pathced) const = 0;
    
};


class NCBI_XOBJMGR_EXPORT CSeq_id_Translator_Simple : public ISeq_id_Translator
{
public:

    virtual ~CSeq_id_Translator_Simple();

    void AddMapEntry(const CSeq_id_Handle& orig, 
                     const CSeq_id_Handle& patched);

    virtual CSeq_id_Handle TranslateToOrig(const CSeq_id_Handle& patched) const;
    virtual CSeq_id_Handle TranslateToPatched(const CSeq_id_Handle& orig) const;
    virtual void TranslateToOrig(const ISeq_id_Translator::TIds& patched, 
                                 ISeq_id_Translator::TIds& orig) const;
    virtual void TranslateToPatched(const ISeq_id_Translator::TIds& orig, 
                                    ISeq_id_Translator::TIds& patched) const;

private:
    typedef map<CSeq_id_Handle, CSeq_id_Handle> TMap;
    
    TMap m_OrigToPatched;
    TMap m_PatchedToOrig;
    
};


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.1  2005/08/25 14:05:36  didenko
* Restructured TSE loading process
*
*
* ===========================================================================
*/

#endif  // SEQ_ID_TRANSLATOR__HPP
