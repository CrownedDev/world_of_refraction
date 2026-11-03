# Expedition 33 and our balance targets (B-293c)

**Status:** analysis for Crown (2026-11-03, cloud writer). What Clair Obscur: Expedition 33 did, what the first nightly balance run showed, and what B-293c changed. Decisions: Q-293c (answered by the reference-backed default), Q-293d (OPEN, Crown's).

## What Expedition 33 did

| # | Lesson | Source |
|---|---|---|
| 1 | **Difficulty scales enemy damage and the defence windows, not enemy HP.** Story / Expeditioner / Expert change how hard enemies hit and how wide the parry and dodge windows are. | [Sportskeeda difficulty guide](https://sportskeeda.com/esports/clair-obscur-expedition-33-difficulty-settings-explained-which-one-go-for) |
| 2 | **Fights are short; the threat is enemy hits, not long HP bars.** | [MobileSyrup interview](https://mobilesyrup.com/2025/11/27/clair-obscur-expedition-33-devs-combat-hard-juicy/) |
| 3 | **"No frustrating deaths."** A loss should come from not yet reading an enemy's attacks; a boss expects about one loss and a win on the retry. | [Automaton interview](https://automaton-media.com/en/news/clair-obscur-expedition-33s-battle-system-was-designed-around-the-premise-of-no-frustrating-deaths-and-a-game-you-can-clear-with/) |
| 4 | **Cap the multipliers that stack.** The Stendhal build reached about 2 billion damage and was cut 40% in patch 1.2.3. | [Destructoid, Stendhal nerf](https://www.destructoid.com/expedition-33-stendhal-nerf-update-patch-notes/) |
| 5 | **Keep difficulty steps even.** Expert's damage jump drew complaints for being too steep. | [Sportskeeda difficulty guide](https://sportskeeda.com/esports/clair-obscur-expedition-33-difficulty-settings-explained-which-one-go-for) |

## What the first nightly showed (2026-11-03, before B-293c)

| Target | Band | Measured |
|---|---|---|
| Normal fight at Medium | 2.5–5.5 | ~18–22 party turns |
| Normal fight at Hard | 2.5–5.5 | ~46–49 party turns |
| Mini-boss rounds, Medium / Hard | 7–11 | 4–6 / 15–31 |
| Boss rounds, Early Medium / Hard | 11–15.5 | 8 / 35–52 |
| Pillar-6 win rate | 0.35–0.65 | 0.92 |
| Win rate vs normal at Medium | 0.85–0.99 | 1.00 |

## What was wrong with the measuring

1. **Normal fights were counted in member turns, the target is party rounds.** BaselineNumbers §2.9: "a group of 2-3 dies in about 3-5 party rounds (3 actors)". The report compared it with every member's turns, three times too many: ~20 party turns is ~6.7 rounds. Fixed in the report.
2. **The sim's "Hard" is the enemy AI's skill, not the trial difficulty.** At Hard the AI tried to defend 85% of hits, chose a full dodge whenever it could, aimed inside the perfect band, and parried 40% of the rest, so most party hits did nothing. That, not HP, is why Hard ran ~2.5x Medium. Enemy HP is scaled once (rank x tier x zone; difficulty at Medium is x1).

## What B-293c changed (values only)

| Change | Lesson | Before → after |
|---|---|---|
| Normal-fight target counts party rounds | measuring | member turns → turns / party size |
| AI defence attempts in even steps | 1, 2, 5 | Easy / Medium / Hard / Expert 0.40 / 0.65 / 0.85 / 0.95 → 0.30 / 0.45 / 0.60 / 0.75 |
| AI parry (Hard / Expert) | 1, 2 | 0.4 / 0.7 → 0.25 / 0.5 |
| AI aim (multiples of the perfect band) | 1, 5 | 3.0 / 1.6 / 0.9 / 0.35 → 2.6 / 1.9 / 1.3 / 0.8 |
| Trial difficulty: enemy damage only | 1, 5 | HP and damage 0.8 / 1 / 1.25 → HP 1 / 1 / 1, damage 0.8 / 1 / 1.2 |

Party allies use the Hard AI too (`ALLY_AI_DIFFICULTY`), so the allies defend less and enemy hits land more: that is the lever on the 100% win rate (lesson 2), rather than more enemy HP.

## Crown's answers (Q-293d, 2026-11-03; B-293d)

Mini-boss x4 and boss x8 HP; the requirement floor at 0.25; a 10% damage floor after Defense, Damage Taken Down and Guard (Elden Ring's chip damage). The pillar-6 band was not moved. Re-run the nightly.

## Before the answers: what needed Crown (Q-293d)

- **Boss and mini-boss length at Medium.** 8 boss rounds and 4–6 mini-boss rounds against 11–15.5 and 7–11. Once enemy defending is lighter they get shorter still. The lever is the rank HP (Mini-boss x3, Boss x6), which is on Crown's Decided list.
- **Pillar 6.** Low level + S gear beats high level + F gear 92% of the time. BaselineNumbers §4 expected exactly that: with the floor at 0.5 the S weapon's part is ~2.3x the F weapon's, and "good gear gives a low level a real chance". The 35–65% band contradicts the Decided floor and requirements.
- **Stacked damage reduction.** Defense caps at 75% and Damage Taken Down at 90% (B-288), but they multiply with each other and with Guard: 0.25 × 0.10 × 0.5 lets 1.25% of a hit through, and a Perfect Guard 0%. Each cap holds; their product has none (lesson 4). The crit cap (x2.5) holds when crit sources stack (B-288's compose spec).
