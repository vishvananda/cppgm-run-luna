// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <iostream>
#include <sstream>
#include <string>

using namespace std;

#include "ctrlexpr.h"
#include "exceptions.h"

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;
	try
	{
		ostringstream input;
		input << cin.rdbuf();
		RunCtrlExpr(input.str());
		return EXIT_SUCCESS;
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
