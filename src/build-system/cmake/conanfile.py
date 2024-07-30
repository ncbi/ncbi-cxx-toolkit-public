from conan import ConanFile, conan_version
from conan.errors import ConanInvalidConfiguration, ConanException
from conan.tools.build import check_min_cppstd
import subprocess

class NCBIToolkitWithConanRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "VirtualBuildEnv", "VirtualRunEnv"

    @property
    def _min_cppstd(self):
        return 17

    def requirements(self):
        res = subprocess.run(["conan", "remote", "list"], 
            stdout = subprocess.PIPE, universal_newlines = True, encoding="utf-8")
        pos = res.stdout.find("ncbi.nlm.nih.gov")
        NCBIfound = pos > 0
        if NCBIfound:
            print("NCBI artifactory is found")
        else:
            print("NCBI artifactory is not found")

        if self.settings.os == "Linux":
            self.requires("backward-cpp/1.6")
        self.requires("boost/[>=1.82.0 <=1.85.0]")
        self.requires("bzip2/1.0.8")
        if self.settings.os == "Linux":
            self.requires("cassandra-cpp-driver/[>=2.15.3 <=2.17.1]")
        self.requires("giflib/[>=5.2.1 <=5.2.2]")
        self.requires("grpc/1.50.1")
        if self.settings.os == "Linux" or NCBIfound:
            self.requires("libdb/5.3.28")
        self.requires("libjpeg/9e")
        self.requires("libnghttp2/[>=1.51.0 <=1.61.0]")
        self.requires("libpng/[>=1.6.37 <=1.6.43]")
        self.requires("libtiff/[>=4.3.0 <=4.6.0]")
        if self.settings.os == "Linux":
            self.requires("libunwind/[>=1.6.2 <=1.8.1]")
        self.requires("libuv/[>=1.45.0 <=1.48.0]")
        self.requires("libxml2/[>=2.11.4 <=2.11.6]")
        self.requires("libxslt/[>=1.1.34 <=1.1.37]")
        self.requires("lmdb/[>=0.9.29 <=0.9.32]")
        self.requires("lzo/2.10")
        self.requires("openssl/1.1.1s")
        # self.requires("opentelemetry-cpp/1.14.2")
        self.requires("pcre/8.45")
        self.requires("pcre2/10.44")
        self.requires("protobuf/[>=3.21.12 <=5.27.0]")
        self.requires("sqlite3/[>=3.40.0 <=3.46.0]")
        self.requires("zlib/[>=1.2.11 <2]")
        self.requires("zstd/[>=1.5.2 <=1.5.5]")

        self._optional_requires("ncbicrypt/20230516")
        if self.settings.os == "Linux":
            self._optional_requires("ncbi-fastcgi/2.4.2")
        self._optional_requires("ncbi-vdb/[>=3.0.1 <=3.1.1]")

    def configure(self):
        self.options["abseil/*"].shared = False
        self.options["grpc/*"].shared = False
        self.options["protobuf/*"].shared = False
        self.options["boost/*"].shared = False
        self.options["libxml2/*"].shared = False
        self.options["libiconv/*"].shared = False
        self.options["pcre/*"].shared = False
        self.options["pcre2/*"].shared = False
        self.options["ncbicrypt/*"].shared = False
        self.options["googleapis/*"].shared = False
        self.options["grpc-proto/*"].shared = False
#
        _s = "/*" if conan_version.major > "1" else ""
        self.options["libunwind"+_s].minidebuginfo = False

#boost/*:header_only = True
        self.options["boost"+_s].header_only = False
#boost/*:without_atomic = True
#boost/*:without_chrono = True
#boost/*:without_container = True
#boost/*:without_context = True
        self.options["boost"+_s].without_contract = True
        self.options["boost"+_s].without_coroutine = True
#boost/*:without_date_time = True
#boost/*:without_exception = True
        self.options["boost"+_s].without_fiber = True
#boost/*:without_filesystem = True
        self.options["boost"+_s].without_graph = True
        self.options["boost"+_s].without_graph_parallel = True
#boost/*:without_iostreams = True
        self.options["boost"+_s].without_json = True
        self.options["boost"+_s].without_locale = True
        self.options["boost"+_s].without_log = True
        self.options["boost"+_s].without_math = True
        self.options["boost"+_s].without_mpi = True
        self.options["boost"+_s].without_nowide = True
        self.options["boost"+_s].without_program_options = True
        self.options["boost"+_s].without_python = True
#boost/*:without_random = True
#boost/*:without_regex = True
#boost/*:without_serialization = True
        self.options["boost"+_s].without_stacktrace = True
#boost/*:without_system = True
#boost/*:without_test = True
#boost/*:without_thread = True
#boost/*:without_timer = True
        self.options["boost"+_s].without_type_erasure = True
        self.options["boost"+_s].without_wave = True

        self.options["opentelemetry-cpp"+_s].with_abseil = False
        # Not looking to use Jaeger, but the recipe might not provide
        # the option and catching e.g. AttributeError won't work.
        # self.options["opentelemetry-cpp"+_s].with_jaeger = False
        self.options["opentelemetry-cpp"+_s].with_otlp_grpc = True
        # otlp-http is already on by default
        # self.options["opentelemetry-cpp"+_s].with_otlp_http = True
        self.options["opentelemetry-cpp"+_s].with_zipkin = False

    def _optional_requires(self, package):
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
        if pos > 0:
            self.requires(package)
        else:
            print("NOT FOUND optional package: " + package)

    def validate(self):
        if self.settings.compiler.cppstd:
            check_min_cppstd(self, self._min_cppstd)
        if self.settings.os not in ["Linux", "Macos", "Windows"]:
            raise ConanInvalidConfiguration("This operating system is not supported")

