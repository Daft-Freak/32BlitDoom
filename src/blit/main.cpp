#include "main.h"

#include "../chocdoom/doomstat.h"
#include "../chocdoom/s_sound.h"

extern void D_DoomMain();
extern void D_Display();


void init()
{
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