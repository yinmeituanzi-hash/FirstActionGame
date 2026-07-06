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
 * Montage、Enter/Leave 效果、Notify 效果，以及 Day5 新增的连段窗口状态。
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
		const FSkillNodeRow& InNodeData,
		UDataTable* InSkillEffectDataTable);

	void Activate();
	void Deactivate();
	void OnNotify(FName EventName);

	/** Combo Notify 打开同技能内的节点跳转窗口。 */
	void OnNotifyNextCombo(FName InputName, FName HoldType);

	/** Quit Notify 标记当前节点可以自然退出。 */
	void OnNotifyQuitSkill();
	void OnNotifyTurnWindow();

	/** 每帧由 SkillComponent 调用，返回应该跳转的目标 NodeId。 */
	FName CheckComboTransition();

	void TickByTimeLine();
	void HandleMontageEndSkillQuit();

	bool ShouldQuitSkill() const { return bQuitSkillFlag; }
	bool CanTurnNextNode() const { return bCanTurnNextNode; }
	bool HasValidInput() const;

	UFUNCTION(BlueprintPure, Category = "Action|Skill|Node")
	FName GetNodeId() const { return NodeId; }

	const FSkillNodeRow& GetNodeData() const { return NodeData; }
	FVector GetActivationLocation() const { return ActivationLocation; }
	FRotator GetActivationRotation() const { return ActivationRotation; }

	FName GetComboInputName() const { return ComboInputName; }
	FName GetLastMatchedComboInputName() const { return LastMatchedComboInputName; }

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UActionSkillComponent> OwnerComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UActionSkillObject> SkillObject;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill|Node", meta = (AllowPrivateAccess = "true"))
	FName NodeId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill|Node", meta = (AllowPrivateAccess = "true"))
	FSkillNodeRow NodeData;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> SkillEffectDataTable = nullptr;

	UPROPERTY(Transient)
	TArray<FName> EffectsWhenEnter;

	UPROPERTY(Transient)
	TArray<FName> EffectsWhenLeave;

	TMap<FName, TArray<FName>> NotifyEffectMap;

	bool bActive = false;
	bool bCanEnterNextNode = false;
	bool bCanTurnNextNode = false;
	bool bQuitSkillFlag = false;
	bool bAppliedRootMotionOverride = false;
	FName ComboInputName = NAME_None;
	FName ComboHoldType = NAME_None;
	FName LastMatchedComboInputName = NAME_None;
	FVector ActivationLocation = FVector::ZeroVector;
	FRotator ActivationRotation = FRotator::ZeroRotator;

	void BuildEffectIndex(UDataTable* InSkillEffectDataTable);
	void ExecuteEffects(const TArray<FName>& EffectIds, const FString& TimingText);
	void ResetComboState();
	bool HasValidBufferedInput(FName InputName) const;
	void ApplyRootMotionSettings();
	void ClearRootMotionSettings();
};
