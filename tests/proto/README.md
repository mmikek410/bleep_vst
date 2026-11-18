# Прототипы тёмного бипа

`Prototypes.h` содержит три подхода (A. Dark FM, B. Resonant, C. Hollow Analog). `RenderPrototypes.cpp` рендерит 18 WAV на одном 16-шаговом паттерне: сухие, с сатурацией и дилэем, с ударной петлёй, A/B-сравнение и шесть рецептов.

    cmake -S . -B build-tests -DBLEEP_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
    cmake --build build-tests --target bleep_render_prototypes
    ./build-tests/bleep_render_prototypes out/

Это экспериментальный код, в плагин он пока не входит. Результаты и измерения описаны в `docs/sound-prototypes.md`.
