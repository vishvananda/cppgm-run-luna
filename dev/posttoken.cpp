#include <iostream>
#include <sstream>
#include <string>

#include "exceptions.h"
#include "posttoken_lexer.h"
#include "posttoken_semantics.h"

using namespace std;

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;
	try
	{
		ostringstream input;
		input << cin.rdbuf();
		RunPostToken(LexPostPPSource(input.str()));
		return EXIT_SUCCESS;
	}
	catch (const exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
