#ifndef __EQUATION_HPP__
#define __EQUATION_HPP__

#include <string>
#include <variant>
#include <memory>

struct Equation {
	struct Variable{
		std::string name;
	};
	enum struct Operator {ADD,SUBTRACT,MULTIPLY,DIVIDE};
	constexpr static Operator to_Operator(char const);
	constexpr static char to_sym(Operator const);
	struct Op_node{
		Operator op;
		std::unique_ptr<Equation> left; // TODO: Switch to COW
		std::unique_ptr<Equation> right;
		Op_node(Operator const,Equation const&,Equation const&);
		Op_node(char const,Equation const&,Equation const&);
		Op_node(Op_node const&); // Allow copying the pointers
		Op_node(Op_node&&)=default;
		Op_node& operator=(Op_node const&);
	};
	std::variant<double,Op_node,Variable> value;
	Equation(double v):value(v){};
	Equation(Op_node v):value(v){};
	Equation(Variable v):value(v){};
};

int precedent(char op);
void print(Equation e);

#endif
