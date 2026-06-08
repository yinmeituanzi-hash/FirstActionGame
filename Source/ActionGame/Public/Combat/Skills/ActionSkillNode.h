#pragma once

#include "CoreMinimal.h"
#include "Combat/Skills/ActionSkillTypes.h"
#include "UObject/Object.h"
#include "ActionSkillNode.generated.h"

class UActionSkillComponent;
class UActionSkillObject;
class UDataTable;

/**
 * 技能中的一个动作节点。
 *
 * SkillObject 管技能生命周期和冷却；SkillNode 管一次动作段：
 * 播哪个 Montage、进入/离开节点执行哪些效果、某个 AnimNotify 到来时触发哪些效果。
 */
UCLASS(BlueprintType)
class ACTIONGAME_API UActionSkillNode : public UObject
{
	GENERATED_BODY()

public:
	void InitFromData(
		UActionSkillComponent* InOwnerComponent,
		UActionSkillObject* InSkillObject,
		FName InNodeId,
		const FActionSkillNodeRow& InNodeData,
		UDataTable* SkillEffectDataTable);

	void Activate();
	void Deactivate();
	void OnNotify(FName EventName);

	UFUNCTION(BlueprintPure, Category = "Action|Skill|Node")
	FName GetNodeId() const { return NodeId; }

	const FActionSkillNodeRow& GetNodeData() const { return NodeData; }

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UActionSkillComponent> OwnerComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UActionSkillObject> SkillObject;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill|Node", meta = (AllowPrivateAccess = "true"))
	FName NodeId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill|Node", meta = (AllowPrivateAccess = "true"))
	FActionSkillNodeRow NodeData;

	UPROPERTY(Transient)
	TArray<FName> EffectsWhenEnter;

	UPROPERTY(Transient)
	TArray<FName> EffectsWhenLeave;

	TMap<FName, TArray<FName>> NotifyEffectMap;

	bool bActive = false;

	void BuildEffectIndex(UDataTable* SkillEffectDataTable);
	void ExecuteEffects(const TArray<FName>& EffectIds, const FString& TimingText);
};
