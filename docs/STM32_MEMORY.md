# STM32 flash and RAM usage

Updated by `ucxapp --stm32 --build` (Release builds). RAM includes the static FreeRTOS heap
(`configTOTAL_HEAP_SIZE`, set per board in `app/stm32/CMakeLists.txt`), so free RAM is the headroom left
after that heap. Flash is code, read-only data and the initial values of `.data`.

| Board | Name | Flash used | Flash size | Flash % | RAM used | RAM size | RAM % |
|---|---|---|---|---|---|---|---|
| `h753` | NUCLEO-H753ZI | 416 KB | 2048 KB | 20% | 284 KB | 512 KB | 56% |
| `f767` | NUCLEO-F767ZI | 397 KB | 2048 KB | 19% | 299 KB | 368 KB | 81% |
| `f439` | NUCLEO-F439ZI | 400 KB | 2048 KB | 20% | 102 KB | 192 KB | 53% |
| `f407` | STM32F4DISCOVERY | 397 KB | 1024 KB | 39% | 102 KB | 128 KB | 80% |
| `f401` | NUCLEO-F401RE | 378 KB | 512 KB | 74% | 85 KB | 96 KB | 89% |
| `f446` | NUCLEO-F446RE | 378 KB | 512 KB | 74% | 101 KB | 128 KB | 79% |
| `f103` | NUCLEO-F103RB | 108 KB | 128 KB | 85% | 15 KB | 20 KB | 78% |
| `g071` | NUCLEO-G071RB | 117 KB | 128 KB | 92% | 25 KB | 36 KB | 71% |
