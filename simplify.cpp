#include "simplify.hpp"
#include <optional>
#include <iostream> // for std::cerr
#include <sstream>

// Copied from cppreference, not sure why its not in the std namespace already.
// This lets std::visit take a set of lambdas.
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

bool simplify_inplace(Equation& e) {
	return std::visit(overloaded{
			// There are a lot of efficiency gains to be made by walking the
			// tree in a smarter fashion.  Namely, finding constants (mostly
			// 1s and 0s) and walking their optimizations up the tree; but,
			// those will be very unusual cases in real examples (rather
			// than worst case or random case) and so the overhead of
			// walking more "intelligently" will[fn::Citation Needed] cost
			// more than it saves.
			[](double){return false;},
			[](Equation::Variable){return false;},
			[&](Equation::Op_node& eq){
				if(simplify_inplace(*eq.left))
					return true;
				if(simplify_inplace(*eq.right))
					return true;

				switch(eq.op){
					// The =tmp= variables are because of smart pointer dereferencing.
				case Equation::Operator::ADD:
					//std::cerr<<"Debug: case ADD"<<std::endl;
					if (auto* v = std::get_if<double>(&eq.left->value))
						if(*v==0){
							auto tmp=*eq.right;
							e=tmp;
							return true;}
						else if (auto* v2 = std::get_if<double>(&eq.right->value)){
							e={*v+*v2}; // Integer optimization
							return true;}
					if (auto* v = std::get_if<double>(&eq.right->value))
						if(*v==0){
							auto tmp=*eq.left;
							e=tmp;
							return true;}
						else
							;// TODO: move integers before variables (sort by complexity)
					break;
				case Equation::Operator::SUBTRACT:
					//std::cerr<<"Debug: case SUBTRACT"<<std::endl;
					if (auto* v = std::get_if<double>(&eq.left->value))
						if(*v==0){
							eq={Equation::Operator::MULTIPLY,Equation(-1),*eq.right};return true;}
						else
							if (auto* v2 = std::get_if<double>(&eq.right->value)){
								e={*v-*v2}; // Integer optimization
								return true;}
					if (auto* v = std::get_if<double>(&eq.right->value))
						if(*v==0){
							auto tmp=*eq.left;
							e=tmp;
							return true;}
				case Equation::Operator::MULTIPLY:
					//std::cerr<<"Debug: case MULTIPLY"<<std::endl;
					if (auto* v = std::get_if<double>(&eq.left->value)){
						//std::cerr<<"Debug: left is v="<<v<<std::endl;
						//std::cerr<<"Debug: left is *v="<<*v<<std::endl;
						if((*v)==0){
							auto tmp=*eq.left;
							e=tmp;
							return true;
						}else if(*v==1){
							auto tmp =*eq.right;
							e=tmp;
							return true;
						}else
							if (auto* v2 = std::get_if<double>(&eq.right->value)){
								e={*v**v2}; // Integer optimization
								return true;}
					}else
						; // TODO: Sort Variables, convert to exponents, etc.
					if (auto* v = std::get_if<double>(&eq.right->value))
						if(*v==0){
							auto tmp=*eq.right;
							e=tmp;return true;}
						else if(*v==1){
							auto tmp=*eq.left;
							e=tmp;return true;}
				case Equation::Operator::DIVIDE:
					//std::cerr<<"Debug: case DIVIDE"<<std::endl;
					if (auto* v = std::get_if<double>(&eq.left->value))
						if(*v==0){
							auto tmp=*eq.left;
							e=tmp;return true;}
						else if(*v==1)
							;// TODO: 1/(1/x)=x
						else
							if (auto* v2 = std::get_if<double>(&eq.right->value)){
								e={*v/ *v2}; // Integer optimization
								return true;}
					if (auto* v = std::get_if<double>(&eq.right->value))
						if(*v==0) ;//{NAN;return true;}
						else if(*v==1){
							auto tmp=*eq.left;
							e=tmp;return true;}
					// TODO: Other operators.
				}
				return false;
			}
		},e.value);}

Equation simplify(Equation e) {
	// TODO: Loop until no changes?
	while(simplify_inplace(e));
	return e;
}
