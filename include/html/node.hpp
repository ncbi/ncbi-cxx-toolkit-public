#ifndef NODE__HPP
#define NODE__HPP

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
* Author:  Lewis Geer
*
* File Description:
*   standard node class
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.16  2000/03/29 15:50:38  vasilche
* Added const version of CRef - CConstRef.
* CRef and CConstRef now accept classes inherited from CObject.
*
* Revision 1.15  2000/03/07 15:40:37  vasilche
* Added AppendChild(CNodeRef&)
*
* Revision 1.14  2000/03/07 15:26:06  vasilche
* Removed second definition of CRef.
*
* Revision 1.13  1999/12/28 18:55:29  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.12  1999/11/19 15:45:32  vasilche
* CNodeRef implemented as CRef<CNCBINode>
*
* Revision 1.11  1999/10/28 13:40:30  vasilche
* Added reference counters to CNCBINode.
*
* Revision 1.10  1999/05/28 16:32:09  vasilche
* Fixed memory leak in page tag mappers.
*
* Revision 1.9  1999/05/20 16:49:13  pubmed
* Changes for SaveAsText: all Print() methods get mode parameter that can be HTML or PlainText
*
* Revision 1.8  1999/01/04 20:06:11  vasilche
* Redesigned CHTML_table.
* Added selection support to HTML forms (via hidden values).
*
* Revision 1.7  1998/12/28 20:29:13  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.6  1998/12/23 21:20:58  vasilche
* Added more HTML tags (almost all).
* Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
*
* Revision 1.5  1998/12/23 14:28:08  vasilche
* Most of closed HTML tags made via template.
*
* Revision 1.4  1998/12/21 22:24:57  vasilche
* A lot of cleaning.
*
* Revision 1.3  1998/11/23 23:47:50  lewisg
* *** empty log message ***
*
* Revision 1.2  1998/10/29 16:15:52  lewisg
* version 2
*
* Revision 1.1  1998/10/06 20:34:31  lewisg
* html library includes
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <map>
#include <list>
#include <memory>

BEGIN_NCBI_SCOPE

class CNCBINode;

typedef CRef<CNCBINode> CNodeRef;

// base class for a graph node

class CNCBINode : public CObject
{
public:
    friend class CRef<CNCBINode>;
    typedef list<CNodeRef> TChildren;
#if NCBI_LIGHTWEIGHT_LIST
    typedef TChildren TChildrenMember;
#else
    typedef auto_ptr<TChildren> TChildrenMember;
#endif
    typedef map<string, string, PNocase> TAttributes;
    
    enum EMode {
        eHTML = 0,
        ePlainText  = 1
    };
    class TMode {
    public:
        TMode(EMode mode = eHTML)
            : m_Mode(mode), m_Node(0), m_Previous(0)
            {
            }
        TMode(int mode)
            : m_Mode(EMode(mode)), m_Node(0), m_Previous(0)
            {
            }
        TMode(const TMode* mode, CNCBINode* node)
            : m_Mode(mode->m_Mode), m_Node(node), m_Previous(mode)
            {
            }
        operator EMode(void) const
            {
                return m_Mode;
            }
        bool operator==(EMode mode) const
            {
                return mode == m_Mode;
            }

        CNCBINode* GetNode(void) const
            {
                return m_Node;
            }
        const TMode* GetPreviousContext(void) const
            {
                return m_Previous;
            }
    private:
        // to avoid allocation in 
        EMode m_Mode;
        CNCBINode* m_Node;
        const TMode* m_Previous;
    };

    // 'structors
    CNCBINode(void);
    CNCBINode(const string& name);
    CNCBINode(const char* name);
    virtual ~CNCBINode();

    // add a Node * to the end of m_Children
    // returns this for chained AppendChild
    CNCBINode* AppendChild(CNCBINode* child);
    CNCBINode* AppendChild(CNodeRef& ref);
    void RemoveAllChildren(void);

    // all child operations (except AppendChild) are valid only if
    // have children returns true
    bool HaveChildren(void) const;
    TChildren& Children(void);
    const TChildren& Children(void) const;
    TChildren::iterator ChildBegin(void);
    TChildren::iterator ChildEnd(void);
    static CNCBINode* Node(TChildren::iterator i);
    TChildren::const_iterator ChildBegin(void) const;
    TChildren::const_iterator ChildEnd(void) const;
    static const CNCBINode* Node(TChildren::const_iterator i);

    virtual CNcbiOstream& Print(CNcbiOstream& out, TMode mode = eHTML);
    virtual CNcbiOstream& PrintBegin(CNcbiOstream& out, TMode mode);
    virtual CNcbiOstream& PrintChildren(CNcbiOstream& out, TMode mode);
    virtual CNcbiOstream& PrintEnd(CNcbiOstream& out, TMode mode);

    // this method will be called when printing and "this" node doesn't
    // contain any children
    virtual void CreateSubNodes(void);
    // finds and replaces text with a node    
    virtual CNCBINode* MapTag(const string& tagname);
    CNodeRef MapTagAll(const string& tagname, const TMode& mode);

    const string& GetName(void) const;

    bool HaveAttributes(void) const;
    TAttributes& Attributes(void);
    const TAttributes& Attributes(void) const;
    // retreive attribute
    bool HaveAttribute(const string& name) const;
    const string& GetAttribute(const string& name) const;
    const string* GetAttributeValue(const string& name) const;

    // set attribute
    void SetAttribute(const string& name, const string& value);
    void SetAttribute(const string& name);
    void SetAttribute(const string& name, int value);
    void SetOptionalAttribute(const string& name, const string& value);
    void SetOptionalAttribute(const string& name, bool set);

    void SetAttribute(const char* name, const string& value);
    void SetAttribute(const char* name);
    void SetAttribute(const char* name, int value);
    void SetOptionalAttribute(const char* name, const string& value);
    void SetOptionalAttribute(const char* name, bool set);

protected:
    TChildrenMember m_Children;  // Child nodes.

    string m_Name; // node name

    auto_ptr<TAttributes> m_Attributes; // attributes, e.g. href="link.html"

private:
    // to prevent copy contructor
    CNCBINode(const CNCBINode& node);
    // to prevent assignment operator
    CNCBINode& operator=(const CNCBINode& node);

    // return children list (create if needed)
    TChildren& GetChildren(void);
    // return attributes map (create if needed)
    TAttributes& GetAttributes(void);
    void DoAppendChild(CNCBINode* child);
};

// inline functions are defined here:
#include <html/node.inl>

END_NCBI_SCOPE
#endif
