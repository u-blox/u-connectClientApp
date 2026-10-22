# STM32 flash and RAM usage

Updated by `ucxapp --stm32 --build` (Release builds). RAM includes the static FreeRTOS heap
(`configTOTAL_HEAP_SIZE`, set per board in `app/stm32/CMakeLists.txt`), so free RAM is the headroom left
after that heap. Flash is code, read-only data and the initial values of `.data`.

| Board | Name | Flash used | Flash size | Flash % | RAM used | RAM size | RAM % |
|---|---|---|---|---|---|---|---|
| `h753` | NUCLEO-H753ZI | 491 KB | 2048 KB | 24% | 285 KB | 512 KB | 56% |
| `f767` | NUCLEO-F767ZI | 470 KB | 2048 KB | 23% | 308 KB | 368 KB | 84% |
| `f439` | NUCLEO-F439ZI | 463 KB | 2048 KB | 23% | 102 KB | 192 KB | 53% |
| `f407` | STM32F4DISCOVERY | 461 KB | 1024 KB | 45% | 102 KB | 128 KB | 80% |
| `f401` | NUCLEO-F401RE | 420 KB | 512 KB | 82% | 85 KB | 96 KB | 89% |
| `f446` | NUCLEO-F446RE | 420 KB | 512 KB | 82% | 101 KB | 128 KB | 80% |
| `f103` | NUCLEO-F103RB | 115 KB | 128 KB | 90% | 15 KB | 20 KB | 78% |
| `g071` | NUCLEO-G071RB | 120 KB | 128 KB | 94% | 25 KB | 36 KB | 71% |
