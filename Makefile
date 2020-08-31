SRCS := $(wildcard *.cpp) $(wildcard util/*.cpp) $(wildcard jasmine/*.cpp)
OBJS := $(patsubst %.cpp,%.o,$(SRCS))

CXX := clang++
CXXHEADERS := -I. -Iutil -Ijasmine
CXXFLAGS := $(CXXHEADERS) -std=c++17 -ffast-math -fno-rtti -fno-exceptions -ffunction-sections -Wno-null-dereference

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
