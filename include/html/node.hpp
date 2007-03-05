#ifndef HTML___NODE__HPP
#define HTML___NODE__HPP

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
 */

/// @file node.hpp 
/// The standard node class.


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <map>
#include <list>
#include <memory>


/** @addtogroup HTMLcomp
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class CNCBINode;
typedef CRef<CNCBINode> CNodeRef;
//#define NCBI_LIGHTWEIGHT_LIST 1

// Base class for a graph node.
class NCBI_XHTML_EXPORT CNCBINode : public CObject
{
public:
    friend class CRef<CNCBINode>;
    typedef list<CNodeRef> TChildren;
#if NCBI_LIGHTWEIGHT_LIST
    typedef TChildren TChildrenMember;
#else
    typedef auto_ptr<TChildren> TChildrenMember;
#endif
    struct SAttributeValue
    {
        SAttributeValue(void)
            : m_Optional(true)
            {
                return;
            }
        SAttributeValue(const string& value, bool optional)
            : m_Value(value), m_Optional(optional)
            {
                return;
            }
        SAttributeValue& operator=(const string& value)
            {
                m_Value = value;
                m_Optional = true;
                return *this;
            }
        const string& GetValue(void) const
            {
                return m_Value;
            }
        operator const string&(void) const
            {
                return m_Value;
            }
        bool IsOptional(void) const
            {
                return m_Optional;
            }
        void SetOptional(bool optional = true)
            {
                m_Optional = optional;
            }
    private:
        string m_Value;
        bool m_Optional;
    };
    typedef map<string, SAttributeValue, PNocase> TAttributes;
    
    enum EMode {
        eHTML      = 0,
        ePlainText = 1,
        eXHTML     = 2
    };

    class TMode {
    public:
        TMode(EMode mode = eHTML)
            : m_Mode(mode), m_Node(0), m_Previous(0)
            {
                return;
            }
        TMode(int mode)
            : m_Mode(EMode(mode)), m_Node(0), m_Previous(0)
            {
                return;
            }
        TMode(const TMode* mode, CNCBINode* node)
            : m_Mode(mode->m_Mode), m_Node(node), m_Previous(mode)
            {
                return;
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

    // Add a Node * to the end of m_Children.
    // Returns 'this' for chained AppendChild().
    CNCBINode* AppendChild(CNCBINode* child);
    CNCBINode* AppendChild(CNodeRef& ref);

    // Remove all occurencies of the child from this node's subtree
    // (along with its subtree).
    // Throw exception if the child is not found.
    // Return smart pointer to the removed child node.
    CNodeRef RemoveChild(CNCBINode* child);
    CNodeRef RemoveChild(CNodeRef&  child);
    void RemoveAllChildren(void);

    // All child operations (except AppendChild) are valid only if
    // have children return true
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

    void    SetRepeatCount(size_t count = 0);
    size_t  GetRepeatCount(void);

    // This method will be called once before Print().
    virtual void CreateSubNodes(void);
    // Call CreateSubNodes() if it's not called yet.
    void Initialize(void);
    // Reinitialize node, so hierarhy can be created anew.
    // All previously set attributes remains unchanged. 
    // On the next Print() the CreateSubNodes() method
    // will be called again.
    void ReInitialize(void);

    // Find and replace text with a node.
    virtual CNCBINode* MapTag(const string& tagname);
    CNodeRef MapTagAll(const string& tagname, const TMode& mode);

    // Repeat tag node (works only inside tag node mappers)
    void RepeatTag(bool enable = true);
    bool NeedRepeatTag(void);

    const string& GetName(void) const;

    bool HaveAttributes(void) const;
    TAttributes& Attributes(void);
    const TAttributes& Attributes(void) const;
    // Retrieve attribute.
    bool HaveAttribute(const string& name) const;
    const string& GetAttribute(const string& name) const;
    bool AttributeIsOptional(const string& name) const;
    bool AttributeIsOptional(const char* name) const;
    void SetAttributeOptional(const string& name, bool optional = true);
    void SetAttributeOptional(const char* name, bool optional = true);
    const string* GetAttributeValue(const string& name) const;

    // Set attribute.
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

    // Exception handling.

    /// Flags defining how to catch and process exceptions.
    /// By default flags are unsettled.
    /// Note that without the fCatchAll flag only CHTMLExceptions and
    /// all derived exceptons can be traced.
    enum EExceptionFlags {
        fAddTrace              = 0x1, ///< Enable tag trace.
        fCatchAll              = 0x2, ///< Catch all other exceptions and
                                      ///< rethrow CHTMLException.
        fDisableCheckRecursion = 0x4  ///< Disable to throw exception if
                                      ///<  nodes tree have endless recursion.
    };
    typedef int TExceptionFlags;      ///< Binary OR of "EExceptionFlags"

    // Set/get global exception handling flags.
    static void SetExceptionFlags(TExceptionFlags flags);
    static TExceptionFlags GetExceptionFlags(void);

protected:
    virtual void DoAppendChild(CNCBINode* child);
    virtual void DoSetAttribute(const string& name,
                                const string& value, bool optional);

    bool            m_CreateSubNodesCalled;
    TChildrenMember m_Children;         ///< Child nodes
    string          m_Name;             ///< Node name
    size_t          m_RepeatCount;      ///< How many times repeat node

    // Repeat tag flag (used only inside tag node mappers hooks). See RepeatTag().
    bool            m_RepeatTag; 

    // Attributes, e.g. href="link.html"
    auto_ptr<TAttributes> m_Attributes;     

private:
    // To prevent copy constructor.
    CNCBINode(const CNCBINode& node);
    // To prevent assignment operator.
    CNCBINode& operator=(const CNCBINode& node);

    // Return children list (create if needed).
    TChildren& GetChildren(void);
    // Return attributes map (create if needed).
    TAttributes& GetAttributes(void);
};


// Inline functions are defined here:
#include <html/node.inl>


END_NCBI_SCOPE


/* @} */

#endif  /*  HTML___NODE__HPP */
