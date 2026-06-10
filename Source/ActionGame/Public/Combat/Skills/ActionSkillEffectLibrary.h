#pragma once

#include "CoreMinimal.h"
#include "Combat/Skills/ActionSkillTypes.h"
#include "CollisionQueryParams.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ActionSkillEffectLibrary.generated.h"

class AActionCharacterBase;
class UActionSkillComponent;
class UActionSkillNode;
class UActionSkillObject;
struct FCollisionShape;

/**
 * 技能效果执行库。
 *
 * SkillNode 只负责把动画事件分派到 EffectId；真正的目标筛选、伤害结算、
 * VFX 播放和噪音上报都集中在这里，避免 AnimNotify 或 SkillNode 变成业务杂烩。
 */
UCLASS()
class ACTIONGAME_API UActionSkillEffectLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static void ExecuteEffect(
		UObject* WorldContext,
		UActionSkillComponent* SkillComponent,
		UActionSkillObject* SkillObject,
		UActionSkillNode* SkillNode,
		FName EffectId,
		const FActionSkillEffectRow& EffectRow);

private:
	static void ExecuteDamage(
		UObject* WorldContext,
		UActionSkillComponent* SkillComponent,
		UActionSkillObject* SkillObject,
		UActionSkillNode* SkillNode,
		FName EffectId,
		const FActionSkillEffectRow& EffectRow);

	static void ExecutePlayVFX(
		UActionSkillComponent* SkillComponent,
		UActionSkillObject* SkillObject,
		const FActionSkillEffectRow& EffectRow);

	static void ExecuteStopVFX(
		UActionSkillComponent* SkillComponent,
		UActionSkillObject* SkillObject,
		const FActionSkillEffectRow& EffectRow);

	static void ExecuteReportNoise(
		UActionSkillComponent* SkillComponent,
		const FActionSkillEffectRow& EffectRow);

	static FHitContext BuildHitContext(AActionCharacterBase* SourceCharacter, const FActionSkillEffectRow& EffectRow);
	static FVector GetEffectCenter(AActionCharacterBase* SourceCharacter, const FActionSkillEffectRow& EffectRow);
	static FRotator GetEffectRotation(AActionCharacterBase* SourceCharacter);

	static int32 ExecuteSphereDamage(
		UObject* WorldContext,
		AActionCharacterBase* SourceCharacter,
		UActionSkillObject* SkillObject,
		const FActionSkillEffectRow& EffectRow,
		const FHitContext& HitContextTemplate,
		TSet<TWeakObjectPtr<AActor>>& InOutHitActors);

	static int32 ExecuteOverlapShapeDamage(
		UWorld* World,
		AActionCharacterBase* SourceCharacter,
		UActionSkillObject* SkillObject,
		FName EffectId,
		const FActionSkillEffectRow& EffectRow,
		const FCollisionShape& Shape,
		const FVector& Center,
		const FQuat& Rotation,
		const FHitContext& HitContextTemplate,
		TSet<TWeakObjectPtr<AActor>>& InOutHitActors);

	static int32 ExecuteSweepBoxDamage(
		UWorld* World,
		AActionCharacterBase* SourceCharacter,
		UActionSkillObject* SkillObject,
		UActionSkillNode* SkillNode,
		FName EffectId,
		const FActionSkillEffectRow& EffectRow,
		const FHitContext& HitContextTemplate,
		TSet<TWeakObjectPtr<AActor>>& InOutHitActors);

	static int32 ExecuteConeDamage(
		UWorld* World,
		AActionCharacterBase* SourceCharacter,
		UActionSkillObject* SkillObject,
		FName EffectId,
		const FActionSkillEffectRow& EffectRow,
		const FHitContext& HitContextTemplate,
		TSet<TWeakObjectPtr<AActor>>& InOutHitActors);

	static int32 DispatchHitActors(
		AActionCharacterBase* SourceCharacter,
		UActionSkillObject* SkillObject,
		FName EffectId,
		const FActionSkillEffectRow& EffectRow,
		const FHitContext& HitContextTemplate,
		const TArray<AActor*>& CandidateActors,
		TSet<TWeakObjectPtr<AActor>>& InOutHitActors);

	static FCollisionObjectQueryParams MakePawnObjectQueryParams();
	static FCollisionQueryParams MakeSkillCollisionQueryParams(AActor* SourceActor);
};
