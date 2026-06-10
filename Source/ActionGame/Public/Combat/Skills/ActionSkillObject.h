#pragma once

#include "CoreMinimal.h"
#include "Combat/Skills/ActionSkillTypes.h"
#include "UObject/Object.h"
#include "ActionSkillObject.generated.h"

class AActionCharacterBase;

/**
 * 单个角色身上一条技能的持久运行时对象。
 *
 * DataTable 行描述“技能是什么”；本对象保存“这个技能当前发生了什么”，
 * 例如是否激活、冷却剩余时间、当前目标、停止原因等。
 */
UCLASS(BlueprintType)
class ACTIONGAME_API UActionSkillObject : public UObject
{
	GENERATED_BODY()

public:
	/** 拷贝技能配置行，并把该运行时对象绑定到所属角色。 */
	void InitFromData(AActionCharacterBase* InOwner, FName InSkillId, const FActionSkillRow& InSkillData);

	UFUNCTION(BlueprintPure, Category = "Action|Skill")
	AActionCharacterBase* GetOwnerCharacter() const { return OwnerCharacter.Get(); }

	UFUNCTION(BlueprintPure, Category = "Action|Skill")
	FName GetSkillId() const { return SkillId; }

	UFUNCTION(BlueprintPure, Category = "Action|Skill")
	FActionSkillRow GetSkillData() const { return SkillData; }

	UFUNCTION(BlueprintPure, Category = "Action|Skill")
	bool IsInitialized() const { return bInitialized; }

	UFUNCTION(BlueprintPure, Category = "Action|Skill")
	bool IsActive() const { return bActive; }

	UFUNCTION(BlueprintPure, Category = "Action|Skill")
	float GetCooldownRemaining() const { return CooldownRemaining; }

	UFUNCTION(BlueprintPure, Category = "Action|Skill")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	bool CanActivate() const;
	void Activate(AActor* InTarget);
	void Deactivate(EActionSkillStopReason Reason);
	void TickCooldown(float DeltaTime);

	/** 位掩码检查：当前技能配置决定哪些新动作类型可以打断它。 */
	bool CanBeCancelledBy(EActionSkillCancelFlag IncomingType) const;

	/** 当前技能节点 / 动作段内的命中去重集合，避免同一段动作的多个 Notify 重复结算同一目标。 */
	TSet<TWeakObjectPtr<AActor>>& GetMutableHitActorsThisNode() { return HitActorsThisNode; }
	void ResetHitActorsThisNode();

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<AActionCharacterBase> OwnerCharacter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill", meta = (AllowPrivateAccess = "true"))
	FName SkillId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill", meta = (AllowPrivateAccess = "true"))
	FActionSkillRow SkillData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill", meta = (AllowPrivateAccess = "true"))
	bool bInitialized = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill", meta = (AllowPrivateAccess = "true"))
	bool bActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill", meta = (AllowPrivateAccess = "true"))
	float CooldownRemaining = 0.0f;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CurrentTarget;

	TSet<TWeakObjectPtr<AActor>> HitActorsThisNode;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill", meta = (AllowPrivateAccess = "true"))
	EActionSkillStopReason LastStopReason = EActionSkillStopReason::Normal;
};
