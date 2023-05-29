#ifndef __EQUATION_HPP__
#define __EQUATION_HPP__

#include <string>
#include <variant>
#include <memory>
#include <vector>

struct Equation {
	struct Variable{
		std::string name;
		auto operator<=>(Variable const&)const=default;
	};
	struct Constant{
		std::string name;
	};
	enum struct Operator {ADD,SUBTRACT,MULTIPLY,DIVIDE,EXPONENT};
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
	struct F_node{
		std::string function;
		std::vector<Equation> customizations;
		std::vector<Equation> subscripts;
		std::vector<Equation> superscripts;
		std::vector<Equation> arguments;
		F_node(std::string f,std::vector<Equation> b)
			:function(f),arguments(b){}
		F_node(std::string f,std::vector<Equation> c,std::vector<Equation> b)
			:function(f),customizations(c),arguments(b){}
		F_node(std::pair<std::vector<Equation>,std::vector<Equation>> ssc,std::string f,std::vector<Equation> b)
			:function(f),subscripts(ssc.first),superscripts(ssc.second),arguments(b){}
	};
	std::variant<double,Constant,Variable,Op_node,F_node> value;
	Equation(double v):value(v){};
	Equation(F_node v):value(v){};
	Equation(Op_node v):value(v){};
	Equation(Variable v):value(v){};
	Equation(Constant v):value(v){};
};

std::tuple<std::vector<Equation::Variable>,std::vector<Equation::Variable>,std::vector<Equation::Variable>>
bindings(Equation const&); // unbound, bound, mixed

int precedent(Equation::Operator op);
bool commutative(Equation::Operator);
std::ostream& operator<<(std::ostream& o,Equation const& rhs);

#endif
