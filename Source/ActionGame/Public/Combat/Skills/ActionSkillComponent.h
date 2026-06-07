#pragma once

#include "CoreMinimal.h"
#include "Combat/Skills/ActionSkillTypes.h"
#include "Components/ActorComponent.h"
#include "ActionSkillComponent.generated.h"

class AActionCharacterBase;
class UActionSkillObject;
class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FActionSkillStateChangedSignature, FName, SkillId, bool, bActive);

/**
 * 角色侧技能系统入口。
 *
 * 负责持有运行时 SkillObject 表，并对外提供 UseSkill / StopSkill。
 * 后续会继续承接技能节点、效果、Montage 事件和取消规则。
 * 玩家输入缓存明确复用已有 UInputBufferComponent，不在这里再做一套。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UActionSkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UActionSkillComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(BlueprintAssignable, Category = "Action|Skill")
	FActionSkillStateChangedSignature OnSkillStateChanged;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Skill|Data")
	TObjectPtr<UDataTable> SkillDataTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Skill|Data")
	TObjectPtr<UDataTable> SkillNodeDataTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Skill|Data")
	TObjectPtr<UDataTable> SkillEffectDataTable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Skill|Data")
	TObjectPtr<UDataTable> SkillCreatureDataTable = nullptr;

	UFUNCTION(BlueprintPure, Category = "Action|Skill")
	AActionCharacterBase* GetOwnerCharacter() const;

	UFUNCTION(BlueprintPure, Category = "Action|Skill")
	bool IsUsingSkill() const { return CurrentSkillObject != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Action|Skill")
	FName GetCurrentSkillId() const;

	UFUNCTION(BlueprintPure, Category = "Action|Skill")
	int32 GetSkillCount() const { return SkillObjectMap.Num(); }

	UFUNCTION(BlueprintPure, Category = "Action|Skill")
	UActionSkillObject* GetSkillObject(FName SkillId) const;

	UFUNCTION(BlueprintPure, Category = "Action|Skill")
	float GetSkillCooldownRemaining(FName SkillId) const;

	UFUNCTION(BlueprintCallable, Category = "Action|Skill")
	bool CanUseSkill(FName SkillId, EActionSkillCancelFlag IncomingType = EActionSkillCancelFlag::Skill) const;

	UFUNCTION(BlueprintCallable, Category = "Action|Skill")
	bool UseSkill(FName SkillId, AActor* OptionalTarget = nullptr, EActionSkillCancelFlag IncomingType = EActionSkillCancelFlag::Skill);

	UFUNCTION(BlueprintCallable, Category = "Action|Skill")
	void StopSkill(EActionSkillStopReason Reason = EActionSkillStopReason::Normal);

	/** 后续由技能 AnimNotify 调用；Day1 先只打日志，验证事件链路。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Skill")
	void OnSkillNotify(FName EventName);

	/** 检查当前技能是否允许被某类新动作打断。 */
	UFUNCTION(BlueprintPure, Category = "Action|Skill")
	bool CheckCanCancelCurrentSkill(EActionSkillCancelFlag IncomingType) const;

	/** 如果当前有技能，按配置尝试打断它；没有技能时视为成功。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Skill")
	bool TryCancelCurrentSkill(EActionSkillCancelFlag IncomingType, EActionSkillStopReason Reason);

private:
	/** 按 SkillId 持有的运行时技能对象。冷却等运行时状态存在这里，不写回 DataTable。 */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UActionSkillObject>> SkillObjectMap;

	/** 当前正在执行的技能。Day3 会在此基础上增加当前节点 / Montage 时间线。 */
	UPROPERTY(Transient)
	TObjectPtr<UActionSkillObject> CurrentSkillObject = nullptr;

	/** 当前技能启动时实际添加过的标签，停止时只移除这一组，避免误删别的系统标签。 */
	UPROPERTY(Transient)
	FGameplayTagContainer ActiveSkillAppliedTags;

	/** 从 SkillDataTable 重建 SkillObjectMap。BeginPlay 调用，也方便编辑器测试时复用。 */
	void LoadSkillObjectsFromTable();
	void ClearSkillObjects();
	void ApplyActiveSkillTags(const FActionSkillRow& SkillData);
	void ClearActiveSkillTags();
};
