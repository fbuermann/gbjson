/*
 * gbjson.cpp: GenBank<->JSON conversion library
 *
 * Copyright (c) 2019 Frank Buermann <fburmann@mrc-lmb.cam.ac.uk>
 *
 * gbjson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

/**
   * @mainpage gbjson - A GenBank to JSON converter
   *
   * @section Introduction
   *
   * This program interconverts GenBank and JSON formats. The program contains
   * two executables. <b>gb2json</b> converts GenBank to JSON,
   * and <b>json2gb</b> does the reverse.
   *
   * @section Usage Example usage
   * @subsection File2File File conversion
   * $ gb2json <i>in.gb</i> <i>out.json</i>
   *
   * $ json2gb <i>in.json</i> <i>out.gb</i>
   *
   * @subsection File2Stdout Write to stdout
   * $ gb2json <i>in.gb</i>
   *
   * $ json2gb <i>in.json</i>
   *
   * @section Building Building from source
   * Use <a href="https://cmake.org/">CMake</a> to build from source.
   *
   * @subsection UNIX UNIX-like operating systems
   * $ cd gbjson\n
   * $ mkdir build\n
   * $ cd build\n
   * $ cmake -G "Unix Makefiles" ..\n
   * $ make
   *
   * @subsection WIN32 Windows
   * Building from source has been tested with <a href="https://visualstudio.microsoft.com/">Visual Studio/MSVC 2019</a>
   * using <a href="https://cmake.org/">CMake</a>.
   *
   * @subsection Library Use as a library
   * Using <b>gbjson</b> as a C++ library is straight forward.
   * Include gbjson.h in your source code. The functions <i>gb2json</i> and <i>json2gb</i> are the API.
   */

#include <iostream>
#include <stdio.h> // fopen, fread
#include <string>
#include <sstream>   // stringstream
#include <algorithm> // find_if, remove
#include <cctype>	// isspace
#include <cmath>	 // ceil
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/reader.h"
#include "gbjson.h"

#define _CRT_SECURE_NO_WARNINGS // Bypass I/O *_s functions in MSVC

gberror::gberror() : flag(false) {}

/***************************************************************
 * File handling
 ***************************************************************/

static inline void fileSize(FILE *fp, size_t *len)
{
	size_t pos = ftell(fp);

	fseek(fp, 0, SEEK_END);
	*len = ftell(fp);

	fseek(fp, pos, SEEK_SET);
}

void fileToString(const std::string *filename, std::string *output, gberror *err)
{
	FILE *input = fopen(filename->c_str(), "r");

	if (!input)
	{
		err->flag = true;
		err->msg = "Failed to open ";
		err->msg.append(filename->c_str());
		err->source = "fileToString";
		return;
	}

	size_t len; // File size
	fileSize(input, &len);
	output->resize(len);

	len = fread(&(*output)[0], 1, len, input);
	output->resize(len); // Actual number of chars read

	fclose(input);
}
/***************************************************************
 * String handling
 ***************************************************************/

static std::string makeSpaces(int n)
{
	std::string spaces(n, ' ');
	return spaces;
}

static inline void stringTrimLeft(std::string *str)
{
	str->erase(str->begin(), std::find_if(str->begin(), str->end(), [](int c) { return !std::isspace(c); }));
}

static inline void stringTrimRight(std::string *str)
{
	str->erase(std::find_if(str->rbegin(), str->rend(), [](int c) { return !std::isspace(c); }).base(), str->end());
}

static inline void stringTrim(std::string *str)
{
	stringTrimLeft(str);
	stringTrimRight(str);
}

static inline void removeSpaces(std::string *str)
{
	str->erase(std::remove(str->begin(), str->end(), ' '), str->end());
}

/*
 * getline that handles \r, \r\n, and \n line endings for files moved
 * between platforms.
 * @param[in] is Input stream.
 * @param[out] line The line.
 */

static std::istream &safeGetline(std::istream &is, std::string &line)
{
	// This was snatched from
	// https://stackoverflow.com/questions/6089231/getting-std-ifstream-to-handle-lf-cr-and-crlf

	// The characters in the stream are read one-by-one using a std::streambuf.
	// That is faster than reading them one-by-one using the std::istream.
	// Code that uses streambuf this way must be guarded by a sentry object.
	// The sentry object performs various tasks,
	// such as thread synchronization and updating the stream state.

	line.clear();

	std::istream::sentry se(is, true);
	std::streambuf *sb = is.rdbuf();

	for (;;)
	{
		int c = sb->sbumpc();
		switch (c)
		{
		case '\n':
			return is;
		case '\r':
			if (sb->sgetc() == '\n')
				sb->sbumpc();
			return is;
		case std::streambuf::traits_type::eof():
			// Also handle the case when the last line has no line ending
			if (line.empty())
				is.setstate(std::ios::eofbit);
			return is;
		default:
			line += (char)c;
		}
	}
}

/**
 * Split a string into lines and left pad with whitespace
 * @param[in] input The string.
 * @param[out] block The padded block.
 * @param[in] leader Number of leading whitespaces.
 * @param[in] len Line length.
 * @param[in] offset Offset for the first line.
 */
static void blockPad(
	const std::string *input,
	std::string *block,
	int leader,
	int len,
	int offset)
{
	// Sanity check
	if (len < leader || len <= 0 || offset >= len - leader)
	{
		*block = "";
		return;
	}

	// Make the streams
	std::stringstream in(*input);
	std::stringstream out;
	std::string line;

	// Consume first input line. This does not get leading whitespace.
	safeGetline(in, line);

	int writeLen = len - leader;										// Length that contains parts of the string
	int nfrag = std::ceil(((double)line.length() + offset) / writeLen); // Number of fragments

	// First part that does not have leading whitespace.
	out << line.substr(0, writeLen - offset);
	out << "\n";

	// Remaining parts
	for (int i = 0; i < nfrag - 1; i++)
	{
		out << makeSpaces(leader);
		out << line.substr(writeLen - offset + i * writeLen, writeLen) << "\n";
	}

	// Consume remaining input lines
	while (!in.eof())
	{
		safeGetline(in, line);
		nfrag = std::ceil(((double)line.length()) / writeLen);

		for (int i = 0; i < nfrag; i++)
		{
			out << makeSpaces(leader);
			out << line.substr(i * writeLen, writeLen) << "\n";
		}
	}

	// Write result
	*block = out.str();
}

/***************************************************************
 * GenBank line processing
 ***************************************************************/

static void splitLine(const std::string *line, std::string *front, std::string *back)
{
	*front = line->substr(0, 10);
	*back = line->substr(12, line->size());
}

static void splitFeatureLine(const std::string *line, std::string *front, std::string *back)
{
	*front = line->substr(0, 21);
	*back = line->substr(21, line->size());
}

static void splitSequenceLine(const std::string *line, std::string *front, std::string *back)
{
	*front = line->substr(0, 10);
	*back = line->substr(10, line->size());
	removeSpaces(back);
}

static inline bool isInteger(const std::string *n)
{
	return !n->empty() && n->end() == std::find_if(n->begin(), n->end(), [](char c) { return !std::isdigit(c); });
}

static inline bool isLocus(const std::string *line)
{
	return line->length() >= 13 && line->substr(0, 5) == "LOCUS" && !isspace((*line)[12]);
}

static inline bool isKeyword(const std::string *line)
{
	return line->length() >= 13 && !isspace((*line)[0]) && isalpha((*line)[0]);
}

static inline bool isSubkeyword(const std::string *line)
{
	return line->length() >= 3 && (line->substr(0, 2) == "  ") && !isspace((*line)[2]);
}

static inline bool isSubsubkeyword(const std::string *line)
{
	return line->length() >= 4 && (line->substr(0, 3) == "   ") && !isspace((*line)[3]);
}

static inline bool isContinuation(const std::string *line)
{
	return line->length() >= 11 && line->substr(0, 11) == "           ";
}

static inline bool isFeatureHeader(const std::string *line)
{
	return line->length() >= 8 && line->substr(0, 8) == "FEATURES";
}

static inline bool isFeature(const std::string *line)
{
	return line->length() >= 6 && (line->substr(0, 5) == "     ") && !isspace((*line)[5]);
}

static inline bool isQualifier(const std::string *back)
{
	return (*back)[0] == '/';
}

static inline bool isOrigin(const std::string *line)
{
	return line->length() >= 6 && line->substr(0, 6) == "ORIGIN";
}

static inline bool isContig(const std::string *line)
{
	return line->length() >= 6 && line->substr(0, 6) == "CONTIG";
}

static bool isSequence(const std::string *line)
{
	if (line->length() < 11)
	{
		return false;
	}

	std::string n(line->substr(3, 6));
	stringTrimLeft(&n);

	return isInteger(&n) && std::isspace((*line)[9]) && !std::isspace((*line)[10]);
}

static inline bool isEnd(const std::string *line)
{
	return line->length() >= 2 && line->substr(0, 2) == "//";
}

// Function table for testing keyword level
bool (*isItemLevel[3])(const std::string *line) = {&isKeyword, &isSubkeyword, &isSubsubkeyword};

/***************************************************************
 * GenBank parsing
 ******************
 * All values are parsed into JSON strings since GenBank
 * is lacking a type system.
 ***************************************************************/

/**
   * Parse a GenBank LOCUS entry.
   * @param[in] gbstream The input string stream.
   * @param[out] line The line buffer.
   * @param[in] writer The JSON writer object.
   */
static void parseLocus(
	std::stringstream *gbstream,
	std::string *line,
	rapidjson::PrettyWriter<rapidjson::StringBuffer> *writer)
{
	std::string back(line->substr(12, line->length() - 12));
	writer->Key("LOCUS");
	writer->StartArray();
	writer->String(back.c_str(), back.length(), true);
	writer->StartArray(); // Dummy array for sub keywords
	writer->EndArray();   // Dummy array for sub keywords
	writer->EndArray();

	safeGetline(*gbstream, *line);
}

/**
 * Parse a GenBank KEYWORD entry.
 * @param[in] gbstream The input string stream.
 * @param[out] line The line buffer.
 * @param[in] writer The JSON writer object.
 */
static void parseKeyword(
	std::stringstream *gbstream,
	std::string *line,
	rapidjson::PrettyWriter<rapidjson::StringBuffer> *writer,
	int level // 0 for Keywords, 1 for Subkeywords, 2 for Subsubkeywords
)
{
	std::string front, back;
	splitLine(line, &front, &back);

	// Copy content into buffer and trim whitespace
	std::string buffer(back);
	bool whitespace = isspace(back.back());
	stringTrimRight(&buffer);
	if (whitespace)
	{
		buffer.append(" ");
	}
	stringTrim(&front);

	writer->Key(front.c_str(), front.length(), true);
	writer->StartArray(); // Top level array

	// Read the next line
	safeGetline(*gbstream, *line);

	// Push continuation lines into buffer
	while (isContinuation(line))
	{
		splitLine(line, &front, &back);
		whitespace = isspace(back.back());
		stringTrimRight(&back);
		if (whitespace)
		{
			back.append(" ");
		}
		buffer.append("\n");
		buffer.append(back);
		safeGetline(*gbstream, *line);
	}

	// Write buffer to JSON
	writer->String(buffer.c_str(), buffer.length(), true);

	// Parse Sub keywords
	writer->StartArray();
	if (level < 2)
	{ // Genbank allows 3 levels for keywords
		while (isItemLevel[level + 1](line))
		{
			writer->StartObject();
			parseKeyword(gbstream, line, writer, level + 1);
			writer->EndObject();
		}
	}
	writer->EndArray(); // Sub keywords
	writer->EndArray(); // Top level array
}

/**
 * Parse a GenBank qualifier entry.
 * @param[in] gbstream The input string stream.
 * @param[out] line The line buffer.
 * @param[in] writer The JSON writer object.
 */
static void parseQualifier(
	std::stringstream *gbstream,
	std::string *line,
	rapidjson::PrettyWriter<rapidjson::StringBuffer> *writer)
{
	// Push content into a buffer and trim whitespace
	std::string front, back;
	splitFeatureLine(line, &front, &back);

	std::string buffer(back);
	bool whitespace = isspace(back.back());
	stringTrimRight(&buffer);
	if (whitespace)
	{
		buffer.append(" ");
	}

	// Read the next line
	safeGetline(*gbstream, *line);

	// Push continuation lines into the buffer
	if (isContinuation(line))
	{
		splitFeatureLine(line, &front, &back);

		while (!isQualifier(&back) && isContinuation(line))
		{
			whitespace = isspace(back.back());
			stringTrimRight(&back);
			if (whitespace)
			{
				buffer.append(" ");
			}
			buffer.append(back);
			safeGetline(*gbstream, *line);

			if (isContinuation(line))
			{
				splitFeatureLine(line, &front, &back);
			}
			else
			{
				break;
			}
		}
	}

	// Find the qualifier delimiter if it exists
	size_t equalSignPos = buffer.find('=');

	// Write out the buffer
	writer->StartObject(); // Qualifier start

	if (equalSignPos == -1 || equalSignPos == buffer.length() - 1)
	{
		// No qualifier value
		writer->Key(buffer.substr(1, buffer.length()).c_str(), buffer.length() - 1, true);
		writer->Null();
	}
	else
	{
		// Key value pair
		std::string key(buffer.substr(1, equalSignPos - 1));
		std::string value(buffer.substr(equalSignPos + 1, buffer.length() - equalSignPos));
		writer->Key(key.c_str(), key.length(), true);
		writer->String(value.c_str(), value.length(), true);
	}

	writer->EndObject(); // Qualifier end
}

/**
 * Parse a GenBank feature entry.
 * @param[in] gbstream The input string stream.
 * @param[out] line The line buffer.
 * @param[in] writer The JSON writer object.
 */
static void parseFeature(
	std::stringstream *gbstream,
	std::string *line,
	rapidjson::PrettyWriter<rapidjson::StringBuffer> *writer)
{
	// Push content into a buffer
	std::string front, back;
	splitFeatureLine(line, &front, &back);

	std::string buffer(back);
	stringTrimRight(&buffer);
	stringTrim(&front);

	writer->StartObject(); // Feature start
	writer->Key(front.c_str(), front.length(), true);

	writer->StartArray();  // Qualifier array
	writer->StartObject(); // Location start
	writer->Key("Location");

	// Consume location continuation lines
	safeGetline(*gbstream, *line);

	while (isContinuation(line))
	{
		splitFeatureLine(line, &front, &back);

		if (isQualifier(&back))
		{
			break; // Exit loop if this is not a continuation line
		}
		else
		{
			bool whitespace = isspace(back.back());
			stringTrimRight(&back);
			if (whitespace)
			{
				back.append(" ");
			}
			buffer.append(back);
			safeGetline(*gbstream, *line);
		}
	}

	// Write location
	writer->String(buffer.c_str(), buffer.length(), true);
	writer->EndObject(); // Location end

	// Parse qualifiers
	while (isQualifier(&back) && isContinuation(line))
	{
		parseQualifier(gbstream, line, writer);
		if (isContinuation(line))
		{
			splitFeatureLine(line, &front, &back);
		}
	}

	writer->EndArray();  // Qualifier array
	writer->EndObject(); // Feature end
}

/**
 * Parse a GenBank feature table.
 * @param[in] gbstream The input string stream.
 * @param[out] line The line buffer.
 * @param[in] writer The JSON writer object.
 */
static void parseFeatures(
	std::stringstream *gbstream,
	std::string *line,
	rapidjson::PrettyWriter<rapidjson::StringBuffer> *writer)
{
	writer->Key("FEATURES");
	safeGetline(*gbstream, *line);

	writer->StartArray(); // Features array

	if (!isFeature(line))
	{ // No features present
		goto cleanup;
	}

	while (isFeature(line))
	{
		parseFeature(gbstream, line, writer);
	}

cleanup:
	writer->EndArray(); // Features array
}

/**
 * Parse a GenBank origin entry.
 * @param[in] gbstream The input string stream.
 * @param[out] line The line buffer.
 * @param[in] writer The JSON writer object.
 */
static void parseOrigin(
	std::stringstream *gbstream,
	std::string *line,
	rapidjson::PrettyWriter<rapidjson::StringBuffer> *writer)
{
	std::string back(line->substr(6, 79 - 6));
	stringTrimRight(&back);

	writer->Key("ORIGIN");
	writer->StartArray();
	if (back.empty())
	{ // No value
		writer->Null();
	}
	else
	{ // Entry
		writer->String(back.c_str(), back.length(), true);
	}
	writer->StartArray(); // Dummy array for sub keywords
	writer->EndArray();   // Dummy array for sub keywords
	writer->EndArray();
}

/**
 * Parse a GenBank sequence entry.
 * @param[in] gbstream The input string stream.
 * @param[out] line The line buffer.
 * @param[in] writer The JSON writer object.
 */
static void parseSequence(
	std::stringstream *gbstream,
	std::string *line,
	rapidjson::PrettyWriter<rapidjson::StringBuffer> *writer)
{
	std::string front, back, buffer;
	safeGetline(*gbstream, *line);

	if (isContig(line))
	{
		// Copy content into buffer and trim whitespace
		splitLine(line, &front, &back);
		buffer.append(back);
		stringTrimRight(&buffer);

		bool whitespace = isspace(back.back());
		if (whitespace)
		{
			buffer.append(" ");
		}

		// Consume contig lines
		safeGetline(*gbstream, *line);
		while (isContinuation(line))
		{

			splitLine(line, &front, &back);
			whitespace = isspace(back.back());
			stringTrimRight(&back);
			if (whitespace)
			{
				back.append(" ");
			}
			buffer.append("\n");
			buffer.append(back);
			safeGetline(*gbstream, *line);
		}

		// Write the data
		writer->Key("CONTIG");
		writer->StartArray();
		if (buffer.empty())
		{
			writer->Null();
		}
		else
		{
			writer->String(buffer.c_str(), buffer.length(), true);
		}
		writer->StartArray(); // Dummy array for sub keywords
		writer->EndArray();   // Dummy array for sub keywords
		writer->EndArray();
	}
	else if (isSequence(line))
	{
		// Consume the sequence lines
		while (isSequence(line))
		{
			splitSequenceLine(line, &front, &back);
			buffer.append(back);
			safeGetline(*gbstream, *line);
		}

		// Write the data
		writer->Key("SEQUENCE");
		writer->StartArray();
		if (buffer.empty())
		{
			writer->Null();
		}
		else
		{
			writer->String(buffer.c_str(), buffer.length(), true);
		}
		writer->StartArray(); // Dummy array for sub keywords
		writer->EndArray();   // Dummy array for sub keywords
		writer->EndArray();
	}
}

/**
 * Delegator function for parsing GenBank items.
 * @param[in] gbstream The input string stream.
 * @param[out] line The line buffer.
 * @param[in] writer The JSON writer object.
 */
static void parseItem(
	std::stringstream *gbstream,
	std::string *line,
	rapidjson::PrettyWriter<rapidjson::StringBuffer> *writer)
{

	if (isLocus(line))
	{
		writer->StartArray(); // Start the GenBank array

		writer->StartObject();
		parseLocus(gbstream, line, writer);
		writer->EndObject();
	}
	else if (isEnd(line))
	{
		writer->EndArray(); // Start the GenBank array
		safeGetline(*gbstream, *line);
	}
	else if (isOrigin(line))
	{
		writer->StartObject();
		parseOrigin(gbstream, line, writer);
		writer->EndObject();

		writer->StartObject();
		parseSequence(gbstream, line, writer);
		writer->EndObject();
	}
	else if (isKeyword(line) && !isFeatureHeader(line))
	{
		writer->StartObject();
		parseKeyword(gbstream, line, writer, 0);
		writer->EndObject();
	}
	else if (isFeatureHeader(line))
	{
		writer->StartObject();
		parseFeatures(gbstream, line, writer);
		writer->EndObject();
	}
	else
	{
		safeGetline(*gbstream, *line);
	}
}

/***************************************************************
* GenBank to JSON converter
***************************************************************/

/**
 * GenBank to JSON converter.
 * @param[in] gb The GenBank string.
 * @param[out] json The JSON string.
 * @param[out] err Error object.
 */
void gb2json(const std::string *gb, std::string *json, gberror *err)
{
	// Initialize stream object
	std::stringstream gbstream(*gb);
	std::string line;

	// Initialize the writer
	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

	// Start the parsing
	safeGetline(gbstream, line);
	writer.StartArray();

	while (!gbstream.eof())
	{
		parseItem(&gbstream, &line, &writer);
	}

	// Close the JSON array and write to string
	writer.EndArray();

	if (!writer.IsComplete())
	{
		err->flag = true;
		err->msg = "Incomplete GenBank";
		err->source = "gb2json";
	}
	else
	{
		*json = buffer.GetString();
	}
}

/***************************************************************
 * JSON to GenBank converter
 * This feeds a events from a JSON stream into a rapidjson handler
 * object. The handler builds the GenBank string.
 ***************************************************************/

/**
   * Handler constructor.
   */
JSONHandler::JSONHandler() : state(START), nwritten(0), skipStateUpdate(false) {}

// Unused handler functions
bool JSONHandler::Bool(bool b) { return true; }
bool JSONHandler::Int(int i) { return true; }
bool JSONHandler::Uint(unsigned i) { return true; }
bool JSONHandler::Int64(int64_t i) { return true; }
bool JSONHandler::Uint64(uint64_t i) { return true; }
bool JSONHandler::Double(double d) { return true; }
bool JSONHandler::RawNumber(const char *str, rapidjson::SizeType length, bool copy) { return true; }

/**
 * Update handler state based on a JSON key.
 * @param[in] key The key.
 */
void JSONHandler::updateState(const std::string key)
{
	if (state == QUALIFIER && key == "Location")
	{
		state = QUALIFIER_LOCATION;
	}
	else if (state == QUALIFIER_LOCATION)
	{
		state = QUALIFIER;
	}
	else if (key == "LOCUS")
	{
		state = LOCUS;
		skipStateUpdate = true;
	}
	else if (key == "ORIGIN")
	{
		state = ORIGIN;
		skipStateUpdate = true;
	}
	else if (key == "SEQUENCE")
	{
		state = SEQUENCE;
		skipStateUpdate = true;
	}
	else if (key == "CONTIG")
	{
		state = CONTIG;
		skipStateUpdate = true;
	}
	else if (key == "FEATURES")
	{
		state = FEATURE_HEADER;
	}
	else if (!skipStateUpdate && (state == KEYWORD || state == SUBKEYWORD || state == SUBSUBKEYWORD))
	{
		skipStateUpdate = true;
	}
	return;
}

bool JSONHandler::StartArray()
{
	switch (state)
	{
	case KEYWORD:
	{
		if (skipStateUpdate)
		{
			skipStateUpdate = false;
		}
		else
		{
			state = SUBKEYWORD;
		}
		break;
	}
	case SUBKEYWORD:
	{
		if (skipStateUpdate)
		{
			skipStateUpdate = false;
		}
		else
		{
			state = SUBSUBKEYWORD;
		}
		break;
	}
	case LOCUS:
	{
		if (skipStateUpdate)
		{
			skipStateUpdate = false;
		}
		else
		{
			state = LOCUSSUB;
		}
		break;
	}
	case ORIGIN:
	{
		if (skipStateUpdate)
		{
			skipStateUpdate = false;
		}
		else
		{
			state = ORIGINSUB;
		}
		break;
	}
	case SEQUENCE:
	{
		if (skipStateUpdate)
		{
			skipStateUpdate = false;
		}
		else
		{
			state = SEQUENCESUB;
		}
		break;
	}
	case CONTIG:
	{
		if (skipStateUpdate)
		{
			skipStateUpdate = false;
		}
		else
		{
			state = CONTIGSUB;
		}
		break;
	}
	case END:
	{
		state = START;
		break;
	}
	default:
		break;
	}
	return true;
}

bool JSONHandler::EndArray(rapidjson::SizeType elementCount)
{
	if (state == QUALIFIER || state == QUALIFIER_LOCATION)
	{
		state = FEATURE;
	}
	else if (state == FEATURE)
	{
		state = FEATURE_HEADER;
	}
	else if (state == FEATURE_HEADER)
	{
		state = KEYWORD;
	}
	else if (state == SUBSUBKEYWORD)
	{
		state = SUBKEYWORD;
	}
	else if (state == SUBKEYWORD)
	{
		state = KEYWORD;
	}
	else if (state == LOCUSSUB)
	{
		state = LOCUS;
	}
	else if (state == LOCUS)
	{
		state = KEYWORD;
	}
	else if (state == ORIGINSUB)
	{
		state = ORIGIN;
	}
	else if (state == ORIGIN)
	{
		state = KEYWORD;
	}
	else if (state == SEQUENCESUB)
	{
		state = SEQUENCE;
	}
	else if (state == CONTIGSUB)
	{
		state = CONTIG;
	}
	else if (state == SEQUENCE || state == CONTIG)
	{
		gb << "//\n";
		nwritten = 0;
		state = END;
	}
	return true;
}

bool JSONHandler::StartObject()
{
	switch (state)
	{
	case FEATURE_HEADER:
	{
		state = FEATURE;
		break;
	}
	case FEATURE:
	{
		state = QUALIFIER;
		break;
	}
	default:
		break;
	}
	return true;
}

bool JSONHandler::EndObject(rapidjson::SizeType elementCount)
{
	if (state == QUALIFIER || state == QUALIFIER_LOCATION)
	{
		state = FEATURE;
	}
	return true;
}

/**
 * Consume a string value.
 * @param[in] value The value string.
 */
void JSONHandler::handleStringValue(const std::string *value)
{
	// Set value indentation
	int valueIndentation = 12;

	switch (state)
	{
	case QUALIFIER:
	{
		valueIndentation = 21;
		break;
	}
	case QUALIFIER_LOCATION:
	{
		valueIndentation = 21;
		break;
	}
	case FEATURE:
	{
		valueIndentation = 21;
		break;
	}
	case KEYWORD:
	{
		valueIndentation = 12;
		break;
	}
	case SUBKEYWORD:
	{
		valueIndentation = 12;
		break;
	}
	case SUBSUBKEYWORD:
	{
		valueIndentation = 12;
		break;
	}
	default:
		break;
	}

	// Write equal sign for qualifiers
	if (state == QUALIFIER)
	{
		gb << "=";
		nwritten += 1;
	}

	// Split the value into padded lines
	std::string block;
	blockPad(value, &block, valueIndentation, 79, nwritten - valueIndentation);

	// Write the value
	gb << block;

	// Reset the column counter
	nwritten = 0;
}

/**
 * Consume the nucleotide sequence.
 * @param[in] value The string value.
 */
void JSONHandler::handleSequence(const std::string *value)
{
	std::string line;

	int nlines = std::ceil(((double)value->length()) / 60); // Number of lines
	int nsections;											// Number of 10 b sections for a given line
	std::string pos;										// Coordinate at start of line

	for (int i = 0; i < nlines; i++)
	{ // Loop through lines
		// Place leading number
		pos = std::to_string(i * 60 + 1);
		gb << makeSpaces(9 - pos.length()) << pos << " ";

		line = value->substr(i * 60, 60);
		nsections = std::ceil(((double)line.length() / 10));

		for (int j = 0; j < nsections; j++)
		{ // Build 10 b sections
			gb << line.substr(j * 10, 10);

			if (j < nsections - 1)
			{
				gb << " ";
			}
		}

		gb << "\n";
	}

	// Reset column counter
	nwritten = 0;
}

bool JSONHandler::String(const char *str, rapidjson::SizeType length, bool copy)
{
	std::string value(str);

	switch (state)
	{
	case LOCUS:
	{
		gb << str << "\n";
		break;
	}
	case ORIGIN:
	{
		gb << str << "\n";
		break;
	}
	case SEQUENCE:
	{
		handleSequence(&value);
		break;
	}
	default:
	{
		handleStringValue(&value);
		break;
	}
	}
	return true;
}

bool JSONHandler::Key(const char *str, rapidjson::SizeType length, bool copy)
{
	std::string key(str);
	updateState(key);

	int keyIndentation = 0;	// Whitespace before key
	int valueIndentation = 12; // Number of chars before value

	if (state == SEQUENCE)
	{
		return true; // Don't print a key
	}
	else if (state == FEATURE_HEADER)
	{
		gb << "FEATURES" << makeSpaces(21 - 8) << "Location/Qualifiers\n";
		nwritten = 0;
		return true;
	}
	else if (state == QUALIFIER)
	{
		// Special formatting for qualifier key
		gb << makeSpaces(21) << "/" << key;
		nwritten = 22 + key.length();
		return true;
	}
	else if (state == QUALIFIER_LOCATION)
	{
		return true; // Don't print a key
	}
	else
	{ // Set indentation
		switch (state)
		{
		case KEYWORD:
		{
			keyIndentation = 0;
			valueIndentation = 12;
			break;
		}
		case SUBKEYWORD:
		{
			keyIndentation = 2;
			valueIndentation = 12;
			break;
		}
		case SUBSUBKEYWORD:
		{
			keyIndentation = 3;
			valueIndentation = 12;
			break;
		}
		case FEATURE:
		{
			keyIndentation = 5;
			valueIndentation = 21;
			break;
		}
		default:
		{
			break;
		}
		}
	}

	// Print key plus left/right padding
	gb << makeSpaces(keyIndentation);
	gb << key;
	gb << makeSpaces(valueIndentation - keyIndentation - key.length());

	// Set column counter
	nwritten = valueIndentation;

	return true;
}

bool JSONHandler::Null()
{
	gb << "\n";
	return true;
}

/**
 * JSON to GenBank converter.
 * @param[in] json The JSON string.
 * @param[out] gb The GenBank string.
 * @param[out] err Error object.
 */
void json2gb(const std::string *json, std::string *gb, gberror *err)
{
	JSONHandler handler;
	rapidjson::Reader reader;

	rapidjson::StringStream sstream(json->c_str());
	reader.Parse(sstream, handler);

	if (reader.HasParseError())
	{
		err->flag = true;
		err->msg = "Unable to parse JSON";
		err->source = "json2gb";
	}
	else
	{
		*gb = handler.gb.str();
	}
}
