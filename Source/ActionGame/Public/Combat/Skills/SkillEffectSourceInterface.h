#pragma once

#include "CoreMinimal.h"
#include "Char/ActionCharacterBase.h"
#include "UObject/Interface.h"
#include "SkillEffectSourceInterface.generated.h"

class AActionCharacterBase;
class UActionSkillComponent;

UINTERFACE(BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class ACTIONGAME_API USkillEffectSourceInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 技能效果来源接口。
 *
 * 对应 010 的 IEffectSourceInterface。角色和 SkillCreature 都可以作为
 * "效果来源"，让 SkillEffectLibrary 在计算伤害、归属、仇恨、SP 回复时
 * 只依赖这个接口，不必区分具体是角色还是投射物。
 *
 * Day8.1 只暴露最小 4 个查询接口：
 * - 归属角色（伤害归属、仇恨、掉落）
 * - 技能 Id（连击统计、Buff 归属）
 * - 阵营（友军过滤）
 * - Actor 本体（Trace/VFX/Location 查询）
 *
 * 后续可以按需追加：GetSkillLevel / GetAttributesSet / GetTeamOverride 等。
 */
class ACTIONGAME_API ISkillEffectSourceInterface
{
	GENERATED_BODY()

public:
	/** 归属角色，null 表示"没有明确角色发起者"（例如场景陷阱）。 */
	virtual AActionCharacterBase* GetSourceCharacter() const = 0;

	/** 触发本次效果的技能 Id。 */
	virtual FName GetSourceSkillId() const = 0;

	/** 阵营。默认转发 SourceCharacter->CombatTeam，可被子类覆写。 */
	virtual EActionCombatTeam GetSourceTeam() const
	{
		if (AActionCharacterBase* Source = GetSourceCharacter())
		{
			return Source->GetCombatTeam();
		}
		return EActionCombatTeam::Neutral;
	}

	/** 效果来源自身 Actor（可以是角色，也可以是 SkillCreature）。 */
	virtual AActor* GetEffectSourceActor() = 0;
};
