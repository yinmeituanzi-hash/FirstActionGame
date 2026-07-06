#pragma once

#include "CoreMinimal.h"
#include "Combat/Skills/ActionSkillTypes.h"
#include "CollisionQueryParams.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SkillEffectLibrary.generated.h"

class AActionCharacterBase;
class UActionSkillComponent;
class UActionSkillNode;
class UActionSkillObject;
struct FCollisionShape;

/**
 * 同一批 Effects 共享的执行上下文。
 *
 * 参照 010 的 EffectStruct.HitTargets 设计：同一次 Notify（或 Enter/Leave）
 * 触发的所有 Effect 共享命中信息，使得 RestoreSP 等后续 Effect
 * 能依赖前面 Damage Effect 的真实命中结果。
 */
struct FSkillEffectContext
{
	int32 TotalHitCount = 0;
	TArray<TWeakObjectPtr<AActor>> HitTargets;
};

/**
 * 技能效果执行库。
 *
 * SkillNode 只负责把动画事件分派到 EffectId；真正的目标筛选、伤害结算、
 * VFX 播放和噪音上报都集中在这里，避免 AnimNotify 或 SkillNode 变成业务杂烩。
 */
UCLASS()
class ACTIONGAME_API USkillEffectLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static void ExecuteEffect(
		UObject* WorldContext,
		UActionSkillComponent* SkillComponent,
		UActionSkillObject* SkillObject,
		UActionSkillNode* SkillNode,
		FName EffectId,
		const FSkillEffectRow& EffectRow,
		FSkillEffectContext& Context);

private:
	static void ExecuteDamage(
		UObject* WorldContext,
		UActionSkillComponent* SkillComponent,
		UActionSkillObject* SkillObject,
		UActionSkillNode* SkillNode,
		FName EffectId,
		const FSkillEffectRow& EffectRow,
		FSkillEffectContext& Context);

	static void ExecutePlayVFX(
		UActionSkillComponent* SkillComponent,
		UActionSkillObject* SkillObject,
		const FSkillEffectRow& EffectRow);

	static void ExecuteStopVFX(
		UActionSkillComponent* SkillComponent,
		UActionSkillObject* SkillObject,
		const FSkillEffectRow& EffectRow);

	static void ExecuteReportNoise(
		UActionSkillComponent* SkillComponent,
		const FSkillEffectRow& EffectRow);

	static void ExecuteRestoreSP(
		UActionSkillComponent* SkillComponent,
		const FSkillEffectRow& EffectRow,
		const FSkillEffectContext& Context);

	static void ExecuteSpawnCreature(
		UObject* WorldContext,
		UActionSkillComponent* SkillComponent,
		UActionSkillObject* SkillObject,
		const FSkillEffectRow& EffectRow);

	static FHitContext BuildHitContext(AActionCharacterBase* SourceCharacter, const FSkillEffectRow& EffectRow);
	static FVector GetEffectCenter(AActionCharacterBase* SourceCharacter, const FSkillEffectRow& EffectRow);
	static FRotator GetEffectRotation(AActionCharacterBase* SourceCharacter);

	static int32 ExecuteSphereDamage(
		UObject* WorldContext,
		AActionCharacterBase* SourceCharacter,
		UActionSkillObject* SkillObject,
		const FSkillEffectRow& EffectRow,
		const FHitContext& HitContextTemplate,
		TSet<TWeakObjectPtr<AActor>>& InOutHitActors);

	static int32 ExecuteOverlapShapeDamage(
		UWorld* World,
		AActionCharacterBase* SourceCharacter,
		UActionSkillObject* SkillObject,
		FName EffectId,
		const FSkillEffectRow& EffectRow,
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
		const FSkillEffectRow& EffectRow,
		const FHitContext& HitContextTemplate,
		TSet<TWeakObjectPtr<AActor>>& InOutHitActors);

	static int32 ExecuteConeDamage(
		UWorld* World,
		AActionCharacterBase* SourceCharacter,
		UActionSkillObject* SkillObject,
		FName EffectId,
		const FSkillEffectRow& EffectRow,
		const FHitContext& HitContextTemplate,
		TSet<TWeakObjectPtr<AActor>>& InOutHitActors);

	static int32 DispatchHitActors(
		AActionCharacterBase* SourceCharacter,
		UActionSkillObject* SkillObject,
		FName EffectId,
		const FSkillEffectRow& EffectRow,
		const FHitContext& HitContextTemplate,
		const TArray<AActor*>& CandidateActors,
		TSet<TWeakObjectPtr<AActor>>& InOutHitActors);

	static FCollisionObjectQueryParams MakePawnObjectQueryParams();
	static FCollisionQueryParams MakeSkillCollisionQueryParams(AActor* SourceActor);
};
