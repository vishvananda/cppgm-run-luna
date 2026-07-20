#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

vector<CPPGMAstNodePtr> ExpandPA18Templates(const vector<CPPGMAstNodePtr>& translation_units)
{
	PA18TemplateExpander expander;
	return expander.Run(translation_units);
}
