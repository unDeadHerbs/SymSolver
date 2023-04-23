#include "equation.hpp"
#include <iostream>

// Copied from cppreference, not sure why its not in the std namespace already.
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

Equation::EQ_node::EQ_node(char const _op,Equation const& _l,Equation const& _r)
			:op(_op){
			left=std::make_unique<Equation>(_l); // Not sure why this isn't just the constructor
			right=std::make_unique<Equation>(_r);
		}

Equation::EQ_node::EQ_node(EQ_node const& eq):
	op(eq.op){
			left=std::make_unique<Equation>(*eq.left); // Not sure why this isn't just the constructor
			right=std::make_unique<Equation>(*eq.right);
		}

int precedent(char op){
	switch(op){
	case '+': case '-':
		return 1;
	case '*': case '/':
		return 2;
	default:
		return 3;
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
				
				std::cout << eq.op;
				
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
