#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_ActionSuperArmor.generated.h"

/**
 * 动画霸体窗口。
 *
 * 把这个 Notify State 放到攻击、蓄力或起身等蒙太奇片段上后，
 * 受击者会在该时间段内跳过 HitReact 动画，但仍然正常扣血和播放命中反馈。
 */
UCLASS(meta = (DisplayName = "Action Super Armor"))
class ACTIONGAME_API UAnimNotifyState_ActionSuperArmor : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

private:
	static void SetSuperArmorTag(USkeletalMeshComponent* MeshComp, bool bEnable);
};
