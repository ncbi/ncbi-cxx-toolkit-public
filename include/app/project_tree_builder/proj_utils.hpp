#ifndef PROJ_UTILS_01152004_HEADER
#define PROJ_UTILS_01152004_HEADER

/// Utilits for Project Tree Builder:
#include <corelib/ncbienv.hpp>


BEGIN_NCBI_SCOPE
//------------------------------------------------------------------------------

template <class A, class B, class C>
struct STriple
{
    typedef A TFirst;
    typedef B TSecond;
    typedef C TThird;

    STriple()
        :m_First  (A()),
         m_Second (B()),
         m_Third  (C())
    {
    }

    STriple(const A& a, const B& b, const C& c)
        :m_First  (a),
         m_Second (b),
         m_Third  (c)
    {
    }

    STriple(const STriple& triple)
        :m_First  (triple.m_First),
         m_Second (triple.m_Second),
         m_Third  (triple.m_Third)
    {
    }

    STriple& operator = (const STriple& triple)
    {
        if(this != &triple)
        {
            m_First  = triple.m_First;
            m_Second = triple.m_Second;
            m_Third  = triple.m_Third;
        }
        return *this;
    }

    ~STriple()
    {
    }

    
    bool operator < (const STriple& triple) const
    {
        if( m_First  > triple.m_First )
            return false;
        else if( m_First  < triple.m_First )
            return true;
        else {

            if(m_Second > triple.m_Second)
                return false;
            else if (m_Second < triple.m_Second)
                return true;
            else
                return m_Third < triple.m_Third;
        }
    }


    bool operator == (const STriple& triple) const
    {
        return !(*this < triple)  &&  !(triple < *this);
    }

    bool operator != (const STriple& triple) const
    {
        return !(*this == triple);
    }


    A m_First;
    B m_Second;
    C m_Third;    
};

//------------------------------------------------------------------------------

inline string GetParentDir(const string& dir)
{
    string parent_dir;
    CDirEntry::SplitPath( CDirEntry::DeleteTrailingPathSeparator(dir), 
                          &parent_dir );
    return parent_dir;
}


inline string GetFolder(const string& dir)
{
    string folder;
    CDirEntry::SplitPath( CDirEntry::DeleteTrailingPathSeparator(dir), 
                          NULL, &folder );
    return folder;
}

//------------------------------------------------------------------------------
END_NCBI_SCOPE

#endif //PROJ_UTILS_01152004_HEADER