#ifndef PROJ_DATATOOL_GENERATED_SRC_HEADER
#define PROJ_DATATOOL_GENERATED_SRC_HEADER

/* $Id$
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
 * Author:  Viatcheslav Gorelenkov
 *
 */

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE

class CDataToolGeneratedSrc
{
public:
    CDataToolGeneratedSrc(void);
    CDataToolGeneratedSrc(const string& source_file_path);
    CDataToolGeneratedSrc(const CDataToolGeneratedSrc& src);
    CDataToolGeneratedSrc& operator= (const CDataToolGeneratedSrc& src);
    ~CDataToolGeneratedSrc(void);

    string       m_SourceFile;
    string       m_SourceBaseDir;

    list<string> m_ImportModules;
    
    static void LoadFrom(const string&          source_file_path, 
                         CDataToolGeneratedSrc* src);

    bool IsEmpty(void) const;

private:
    void Clear(void);
    void SetFrom(const CDataToolGeneratedSrc& src);
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/01/30 20:37:37  gorelenk
 * Initial revision
 *
 * ===========================================================================
 */

#endif // PROJ_DATATOOL_GENERATED_SRC_HEADER
