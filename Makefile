SRCS := $(wildcard *.cpp) \
		$(wildcard compiler/*.cpp) \
		$(wildcard util/*.cpp) \
		$(wildcard jasmine/*.cpp)
OBJS := $(patsubst %.cpp,%.o,$(SRCS))

CXX := clang++
CXXHEADERS := -I. -Iutil -Ijasmine -Icompiler
CXXFLAGS := $(CXXHEADERS) -std=c++17 -ffast-math -fno-rtti -fno-exceptions -Wno-null-dereference -Wl,--unresolved-symbols=ignore-in-object-files

clean:
	rm -f $(OBJS) *.o.tmp basil

basil: CXXFLAGS += -g3

release: CXXFLAGS += -Os

basil: $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

release: $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o basil

%.o: %.cpp %.h
	$(CXX) $(CXXFLAGS) -c $< -o $@
