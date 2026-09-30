# Northstar Master - VST3 для Windows (x64)

## Вариант A. Собрать у себя (10-20 минут при первом запуске)

Нужно один раз установить:

1. **Visual Studio 2022** (Community, бесплатно) или *Build Tools for Visual Studio* -
   при установке отметить **Desktop development with C++**.
2. **CMake** 3.22+: `winget install Kitware.CMake` (галочка «Add to PATH»).
3. **Git**: `winget install Git.Git` (CMake скачает JUCE автоматически, нужен интернет).

Затем:

- **Правой кнопкой по `build-windows.bat` -> «Запуск от имени администратора».**
  Скрипт соберёт плагин и сам скопирует его в `C:\Program Files\Common Files\VST3`.
- Откройте DAW и выполните **Rescan / Scan for plug-ins**. Плагин: **Northstar Master**
  (категория Fx / Mastering). Ставьте его на мастер-шину.

Без прав администратора плагин ставится в `%LOCALAPPDATA%\Programs\Common\VST3` -
этот путь сканируют не все DAW; тогда скопируйте папку
`build-windows\NorthstarMastering_artefacts\Release\VST3\Northstar Master.vst3`
в `C:\Program Files\Common Files\VST3` вручную (папку целиком, не только .dll).

Полезные ключи (в PowerShell: `.\build-windows.ps1 -Ключ`):
`-Clean` (пересобрать с нуля), `-BuildStandalone` (ещё и .exe для проверки без DAW),
`-JuceDir <папка>` (использовать скачанный JUCE вместо git).

## Вариант B. Собрать в облаке, ничего не устанавливая

Залейте эту папку в репозиторий на GitHub -> вкладка **Actions** -> **Build Windows VST3**
-> **Run workflow**. Через несколько минут внизу страницы запуска появится артефакт
`Northstar-Master-VST3-Windows-x64` - распакуйте и положите папку
`Northstar Master.vst3` в `C:\Program Files\Common Files\VST3`.

## Если плагин не появился в DAW

- Нужна **64-битная** DAW (32-битные не поддерживаются).
- Убедитесь, что скопирована **вся папка** `Northstar Master.vst3` (внутри `Contents\x86_64-win\`).
- В настройках DAW включите путь `C:\Program Files\Common Files\VST3` и запустите полное
  пересканирование (в некоторых DAW - с очисткой чёрного списка).

## Что реализовано

Плагин теперь разделён на пять рабочих вкладок:

- **Equalization** — 15 узлов EQ с перетаскиванием по частоте и dB, ручным режимом,
  автоматическим профилем после learn, Q и dynamic EQ для каждого узла. Кнопка
  **Linear Phase** переключает EQ на отдельный FIR-путь; в обычном режиме используется
  минимально-фазовый IIR. Под узлами отображается сглаженный live spectrum входного
  сигнала, а EQ-кривая строится как сумма плавных bell-фильтров, а не прямые между узлами.
- **Loudness** — 10-секундный анализ входного сигнала, входной/выходной LUFS,
  peak и target LUFS. Ручной Volume — отдельный чистый gain без waveshaper.
- **Saturation** — общий Mix 0–100% и пресеты Warm, Cold / Airy, Distortion, Tube.
- **Stereo Imager** — четыре полосы с тремя подвижными crossover-точками, независимой
  шириной 0–200% и выбором пресета на каждой полосе. Imager не запускает waveshaper
  на полосах: пресеты меняют только side-width, чтобы нейтральный режим не создавал
  лишних фазовых артефактов.
- **OPTO Compressor** — ручные Attack, Release, Ratio, Input, Output и Threshold,
  с live gain-reduction метром.

Параметры зарегистрированы в `AudioProcessorValueTreeState`, поэтому DAW может
автоматизировать их и сохранять состояние проекта.

## Как пользоваться

1. Вставьте плагин на мастер-шину.
2. Запустите воспроизведение репрезентативного фрагмента и нажмите **ANALYZE 10 SEC**.
   Анализ завершается после 10 секунд реального воспроизведения (остановка DAW прерывает
   накопление).
3. В **Equalization** выберите AUTO для профиля анализа или MANUAL для ручных узлов.
4. Проверьте LUFS/peak и при необходимости задайте target/volume, затем настройте
   сатурацию, stereo bands и OPTO на отдельных вкладках.

LUFS в этой версии — realtime K-weighted оценка, пригодная для настройки во время
работы, но финальный экспорт следует перепроверить отдельным loudness meter.

Лицензия JUCE: у неё есть открытая и коммерческая версии - перед распространением
собранного продукта проверьте, какая вам подходит.
