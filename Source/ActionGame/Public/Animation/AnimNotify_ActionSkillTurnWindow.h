#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ActionSkillTurnWindow.generated.h"

/**
 * 标记当前 SkillNode 允许下一段启动时进行一次轻量转向。
 *
 * Notify 本身不直接转角色，只把窗口状态交给 SkillNode；
 * SkillComponent 在真正切到下一段时消费该状态，转向执行仍走 Combat / Movement 层。
 */
UCLASS(meta = (DisplayName = "Action Skill Turn Window"))
class ACTIONGAME_API UAnimNotify_ActionSkillTurnWindow : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
