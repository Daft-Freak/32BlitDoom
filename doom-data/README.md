Place `doom1.wad` here. Other WAD files may work but are untested.

Edit `assets.yml` to pack additional files and the call to `blit::File::add_buffer_file` in `src/blit/main.cpp` to change the filename DOOM thinks it's loading.