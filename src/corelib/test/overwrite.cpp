#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

USING_NCBI_SCOPE;

void Remove(const string& file)
{
    unlink(file.c_str());
}

bool Exists(const string& file)
{
    struct stat st;
    if ( stat(file.c_str(), &st) == 0 )
        return true;
    if ( errno == ENOENT )
        return false;
    ERR_POST("Unexpected errno after stat(): " << errno);
    return false;
}

long FileSize(const string& file)
{
    struct stat st;
    if ( stat(file.c_str(), &st) == 0 )
        return st.st_size;
    ERR_POST("Cannot get size of file: " << errno);
    return 0;
}

int main()
{
    SetDiagStream(&NcbiCerr);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostPrefix("test_overwrite");
    
    string fileName = "test_file.tmp";
    Remove(fileName);
    if ( Exists(fileName) ) {
        ERR_POST("cannot remove " << fileName);
        return 1;
    }
    {
        // write data
        CNcbiOfstream o(fileName.c_str(), IOS_BASE::app);
        if ( !o ) {
            ERR_POST("Cannot create file " << fileName);
            return 1;
        }
        o.seekp(0, IOS_BASE::end);
        if ( streampos(o.tellp()) != streampos(0) ) {
            ERR_POST("New file returns non zero size" << streampos(o.tellp()));
            return 1;
        }
        o << "TEST";
    }
    if ( FileSize(fileName) != 4 ) {
        ERR_POST("Wrong file size after write" << FileSize(fileName));
        return 1;
    }
    {
        // try to overwrite data
        CNcbiOfstream o(fileName.c_str(), IOS_BASE::in | IOS_BASE::out);
        if ( !o ) {
            ERR_POST("Cannot open file " << fileName);
            return 1;
        }
        o.seekp(0, IOS_BASE::end);
        if ( streampos(o.tellp()) == streampos(0) ) {
            ERR_POST("Non empty file returns zero size");
            return 1;
        }
    }
    if ( FileSize(fileName) != 4 ) {
        ERR_POST("File size was changed after test" << FileSize(fileName));
        return 1;
    }
    Remove(fileName);
    ERR_POST(Info << "Test passed successfully");
    return 0;
}
