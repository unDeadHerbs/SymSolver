#include "simplify.hpp"
#include <optional>
#include <iostream> // for std::cerr
#include <sstream>
#include <type_traits>

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

			
			#define Changed() return true
			#define NoChange() return false
			// The =tmp= variable is because of smart pointer dereferencing
			// and the destructor being called in the assignment operation.
			// TODO: use std::decay<decltype(e)> or something
			// TODO: Sometimes the argument needs casting pre-call.  Why?
			#define Return(X) do{Equation tmp(X);e=tmp;Changed();}while(0)
			
			[](double){NoChange();},
			[](Equation::Variable){NoChange();},
			[&](Equation::Op_node& eq){
				if(simplify_inplace(*eq.left))
					Changed();
				if(simplify_inplace(*eq.right))
					Changed();

				auto* vln = std::get_if<double>(&eq.left->value);
				auto* vrn = std::get_if<double>(&eq.right->value);
				auto* vlv = std::get_if<Equation::Variable>(&eq.left->value);
				auto* vrv = std::get_if<Equation::Variable>(&eq.right->value);
				switch(eq.op){
				case Equation::Operator::ADD:
					//std::cerr<<"Debug: case ADD"<<std::endl;
					if (vln){
						if(*vln==0)	Return(*eq.right);
						if(vrn)
							Return(*vln+*vrn); // Integer optimization
					}if (vrn){
						if(*vrn==0) Return(*eq.left);
						;// TODO: move integers before variables (sort by complexity)
					}break;
				case Equation::Operator::SUBTRACT:
					//std::cerr<<"Debug: case SUBTRACT"<<std::endl;
					if (vln){
						if(*vln==0) Return(Equation({Equation::Operator::MULTIPLY,Equation(-1),*eq.right}));
						if (vrn)
							Return(*vln-*vrn); // Integer optimization
					}if (vrn)
						 if(*vrn==0) Return(*eq.left);
					break;
				case Equation::Operator::MULTIPLY:
					//std::cerr<<"Debug: case MULTIPLY"<<std::endl;
					if (vln){
						//std::cerr<<"Debug: left is v="<<v<<std::endl;
						//std::cerr<<"Debug: left is *v="<<*v<<std::endl;
						if(*vln==0) Return(*vln);
						if(*vln==1) Return(*eq.right);
						if (vrn)
							Return(*vln**vrn);
					}else if(vlv){
						if (vrn)
							Return(Equation({eq.op,*vrn,*vlv}));// if other is number, swap order
						if(vrv)
							if(vlv->name > vrv->name)
								Return(Equation({eq.op,*vrv,*vlv}));// if other is var, check lex order
						// if other is MULTIPLY, swap to have a left deep tree
					}else
						;// if both children are + or -, sort by variable, if both same var, sort by constant.
					if (vrn){
						if(*vrn==0) Return(*eq.right);
						if(*vrn==1) Return(*eq.left);
					}else if(vrv){
						// if other is var, check lex order
						// if other is MULTIPLY, check lex of it's right child
					}
					break;
				case Equation::Operator::DIVIDE:
					//std::cerr<<"Debug: case DIVIDE"<<std::endl;
					if (vln){
						if(*vln==0) Return(*eq.left); // TODO: Check if 0/0 or add a condition
						//if(*vln==1) ; TODO: 1/(1/x)=x
						if (vrn)
							Return(*vln/ *vrn); // Integer optimization
					}if (vrn){
						// if(*vrn==0) ; TODO: Infinity(unsigned) (undef if 0/0, add a condition)
						if(*vrn==1) Return(*eq.left);
					}
					break;
					// TODO: Other operators.
				}
				NoChange();
			}
		},e.value);}

Equation simplify(Equation e) {
	// Loop until no changes
	while(simplify_inplace(e));
	return e;
}
