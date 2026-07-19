// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <vector>
#include <string>
#include <stdexcept>
#include <fstream>
#include <iostream>

#include "exceptions.h"
#include "posttoken_semantics.h"
#include "preprocessor_engine.h"
#include "recog_parser.h"

using namespace std;

void DoRecog(const string& path)
{
	const vector<PostPPToken> tokens = PreprocessSourceFile(path);
	if (!ValidatePostTokens(tokens))
		throw logic_error("invalid token in preprocessed sequence");
	if (!RecognizePA6(tokens))
		throw logic_error("token sequence is not a translation-unit");
}

int main(int argc, char** argv)
{
	try
	{
		vector<string> args;

		for (int i = 1; i < argc; i++)
			args.emplace_back(argv[i]);

		if (args.size() < 3 || args[0] != "-o")
			throw logic_error("invalid usage");

		string outfile = args[1];
		size_t nsrcfiles = args.size() - 2;

		ofstream out(outfile);
		if (!out)
			throw logic_error("unable to open output file");

		out << "recog " << nsrcfiles << endl;

		for (size_t i = 0; i < nsrcfiles; i++)
		{
			string srcfile = args[i+2];

			try
			{
				DoRecog(srcfile);
				out << srcfile << " OK" << endl;
			}
			catch (const exception& e)
			{
				cerr << e.what() << endl;
				out << srcfile << " BAD" << endl;
			}
		}
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
