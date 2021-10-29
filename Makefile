SRCS := $(wildcard *.cpp) \
		$(filter-out compiler/main.cpp,$(wildcard compiler/*.cpp)) \
		$(wildcard util/*.cpp) \
		$(filter-out jasmine/main.cpp,$(wildcard jasmine/*.cpp))
		
JASMINE_SRCS := $(wildcard *.cpp) \
		$(wildcard util/*.cpp) \
		$(filter-out jasmine/main.cpp,$(wildcard jasmine/*.cpp))

TEST_SRCS := $(wildcard test/**/*.cpp)
TEST_OBJS := $(patsubst %.cpp,%.test,$(TEST_SRCS))

CXX := clang++
CXXHEADERS := -I. -Iutil -Ijasmine -Icompiler -Itest
SHAREDFLAGS := $(CXXHEADERS) -std=c++17 \
	-ffunction-sections -fdata-sections -ffast-math -fno-rtti -finline-functions \
	-fPIC -fomit-frame-pointer -fmerge-all-constants -fno-exceptions \
	-Wall -Wno-unused -Wno-comment \
	-DINCLUDE_UTF8_LOOKUP_TABLE

ifeq '$(findstring ;,$(PATH))' ';'
    OS := Windows
else
    OS := $(shell uname 2>/dev/null || echo Unknown)
    OS := $(patsubst CYGWIN%,Cygwin,$(OS))
    OS := $(patsubst MSYS%,MSYS,$(OS))
    OS := $(patsubst MINGW%,MinGW,$(OS))
endif

ifeq (${OS}, Linux)
	LDFLAGS := -nodefaultlibs -Wl,--gc-sections
	LDLIBS := -lc -lm
endif

ifeq (${OS}, MSYS)
	# LDLIBS := -lc++abi -lm -lmingw32 -lmoldname -lmingwex -lmsvcrt -ladvapi32 -lshell32 -luser32 -lkernel32 -lmingw32
	# LDFLAGS += -L/mingw64/lib
endif

ifeq (${OS}, MinGW)
	# LDLIBS := -lc++abi -lm -lmingw32 -lmoldname -lmingwex -lmsvcrt -ladvapi32 -lshell32 -luser32 -lkernel32 -lmingw32
	# LDFLAGS += -L/mingw64/lib
endif

release: SRCS += compiler/main.cpp
jasmine-release: JASMINE_SRCS += jasmine/main.cpp

OBJS := $(patsubst %.cpp,%.o,$(SRCS))
JASMINE_OBJS := $(patsubst %.cpp,%.o,$(JASMINE_SRCS))

%.test: SHAREDFLAGS += -g3
test: SHAREDFLAGS += -g3

basil: SHAREDFLAGS += -g3 -O0
release: SHAREDFLAGS += -Os -DBASIL_RELEASE -fno-unwind-tables -fno-asynchronous-unwind-tables
jasmine: SHAREDFLAGS += -g3 -O0
jasmine-release: SHAREDFLAGS += -Os -DBASIL_RELEASE -fno-unwind-tables -fno-asynchronous-unwind-tables

CXXFLAGS = $(SHAREDFLAGS) -fno-exceptions -fno-threadsafe-statics 
TESTFLAGS = $(SHAREDFLAGS)

build:
	mkdir -p bin

clean:
	rm -rf $(OBJS) $(TEST_OBJS) *.o.tmp basil librt.a runtime/*.o bin/

jasmine: $(JASMINE_OBJS) jasmine/main.o build
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(JASMINE_OBJS) jasmine/main.o -o bin/$@ $(LDLIBS)

jasmine-release: $(JASMINE_SRCS) build
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(JASMINE_SRCS) -o bin/jasmine $(LDLIBS)
	strip -g -R .gnu.version \
			 -R .gnu.hash \
			 -R .note \
			 -R .note.ABI-tag \
			 -R .note.gnu.build-id \
			 -R .comment \
			 --strip-unneeded bin/jasmine

basil: $(OBJS) compiler/main.o librt.a build
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJS) compiler/main.o -o bin/$@ $(LDLIBS)

release: $(SRCS) librt.a build
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SRCS) -o bin/basil $(LDLIBS)
	strip -g -R .gnu.version \
			 -R .gnu.hash \
			 -R .note \
			 -R .note.ABI-tag \
			 -R .note.gnu.build-id \
			 -R .comment \
			 --strip-unneeded bin/basil

librt.a: build runtime/core.o runtime/sys.o
	ar r bin/$@ $(filter-out build,$^)

runtime/%.o: runtime/%.cpp
	$(CXX) -I. -Iutil -std=c++11 -fPIC -ffast-math -fno-rtti -fno-exceptions -Os -nodefaultlibs -c $< -o $@

%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PRECIOUS: %.test # don't delete tests if they run and error (we might want to run valgrind or gdb)

%.test: %.cpp $(OBJS)
	@$(CXX) $(TESTFLAGS) $< test/test.cpp $(LDFLAGS) $(OBJS) -o $@ $(LDLIBS)
	@$@

test: $(TEST_OBJS)
