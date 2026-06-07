#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ActionSkillCreature.generated.h"

class AActionCharacterBase;

/**
 * 技能生成物基础 Actor，例如投射物、陷阱、范围场。
 *
 * Day1 只保存来源角色和技能 Id。移动、碰撞、命中逻辑会在后续 SkillCreature
 * 专项开发时补齐，让技能效果可以统一生成这类对象。
 */
UCLASS()
class ACTIONGAME_API AActionSkillCreature : public AActor
{
	GENERATED_BODY()

public:
	AActionSkillCreature();

	UFUNCTION(BlueprintPure, Category = "Action|Skill|Creature")
	AActionCharacterBase* GetSourceCharacter() const { return SourceCharacter.Get(); }

	UFUNCTION(BlueprintPure, Category = "Action|Skill|Creature")
	FName GetSourceSkillId() const { return SourceSkillId; }

	void InitSkillCreature(AActionCharacterBase* InSourceCharacter, FName InSourceSkillId);

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<AActionCharacterBase> SourceCharacter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill|Creature", meta = (AllowPrivateAccess = "true"))
	FName SourceSkillId = NAME_None;
};
