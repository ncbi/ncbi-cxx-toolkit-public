from conan import ConanFile, conan_version
from conan.errors import ConanInvalidConfiguration, ConanException
from conan.tools.build import check_min_cppstd
import subprocess

class NCBIToolkitWithConanRecipe(ConanFile):
    name = "ncbi-cxx-toolkit"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "VirtualBuildEnv", "VirtualRunEnv"
    options = {
        "with_req": ["ANY"]
    }
    default_options = {
        "with_req": ""
    }
    _req_map = {
        "AWS_SDK":    ["aws-sdk-cpp"],
        "BACKWARD":   ["backward-cpp"],
        "BerkeleyDB": ["libdb"],
        "BOOST":      ["boost"],
        "BZ2":        ["bzip2"],
        "CASSANDRA":  ["cassandra-cpp-driver"],
        "FASTCGI":    ["ncbi-fastcgi"],
        "FASTCGIPP":  ["ncbi-fastcgipp"],
        "GIF":        ["giflib"],
        "GRPC":       ["grpc", "protobuf", "abseil"],
        "Iconv":      ["libiconv"],
        "JPEG":       ["libjpeg"],
        "LMDB":       ["lmdb"],
        "LZO":        ["lzo"],
        "NCBICRYPT":  ["ncbicrypt"],
        "NGHTTP2":    ["libnghttp2"],
        "OPENTELEMETRY": ["opentelemetry-cpp"],
        "PCRE":       ["pcre"],
        "PCRE2":      ["pcre2"],
        "PNG":        ["libpng"],
        "PROTOBUF":   ["protobuf"],
        "SQLITE3":    ["sqlite3"],
        "TIFF":       ["libtiff"],
        "UNWIND":     ["libunwind"],
        "UV":         ["libuv"],
        "VDB":        ["ncbi-vdb"],
        "wxWidgets":  ["wxwidgets"],
        "XML":        ["libxml2"],
        "XSLT":       ["libxslt"],
        "Z":          ["zlib"],
        "ZSTD":       ["zstd"]
    }
    _req = None

    @property
    def _min_cppstd(self):
        return 20


# _default_requires:  enabled by default, can be disabled by request
# _optional_requires: disabled by default, can be enabled by request
# _internal_requires: by default, enabled if found in remotes or local cache,
#                     can be enabled or disabled by request
    def requirements(self):
        res = subprocess.run(["conan", "remote", "list"], 
            stdout = subprocess.PIPE, universal_newlines = True, encoding="utf-8")
        pos = res.stdout.find("ncbi.nlm.nih.gov")
        NCBIfound = pos > 0
        if NCBIfound:
            print("NCBI artifactory is found")
        else:
            print("NCBI artifactory is not found")

        self._default_requires("abseil/[>=20230125.3 <=20240116.2]")
        self._optional_requires("aws-sdk-cpp/[>=1.9.234 <=1.11.352]")
        if self.settings.os == "Linux":
            self._default_requires("backward-cpp/1.6")
        self._default_requires("boost/[>=1.82.0 <=1.86.0]")
        self._default_requires("bzip2/1.0.8")
        if self.settings.os == "Linux":
            self._default_requires("cassandra-cpp-driver/[>=2.15.3 <=2.16.2]")
        self._default_requires("giflib/[>=5.2.1 <=5.2.2]")
        self._default_requires("grpc/[>=1.50.1 <=1.67.1]")
        if self.settings.os == "Linux" or NCBIfound:
            self._default_requires("libdb/5.3.28")
        self._default_requires("libiconv/[>=1.17]")
        self._default_requires("libjpeg/9e")
        self._default_requires("libnghttp2/[>=1.51.0 <=1.61.0]")
        self._default_requires("libpng/[>=1.6.37 <=1.6.44]")
        self._default_requires("libtiff/[>=4.3.0 <=4.6.0]")
        if self.settings.os == "Linux":
            self._default_requires("libunwind/[>=1.6.2 <=1.8.1]")
        self._default_requires("libuv/[>=1.45.0 <=1.49.2]")
        self._default_requires("libxml2/[>=2.11.4 <3]")
        self._default_requires("libxslt/[>=1.1.34 <=1.1.42]")
        self._default_requires("lmdb/[>=0.9.29 <=0.9.32]")
        self._default_requires("lzo/2.10")
        self._optional_requires("opentelemetry-cpp/[>=1.14.2 <=1.17.0]")
        self._default_requires("pcre/8.45")
        self._default_requires("pcre2/10.42")
        self._default_requires("protobuf/[>=3.21.12 <=5.27.0]")
        self._default_requires("sqlite3/[>=3.40.0 <=3.47.1]")
        self._optional_requires("wxwidgets/3.2.6")
        self._default_requires("zlib/[>=1.2.11 <2]")
        self._default_requires("zstd/[>=1.5.2 <=1.5.6]")

        self._internal_requires("ncbicrypt/20230516")
        if self.settings.os == "Linux":
            self._internal_requires("ncbi-fastcgi/2.4.2")
            self._internal_requires("ncbi-fastcgipp/[>=3.1.0.4]")
            self._internal_requires("libcurl/8.8.0")
        self._internal_requires("ncbi-vdb/[>=3.0.1 <=3.2.1]")


    def configure(self):
        self.options["abseil/*"].shared = False
        self.options["grpc/*"].shared = False
        self.options["protobuf/*"].shared = False
        self.options["boost/*"].shared = False
        self.options["ncbicrypt/*"].shared = False
#
        _s = "/*" if conan_version.major > "1" else ""
        self.options["opentelemetry-cpp"+_s].with_otlp_grpc = True
        self.options["opentelemetry-cpp"+_s].with_jaeger = False
# hyphens make it tricky
        setattr(self.options["aws-sdk-cpp"], "text-to-speech", False), 


    def _parse_option(self, data):
        _res = set()
        if data != "":
            _data = str(data)
            _data = _data.replace(",", ";")
            _data = _data.replace(" ", ";")
            _res.update(_data.split(";"))
            if "" in _res:
                _res.remove("")
        return _res

    @property
    def _tk_req(self):
        if self._req is None:
            self._req = self._parse_option(self.options.with_req)
        return self._req

    def _is_enabled(self, package, default):
        _result = default
        pkg = package[:package.find("/")]
        for key in self._req_map.keys():
            if pkg in self._req_map[key]:
                if key in self._tk_req:
                    _result = True
                if "-"+key in self._tk_req:
                    _result = False
                if _result != default:
                    break
        return _result

    def _do_requires(self, package, enabled):
        if enabled:
            print("required: ", package)
            self.requires(package)
        else:
            print("disabled: ", package)

    def _default_requires(self, package):
        self._do_requires(package, self._is_enabled(package, True))

    def _optional_requires(self, package):
        self._do_requires(package, self._is_enabled(package, False))

    def _internal_requires(self, package):
        pkg = package[:package.find("/")]
# local cache first
        res = subprocess.run(["conan", 
            "list" if conan_version.major > "1" else "search", pkg], 
            stdout = subprocess.PIPE, universal_newlines = True, encoding="utf-8")
        pos = res.stdout.find(pkg+"/")
        if pos <= 0:
# remotes
            res = subprocess.run(["conan", 
                "list" if conan_version.major > "1" else "search", pkg, "-r",
                "*"    if conan_version.major > "1" else "all"], 
                stdout = subprocess.PIPE, universal_newlines = True, encoding="utf-8")
            pos = res.stdout.find(pkg+"/")
        if pos < 0:
            print("NOT FOUND optional package: " + package)
        self._do_requires(package, self._is_enabled(package, pos >= 0))


    def validate(self):
        if self.settings.compiler.cppstd:
            check_min_cppstd(self, self._min_cppstd)
        if self.settings.os not in ["Linux", "Macos", "Windows"]:
            raise ConanInvalidConfiguration("This operating system is not supported")
