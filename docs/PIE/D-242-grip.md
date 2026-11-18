# PIE — D-242: weapons attach to the grip sockets

**Branch:** `feature/D-242-grip-sockets` (built and tested on the desktop 2026-11-18). **Built unseen:** not played yet.

**What it is:** a weapon now attaches to one hold per hand on the body, in place of one socket per weapon type.
- Right-hand weapon (any type): `hand_r_grip`.
- Off-hand weapon (dual wield): `hand_l_grip`.
- Shield: `hand_l_shield` in the left hand, `hand_r_shield` in the right.
- If the body has no such socket, the weapon uses the older socket for its type (a shield: `hand_l_sas`) and the Output Log warns once, naming the body mesh and the socket.
- `wor.Weapons.UseGrip 0` switches every hand back to the older sockets, `1` (the default) back to the grip; the weapons move at once.

**Known:** weapons are to be made to fit the grip (the reference is the Beastslayer Sword). Until a weapon has its fixed copy it may sit turned or shifted on the grip; the weapon's own Mesh Rotation is still applied on top. Use check 3 to list which need a fixed copy.

**Editor steps (Crown):** the sockets `hand_l_grip`, `hand_l_shield` and `hand_r_shield` on the shared skeleton. `hand_r_grip` is there already.

| # | Do this | You should see | Result |
|---|---|---|---|
| 1 | Male body, sword in hand, in the hub | The sword sits in the right hand at the grip; no warning in the Output Log | [ ] |
| 2 | Female body, the same | The same hold as the male body (not sideways, not at the hip) | [ ] |
| 3 | Console `wor.Weapons.PrintSockets` | One line per character: the weapon, the body mesh, and for each hand `hand_r_grip (grip)` or the older socket | [ ] |
| 4 | Console `wor.Weapons.UseGrip 0`, then `1` | The weapon jumps to the older hold, then back to the grip | [ ] |
| 5 | Sword and shield | The shield on `hand_l_shield` once that socket exists; before that, on the older shield socket with one warning | [ ] |
| 6 | Two weapons of one type (dual wield) | The second on `hand_l_grip` once that socket exists; before that, on the older left socket with one warning | [ ] |
| 7 | Each weapon type you have (axe, greatsword, dagger) | Note here which sit wrong on the grip: those need a fixed copy | [ ] |
| 8 | An enemy with a weapon, in a fight | It holds it the same way as the player | [ ] |
