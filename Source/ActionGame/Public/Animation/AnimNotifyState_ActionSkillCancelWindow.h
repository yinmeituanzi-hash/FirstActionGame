#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Combat/Skills/ActionSkillTypes.h"
#include "AnimNotifyState_ActionSkillCancelWindow.generated.h"

class UActionSkillComponent;

/**
 * 技能取消窗口。
 *
 * 把这个 NotifyState 放到技能 Montage 的恢复段上。Begin 时打开指定取消窗口，
 * End 时关闭窗口；玩家输入仍然走 SkillComponent 的取消规则，不会在 NotifyState 里直接停技能。
 */
UCLASS(meta = (DisplayName = "Action Skill Cancel Window"))
class ACTIONGAME_API UAnimNotifyState_ActionSkillCancelWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Skill", meta = (Bitmask, BitmaskEnum = "/Script/ActionGame.EActionSkillCancelFlag"))
	int32 CancelWindowMask = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Skill")
	bool bReleaseMoveBlock = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Skill")
	bool bReleaseDodgeBlock = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Skill")
	bool bReleaseAttackBlock = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Skill")
	bool bMarkRecoverWindow = true;

	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

private:
	static UActionSkillComponent* GetSkillComponent(USkeletalMeshComponent* MeshComp);
};
