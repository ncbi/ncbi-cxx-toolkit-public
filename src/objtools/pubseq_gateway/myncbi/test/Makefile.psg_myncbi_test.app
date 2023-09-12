APP = psg_myncbi_test
SRC = psg_myncbi_test unit/myncbi_factory

WATCHERS = saprykin
REQUIRES = MT Linux LIBUV LIBXML LIBXSLT

#COVERAGE_FLAGS=-fprofile-arcs -ftest-coverage
CPPFLAGS=$(GMOCK_INCLUDE) $(CURL_INCLUDE) $(LIBUV_INCLUDE) $(OPENSSL_INCLUDE) $(LIBXML_INCLUDE) $(LIBXSLT_INCLUDE) $(ORIG_CPPFLAGS) $(COVERAGE_FLAGS)

MY_LIB=psg_myncbi xmlwrapp xconnext connext xconnect
LIB=$(MY_LIB:%=%$(STATIC)) $(LOCAL_LIB) xncbi

LIBS = $(NETWORK_LIBS) $(GMOCK_LIBS) $(LIBUV_STATIC_LIBS) $(CURL_LIBS) $(OPENSSL_LIBS) $(LIBXML_LIBS) $(LIBXSLT_LIBS) $(ORIG_LIBS)

LDFLAGS = $(ORIG_LDFLAGS) $(FAST_LDFLAGS) $(COVERAGE_FLAGS) $(LOCAL_LDFLAGS)

#EXTRA=-fno-omit-frame-pointer -fsanitize=address -fsanitize=undefined -fsanitize=leak
LOCAL_CPPFLAGS += $(EXTRA) -fno-delete-null-pointer-checks
LOCAL_LDFLAGS += $(EXTRA)

#CHECK_CMD = psg_myncbi_test
