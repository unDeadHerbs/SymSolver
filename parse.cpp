#include "parse.hpp"
#include <optional>
#include <iostream> // for std::cerr
#include <sstream>

using std::string;

auto parse_number(string const& formula,size_t head,bool allow_unary_minus)
	->std::optional<std::pair<double,size_t>>{
	if(allow_unary_minus)
		// TODO: it seems that >> can extract just the - and then fail?
		allow_unary_minus=head+1<formula.size() && (std::isdigit(formula[head+1])
																								|| formula[head+1]=='.');
	if(head<formula.size()
		 && ( std::isdigit(formula[head])
					|| formula[head]=='.'
					|| (allow_unary_minus && formula[head]=='-'))){
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
	->std::optional<std::pair<std::string,size_t>>{
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
		ret += formula[head++];
		return {{ret,head}};
	}
	if(auto on=parse_number(formula,head,false)){
		auto [n,h]=*on;
		return {{formula.substr(head,h),h}};
	}
	std::cerr <<"Warning: LaTeX group requested but not found, using one char." << std::endl;
	return {{{formula[head]},head+1}};
}

auto parse_variable(string const& formula,size_t head)
	->std::optional<std::pair<Equation,size_t>>{
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
	->std::optional<std::pair<Equation,size_t>>;

auto parse_term(string const& formula,size_t head,bool allow_leading_unary)
	->std::optional<std::pair<Equation,size_t>>{
	// parenthesized | number | variable
	if(auto par=parse_parenthetical(formula,head))
		return par;
	if(auto num=parse_number(formula,head,allow_leading_unary)){
		auto [n,h]=*num;
		return {{{n},h}};
	}
	if(auto var=parse_variable(formula,head))
		return var;
	return {};
}

auto parse_product(string const& formula,size_t head,bool allow_leading_unary)
	->std::optional<std::pair<Equation,size_t>>{
	// term+(('*'?|'/')+term)*
	if(auto term=parse_term(formula,head,allow_leading_unary)){
		auto [t,h]=*term;
		head=h;
		auto ret=t;
		while(head<formula.size()){
			if(formula[head]=='*' || formula[head]=='/'){
				auto op=formula[head++];
				if(auto term2=parse_term(formula,head,true)){
					auto [t2,h2]=*term2;
					head=h2;
					ret={Equation::Op_node({op,{ret},{t2}})};
				}else{
					std::cerr <<"Error: Back track in parse_product."<<std::endl;
					return {{ret,head-1}};
				}
			}else
				if(auto term2=parse_term(formula,head,false)){
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

auto parse_sum(string const& formula,size_t head,bool allow_leading_unary)
	->std::optional<std::pair<Equation,size_t>>{
	// product+([+-]+product)*
	if(auto term=parse_product(formula,head,allow_leading_unary)){
		auto [t,h]=*term;
		head=h;
		auto ret=t;
		while(head<formula.size()){
			if(formula[head]=='+' || formula[head]=='-'){
				auto op=formula[head++];
				if(auto term2=parse_product(formula,head,true)){
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

auto parse_exp(string const& formula,size_t head)
	->std::optional<std::pair<Equation,size_t>>{
	return parse_sum(formula,head,true); // good enough for now
}

auto parse_parenthetical(string const& formula,size_t head)
	->std::optional<std::pair<Equation,size_t>>{
	// '('+expression+')'
	if(head>=formula.size()) return {};
	if(formula[head++]!='(') return {};
	if(auto eq=parse_exp(formula,head)){
		auto [e,h]=*eq;
		head=h;
		if(head>=formula.size()) return {};
		if(formula[head++]!=')') return {};
		return {{e,head}};
	}else return {};
}

Equation parse_formula(string const& formula) {
	if(auto eq= parse_exp(formula,0))
		if(eq->second == formula.size())
			return eq->first;
	throw "Unable to Parse";
}
