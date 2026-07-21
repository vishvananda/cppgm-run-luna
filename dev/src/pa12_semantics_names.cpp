#include "pa12_semantics_support.h"

namespace {

bool IsSyntheticAnonymousNamespace(const string& component)
{
	const string prefix = "_GLOBAL__N_";
	if (component.compare(0, prefix.size(), prefix) != 0 ||
		component.size() == prefix.size()) return false;
	for (size_t i = prefix.size(); i < component.size(); ++i)
		if (component[i] < '0' || component[i] > '9') return false;
	return true;
}

}

string PA12PublicQualifiedName(const string& raw)
{
	string result;
	size_t begin = 0;
	while (begin <= raw.size()) {
		const size_t separator = raw.find("::", begin);
		const size_t end = separator == string::npos ? raw.size() : separator;
		const string component = raw.substr(begin, end - begin);
		if (!component.empty() && !IsSyntheticAnonymousNamespace(component)) {
			if (!result.empty()) result += "::";
			result += component;
		}
		if (separator == string::npos) break;
		begin = separator + 2;
	}
	return result;
}
