/* 
 * Copyright (c) 2019 Frank Buermann <fburmann@mrc-lmb.cam.ac.uk>
 * 
 * gbjson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#pragma once

#include <string>
#include <sstream>
#include "rapidjson/reader.h"

#define VERSION_MAJOR "@PROJECT_VERSION_MAJOR@"
#define VERSION_MINOR "@PROJECT_VERSION_MINOR@"
#define VERSION_PATCH "@PROJECT_VERSION_PATCH@"

/**
 * Error struct.
 */
struct gberror
{
	bool flag;		 ///< Error?
	std::string msg; ///< Error message.
	std::string source; ///< The function raising the error
	gberror();
};

void fileToString(const std::string *filename, std::string *output, gberror *err);
void gb2json(const std::string *gb, std::string *json, gberror *err);
void json2gb(const std::string *json, std::string *gb, gberror *err);

/*
 * JSON handler state.
 * This is used to determine formatting rules.
 */
enum handlerState
{
    /* Dummy arrays give all top level objects the same structure */
	START,
	LOCUS,
	LOCUSSUB, // Dummy array
	KEYWORD,
	SUBKEYWORD,
	SUBSUBKEYWORD,
	FEATURE_HEADER,
	FEATURE,
	QUALIFIER_LOCATION,
	QUALIFIER,
	ORIGIN,
	ORIGINSUB, // Dummy array
	SEQUENCE,
	SEQUENCESUB, // Dummy array
	CONTIG,
	CONTIGSUB, // Dummy array
	END
};

/**
 * Handler for rapidjson events. This calls the appropriate member functions
 * on JSON elements. 
 */
struct JSONHandler : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, JSONHandler>
{
	handlerState state;   ///< The handler is a finite state machine. Its behavior depends on its state.
	bool skipStateUpdate; ///< Flag for skipping state update
	std::stringstream gb; ///< The GenBank string.
	int nwritten;		  ///< Number of characters that have been written to line.
	JSONHandler();
	void updateState(const std::string key);
	void handleStringValue(const std::string *value);
	void handleSequence(const std::string *value);
	bool Null();
	bool Bool(bool b);
	bool Int(int i);
	bool Uint(unsigned i);
	bool Int64(int64_t i);
	bool Uint64(uint64_t i);
	bool Double(double d);
	bool RawNumber(const char *str, rapidjson::SizeType length, bool copy);
	bool String(const char *str, rapidjson::SizeType length, bool copy);
	bool StartObject();
	bool Key(const char *str, rapidjson::SizeType length, bool copy);
	bool EndObject(rapidjson::SizeType memberCount);
	bool StartArray();
	bool EndArray(rapidjson::SizeType elementCount);
};
