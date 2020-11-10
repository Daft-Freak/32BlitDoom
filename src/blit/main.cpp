#include <csetjmp>

#include "main.h"

#ifdef ASSET_WAD
#include "assets.hpp"
#endif

#include "../chocdoom/doomstat.h"
#include "../chocdoom/s_sound.h"

extern void D_DoomMain();
extern void D_Display();

#ifdef TARGET_32BLIT_HW
extern "C" void *_sbrk(ptrdiff_t incr)
{
    extern char end, __ltdc_start;
    static char *heap_end;

    if(!heap_end)
        heap_end = &end;

    // ltdc is at the end of the heap
    if(heap_end + incr > &__ltdc_start + 320 * 240 * 2)
    {
        puts("sbrk oom");
        return (void *)-1;
    }

    char *ret = heap_end;
    heap_end += incr;

    return (void *)ret;
}
#endif

jmp_buf jump_buffer;
std::string fatal_error;
void blit_die(const char *msg)
{
    fatal_error = msg;
    fatal_error = blit::screen.wrap_text(fatal_error, blit::screen.bounds.w - 20, blit::minimal_font);

    for(int i = 0; i < CHANNEL_COUNT; i++)
        blit::channels[i].off();

    longjmp(jump_buffer, 1);
}

void add_appended_files()
{
#ifdef TARGET_32BLIT_HW
    extern char _flash_end;

    if(memcmp(&_flash_end, "APPFILES", 8) != 0)
        return;

    uint32_t num_files = *reinterpret_cast<uint32_t *>(&_flash_end + 8);

    const int header_size = 12, file_header_size = 8;

    auto dataPtr = &_flash_end + header_size + file_header_size * num_files;

    for(auto i = 0u; i < num_files; i++)
    {
        auto filename_length = *reinterpret_cast<uint16_t *>(&_flash_end + header_size + i * file_header_size);
        auto file_length = *reinterpret_cast<uint32_t *>(&_flash_end + header_size + i * file_header_size + 4);

        blit::File::add_buffer_file(std::string(dataPtr, filename_length), reinterpret_cast<uint8_t *>(dataPtr + filename_length), file_length);

        dataPtr += filename_length + file_length;
    }
#endif
}

void init()
{
    blit::set_screen_mode(blit::ScreenMode::hires_palette);
    blit::screen.pen = blit::Pen(1);
    blit::screen.clear();

#ifdef ASSET_WAD
    blit::File::add_buffer_file("doom-data/doom1.wad", doom1_wad, doom1_wad_length);
#endif

    add_appended_files();

#ifndef TARGET_32BLIT_HW
    // for testing, load all the WADs into memory
    for(auto &file : blit::list_files("doom-data"))
    {
        if(file.flags & blit::FileFlags::directory)
            continue;

        if(file.name.length() <= 4 || file.name.substr(file.name.length() - 4) != ".wad")
            continue;

        blit::File f;
        if(!f.open("doom-data/" + file.name))
            continue;

        printf("%s\n", file.name.c_str());

        auto length = f.get_length();
        auto data = new uint8_t[length];

        f.read(0, length, (char *)data);

        blit::File::add_buffer_file("doom-data/" + file.name, data, length);
    }
#endif

    if(setjmp(jump_buffer))
        return;

    D_DoomMain();
}

void update(uint32_t time)
{
    if(!fatal_error.empty() || setjmp(jump_buffer))
        return;

    TryRunTics();
	S_UpdateSounds(players[consoleplayer].mo);
}

void render(uint32_t time)
{
    // everything is broken!
    if(!fatal_error.empty())
    {
        blit::screen.pen = blit::Pen(8); // 070707

        blit::Size text_bounds = blit::screen.measure_text(fatal_error, blit::minimal_font);

        text_bounds.w += 4;
        text_bounds.h += 4;

        blit::Rect text_rect(
            (blit::screen.bounds.w - text_bounds.w) / 2,
            (blit::screen.bounds.h - text_bounds.h) / 2,
            text_bounds.w, text_bounds.h);

        blit::screen.rectangle(text_rect);

        blit::screen.pen = blit::Pen(176); // FF0000
        blit::screen.text(fatal_error, blit::minimal_font, text_rect, true, blit::TextAlign::center_center);

        return;
    }

    if(setjmp(jump_buffer))
        return;

    D_Display();
}