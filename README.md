# 32BlitDoom

This is a port of Chocolate Doom for the 32Blit console. It is based on [STM32Doom](https://github.com/floppes/stm32doom) (mainly the Doom sources).

# Data

Copy `doom1.wad` to `doom-data` ([more info](doom-data/README.md)).

# Compilation

```
mkdir build
cd build
cmake -D32BLIT_PATH=path/to/32blit-beta ..
make
```