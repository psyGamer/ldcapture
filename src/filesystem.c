#include "filesystem.h"

#include <sys/stat.h>

bool file_exists(const char* path)
{
    struct stat st = { 0 };
    return stat(path, &st) != -1;
}

bool directory_exists(const char* path)
{
    return file_exists(path); // On UNIX a directory is a file
}

void create_directory(const char* path)
{
    mkdir(path, 700);
}