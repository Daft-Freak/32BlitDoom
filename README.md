# 32BlitDoom

This is a port of Chocolate Doom for the 32Blit console. It is based on [STM32Doom](https://github.com/floppes/stm32doom) (mainly the Doom sources).

# Compilation

```
mkdir build
cd build
cmake -D32BLIT_PATH=path/to/32blit-beta ..
make
```

# Data

Assuming you're in the build dir (`cd build`), run:
```
python3 ../append_wads.py doom.bin doom1.bin path/to/doom1.wad
```
Where `doom1.bin` is the name of the new .bin with the WAD inserted. You can specify other WAD files here, or multiple files.

You can also copy `doom1.wad` to `doom-data` ([more info](doom-data/README.md)) and set `-DEMBED_ASSET_WAD=1`. This is useful for testing
