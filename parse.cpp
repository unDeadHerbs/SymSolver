#include "parse.hpp"
#include <optional>
#include <iostream> // for std::cerr
#include <sstream>
#include <functional>
#include <cstring>

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

struct SimpleParserState{
	string token;
	size_t head;
};

typedef std::vector<ParserState> vPS;
typedef std::vector<SimpleParserState> vSPS;
vSPS operator+(vSPS lhs,vSPS rhs){for(auto e:rhs)lhs.push_back(e);return lhs;} // TODO: Ranges V3

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

template<char C>
vSPS parse_sym(string const& formula,size_t head,bool consume_white_space=true){
	if(consume_white_space) consume_spaces(head);
	if(head>=formula.size()) return {};
	if(formula[head]==C) return {{{C},head+1}};
	return {};}

vSPS parse_sym(string const sym, string const& formula,size_t head,bool consume_white_space=true){
	if(consume_white_space) consume_spaces(head);
	if(head>=formula.size()) return {};
	if(formula.substr(head).starts_with(sym)) return {{sym,head+sym.size()}};
	return {};}

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
	for(auto h:parse_sym<'{'>(formula,head)){
		std::string ret(h.token);
		while(formula[h.head]!='}'){ // TODO: Use std::find and substr
			if(h.head>=formula.size()){
				std::cerr <<"Error: LaTeX group started but not ended." << std::endl;
				return;
			}
			ret += formula[h.head++];
		}
		ret += formula[h.head++];
		Return(Equation::Variable({ret}),h.head);
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
	if(allow_leading_unary)
		for(auto h:parse_sym<'-'>(formula,head))
			for(auto var:parse_variable(formula,h.head,false))
				Return(Equation({Equation::Operator::MULTIPLY,Equation(-1),var.eq}),var.head);
	if(head<formula.size() && std::isalpha(formula[head])){
		std::string var({formula[head++]});
		for(auto under:parse_sym<'_'>(formula,head))
			for(auto nxt:parse_latex_basic(formula,under.head,allow_leading_unary)){
				auto var2=var+under.token+std::get<Equation::Variable>(nxt.eq.value).name;
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
	for(auto h:parse_sym("\\sqrt",formula,head)){
		for(auto tmplte:parse_bracketed(formula,h.head,true))
			for(auto body:parse_space_term_or_brace(formula,tmplte.head,true))
				Return(Equation::F_node({"\\sqrt",{tmplte.eq},{body.eq}}),body.head);
		for(auto body:parse_space_term_or_brace(formula,h.head,true))
			Return(Equation::F_node({"\\sqrt",{body.eq}}),body.head);}
	for(auto h:parse_sym("\\ln",formula,head))
		for(auto body:parse_space_term_or_brace(formula,h.head,true))
			Return(Equation::F_node({"\\ln",{body.eq}}),body.head);
	for(auto h:parse_sym("\\log",formula,head)){
		for(auto h2:parse_sym<'_'>(formula,h.head)){
			for(auto base:parse_term(formula,h2.head,false))
				for(auto body:parse_space_term_or_brace(formula,base.head,true))
					Return(Equation::F_node({{{base.eq},{}},"\\log",{body.eq}}),body.head);}
		for(auto body:parse_space_term_or_brace(formula,h.head,true))
			Return(Equation::F_node({"\\log",{body.eq}}),body.head);}

	// Trig Functions
	for(auto h:parse_sym("\\exp",formula,head))
		for(auto body:parse_space_term_or_brace(formula,h.head,true))
			Return(Equation::F_node({"\\exp",{body.eq}}),body.head);
	for(auto h:parse_sym("\\sin",formula,head))
		for(auto body:parse_space_term_or_brace(formula,h.head,true))
			Return(Equation::F_node({"\\sin",{body.eq}}),body.head);
	for(auto h:parse_sym("\\cos",formula,head))
		for(auto body:parse_space_term_or_brace(formula,h.head,true))
			Return(Equation::F_node({"\\cos",{body.eq}}),body.head);
	for(auto h:parse_sym("\\tan",formula,head))
		for(auto body:parse_space_term_or_brace(formula,h.head,true))
			Return(Equation::F_node({"\\tan",{body.eq}}),body.head);
}

Parser(constant){
	unused(allow_leading_unary);
	for(auto h:parse_sym("\\pi",formula,head))
		Return(Equation::Constant({"pi"}),h.head);
	for(auto h:parse_sym("\\phi",formula,head))
		Return(Equation::Constant({"phi"}),h.head);
	for(auto h:parse_sym("i",formula,head))
		Return(Equation::Constant({"i"}),h.head);
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
		for(auto h:parse_sym<'^'>(formula,term.head))
			for(auto exp:parse_power(formula,h.head,true))
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
		for(auto op:parse_sym<'*'>(formula,ps.head) + parse_sym<'/'>(formula,ps.head)){
			for(auto power:parse_power(formula,ps.head+1,true)){
					Equation t={Equation::Op_node({op.token[0],{ps.eq},{power.eq}})};
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
		for(auto op:parse_sym<'+'>(formula,ps.head) + parse_sym<'-'>(formula,ps.head)){
			for(auto product:parse_product(formula,ps.head+1,true)){
					Equation t={Equation::Op_node({op.token[0],{ps.eq},{product.eq}})};
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
	for(auto h:parse_sym<'{'>(formula,head))
		for(auto eq:parse_expression(formula,h.head,true))
			for(auto h2:parse_sym<'}'>(formula,eq.head))
				Return(eq.eq,h2.head);}

Parser(bracketed){
	// '['+expression+']'
	unused(allow_leading_unary);
	for(auto h:parse_sym<'['>(formula,head))
		for(auto eq:parse_expression(formula,h.head,true))
			for(auto h2:parse_sym<']'>(formula,eq.head))
				Return(eq.eq,h2.head);}


Parser(parenthetical){
	// '('+expression+')'
	unused(allow_leading_unary);
	for(auto h:parse_sym<'('>(formula,head))
		for(auto eq:parse_expression(formula,h.head,true))
			for(auto h2:parse_sym<')'>(formula,eq.head))
				Return(eq.eq,h2.head);}

Equation parse_formula(string const& formula) {
	for(auto eq: parse_expression(formula,0,true))
		if(eq.head == formula.size())
			return eq.eq;
	// TODO: If there are multiple, parse ambiguous.
	throw "Unable to Parse";
}
