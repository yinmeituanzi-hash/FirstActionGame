#include "Combat/Skills/SkillCreatureLogic.h"

#include "Char/ActionCharacterBase.h"
#include "Combat/Skills/SkillCreatureTypes.h"
#include "GameFramework/Actor.h"

// -----------------------------------------------------------------------------
// Day8.1 只提供最小可用实现：
//   - SpawnTransform 支持 SelfOffset / WorldLocation（其他模式后续 milestone 打开）
//   - Direction 支持 SourceForward / WorldDirection / TowardsTarget
//   - Spread 按角度均匀分布，Count=1 时直接返回 BaseDirection
// 头文件里的 SourceSocket / WeaponSocket 分支现在只是回退到 SelfOffset，
// 留 TODO 供 Step 3 或 Day8.2 补齐（那时会引入 GetMesh() / 武器组件依赖）。
// -----------------------------------------------------------------------------

FTransform USkillCreatureLogic::ResolveSpawnTransform(
	const FSkillCreatureSpawnRequest& Request,
	const FSkillCreatureRow& Row)
{
	AActionCharacterBase* Source = Request.SourceCharacter.Get();

	FVector BaseLocation = FVector::ZeroVector;
	FRotator BaseRotation = FRotator::ZeroRotator;

	switch (Request.SpawnMode)
	{
	case ECreatureSpawnTargetMode::WorldLocation:
	{
		BaseLocation = Request.ExplicitWorldLocation;
		BaseRotation = FRotator::ZeroRotator;
		break;
	}
	case ECreatureSpawnTargetMode::TargetActor:
	{
		if (AActor* T = Request.Target.Get())
		{
			BaseLocation = T->GetActorLocation();
			BaseRotation = T->GetActorRotation();
			break;
		}
		// TargetActor 缺失时回退 SelfOffset
		[[fallthrough]];
	}
	case ECreatureSpawnTargetMode::SourceSocket:
	case ECreatureSpawnTargetMode::WeaponSocket:
		// TODO(Day8.1 Step 3 或 Day8.2)：
		//   - SourceSocket：Source->GetMesh()->GetSocketTransform(SocketName, ERelativeTransformSpace::RTS_World)
		//   - WeaponSocket：拿到当前武器 Mesh 后同样处理
		//   现阶段回退到 SelfOffset，功能可用但不精确。
		[[fallthrough]];
	case ECreatureSpawnTargetMode::SelfOffset:
	default:
	{
		if (Source != nullptr)
		{
			BaseLocation = Source->GetActorLocation();
			BaseRotation = Source->GetActorRotation();
		}
		break;
	}
	}

	// Row + Request 的 Local 偏移在 BaseRotation 空间下叠加
	const FVector CombinedLocalOffset = Row.BornLocationOffset + Request.SpawnOffset;
	const FVector WorldOffset = BaseRotation.RotateVector(CombinedLocalOffset);

	FTransform Result;
	Result.SetLocation(BaseLocation + WorldOffset);
	Result.SetRotation(BaseRotation.Quaternion());
	Result.SetScale3D(FVector::OneVector);
	return Result;
}

bool USkillCreatureLogic::ResolveTargetLocation(
	const FSkillCreatureSpawnRequest& Request,
	FVector& OutTargetLocation)
{
	if (AActor* T = Request.Target.Get())
	{
		OutTargetLocation = T->GetActorLocation();
		return true;
	}
	return false;
}

FVector USkillCreatureLogic::ResolveInitialVelocity(
	const FSkillCreatureSpawnRequest& Request,
	const FSkillCreatureRow& Row,
	const FTransform& SpawnTransform)
{
	FVector Direction = FVector::ForwardVector;

	switch (Request.DirectionMode)
	{
	case ECreatureDirectionMode::WorldDirection:
	{
		Direction = Request.ExplicitWorldDirection.GetSafeNormal();
		if (Direction.IsNearlyZero())
		{
			Direction = SpawnTransform.GetUnitAxis(EAxis::X);
		}
		break;
	}
	case ECreatureDirectionMode::TowardsTarget:
	{
		FVector TargetLoc;
		if (ResolveTargetLocation(Request, TargetLoc))
		{
			Direction = (TargetLoc - SpawnTransform.GetLocation()).GetSafeNormal();
			if (Direction.IsNearlyZero())
			{
				Direction = SpawnTransform.GetUnitAxis(EAxis::X);
			}
			break;
		}
		[[fallthrough]];
	}
	case ECreatureDirectionMode::SourceForward:
	default:
	{
		Direction = SpawnTransform.GetUnitAxis(EAxis::X);
		break;
	}
	}

	return Direction * FMath::Max(0.0f, Row.InitialSpeed);
}

FVector USkillCreatureLogic::GetSpreadDirection(
	const FVector& BaseDirection,
	int32 Index,
	int32 Count,
	float SpreadHalfAngleDeg)
{
	const FVector Base = BaseDirection.GetSafeNormal();
	if (Count <= 1 || SpreadHalfAngleDeg <= KINDA_SMALL_NUMBER || Base.IsNearlyZero())
	{
		return Base;
	}

	// 在 [-SpreadHalfAngleDeg, +SpreadHalfAngleDeg] 范围内均匀铺开
	const float T = (Count == 1) ? 0.5f : static_cast<float>(Index) / static_cast<float>(Count - 1);
	const float AngleDeg = FMath::Lerp(-SpreadHalfAngleDeg, SpreadHalfAngleDeg, T);
	const FQuat Rot(FVector::UpVector, FMath::DegreesToRadians(AngleDeg));
	return Rot.RotateVector(Base);
}
