<div align="center">

<!-- 🖼️ App icon goes here -->
<img src="app.ico" width="96" height="96" alt="LensIt icon">

# LensIt

**A tiny, instant screen magnifier with on-screen annotation for Windows.**

Hold a key, scroll to zoom in on anything, and draw lines, arrows or rectangles right over your screen to point things out — perfect for presentations, tutorials, streams, or just showing someone something on your screen.

[Features](#-features) • [Usage](#-usage) • [Settings](#-settings) • [Installation](#-installation) • [Building from source](#-building-from-source) • [Русская версия](#-lensit-русская-версия)

</div>

---

## ✨ Features

- 🪶 < 1 MB single standalone executable
- ⚡ ~15 MB RAM consumption
- 🚀 0% CPU idle usage
- 📦 Zero dependencies — runs out-of-the-box on clean Windows 10 & 11

- 🔍 **Instant magnifier** — hold your trigger key and scroll the mouse wheel to zoom smoothly in on the cursor
- ✏️ **On-screen drawing** — draw lines, arrows and rectangles directly over your screen while zoomed
- 🎛️ **Fully customizable** — pick your own trigger key, modifier key, colors and stroke widths for every tool
- 🧷 **Lives in the tray** — no taskbar clutter, right-click the tray icon for settings or to exit
- 💾 **Persistent config** — all your settings are saved to a local `config.ini` next to the executable

## 🕹 Usage

| Action | Input |
|---|---|
| Zoom in / out | Hold **Trigger Key** (default `Alt`) + Mouse Wheel |
| Draw a line | Hold **Trigger Key** + Left Mouse Button |
| Draw an arrow | Hold **Trigger Key** + Right Mouse Button |
| Draw a rectangle | Hold **Trigger Key** + **Rectangle Key** (default `Shift`) + Left Mouse Button |
| Reset zoom & clear drawings | `Esc` |
| Open settings / Exit | Right-click the tray icon |

Releasing the trigger key clears all drawings on screen. Optionally, you can also make it reset the zoom level automatically — see [Settings](#-settings).

> [!WARNING]
> **A note for gamers:** LensIt overlays the screen and hooks system-wide magnification, which does **not** work over applications running in true **exclusive fullscreen** mode (e.g. *Geometry Dash*, *Counter-Strike 2*, and many other games).
> To use LensIt with these, switch the game's display mode to **Borderless / Windowed Borderless** (or plain Windowed) instead of Fullscreen — this is usually found in the game's video/graphics settings.

## ⚙️ Settings

Right-click the tray icon → **Settings** to open the settings panel, where you can configure:

- **Trigger Key** — the key you hold to activate zoom & drawing mode
- **Rectangle Modifier Key** — the extra key held together with the trigger key to draw rectangles instead of lines
- **Reset zoom on trigger key release** — automatically snap back to 1x zoom when you let go
- **Line / Arrow / Rectangle** — individual color pickers and width sliders for each drawing tool

All changes are saved automatically to `config.ini`.

## 📥 Installation

1. Go to the [Releases](../../releases) page
2. Download the latest `AkwaLens.exe` (or `LensIt.exe`)
3. Run it — that's it, no installer needed

> LensIt is a portable, single-file executable. To uninstall, simply delete the `.exe` (and the generated `config.ini`, if any).

## 🛠 Building from source

**Requirements:**
- Windows 10/11
- Visual Studio 2022 (or newer) with the **Desktop development with C++** workload

**Steps:**
1. Clone the repository
2. Open `AkwaLens.vcxproj` in Visual Studio
3. Select the `Release` configuration (`x64` recommended)
4. Build (`Ctrl+Shift+B`)

The project links against `magnification.lib`, `gdiplus.lib`, `dwmapi.lib` and other standard Windows libraries — no external dependencies required.

## 🧩 Tech stack

- **C++** with the native Win32 API
- **Windows Magnification API** for the zoom effect
- **GDI+** for rendering the on-screen drawings and settings UI
- **Low-level keyboard & mouse hooks** for global input capture

---
---

<div align="center">

# 🔎 LensIt — русская версия

**Компактная лупа для экрана с рисованием поверх экрана для Windows.**

Зажми клавишу, крути колесо мыши, чтобы приблизить нужную область, и рисуй линии, стрелки или прямоугольники прямо поверх экрана — удобно для презентаций, обучающих видео, стримов или просто чтобы показать что-то собеседнику.

[Возможности](#-возможности) • [Использование](#-использование) • [Настройки](#-настройки) • [Установка](#-установка) • [Сборка из исходников](#-сборка-из-исходников)

</div>

---

## ✨ Возможности

- 🪶 Один исполняемый файл размером < 1 МБ
- ⚡ ~15 МБ потребления ОЗУ
- 🚀 0% загрузки процессора в простое
- 📦 Никаких зависимостей — работает "из коробки" на чистой Windows 10 и 11

- 🔍 **Мгновенная лупа** — зажми клавишу-триггер и крути колесо мыши, чтобы плавно приблизить область вокруг курсора
- ✏️ **Рисование поверх экрана** — рисуй линии, стрелки и прямоугольники прямо на экране во время приближения
- 🎛️ **Полная настройка** — выбери свою клавишу-триггер, модификатор, цвета и толщину линий для каждого инструмента
- 🧷 **Работает из трея** — не занимает панель задач, все настройки и выход — через иконку в трее
- 💾 **Сохранение настроек** — все параметры сохраняются в локальный `config.ini` рядом с исполняемым файлом

## 🕹 Использование

| Действие | Управление |
|---|---|
| Приблизить / отдалить | Зажать **клавишу-триггер** (по умолчанию `Alt`) + колесо мыши |
| Нарисовать линию | Зажать **клавишу-триггер** + ЛКМ |
| Нарисовать стрелку | Зажать **клавишу-триггер** + ПКМ |
| Нарисовать прямоугольник | Зажать **клавишу-триггер** + **клавишу для прямоугольника** (по умолчанию `Shift`) + ЛКМ |
| Сбросить приближение и рисунки | `Esc` |
| Открыть настройки / Выйти | ПКМ по иконке в трее |

При отпускании клавиши-триггера все рисунки на экране очищаются. При желании можно включить автоматический сброс приближения — см. раздел [Настройки](#-настройки).

> [!WARNING]
> **Примечание для геймеров:** LensIt накладывается поверх экрана и использует системное приближение, которое **не работает** поверх приложений, запущенных в настоящем **полноэкранном (exclusive fullscreen) режиме** (например, *Geometry Dash*, *Counter-Strike 2* и многие другие игры).
> Чтобы LensIt работал с такими играми, переключи режим экрана игры на **оконный без рамки (Borderless / Windowed Borderless)** или обычный оконный режим вместо полноэкранного — это обычно находится в настройках видео/графики игры.

## ⚙️ Настройки

ПКМ по иконке в трее → **Settings**, чтобы открыть панель настроек, где можно изменить:

- **Trigger Key** — клавиша, которую нужно зажимать для активации приближения и рисования
- **Rectangle Modifier Key** — дополнительная клавиша, которую нужно зажимать вместе с триггером для рисования прямоугольников вместо линий
- **Reset zoom on trigger key release** — автоматический сброс приближения до 1x при отпускании клавиши
- **Line / Arrow / Rectangle** — отдельные выбор цвета и толщины для каждого инструмента рисования

Все изменения сохраняются автоматически в `config.ini`.

## 📥 Установка

1. Перейди на страницу [Releases](../../releases)
2. Скачай последнюю версию `AkwaLens.exe` (или `LensIt.exe`)
3. Запусти — установка не требуется

> LensIt — портативный исполняемый файл. Чтобы удалить программу, просто удали `.exe` (и файл `config.ini`, если он создался).

## 🛠 Сборка из исходников

**Требования:**
- Windows 10/11
- Visual Studio 2022 (или новее) с компонентом **Разработка классических приложений на C++**

**Шаги:**
1. Склонируй репозиторий
2. Открой `AkwaLens.vcxproj` в Visual Studio
3. Выбери конфигурацию `Release` (рекомендуется `x64`)
4. Собери проект (`Ctrl+Shift+B`)

Проект использует `magnification.lib`, `gdiplus.lib`, `dwmapi.lib` и другие стандартные библиотеки Windows — внешние зависимости не требуются.

## 🧩 Технологии

- **C++** с нативным Win32 API
- **Windows Magnification API** для эффекта приближения
- **GDI+** для отрисовки рисунков поверх экрана и интерфейса настроек
- **Низкоуровневые хуки клавиатуры и мыши** для глобального перехвата ввода