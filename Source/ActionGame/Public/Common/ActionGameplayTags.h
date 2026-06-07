#pragma once

#include "NativeGameplayTags.h"

namespace ActionGameplayTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Action_Attacking);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Action_Skill);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Action_Dodging);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Action_HitReact);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Action_Dead);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Action_LandingStartup);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Action_LandingRecovery);

	/** 霸体：拥有此 Tag 时受击不会触发 HitReact 动画，但伤害和反馈仍正常结算。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_SuperArmor);

	/** Ragdoll 状态：物理布娃娃中。受击系统在此期间应跳过 React/Physics 重复施加。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Ragdoll);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Block_Attack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Block_Dodge);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Block_Move);

	/** 在受击中，禁止进入新的攻击/移动等主动动作。由 HitReactFeature 启停时维护。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Block_HitReact);

	/**
	 * Ragdoll / 死亡 / 强控等"AI 不应该决策"的状态期间挂上此 Tag。
	 * BehaviorTree 根 Decorator 检测到此 Tag 即整树跳过，状态结束时自然恢复。
	 * 由 HitPhysicsComponent 在进入/退出 Ragdoll 时维护。
	 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Block_AIControl);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Window_Attack_CanDodgeCancel);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Window_Attack_CanCombo);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Window_Attack_CanTurn);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Window_Dodge_CanRecover);
}
