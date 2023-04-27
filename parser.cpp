#include "equation.hpp"
#include "parse.hpp"
#include <iostream>
#include <fstream>

void usage(char* name){
	std::cerr<<"Usage: "<<name<<" filename"<<std::endl;
	exit(-1);
}
int main(int argc, char** argv){
	char opt=0;
	// Parse the "arguments"
	while ((opt = getopt(argc, argv, "")) != -1)
		switch (opt){
		default: /* ’?’ */
			usage(argv[0]);
		}
	if (optind != argc-1)
		usage(argv[0]);

	try{
		std::ifstream file(argv[optind]);
		std::string formula;
		std::getline(file,formula);
		Equation eq= parse_formula(formula);
		std::cout<<eq<<std::endl;
	}catch(const char* e){
		std::cerr<<"[ERROR] Caught unhandled Exception: \""<<e<<"\""<<std::endl;
	}

	return 0;
}
