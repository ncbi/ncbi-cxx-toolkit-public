#ifndef OBJECTS_BIBLIO___LABEL_UTIL__HPP
#define OBJECTS_BIBLIO___LABEL_UTIL__HPP

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
 * Author:  Clifford Clausen, Aleksey Grichenko
 *          (moved from CPub class)
 *
 * File Description:
 *   utility functions for GetLabel()
 *
 */   


#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

#ifndef BEGIN_objects_SCOPE
#  define BEGIN_objects_SCOPE BEGIN_SCOPE(objects)
#  define END_objects_SCOPE END_SCOPE(objects)
#endif
BEGIN_objects_SCOPE // namespace ncbi::objects::

class CAuth_list;
class CImprint;
class CTitle;
class CCit_book;
class CCit_jour;


NCBI_BIBLIO_EXPORT
void GetLabelContent(string*            label,
                     bool               unique,
                     const CAuth_list*  authors,
                     const CImprint*    imprint,
                     const CTitle*      title,
                     const CCit_book*   book,
                     const CCit_jour*   journal,
                     const string*      title1 = 0,
                     const string*      title2 = 0,
                     const string*      titleunique = 0,
                     const string*      date = 0,
                     const string*      volume = 0,
                     const string*      issue = 0,
                     const string*      pages = 0,
                     bool               unpublished = false);


END_objects_SCOPE
END_NCBI_SCOPE

#endif  /* OBJECTS_BIBLIO___LABEL_UTIL__HPP */
