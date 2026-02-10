# OVERCLOCKED: Data Dash MAX

An endless runner built in Unreal Engine 5.7 for arcade cabinet hardware. The player is an electric impulse racing through neon tunnels inside a computer system, dodging obstacles and collecting data packets for points.

**Project:** OVERCLOCKED_DDM_Game  
**Engine:** Unreal Engine 5.7

---

## How It Works

The player stays in place while the world scrolls toward them (e.g. Temple Run / Subway Surfers style). Three lanes, jump/slide/lane changes to avoid obstacles. The **OVERCLOCK** system lets players boost speed for higher scores at greater risk.

### Controls

| Action | Arcade | Keyboard | Gamepad |
|--------|--------|----------|---------|
| Move Left/Right | Joystick | Arrow Keys | Left Stick / D-Pad |
| Jump (hold for height) | Joystick Up | Up Arrow | Left Stick Up |
| Slide (hold for duration) | Joystick Down | Down Arrow | Left Stick Down |
| Fast Fall + Slide | Joystick Down (while airborne) | Down Arrow (while airborne) | Left Stick Down (while airborne) |
| OVERCLOCK | Bottom Button | Left Shift | L2 |
| Camera Toggle | Top Button | Ctrl | R2 |
| Confirm | Select | Enter | A |
| Back | Exit | Escape | B |

Keyboard/gamepad only (no mouse)—designed for arcade hardware.

---

## What's Implemented

| System | Notes |
|--------|-------|
| Movement | 3-lane switching, holdable jump/slide, fast fall to slide |
| World Scroll | Speed increases over time, slows briefly on damage |
| Obstacles | Hand-designed patterns plus procedural generation |
| Pickups | Data Packets (+500 pts), 1-Ups, EMP (clears screen) |
| OVERCLOCK | Speed boost + score multiplier while held |
| Scoring | Base rate over time, combo bonuses (e.g. NICE 6x, INSANE 10x) |
| Lives | 5 lives, brief invulnerability, screen shake on damage |
| Leaderboard | Top 10 with initials, medal colors for top 3 |
| Theme System | 6 selectable color themes |
| Music Player | Multiple tracks with skip/shuffle, persists across menus |
| UI | Main menu, HUD, game over, settings, controls, credits—keyboard/gamepad navigable |
| Audio | SFX and music with volume controls |

---

## Project Structure

- **Source/** — C++ gameplay code (character, world scroll, spawners, scoring, lives, overclock, theme system, UI widgets).
- **Content/** — Blueprints, maps, UI, audio, VFX, materials, theme data assets.
- **Config/** — Default engine, game, and input config.

Core systems are in C++ with Blueprint wrappers for editor tuning.

---

## Getting Started

1. Clone this repository.
2. Open **`OVERCLOCKED_DDM_Game.uproject`** in Unreal Editor 5.7.
3. Allow the project to compile (or build the solution in Visual Studio).
4. Play in-editor or package for Windows.

---

## License

Proprietary. See [LICENSE](LICENSE) for terms. For inquiries: mcharafeddinedev@gmail.com