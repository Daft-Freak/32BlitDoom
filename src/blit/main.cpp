#include "main.h"

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
    if(heap_end + incr > &__ltdc_start)
    {
        puts("sbrk oom");
        return (void *)-1;
    }

    char *ret = heap_end;
    heap_end += incr;

    return (void *)ret;
}
#endif

void init()
{
    blit::set_screen_mode(blit::ScreenMode::hires);
    D_DoomMain();
}

void update(uint32_t time)
{
    TryRunTics();
	S_UpdateSounds(players[consoleplayer].mo);
}

void render(uint32_t time)
{
    D_Display();
}