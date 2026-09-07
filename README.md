<div align="center">

<!-- 🖼️ App icon goes here -->
<img src="app.ico" width="96" height="96" alt="LensIt icon">

# LensIt

**A tiny, instant screen magnifier with on-screen annotation for Windows.**

Hold a key, scroll to zoom in on anything, and draw lines, arrows, rectangles, badges or blur out sensitive data right over your screen — perfect for presentations, tutorials, streams, or bug reporting.

[Features](#-features) • [Usage](#-usage) • [Settings](#-settings) • [Installation](#-installation) • [Building from source](#-building-from-source) • [Русская версия](#-lensit-русская-версия)

</div>

---

## ✨ Features

- 🪶 < 1 MB single standalone executable
- ⚡ ~15 MB RAM consumption
- 🚀 0% CPU idle usage
- 📦 Zero dependencies — runs out-of-the-box on clean Windows 10 & 11

- 🔍 **Instant magnifier** — hold your trigger key and scroll the mouse wheel to zoom smoothly in on the cursor
- ✏️ **Rich annotation tools** — draw smooth freehand lines, directional arrows, and rectangles
- 🔢 **Step badges** — drop auto-incrementing numbered markers (1, 2, 3...) to guide attention step by step
- 🖍️ **Highlighter mode** — translucent marker tool to emphasize text and UI elements
- 🔲 **Blackout Blur** — quickly obscure passwords, tokens, API keys, or sensitive UI details
- 📌 **Pin Mode (Persistent drawings)** — keep annotations visible on screen as an interactive click-through overlay
- 🎨 **Quick color palette** — switch ink colors instantly on the fly (`R`/`G`/`B`/`Y`)
- 📋 **One-key screenshot** — copy your annotated view straight to the clipboard
- 🔔 **Subtle Toast HUD** — minimal on-screen notifications for mode toggles, color changes, and actions
- 🎛️ **Fully customizable** — pick your own trigger key, modifier key, colors, and stroke widths for every tool
- 🧷 **Lives in the tray** — no taskbar clutter, right-click the tray icon for settings or to exit
- 💾 **Persistent config** — all your settings are saved to a local `config.ini` next to the executable

## 🕹 Usage

| Action | Input |
|---|---|
| Zoom in / out | Hold **Trigger Key** (default `Alt`) + Mouse Wheel |
| Draw a line | Hold **Trigger Key** + Left Mouse Button |
| Draw an arrow | Hold **Trigger Key** + Right Mouse Button |
| Draw a rectangle | Hold **Trigger Key** + **Rectangle Key** (default `Shift`) + Left Mouse Button |
| Place a step badge (1, 2, 3...) | Hold **Trigger Key** + Middle Mouse Button |
| Toggle Highlighter | Hold **Trigger Key** + `H` |
| Toggle Blackout Blur | Hold **Trigger Key** + `O` |
| Quick Color switch | Hold **Trigger Key** + `R` (Red) / `G` (Green) / `B` (Blue) / `Y` (Yellow) |
| Undo last stroke | Hold **Trigger Key** + `Z` |
| Copy screenshot to clipboard | Hold **Trigger Key** + `C` |
| Pin drawings (keep on screen) | Hold **Trigger Key** + `P` |
| Reset zoom & clear drawings | `Esc` |
| Open settings / Exit | Right-click the tray icon |

> [!TIP]
> By default, releasing the trigger key clears unpinned drawings. You can toggle **"Keep drawings on screen"** or enable **"Reset zoom on release"** in the settings.

> [!WARNING]
> **A note for gamers:** LensIt overlays the screen and hooks system-wide magnification, which does **not** work over applications running in true **exclusive fullscreen** mode (e.g. *Geometry Dash*, *Counter-Strike 2*, and many other games).
> To use LensIt with these, switch the game's display mode to **Borderless / Windowed Borderless** (or plain Windowed) instead of Fullscreen — this is usually found in the game's video/graphics settings.

## ⚙️ Settings

Right-click the tray icon → **Settings** to open the settings panel, where you can configure:

- **Trigger Key** — the key or mouse button (X1/X2/Middle) you hold to activate zoom & drawing mode
- **Reset zoom on trigger release** — automatically snap back to 1x zoom when letting go of the trigger
- **Keep drawings on screen** — make your drawings persist and remain click-through after releasing the trigger
- **Line / Arrow / Rectangle** — individual color pickers and width sliders for every tool
- **Rectangle Modifier Key** — the extra key held together with the trigger key to draw rectangles
- **Step Badge Color** — customizable color for numbered step indicators
- **Shortcuts & Hotkeys Drawer** — an interactive expandable cheat sheet showing all built-in hotkeys

All changes are saved automatically to `config.ini`.

## 📥 Installation

1. Go to the [Releases](../../releases) page
2. Download the latest `AkwaLens.exe` (or `LensIt.exe`)
3. Run it — that's it, no installer needed

> LensIt is a portable, single-file executable. On first launch, a welcome dialog helps you optionally configure automatic Windows startup and create a desktop shortcut. To uninstall, simply delete the `.exe` (and the generated `config.ini`, if any).

## 🛠 Building from source

**Requirements:**
- Windows 10/11
- Visual Studio 2022 (or newer) with the **Desktop development with C++** workload

**Steps:**
1. Clone the repository
2. Open `AkwaLens.vcxproj` in Visual Studio
3. Select the `Release` configuration (`x64` recommended)
4. Build (`Ctrl+Shift+B`)

The project links against `magnification.lib`, `gdiplus.lib`, `dwmapi.lib`, `shlwapi.lib` and other standard Windows libraries — no external dependencies required.

## 🧩 Tech stack

- **C++** with the native Win32 API
- **Windows Magnification API** for high-performance hardware-accelerated zoom
- **GDI+** for rendering smooth anti-aliased annotations, blur baking, and dark-themed UI
- **Low-level keyboard & mouse hooks** for global input capture

---
---

<div align="center">

# 🔎 LensIt — русская версия

**Компактная лупа для экрана с инструментами аннотаций поверх экрана для Windows.**

Зажми клавишу, крути колесо мыши, чтобы приблизить нужную область, рисуй линии, стрелки, прямоугольники, расставляй шаги или размывай конфиденциальные данные прямо на экране — удобно для презентаций, обучающих видео, стримов и баг-репортов.

[Возможности](#-возможности) • [Использование](#-использование) • [Настройки](#-настройки) • [Установка](#-установка) • [Сборка из исходников](#-сборка-из-исходников)

</div>

---

## ✨ Возможности

- 🪶 Один исполняемый файл размером < 1 МБ
- ⚡ ~15 МБ потребления ОЗУ
- 🚀 0% загрузки процессора в простое
- 📦 Никаких зависимостей — работает "из коробки" на чистой Windows 10 и 11

- 🔍 **Мгновенная лупа** — зажми клавишу-триггер и крути колесо мыши, чтобы плавно приблизить область вокруг курсора
- ✏️ **Набор для аннотаций** — рисуй плавные произвольные линии, стрелки и прямоугольники
- 🔢 **Бейджи шагов** — расставляй круглые маркеры с автоматической нумерацией (1, 2, 3...), чтобы вести зрителя по шагам
- 🖍️ **Режим маркера (Highlighter)** — полупрозрачное выделение текста и ключевых элементов
- 🔲 **Размытие и цензура (Blur)** — быстрое скрытие паролей, токенов, ключей и личных данных
- 📌 **Закрепление рисунков (Pin Mode)** — сохранение аннотаций на экране в виде прозрачного для кликов слоя
- 🎨 **Быстрая смена цветов** — мгновенное переключение цвета чернил на лету (`R`/`G`/`B`/`Y`)
- 📋 **Скриншот в буфер обмена** — копирование экрана вместе со всеми рисунками в один клик
- 🔔 **Всплывающие Toast-уведомления** — аккуратный анимированный HUD с информацией о режимах и действиях
- 🎛️ **Полная настройка** — выбери свою клавишу-триггер, модификатор, цвета и толщину для каждого инструмента
- 🧷 **Работает из трея** — не занимает панель задач, все настройки и выход — через иконку в трее
- 💾 **Сохранение настроек** — все параметры сохраняются в локальный `config.ini` рядом с исполняемым файлом

## 🕹 Использование

| Действие | Управление |
|---|---|
| Приблизить / отдалить | Зажать **клавишу-триггер** (по умолчанию `Alt`) + колесо мыши |
| Нарисовать линию | Зажать **клавишу-триггер** + ЛКМ |
| Нарисовать стрелку | Зажать **клавишу-триггер** + ПКМ |
| Нарисовать прямоугольник | Зажать **клавишу-триггер** + **клавишу для прямоугольника** (по умолчанию `Shift`) + ЛКМ |
| Поставить бейдж шага (1, 2, 3...) | Зажать **клавишу-триггер** + СКМ (клик колесом мыши) |
| Включить / выключить маркер | Зажать **клавишу-триггер** + `H` |
| Включить / выключить размытие (блюр) | Зажать **клавишу-триггер** + `O` |
| Быстро сменить цвет | Зажать **клавишу-триггер** + `R` (красный) / `G` (зеленый) / `B` (синий) / `Y` (желтый) |
| Отменить последнее действие | Зажать **клавишу-триггер** + `Z` |
| Скопировать скриншот с рисунками | Зажать **клавишу-триггер** + `C` |
| Закрепить рисунки на экране | Зажать **клавишу-триггер** + `P` |
| Сбросить приближение и рисунки | `Esc` |
| Открыть настройки / Выйти | ПКМ по иконке в трее |

> [!TIP]
> По умолчанию при отпускании клавиши-триггера рисунки очищаются. Вы можете закрепить их клавишей `P` (или через настройку **"Keep drawings on screen"**), а также включить автоматический сброс приближения при отпускании клавиши.

> [!WARNING]
> **Примечание для геймеров:** LensIt накладывается поверх экрана и использует системное приближение, которое **не работает** поверх приложений, запущенных в настоящем **полноэкранном (exclusive fullscreen) режиме** (например, *Geometry Dash*, *Counter-Strike 2* и многие другие игры).
> Чтобы LensIt работал с такими играми, переключи режим экрана игры на **оконный без рамки (Borderless / Windowed Borderless)** или обычный оконный режим вместо полноэкранного — это обычно находится в настройках видео/графики игры.

## ⚙️ Настройки

ПКМ по иконке в трее → **Settings**, чтобы открыть панель настроек, где можно изменить:

- **Trigger Key** — клавиша или кнопка мыши (боковые кнопки X1/X2, колесико), которую нужно зажимать для активации
- **Reset zoom on trigger release** — автоматический сброс приближения до 1x при отпускании триггера
- **Keep drawings on screen** — сохранять рисунки на экране после отпускания клавиши (сквозной оверлей)
- **Line / Arrow / Rectangle** — раздельный выбор цвета и ползунки толщины для каждого инструмента
- **Rectangle Modifier Key** — клавиша для рисования прямоугольников
- **Step Badge Color** — выбор цвета для нумерованных маркеров
- **Shortcuts & Hotkeys** — встроенная раскрывающаяся карточка-шпаргалка со всеми горячими клавишами

Все изменения сохраняются автоматически в `config.ini`.

## 📥 Установка

1. Перейди на страницу [Releases](../../releases)
2. Скачай последнюю версию `AkwaLens.exe` (или `LensIt.exe`)
3. Запусти — установка не требуется

> LensIt — портативный исполняемый файл. При первом запуске появится стартовое окно с возможностью включить автозапуск с Windows и создать ярлык на рабочем столе. Чтобы удалить программу, просто удали `.exe` (и файл `config.ini`, если он создался).

## 🛠 Сборка из исходников

**Требования:**
- Windows 10/11
- Visual Studio 2022 (или новее) с компонентом **Разработка классических приложений на C++**

**Шаги:**
1. Склонируй репозиторий
2. Открой `AkwaLens.vcxproj` в Visual Studio
3. Выбери конфигурацию `Release` (рекомендуется `x64`)
4. Собери проект (`Ctrl+Shift+B`)

Проект использует `magnification.lib`, `gdiplus.lib`, `dwmapi.lib`, `shlwapi.lib` и другие стандартные библиотеки Windows — внешние зависимости не требуются.

## 🧩 Технологии

- **C++** с нативным Win32 API
- **Windows Magnification API** для плавного аппаратного масштабирования экрана
- **GDI+** для сглаженной отрисовки аннотаций, генерации размытия и интерфейса настроек
- **Низкоуровневые хуки клавиатуры и мыши** для глобального перехвата ввода