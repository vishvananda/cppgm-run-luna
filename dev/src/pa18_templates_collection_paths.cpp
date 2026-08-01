#include "pa18_templates_collection.h"
#include "pa18_templates_rewrite.h"

using namespace std;

namespace pa18_templates_internal {

string CollapseRepeatedQualifiedPath(string value)
{
	// A repeated qualified path is only meaningful at namespace/class scope.
	// Do not compare a suffix that starts inside a template argument list with
	// the following scope component: `Trait<T>::value>::type` contains the
	// textual sequence `value>::value>::`, but it is not `value>::value>::type`.
	// The old textual scan collapsed that sequence and silently changed a
	// non-type member expression into a different type-id.
	const auto template_depth = [&value](size_t end) {
		int depth = 0;
		for(size_t position = 0; position < end; ++position) {
			if(value[position] == '<' && IsTemplateAngleOpen(value, position)) ++depth;
			else if(value[position] == '>' && depth > 0 &&
				IsTemplateAngleClose(value, position)) --depth;
		}
		return depth;
	};
	bool changed = false;
	do {
		changed = false;
		for(size_t start = 0; !changed && start < value.size(); ++start) {
			if(template_depth(start) != 0) continue;
			for(size_t separator = value.find("::", start);
				separator != string::npos;
				separator = value.find("::", separator + 2)) {
				if(template_depth(separator) != 0) continue;
				const size_t prefix_size = separator + 2 - start;
				if(separator + 2 + prefix_size > value.size() ||
					value.compare(separator + 2, prefix_size, value, start,
						prefix_size) != 0) continue;
				value.erase(separator + 2, prefix_size);
				changed = true;
				break;
			}
		}
	} while(changed);
	return value;
}

} // namespace pa18_templates_internal
