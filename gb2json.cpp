/* 
 * gb2json.cpp: GenBank to JSON converter
 * 
 * Copyright (c) 2019 Frank Buermann <fburmann@mrc-lmb.cam.ac.uk>
 * 
 * gbjson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <iostream>
#include <fstream> // ofstream
#include <string>
#include "gbjson.h"
#include "optionparser/optionparser.h"

enum optionIndex
{
	UNKNOWN,
	HELP,
	FORCE,
	VERSION
};

const option::Descriptor usage[] =
	{
		{UNKNOWN, 0, "", "", option::Arg::None, "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n"
												"~~ GenBank to JSON converter\n\n"
												"USAGE: gb2json [options] in.gb out.json\n\n"
												"Options:"},
		{HELP, 0, "h", "help", option::Arg::None, "  -h  --help      Print help."},
		{FORCE, 0, "f", "force", option::Arg::None, "  -f  --force     Input and output filenames can be the same."},
		{VERSION, 0, "v", "version", option::Arg::None, "  -v  --version   Print program version.\n"},
		{0, 0, 0, 0, 0, 0}};

int main(int argc, char *argv[])
{
	// Parse command line options
	argc -= (argc > 0);
	argv += (argc > 0); // skip program name argv[0]
	option::Stats stats(usage, argc, argv);
	auto options = std::make_unique<option::Option[]>(stats.options_max);
	auto buffer = std::make_unique<option::Option[]>(stats.buffer_max);
	option::Parser parse(usage, argc, argv, options.get(), buffer.get());

	if (parse.error())
	{
		option::printUsage(std::cout, usage);
		return 1;
	}

	if (options[HELP])
	{
		option::printUsage(std::cout, usage);
		return 0;
	}

	if (options[VERSION])
	{
		std::cout << "gb2json "
				  << "v" << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_PATCH << std::endl;
		return 0;
	}

	if (parse.nonOptionsCount() != 2)
	{
		option::printUsage(std::cout, usage);
		return 1;
	}

	std::string infile(parse.nonOptions()[0]);
	std::string outfile(parse.nonOptions()[1]);

	if (infile == outfile && !options[FORCE])
	{
		std::cout << "Input and output filenames must be different." << std::endl;
		return 1;
	}

	std::string gb, json;
	gberror err;

	// Read the input file
	fileToString(&infile, &gb, &err);
	if (err.flag)
	{
		std::cout << err.msg << std::endl;
		return 1;
	}

	// Convert the GenBank string to JSON
	gb2json(&gb, &json, &err);

	// Print to console
	if (err.flag)
	{
		std::cout << err.msg << std::endl;
		return 1;
	}
	else
	{
		std::ofstream output(outfile);
		output << json;
		output.close();
		std::cout << outfile << std::endl;
	}
}