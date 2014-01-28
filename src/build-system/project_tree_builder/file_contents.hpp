#ifndef PROJECT_TREE_BUILDER__FILE_CONTENTS__HPP
#define PROJECT_TREE_BUILDER__FILE_CONTENTS__HPP

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

#include "stl_msvc_usage.hpp"

#include <string>
#include <list>

#include <corelib/ncbistre.hpp>
#include "proj_utils.hpp"

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE

enum EMakeFileType {
    eMakeType_Undefined  = 0,
    eMakeType_Potential  = 1,
    eMakeType_Expendable = 2,
    eMakeType_Excluded   = 3,
    eMakeType_ExcludedByReq = 4
};

string MakeFileTypeAsString(EMakeFileType type);

/////////////////////////////////////////////////////////////////////////////
///
/// CSimpleMakeFileContents --
///
/// Abstraction of makefile contents.
///
/// Container for key/values pairs with included makefiles parsing support.

class CSimpleMakeFileContents
{
public:
    CSimpleMakeFileContents(void);
    CSimpleMakeFileContents(const CSimpleMakeFileContents& contents);

    CSimpleMakeFileContents& operator= (
	    const CSimpleMakeFileContents& contents);

    CSimpleMakeFileContents(const string& file_path, EMakeFileType type);
    CSimpleMakeFileContents(const string& file_path);

    ~CSimpleMakeFileContents(void);

    /// Key-Value(s) pairs
    typedef map< string, list<string> > TContents;
    TContents m_Contents;

    bool IsEmpty(void) const {
        return m_Contents.empty();
    }

    static void LoadFrom(const string& file_path, CSimpleMakeFileContents* fc);
    void AddDefinition( const string& key, const string& value);
    void RemoveDefinition( const string& key);
    bool HasDefinition( const string& key) const;
    bool DoesValueContain(const string& key, const string& value, bool ifnokey=true) const;
    bool GetPathValue(const string& key, string& value) const;
    bool GetValue(const string& key, string& value) const;
    bool GetValue(const string& key, list<string>& value) const;
    
    enum EHowToCollect {
        eAsIs,
        eSortUnique,
        eMergePlusMinus,
        eFirstNonempty
    };
    bool CollectValues( const string& key, list<string>& values, EHowToCollect how) const;
    
    EMakeFileType GetMakeType(void) const
    {
        return m_Type;
    }
    const string& GetFileName(void) const
    {
        return m_Filename;
    }

    void Save(const string& filename) const;
    /// Debug dump
    void Dump(CNcbiOstream& ostr, const list<string>* skip=0) const;
    
    void SetParent( const CSimpleMakeFileContents* parent)
    {
        m_Parent = parent;
    }

    void Clear(void);
    void SetValueSeparator(const string& sep)
    {
        m_ValueSeparator = sep;
    }

private:

    void SetFrom(const CSimpleMakeFileContents& contents);

    struct SParser;
    friend struct SParser;

    struct SParser
    {
        SParser(CSimpleMakeFileContents* fc);

        void StartParse(void);
        void AcceptLine(const string& line);
        void EndParse(void);

        bool      m_Continue;
        SKeyValue m_CurrentKV;

        CSimpleMakeFileContents* m_FileContents;

    private:
        SParser(void);
        SParser(const SParser&);
        SParser& operator= (const SParser&);
    };

    void AddReadyKV(const SKeyValue& kv);
    EMakeFileType m_Type;
    string m_Filename;
    string m_ValueSeparator;
    const CSimpleMakeFileContents* m_Parent;
    bool m_Raw;
};


END_NCBI_SCOPE

#endif //PROJECT_TREE_BUILDER__FILE_CONTENTS__HPP
