#include <iostream>
#include <sstream>

#include "exceptions.h"
#include "macro_engine.h"

using namespace std;

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;
	try
	{
		ostringstream input;
		input << cin.rdbuf();
		RunMacro(input.str());
		return EXIT_SUCCESS;
	}
	catch (const exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
