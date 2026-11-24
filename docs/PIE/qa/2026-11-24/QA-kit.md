# QA kit (`chore/qa-kit`): agent QA (2026-11-24)

Played on the QA save in a temp checkout without the vendor art (black world; no skill animations). Head `f3f3932b3`, clean build OK.

| # | Check | Result |
|---|---|---|
| 1 | `wor.Debug.QAKit skill` | Not checkable here: "fitting: none fits", because the sword skills' animations live in the vendor packs, outside git. Needs Crown's checkout. (Found and fixed at `f3f3932b3`: a brand-new character learned no skill.) |
| 4 | `wor.Debug.QAKit caster` | QA passed (agent): Broken Darkness Lord joined as your seat; the Gate shows "You" |
| 5 | Caster's turn | QA passed (agent): its actions were picked by me; the Darkness selector is a white frame on the lit row and the card |
| 8 | `wor.Debug.QAKit caster ice` | QA passed (agent): refused, nothing changed |

Not checked: 2, 3, 3b (need the skill), 6, 7, 9.
