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
				// TODO: Instead of short cutting, loop.  This will save the
				// stack frames from needing remaking and can be made smarter
				// to only rework the modified sub-parts.  It'll also prevent
				// rescanning the whole tree when a part near the end is
				// changed.
				if(simplify_inplace(*eq.left))
					Changed();
				if(simplify_inplace(*eq.right))
					Changed();

				#define Return_Swap() Return(Equation({op,*eq.right,*eq.left}))
				using Equation::Operator::ADD;
				using Equation::Operator::SUBTRACT;
				using Equation::Operator::MULTIPLY;
				using Equation::Operator::DIVIDE;

				// TODO: Use pattern matching to resyntax this.
				auto* vln = std::get_if<double>(&eq.left->value);
				auto* vrn = std::get_if<double>(&eq.right->value);
				auto* vlv = std::get_if<Equation::Variable>(&eq.left->value);
				auto* vrv = std::get_if<Equation::Variable>(&eq.right->value);
				auto* vle = std::get_if<Equation::Op_node>(&eq.left->value);
				auto* vre = std::get_if<Equation::Op_node>(&eq.right->value);
				auto op = eq.op;

				if(commutative(op) && vlv && vrn) Return_Swap();
				if(commutative(op) && !vle && vre) Return_Swap();
				if(commutative(op) && vlv && vrv && vlv->name > vrv->name) Return_Swap();
				if(commutative(op) && vle && vrv && vle->op==op)
					if(auto* vlerv = std::get_if<Equation::Variable>(&vle->right->value))
						if(vlerv->name > vrv->name)
							Return(Equation({op,Equation({op,*vle->left,*eq.right}),*vle->right}));
				if(commutative(op) && vle && vrn && vle->op==op)
					if(!std::get_if<double>(&vle->right->value))
							Return(Equation({op,Equation({op,*vle->left,*eq.right}),*vle->right}));
				switch(op){
				case ADD:
					//std::cerr<<"Debug: case ADD"<<std::endl;
					if(vln && *vln==0) Return(*eq.right);
					if(vrn && *vrn==0) Return(*eq.left);
					if(vln && vrn) Return(*vln+*vrn);
					break;
				case SUBTRACT:
					//std::cerr<<"Debug: case SUBTRACT"<<std::endl;
					if(vln && *vln==0) Return(Equation({MULTIPLY,Equation(-1),*eq.right}));
					if(vrn && *vrn==0) Return(*eq.left);
					if(vln && vrn) Return(*vln-*vrn);
					break;
				case MULTIPLY:
					//std::cerr<<"Debug: case MULTIPLY"<<std::endl;
					if(vln && *vln==0) Return(*vln);
					if(vln && *vln==1) Return(*eq.right);
					if(vln && vrn) Return(*vln**vrn);
					// if(vlv && vre) Return_Swap();
					// if(vle && vre) if both children are + or -, sort by variable, if both same var, sort by constant.
					if(vrn && *vrn==0) Return(*eq.right);
					if(vrn && *vrn==1) Return(*eq.left);
					// if(vle && vrv) check lex of it's right child
					break;
				case DIVIDE:
					//std::cerr<<"Debug: case DIVIDE"<<std::endl;
					if(vln && *vln==0) Return(*eq.left); // TODO: Check if 0/0 or add a condition
					//if(vln && *vln==1) ; TODO: 1/(1/x)=x
					if (vln && vrn) Return(*vln/ *vrn); // Integer optimization
					// if(vrn && *vrn==0) ; TODO: Infinity(unsigned) (undef if 0/0, add a condition)
					if(vrn && *vrn==1) Return(*eq.left);
					break;
				}
				NoChange();
			}
		},e.value);}

Equation simplify(Equation e) {
	// Loop until no changes
	while(simplify_inplace(e));
	return e;
}
