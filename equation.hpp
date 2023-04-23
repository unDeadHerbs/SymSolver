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
	struct EQ_node{
		Operator op;
		std::unique_ptr<Equation> left; // TODO: Switch to COW
		std::unique_ptr<Equation> right;
		EQ_node(Operator const,Equation const&,Equation const&);
		EQ_node(char const,Equation const&,Equation const&);
		EQ_node(EQ_node const&); // Allow copying the pointers
	};
	std::variant<double,EQ_node,Variable> value;
};

int precedent(char op);
void print(Equation e);

#endif
