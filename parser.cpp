#include "equation.hpp"
#include "parse.hpp"
#include <iostream>
#include <fstream>

void usage(char* name){
	std::cerr<<"Usage: "<<name<<" [options] filename"<<std::endl;
	exit(-1);
}
int main(int argc, char** argv){
	char opt=0;
	bool print_bindings=false;
	// Parse the arguments
	while ((opt = getopt(argc, argv, "B?")) != -1)
		switch (opt){
		case 'B':
			print_bindings=true;
			break;
		case '?':
		default:
			usage(argv[0]);
		}
	if (optind != argc-1)
		usage(argv[0]);

	(void) print_bindings;
	try{
		std::ifstream file(argv[optind]);
		std::string formula;
		std::getline(file,formula);
		Equation eq = parse_formula(formula);
		std::cout<<eq<<std::endl;
		if(print_bindings){
			auto [u,b,m] = bindings(eq);
			std::cout<<std::endl;

			// u|std::transform(.value)|join(",")
			std::cout<<"Unbound: ";
			if(u.size()){
				std::cout<<u.front();
				for(size_t i=1;i<u.size();i++)
					std::cout<<","<<u[i];
			}
			std::cout<<std::endl;

			std::cout<<"Bound: ";
			if(b.size()){
				std::cout<<b.front();
				for(size_t i=1;i<b.size();i++)
					std::cout<<","<<b[i];
			}
			std::cout<<std::endl;

			std::cout<<"Mixed Use: ";
			if(m.size()){
				std::cout<<m.front();
				for(size_t i=1;i<m.size();i++)
					std::cout<<","<<m[i];
			}
			std::cout<<std::endl;
		}
	}catch(const char* e){
		std::cerr<<"[ERROR] Caught unhandled Exception: \""<<e<<"\""<<std::endl;
		std::cout<<"Parse Error"<<std::endl;
	}

	return 0;
}
