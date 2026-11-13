# B-424: builds the light hit's assets in /Game/Data/Skills/Weapons/SwordAndShield/LightHit and hooks it up as the
# starting sword-and-shield's attack. Run headless with the editor closed (it saves packages):
#   UnrealEditor-Cmd <uproject> -run=pythonscript -script="Scripts/editor/build_light_hit.py" -unattended -nullrhi
# Vendor folders are read only: the clips are copied into our folder with root motion on; nothing else is saved.
import unreal

FOLDER = "/Game/Data/Skills/Weapons/SwordAndShield/LightHit"
ROOT_MOTION_COPIES = {
    "AS_SAS_LightHit_Dash": "/Game/CombatMasterAnimBundle/Animations/DSAS_V3/Manny_UE5/RootMotion/Dodge/Anim_DSS3_Dodge_F_RM",
    "AS_SAS_LightHit_HopBack": "/Game/CombatMasterAnimBundle/Animations/HandsomeSwordAndShield/SwordAnimations/Evade/Anim_HSAS_Evade",
}
APPROACH_MONTAGE = FOLDER + "/AM_SAS_LightHit"
RETURN_MONTAGE = FOLDER + "/AM_SAS_LightHit_Return"
SKILL = FOLDER + "/DA_SAS_Attack_LightHit"
SKILL_TEMPLATE = "/Game/Data/Skills/Weapons/SwordAndShield/Attack/Attack1/DA_SAS_Attack_F"
STARTING_WEAPON = "/Game/Data/Characters/Data/Generic/Inventory/Loadouts/Loadout0/Weapons/DA_Equipment_Generic_SAS"

EAL = unreal.EditorAssetLibrary


def log(msg):
    unreal.log("[LightHitBuild] " + msg)


def save(path):
    if not EAL.save_asset(path, only_if_is_dirty=False):
        raise RuntimeError("could not save " + path)
    log("saved " + path)


for name, source in ROOT_MOTION_COPIES.items():
    path = FOLDER + "/" + name
    if EAL.does_asset_exist(path):
        continue
    clip = EAL.duplicate_asset(source, path)
    clip.set_editor_property("enable_root_motion", True)
    save(path)

if not EAL.does_asset_exist(APPROACH_MONTAGE):
    unreal.SystemLibrary.execute_console_command(None, "wor.Editor.BuildLightHitMontages")
    save(APPROACH_MONTAGE)
    save(RETURN_MONTAGE)

if not EAL.does_asset_exist(SKILL):
    skill = EAL.duplicate_asset(SKILL_TEMPLATE, SKILL)
    skill.set_editor_property("name", "Light Hit")
    skill.set_editor_property("skill_montage", EAL.load_asset(APPROACH_MONTAGE))
    skill.set_editor_property("return_montage", EAL.load_asset(RETURN_MONTAGE))
    save(SKILL)

weapon = EAL.load_asset(STARTING_WEAPON)
if weapon.get_editor_property("weapon_attack") != EAL.load_asset(SKILL):
    weapon.set_editor_property("weapon_attack", EAL.load_asset(SKILL))
    save(STARTING_WEAPON)
log("done")
