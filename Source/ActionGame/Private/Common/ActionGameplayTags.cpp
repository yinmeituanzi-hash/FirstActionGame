#include "Common/ActionGameplayTags.h"

namespace ActionGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(State_Action_Attacking, "State.Action.Attacking");
	UE_DEFINE_GAMEPLAY_TAG(State_Action_Dodging, "State.Action.Dodging");
	UE_DEFINE_GAMEPLAY_TAG(State_Action_HitReact, "State.Action.HitReact");
	UE_DEFINE_GAMEPLAY_TAG(State_Action_Dead, "State.Action.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Action_LandingStartup, "State.Action.Landing.Startup");
	UE_DEFINE_GAMEPLAY_TAG(State_Action_LandingRecovery, "State.Action.Landing.Recovery");

	UE_DEFINE_GAMEPLAY_TAG(State_SuperArmor, "State.SuperArmor");
	UE_DEFINE_GAMEPLAY_TAG(State_Ragdoll, "State.Ragdoll");

	UE_DEFINE_GAMEPLAY_TAG(Block_Attack, "Block.Attack");
	UE_DEFINE_GAMEPLAY_TAG(Block_Dodge, "Block.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(Block_Move, "Block.Move");

	UE_DEFINE_GAMEPLAY_TAG(Block_HitReact, "Block.HitReact");
	UE_DEFINE_GAMEPLAY_TAG(Block_AIControl, "Block.AIControl");

	UE_DEFINE_GAMEPLAY_TAG(Window_Attack_CanDodgeCancel, "Window.Attack.CanDodgeCancel");
	UE_DEFINE_GAMEPLAY_TAG(Window_Attack_CanCombo, "Window.Attack.CanCombo");
	UE_DEFINE_GAMEPLAY_TAG(Window_Attack_CanTurn, "Window.Attack.CanTurn");
	UE_DEFINE_GAMEPLAY_TAG(Window_Dodge_CanRecover, "Window.Dodge.CanRecover");
}
