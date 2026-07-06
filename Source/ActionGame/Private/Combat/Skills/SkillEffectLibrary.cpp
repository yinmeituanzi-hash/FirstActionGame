#include "Combat/Skills/SkillEffectLibrary.h"

#include "AI/Noise/AINoiseSubsystem.h"
#include "Char/ActionCharacterBase.h"
#include "CollisionShape.h"
#include "Combat/Attributes/AttributeComponent.h"
#include "Combat/ActionCombatLibrary.h"
#include "Combat/Skills/ActionSkillComponent.h"
#include "Combat/Skills/ActionSkillNode.h"
#include "Combat/Skills/ActionSkillObject.h"
#include "Combat/Skills/SkillCreatureSubsystem.h"
#include "Combat/Skills/SkillCreatureTypes.h"
#include "Combat/VFX/ActionVFXComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogSkillEffect, Log, All);

namespace
{
	constexpr float SkillEffectDebugDuration = 1.0f;

	void AddUniqueActor(TArray<AActor*>& OutActors, AActor* Actor)
	{
		if (Actor != nullptr)
		{
			OutActors.AddUnique(Actor);
		}
	}
}

void USkillEffectLibrary::ExecuteEffect(
	UObject* WorldContext,
	UActionSkillComponent* SkillComponent,
	UActionSkillObject* SkillObject,
	UActionSkillNode* SkillNode,
	FName EffectId,
	const FSkillEffectRow& EffectRow,
	FSkillEffectContext& Context)
{
	if (SkillComponent == nullptr || SkillObject == nullptr)
	{
		return;
	}

	switch (EffectRow.EffectType)
	{
	case ESkillEffectType::Damage:
		ExecuteDamage(WorldContext, SkillComponent, SkillObject, SkillNode, EffectId, EffectRow, Context);
		break;
	case ESkillEffectType::PlayVFX:
		ExecutePlayVFX(SkillComponent, SkillObject, EffectRow);
		break;
	case ESkillEffectType::StopVFX:
		ExecuteStopVFX(SkillComponent, SkillObject, EffectRow);
		break;
	case ESkillEffectType::ReportNoise:
		ExecuteReportNoise(SkillComponent, EffectRow);
		break;
	case ESkillEffectType::RestoreSP:
		ExecuteRestoreSP(SkillComponent, EffectRow, Context);
		break;
	case ESkillEffectType::SpawnCreature:
		ExecuteSpawnCreature(WorldContext, SkillComponent, SkillObject, EffectRow);
		break;
	case ESkillEffectType::ApplyTagForDuration:
	case ESkillEffectType::Stun:
	default:
		UE_LOG(
			LogSkillEffect,
			Verbose,
			TEXT("SkillEffect: EffectType not implemented yet. EffectId=%s Type=%d"),
			*EffectId.ToString(),
			static_cast<uint8>(EffectRow.EffectType));
		break;
	}
}

void USkillEffectLibrary::ExecuteDamage(
	UObject* WorldContext,
	UActionSkillComponent* SkillComponent,
	UActionSkillObject* SkillObject,
	UActionSkillNode* SkillNode,
	FName EffectId,
	const FSkillEffectRow& EffectRow,
	FSkillEffectContext& Context)
{
	AActionCharacterBase* SourceCharacter = SkillComponent != nullptr ? SkillComponent->GetOwnerCharacter() : nullptr;
	if (WorldContext == nullptr || SourceCharacter == nullptr || SkillObject == nullptr || SourceCharacter->IsDead())
	{
		return;
	}

	UWorld* World = WorldContext->GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const FHitContext HitContextTemplate = BuildHitContext(SourceCharacter, EffectRow);
	const FVector Center = GetEffectCenter(SourceCharacter, EffectRow);
	const FRotator Rotation = GetEffectRotation(SourceCharacter);
	TSet<TWeakObjectPtr<AActor>> HitActorsThisExecution;
	TSet<TWeakObjectPtr<AActor>>& HitActors = EffectRow.bDeduplicateHitsWithinNode
		? SkillObject->GetMutableHitActorsThisNode()
		: HitActorsThisExecution;
	int32 HitCount = 0;

	switch (EffectRow.TargetFilter)
	{
	case ESkillTargetFilter::Sphere:
		HitCount = ExecuteSphereDamage(WorldContext, SourceCharacter, SkillObject, EffectRow, HitContextTemplate, HitActors);
		break;
	case ESkillTargetFilter::Cone:
		HitCount = ExecuteConeDamage(World, SourceCharacter, SkillObject, EffectId, EffectRow, HitContextTemplate, HitActors);
		break;
	case ESkillTargetFilter::Capsule:
		HitCount = ExecuteOverlapShapeDamage(
			World,
			SourceCharacter,
			SkillObject,
			EffectId,
			EffectRow,
			FCollisionShape::MakeCapsule(EffectRow.Radius, EffectRow.CapsuleHalfHeight),
			Center,
			Rotation.Quaternion(),
			HitContextTemplate,
			HitActors);
		break;
	case ESkillTargetFilter::Box:
		HitCount = ExecuteOverlapShapeDamage(
			World,
			SourceCharacter,
			SkillObject,
			EffectId,
			EffectRow,
			FCollisionShape::MakeBox(EffectRow.BoxExtent),
			Center,
			Rotation.Quaternion(),
			HitContextTemplate,
			HitActors);
		break;
	case ESkillTargetFilter::SweepBox:
		HitCount = ExecuteSweepBoxDamage(World, SourceCharacter, SkillObject, SkillNode, EffectId, EffectRow, HitContextTemplate, HitActors);
		break;
	case ESkillTargetFilter::TargetActor:
	{
		TArray<AActor*> Candidates;
		AddUniqueActor(Candidates, SkillObject->GetCurrentTarget());
		HitCount = DispatchHitActors(SourceCharacter, SkillObject, EffectId, EffectRow, HitContextTemplate, Candidates, HitActors);
		break;
	}
	case ESkillTargetFilter::Self:
	{
		UE_LOG(
			LogSkillEffect,
			Verbose,
			TEXT("SkillEffect: Damage Self target is skipped for now. Effect=%s"),
			*EffectId.ToString());
		break;
	}
	default:
		break;
	}

	UE_LOG(
		LogSkillEffect,
		Verbose,
		TEXT("SkillEffect: Damage finished. Skill=%s Effect=%s HitCount=%d"),
		*SkillObject->GetSkillId().ToString(),
		*EffectId.ToString(),
		HitCount);

	Context.TotalHitCount += HitCount;
	for (const TWeakObjectPtr<AActor>& WeakActor : HitActors)
	{
		if (WeakActor.IsValid())
		{
			Context.HitTargets.AddUnique(WeakActor);
		}
	}
}

void USkillEffectLibrary::ExecutePlayVFX(
	UActionSkillComponent* SkillComponent,
	UActionSkillObject* SkillObject,
	const FSkillEffectRow& EffectRow)
{
	AActionCharacterBase* SourceCharacter = SkillComponent != nullptr ? SkillComponent->GetOwnerCharacter() : nullptr;
	if (SourceCharacter == nullptr || EffectRow.VFXId.IsNone())
	{
		return;
	}

	UActionVFXComponent* VFXComponent = SourceCharacter->GetActionVFXComponent();
	if (VFXComponent == nullptr)
	{
		return;
	}

	FActionVFXContext Context;
	Context.SourceActor = SourceCharacter;
	Context.TargetActor = SkillObject != nullptr ? SkillObject->GetCurrentTarget() : nullptr;
	Context.SkillId = SkillObject != nullptr ? SkillObject->GetSkillId() : NAME_None;
	Context.WorldLocation = GetEffectCenter(SourceCharacter, EffectRow);
	Context.WorldRotation = GetEffectRotation(SourceCharacter);
	Context.HitLocation = Context.WorldLocation;

	VFXComponent->PlayVFX(EffectRow.VFXId, Context);
}

void USkillEffectLibrary::ExecuteStopVFX(
	UActionSkillComponent* SkillComponent,
	UActionSkillObject* SkillObject,
	const FSkillEffectRow& EffectRow)
{
	AActionCharacterBase* SourceCharacter = SkillComponent != nullptr ? SkillComponent->GetOwnerCharacter() : nullptr;
	if (SourceCharacter == nullptr)
	{
		return;
	}

	UActionVFXComponent* VFXComponent = SourceCharacter->GetActionVFXComponent();
	if (VFXComponent == nullptr)
	{
		return;
	}

	if (!EffectRow.VFXStopGroup.IsNone())
	{
		VFXComponent->StopVFXByGroup(EffectRow.VFXStopGroup, false);
		return;
	}

	if (SkillObject != nullptr)
	{
		// 未指定 Group 时，按技能生命周期清理，避免持续特效残留在角色身上。
		VFXComponent->StopSkillLifetimeVFX(SkillObject->GetSkillId());
	}
}

void USkillEffectLibrary::ExecuteReportNoise(
	UActionSkillComponent* SkillComponent,
	const FSkillEffectRow& EffectRow)
{
	AActionCharacterBase* SourceCharacter = SkillComponent != nullptr ? SkillComponent->GetOwnerCharacter() : nullptr;
	if (SourceCharacter == nullptr)
	{
		return;
	}

	if (UAINoiseSubsystem* NoiseSubsystem = UAINoiseSubsystem::Get(SourceCharacter))
	{
		NoiseSubsystem->ReportNoise(
			GetEffectCenter(SourceCharacter, EffectRow),
			EffectRow.NoiseLoudness,
			SourceCharacter,
			EffectRow.NoiseCategory);
	}
}

void USkillEffectLibrary::ExecuteRestoreSP(
	UActionSkillComponent* SkillComponent,
	const FSkillEffectRow& EffectRow,
	const FSkillEffectContext& Context)
{
	if (EffectRow.SPRestoreAmount <= 0)
	{
		return;
	}

	if (Context.TotalHitCount <= 0)
	{
		return;
	}

	AActionCharacterBase* SourceCharacter = SkillComponent != nullptr ? SkillComponent->GetOwnerCharacter() : nullptr;
	if (SourceCharacter == nullptr)
	{
		return;
	}

	UAttributeComponent* AttrComp = SourceCharacter->FindComponentByClass<UAttributeComponent>();
	if (AttrComp == nullptr)
	{
		return;
	}

	const float RestoreTotal = static_cast<float>(EffectRow.SPRestoreAmount) * Context.TotalHitCount;
	const float OldSP = AttrComp->GetAttribute(EAttributeType::SP);
	AttrComp->ModifyAttribute(EAttributeType::SP, RestoreTotal);
	const float NewSP = AttrComp->GetAttribute(EAttributeType::SP);

	UE_LOG(
		LogSkillEffect,
		Verbose,
		TEXT("SkillEffect: RestoreSP. AmountPerHit=%d HitCount=%d TotalRestore=%.0f OldSP=%.0f NewSP=%.0f"),
		EffectRow.SPRestoreAmount,
		Context.TotalHitCount,
		RestoreTotal,
		OldSP,
		NewSP);
}

void USkillEffectLibrary::ExecuteSpawnCreature(
	UObject* WorldContext,
	UActionSkillComponent* SkillComponent,
	UActionSkillObject* SkillObject,
	const FSkillEffectRow& EffectRow)
{
	if (SkillComponent == nullptr || EffectRow.SpawnCreatureId.IsNone())
	{
		return;
	}

	AActionCharacterBase* SourceCharacter = Cast<AActionCharacterBase>(SkillComponent->GetOwner());
	if (SourceCharacter == nullptr)
	{
		return;
	}

	UWorld* World = SourceCharacter->GetWorld();
	if (World == nullptr && WorldContext != nullptr)
	{
		World = WorldContext->GetWorld();
	}
	if (World == nullptr)
	{
		return;
	}

	USkillCreatureSubsystem* CreatureSubsystem = World->GetSubsystem<USkillCreatureSubsystem>();
	if (CreatureSubsystem == nullptr)
	{
		UE_LOG(
			LogSkillEffect,
			Warning,
			TEXT("SkillEffect: SpawnCreature but SkillCreatureSubsystem missing. EffectRow.SpawnCreatureId=%s"),
			*EffectRow.SpawnCreatureId.ToString());
		return;
	}

	// Day8.1 简化：DataTable 由 SkillComponent 一次性注入到 Subsystem。
	// 未来 SkillComponent::InitializeComponent 会主动注入，这里只做惰性兜底。
	if (!CreatureSubsystem->HasSkillCreatureDataTable())
	{
		if (UDataTable* CreatureDT = SkillComponent->GetSkillCreatureDataTable())
		{
			CreatureSubsystem->SetSkillCreatureDataTable(CreatureDT);
		}
		else
		{
			UE_LOG(
				LogSkillEffect,
				Warning,
				TEXT("SkillEffect: SkillCreatureDataTable missing on SkillComponent. Skip spawn. Id=%s"),
				*EffectRow.SpawnCreatureId.ToString());
			return;
		}
	}

	FSkillCreatureSpawnRequest Request;
	Request.CreatureId = EffectRow.SpawnCreatureId;
	Request.SourceSkillId = SkillComponent->GetCurrentSkillId();
	Request.SourceCharacter = SourceCharacter;
	Request.SourceSkillComponent = SkillComponent;
	if (SkillObject != nullptr)
	{
		Request.Target = SkillObject->GetCurrentTarget();
	}
	Request.SpawnMode = EffectRow.SpawnMode;
	Request.DirectionMode = EffectRow.DirectionMode;
	Request.SpawnSocket = EffectRow.SpawnSocket;
	Request.SpawnOffset = EffectRow.SpawnOffset;
	Request.SpawnCount = EffectRow.SpawnCount;
	Request.SpreadAngleDeg = EffectRow.SpawnSpreadAngleDeg;

	CreatureSubsystem->SpawnSkillCreature(Request);
}

FHitContext USkillEffectLibrary::BuildHitContext(AActionCharacterBase* SourceCharacter, const FSkillEffectRow& EffectRow)
{
	FHitContext Context;
	Context.Attacker = SourceCharacter;
	Context.ReactType = EffectRow.ReactType;
	Context.DamageAmount = SourceCharacter != nullptr ? SourceCharacter->GetAttackPower() * EffectRow.DamageScale : 0.0f;
	Context.FeedbackScale = EffectRow.FeedbackScale;
	Context.HitFlyXYStrength = EffectRow.HitFlyXY;
	Context.HitFlyZStrength = EffectRow.HitFlyZ;
	Context.bUseRagdoll = EffectRow.bUseRagdoll;
	return Context;
}

FVector USkillEffectLibrary::GetEffectCenter(AActionCharacterBase* SourceCharacter, const FSkillEffectRow& EffectRow)
{
	if (SourceCharacter == nullptr)
	{
		return FVector::ZeroVector;
	}
	return SourceCharacter->GetActorLocation() + SourceCharacter->GetActorForwardVector() * EffectRow.ForwardOffset;
}

FRotator USkillEffectLibrary::GetEffectRotation(AActionCharacterBase* SourceCharacter)
{
	return SourceCharacter != nullptr ? SourceCharacter->GetActorRotation() : FRotator::ZeroRotator;
}

int32 USkillEffectLibrary::ExecuteSphereDamage(
	UObject* WorldContext,
	AActionCharacterBase* SourceCharacter,
	UActionSkillObject* SkillObject,
	const FSkillEffectRow& EffectRow,
	const FHitContext& HitContextTemplate,
	TSet<TWeakObjectPtr<AActor>>& InOutHitActors)
{
	if (SourceCharacter == nullptr || SkillObject == nullptr)
	{
		return 0;
	}

	FSphereAttackHitParams Params;
	Params.Attacker = SourceCharacter;
	Params.Center = GetEffectCenter(SourceCharacter, EffectRow);
	Params.Radius = EffectRow.Radius;
	Params.TargetClassFilter = AActionCharacterBase::StaticClass();
	Params.HitContextTemplate = HitContextTemplate;
	Params.HitLocationBackstep = EffectRow.HitLocationBackstep;
	Params.bDrawDebugSphere = EffectRow.bDrawDebug;
	Params.DebugDrawDuration = SkillEffectDebugDuration;

	return UActionCombatLibrary::PerformSphereAttackHit(
		WorldContext,
		Params,
		InOutHitActors);
}

int32 USkillEffectLibrary::ExecuteOverlapShapeDamage(
	UWorld* World,
	AActionCharacterBase* SourceCharacter,
	UActionSkillObject* SkillObject,
	FName EffectId,
	const FSkillEffectRow& EffectRow,
	const FCollisionShape& Shape,
	const FVector& Center,
	const FQuat& Rotation,
	const FHitContext& HitContextTemplate,
	TSet<TWeakObjectPtr<AActor>>& InOutHitActors)
{
	if (World == nullptr || SourceCharacter == nullptr || SkillObject == nullptr)
	{
		return 0;
	}

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		Center,
		Rotation,
		MakePawnObjectQueryParams(),
		Shape,
		MakeSkillCollisionQueryParams(SourceCharacter));

	TArray<AActor*> Candidates;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AddUniqueActor(Candidates, Overlap.GetActor());
	}

	if (EffectRow.bDrawDebug)
	{
		if (EffectRow.TargetFilter == ESkillTargetFilter::Capsule)
		{
			DrawDebugCapsule(World, Center, EffectRow.CapsuleHalfHeight, EffectRow.Radius, Rotation, FColor::Red, false, SkillEffectDebugDuration);
		}
		else
		{
			DrawDebugBox(World, Center, EffectRow.BoxExtent, Rotation, FColor::Red, false, SkillEffectDebugDuration);
		}
	}

	return DispatchHitActors(SourceCharacter, SkillObject, EffectId, EffectRow, HitContextTemplate, Candidates, InOutHitActors);
}

int32 USkillEffectLibrary::ExecuteSweepBoxDamage(
	UWorld* World,
	AActionCharacterBase* SourceCharacter,
	UActionSkillObject* SkillObject,
	UActionSkillNode* SkillNode,
	FName EffectId,
	const FSkillEffectRow& EffectRow,
	const FHitContext& HitContextTemplate,
	TSet<TWeakObjectPtr<AActor>>& InOutHitActors)
{
	if (World == nullptr || SourceCharacter == nullptr || SkillObject == nullptr)
	{
		return 0;
	}

	const FVector CurrentCenter = GetEffectCenter(SourceCharacter, EffectRow);
	const FQuat Rotation = GetEffectRotation(SourceCharacter).Quaternion();
	FVector StartCenter = CurrentCenter;
	if (SkillNode != nullptr)
	{
		StartCenter = SkillNode->GetActivationLocation() + SkillNode->GetActivationRotation().Vector() * EffectRow.ForwardOffset;
	}

	const FCollisionShape Shape = FCollisionShape::MakeBox(EffectRow.BoxExtent);
	TArray<AActor*> Candidates;

	if (StartCenter.Equals(CurrentCenter, KINDA_SMALL_NUMBER))
	{
		return ExecuteOverlapShapeDamage(
			World,
			SourceCharacter,
			SkillObject,
			EffectId,
			EffectRow,
			Shape,
			CurrentCenter,
			Rotation,
			HitContextTemplate,
			InOutHitActors);
	}

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(
		Hits,
		StartCenter,
		CurrentCenter,
		Rotation,
		MakePawnObjectQueryParams(),
		Shape,
		MakeSkillCollisionQueryParams(SourceCharacter));

	for (const FHitResult& Hit : Hits)
	{
		AddUniqueActor(Candidates, Hit.GetActor());
	}

	if (EffectRow.bDrawDebug)
	{
		DrawDebugLine(World, StartCenter, CurrentCenter, FColor::Red, false, SkillEffectDebugDuration, 0, 2.0f);
		DrawDebugBox(World, StartCenter, EffectRow.BoxExtent, Rotation, FColor::Orange, false, SkillEffectDebugDuration);
		DrawDebugBox(World, CurrentCenter, EffectRow.BoxExtent, Rotation, FColor::Red, false, SkillEffectDebugDuration);
	}

	return DispatchHitActors(SourceCharacter, SkillObject, EffectId, EffectRow, HitContextTemplate, Candidates, InOutHitActors);
}

int32 USkillEffectLibrary::ExecuteConeDamage(
	UWorld* World,
	AActionCharacterBase* SourceCharacter,
	UActionSkillObject* SkillObject,
	FName EffectId,
	const FSkillEffectRow& EffectRow,
	const FHitContext& HitContextTemplate,
	TSet<TWeakObjectPtr<AActor>>& InOutHitActors)
{
	if (World == nullptr || SourceCharacter == nullptr || SkillObject == nullptr)
	{
		return 0;
	}

	const FVector Origin = GetEffectCenter(SourceCharacter, EffectRow);
	const FVector Forward = SourceCharacter->GetActorForwardVector().GetSafeNormal2D();
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(EffectRow.ConeHalfAngle));

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		Origin,
		FQuat::Identity,
		MakePawnObjectQueryParams(),
		FCollisionShape::MakeSphere(EffectRow.Radius),
		MakeSkillCollisionQueryParams(SourceCharacter));

	TArray<AActor*> Candidates;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (Actor == nullptr)
		{
			continue;
		}

		const FVector ToActor = (Actor->GetActorLocation() - Origin).GetSafeNormal2D();
		if (!ToActor.IsNearlyZero() && FVector::DotProduct(Forward, ToActor) >= CosHalfAngle)
		{
			AddUniqueActor(Candidates, Actor);
		}
	}

	if (EffectRow.bDrawDebug)
	{
		const float ConeAngleRadians = FMath::DegreesToRadians(EffectRow.ConeHalfAngle);
		DrawDebugCone(World, Origin, Forward, EffectRow.Radius, ConeAngleRadians, ConeAngleRadians, 24, FColor::Red, false, SkillEffectDebugDuration);
	}

	return DispatchHitActors(SourceCharacter, SkillObject, EffectId, EffectRow, HitContextTemplate, Candidates, InOutHitActors);
}

int32 USkillEffectLibrary::DispatchHitActors(
	AActionCharacterBase* SourceCharacter,
	UActionSkillObject* SkillObject,
	FName EffectId,
	const FSkillEffectRow& EffectRow,
	const FHitContext& HitContextTemplate,
	const TArray<AActor*>& CandidateActors,
	TSet<TWeakObjectPtr<AActor>>& InOutHitActors)
{
	if (SourceCharacter == nullptr || SkillObject == nullptr)
	{
		return 0;
	}

	int32 HitCount = 0;

	for (AActor* Actor : CandidateActors)
	{
		if (Actor == nullptr || Actor == SourceCharacter)
		{
			continue;
		}

		AActionCharacterBase* Victim = Cast<AActionCharacterBase>(Actor);
		if (Victim == nullptr || Victim->IsDead() || !SourceCharacter->CanDamageTarget(Victim))
		{
			continue;
		}

		const TWeakObjectPtr<AActor> WeakVictim(Victim);
		if (InOutHitActors.Contains(WeakVictim))
		{
			continue;
		}
		InOutHitActors.Add(WeakVictim);

		FHitContext Context = HitContextTemplate;
		Context.Attacker = SourceCharacter;

		FVector ToVictim = (Victim->GetActorLocation() - SourceCharacter->GetActorLocation()).GetSafeNormal2D();
		if (ToVictim.IsNearlyZero())
		{
			ToVictim = SourceCharacter->GetActorForwardVector().GetSafeNormal2D();
		}
		Context.HitDirection = ToVictim;
		Context.HitLocation = Victim->GetActorLocation() - ToVictim * EffectRow.HitLocationBackstep;

		UE_LOG(
			LogSkillEffect,
			Log,
			TEXT("SkillEffect: Effect=%s Attacker=%s hit Victim=%s for %.1f damage."),
			*EffectId.ToString(),
			*GetNameSafe(SourceCharacter),
			*GetNameSafe(Victim),
			Context.DamageAmount);

		Victim->ReceiveHit(Context);
		++HitCount;
	}

	return HitCount;
}

FCollisionObjectQueryParams USkillEffectLibrary::MakePawnObjectQueryParams()
{
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	return ObjectParams;
}

FCollisionQueryParams USkillEffectLibrary::MakeSkillCollisionQueryParams(AActor* SourceActor)
{
	FCollisionQueryParams QueryParams(FName(TEXT("SkillEffect")), false);
	QueryParams.bReturnPhysicalMaterial = false;
	QueryParams.bFindInitialOverlaps = true;
	if (SourceActor != nullptr)
	{
		QueryParams.AddIgnoredActor(SourceActor);
	}
	return QueryParams;
}
