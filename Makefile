bfc: main.cpp
	g++ main.cpp `llvm-config --cxxflags --ldflags --libs` -ldl -lpthread -ltinfo -fexceptions -o bfc
