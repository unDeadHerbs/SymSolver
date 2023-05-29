#include "parse.hpp"
#include <optional>
#include <iostream> // for std::cerr
#include <sstream>
#include <functional>
#include <cstring>
#include <concepts>

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
	size_t head;
};

ParserState operator+(Equation const& eq,ParserState ps){
	ps.eq=eq;
	return ps;
}

struct SimpleParserState{
	string token;
	size_t head;
};

typedef std::vector<ParserState> vPS;
vPS& operator+=(vPS& lhs,vPS rhs){for(auto e:rhs)lhs.push_back(e);return lhs;} // TODO: Ranges V3
vPS operator+(vPS lhs,vPS rhs){for(auto e:rhs)lhs.push_back(e);return lhs;} // TODO: Ranges V3
typedef std::vector<SimpleParserState> vSPS;
vSPS operator+(vSPS lhs,vSPS rhs){for(auto e:rhs)lhs.push_back(e);return lhs;} // TODO: Ranges V3

#define unused(X) (void)X
// Return Internal : For the base Parsers
#define ReturnI(EQ,H) _ret.push_back({EQ,H})
// Return : For modifying the equation at HEAD
#define Return(PS) _ret.push_back(PS)
// Return Vector : for returning a whole vector of Parser States
#define ReturnV(VPS) _ret+=VPS
// Return Parser : for directly calling another parser and returning all values
#define ReturnP(P) ReturnV((P)(formula,head,allow_leading_unary))
template<typename T>
concept Parser=requires(T parser,string const& formula,size_t head,bool allow_leading_unary){
	{parser(formula,head,allow_leading_unary) } -> std::convertible_to<vPS>;
};
#define Parser_Proto(NAME)																							\
	struct parse_##NAME##_helper{																					\
		vPS _ret;																														\
		void internal(string const&,size_t,bool);														\
	};																																		\
	struct Parser_##NAME{																									\
	vPS operator()																												\
	(string const& formula,size_t head,bool allow_leading_unary) const{		\
		DB("Entering "<<#NAME << ": " <<																		\
			 formula.substr(0,head)<<" || "<<formula.substr(head));						\
		parse_##NAME##_helper h;																						\
		h.internal(formula,head,allow_leading_unary);												\
		DB(" Leaving " << #NAME << ": "<<h._ret.size());										\
		DBv(h._ret);																												\
		return h._ret;																											\
	}} parse_##NAME
#define Parser_Impl(NAME)																								\
	void parse_##NAME##_helper::internal																	\
	(string const& formula,size_t head,bool allow_leading_unary)
#define Parser(NAME) Parser_Proto(NAME);Parser_Impl(NAME)

struct Parser_Generated{
	std::function<vPS(string const&,size_t,bool)> internal;
	vPS operator()(string const& formula,size_t head,bool allow_leading_unary) const{
		return internal(formula,head,allow_leading_unary);
	};
	Parser_Generated(std::function<vPS(string const&,size_t,bool)> i):internal(i){}
};

// TODO Next: Overload `Parser | Parser` to make a Parser.
Parser_Generated operator|(Parser auto lhs,Parser auto rhs){
	// TODO: Reference or move here?  That does make use-after-free a problem.
	return {[=](string const& formula,size_t head,bool allow_leading_unary){
		return lhs(formula,head,allow_leading_unary)+rhs(formula,head,allow_leading_unary);
	}};
}
// - join the outputs of each
// TODO Next: Overload `Parser + function` to return a Parser.
// - for each in parser return, run the function and join the outputs.

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
			ReturnI(v,formula.size());
		else
			ReturnI(v,head+buf.tellg());
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
		ReturnI(Equation::Variable({ret}),h.head);
	}
	for(auto& on:parse_number(formula,head,false))
		// Just grabbing the length.
		Return(Equation::Variable({formula.substr(head,on.head)})+on);
	if(_ret.size()==0 && formula[head]!='{'){ // TODO: Don't touch the internals.
		std::cerr <<"Warning: LaTeX group requested but not found, using one char." << std::endl;
		ReturnI(Equation::Variable({string({formula[head]})}),head+1);
	}
}

Parser(variable){
	consume_spaces(head);
	if(allow_leading_unary)
		for(auto h:parse_sym<'-'>(formula,head))
			for(auto var:parse_variable(formula,h.head,false))
				Return(Equation({Equation::Operator::MULTIPLY,Equation(-1),var.eq})+var);
	if(head<formula.size() && std::isalpha(formula[head])){
		std::string var({formula[head++]});
		for(auto under:parse_sym<'_'>(formula,head))
			for(auto nxt:parse_latex_basic(formula,under.head,allow_leading_unary)){
				auto var2=var+under.token+std::get<Equation::Variable>(nxt.eq.value).name;
				Return(Equation({Equation::Variable({var2})})+nxt);
			}
		if(var!="i") // The imaginary unit isn't a variable.
		  ReturnI({Equation::Variable({var})},head);
	}
}

Parser_Proto(parenthetical);
Parser_Proto(expression);
Parser_Proto(bracketed);
Parser_Proto(braces);
Parser_Proto(term);

Parser(space_term_or_brace){
	if(head>=formula.size()) return;
	bool has_space = formula[head]==' ';
	consume_spaces(head);
	ReturnP(parse_braces);
	if(has_space)
		ReturnP(parse_term);}

Parser(named_operator){
	consume_spaces(head);
	unused(allow_leading_unary);
	// Basic Functions
	for(auto h:parse_sym("\\sqrt",formula,head)){
		for(auto tmplte:parse_bracketed(formula,h.head,true))
			for(auto body:parse_space_term_or_brace(formula,tmplte.head,true))
				Return(Equation::F_node({"\\sqrt",{tmplte.eq},{body.eq}})+body);
		for(auto body:parse_space_term_or_brace(formula,h.head,true))
			Return(Equation::F_node({"\\sqrt",{body.eq}})+body);}
	for(auto h:parse_sym("\\ln",formula,head))
		for(auto body:parse_space_term_or_brace(formula,h.head,true))
			Return(Equation::F_node({"\\ln",{body.eq}})+body);
	for(auto h:parse_sym("\\log",formula,head)){
		for(auto h2:parse_sym<'_'>(formula,h.head)){
			for(auto base:parse_term(formula,h2.head,false))
				for(auto body:parse_space_term_or_brace(formula,base.head,true))
					ReturnI(Equation::F_node({{{base.eq},{}},"\\log",{body.eq}}),body.head);}// TODO: Bidnings
		for(auto body:parse_space_term_or_brace(formula,h.head,true))
			Return(Equation::F_node({"\\log",{body.eq}})+body);}

	// Trig Functions
	for(auto h:parse_sym("\\exp",formula,head))
		for(auto body:parse_space_term_or_brace(formula,h.head,true))
			Return(Equation::F_node({"\\exp",{body.eq}})+body);
	for(auto h:parse_sym("\\sin",formula,head))
		for(auto body:parse_space_term_or_brace(formula,h.head,true))
			Return(Equation::F_node({"\\sin",{body.eq}})+body);
	for(auto h:parse_sym("\\cos",formula,head))
		for(auto body:parse_space_term_or_brace(formula,h.head,true))
			Return(Equation::F_node({"\\cos",{body.eq}})+body);
	for(auto h:parse_sym("\\tan",formula,head))
		for(auto body:parse_space_term_or_brace(formula,h.head,true))
			Return(Equation::F_node({"\\tan",{body.eq}})+body);
}

Parser(constant){
	unused(allow_leading_unary);
	for(auto h:parse_sym("\\pi",formula,head))
		ReturnI(Equation::Constant({"pi"}),h.head);
	for(auto h:parse_sym("\\phi",formula,head))
		ReturnI(Equation::Constant({"phi"}),h.head);
	for(auto h:parse_sym("i",formula,head))
		ReturnI(Equation::Constant({"i"}),h.head);
}

Parser(Bracket_Substitution){
	// [expr]_{var=exp}
	for(auto body:parse_bracketed(formula,head,allow_leading_unary)){
		for(auto u:parse_sym("^{",formula,body.head))
			for(auto up:parse_expression(formula,u.head,true))
				for(auto b:parse_sym("}_{",formula,up.head))
					for(auto v:parse_variable(formula,b.head,true))
						for(auto e:parse_sym<'='>(formula,v.head))
							for(auto lower:parse_expression(formula,e.head,true))
								for(auto c:parse_sym<'}'>(formula,lower.head))
									ReturnI(Equation::F_node({"binding",std::get<Equation::Variable>(v.eq.value),
									                         {{lower.eq},{up.eq}},{body.eq}}),c.head);
		for(auto b:parse_sym("_{",formula,body.head))
			for(auto v:parse_variable(formula,b.head,true))
				for(auto e:parse_sym<'='>(formula,v.head))
					for(auto lower:parse_expression(formula,e.head,true))
						for(auto c:parse_sym<'}'>(formula,lower.head))
							ReturnI(Equation::F_node({"binding",std::get<Equation::Variable>(v.eq.value),
							                         {{lower.eq},{}},{body.eq}}),c.head);
	}}

Parser_Impl(term){
	ReturnP(parse_parenthetical
	       |parse_Bracket_Substitution
	       |parse_number
	       |parse_constant
	       |parse_variable
	       |parse_named_operator);
}

Parser(power){
	for(auto term:parse_term(formula,head,allow_leading_unary)){
		consume_spaces(term.head);
		for(auto h:parse_sym<'^'>(formula,term.head))
			for(auto exp:parse_power(formula,h.head,true))
				ReturnI(Equation::Op_node({'^',term.eq,exp.eq}),exp.head); // TODO: Bindings
		Return(term);
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
					for(auto r:helper(t+power))
						ret.push_back(r);
					// TODO: make =t= a transform function and then use + to chain these.
					// TODO: ret+=(parse_power + t + helper)(formula,ps.head+1,true);
			}}
		for(auto power:parse_power(formula,ps.head,true)){
			Equation t={Equation::Op_node({'*',{ps.eq},{power.eq}})};
			for(auto r:helper(t+power))
				ret.push_back(r);}
		// TODO: make =t= a transform function and then use + to chain these.
		return ret;
	};
	// power+(('*'?|'/')+power)*
	for(auto power:parse_power(formula,head,allow_leading_unary))
		ReturnV(helper(power));
	// TODO: ReturnP(parse_power+helper);
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
					for(auto r:helper(t+product))
						ret.push_back(r);
			}}
		return ret;
	};
	// product+([+-]+product)*
	for(auto product:parse_product(formula,head,allow_leading_unary))
		ReturnV(helper(product));
	// TODO: ReturnP(parse_product+helper);
}

Parser_Impl(expression){
	// good enough for now
	ReturnP(parse_sum);
}

Parser_Impl(braces){
	// '{'+expression+'}'
	unused(allow_leading_unary);
	for(auto h:parse_sym<'{'>(formula,head))
		for(auto eq:parse_expression(formula,h.head,true))
			for(auto h2:parse_sym<'}'>(formula,eq.head))
				ReturnI(eq.eq,h2.head);}

Parser_Impl(bracketed){
	// '['+expression+']'
	unused(allow_leading_unary);
	for(auto h:parse_sym<'['>(formula,head))
		for(auto eq:parse_expression(formula,h.head,true))
			for(auto h2:parse_sym<']'>(formula,eq.head))
				ReturnI(eq.eq,h2.head);}


Parser_Impl(parenthetical){
	// '('+expression+')'
	unused(allow_leading_unary);
	for(auto h:parse_sym<'('>(formula,head))
		for(auto eq:parse_expression(formula,h.head,true))
			for(auto h2:parse_sym<')'>(formula,eq.head))
				ReturnI(eq.eq,h2.head);}

Equation parse_formula(string const& formula) {
	for(auto eq: parse_expression(formula,0,true))
		if(eq.head == formula.size())
			return eq.eq;
	// TODO: If there are multiple, parse ambiguous.
	throw "Unable to Parse";
}
