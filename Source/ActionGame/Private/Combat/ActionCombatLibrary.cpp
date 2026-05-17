#include "Combat/ActionCombatLibrary.h"

#include "Char/ActionCharacterBase.h"
#include "DrawDebugHelpers.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(LogActionCombatLibrary, Log, All);

int32 UActionCombatLibrary::PerformSphereAttackHit(
	UObject* WorldContext,
	const FSphereAttackHitParams& Params,
	TSet<TWeakObjectPtr<AActor>>& InOutHitActors)
{
	if (WorldContext == nullptr)
	{
		return 0;
	}

	UWorld* World = WorldContext->GetWorld();
	if (World == nullptr)
	{
		return 0;
	}

	AActor* Attacker = Params.Attacker.Get();

	if (Params.bDrawDebugSphere)
	{
		DrawDebugSphere(
			World,
			Params.Center,
			Params.Radius,
			16,
			FColor::Red,
			false,
			Params.DebugDrawDuration);
	}

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	TArray<AActor*> Ignored;
	if (Attacker != nullptr)
	{
		Ignored.Add(Attacker);
	}

	// TargetClassFilter 为 null 时传 AActor::StaticClass()，等价于"所有 Pawn 都算"。
	UClass* FilterClass = Params.TargetClassFilter != nullptr
		? Params.TargetClassFilter.Get()
		: AActor::StaticClass();

	TArray<AActor*> Overlapped;
	UKismetSystemLibrary::SphereOverlapActors(
		WorldContext,
		Params.Center,
		Params.Radius,
		ObjectTypes,
		FilterClass,
		Ignored,
		Overlapped);

	int32 HitCount = 0;
	for (AActor* Actor : Overlapped)
	{
		if (Actor == nullptr || Actor == Attacker)
		{
			continue;
		}

		AActionCharacterBase* Victim = Cast<AActionCharacterBase>(Actor);
		if (Victim == nullptr || Victim->IsDead())
		{
			continue;
		}

		const TWeakObjectPtr<AActor> WeakActor(Actor);
		if (InOutHitActors.Contains(WeakActor))
		{
			continue;
		}
		InOutHitActors.Add(WeakActor);

		// 基于模板复制 HitContext，覆盖 Library 负责的三个字段。
		FHitContext Ctx = Params.HitContextTemplate;
		Ctx.Attacker = Attacker;

		const FVector VictimCenter = Victim->GetActorLocation();
		FVector ToVictim = (VictimCenter - (Attacker != nullptr ? Attacker->GetActorLocation() : Params.Center))
			.GetSafeNormal2D();
		if (ToVictim.IsNearlyZero())
		{
			ToVictim = Victim->GetActorForwardVector().GetSafeNormal2D() * -1.0f;
		}
		Ctx.HitDirection = ToVictim;
		Ctx.HitLocation = VictimCenter - ToVictim * Params.HitLocationBackstep;

		UE_LOG(
			LogActionCombatLibrary,
			Log,
			TEXT("PerformSphereAttackHit: Attacker=%s hit Victim=%s for %.1f damage."),
			*GetNameSafe(Attacker),
			*GetNameSafe(Victim),
			Ctx.DamageAmount);

		Victim->ReceiveHit(Ctx);
		++HitCount;
	}

	return HitCount;
}
