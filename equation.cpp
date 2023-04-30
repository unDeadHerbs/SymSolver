#include "equation.hpp"
#include <iostream>
#include <cmath>

// Copied from cppreference, not sure why its not in the std namespace already.
// This lets std::visit take a set of lambdas.
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;


// The two cast "operators" for the Operator type.
Equation::Operator constexpr Equation::to_Operator(char const c){
	switch(c){
	case '+': return Operator::ADD;
	case '-': return Operator::SUBTRACT;
	case '*': return Operator::MULTIPLY;
	case '/': return Operator::DIVIDE;
	case '^': return Operator::EXPONENT;
	default: throw "Invalid Operator"; // TODO: That's not very constexper of you.
	}
}

char constexpr Equation::to_sym(Operator const op){
	switch(op){
	case Operator::ADD: return '+';
	case Operator::SUBTRACT: return '-';
	case Operator::MULTIPLY: return '*';
	case Operator::DIVIDE: return '/';
	case Operator::EXPONENT: return '^';
	}
}

//
// Fixing the semantics of "unique_ptr" until I switch to a COW.
//
Equation::Op_node::Op_node(Operator const _op,Equation const& _l,Equation const& _r)
	:op(_op){
	left=std::make_unique<Equation>(_l); // Not sure why this isn't just the constructor
	right=std::make_unique<Equation>(_r);
}

Equation::Op_node::Op_node(char const _op,Equation const& _l,Equation const& _r)
	:op(Equation::to_Operator(_op)){
	left=std::make_unique<Equation>(_l); // Not sure why this isn't just the constructor
	right=std::make_unique<Equation>(_r);
}

Equation::Op_node::Op_node(Op_node const& eq)
	:op(eq.op){
	left=std::make_unique<Equation>(*eq.left); // Not sure why this isn't just the constructor
	right=std::make_unique<Equation>(*eq.right);
}

Equation::Op_node& Equation::Op_node::operator=(Equation::Op_node const& rhs){
	op=rhs.op;
	left=std::make_unique<Equation>(*rhs.left);
	right=std::make_unique<Equation>(*rhs.right);
	return *this;
}


// The actual things that belong in this file.
int precedent(Equation::Operator op){
	switch(op){
	case Equation::Operator::ADD: case Equation::Operator::SUBTRACT:
		return 1;
	case Equation::Operator::MULTIPLY: case Equation::Operator::DIVIDE:
		return 2;
	case Equation::Operator::EXPONENT:
		return 3;
	}}

bool commutative(Equation::Operator op){
	switch(op){
	case Equation::Operator::ADD:
	case Equation::Operator::MULTIPLY:
		return true;
	case Equation::Operator::SUBTRACT:
	case Equation::Operator::DIVIDE:
	case Equation::Operator::EXPONENT:
		return false;
	}}

bool right_binding(Equation::Operator op){
	// If this operator binds to it's right argument more strongly than
	// it's left, then `a/(b/c)` needs parenthesizes to not be
	// misinterpreted.
	switch(op){
	case Equation::Operator::ADD:
	case Equation::Operator::MULTIPLY:
	case Equation::Operator::EXPONENT:
		return false;
	case Equation::Operator::SUBTRACT:
	case Equation::Operator::DIVIDE:
		return true;
	}}

std::ostream& operator<<(std::ostream& o,Equation const& rhs){
	std::visit(overloaded{
			[&](double val){
				if(std::isinf(val))
					if(val>0)
						o<<"\\infty";
					else
						o<<"-\\infty";
				else if(std::isnan(val))
					o<<"NaN";
				else
					o << val;
			},
			[&](Equation::Variable var){o << var.name;},
			[&](Equation::F_node const& f){
				o << f.function;
				if(f.subscripts.size()){
					auto parens=false;
					if(auto eqq=std::get_if<Equation::Op_node>(&f.subscripts[0].value))
						parens=true;
					o<<"_";
					if(parens)
						o<<"(";
					o <<f.subscripts[0];// TODO: maybe add {}s?
					if(parens)
						o<<")";
				}
				if(f.customizations.size())
					o<<'['<<f.customizations[0]<<']';
				if(f.arguments.size())
					o<<'{'<<f.arguments[0]<<'}';
			},
			[&](Equation::Op_node const& eq){
				auto prec = precedent(eq.op);

				bool add_parentheses = false;
				if (auto* eqq=std::get_if<Equation::Op_node>(&eq.left->value))
					add_parentheses = precedent(eqq->op)<prec;
        if (add_parentheses)
					o << '(';
        o<<*eq.left;
        if (add_parentheses)
					o << ')';
				
				o << Equation::to_sym(eq.op);
				
        add_parentheses = false;
				if (auto* eqq=std::get_if<Equation::Op_node>(&eq.right->value))
					add_parentheses = precedent(eqq->op)<prec || (precedent(eqq->op)==prec && right_binding(eq.op));
        if (add_parentheses)
					o << '(';
        o<< *eq.right;
        if (add_parentheses)
					o << ')';
			}
		},rhs.value);
	return o;
}
