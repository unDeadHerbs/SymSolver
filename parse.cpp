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

struct ParserState{
	Equation eq;
	//std::vector<string> bound; // list of bound variables.
	//std::vector<string> unbound; // list of unbound variables.
	size_t head;
};

struct vPS{
	std::vector<ParserState> PSs;
	operator bool()const{return PSs.size();} // TODO: Remove for for_each.
	template<typename T>
	auto& operator[](T rhs){return PSs[rhs];} // TODO: Remove for for_each.
	auto push_back(ParserState rhs){return PSs.push_back(rhs);}
};

#define unused(X) (void)X
#define Return(EQ,H) _ret.push_back({EQ,H})
#define Parser(NAME)																										\
	struct parse_##NAME##_helper{																					\
		vPS _ret;																														\
		void internal(string const&,size_t,bool);														\
	};																																		\
	vPS parse_##NAME																											\
	(string const& formula,size_t head,bool allow_leading_unary){						\
		DB(__func__ << ": " <<																							\
			 formula.substr(0,head)<<" || "<<formula.substr(head));						\
		parse_##NAME##_helper h;																						\
		h.internal(formula,head,allow_leading_unary);													\
		return h._ret;																											\
	}																																			\
	void parse_##NAME##_helper::internal																	\
	(string const& formula,size_t head,bool allow_leading_unary)

Parser(number){
	if(allow_leading_unary)
		// TODO: it seems that >> can extract just the - and then fail?
		allow_leading_unary=head+1<formula.size() && (std::isdigit(formula[head+1])
																								|| formula[head+1]=='.');
	if(head<formula.size()
		 && ( std::isdigit(formula[head])
					|| formula[head]=='.'
					|| (allow_leading_unary && formula[head]=='-'))){
		std::istringstream buf(formula.substr(head));
		double v;
		buf >> v;
		//std::cerr<<"[NUMBER] val = "<<v<<std::endl;
		//std::cerr<<"[NUMBER] tellg = "<<buf.tellg()<<std::endl;
		DB("parse_number: " << v );
		if(buf.tellg()==-1)
			Return(v,formula.size());
		else
			Return(v,head+buf.tellg());
	}
}

Parser(latex_basic){
	unused(allow_leading_unary);
	if(head>=formula.size()) return;
	if(formula[head]=='{'){
		std::string ret({formula[head++]});
		while(formula[head]!='}'){ // TODO: Use std::find and substr
			if(head>=formula.size()){
				std::cerr <<"Error: LaTeX group started but not ended." << std::endl;
				return;
			}
			ret += formula[head++];
		}
		ret += formula[head++];
		Return(Equation::Variable({ret}),head);
	}
	if(auto on=parse_number(formula,head,false)){
		// Just grabbing the length.
		DB("parse_latex_basic:" <<on[0].eq);
		Return(Equation::Variable({formula.substr(head,on[0].head)}),on[0].head);
	}else if(formula[head]!='{'){
		std::cerr <<"Warning: LaTeX group requested but not found, using one char." << std::endl;
		Return(Equation::Variable({string({formula[head]})}),head+1);
	}
}

Parser(variable){
	if(allow_leading_unary && head<formula.size() && formula[head]=='-')
		if(auto var=parse_variable(formula,head+1,false))
			Return(Equation({Equation::Operator::MULTIPLY,Equation(-1),var[0].eq}),var[0].head);
	if(head<formula.size() && std::isalpha(formula[head])){
		std::string var({formula[head++]});
		if(head<formula.size() && formula[head]=='_'){
			var+=formula[head++];
			if(auto nxt=parse_latex_basic(formula,head,allow_leading_unary)){
				var+=std::get<Equation::Variable>(nxt[0].eq.value).name;
				head=nxt[0].head;
			}else{
				std::cerr <<"Error: Variable LaTeX subscript requested but missing." << std::endl;
				return;
			}
		}
		if(var=="i") // The imaginary unit isn't a variable.
			return;
		DB("parse_variable: " <<var);
		Return({Equation::Variable({var})},head);
	}
}

auto parse_parenthetical(string const& formula,size_t head,char open, char close)
	->std::optional<std::pair<Equation,size_t>>;

vPS parse_term(string const& formula,size_t head,bool allow_leading_unary);
// TODO: Prototype Macro?

Parser(named_operator){
	unused(allow_leading_unary);
	// Basic Functions
	if(formula.substr(head).starts_with("\\sqrt")){
		auto h=head+5;
		if(auto tmplte=parse_parenthetical(formula,h,'[',']')){
			auto [t,h2]=*tmplte;
			if(auto body=parse_parenthetical(formula,h2,'{','}')){
				auto [b,h3] = *body;
				Return(Equation::F_node({"\\sqrt",{t},{b}}),h3);}}
		if(auto body=parse_parenthetical(formula,h,'{','}')){
			auto [b,h2] = *body;
			Return(Equation::F_node({"\\sqrt",{b}}),h2);}}
	if(formula.substr(head).starts_with("\\ln")){
		auto h=head+3;
		if(auto body=parse_parenthetical(formula,h,'{','}')){
			auto [b,h2] = *body;
			Return(Equation::F_node({"\\ln",{b}}),h2);}}
	if(formula.substr(head).starts_with("\\log")){
		auto h=head+4;
		if(h<formula.size() && formula[h]=='_'){
			auto h2=h+1;
			if(auto base=parse_term(formula,h2,false))
				if(auto body=parse_parenthetical(formula,base[0].head,'{','}')){
					auto [b,h4] = *body;
					Return(Equation::F_node({{{base[0].eq},{}},"\\log",{b}}),h4);}}
		if(auto body=parse_parenthetical(formula,h,'{','}')){
			auto [b,h2] = *body;
			Return(Equation::F_node({"\\log",{b}}),h2);}}

	// Trig Functions
	if(formula.substr(head).starts_with("\\exp")){
		auto h=head+4;
		if(auto body=parse_parenthetical(formula,h,'{','}')){
			auto [b,h2] = *body;
			Return(Equation::F_node({"\\exp",{b}}),h2);}}
	if(formula.substr(head).starts_with("\\sin")){
		auto h=head+4;
		if(auto body=parse_parenthetical(formula,h,'{','}')){
			auto [b,h2] = *body;
			Return(Equation::F_node({"\\sin",{b}}),h2);}}
	if(formula.substr(head).starts_with("\\cos")){
		auto h=head+4;
		if(auto body=parse_parenthetical(formula,h,'{','}')){
			auto [b,h2] = *body;
			Return(Equation::F_node({"\\cos",{b}}),h2);}}
	if(formula.substr(head).starts_with("\\tan")){
		auto h=head+4;
		if(auto body=parse_parenthetical(formula,h,'{','}')){
			auto [b,h2] = *body;
			Return(Equation::F_node({"\\tan",{b}}),h2);}}
}

Parser(constant){
	unused(allow_leading_unary);
	if(formula.substr(head).starts_with("\\pi"))
		Return(Equation::Constant({"pi"}),head+3);
	if(formula.substr(head).starts_with("\\phi"))
		Return(Equation::Constant({"phi"}),head+4);
	if(formula.substr(head).starts_with("i"))
		Return(Equation::Constant({"i"}),head+1);
}

Parser(term){
	// parenthesized | number | constant | variable | named_operator
	if(auto par=parse_parenthetical(formula,head,'(',')'))
		Return(par->first,par->second);
	if(auto num=parse_number(formula,head,allow_leading_unary))
		Return(num[0].eq,num[0].head);
	if(auto cnst=parse_constant(formula,head,allow_leading_unary))
		Return(cnst[0].eq,cnst[0].head);
	if(auto var=parse_variable(formula,head,allow_leading_unary))
		Return(var[0].eq,var[0].head);
	if(auto func=parse_named_operator(formula,head,allow_leading_unary))
		Return(func[0].eq,func[0].head);
}

Parser(power){
	if(auto term=parse_term(formula,head,allow_leading_unary)){
		if(term[0].head<formula.size() && formula[term[0].head]=='^')
			if(auto exp=parse_power(formula,term[0].head+1,true))
				Return(Equation::Op_node({'^',term[0].eq,exp[0].eq}),exp[0].head);
		Return(term[0].eq,term[0].head);
	}
}

auto parse_product(string const& formula,size_t head,bool allow_leading_unary)
	->std::optional<std::pair<Equation,size_t>>{
	DB(__func__ << ": " << formula.substr(0,head)<<" || "<<formula.substr(head));
	// power+(('*'?|'/')+power)*
	if(auto power=parse_power(formula,head,allow_leading_unary)){
		head=power[0].head;
		auto ret=power[0].eq;
		while(head<formula.size()){
			if(formula[head]=='*' || formula[head]=='/'){
				auto op=formula[head++];
				if(auto power2=parse_power(formula,head,true)){
					head=power2[0].head;
					ret={Equation::Op_node({op,{ret},{power2[0].eq}})};
				}else{
					std::cerr <<"Error: Back track in parse_product."<<std::endl;
					return {{ret,head-1}};
				}
			}else
				if(auto power2=parse_power(formula,head,false)){
					head=power2[0].head;
					ret={Equation::Op_node({'*',{ret},{power2[0].eq}})};
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
