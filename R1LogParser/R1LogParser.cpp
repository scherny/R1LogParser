// R1LogParser.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <fstream>

bool ParseLine(char const* buf)
{
	return true;
}

int _tmain(int argc, _TCHAR* argv [])
{
	if ( argc != 2 )
	{
		std::cout << "No log" << std::endl;
		return 1;
	}

	std::ifstream log_file(argv[1]);

	if ( !log_file )
	{
		std::cout << "Can't open log file: " << argv[1] << std::endl;
	}

	char buf[10*1024];

	while ( log_file.getline(buf, sizeof(buf)-1) )
	{
		if ( ParseLine(buf) )
		{
			std::cout << buf << std::endl;
		}
	}
	return 0;
}

