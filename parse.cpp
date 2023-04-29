#include "parse.hpp"
#include <optional>
#include <iostream> // for std::cerr
#include <sstream>

//#define DEBUG 1
#ifdef DEBUG
#define DB(X) do{std::cerr<<X<<std::endl;}while(0)
#else
#define DB(X) do{}while(0)
#endif

using std::string;

auto parse_number(string const& formula,size_t head,bool allow_unary_minus)
	->std::optional<std::pair<double,size_t>>{
	DB(__func__ << ": " << formula.substr(0,head)<<" || "<<formula.substr(head));
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
		DB("parse_number: " << v );
		if(buf.tellg()==-1)
			return {{v,formula.size()}};
		else
			return {{v,head+buf.tellg()}};
	}
	return {};
}

auto parse_latex_basic(string const& formula,size_t head)
	->std::optional<std::pair<std::string,size_t>>{
	DB(__func__ << ": " << formula.substr(0,head)<<" || "<<formula.substr(head));
	if(head>=formula.size()) return {};
	if(formula[head]=='{'){
		std::string ret({formula[head++]});
		while(formula[head]!='}'){ // TODO: Use std::find and substr
			if(head>=formula.size()){
				std::cerr <<"Error: LaTeX group started but not ended." << std::endl;
				return {};
			}
			ret += formula[head++];
		}
		ret += formula[head++];
		DB("parse_latex_basic: "<<ret);
		return {{ret,head}};
	}
	if(auto on=parse_number(formula,head,false)){
		auto [n,h]=*on; // just grabbing the length
		DB("parse_latex_basic:" <<n);
		return {{formula.substr(head,h),h}};
	}
	std::cerr <<"Warning: LaTeX group requested but not found, using one char." << std::endl;
	return {{{formula[head]},head+1}};
}

auto parse_variable(string const& formula,size_t head)
	->std::optional<std::pair<Equation,size_t>>{
	DB(__func__ << ": " << formula.substr(0,head)<<" || "<<formula.substr(head));
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
		DB("parse_variable: " <<var);
		return {{{Equation::Variable({var})},head}};
	}
	return {};
}

auto parse_parenthetical(string const& formula,size_t head,char open, char close)
	->std::optional<std::pair<Equation,size_t>>;

auto parse_term(string const& formula,size_t head,bool allow_leading_unary)
	->std::optional<std::pair<Equation,size_t>>;

auto parse_named_operator(string const& formula,size_t head)
	->std::optional<std::pair<Equation,size_t>>{
	DB(__func__ << ": " << formula.substr(0,head)<<" || "<<formula.substr(head));
	// starts with \sqrt
	// parse { exp }
	if(formula.substr(head).starts_with("\\sqrt")){
		auto h=head+5;
		if(auto tmplte=parse_parenthetical(formula,h,'[',']')){
			auto [t,h2]=*tmplte;
			if(auto body=parse_parenthetical(formula,h2,'{','}')){
				auto [b,h3] = *body;
				return {{{Equation::F_node({"\\sqrt",{t},{b}})},h3}};}}
		if(auto body=parse_parenthetical(formula,h,'{','}')){
			auto [b,h2] = *body;
			return {{{Equation::F_node({"\\sqrt",{b}})},h2}};}}
	if(formula.substr(head).starts_with("\\ln")){
		auto h=head+3;
		if(auto body=parse_parenthetical(formula,h,'{','}')){
			auto [b,h2] = *body;
			return {{{Equation::F_node({"\\ln",{b}})},h2}};}}
	if(formula.substr(head).starts_with("\\log")){
		auto h=head+4;
		if(h<formula.size() && formula[h]=='_'){
			auto h2=h+1;
			if(auto base=parse_term(formula,h2,false)){
				auto [bs,h3]=*base;
				if(auto body=parse_parenthetical(formula,h3,'{','}')){
					auto [b,h4] = *body;
					return {{{Equation::F_node({{{bs},{}},"\\log",{b}})},h4}};}}}
		if(auto body=parse_parenthetical(formula,h,'{','}')){
			auto [b,h2] = *body;
			return {{{Equation::F_node({"\\log",{b}})},h2}};}}
	return {};
}

auto parse_term(string const& formula,size_t head,bool allow_leading_unary)
	->std::optional<std::pair<Equation,size_t>>{
	DB(__func__ << ": " << formula.substr(0,head)<<" || "<<formula.substr(head));
	// parenthesized | number | variable | named_operator
	if(auto par=parse_parenthetical(formula,head,'(',')'))
		return par;
	if(auto num=parse_number(formula,head,allow_leading_unary)){
		auto [n,h]=*num;
		return {{{n},h}};
	}
	if(auto var=parse_variable(formula,head))
		// TODO: Leading unary on Variables.
		return var;
	if(auto func=parse_named_operator(formula,head))
		return func;
	return {};
}

auto parse_power(string const& formula,size_t head,bool allow_leading_unary)
	->std::optional<std::pair<Equation,size_t>>{
	DB(__func__ << ": " << formula.substr(0,head)<<" || "<<formula.substr(head));
	if(auto term=parse_term(formula,head,allow_leading_unary)){
		auto [t,h] = *term;
		if(h<formula.size() && formula[h]=='^')
			if(auto exp=parse_power(formula,h+1,true)){
				auto [e,h2]=*exp;
				return {{{Equation::Op_node({'^',t,e})},h2}};
			}
		return term;
	}
	return {};
}

auto parse_product(string const& formula,size_t head,bool allow_leading_unary)
	->std::optional<std::pair<Equation,size_t>>{
	DB(__func__ << ": " << formula.substr(0,head)<<" || "<<formula.substr(head));
	// power+(('*'?|'/')+power)*
	if(auto power=parse_power(formula,head,allow_leading_unary)){
		auto [t,h]=*power;
		head=h;
		auto ret=t;
		while(head<formula.size()){
			if(formula[head]=='*' || formula[head]=='/'){
				auto op=formula[head++];
				if(auto power2=parse_power(formula,head,true)){
					auto [t2,h2]=*power2;
					head=h2;
					ret={Equation::Op_node({op,{ret},{t2}})};
				}else{
					std::cerr <<"Error: Back track in parse_product."<<std::endl;
					return {{ret,head-1}};
				}
			}else
				if(auto power2=parse_power(formula,head,false)){
					auto [t2,h2]=*power2;
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
	DB(__func__ << ": " << formula.substr(0,head)<<" || "<<formula.substr(head));
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
	DB(__func__ << ": " << formula.substr(0,head)<<" || "<<formula.substr(head));
	return parse_sum(formula,head,true); // good enough for now
}

auto parse_parenthetical(string const& formula,size_t head,char open, char close)
	->std::optional<std::pair<Equation,size_t>>{
	DB(__func__ << ": " << formula.substr(0,head)<<" || "<<formula.substr(head));
	// 'open+expression+'close
	if(head>=formula.size()) return {};
	if(formula[head++]!=open) return {};
	if(auto eq=parse_exp(formula,head)){
		auto [e,h]=*eq;
		head=h;
		if(head>=formula.size()) return {};
		if(formula[head++]!=close) return {};
		return {{e,head}};
	}else return {};
}

Equation parse_formula(string const& formula) {
	if(auto eq= parse_exp(formula,0))
		if(eq->second == formula.size())
			return eq->first;
	throw "Unable to Parse";
}
