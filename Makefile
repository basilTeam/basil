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
	-fPIC -fomit-frame-pointer -fmerge-all-constants \
	-Wall -Wno-unused -Wno-comment -Wno-implicit-exception-spec-mismatch -Wno-strict-aliasing \
	-DINCLUDE_UTF8_LOOKUP_TABLE

release: SRCS += compiler/main.cpp
jasmine-release: JASMINE_SRCS += jasmine/main.cpp

OBJS := $(patsubst %.cpp,%.o,$(SRCS))
JASMINE_OBJS := $(patsubst %.cpp,%.o,$(JASMINE_SRCS))

%.test: SHAREDFLAGS += -g3
test: SHAREDFLAGS += -g3

basil: SHAREDFLAGS += -g3
release: SHAREDFLAGS += -Os -DBASIL_RELEASE -fno-unwind-tables -fno-asynchronous-unwind-tables
jasmine: SHAREDFLAGS += -g3
jasmine-release: SHAREDFLAGS += -Os -DBASIL_RELEASE -fno-unwind-tables -fno-asynchronous-unwind-tables

CXXFLAGS = $(SHAREDFLAGS) -fno-exceptions -fno-threadsafe-statics -nodefaultlibs
TESTFLAGS = $(SHAREDFLAGS)
LDFLAGS := -Wl,--gc-sections -lc -lm

build:
	mkdir -p build

clean:
	rm -rf $(OBJS) $(TEST_OBJS) *.o.tmp basil librt.a runtime/*.o build/

jasmine: $(JASMINE_OBJS) jasmine/main.o build
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(JASMINE_OBJS) jasmine/main.o -o build/$@

jasmine-release: $(JASMINE_SRCS) build
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(JASMINE_SRCS) -o build/jasmine
	strip -g -R .gnu.version \
			 -R .gnu.hash \
			 -R .note \
			 -R .note.ABI-tag \
			 -R .note.gnu.build-id \
			 -R .comment \
			 --strip-unneeded build/jasmine

basil: $(OBJS) compiler/main.o librt.a build
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJS) compiler/main.o -o build/$@

release: $(SRCS) librt.a build
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SRCS) -o build/basil
	strip -g -R .gnu.version \
			 -R .gnu.hash \
			 -R .note \
			 -R .note.ABI-tag \
			 -R .note.gnu.build-id \
			 -R .comment \
			 --strip-unneeded build/basil

librt.a: build runtime/core.o runtime/sys.o
	ar r build/$@ $(filter-out build,$^)

runtime/%.o: runtime/%.cpp
	$(CXX) -I. -Iutil -std=c++11 -fPIC -ffast-math -fno-rtti -fno-exceptions -Os -nodefaultlibs -c $< -o $@

%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PRECIOUS: %.test # don't delete tests if they run and error (we might want to run valgrind or gdb)

%.test: %.cpp $(OBJS)
	@$(CXX) $(TESTFLAGS) $< test/test.cpp $(LDFLAGS) $(OBJS) -o $@
	@$@

test: $(TEST_OBJS)
