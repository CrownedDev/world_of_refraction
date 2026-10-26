"""B-100 smoke test: one draft that touches every kit kind the combat HUD drafts will use (background, frame, button,
badge, icon, divider, text, annotation). Writes docs/Design/screen_mockups/tool/test_draft/v1/.

    python Tools/ui_drafts/test_draft.py
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import kit  # noqa: E402

SCREEN, VERSION, NAME = "tool/test_draft", "v1", "test_draft"
ELEMENTS = ("fire", "water", "earth", "wind", "lightning", "ice", "light", "darkness", "void")


def draw():
    canvas = kit.background("dfhb_battle")
    kit.vignette(canvas, 0.6)
    kit.motes(canvas, 40, seed=100, area=(0, 300, kit.W, kit.H))

    header = kit.piece("frames", "header_bar")
    canvas.alpha_composite(header, (0, 0))
    kit.text(canvas, (kit.W // 2, 40), "MOCKUP TOOL CHECK", kit.font(kit.DISPLAY_BOLD, kit.SIZE_TITLE), tracking=2)

    panel_xy = (440, 150)
    panel = kit.piece("frames", "panel_plain")
    canvas.alpha_composite(panel, panel_xy)
    rule = kit.scaled(kit.piece("dividers", "title_rule"), width=340)
    canvas.alpha_composite(rule, (panel_xy[0] + 30, panel_xy[1] + 54))
    kit.text(canvas, (640, panel_xy[1] + 36), "Kit pieces", kit.font(kit.DISPLAY, 24))

    badge_y = panel_xy[1] + 110
    badge_h = 34
    shown = [e for e in ELEMENTS if (kit.KIT / "badges" / f"element_{e}.png").exists()]
    step = 340 // max(1, len(shown))
    for i, element in enumerate(shown):
        badge = kit.scaled(kit.piece("badges", f"element_{element}"), height=badge_h)
        canvas.alpha_composite(badge, (panel_xy[0] + 30 + i * step + (step - badge.width) // 2, badge_y))

    guard = kit.scaled(kit.piece("icons", "action_guard"), height=48)
    canvas.alpha_composite(guard, (640 - guard.width // 2, panel_xy[1] + 160))

    button = kit.scaled(kit.piece("buttons", "purple_long_normal"), width=300)
    button_xy = (640 - button.width // 2, panel_xy[1] + 226)
    canvas.alpha_composite(button, button_xy)
    kit.text(canvas, (640, button_xy[1] + button.height // 2), "Defend", kit.font(kit.UI_BOLD, kit.SIZE_UI))

    notes = [
        (1, (60, 40), "Header bar", ["frames/header_bar, full width, title in Spectral SC"]),
        (2, (panel_xy[0] - 10, panel_xy[1] + 10), "Panel", ["frames/panel_plain at native size, title_rule divider"]),
        (3, (panel_xy[0] + 10, badge_y + badge_h // 2), "Element badges", [f"badges/element_* ({len(shown)} found)"]),
        (4, (640 + 50, panel_xy[1] + 184), "Icon", ["icons/action_guard (Defend)"]),
        (5, (button_xy[0] - 16, button_xy[1] + 30), "Button", ["buttons/purple_long_normal, Lato Bold label"]),
        (6, (80, 660), "Background", ["backgrounds/dfhb_battle, vignette + seeded motes"]),
    ]
    return canvas, notes


if __name__ == "__main__":
    plain, notes = draw()
    kit.save_draft(plain, notes, SCREEN, VERSION, NAME)
