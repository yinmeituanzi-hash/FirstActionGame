#include "AI/ActionMonsterAIController.h"

#include "AI/ActionAIBlackboardKeys.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Char/ActionMonsterCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogActionMonsterAI, Log, All);

AActionMonsterAIController::AActionMonsterAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// AIController 不需要 Tick 自己的逻辑（Tick 走 BTComponent + Character）。
	// 但 UpdateControlRotation 走的是 AAIController 父类 Tick 链路，必须保留 Tick。
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AActionMonsterAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (BTAsset == nullptr)
	{
		UE_LOG(
			LogActionMonsterAI,
			Warning,
			TEXT("ActionMonsterAIController: BTAsset is not assigned. AI will be idle. Pawn=%s"),
			*GetNameSafe(InPawn));
		return;
	}

	// RunBehaviorTree 内部会：
	//  1. 如果 BTAsset 关联 BlackboardAsset 且当前 Blackboard 不兼容，UseBlackboard 重新初始化。
	//  2. 创建 UBehaviorTreeComponent 并赋给 BrainComponent。
	//  3. StartTree(*BTAsset, EBTExecutionMode::Looped)。
	if (!RunBehaviorTree(BTAsset))
	{
		UE_LOG(
			LogActionMonsterAI,
			Warning,
			TEXT("ActionMonsterAIController: RunBehaviorTree failed. BTAsset=%s"),
			*GetNameSafe(BTAsset));
		return;
	}

	// 缓存到自己的 typed 字段，方便外部直接拿，不用每次 Cast。
	BehaviorComp = Cast<UBehaviorTreeComponent>(BrainComponent);
	BlackboardComp = Blackboard;
	if (BlackboardComp != nullptr && InPawn != nullptr)
	{
		// 必须在 RunBehaviorTree 之后写：RunBehaviorTree 会初始化/替换 Blackboard。
		// 写早了可能写到旧 Blackboard 上，FindPatrolPoint 就读不到 HomeLocation。
		BlackboardComp->SetValueAsVector(ActionAIBlackboardKeys::HomeLocation, InPawn->GetActorLocation());
	}

	// 初始化 DesiredControlRotation，避免第一次插值从 (0,0,0) 大角度滑动。
	DesiredControlRotation = GetControlRotation();

	UE_LOG(
		LogActionMonsterAI,
		Log,
		TEXT("ActionMonsterAIController: BehaviorTree started. Pawn=%s BT=%s"),
		*GetNameSafe(InPawn),
		*GetNameSafe(BTAsset));
}

void AActionMonsterAIController::OnUnPossess()
{
	if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(BrainComponent))
	{
		BTComp->StopTree(EBTStopMode::Safe);
	}

	BehaviorComp = nullptr;
	BlackboardComp = nullptr;

	Super::OnUnPossess();
}

void AActionMonsterAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(BrainComponent))
	{
		BTComp->StopTree(EBTStopMode::Forced);
	}

	BehaviorComp = nullptr;
	BlackboardComp = nullptr;

	Super::EndPlay(EndPlayReason);
}

AActionMonsterCharacter* AActionMonsterAIController::GetMonsterCharacter() const
{
	return Cast<AActionMonsterCharacter>(GetCharacter());
}

void AActionMonsterAIController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	APawn* const MyPawn = GetPawn();
	if (MyPawn == nullptr)
	{
		return;
	}

	// 当前帧"想要"看向的方向：
	//  - 有 SetFocus / SetFocalPoint 设置时，这里会指向焦点位置。
	//  - 没有设置时退化成当前 ControlRotation。
	FRotator NewControlRotation = GetControlRotation();
	const FVector FocalPoint = GetFocalPoint();
	if (FAISystem::IsValidLocation(FocalPoint))
	{
		NewControlRotation = (FocalPoint - MyPawn->GetPawnViewLocation()).Rotation();
	}

	// 怪物视角通常只需要 Yaw，强制 Pitch / Roll = 0 避免抬头/侧翻。
	NewControlRotation.Pitch = 0.0f;
	NewControlRotation.Roll = 0.0f;

	if (bSmoothControlRotation && SmoothControlRotationSpeed > 0.0f)
	{
		// 用 RInterpConstantTo 做"匀速插值"，不是 RInterpTo（指数衰减）。
		// 原因：动作游戏里怪物转头需要"恒定快速但不瞬切"，匀速比指数曲线更"机敏"。
		const FRotator CurrentRotation = GetControlRotation();
		DesiredControlRotation = NewControlRotation;
		const FRotator InterpRotation = FMath::RInterpConstantTo(
			CurrentRotation, DesiredControlRotation, DeltaTime, SmoothControlRotationSpeed);
		SetControlRotation(InterpRotation);
	}
	else
	{
		SetControlRotation(NewControlRotation);
	}

	if (bUpdatePawn)
	{
		const FRotator CurrentPawnRotation = MyPawn->GetActorRotation();
		const FRotator FinalControlRotation = GetControlRotation();
		if (!CurrentPawnRotation.Equals(FinalControlRotation, 1e-3f))
		{
			MyPawn->FaceRotation(FinalControlRotation, DeltaTime);
		}
	}
}
