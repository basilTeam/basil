SRCS := $(wildcard *.cpp) \
		$(wildcard compiler/*.cpp) \
		$(wildcard util/*.cpp) \
		$(wildcard jasmine/*.cpp)
OBJS := $(patsubst %.cpp,%.o,$(SRCS))

CXX := clang++
CXXHEADERS := -I. -Iutil -Ijasmine -Icompiler
CXXFLAGS := $(CXXHEADERS) -std=c++17 -ffunction-sections -fdata-sections -ffast-math -fno-rtti -fno-exceptions -Wno-null-dereference
LDFLAGS := -Wl,--gc-sections -Wl,--unresolved-symbols=ignore-in-object-files

clean:
	rm -f $(OBJS) *.o.tmp basil librt.a runtime/*.o

basil: CXXFLAGS += -g3

release: CXXFLAGS += -Os

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
