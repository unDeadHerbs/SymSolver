#include "simplify.hpp"
#include <optional>
#include <iostream> // for std::cerr
#include <sstream>
#include <type_traits>

//#define DEBUG 1
#ifdef DEBUG
#define DB(X) do{std::cerr<<X<<std::endl;}while(0)
#else
#define DB(X) do{}while(0)
#endif

// Copied from cppreference, not sure why its not in the std namespace already.
// This lets std::visit take a set of lambdas.
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// TODO: Make this a spaceship?
bool greater_than(Equation const& l,Equation const& r){
	// TODO: This "should" return invalid if the operators don't match;
	// but, I'll return false for now as the only use of this is in
	// swapping elements.  Another option is choosing "lower" operators
	// first.  If this is ordering by "complexity".
	auto* vln = std::get_if<double>(&l.value);
	auto* vrn = std::get_if<double>(&r.value);
	auto* vlv = std::get_if<Equation::Variable>(&l.value);
	auto* vrv = std::get_if<Equation::Variable>(&r.value);
	auto* vle = std::get_if<Equation::Op_node>(&l.value);
	auto* vre = std::get_if<Equation::Op_node>(&r.value);
	if(vln && vrn) return *vln > *vrn;
	if(vln && !vrn) return false;
	if(!vln && vrn) return true;
	if(vlv && vrv) return vlv->name > vrv->name;
	if(vlv && !vrv) return false;
	if(!vlv && vrv) return true;
	if(vle->op != vre->op) return precedent(vle->op)>precedent(vre->op);
	if(greater_than(*vle->left,*vre->left)) return true;
	if(not greater_than(*vre->left,*vle->left)) // Since I don't have an == operator here.
		return greater_than(*vle->right,*vre->right);
	return false;
}

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
			#define NoChange() do{return false;}while(0)
			// The =tmp= variable is because of smart pointer dereferencing
			// and the destructor being called in the assignment operation.
			// TODO: use std::decay<decltype(e)> or something
			// TODO: Sometimes the argument needs casting pre-call.  Why?
			#define Return(MSG,X) do{DB("Change: "<<MSG);Equation tmp(X);e=tmp;Changed();}while(0)
			
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

				DB(e);

				#define Return_Swap() Return("LR Swap",Equation({op,*eq.right,*eq.left}))
				using Equation::Operator::ADD;
				using Equation::Operator::SUBTRACT;
				using Equation::Operator::MULTIPLY;
				using Equation::Operator::DIVIDE;
				using Equation::Operator::EXPONENT;

				// TODO: Use pattern matching to resyntax this.
				auto* vln = std::get_if<double>(&eq.left->value);
				auto* vrn = std::get_if<double>(&eq.right->value);
				//auto* vlv = std::get_if<Equation::Variable>(&eq.left->value);
				//auto* vrv = std::get_if<Equation::Variable>(&eq.right->value);
				//auto* vle = std::get_if<Equation::Op_node>(&eq.left->value);
				auto* vre = std::get_if<Equation::Op_node>(&eq.right->value);
				decltype(vln) vrln;
				if(vre) vrln = std::get_if<double>(&vre->left->value);
				auto op = eq.op;

				//if(commutative(op) && vle && vle->op==op && greater_than(*vle->right,*eq.right))
				//	Return("LRwR Swap",Equation({op,Equation({op,*vle->left,*eq.right}),*vle->right}));
				if(commutative(op) && greater_than(*eq.left,*eq.right)) Return_Swap();
				if(commutative(op) && vre && vre->op==op && greater_than(*eq.left,*vre->left))
					Return("LwRL Swap",Equation({op,*vre->left,Equation({op,*eq.left,*vre->right})}));

				switch(op){
				case ADD:
					if(vln && *vln==0) Return("Left 0 Addition",*eq.right);
					if(vrn && *vrn==0) Return("Right 0 Addition",*eq.left);
					if(vln && vrn) Return("Constant Addition",*vln+*vrn);
					if(vln && vre && vrln) Return("Constant Addition 2",Equation({op,*vln+*vrln,*vre->right}));
					break;
				case SUBTRACT:
					if(vln && *vln==0) Return("Left 0 Subtraction",Equation({MULTIPLY,Equation(-1),*eq.right}));
					if(vrn && *vrn==0) Return("Right 0 Subtraction",*eq.left);
					if(vln && vrn) Return("Constant Subtraction",*vln-*vrn);
					if(!vln && vrn) Return("Subtract Constant Add Inverse",Equation({ADD,*eq.left,{-1**vrn}}));
					break;
				case MULTIPLY:
					if(vln && *vln==0) Return("Left 0 Multiplication",*vln);
					if(vln && *vln==1) Return("Left 1 Multiplication",*eq.right);
					if(vln && vrn) Return("Constant Multiplication",*vln**vrn);
					if(vrn && *vrn==0) Return("Right 0 Multiplication",*eq.right);
					if(vrn && *vrn==1) Return("Right 1 Multiplication",*eq.left);
					// if(vle && vrv) check lex of it's right child
					break;
				case DIVIDE:
					if(vrn && *vrn==1) Return("Right 1 Division",*eq.left);
					//if(vln && *vln==1 && vre && vre->op==op && vre->left==1) Return("Double Division",vre->right);
					if (vln && vrn) Return("Constant Division",*vln/ *vrn);
					if(vln && *vln==0) Return("Left 0 Division",*eq.left); // TODO: Add a 0/0 condition.
					break;
				case EXPONENT:
					break;
				}
				NoChange();
			},
			[&](Equation::F_node const&){
				NoChange();
			}
		},e.value);}

Equation simplify(Equation e) {
	// Loop until no changes
	while(simplify_inplace(e));
	return e;
}
