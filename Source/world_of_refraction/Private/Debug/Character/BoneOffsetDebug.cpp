// BoneOffsetDebug.cpp
// D-242: read where a bone sits against another in a skeletal mesh's reference pose (how a hero holds a baked-in weapon):
// wor.Debug.PrintBoneOffset <SkeletalMeshAssetPath> <Bone> [RelativeToBone]

#include "AnimationRuntime.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace BoneOffsetDebug
{
	/** False with the reason in OutLine when the mesh or a bone isn't found. */
	bool TryGetBoneOffsetString(const FString &MeshPath, const FName Bone, FName RelativeTo, FString &OutLine)
	{
		const USkeletalMesh *Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
		if (!Mesh)
		{
			OutLine = FString::Printf(TEXT("[BoneOffset] no skeletal mesh at %s"), *MeshPath);
			return false;
		}
		const FReferenceSkeleton &RefSkeleton = Mesh->GetRefSkeleton();
		const int32 BoneIndex = RefSkeleton.FindBoneIndex(Bone);
		if (BoneIndex == INDEX_NONE)
		{
			OutLine = FString::Printf(TEXT("[BoneOffset] %s has no bone %s"), *Mesh->GetName(), *Bone.ToString());
			return false;
		}
		const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
		const FName ParentName = ParentIndex == INDEX_NONE ? NAME_None : RefSkeleton.GetBoneName(ParentIndex);
		if (RelativeTo.IsNone())
		{
			RelativeTo = ParentName;
		}
		const int32 RelativeIndex = RelativeTo.IsNone() ? INDEX_NONE : RefSkeleton.FindBoneIndex(RelativeTo);
		if (!RelativeTo.IsNone() && RelativeIndex == INDEX_NONE)
		{
			OutLine = FString::Printf(TEXT("[BoneOffset] %s has no bone %s"), *Mesh->GetName(), *RelativeTo.ToString());
			return false;
		}
		// The root bone with no relative bone reads against the component.
		const FTransform BoneSpace = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, BoneIndex);
		const FTransform RelativeSpace = RelativeIndex == INDEX_NONE ? FTransform::Identity : FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, RelativeIndex);
		const FTransform Offset = BoneSpace.GetRelativeTransform(RelativeSpace);
		const FRotator Rotation = Offset.Rotator();
		const FVector Location = Offset.GetLocation();
		const FVector Scale = Offset.GetScale3D();
		OutLine = FString::Printf(TEXT("[BoneOffset] %s: %s (parent %s) relative to %s: location (%.3f, %.3f, %.3f), rotation pitch %.3f yaw %.3f roll %.3f, scale (%.3f, %.3f, %.3f)"),
							   *Mesh->GetName(), *Bone.ToString(), *ParentName.ToString(), RelativeIndex == INDEX_NONE ? TEXT("component") : *RelativeTo.ToString(),
							   Location.X, Location.Y, Location.Z, Rotation.Pitch, Rotation.Yaw, Rotation.Roll, Scale.X, Scale.Y, Scale.Z);
		return true;
	}

	static FAutoConsoleCommand GPrintBoneOffsetCommand(
		TEXT("wor.Debug.PrintBoneOffset"),
		TEXT("DEBUG (D-242): a bone's reference-pose offset: wor.Debug.PrintBoneOffset <SkeletalMeshAssetPath> <Bone> [RelativeToBone=its parent]. "
			 "Logs the bone's parent, and its location, rotation and scale relative to RelativeToBone."),
		FConsoleCommandWithArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args)
			{
				if (Args.Num() < 2)
				{
					UE_LOG(LogWoR, Warning, TEXT("[BoneOffset] usage: wor.Debug.PrintBoneOffset <SkeletalMeshAssetPath> <Bone> [RelativeToBone]"));
					return;
				}
				FString Line;
				if (TryGetBoneOffsetString(Args[0], FName(*Args[1]), Args.IsValidIndex(2) ? FName(*Args[2]) : NAME_None, Line))
				{
					UE_LOG(LogWoR, Display, TEXT("%s"), *Line);
				}
				else
				{
					UE_LOG(LogWoR, Warning, TEXT("%s"), *Line);
				}
			}));
}
