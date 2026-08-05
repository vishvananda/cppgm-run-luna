#pragma once
#include "pa11_semantics_analyzer.h"

void PA11BeginConstantFunctionReturn(Analyzer* analyzer, Binding* function);
void PA11EndConstantFunctionReturn(Analyzer* analyzer);
Binding* PA11FindGeneratedConstructor(Analyzer* analyzer, const TypePtr& type,
	 size_t argument_count, Scope* caller_scope);
TypePtr PA11FindMemberObjectType(Analyzer* analyzer,
	 const CPPGMAstNodePtr& object, const ConstantValue& receiver, Scope* scope);
TypePtr PA11FindGeneratedConstructorOwner(Analyzer* analyzer, Binding* function,
	 Scope* scope);

bool PA11NoexceptCall(Analyzer& analyzer, const CPPGMAstNodePtr& call,
	Scope* scope);
