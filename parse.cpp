#include "parse.hpp"
#include <optional>
#include <iostream> // for std::cerr
#include <sstream>
#include <functional>

//#define DEBUG 1
#ifdef DEBUG
#define DB(X) do{std::cerr<<X<<std::endl;}while(0)
#define DBv(X) do{for(auto& v:X) DB(" - "<<v.eq);}while(0)
#else
#define DB(X) do{}while(0)
#define DBv(X) do{}while(0)
#endif

using std::string;

struct ParserState{
	Equation eq;
	//std::vector<string> bound; // list of bound variables.
	//std::vector<string> unbound; // list of unbound variables.
	size_t head;
};

typedef std::vector<ParserState> vPS;

#define unused(X) (void)X
#define Return(EQ,H) _ret.push_back({EQ,H})
#define Parser(NAME)																										\
	struct parse_##NAME##_helper{																					\
		vPS _ret;																														\
		void internal(string const&,size_t,bool);														\
	};																																		\
	vPS parse_##NAME																											\
	(string const& formula,size_t head,bool allow_leading_unary){					\
		DB("Entering "<<__func__ << ": " <<																	\
			 formula.substr(0,head)<<" || "<<formula.substr(head));						\
		parse_##NAME##_helper h;																						\
		h.internal(formula,head,allow_leading_unary);												\
		DB(" Leaving " << __func__ << ": "<<h._ret.size());									\
		DBv(h._ret);																												\
		return h._ret;																											\
	}																																			\
	void parse_##NAME##_helper::internal																	\
	(string const& formula,size_t head,bool allow_leading_unary)

#define consume_spaces(H) do{while(formula.size()>H && formula[H]==' ')H++;}while(0)

Parser(number){
	consume_spaces(head);
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
		if(buf.tellg()==-1)
			Return(v,formula.size());
		else
			Return(v,head+buf.tellg());
	}
}

Parser(latex_basic){
	consume_spaces(head);
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
	for(auto& on:parse_number(formula,head,false))
		// Just grabbing the length.
		Return(Equation::Variable({formula.substr(head,on.head)}),on.head);
	if(_ret.size()==0 && formula[head]!='{'){ // TODO: Don't touch the internals.
		std::cerr <<"Warning: LaTeX group requested but not found, using one char." << std::endl;
		Return(Equation::Variable({string({formula[head]})}),head+1);
	}
}

Parser(variable){
	consume_spaces(head);
	if(allow_leading_unary && head<formula.size() && formula[head]=='-')
		for(auto var:parse_variable(formula,head+1,false))
			Return(Equation({Equation::Operator::MULTIPLY,Equation(-1),var.eq}),var.head);
	if(head<formula.size() && std::isalpha(formula[head])){
		std::string var({formula[head++]});
		if(head<formula.size() && formula[head]=='_')
			for(auto nxt:parse_latex_basic(formula,head+1,allow_leading_unary)){
				auto var2=var+"_"+std::get<Equation::Variable>(nxt.eq.value).name;
				Return({Equation::Variable({var2})},nxt.head);
			}
		if(var!="i") // The imaginary unit isn't a variable.
		  Return({Equation::Variable({var})},head);
	}
}

vPS parse_parenthetical(string const& formula,size_t head,bool allow_leading_unary);
vPS parse_bracketed(string const& formula,size_t head,bool allow_leading_unary);
vPS parse_braces(string const& formula,size_t head,bool allow_leading_unary);
vPS parse_term(string const& formula,size_t head,bool allow_leading_unary);
// TODO: Prototype Macro?

Parser(space_term_or_brace){
	if(head>=formula.size()) return;
	bool has_space = formula[head]==' ';
	consume_spaces(head);
	for(auto body:parse_braces(formula,head,allow_leading_unary))
		Return(body.eq,body.head);
	if(has_space)
		for(auto body:parse_term(formula,head,allow_leading_unary))
			Return(body.eq,body.head);}

Parser(named_operator){
	consume_spaces(head);
	unused(allow_leading_unary);
	// Basic Functions
	if(formula.substr(head).starts_with("\\sqrt")){
		auto h=head+5;
		for(auto tmplte:parse_bracketed(formula,h,true))
			for(auto body:parse_space_term_or_brace(formula,tmplte.head,true))
				Return(Equation::F_node({"\\sqrt",{tmplte.eq},{body.eq}}),body.head);
		for(auto body:parse_space_term_or_brace(formula,h,true))
			Return(Equation::F_node({"\\sqrt",{body.eq}}),body.head);}
	if(formula.substr(head).starts_with("\\ln")){
		auto h=head+3;
		for(auto body:parse_space_term_or_brace(formula,h,true))
			Return(Equation::F_node({"\\ln",{body.eq}}),body.head);}
	if(formula.substr(head).starts_with("\\log")){
		auto h=head+4;
		if(h<formula.size() && formula[h]=='_'){
			auto h2=h+1;
			for(auto base:parse_term(formula,h2,false))
				for(auto body:parse_space_term_or_brace(formula,base.head,true))
					Return(Equation::F_node({{{base.eq},{}},"\\log",{body.eq}}),body.head);}
		for(auto body:parse_space_term_or_brace(formula,h,true))
			Return(Equation::F_node({"\\log",{body.eq}}),body.head);}

	// Trig Functions
	if(formula.substr(head).starts_with("\\exp")){
		auto h=head+4;
		for(auto body:parse_space_term_or_brace(formula,h,true))
			Return(Equation::F_node({"\\exp",{body.eq}}),body.head);}
	if(formula.substr(head).starts_with("\\sin")){
		auto h=head+4;
		for(auto body:parse_space_term_or_brace(formula,h,true))
			Return(Equation::F_node({"\\sin",{body.eq}}),body.head);}
	if(formula.substr(head).starts_with("\\cos")){
		auto h=head+4;
		for(auto body:parse_space_term_or_brace(formula,h,true))
			Return(Equation::F_node({"\\cos",{body.eq}}),body.head);}
	if(formula.substr(head).starts_with("\\tan")){
		auto h=head+4;
		for(auto body:parse_space_term_or_brace(formula,h,true))
			Return(Equation::F_node({"\\tan",{body.eq}}),body.head);}
}

Parser(constant){
	consume_spaces(head);
	unused(allow_leading_unary);
	if(formula.substr(head).starts_with("\\pi"))
		Return(Equation::Constant({"pi"}),head+3);
	if(formula.substr(head).starts_with("\\phi"))
		Return(Equation::Constant({"phi"}),head+4);
	if(formula.substr(head).starts_with("i"))
		Return(Equation::Constant({"i"}),head+1);
}

Parser(term){
	consume_spaces(head);
	// parenthesized | number | constant | variable | named_operator
	for(auto par:parse_parenthetical(formula,head,true))
		Return(par.eq,par.head);
	for(auto num:parse_number(formula,head,allow_leading_unary))
		Return(num.eq,num.head);
	for(auto cnst:parse_constant(formula,head,allow_leading_unary))
		Return(cnst.eq,cnst.head);
	for(auto var:parse_variable(formula,head,allow_leading_unary))
		Return(var.eq,var.head);
	for(auto func:parse_named_operator(formula,head,allow_leading_unary))
		Return(func.eq,func.head);
}

Parser(power){
	for(auto term:parse_term(formula,head,allow_leading_unary)){
		consume_spaces(term.head);
		if(term.head<formula.size() && formula[term.head]=='^')
			for(auto exp:parse_power(formula,term.head+1,true))
				Return(Equation::Op_node({'^',term.eq,exp.eq}),exp.head);
		Return(term.eq,term.head);
	}
}

Parser(product){
	std::function<vPS(ParserState)> helper=[&](ParserState ps)->vPS{
		// (('*'?|'/')+power)*

		// Builds tree from left so that division doesn't invert everything.
		vPS ret;
		ret.push_back(ps);
		consume_spaces(ps.head);
	  if(ps.head>=formula.size()) return ret;
		if(formula[ps.head]=='*' || formula[ps.head]=='/'){
			auto op=formula[ps.head];
			for(auto power:parse_power(formula,ps.head+1,true)){
					Equation t={Equation::Op_node({op,{ps.eq},{power.eq}})};
					for(auto r:helper(ParserState({t,power.head})))
						ret.push_back(r);
			}}
		for(auto power:parse_power(formula,ps.head,true)){
			Equation t={Equation::Op_node({'*',{ps.eq},{power.eq}})};
			for(auto r:helper(ParserState({t,power.head})))
				ret.push_back(r);}
		return ret;
	};
	// power+(('*'?|'/')+power)*
	for(auto power:parse_power(formula,head,allow_leading_unary))
		for(auto rest:helper(power))
			Return(rest.eq,rest.head);
}

Parser(sum){
	std::function<vPS(ParserState)> helper=[&](ParserState ps)->vPS{
		// ([+-]+product)*

		// Builds tree from left so that division doesn't invert everything.
		vPS ret;
		ret.push_back(ps);
		consume_spaces(ps.head);
	  if(ps.head>=formula.size()) return ret;
		if(formula[ps.head]=='+' || formula[ps.head]=='-'){
			auto op=formula[ps.head];
			for(auto product:parse_product(formula,ps.head+1,true)){
					Equation t={Equation::Op_node({op,{ps.eq},{product.eq}})};
					for(auto r:helper(ParserState({t,product.head})))
						ret.push_back(r);
			}}
		return ret;
	};
	// product+([+-]+product)*
	for(auto product:parse_product(formula,head,allow_leading_unary))
		for(auto rest:helper(product))
			Return(rest.eq,rest.head);
}

Parser(expression){
	// good enough for now
	for(auto sum:parse_sum(formula,head,allow_leading_unary))
		Return(sum.eq,sum.head);
}

Parser(braces){
	// '{'+expression+'}'
	unused(allow_leading_unary);
	consume_spaces(head);
	if(head>=formula.size()) return;
	if(formula[head++]!='{') return;
	for(auto eq:parse_expression(formula,head,true)){
		auto h=eq.head;
		consume_spaces(h);
		if(h>=formula.size()) continue;
		if(formula[h]!='}') continue;
		Return(eq.eq,h+1);
	}
}

Parser(bracketed){
	// '['+expression+']'
	unused(allow_leading_unary);
	consume_spaces(head);
	if(head>=formula.size()) return;
	if(formula[head++]!='[') return;
	for(auto eq:parse_expression(formula,head,true)){
		auto h=eq.head;
		consume_spaces(h);
		if(h>=formula.size()) continue;
		if(formula[h]!=']') continue;
		Return(eq.eq,h+1);
	}
}


Parser(parenthetical){
	// '('+expression+')'
	unused(allow_leading_unary);
	consume_spaces(head);
	if(head>=formula.size()) return;
	if(formula[head++]!='(') return;
	for(auto eq:parse_expression(formula,head,true)){
		auto h=eq.head;
		consume_spaces(h);
		if(h>=formula.size()) continue;
		if(formula[h]!=')') continue;
		Return(eq.eq,h+1);
	}
}

Equation parse_formula(string const& formula) {
	for(auto eq: parse_expression(formula,0,true))
		if(eq.head == formula.size())
			return eq.eq;
	throw "Unable to Parse";
}
