#pragma once
#include <cstdint>
#include <cstdio> // make sure stdio isn't included after this

#include "engine/file.hpp"

// wrap stdio funcs around blit:: funcs

struct wrap_FILE
{
    blit::File file;
    uint32_t offset;

    uint8_t getc_buffer[512];
    int getc_buf_len, getc_buf_off;
};

inline wrap_FILE *wrap_fopen(const char *filename, const char *mode)
{
    auto ret = new wrap_FILE;

    ret->file.open(filename);
    ret->offset = 0;

    ret->getc_buf_len = ret->getc_buf_off = 0;

    if(!ret->file.is_open())
    {
        delete ret;
        return nullptr;
    }
    
    return ret;
}

inline int wrap_fclose(wrap_FILE *file)
{
    file->file.close();
    delete file;
    return 0;
}

inline size_t wrap_fread(void *buffer, size_t size, size_t count, wrap_FILE *file)
{
    // invalidate getc buffer
    if(file->getc_buf_len)
    {
        file->offset += file->getc_buf_off;
        file->getc_buf_off = file->getc_buf_len = 0;
    }

    auto ret = file->file.read(file->offset, size * count, (char *)buffer);
    file->offset += ret;

    return ret < 0 ? 0 : ret / size;
}

inline int wrap_fgetc(wrap_FILE *file)
{
    // refill getc buffer
    if(file->getc_buf_off >= file->getc_buf_len)
    {
        file->offset += file->getc_buf_off;
        file->getc_buf_len = file->file.read(file->offset, 512, (char*)file->getc_buffer);
        file->getc_buf_off = 0;
    }

    if(file->getc_buf_len)
        return file->getc_buffer[file->getc_buf_off++];

    return EOF;
}

inline int wrap_fseek(wrap_FILE *file, long offset, int origin)
{
    // invalidate getc buffer
    if(file->getc_buf_len)
    {
        file->offset += file->getc_buf_off;
        file->getc_buf_off = file->getc_buf_len = 0;
    }

    if(origin == SEEK_SET)
        file->offset = offset;
    else if(origin == SEEK_CUR)
        file->offset += offset;
    else
        file->offset = file->file.get_length() - offset;

    return 0;
}

inline long wrap_ftell(wrap_FILE *file)
{
    return file->offset + file->getc_buf_off;
}

#define FILE wrap_FILE
#define fopen wrap_fopen
#define fclose wrap_fclose
#define fread wrap_fread
#define fgetc wrap_fgetc
#define fseek wrap_fseek
#define ftell wrap_ftell
