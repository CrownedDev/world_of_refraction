# Weather

**Status:** [Built · No UI] — the C++ is wired and broadcasting, but the in-combat sky depends on a Blueprint consumer not verifiable from source. Owning code: `UWeatherStateManager`.

## What it is

The battlefield **sky/atmosphere shifts with the fight**: each team's weather is set by its **leader's element**, and the blend tracks the **leader HP %** balance between teams — as one side gains the HP edge, the sky leans to their element. Ambient feedback; the player makes no direct weather choice.

- **Leadership (folded in)** — the leader is the **highest-world-stat** member of a team (`BuildHierarchy` / `GetCurrentLeader`); the weather variant comes from that leader's element (`ResolveWeatherDA`). Internal — no leader label in UI.
- **Blend** — `RecalculateWeather` on HP change → `OnWeatherChanged(Team0DA, Team1DA, BlendValue)`.

## Caveat

The C++ broadcasts correctly, but the visual is driven by a Blueprint consumer (`BP_WeatherController`, an LFS asset) that can't be verified from source. It may be unbound/regressed — `../Architecture/WeatherSystem.md` lists known gaps (G1–G9). **Confirm in PIE** before assuming it's visibly live. Resolver returns null for leaders with no `EquippedWeatherVariant`.

## Entry points

- `UWeatherStateManager` — `InitialiseLeaders`, `RecalculateWeather`, `BuildHierarchy`, `GetCurrentLeader`, `ResolveWeatherDA`, `OnWeatherChanged`.

## Related

- [Elements](../Magic/Elements.md) (weather colour) · [`../Architecture/WeatherSystem.md`](../../Architecture/WeatherSystem.md) (full analysis + gaps)
