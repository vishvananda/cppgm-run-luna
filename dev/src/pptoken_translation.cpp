#include <vector>

#include "pptoken_translation.h"

using namespace std;

vector<SourceUnit> BuildSourceUnits(const vector<int>& decoded,
	const vector<bool>& raw_spans)
{
	vector<SourceUnit> source;
	source.reserve(decoded.size());
	for (size_t i = 0; i < decoded.size(); ++i)
		source.push_back(SourceUnit(decoded[i], raw_spans[i], i, i + 1));
	return source;
}

bool AddTranslatedRawSpans(const vector<SourceUnit>& source,
	vector<bool>* raw_spans)
{
	// Hide already-protected raw bodies while looking for raw literals whose
	// prefixes were formed by an earlier translation step.  The sentinel is
	// not a source code point and therefore cannot start or complete a token.
	const int RawSentinel = -2;
	vector<int> probe;
	probe.reserve(source.size());
	for (size_t i = 0; i < source.size(); ++i)
		probe.push_back(source[i].raw ? RawSentinel : source[i].code_point);

	const vector<bool> discovered = MarkRawLiteralSpans(probe);
	bool added = false;
	for (size_t begin = 0; begin < discovered.size(); ++begin)
	{
		if (!discovered[begin] ||
			(begin != 0 && discovered[begin - 1]))
			continue;

		const int prefix_length = RawPrefixLength(probe, begin);
		if (prefix_length < 0)
			continue;
		const size_t quote = begin +
			static_cast<size_t>(prefix_length);
		size_t end = begin;
		while (end < discovered.size() && discovered[end])
			++end;

		const size_t body_begin = quote + 1;
		size_t open_paren = body_begin;
		for (; open_paren < end && probe[open_paren] != '('; ++open_paren)
		{}
		if (open_paren == end)
			continue;

		// A transformed delimiter was not a valid raw delimiter in the
		// original source.  Only the prefix may be formed by translation;
		// otherwise an ordinary token could be reclassified as raw.
		bool original_delimiter = true;
		for (size_t i = body_begin; i < open_paren; ++i)
		{
			if (source[i].origin_begin == SourceUnit::NoOrigin ||
				source[i].origin_end != source[i].origin_begin + 1)
			{
				original_delimiter = false;
				break;
			}
		}
		if (!original_delimiter)
			continue;

		// The prefix and its quote remain ordinary translated source.  The
		// delimiter and body are the portion that must be protected on the
		// next translation pass.
		for (size_t i = body_begin; i < end; ++i)
		{
			const size_t origin_begin = source[i].origin_begin;
			const size_t origin_end = source[i].origin_end;
			if (origin_begin == SourceUnit::NoOrigin ||
				origin_end == SourceUnit::NoOrigin)
				continue;
			for (size_t origin = origin_begin;
				origin < origin_end && origin < raw_spans->size(); ++origin)
			{
				if (!(*raw_spans)[origin])
				{
					(*raw_spans)[origin] = true;
					added = true;
				}
			}
		}
	}
	return added;
}
