#include "pa11_semantics_analyzer.h"

} // namespace

void EmitPA11Types(const CPPGMAstNodePtr& translation_unit, ostream& out)
{
	Analyzer analyzer;
	analyzer.Analyze(translation_unit);
	analyzer.Print(out);
}
