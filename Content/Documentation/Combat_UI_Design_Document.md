# Combat UI Design Document
## World of Refraction - Radial Menu System

**Date:** November 25, 2025  
**Version:** 1.0  
**Status:** Design Complete - Ready for Implementation

---

## Table of Contents
1. [Core Concept](#core-concept)
2. [UI Layout Structure](#ui-layout-structure)
3. [Visual Design Philosophy](#visual-design-philosophy)
4. [Button States & Energy System](#button-states--energy-system)
5. [Visual Hierarchy](#visual-hierarchy)
6. [Spell Type Visual Themes](#spell-type-visual-themes)
7. [Infusion System](#infusion-system)
8. [Technical Specifications](#technical-specifications)
9. [Implementation Phases](#implementation-phases)

---

## Core Concept

### Design Philosophy
**"Crystalline Energy Release"**

The combat UI represents magical energy stored within prismatic crystals. When actions are selected, the energy is released from the crystal, causing it to visually "drain" - becoming lighter and more desaturated. This creates a clear visual metaphor: **saturated = charged, desaturated = depleted**.

### Thematic Foundation
- **Base State:** White/clear crystalline material
- **Charged State:** Saturated with character's element color
- **Depleted State:** Desaturated toward white (energy released)
- **Element Identity:** Shows in actual spell/ability effects, not just UI chrome

### Back Button / Navigation Control

**Input Methods:**
```
Keyboard/Mouse:
├─ ESC key: Goes back one level
├─ Right-click: Goes back one level
└─ Click center hub when rings open: Goes back one level

Controller:
├─ B button (Xbox) / Circle (PlayStation): Goes back one level
├─ Click right stick: Goes back one level
└─ Move stick to center deadzone: Highlights back action
```

**Visual Indicator:**
```
Option A: Center Hub Glow
├─ When rings are open, center hub pulses gently
├─ Indicates "click here to go back"
└─ Subtle, not distracting

Option B: Dedicated Back Button
├─ Small button in center of hub
├─ "← BACK" label
└─ Only visible when rings are open

Option C: Edge Button
├─ Small button on outer edge of current ring
├─ "← BACK" or just arrow icon
└─ Follows cursor/controller input

[Design Decision: Option A recommended - cleanest visually]
```

---

## Navigation Flow & Ring Behavior

### Quick Reference Summary

**Ring Structure:**
- **Center Hub (200px):** 4 buttons - Always visible, never collapses
- **First Ring (350px):** 4 or 6 buttons - Schools, Abilities, or Items
- **Second Ring (500px):** 6 buttons - Spells only (when school selected)

**Navigation Depth:**
- **Items/Abilities:** 1 level deep (Center → First Ring → Use)
- **Spells:** 2 levels deep (Center → First Ring → Second Ring → Cast)

**Collapse Behavior:**
- Rings collapse **INTO** each other (inward motion toward center)
- Second Ring collapses into First Ring (0.3s)
- First Ring collapses into Center Hub (0.3s)

**Back Navigation:**
- ESC / Right-click / B button goes back one level
- Center hub pulses when rings are open (indicates back action)

---

### Ring Expansion Rules

**Maximum Depth:**
- **3 Rings on screen maximum:** Center Hub + First Ring + Second Ring
- Center Hub is ALWAYS visible (never collapses)

**Expansion Behavior:**

1. **Opening First Ring:**
   ```
   User clicks: Items, Abilities, or Refractions
   Animation: First Ring expands outward from center (0.4s)
   Result: Center Hub + First Ring visible
   ```

2. **Opening Second Ring (Spells Only):**
   ```
   User clicks: A school (Destruction, Conjuration, Enhancement, Restoration)
   Animation: Second Ring expands outward from First Ring (0.4s)
   Result: Center Hub + First Ring + Second Ring visible
   ```

3. **Direct Actions (No Second Ring):**
   ```
   From First Ring:
   ├─ Click Item → Uses immediately, rings stay open
   ├─ Click Ability → Uses immediately, rings stay open
   └─ Click Attack/Ultimate from center → Uses immediately
   ```

### Ring Collapse Behavior

**Back Navigation:**

```
From Second Ring (spells):
├─ Press Back/Escape
├─ Animation: Second Ring collapses INTO First Ring (0.3s)
└─ Result: Center Hub + First Ring remain

From First Ring:
├─ Press Back/Escape
├─ Animation: First Ring collapses INTO Center Hub (0.3s)
└─ Result: Only Center Hub remains

From Center Hub:
├─ Press Back/Escape
└─ Menu closes completely
```

**After Action:**
```
When action is executed (spell cast, item used, ability used):
├─ Option A: All rings collapse immediately (fast combat)
├─ Option B: Rings stay open for quick follow-up actions
└─ [Design Decision: To be determined during testing]
```

### Visual Depth States

**State 1: Menu Closed**
```
Nothing visible
```

**State 2: Center Hub Only**
```
    Attack
      |
Ultimate ⊕ Abilities
      |
    Items
```

**State 3: First Ring Open**
```
      ╔═══════════╗
      ║  Ring 1   ║
    ║  (4 or 6)   ║
      ║   [Hub]   ║
      ╚═══════════╝
```

**State 4: Second Ring Open (Spells Only)**
```
  ╔════════════════╗
  ║   Ring 2 (6)   ║
  ║  ╔═══════╗     ║
  ║  ║ Ring1 ║     ║
  ║  ║ [Hub] ║     ║
  ║  ╚═══════╝     ║
  ╚════════════════╝
```

### Navigation Examples

**Example 1: Using an Item**
```
1. Open menu → Center Hub appears
2. Click "Items" → First Ring expands (6 items)
3. Click "Health Potion" → Item used, [rings close/stay open]
4. Turn continues
```

**Example 2: Using an Ability**
```
1. Open menu → Center Hub appears
2. Click "Abilities" → First Ring expands (6 abilities)
3. Click "6-Hit Combo" → Ability executes, [rings close/stay open]
4. Turn continues
```

**Example 3: Casting a Spell**
```
1. Open menu → Center Hub appears
2. Click "Refractions" → First Ring expands (4 schools)
3. Click "Destruction" → Second Ring expands (6 spells)
4. Click "Fireball" → Spell casts, [rings close/stay open]
5. Turn continues
```

**Example 4: Changing Mind**
```
1. Open menu → Center Hub
2. Click "Refractions" → First Ring (4 schools)
3. Click "Destruction" → Second Ring (6 spells)
4. Press Back → Second Ring collapses into First Ring
5. Press Back → First Ring collapses into Center Hub
6. Click "Items" instead → First Ring expands (6 items)
```

---

## UI Layout Structure

### Visual Overview - Three Ring System

```
STATE 1: Menu Closed
═══════════════════════════════════════
(Nothing visible)


STATE 2: Center Hub Open (Always First)
═══════════════════════════════════════
              Attack
                |
    Ultimate -- ⊕ -- Abilities/Refractions
                |
              Items
        
        (4 buttons, radius ~200px)


STATE 3: First Ring Expanded
═══════════════════════════════════════
        ╔════════════════════╗
        ║    School 1        ║
        ║                    ║
   ║  School 4   ⊕   School 2  ║
        ║                    ║
        ║    School 3        ║
        ╚════════════════════╝
    
    Center hub (⊕) remains visible
    First Ring: 4 schools (radius ~350px)
    OR: 6 abilities/items at 60° spacing


STATE 4: Second Ring Expanded (Spells Only)
═══════════════════════════════════════
    ╔═══════════════════════════════╗
    ║ Spell1      Spell2      Spell3║
    ║    ╔═══════════════╗          ║
    ║    ║   School 1    ║          ║
    ║    ║      ⊕        ║          ║
    ║    ║   School 4    ║          ║
    ║    ╚═══════════════╝          ║
    ║ Spell4      Spell5      Spell6║
    ╚═══════════════════════════════╝

    Center hub (⊕) still visible (background)
    First Ring: School buttons visible
    Second Ring: 6 spells (radius ~500px)
    
    Maximum depth: 3 rings simultaneously
```

### Ring Radii & Spacing

```
Center Hub:   Radius 200px  (4 buttons at 90°)
First Ring:   Radius 350px  (4 or 6 buttons)
Second Ring:  Radius 500px  (6 buttons at 60°)

Spacing between rings: ~150px
Total UI footprint: ~1000px diameter at max depth
```

---

### Center Hub - Always 4 Buttons

```
       Attack
         |
Ultimate - ⊕ - Abilities/Refractions
         |
       Items
```

**Button Positions:**
- **Top (0°):** Attack
- **Right (90°):** Abilities (weapon equipped) OR Refractions (no weapon)
- **Bottom (180°):** Items
- **Left (270°):** Ultimate

**Visibility Rules:**
- Attack, Items, Ultimate: **ALWAYS visible**
- Right button switches based on weapon state:
  - Weapon equipped → **Abilities** (up to 6 weapon skills)
  - No weapon → **Refractions** (spell schools)

### First Ring - Context Dependent (Radius: 350px)

**When "Refractions" Selected (No Weapon):**
```
4 School Segments (90° each):
├─ DESTRUCTION
├─ CONJURATION
├─ ENHANCEMENT
└─ RESTORATION

Click school → Expands to Second Ring
```

**When "Abilities" Selected (Weapon Equipped):**
```
6 Ability Segments (60° each):
├─ Ability 1
├─ Ability 2
├─ ...
└─ Ability 6

Click ability → Uses immediately (no second ring)
```

**When "Items" Selected:**
```
6 Item Segments (60° each):
├─ Item Slot 1
├─ Item Slot 2
├─ ...
└─ Item Slot 6

Click item → Uses immediately (no second ring)
```

### Second Ring - Spells Only (Radius: 500px)

**ONLY appears when a School is selected from First Ring:**

```
6 Spell Segments (60° each):
├─ Spell 1 (learned, enabled)
├─ Spell 2 (learned, enabled)
├─ Spell 3 (locked, grayed)
├─ Spell 4 (learned, enabled)
├─ Spell 5 (locked, grayed)
└─ Spell 6 (learned, enabled)

Click spell → Casts spell
```

**Note:** Abilities and Items do NOT have a second ring - they execute directly from First Ring.

---

## Visual Design Philosophy

### Crystalline Material Concept

**Base Material Properties:**
- Semi-transparent crystalline structure
- Prismatic refraction effects on edges
- Subtle chromatic aberration (RGB split)
- Inner glow emanating from center
- Energy flow animations

**NOT Using:**
- Heavy background blur (keeps combat visible)
- Solid opaque segments (maintains crystal aesthetic)
- Generic flat colors (everything has depth/material)

### Color Application Strategy

**Center Hub & Schools:**
- All share character's element color
- Differentiated by energy flow patterns and text labels
- No icons, just clear text (ATTACK, ABILITIES, etc.)

**Individual Spells/Abilities:**
- Show their TRUE element colors
- This is where color variety appears
- Element identity matters at spell level

**Example - Fire Mage:**
```
Center Hub: All red (fire element)
Schools: All red (fire element)
Spells: 
├─ Fireball: Red (fire)
├─ Ice Shard: Blue (water - learned from other source)
└─ Stone Spear: Brown/Yellow (earth)
```

---

## Button States & Energy System

### State 1: Default (Charged)
```
Appearance:
├─ Deep, saturated element color
├─ Vibrant and rich
├─ Energy flowing through crystal
└─ Ready for use

Technical:
├─ Saturation: 100%
├─ Energy Level: 1.0
└─ Emissive: Base intensity
```

### State 2: Hover (Pre-Selection)
```
Appearance:
├─ Slight scale increase (1.05x)
├─ Border highlight appears
├─ Glow intensity increases slightly
└─ Indicates interactivity

Technical:
├─ Scale: 1.0 → 1.05
├─ Border: White glow (2-3px)
└─ Transition: 0.15s
```

### State 3: Clicked (Energy Released)
```
Appearance:
├─ Color desaturates toward white
├─ Becomes pale/washed out
├─ Energy has "left" the crystal
└─ Scale dips briefly (0.95x → 1.0x)

Technical:
├─ Saturation: 100% → 30%
├─ Lerp toward white: 70%
├─ Energy Level: 1.0 → 0.3
└─ Emissive: Reduced

Duration:
├─ Quick actions: 0.2s flash, return to full
├─ School selection: Stays pale while ring open
└─ Ultimate: May tie to cooldown duration
```

### State 4: Disabled
```
Appearance:
├─ Grayed out (80% grayscale)
├─ 30% opacity overall
├─ No energy flow animation
└─ Lock icon or "X" overlay

Technical:
├─ Desaturate: 80%
├─ Opacity: 0.3
└─ No interactivity
```

### Color Desaturation Math
```cpp
// Depleted state calculation
FLinearColor DepletedColor = FMath::Lerp(
    ElementColor,        // Full saturation (e.g., 1.0, 0.0, 0.0 for fire)
    FLinearColor::White, // (1.0, 1.0, 1.0)
    0.7f                 // 70% toward white
);

// Examples:
Fire:     (1.0, 0.0, 0.0) → (1.0, 0.7, 0.7) pale red
Water:    (0.0, 0.5, 1.0) → (0.7, 0.85, 1.0) pale blue
Darkness: (0.1, 0.1, 0.1) → (0.55, 0.55, 0.55) gray
```

---

## Visual Hierarchy

### Level 1: Center Hub
```
Visual Priority: Character identity
├─ All same element color
├─ Differentiation by energy flow:
│   ├─ Attack: Angular, sharp energy paths
│   ├─ Abilities: Spiral energy pattern
│   ├─ Items: Gentle wave motion
│   └─ Ultimate: Radial convergence (most dramatic)
└─ Text labels clearly visible (no icons)
```

### Level 2: Schools/Categories
```
Visual Priority: Organization
├─ Same element color as center hub
├─ Standard crystalline appearance
├─ Text labels (DESTRUCTION, CONJURATION, etc.)
├─ No special theming at this level
└─ Energy flow matches character element
```

### Level 3: Individual Spells/Abilities
```
Visual Priority: Specific actions & their elements
├─ TRUE element colors shown
├─ THIS is where visual theming happens
├─ Destruction spells: Fractured crystal
├─ Enhancement spells: Glossy/shiny
├─ Restoration spells: Cracked (heal on click)
└─ Color variety finally appears
```

---

## Spell Type Visual Themes

**IMPORTANT:** These visual themes apply to the **Second Ring** (individual spells), NOT the school buttons in the First Ring.

### Destruction Spells
```
Visual Theme: Fractured Crystalline Structure

Appearance:
├─ Visible cracks spreading across button
├─ Crystalline structure looks damaged/breaking
├─ Jagged, sharp edges
├─ Particles: Sharp shards breaking off
└─ Energy pattern: Explosive bursts

Material:
├─ Crack texture overlay (0.6-0.8 intensity)
├─ Crack animation: Subtle spreading
├─ Particle system: Sharp geometric shards
└─ Edge distortion: Jagged breaks

Feel: "This crystal is shattering/destructive"
Color: Spell's actual element (fire=red, water=blue, etc.)
```

### Conjuration Spells
```
Visual Theme: Forming/Coalescing Structure

Appearance:
├─ Particles gathering from edges to center
├─ "Materializing" shimmer effect
├─ Smooth, circular edges
├─ Energy pattern: Vortex inward
└─ Slight blur/transparency (forming)

Material:
├─ Vortex swirl pattern
├─ Particle system: Coalescing orbs
├─ Edge style: Smooth, circular
└─ Animation: Gathering motion

Feel: "Something being created/summoned"
Color: Spell's actual element
```

### Enhancement Spells
```
Visual Theme: Pristine/Polished Structure

Appearance:
├─ Extra glossy, mirror-like surface
├─ Bright highlights and reflections
├─ Stronger emissive glow than others
├─ Particles: Upward rising sparkles
└─ Energy pattern: Rising/empowering

Material:
├─ Glossiness: 0.85 (very shiny, vs 0.5 standard)
├─ Metallic: 0.2 (slight metallic sheen)
├─ Emissive: 1.8x base intensity
├─ Specular: 1.0 (strong highlights)
├─ Fresnel: Enhanced (brighter edges)
└─ Particle system: Sparkles rising

Feel: "Empowered, stronger, gleaming"
Color: Spell's actual element
```

### Restoration Spells
```
Visual Theme: Damaged → Healing Structure

Appearance (3 states):
1. Default: Clean, pristine crystal
2. Hover: Cracks fade in (0.3s)
   ├─ Visible fractures appear
   ├─ Gentle glow from crack lines
   └─ Particles: Damage dust
3. Click: Healing animation (1.0s)
   ├─ Healing light intensifies in cracks
   ├─ Cracks seal from center outward
   ├─ Particles: Healing wisps flow into cracks
   └─ Crystal becomes pristine + desaturated

Material:
├─ Crack texture (animated visibility)
├─ Crack glow: Element color emanating
├─ Healing animation: Sealing pattern
├─ Particle system: Healing wisps
└─ Animation cycle: Hover → Click → Heal

Feel: "Broken → Mending → Restored"
Color: Spell's actual element
```

---

## Infusion System

### Overview
When abilities have an element infused, buttons display **clearly visible** element-specific effects overlaid on the standard crystalline material.

### Activation Context
```
Weapon Equipped + Infusion Active:
├─ "Abilities" button shows infusion effect
├─ All 6 ability buttons show infusion effect
└─ Visual: "These attacks will deal [element] damage"

No Weapon OR No Infusion:
└─ Standard crystalline appearance (no overlay)
```

### Visual Intensity
```
Prominence Level: Clearly Visible
├─ Opacity: 0.7-0.9 (unmistakable)
├─ Takes up ~60% of button visual space
├─ Always animating (never static)
├─ Particles always emitting
├─ Edge glow 2.0x normal intensity
└─ Impossible to miss when active
```

### Element-Specific Infusion Effects

#### Fire Infusion
```
Visual: Animated flames
├─ Flame texture scrolling upward
├─ Flickering intensity (0.7-1.0 opacity)
├─ Particles: Small embers rising from bottom
├─ Edge glow: Orange-red
├─ Animation speed: Fast (0.8s cycle)
└─ Texture: Licking flames on surface

Color: (1.0, 0.0, 0.0) → (1.0, 0.5, 0.0) gradient
```

#### Water Infusion
```
Visual: Flowing liquid shimmer
├─ Ripple/wave distortion on surface
├─ Undulating motion (sine wave)
├─ Particles: Water droplets flowing across
├─ Edge glow: Blue
├─ Animation speed: Medium (1.2s cycle)
└─ Texture: Rippling water surface

Color: (0.0, 0.5, 1.0)
```

#### Lightning Infusion
```
Visual: Electric arcs
├─ Branching lightning texture
├─ Rapid flickering/crackling
├─ Particles: Small electric sparks
├─ Edge glow: Cyan-yellow
├─ Animation speed: Very fast (0.4s cycle)
└─ Texture: Electrical discharge pattern

Color: (1.0, 0.5, 0.0) → (1.0, 1.0, 0.0) gradient
```

#### Earth Infusion
```
Visual: Rocky/mineral texture
├─ Rough stone overlay on surface
├─ Heavy, stable appearance
├─ Particles: Dust/small pebbles orbiting
├─ Edge glow: Brown-yellow
├─ Animation speed: Slow (2.0s cycle)
└─ Texture: Rocky stone surface

Color: (1.0, 1.0, 0.0) → (0.6, 0.4, 0.2) gradient
```

#### Wind Infusion
```
Visual: Swirling air currents
├─ Translucent wisps flowing
├─ Spiral motion around button
├─ Particles: Leaves/air currents
├─ Edge glow: Green
├─ Animation speed: Fast (0.9s cycle)
└─ Texture: Flowing air streaks

Color: (0.0, 1.0, 0.0)
```

#### Light Infusion
```
Visual: Radiant beams
├─ Bright rays emanating outward
├─ Pulsing glow (breathing)
├─ Particles: Light motes floating
├─ Edge glow: White-yellow
├─ Animation speed: Medium pulse (1.5s)
└─ Texture: Sunray burst pattern

Color: (1.0, 1.0, 1.0)
```

#### Darkness Infusion
```
Visual: Shadowy tendrils
├─ Dark wisps crawling across surface
├─ Ominous, creeping motion
├─ Particles: Dark smoke
├─ Edge glow: Purple-black
├─ Animation speed: Slow, menacing (1.8s)
└─ Texture: Shadow tendrils

Color: (0.1, 0.1, 0.1) → (0.4, 0.0, 0.5) gradient
```

#### Void Infusion
```
Visual: Reality distortion
├─ Warping/bending of space
├─ Chromatic aberration (RGB split)
├─ Particles: Fragmenting geometry
├─ Edge glow: Violet
├─ Animation speed: Irregular/chaotic
└─ Texture: Distortion/displacement map

Color: (0.6, 0.0, 1.0)
```

#### Reality Infusion
```
Visual: Geometric patterns
├─ Sacred geometry overlays
├─ Crystalline fractals forming
├─ Particles: Geometric shapes
├─ Edge glow: Indigo-white
├─ Animation speed: Steady (1.0s)
└─ Texture: Mandala/geometric pattern

Color: (0.3, 0.0, 0.5) → (1.0, 1.0, 1.0) gradient
```

### Infusion Technical Implementation
```cpp
// Material Layer Structure:
Base Crystal Material (character element)
+ Infusion Overlay Material (additive blend)

Infusion Layer Properties:
├─ BlendMode: Additive
├─ Opacity: 0.7-0.9
├─ AnimationStrength: High (prominent movement)
├─ ParticleCount: Medium-High
├─ EdgeGlowIntensity: 2.0x
└─ Always animating when active

Transition:
├─ Infusion ON: Fade in (0.3s)
├─ Infusion OFF: Fade out (0.5s)
└─ Smooth lerp between states
```

---

## Technical Specifications

### Material System Architecture

#### Base Crystal Material
```cpp
M_CrystalButton_Base

Parameters:
├─ ElementColor (FLinearColor): Character's element
├─ EnergyLevel (float 0-1): Saturation control
│   ├─ 1.0 = Full saturation (charged)
│   └─ 0.3 = Desaturated (depleted)
├─ Glossiness (float): 0.5 (standard)
├─ Metallic (float): 0.0 (non-metallic)
├─ Emissive (float): 1.0x base
├─ FlowPattern (enum): Energy animation type
└─ Opacity (float): 0.7-0.9 (semi-transparent)

Visual Features:
├─ Semi-transparent crystalline base
├─ Chromatic aberration on edges (0.02)
├─ Rim lighting (element color)
├─ Inner glow (50% intensity)
├─ Fresnel effect (edge highlights)
└─ Energy flow animation
```

#### Destruction Spell Material
```cpp
M_CrystalButton_Destruction (inherits M_CrystalButton_Base)

Additional Parameters:
├─ CrackTexture: Fracture pattern overlay
├─ CrackIntensity (float): 0.6-0.8
├─ ShardEmissionRate (float): Low-medium
└─ EdgeSharpness (float): High (jagged)

Additional Features:
├─ Crack texture overlay (multiply blend)
├─ Jagged edge distortion
├─ Particle system: Sharp geometric shards
└─ Subtle crack spreading animation
```

#### Enhancement Spell Material
```cpp
M_CrystalButton_Enhancement (inherits M_CrystalButton_Base)

Modified Parameters:
├─ Glossiness: 0.85 (very shiny)
├─ Metallic: 0.2 (slight metallic)
├─ Emissive: 1.8x base intensity
├─ Specular: 1.0 (strong highlights)
└─ FresnelIntensity: 1.5x

Additional Features:
├─ Enhanced reflection probe capture
├─ Stronger rim lighting
├─ Particle system: Rising sparkles
└─ Mirror-like reflections
```

#### Restoration Spell Material
```cpp
M_CrystalButton_Restoration (inherits M_CrystalButton_Base)

Additional Parameters:
├─ CrackTexture: Organic healing pattern
├─ CrackVisibility (float 0-1): Animated visibility
├─ CrackGlowIntensity (float): Element color in cracks
├─ HealProgress (float 0-1): Sealing animation
└─ WispEmissionRate (float): Healing particles

States:
├─ Default: CrackVisibility = 0 (pristine)
├─ Hover: CrackVisibility = 0 → 0.8 (0.3s)
├─ Click: HealProgress = 0 → 1.0 (1.0s)
│   └─ Cracks seal from center outward
└─ Post-heal: CrackVisibility = 0 (clean)

Additional Features:
├─ Animated crack texture
├─ Glow emanating from cracks
├─ Healing animation pattern
└─ Particle system: Healing wisps
```

#### Conjuration Spell Material
```cpp
M_CrystalButton_Conjuration (inherits M_CrystalButton_Base)

Additional Parameters:
├─ VortexIntensity (float): Swirl strength
├─ ParticleGatherSpeed (float): Coalescing speed
└─ EdgeSmoothness (float): Very smooth

Additional Features:
├─ Vortex swirl pattern (UV distortion)
├─ Smooth, circular edges
├─ Particle system: Coalescing orbs
└─ Gathering motion animation
```

#### Infusion Overlay Material
```cpp
M_InfusionOverlay (separate material, additive blend)

Parameters:
├─ InfusionType (enum): Fire/Water/Lightning/Earth/Wind/Light/Darkness/Void/Reality
├─ InfusionIntensity (float): 0.7-0.9
├─ AnimationSpeed (float): Element-dependent
├─ ParticleEmissionRate (float): Medium-High
└─ EdgeGlowMultiplier (float): 2.0

Features:
├─ Element-specific texture (animated)
├─ Element-specific particles
├─ Additive blend with base crystal
├─ Always animating when active
└─ 9 unique visual variants (one per element)
```

### Widget Component Structure
```
WBP_RadialMenu_Master (Root Widget)
├─ [Canvas Panel] - Full screen
│   ├─ [WBP_CenterHub] - Always visible (4 buttons)
│   │   ├─ Attack button (Top)
│   │   ├─ Abilities/Refractions button (Right)
│   │   ├─ Items button (Bottom)
│   │   ├─ Ultimate button (Left)
│   │   └─ Center prism visual
│   │
│   ├─ [WBP_FirstRing] - Expandable (4 or 6 buttons)
│   │   ├─ Visibility: Collapsed by default
│   │   ├─ Radius: 350px from center
│   │   ├─ Content: Schools OR Abilities OR Items
│   │   └─ Animation: Expand/Collapse
│   │
│   └─ [WBP_SecondRing] - Expandable (6 buttons, spells only)
│       ├─ Visibility: Collapsed by default
│       ├─ Radius: 500px from center
│       ├─ Content: Spells (only when school selected)
│       └─ Animation: Expand/Collapse

WBP_CrystalButton (Individual Button Component)
├─ [Canvas Panel] - Root
│   └─[Overlay]
│       ├─[Image] - Background blur (optional)
│       ├─[Image] - Crystal base material
│       ├─[Image] - Spell type variant material (if applicable)
│       ├─[Image] - Infusion overlay material (if active)
│       ├─[Particle System] - Element particles
│       └─[Text Block] - Label
│
└─Animations:
    ├─ Hover_Anim (0.15s, scale 1.0→1.05, glow)
    ├─ Click_Anim (0.2s, scale 1.0→0.95→1.0, desaturate)
    └─ Restoration_Heal_Anim (1.0s, crack sealing)
```

### Animation Timing Standards
```
UI Animation Guidelines:
├─ Hover feedback: 0.10-0.15s (snappy)
├─ Click feedback: 0.15-0.25s (responsive)
├─ Ring expansion: 0.40s (smooth outward growth)
│   ├─ Scale: 0.5 → 1.0
│   ├─ Rotation: -180° → 0° (spiral effect)
│   ├─ Opacity: 0 → 1.0
│   └─ Stagger: 0.05s per segment
├─ Ring collapse: 0.30s (quick inward shrink)
│   ├─ Scale: 1.0 → 0.5 → 0.0
│   ├─ Rotation: 0° → 90° (partial spin)
│   ├─ Opacity: 1.0 → 0
│   └─ All segments simultaneous (no stagger)
├─ Energy flow: 0.8-2.0s cycles (ambient)
├─ Restoration heal: 1.0s (satisfying)
└─ Infusion fade: 0.3s in, 0.5s out

Curve: Ease-in-out cubic (smooth, natural)
Ring collapse destination: Collapses INTO inner ring (toward center)
```

### Ring Collapse Animation Details

**Second Ring → First Ring Collapse:**
```
Animation: "Ring_Collapse_Inward"
Duration: 0.3s

Keyframes:
├─ 0.0s: Scale 1.0, Opacity 1.0, Position (radius 500px)
├─ 0.1s: Scale 0.9, Opacity 0.8, Position (radius 450px)
├─ 0.2s: Scale 0.6, Opacity 0.4, Position (radius 400px)
└─ 0.3s: Scale 0.0, Opacity 0.0, Position (radius 350px - First Ring)

Visual: Second ring shrinks and moves inward toward First Ring
Effect: "Absorbed" or "sucked into" the inner ring
```

**First Ring → Center Hub Collapse:**
```
Animation: "Ring_Collapse_ToCenter"
Duration: 0.3s

Keyframes:
├─ 0.0s: Scale 1.0, Opacity 1.0, Position (radius 350px)
├─ 0.1s: Scale 0.9, Opacity 0.8, Position (radius 300px)
├─ 0.2s: Scale 0.6, Opacity 0.4, Position (radius 250px)
└─ 0.3s: Scale 0.0, Opacity 0.0, Position (radius 200px - Center Hub)

Visual: First ring shrinks and moves inward toward Center Hub
Effect: "Absorbed" or "sucked into" the center
```

---

## Implementation Phases

### Phase 1: Base Crystal System
**Goal:** Establish foundation with energy saturation mechanic

**Tasks:**
1. Create M_CrystalButton_Base material
   - Semi-transparent crystalline base
   - Element color input parameter
   - Energy level parameter (0-1 saturation)
   - Basic glossiness/emissive properties

2. Implement energy saturation system
   - Full color = charged (1.0)
   - Desaturated = depleted (0.3)
   - Lerp toward white calculation

3. Create WBP_CrystalButton widget
   - Image component with material instance
   - Text label support
   - Hover/click state handling

4. Test with Fire element
   - Red → Pale red transition
   - Verify visual clarity
   - Timing adjustments

**Deliverable:** Functional button that changes from saturated to desaturated on click

---

### Phase 2: Spell Type Variants
**Goal:** Create visual themes for different spell categories

**Tasks:**
1. Create M_CrystalButton_Destruction
   - Add crack texture overlay
   - Implement shard particle system
   - Jagged edge effects
   - Test with multiple element colors

2. Create M_CrystalButton_Enhancement
   - Increase glossiness (0.85)
   - Boost emissive (1.8x)
   - Add sparkle particles
   - Enhanced reflections

3. Create M_CrystalButton_Restoration
   - Implement crack visibility system
   - Create hover → crack animation (0.3s)
   - Create click → heal animation (1.0s)
   - Add healing wisp particles

4. Create M_CrystalButton_Conjuration
   - Add vortex swirl pattern
   - Smooth edge styling
   - Coalescing particle system

**Deliverable:** 4 distinct spell type materials that work with any element color

---

### Phase 3: Infusion System
**Goal:** Implement element-specific ability overlays

**Tasks:**
1. Create M_InfusionOverlay material base
   - Additive blend mode setup
   - Opacity/intensity parameters
   - Animation speed controls
   - Particle emission settings

2. Implement Fire infusion (proof of concept)
   - Flame texture scrolling
   - Ember particles rising
   - Orange-red edge glow
   - Test with ability buttons

3. Add remaining 8 element infusions
   - Water: Ripple/flow
   - Lightning: Electric arcs
   - Earth: Stone texture
   - Wind: Air currents
   - Light: Radiant beams
   - Darkness: Shadow tendrils
   - Void: Distortion
   - Reality: Geometric patterns

4. Implement infusion on/off transitions
   - Fade in (0.3s)
   - Fade out (0.5s)
   - Smooth blending

**Deliverable:** Complete infusion overlay system for all 9 elements

---

### Phase 4: Widget Integration & Polish
**Goal:** Integrate materials into complete radial menu system

**Tasks:**
1. Create WBP_CenterHub (Center - Always Visible)
   - 4 button layout (Attack, Abilities/Refractions, Items, Ultimate)
   - Fixed positions (Top, Right, Bottom, Left)
   - Dynamic button content (Abilities vs Refractions based on weapon)
   - Energy flow patterns per button type
   - Text labels
   - Center prism visual

2. Create WBP_FirstRing (Expandable Ring)
   - Dynamic segment count (4 schools OR 6 abilities OR 6 items)
   - Material instance assignment based on content type
   - Expand animation (0.4s outward from center)
   - Collapse animation (0.3s inward to center)
   - Radius: 350px

3. Create WBP_SecondRing (Spell Ring Only)
   - Fixed 6 spell button layout
   - Only appears when school selected from First Ring
   - Spell type material assignment (Destruction/Enhancement/etc)
   - Element color assignment per spell
   - Locked/unlocked states
   - Expand animation (0.4s outward from First Ring)
   - Collapse animation (0.3s inward to First Ring)
   - Radius: 500px

4. Create WBP_RadialMenu_Master (Root Container)
   - Manages all three ring widgets
   - Handles navigation state (which rings are open)
   - Input routing (mouse/controller)
   - Back navigation logic
   - Ring collapse sequencing

5. Implement state management
   - Weapon equipped detection → Abilities vs Refractions
   - Infusion active detection → Overlay effects on abilities
   - Material parameter updates → Energy levels, colors
   - Navigation tracking → Which ring is active
   - Back button handling → Collapse to previous level

6. Polish & optimization
   - Animation timing refinement
   - Ring collapse "into each other" feel
   - Performance profiling
   - Material instance pooling
   - Input handling (mouse + controller)
   - Back navigation feedback (center hub pulse?)

**Deliverable:** Complete, functional radial menu system with proper three-ring nested navigation

---

### Phase 5: Testing & Refinement
**Goal:** Verify all systems work together and feel good

**Tasks:**
1. Test all element colors
   - Verify saturation changes are clear
   - Check readability for all 9 elements
   - Test with Generic and BrokenDarkness

2. Test all spell type combinations
   - Destruction spells in all elements
   - Enhancement spells in all elements
   - Restoration spells in all elements
   - Conjuration spells in all elements

3. Test infusion system
   - All 9 elements on ability buttons
   - Infusion on/off transitions
   - Visual clarity during combat

4. Accessibility testing
   - Colorblind modes (if needed)
   - High contrast mode
   - Reduced motion option

5. Performance optimization
   - Material complexity reduction (if needed)
   - Particle system LOD
   - Widget pooling efficiency

**Deliverable:** Polished, performant, combat-ready UI system

---

## Design Decisions Record

### Why No Icons on Center Hub & Schools?
- Reduces visual clutter
- Text is clearer for quick reading
- Icons would compete with element colors
- Anime aesthetic favors clean, readable design
- Icons reserved for actual spells/abilities (where they matter)

### Why Desaturation Instead of Other Feedback?
- Thematically consistent ("energy leaving crystal")
- Visually distinct from hover state
- Works across all element colors
- Clear visual metaphor players understand instantly
- Allows infusion effects to remain vibrant

### Why Different Visual Themes for Spell Types?
- Communicates spell function at a glance
- Adds visual interest without color coding
- Thematic consistency across all elements
- Destruction always looks "breaking" regardless of element
- Enhancement always looks "polished" regardless of element

### Why Element-Specific Infusion Effects?
- Each element has unique personality
- Fire should FEEL like fire, not generic energy
- Adds immersion and satisfaction
- Clearly communicates which element is infused
- Worth the extra work for quality experience

### Why Schools Don't Have Special Theming?
- Schools are organizational, not functional
- Visual complexity should be at the spell level
- Keeps middle layer clean and navigable
- Player needs to quickly see school names, not be distracted
- Maintains visual hierarchy (center → schools → spells)

---

## Open Questions / Future Considerations

### Generic Element
- Generic has no elemental affinity
- Brown color (0.6, 0.4, 0.2)
- Question: Can Generic characters use infusion?
- If yes: What does Generic infusion look like?

### BrokenDarkness Element
- Special state that absorbs other elements
- Base: Black (0.1, 0.1, 0.1)
- When absorbed: Black + absorbed element blend
- Question: How does infusion work with absorbed elements?
- Should it show the absorbed element's infusion effect?

### Post-Action Ring Behavior
- After casting a spell/using item/executing ability, should rings:
  - **Option A:** Close immediately (forces deliberate menu re-open, faster combat flow)
  - **Option B:** Stay open (allows quick follow-up actions, slower but more flexible)
- **Recommendation:** Test both during Phase 4, likely Option A for turn-based combat
- Consider: Ultimate should probably close menu (dramatic action)
- Consider: Items might stay open (quick healing then attack)

### School Button Energy Patterns
- Schools currently "follow suit" (standard crystalline)
- Question: Should they have unique patterns like center hub?
  - Destruction: Explosive/angular
  - Conjuration: Gathering/vortex
  - Enhancement: Rising/ascending
  - Restoration: Pulsing/heartbeat
- Or all the same pattern for consistency?

### Cooldown Integration
- Ultimate has cooldown
- Question: Should depleted state = cooldown visual?
- Or separate cooldown indicator (radial fill/timer)?
- Gradual re-saturation as cooldown recovers?

### Performance Targets
- Target platform: PC (high-end)
- Material complexity limits: TBD
- Particle count limits: TBD
- Widget count on screen: ~20 max (4 hub + 4 schools + 6 spells + misc)

---

## Asset Requirements

### Textures Needed
- Crack texture (Destruction)
- Crack texture organic (Restoration)
- Flame texture (Fire infusion)
- Water ripple texture (Water infusion)
- Lightning arc texture (Lightning infusion)
- Stone texture (Earth infusion)
- Wind wisp texture (Wind infusion)
- Light ray texture (Light infusion)
- Shadow tendril texture (Darkness infusion)
- Distortion map (Void infusion)
- Sacred geometry texture (Reality infusion)

### Particle Systems Needed
- Sharp shards (Destruction)
- Rising sparkles (Enhancement)
- Healing wisps (Restoration)
- Coalescing orbs (Conjuration)
- Fire embers (Fire infusion)
- Water droplets (Water infusion)
- Electric sparks (Lightning infusion)
- Dust/pebbles (Earth infusion)
- Air currents (Wind infusion)
- Light motes (Light infusion)
- Dark smoke (Darkness infusion)
- Geometric fragments (Void infusion)
- Geometric shapes (Reality infusion)

### Font Requirements
- Header font: Bebas Neue or similar (bold, all caps)
- Body font: Inter or Noto Sans (clean, readable)
- Number font: Exo 2 or Michroma (sci-fi feel)

---

## Success Criteria

### Visual Clarity
- [ ] Can distinguish button types at a glance
- [ ] Energy charged/depleted states are obvious
- [ ] Element colors are clearly identifiable
- [ ] Infusion effects are unmistakable
- [ ] Spell type themes are distinct

### Thematic Consistency
- [ ] "Crystalline energy" metaphor is clear
- [ ] Light refraction theme is present
- [ ] Anime aesthetic is maintained
- [ ] All elements feel cohesive

### Functional Requirements
- [ ] All 4 center buttons work
- [ ] School/ability switching works based on weapon
- [ ] Spell type materials apply correctly
- [ ] Infusion overlays activate/deactivate properly
- [ ] Energy saturation changes on interaction

### Performance
- [ ] Maintains 60 FPS during combat
- [ ] Material complexity is reasonable
- [ ] Particle counts are optimized
- [ ] Widget updates don't cause hitches

### User Experience
- [ ] Responsive (no input lag)
- [ ] Intuitive (minimal learning curve)
- [ ] Satisfying (animations feel good)
- [ ] Clear (never confused about state)
- [ ] Accessible (readable for most users)

---

**End of Combat UI Design Document**

*Ready for Phase 1 Implementation: Base Crystal System*
