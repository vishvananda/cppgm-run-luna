#include "pa14_lowering.h"

using namespace std;
namespace cppgm_pa14_lowering {

string PA14Lowerer::EmitLiteralAddress(const CPPGMAstNodePtr& node)
{
	const string symbol = InternString(node->value);
	const string temp = new_temp();
	AddInstruction(temp + " = addr @" + symbol);
	return temp;
}

} // namespace cppgm_pa14_lowering
