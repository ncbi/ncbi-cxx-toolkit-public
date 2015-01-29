#ifndef SEQ_TABLE_SETTER__HPP
#define SEQ_TABLE_SETTER__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   Object field setter interface
*
*/

#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE

class CObjectInfo;
class CObjectTypeInfo;

BEGIN_SCOPE(objects)

class CSeq_loc;
class CSeq_feat;

/////////////////////////////////////////////////////////////////////////////
// CSeq_feat and CSeq_loc field setters
/////////////////////////////////////////////////////////////////////////////

class CSeqTableSetLocField : public CObject
{
public:
    virtual ~CSeqTableSetLocField();

    virtual void SetInt(CSeq_loc& loc, int value) const;
    virtual void SetInt8(CSeq_loc& loc, Int8 value) const;
    virtual void SetReal(CSeq_loc& loc, double value) const;
    virtual void SetString(CSeq_loc& loc, const string& value) const;
};


class CSeqTableSetFeatField : public CObject
{
public:
    virtual ~CSeqTableSetFeatField();

    virtual void SetInt(CSeq_feat& feat, int value) const;
    virtual void SetInt8(CSeq_feat& feat, Int8 value) const;
    virtual void SetReal(CSeq_feat& feat, double value) const;
    virtual void SetString(CSeq_feat& feat, const string& value) const;
    virtual void SetBytes(CSeq_feat& feat, const vector<char>& value) const;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQ_TABLE_INFO__HPP
