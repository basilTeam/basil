SRCS := $(wildcard *.cpp) \
		$(wildcard compiler/*.cpp) \
		$(wildcard util/*.cpp) \
		$(wildcard jasmine/*.cpp)
OBJS := $(patsubst %.cpp,%.o,$(SRCS))

CXX := clang++
CXXHEADERS := -I. -Iutil -Ijasmine -Icompiler
CXXFLAGS := $(CXXHEADERS) -std=c++11 -Os -ffast-math -fno-rtti -fno-exceptions -Wno-null-dereference
LDFLAGS :=  -Wl,--unresolved-symbols=ignore-in-object-files

clean:
	rm -f $(OBJS) *.o.tmp basil

basil: CXXFLAGS += -g3

release: CXXFLAGS += -Os

basil: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

release: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o basil

librt.a: runtime/core.o runtime/sys.o
	ar r $@ $^

runtime/%.o: runtime/%.cpp
	$(CXX) -I. -Iutil -std=c++11 -fPIC -Os -ffast-math -fno-rtti -fno-exceptions -Os -nostdlib -nostdlib++ -c $< -o $@

%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -c $< -o $@
