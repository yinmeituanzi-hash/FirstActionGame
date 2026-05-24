#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AlertBroadcastSubsystem.generated.h"

class UAlertComponent;

/**
 * Sprint 4-C+++ Day 8：怪物报警广播中心。
 *
 * 这是"群体信息传播"层，不属于单只怪物本体：
 * - Source 怪确认 Target 后调用 BroadcastAlert。
 * - Subsystem 按半径找到附近怪物并调用 ReceiveCombatAlert。
 * - 收到广播的怪物会进 Combat，但不会继续向外广播，避免无限扩散。
 */
UCLASS()
class ACTIONGAME_API UAlertBroadcastSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UAlertBroadcastSubsystem();

	static UAlertBroadcastSubsystem* Get(const UObject* WorldContext);

	/**
	 * 向 Source 周围 Radius 范围内广播"确认目标"。
	 *
	 * @return 实际通知到的怪物数量。
	 */
	void RegisterAlertComponent(UAlertComponent* AlertComponent);
	void UnregisterAlertComponent(UAlertComponent* AlertComponent);

	int32 BroadcastAlert(UAlertComponent* Source, AActor* Target, float Radius);

	/** 控制台命令 AI.AlertBroadcastDebug。1=画广播半径球和接收者连线。 */
	static int32 GDebugDrawAlertBroadcast;

private:
	UPROPERTY()
	TSet<TObjectPtr<UAlertComponent>> RegisteredAlertComponents;
};
