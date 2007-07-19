/*
*  $Id$
*
* =========================================================================
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
*  Government do not and cannt warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* =========================================================================
*
*  Author: Boris Kiryutin
*
* =========================================================================
*/


#ifndef PSEQ_H
#define PSEQ_H

#include <corelib/ncbistl.hpp>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
    class CScope;
    class CSeq_id;
END_SCOPE(objects)
USING_SCOPE(ncbi::objects);

typedef vector<char> PSEQ;

class CPSeq
{
public:
    CPSeq(CScope& scope, const CSeq_id& protein);
    ~CPSeq(void);
    bool HasStart(void) {
        return toupper((int)seq[0]) == (int)'M';
    }

    PSEQ seq;
};

END_NCBI_SCOPE

#endif//PSEQ_H
