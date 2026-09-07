<div align="center">

<!-- 🖼️ App icon goes here -->
<img src="app.ico" width="96" height="96" alt="LensIt icon">

# LensIt

**A tiny screen magnifier, annotation tool and break timer for Windows.**

Hold a key and scroll to zoom in on anything, draw lines, arrows and rectangles, drop numbered step badges, blur out sensitive data, or pop up a countdown timer over your screen. Useful for presentations, tutorials, streams, lectures, bug reports, whatever needs pointing at.

[Features](#-features) • [Usage](#-usage) • [Break Timer](#-break-countdown-timer) • [Settings](#-settings) • [Installation](#-installation) • [Building from source](#-building-from-source) • [Русская версия](#-lensit-русская-версия)

</div>

---

## ✨ Features

- Single executable, under 1 MB
- Around 15 MB of RAM
- 0% CPU when idle
- No dependencies, runs on a clean Windows 10 or 11 install

- **Instant magnifier** — hold the trigger key and scroll to zoom smoothly on the cursor
- **Annotation tools** — freehand lines, arrows, rectangles
- **Step badges** — numbered markers (1, 2, 3...) that auto-increment, handy for walking someone through steps
- **Highlighter** — translucent marker for text and UI elements
- **Blackout blur** — cover up passwords, tokens, API keys, anything you don't want on screen
- **Break countdown timer** — translucent fullscreen timer with mouse-wheel scrubbing and manual time entry
- **Pin mode** — keep drawings on screen as a click-through overlay instead of clearing them
- **Quick color palette** — swap ink color on the fly with `R`/`G`/`B`/`Y`
- **One-key screenshot** — copies the annotated view straight to your clipboard
- **Toast HUD** — small on-screen notices when you switch modes or colors
- Fully customizable trigger key, modifier key, colors and stroke widths
- Lives in the tray, no taskbar clutter
- Settings are saved automatically to a local `config.ini` next to the exe

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
| **Toggle Break Timer** | Hold **Trigger Key** + `T` |
| Quick Color switch | Hold **Trigger Key** + `R` (Red) / `G` (Green) / `B` (Blue) / `Y` (Yellow) |
| Undo last stroke | Hold **Trigger Key** + `Z` |
| Copy screenshot to clipboard | Hold **Trigger Key** + `C` |
| Pin drawings (keep on screen) | Hold **Trigger Key** + `P` |
| Reset zoom & clear drawings | `Esc` |
| Open settings / Exit | Right-click the tray icon |

> [!TIP]
> By default, releasing the trigger key clears unpinned drawings. You can toggle "Keep drawings on screen" or turn on "Reset zoom on release" in settings if you want different behavior.

> [!WARNING]
> **Note for gamers:** LensIt overlays the screen and hooks into system-wide magnification, so it won't work over apps running in true exclusive fullscreen (Geometry Dash, Counter-Strike 2, and a lot of other games behave this way).
> If you want to use LensIt with a game like that, switch its display mode to Borderless / Windowed Borderless, or plain Windowed, in the game's video settings.

## ⏳ Break Countdown Timer

Need a 5-minute break, or want to give people time for an exercise during a webinar, lecture or stream? Press **`[Trigger] + T`** and you get a translucent backdrop with a countdown clock and progress bar.

- **Adjust by minutes:** scroll the wheel (or `↑` / `↓`)
- **Adjust by seconds:** hold `Shift` while scrolling (or `Shift + ↑` / `Shift + ↓`) for ±5 second steps
- **Type an exact time:** click the clock, type something like `3:50`, `10` or `:45`, hit `Enter`
- **Pause / resume:** `Space`
- **Dismiss:** `Esc` (or `[Trigger] + T` again)
- When it hits zero you get a quiet chime and a toast notification

## ⚙️ Settings

Right-click the tray icon and open **Settings** to configure:

- **Trigger Key** — which key or mouse button (including X1/X2/Middle) activates zoom and drawing
- **Reset zoom on trigger release** — snap back to 1x when you let go
- **Keep drawings on screen** — make drawings persist and stay click-through after release
- **Line / Arrow / Rectangle** — color and width for each tool separately
- **Rectangle Modifier Key** — the extra key held with the trigger to draw rectangles
- **Step Badge Color**
- A shortcuts drawer with the full hotkey list, in case you forget one

Everything saves automatically to `config.ini`.

## 📥 Installation

1. Go to the [Releases](../../releases) page
2. Grab the latest `AkwaLens.exe` (also shows up as `LensIt.exe`)
3. Run it. No installer, no setup wizard.

On first launch you'll get a small welcome dialog offering to enable startup with Windows and create a desktop shortcut, both optional. To remove it, just delete the `.exe` and, if one was created, `config.ini`.

## 🛠 Building from source

**Requirements:**
- Windows 10/11
- Visual Studio 2022 or newer, with the "Desktop development with C++" workload

It links against `magnification.lib`, `gdiplus.lib`, `dwmapi.lib`, `shlwapi.lib` and other standard Windows libraries. Nothing external to install.

## 🧩 Tech stack

- C++ with the native Win32 API
- Windows Magnification API for hardware-accelerated zoom
- GDI+ for the annotations, blur baking, timer HUD and dark UI
- Low-level keyboard/mouse hooks for global input capture

---
---

<div align="center">

# 🔎 LensIt — русская версия

**Компактная лупа для экрана с инструментами аннотаций и таймером перерыва для Windows.**

Зажимаешь клавишу, крутишь колесо мыши — приближаешь нужную область. Можно рисовать линии, стрелки, прямоугольники, ставить шаги, размывать конфиденциальные данные или запускать таймер перерыва прямо поверх экрана. Пригодится для презентаций, обучающих видео, лекций, стримов, баг-репортов.

[Возможности](#-возможности) • [Использование](#-использование) • [Таймер перерыва](#-таймер-перерыва-break-timer) • [Настройки](#-настройки) • [Установка](#-установка) • [Сборка из исходников](#-сборка-из-исходников)

</div>

---

## ✨ Возможности

- Один exe-файл, меньше 1 МБ
- Около 15 МБ ОЗУ
- 0% процессора в простое
- Никаких зависимостей, работает на чистой Windows 10 и 11

- **Мгновенная лупа** — зажми клавишу-триггер и крути колесо, чтобы плавно приблизить область вокруг курсора
- **Аннотации** — линии, стрелки, прямоугольники от руки
- **Бейджи шагов** — нумерованные маркеры (1, 2, 3...) с автоинкрементом, удобно, когда нужно провести кого-то по шагам
- **Маркер (Highlighter)** — полупрозрачное выделение текста и элементов интерфейса
- **Размытие (Blur)** — скрыть пароли, токены, ключи и всё, что не должно светиться на экране
- **Таймер перерыва** — полупрозрачный полноэкранный таймер с настройкой колесом мыши и вводом времени вручную
- **Pin Mode** — рисунки остаются на экране как сквозной для кликов слой, вместо того чтобы сразу пропадать
- **Быстрая смена цвета** — `R`/`G`/`B`/`Y` прямо на лету
- **Скриншот в один клик** — аннотированный вид сразу в буфер обмена
- **Toast-уведомления** — небольшие подсказки при переключении режимов и цветов
- Полностью настраиваемые клавиша-триггер, модификатор, цвета и толщина линий
- Живёт в трее, не занимает панель задач
- Настройки сохраняются автоматически в `config.ini` рядом с exe

## 🕹 Использование

| Действие | Управление |
|---|---|
| Приблизить / отдалить | Зажать **клавишу-триггер** (по умолчанию `Alt`) + колесо мыши |
| Нарисовать линию | Зажать **клавишу-триггер** + ЛКМ |
| Нарисовать стрелку | Зажать **клавишу-триггер** + ПКМ |
| Нарисовать прямоугольник | Зажать **клавишу-триггер** + **клавишу для прямоугольника** (по умолчанию `Shift`) + ЛКМ |
| Поставить бейдж шага (1, 2, 3...) | Зажать **клавишу-триггер** + СКМ |
| Включить/выключить маркер | Зажать **клавишу-триггер** + `H` |
| Включить/выключить размытие | Зажать **клавишу-триггер** + `O` |
| **Включить/выключить таймер перерыва** | Зажать **клавишу-триггер** + `T` |
| Быстрая смена цвета | Зажать **клавишу-триггер** + `R` (красный) / `G` (зелёный) / `B` (синий) / `Y` (жёлтый) |
| Отменить последнее действие | Зажать **клавишу-триггер** + `Z` |
| Скопировать скриншот с рисунками | Зажать **клавишу-триггер** + `C` |
| Закрепить рисунки на экране | Зажать **клавишу-триггер** + `P` |
| Сбросить приближение и рисунки | `Esc` |
| Открыть настройки / выйти | ПКМ по иконке в трее |

> [!TIP]
> По умолчанию при отпускании клавиши-триггера незакреплённые рисунки исчезают. Это можно поменять через "Keep drawings on screen" или включить "Reset zoom on release" в настройках.

> [!WARNING]
> **Для геймеров:** LensIt накладывается поверх экрана и цепляется к системному масштабированию, поэтому не работает поверх игр в настоящем exclusive fullscreen режиме (Geometry Dash, Counter-Strike 2 и многие другие так себя ведут).
> Чтобы LensIt заработал, переключи игру на Borderless / Windowed Borderless или обычный оконный режим в настройках графики.

## ⏳ Таймер перерыва (Break Timer)

Нужен 5-минутный перерыв или время на задание во время созвона, вебинара или стрима? Нажми **`[Триггер] + T`** — экран затемнится, появится крупный таймер с прогресс-баром.

- **По минутам:** колесо мыши (или `↑` / `↓`)
- **По секундам:** `Shift` + колесо (или `Shift + ↑` / `Shift + ↓`), шаг ±5 секунд
- **Ввести точное время:** кликни по цифрам, введи что-то вроде `3:50`, `10` или `:45`, нажми `Enter`
- **Пауза/продолжить:** `Пробел`
- **Закрыть:** `Esc` (или снова `[Триггер] + T`)
- По окончании — тихий системный сигнал и toast-уведомление

## ⚙️ Настройки

ПКМ по иконке в трее → **Settings**, там можно настроить:

- **Trigger Key** — клавишу или кнопку мыши (включая X1/X2/среднюю), которая активирует режим
- **Reset zoom on trigger release** — сброс приближения при отпускании
- **Keep drawings on screen** — рисунки остаются на экране и сквозными для кликов после отпускания
- **Line / Arrow / Rectangle** — цвет и толщина отдельно для каждого инструмента
- **Rectangle Modifier Key** — дополнительная клавиша для прямоугольников
- **Step Badge Color**
- Шпаргалка по горячим клавишам прямо в настройках, если что-то забудешь

Все изменения сохраняются в `config.ini` автоматически.

## 📥 Установка

1. Открой страницу [Releases](../../releases)
2. Скачай последний `AkwaLens.exe` (он же `LensIt.exe`)
3. Запусти. Установщика нет.

При первом запуске появится окошко с предложением включить автозапуск и создать ярлык на рабочем столе — оба пункта опциональны. Чтобы удалить программу, просто сотри `.exe`, а заодно и `config.ini`, если он успел создаться.

## 🛠 Сборка из исходников

**Требования:**
- Windows 10/11
- Visual Studio 2022 или новее, с компонентом "Разработка классических приложений на C++"

Проект линкуется с `magnification.lib`, `gdiplus.lib`, `dwmapi.lib`, `shlwapi.lib` и другими стандартными библиотеками Windows. Ставить ничего лишнего не нужно.

## 🧩 Технологии

- C++ на нативном Win32 API
- Windows Magnification API для аппаратного масштабирования
- GDI+ для аннотаций, размытия, графики таймера и интерфейса
- Низкоуровневые хуки клавиатуры и мыши для глобального перехвата ввода