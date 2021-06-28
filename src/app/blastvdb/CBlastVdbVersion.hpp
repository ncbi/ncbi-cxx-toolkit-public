#ifndef __C_BLAST_VDB_VERSION_HPP__
#define __C_BLAST_VDB_VERSION_HPP__


#include <algo/blast/api/version.hpp> // CBlastVersion

#include <klib/ncbi-vdb-version.h> // GetPackageVersion


namespace ncbi {
  namespace blast {
    class CBlastVdbVersion : public CBlastVersion {
        virtual string Print(void) const {
            string version(CBlastVersion::Print());
            version.append("\nncbi-vdb: ").append(GetPackageVersion());
            return version;
        }
    };
  }
}


#endif // __C_BLAST_VDB_VERSION_HPP__
