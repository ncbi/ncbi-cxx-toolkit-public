#ifndef DATATOOL__HPP
#define DATATOOL__HPP

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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <serial/datatool/generate.hpp>
#include <serial/datatool/fileutil.hpp>
#include <list>

BEGIN_NCBI_SCOPE

class CArgs;
class CFileSet;

class CDataTool : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

private:
    bool ProcessModules(void);
    bool ProcessData(void);
    bool GenerateCode(void);

    SourceFile::EType LoadDefinitions(
        CFileSet& fileSet, const list <string>& modulesPath,
        const string& names, bool split_names,
        SourceFile::EType srctype = SourceFile::eUnknown);

    CCodeGenerator generator;
};

//#include <serial/datatool/datatool.inl>

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.4  2005/04/13 15:56:07  gouriano
* Handle paths with spaces
*
* Revision 1.3  2003/05/23 19:19:08  gouriano
* modules of unknown type are assumed to be ASN
* all modules must have the same type
*
* Revision 1.2  2002/08/06 17:03:47  ucko
* Let -opm take a comma-delimited list; move relevant CVS logs to end.
*
* Revision 1.1  2000/11/27 18:19:30  vasilche
* Datatool now conforms CNcbiApplication requirements.
*
* ===========================================================================
*/

#endif  /* DATATOOL__HPP */
