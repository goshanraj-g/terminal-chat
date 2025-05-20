CXX     := g++
CXXFLAGS:= -std=c++20 -Wall -pthread

all: server client
server: server.cpp
	$(CXX) $(CXXFLAGS) $< -o $@
client: client.cpp
	$(CXX) $(CXXFLAGS) $< -o $@
clean:
	rm -f server client