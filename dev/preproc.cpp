// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

#include "exceptions.h"
#include "posttoken_semantics.h"
#include "preprocessor_engine.h"

namespace {

void EmitPostTokens(const vector<PostPPToken>& tokens, ostream& output)
{
	ostringstream captured;
	streambuf* old = cout.rdbuf(captured.rdbuf());
	try
	{
		RunPostToken(tokens);
	}
	catch (...)
	{
		cout.rdbuf(old);
		throw;
	}
	cout.rdbuf(old);
	const string text = captured.str();
	istringstream lines(text);
	string line;
	while (getline(lines, line))
		if (line.compare(0, 8, "invalid ") == 0)
			throw logic_error("invalid token in preprocessed sequence");
	output << text;
}

} // namespace

int main(int argc, char** argv)
{
	try
	{
		if (argc < 4 || string(argv[1]) != "-o")
			throw logic_error("invalid usage");
		const string outfile = argv[2];
		const size_t source_count = static_cast<size_t>(argc - 3);

		ofstream output(outfile.c_str());
		if (!output) throw logic_error("unable to open output file");
		output << "preproc " << source_count << '\n';
		for (size_t i = 0; i < source_count; ++i)
		{
			const string path = argv[i + 3];
			output << "sof " << path << '\n';
			const vector<PostPPToken> tokens = PreprocessSourceFile(path);
			EmitPostTokens(tokens, output);
		}
		return EXIT_SUCCESS;
	}
	catch (const exception& error)
	{
		cerr << "ERROR: " << error.what() << endl;
		return EXIT_FAILURE;
	}
}
