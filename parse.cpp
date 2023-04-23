#include "parse.hpp"
#include <optional>
#include <iostream> // for std::cerr
#include <sstream>

using std::string;

auto parse_number(string const& formula,size_t head)
	->std::optional<std::pair<double,int>>{
	if(head<formula.size() &&(std::isdigit(formula[head]) || formula[head]=='.')){
		std::istringstream buf(formula.substr(head));
		double v;
		buf >> v;
		//std::cerr<<"[NUMBER] val = "<<v<<std::endl;
		//std::cerr<<"[NUMBER] tellg = "<<buf.tellg()<<std::endl;
		if(buf.tellg()==-1)
			return {{v,formula.size()}};
		else
			return {{v,head+buf.tellg()}};
	}else
		return {};
}

auto parse_latex_basic(string const& formula,size_t head)
	->std::optional<std::pair<std::string,int>>{
	if(head>=formula.size()) return {};
	if(formula[head]=='{'){
		std::string ret({formula[head++]});
		while(formula[head]!='}'){
			if(head>=formula.size()){
				std::cerr <<"Error: LaTeX group started but not ended." << std::endl;
				return {};
			}
			ret += formula[head++];
		}
		return {{ret,head}};
	}
	if(auto on=parse_number(formula,head)){
		auto [n,h]=*on;
		return {{std::to_string(n),h}};
	}
	std::cerr <<"Warning: LaTeX group requested but not found, using one char." << std::endl;
	return {{{formula[head]},head+1}};
}

auto parse_variable(string const& formula,size_t head)
	->std::optional<std::pair<Equation,int>>{
	if(head<formula.size() && std::isalpha(formula[head])){
		std::string var({formula[head++]});
		if(head<formula.size() && formula[head]=='_'){
			var+=formula[head++];
			if(auto nxt=parse_latex_basic(formula,head)){
				auto [s,h]=*nxt;
				var+=s;
				head=h;
			}else{
				std::cerr <<"Error: Variable LaTeX subscript requested but missing." << std::endl;
				return {};
			}
		}
		return {{{Equation::Variable({var})},head}};
	}else return {};
}

auto parse_parenthetical(string const& formula,size_t head)
	->std::optional<std::pair<Equation,int>>;

auto parse_term(string const& formula,size_t head)
	->std::optional<std::pair<Equation,int>>{
	// parenthesized | number | variable
	if(auto par=parse_parenthetical(formula,head))
		return par;
	if(auto num=parse_number(formula,head)){
		auto [n,h]=*num;
		return {{{n},h}};
	}
	if(auto var=parse_variable(formula,head))
		return var;
	return {};
}

auto parse_product(string const& formula,size_t head)
	->std::optional<std::pair<Equation,int>>{
	// term+(('*'?|'/')+term)*
	if(auto term=parse_term(formula,head)){
		auto [t,h]=*term;
		head=h;
		auto ret=t;
		while(head<formula.size()){
			if(formula[head]=='*' || formula[head]=='/'){
				auto op=formula[head++];
				if(auto term2=parse_term(formula,head)){
					auto [t2,h2]=*term2;
					head=h2;
					ret={Equation::Op_node({op,{ret},{t2}})};
				}else{
					std::cerr <<"Error: Back track in parse_product."<<std::endl;
					return {{ret,head-1}};
				}
			}else
				if(auto term2=parse_term(formula,head)){
					auto [t2,h2]=*term2;
					head=h2;
					ret={Equation::Op_node({'*',{ret},{t2}})};
				}else
					return {{ret,head}};
		}
		return {{ret,head}};
	}else
		return {};
}

auto parse_sum(string const& formula,size_t head)
	->std::optional<std::pair<Equation,int>>{
	// product+([+-]+product)*
	if(auto term=parse_product(formula,head)){
		auto [t,h]=*term;
		head=h;
		auto ret=t;
		while(head<formula.size()){
			if(formula[head]=='+' || formula[head]=='-'){
				auto op=formula[head++];
				if(auto term2=parse_product(formula,head)){
					auto [t2,h2]=*term2;
					head=h2;
					ret={Equation::Op_node({op,{ret},{t2}})};
				}else{
					std::cerr <<"Error: Back track in parse_sum."<<std::endl;
					return {{ret,head-1}};
				}
			}else
				return {{ret,head}};
		}
		return {{ret,head}};
	}else
		return {};
}

auto parse_eq(string const& formula,size_t head)
	->std::optional<std::pair<Equation,int>>{
	return parse_sum(formula,head); // good enough for now
}

auto parse_parenthetical(string const& formula,size_t head)
	->std::optional<std::pair<Equation,int>>{
	// '('+equation+')'
	if(head>=formula.size()) return {};
	if(formula[head++]!='(') return {};
	if(auto eq=parse_eq(formula,head)){
		auto [e,h]=*eq;
		head=h;
		if(head>=formula.size()) return {};
		if(formula[head++]!=')') return {};
		return {{e,head}};
	}else return {};
}

Equation parse_formula(string const& formula) {
	if(auto eq= parse_eq(formula,0))
		// Check that all are parsed
		return eq->first;
	throw "Unable to Parse";
}
