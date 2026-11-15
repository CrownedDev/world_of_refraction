# Hold time for "press = one action, hold = another" (LB / R)

**Researcher, 2026-11-15.** Our current value is 0.4 s (press steps the infusion source, hold steps the element).

**Recommendation: 0.5 s** (good range 0.4–0.6 s), plus a hold-time setting in Options (0.3–1.0 s), and a fill ring that appears after about 0.15 s.

**Why:**
- **Common defaults sit at 0.4–0.5 s.** iOS long press: 0.5 s ([Apple](https://developer.apple.com/documentation/uikit/uilongpressgesturerecognizer)). Android: 0.5 s by default, with 0.4 s as its "Short" accessibility option ([source](https://kiwix.lema.org/stackoverflow.com_en_all_2023-11/a/1931005)). Unreal 5.8 `InputTriggers.h` (read from the engine install): Tap 0.2 s, Hold-and-Release 0.5 s, Hold 1.0 s. Unity's input hold default is 0.4 s ([docs](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.8/api/UnityEngine.InputSystem.InputSettings.html)).
- **0.4 s is the bottom of that range**, so a slow or relaxed press can turn into a hold by accident. 1.0 s feels sluggish for an action you repeat every turn. 0.5 s is the middle.
- **Accessibility:** Xbox XAG 107 treats hold length as a barrier and expects a toggle or press alternative for holds ([XAG 107](https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/107)). XAG 121 lists "reduce button hold time" as a feature players look for. So make the hold time adjustable.
- **Fill ring:** standard for hold-to-confirm. It shows the hold is counting and stops false "it didn't work" presses. Delay it slightly so normal taps don't flash it.

Not found: published hold values from specific well-liked games. Treat 0.5 s as a starting point and tune it in PIE.
