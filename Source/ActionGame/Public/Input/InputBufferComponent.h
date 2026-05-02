#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputBufferComponent.generated.h"

/**
 * 最小输入缓存组件。
 *
 * 现在只做一件事：
 * 把“某个输入在某个时间点被按下”记住一小段时间，
 * 之后由角色逻辑决定什么时候消费它。
 *
 * 这样做的好处是：
 * 以后就算角色正处于攻击中、受击中，暂时不能立刻执行动作，
 * 也可以把输入先存一下，等窗口到了再取出来执行。
 */
UCLASS(ClassGroup=(Action), meta=(BlueprintSpawnableComponent))
class ACTIONGAME_API UInputBufferComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInputBufferComponent();

	/** 记录一个输入，并设置它的有效期。 */
	void PushInput(FName InputName, float Lifetime);

	/** 如果该输入还在有效期内，就消费一次并返回 true。 */
	bool ConsumeInput(FName InputName);

	/** 查询输入当前是否仍在有效期内。 */
	bool HasValidInput(FName InputName) const;

private:
	/** 记录每个输入的过期时间。Key 是输入名，Value 是世界时间秒数。 */
	TMap<FName, float> BufferedInputExpireTimes;

	/** 清理已经过期的输入，避免缓存表无限增长。 */
	void ClearExpiredInputs();
};
