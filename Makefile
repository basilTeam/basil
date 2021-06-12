SRCS := $(wildcard *.cpp) \
		$(wildcard compiler/*.cpp) \
		$(wildcard util/*.cpp) \
		$(wildcard jasmine/*.cpp)
OBJS := $(patsubst %.cpp,%.o,$(SRCS))

TEST_SRCS := $(wildcard test/**/*.cpp)
TEST_OBJS := $(patsubst %.cpp,%.test,$(TEST_SRCS))

CXX := clang++
CXXHEADERS := -I. -Iutil -Ijasmine -Icompiler -Itest
SHAREDFLAGS := $(CXXHEADERS) -std=c++17 \
	-ffunction-sections -fdata-sections -ffast-math -fno-rtti \
	-Wall -Werror -Wno-unused -Wno-comment -Wno-implicit-exception-spec-mismatch \
	-DINCLUDE_UTF8_LOOKUP_TABLE
	
%.test: SHAREDFLAGS += -g3
test: SHAREDFLAGS += -g3

basil: SHAREDFLAGS += -g3

release: SHAREDFLAGS += -Os -DBASIL_RELEASE

CXXFLAGS = $(SHAREDFLAGS) -fno-exceptions -nostdlib++ 
TESTFLAGS = $(SHAREDFLAGS)
LDFLAGS := -Wl,--gc-sections

clean:
	rm -f $(OBJS) $(TEST_OBJS) *.o.tmp basil librt.a runtime/*.o

basil: $(OBJS) librt.a
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJS) -o $@

release: $(OBJS) librt.a
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJS) -o basil

librt.a: runtime/core.o runtime/sys.o
	ar r $@ $^

runtime/%.o: runtime/%.cpp
	$(CXX) -I. -Iutil -std=c++11 -fPIC -ffast-math -fno-rtti -fno-exceptions -Os -nostdlib -nostdlib++ -c $< -o $@

%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PRECIOUS: %.test # don't delete tests if they run and error (we might want to run valgrind or gdb)

%.test: %.cpp $(OBJS)
	@$(CXX) $(TESTFLAGS) $< test/test.cpp $(LDFLAGS) $(OBJS) -o $@
	@$@

test: $(TEST_OBJS)
