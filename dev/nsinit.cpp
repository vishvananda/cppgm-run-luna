// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
#include <fstream>

using namespace std;

#include "nsinit_parser.h"

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
		vector<string> source_paths;
		for (size_t i = 0; i < nsrcfiles; ++i)
			source_paths.push_back(args[i + 2]);
		vector<unsigned char> program_image;
		BuildNSInitImage(source_paths, &program_image);
		ofstream out(outfile.c_str(), ios::binary);
		if (!out) throw logic_error("unable to open output file");
		if (!program_image.empty())
			out.write(reinterpret_cast<const char*>(program_image.data()),
				static_cast<streamsize>(program_image.size()));
		if (!out) throw logic_error("unable to write output file");
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
