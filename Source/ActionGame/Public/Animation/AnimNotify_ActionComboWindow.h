#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ActionComboWindow.generated.h"

/**
 * 打开技能连段窗口的单帧 Notify。
 *
 * 它只告诉当前 SkillNode：“从这一帧开始可以检查某个输入并跳到下一个节点”。
 * 真正的输入判断、消费和节点切换都在 SkillComponent Tick 中完成。
 */
UCLASS(meta = (DisplayName = "Action Combo Window"))
class ACTIONGAME_API UAnimNotify_ActionComboWindow : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Skill|Combo")
	FName ComboInputName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Skill|Combo")
	FName HoldType = NAME_None;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
