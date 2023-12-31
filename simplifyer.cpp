#include "equation.hpp"
#include "parse.hpp"
#include "simplify.hpp"
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

	std::ifstream file(argv[optind]);
	std::string formula;
	std::getline(file,formula);
	Equation eq= parse_formula(formula);
	eq=simplify(eq);
	std::cout<<eq<<std::endl;

	return 0;
}
