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
* Authors:  Paul Thiessen
*
* File Description:
*       private common stuff for alignment utility algorithms
*
* ===========================================================================
*/

#ifndef STRUCT_UTIL_PRIVATE__HPP
#define STRUCT_UTIL_PRIVATE__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>


BEGIN_SCOPE(struct_util)

#define ERROR_MESSAGE(s) ERR_POST(Error << "struct_util: " << s << '!')
#define WARNING_MESSAGE(s) ERR_POST(Warning << "struct_util: " << s)
#define INFO_MESSAGE(s) ERR_POST(Info << "struct_util: " << s)
#define TRACE_MESSAGE(s) ERR_POST(Trace << "struct_util: " << s)

#define THROW_MESSAGE(str) throw CException(__FILE__, __LINE__, NULL, CException::eUnknown, (str))

// utility function to remove some elements from a vector. Actually does this by copying to a new
// vector, so T should have an efficient copy ctor.
template < class T >
void VectorRemoveElements(std::vector < T >& v, const std::vector < bool >& remove, unsigned int nToRemove)
{
    if (v.size() != remove.size()) {
#ifndef _DEBUG
        // MSVC gets internal compiler error here on debug builds... ugh!
        ERROR_MESSAGE("VectorRemoveElements() - size mismatch");
#endif
        return;
    }

    std::vector < T > copy(v.size() - nToRemove);
    unsigned int i, nRemoved = 0;
    for (i=0; i<v.size(); ++i) {
        if (remove[i])
            ++nRemoved;
        else
            copy[i - nRemoved] = v[i];
    }
    if (nRemoved != nToRemove) {
#ifndef _DEBUG
        ERR_POST(Error << "VectorRemoveElements() - bad nToRemove");
#endif
        return;
    }

    v = copy;
}

END_SCOPE(struct_util)

#endif // STRUCT_UTIL_PRIVATE__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2004/05/25 16:24:51  thiessen
* remove WorkShop warnings
*
* Revision 1.3  2004/05/25 16:12:30  thiessen
* fix GCC warnings
*
* Revision 1.2  2004/05/25 15:52:18  thiessen
* add BlockMultipleAlignment, IBM algorithm
*
* Revision 1.1  2004/05/24 23:04:05  thiessen
* initial checkin
*
*/

