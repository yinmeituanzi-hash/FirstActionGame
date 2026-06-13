#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ActionQuitSkill.generated.h"

/**
 * 标记当前技能节点可以自然退出的单帧 Notify。
 *
 * Quit 的优先级低于连段跳转：同一帧既能跳下一段又能退出时，SkillComponent 会先处理连段。
 */
UCLASS(meta = (DisplayName = "Action Quit Skill"))
class ACTIONGAME_API UAnimNotify_ActionQuitSkill : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
