#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ActionSkillEvent.generated.h"

/**
 * 技能事件 Notify。
 *
 * 动画只负责发出事件名，例如 Hit / CastStart / SkillRecoverStart。
 * 具体执行哪些效果由当前 SkillNode 根据 DataTable 映射决定。
 */
UCLASS(meta = (DisplayName = "Action Skill Event"))
class ACTIONGAME_API UAnimNotify_ActionSkillEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Skill")
	FName EventName = NAME_None;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
