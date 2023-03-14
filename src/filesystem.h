#pragma once

#include "base.h"

#define LDCAPTURE_RECORDINGS_DIRECTORY "ldcapture-recordings"

bool file_exists(const char* path);
bool directory_exists(const char* path);

void create_directory(const char* path);