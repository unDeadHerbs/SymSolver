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
	(string const& formula,size_t head,bool allow_leading_unary){					\
		DB(__func__ << ": " <<																							\
			 formula.substr(0,head)<<" || "<<formula.substr(head));						\
		parse_##NAME##_helper h;																						\
		h.internal(formula,head,allow_leading_unary);												\
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

vPS parse_parenthetical(string const& formula,size_t head,bool allow_leading_unary);
vPS parse_bracketed(string const& formula,size_t head,bool allow_leading_unary);
vPS parse_braces(string const& formula,size_t head,bool allow_leading_unary);
vPS parse_term(string const& formula,size_t head,bool allow_leading_unary);
// TODO: Prototype Macro?

Parser(named_operator){
	unused(allow_leading_unary);
	// Basic Functions
	if(formula.substr(head).starts_with("\\sqrt")){
		auto h=head+5;
		if(auto tmplte=parse_bracketed(formula,h,true))
			if(auto body=parse_braces(formula,tmplte[0].head,true))
				Return(Equation::F_node({"\\sqrt",{tmplte[0].eq},{body[0].eq}}),body[0].head);
		if(auto body=parse_braces(formula,h,true))
			Return(Equation::F_node({"\\sqrt",{body[0].eq}}),body[0].head);}
	if(formula.substr(head).starts_with("\\ln")){
		auto h=head+3;
		if(auto body=parse_braces(formula,h,true))
			Return(Equation::F_node({"\\ln",{body[0].eq}}),body[0].head);}
	if(formula.substr(head).starts_with("\\log")){
		auto h=head+4;
		if(h<formula.size() && formula[h]=='_'){
			auto h2=h+1;
			if(auto base=parse_term(formula,h2,false))
				if(auto body=parse_braces(formula,base[0].head,true))
					Return(Equation::F_node({{{base[0].eq},{}},"\\log",{body[0].eq}}),body[0].head);}
		if(auto body=parse_braces(formula,h,true))
			Return(Equation::F_node({"\\log",{body[0].eq}}),body[0].head);}

	// Trig Functions
	if(formula.substr(head).starts_with("\\exp")){
		auto h=head+4;
		if(auto body=parse_braces(formula,h,true))
			Return(Equation::F_node({"\\exp",{body[0].eq}}),body[0].head);}
	if(formula.substr(head).starts_with("\\sin")){
		auto h=head+4;
		if(auto body=parse_braces(formula,h,true))
			Return(Equation::F_node({"\\sin",{body[0].eq}}),body[0].head);}
	if(formula.substr(head).starts_with("\\cos")){
		auto h=head+4;
		if(auto body=parse_braces(formula,h,true))
			Return(Equation::F_node({"\\cos",{body[0].eq}}),body[0].head);}
	if(formula.substr(head).starts_with("\\tan")){
		auto h=head+4;
		if(auto body=parse_braces(formula,h,true))
			Return(Equation::F_node({"\\tan",{body[0].eq}}),body[0].head);}
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
	if(auto par=parse_parenthetical(formula,head,true))
		Return(par[0].eq,par[0].head);
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

Parser(product){
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
					Return(ret,head-1);
				}
			}else if(auto power2=parse_power(formula,head,false)){
				head=power2[0].head;
				ret={Equation::Op_node({'*',{ret},{power2[0].eq}})};
			}else
				break;
		}
		Return(ret,head);
	}
}

Parser(sum){
	// product+([+-]+product)*
	if(auto term=parse_product(formula,head,allow_leading_unary)){
		head=term[0].head;
		auto ret=term[0].eq;
		while(head<formula.size()){
			if(formula[head]=='+' || formula[head]=='-'){
				auto op=formula[head++];
				if(auto term2=parse_product(formula,head,true)){
					head=term2[0].head;
					ret={Equation::Op_node({op,{ret},{term2[0].eq}})};
				}else{
					std::cerr <<"Error: Back track in parse_sum."<<std::endl;
					Return(ret,head-1);
				}
			}else
				break;
		}
		Return(ret,head);
	}
}

Parser(expression){
	// good enough for now
	if(auto sum=parse_sum(formula,head,allow_leading_unary))
		Return(sum[0].eq,sum[0].head);
}

Parser(braces){
	// '{'+expression+'}'
	unused(allow_leading_unary);
	if(head>=formula.size()) return;
	if(formula[head++]!='{') return;
	if(auto eq=parse_expression(formula,head,true)){
		head=eq[0].head;
		if(head>=formula.size()) return;
		if(formula[head++]!='}') return;
		Return(eq[0].eq,head);
	}
}

Parser(bracketed){
	// '['+expression+']'
	unused(allow_leading_unary);
	if(head>=formula.size()) return;
	if(formula[head++]!='[') return;
	if(auto eq=parse_expression(formula,head,true)){
		head=eq[0].head;
		if(head>=formula.size()) return;
		if(formula[head++]!=']') return;
		Return(eq[0].eq,head);
	}
}


Parser(parenthetical){
	// '('+expression+')'
	unused(allow_leading_unary);
	if(head>=formula.size()) return;
	if(formula[head++]!='(') return;
	if(auto eq=parse_expression(formula,head,true)){
		head=eq[0].head;
		if(head>=formula.size()) return;
		if(formula[head++]!=')') return;
		Return(eq[0].eq,head);
	}
}

Equation parse_formula(string const& formula) {
	if(auto eq= parse_expression(formula,0,true))
		if(eq[0].head == formula.size())
			return eq[0].eq;
	throw "Unable to Parse";
}
