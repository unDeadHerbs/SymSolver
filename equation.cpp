#include "equation.hpp"
#include <iostream>

// Copied from cppreference, not sure why its not in the std namespace already.
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

Equation::Operator constexpr Equation::to_Operator(char const c){
	switch(c){
	case '+': return Operator::ADD;
	case '-': return Operator::SUBTRACT;
	case '*': return Operator::MULTIPLY;
	case '/': return Operator::DIVIDE;
	default: throw "Invalid Operator"; // TODO: That's not very constexper of you.
	}
}

char constexpr Equation::to_sym(Operator const op){
	switch(op){
	case Operator::ADD: return '+';
	case Operator::SUBTRACT: return '-';
	case Operator::MULTIPLY: return '*';
	case Operator::DIVIDE: return '/';
	}
}

Equation::EQ_node::EQ_node(Operator const _op,Equation const& _l,Equation const& _r)
	:op(_op){
	left=std::make_unique<Equation>(_l); // Not sure why this isn't just the constructor
	right=std::make_unique<Equation>(_r);
}

Equation::EQ_node::EQ_node(char const _op,Equation const& _l,Equation const& _r)
	:op(Equation::to_Operator(_op)){
	left=std::make_unique<Equation>(_l); // Not sure why this isn't just the constructor
	right=std::make_unique<Equation>(_r);
}

Equation::EQ_node::EQ_node(EQ_node const& eq)
	:op(eq.op){
	left=std::make_unique<Equation>(*eq.left); // Not sure why this isn't just the constructor
	right=std::make_unique<Equation>(*eq.right);
}

int precedent(Equation::Operator op){
	switch(op){
	case Equation::Operator::ADD: case Equation::Operator::SUBTRACT:
		return 1;
	case Equation::Operator::MULTIPLY: case Equation::Operator::DIVIDE:
		return 2;
	}}

void print(Equation e) {
	std::visit(overloaded{
			[](double val){std::cout << val;},
			[](Equation::Variable var){std::cout << var.name;},
			[](Equation::EQ_node const& eq){
				auto prec = precedent(eq.op);

				bool add_parentheses = false;
				if (auto* eqq=std::get_if<Equation::EQ_node>(&eq.left->value))
					add_parentheses = precedent(eqq->op)<prec;
        if (add_parentheses)
					std::cout << '(';
        print(*eq.left);
        if (add_parentheses)
					std::cout << ')';
				
				std::cout << Equation::to_sym(eq.op);
				
        add_parentheses = false;
				if (auto* eqq=std::get_if<Equation::EQ_node>(&eq.right->value))
					add_parentheses = precedent(eqq->op)<prec;
        if (add_parentheses)
					std::cout << '(';
        print(*eq.right);
        if (add_parentheses)
					std::cout << ')';
			}
		},e.value);}
