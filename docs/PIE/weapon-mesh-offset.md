# PIE - Weapon mesh offset (where the hand holds a long weapon)

**Branch:** `feature/weapon-mesh-offset` (built and tested on the desktop 2026-11-18). **Built unseen:** not played yet.

**What it is:** two new fields on every weapon, beside Mesh Rotation and Left Mesh Rotation:
- **Mesh Offset:** where the right-hand mesh's own origin sits from the hand socket, in cm.
- **Left Mesh Offset:** the same for this weapon's mesh in the left hand (a dual weapon's left mesh, or the piece held as an off hand: a shield, a second weapon).

Both are zero by default, so every weapon is held exactly as before until a value is set.

**How the numbers read:** the offset is along the **hand socket's** axes, not the weapon's. Mesh Rotation turns the weapon about its own origin and does not turn the offset. So for a mesh whose origin is at the butt of the handle (the War Axe, about 128 cm long), find by eye which socket axis the shaft lies along once it is in the hand, and move the mesh back along that axis by about half its length (about 64 cm) so the hand holds the middle.

**Editor steps (after merge):** set Mesh Offset on the long weapons (the War Axe first). A new field needs a full editor restart before the editor connection can write it.

| # | Do this | You should see | Result |
|---|---|---|---|
| 1 | Any weapon you have not touched, in the hub | Held exactly as before | [ ] |
| 2 | On the War Axe, set Mesh Offset to 64 on one axis, wear it | The axe has moved 64 cm along that axis of the hand; try the other axes and signs until the hand holds mid-shaft | [ ] |
| 3 | Change Mesh Rotation on that axe | The axe turns about its own origin; the origin stays where the offset put it | [ ] |
| 4 | A shield in the off hand with Left Mesh Offset set | The shield moves on the left hand by that much; its Mesh Offset (right hand) does nothing there | [ ] |
| 5 | A dual weapon (its own left mesh) with Left Mesh Offset set | Only the left-hand mesh moves | [ ] |
| 6 | Switch weapons and back, enter a fight | The offset is kept each time the weapon is drawn again | [ ] |
| 7 | The character's Details panel: Weapon Mesh, Debug Log Mesh State | The log shows "Mesh Offset (right)" and "Left Mesh Offset" with your values | [ ] |
