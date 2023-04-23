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

void simplify_inplace(Equation& e) {
	// Bigger TODOs:
	// - rationalize denominator
	// - sqrt(a)/sqrt(b) -> sqrt(a/b)
	// - log rules
	std::visit(overloaded{
			[](double){},
			[](Equation::Variable){},
			[&](Equation::Op_node& eq){
				simplify_inplace(*eq.left);
				simplify_inplace(*eq.right);
				switch(eq.op){
					// The =tmp= variables are because of smart pointer dereferencing.
				case Equation::Operator::ADD:
					//std::cerr<<"Debug: case ADD"<<std::endl;
					if (auto* v = std::get_if<double>(&eq.left->value))
						if(*v==0){
							auto tmp=*eq.right;
							e=tmp;
							return;}
						else if (auto* v2 = std::get_if<double>(&eq.right->value)){
							e={*v+*v2}; // Integer optimization
							return;}
					if (auto* v = std::get_if<double>(&eq.right->value))
						if(*v==0){
							auto tmp=*eq.left;
							e=tmp;
							return;}
						else
							;// TODO: move integers before variables (sort by complexity)
					break;
				case Equation::Operator::SUBTRACT:
					//std::cerr<<"Debug: case SUBTRACT"<<std::endl;
					if (auto* v = std::get_if<double>(&eq.left->value))
						if(*v==0){
							eq={Equation::Operator::MULTIPLY,Equation(-1),*eq.right};return;}
						else
							if (auto* v2 = std::get_if<double>(&eq.right->value)){
								e={*v-*v2}; // Integer optimization
								return;}
					if (auto* v = std::get_if<double>(&eq.right->value))
						if(*v==0){
							auto tmp=*eq.left;
							e=tmp;
							return;}
				case Equation::Operator::MULTIPLY:
					//std::cerr<<"Debug: case MULTIPLY"<<std::endl;
					if (auto* v = std::get_if<double>(&eq.left->value)){
						//std::cerr<<"Debug: left is v="<<v<<std::endl;
						//std::cerr<<"Debug: left is *v="<<*v<<std::endl;
						if((*v)==0){
							auto tmp=*eq.left;
							e=tmp;
							return;
						}else if(*v==1){
							auto tmp =*eq.right;
							e=tmp;
							return;
						}else
							if (auto* v2 = std::get_if<double>(&eq.right->value)){
								e={*v**v2}; // Integer optimization
								return;}
					}else
						; // TODO: Sort Variables, convert to exponents, etc.
					if (auto* v = std::get_if<double>(&eq.right->value))
						if(*v==0){
							auto tmp=*eq.right;
							e=tmp;return;}
						else if(*v==1){
							auto tmp=*eq.left;
							e=tmp;return;}
				case Equation::Operator::DIVIDE:
					//std::cerr<<"Debug: case DIVIDE"<<std::endl;
					if (auto* v = std::get_if<double>(&eq.left->value))
						if(*v==0){
							auto tmp=*eq.left;
							e=tmp;return;}
						else if(*v==1)
							;// TODO: 1/(1/x)=x
						else
							if (auto* v2 = std::get_if<double>(&eq.right->value)){
								e={*v/ *v2}; // Integer optimization
								return;}
					if (auto* v = std::get_if<double>(&eq.right->value))
						if(*v==0) ;//{NAN;return;}
						else if(*v==1){
							auto tmp=*eq.left;
							e=tmp;return;}
					// TODO: Other operators.
				}
			}
		},e.value);}

Equation simplify(Equation e) {
	simplify_inplace(e);
	return e;
}
