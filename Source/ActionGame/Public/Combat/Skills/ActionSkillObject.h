#pragma once

#include "CoreMinimal.h"
#include "Combat/Skills/ActionSkillTypes.h"
#include "UObject/Object.h"
#include "ActionSkillObject.generated.h"

class AActionCharacterBase;
class UActionSkillComponent;
class UActionSkillNode;
class UDataTable;

/**
 * 单个角色身上一条技能的运行时对象。
 *
 * DataTable 描述“技能是什么”；SkillObject 保存“这个技能当前发生了什么”。
 * Day5 起它还负责持有当前技能激活期间可复用的 NodeMap，避免每段连击临时创建节点。
 */
UCLASS(BlueprintType)
class ACTIONGAME_API UActionSkillObject : public UObject
{
	GENERATED_BODY()

public:
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

	/** 从 BeginNodeId 开始递归创建当前技能激活期间复用的节点池。 */
	bool InitSkillNodes(UActionSkillComponent* OwnerComponent, UDataTable* SkillNodeDataTable, UDataTable* SkillEffectDataTable);

	UActionSkillNode* GetSkillNode(FName NodeId) const;

	/** 当前技能配置决定哪些新动作类型可以打断它。 */
	bool CanBeCancelledBy(EActionSkillCancelFlag IncomingType) const;

	/** 当前技能节点内的命中去重集合。只在 Effect 明确要求节点级去重时使用。 */
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

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UActionSkillNode>> NodeMap;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill", meta = (AllowPrivateAccess = "true"))
	EActionSkillStopReason LastStopReason = EActionSkillStopReason::Normal;

	bool InitSkillNodeRecursive(
		FName NodeId,
		UActionSkillComponent* OwnerComponent,
		UDataTable* SkillNodeDataTable,
		UDataTable* SkillEffectDataTable);
};
