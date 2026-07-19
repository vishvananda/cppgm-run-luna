// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <vector>
#include <string>
#include <stdexcept>
#include <fstream>
#include <iostream>

using namespace std;

#include "nsdecl_parser.h"

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
		if (!out) throw logic_error("unable to open output file");

		out << nsrcfiles << " translation units" << endl;

		for (size_t i = 0; i < nsrcfiles; i++)
		{
			string srcfile = args[i+2];
			out << "start translation unit " << srcfile << endl;
			// EmitNSDeclTranslationUnit writes the global namespace and all of
			// its declarations.  Keep the per-file header here so the output
			// preserves the exact command-line path spelling.
			EmitNSDeclTranslationUnit(srcfile, out);
			out << "end translation unit" << endl;
		}
		return EXIT_SUCCESS;
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
