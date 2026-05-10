#include "Combat/Feedback/HitFeedbackComponent.h"

#include "Camera/CameraShakeBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHitFeedback, Log, All);

UHitFeedbackComponent::UHitFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHitFeedbackComponent::TriggerHitFeedback(ACharacter* Victim, const FVector& HitLocation, ACharacter* Attacker, float DamageScale)
{
	const float Scale = FMath::Clamp(DamageScale, 0.0f, 2.0f);

	// 1. HitStop
	if (HitStopDuration > 0.0f)
	{
		const float ScaledDuration = HitStopDuration * Scale;
		if (Victim != nullptr)
		{
			ApplyHitStop(Victim, ScaledDuration);
		}
		if (bApplyHitStopToAttacker && Attacker != nullptr && Attacker != Victim)
		{
			ApplyHitStop(Attacker, ScaledDuration);
		}
	}

	// 2. Camera Shake
	PlayCameraShakeFor(Attacker, Scale);

	// 3. Particle
	SpawnHitParticleAt(HitLocation);

	// 4. Sound
	PlayHitSoundAt(HitLocation);
}

// ============================================================================
// HitStop
// ============================================================================

void UHitFeedbackComponent::ApplyHitStop(ACharacter* Char, float Duration)
{
	if (Char == nullptr || Duration <= 0.0f)
	{
		return;
	}
	USkeletalMeshComponent* Mesh = Char->GetMesh();
	if (Mesh == nullptr)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// 直接改 GlobalAnimRateScale，是 UE 里实现 HitStop 最简单可靠的方式：
	// - 不影响 RootMotion 已经驱动的胶囊位移（只冻结动画播放节奏）
	// - 不需要单独冻结某个 Montage
	// - 多个 HitStop 重叠时也能正确叠加（每次都重置为 HitStopAnimRate，再用 Timer 还原）
	Mesh->GlobalAnimRateScale = HitStopAnimRate;

	TWeakObjectPtr<USkeletalMeshComponent> MeshRef(Mesh);
	FTimerHandle TempHandle;
	World->GetTimerManager().SetTimer(
		TempHandle,
		FTimerDelegate::CreateUObject(this, &UHitFeedbackComponent::RestoreAnimRate, MeshRef),
		Duration,
		false);
}

void UHitFeedbackComponent::RestoreAnimRate(TWeakObjectPtr<USkeletalMeshComponent> MeshRef)
{
	if (USkeletalMeshComponent* Mesh = MeshRef.Get())
	{
		Mesh->GlobalAnimRateScale = 1.0f;
	}
}

// ============================================================================
// Camera Shake
// ============================================================================

void UHitFeedbackComponent::PlayCameraShakeFor(ACharacter* Attacker, float Scale)
{
	if (CameraShakeClass == nullptr || Attacker == nullptr)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Attacker->GetController());
	if (PC == nullptr)
	{
		return;
	}

	PC->ClientStartCameraShake(CameraShakeClass, CameraShakeScale * Scale);
}

// ============================================================================
// Particle
// ============================================================================

void UHitFeedbackComponent::SpawnHitParticleAt(const FVector& Location)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	if (HitParticle != nullptr)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			HitParticle,
			Location,
			FRotator::ZeroRotator);
		return;
	}

	if (bDrawDebugIfNoParticle)
	{
		// 没有粒子时画个红球当占位提示，开发期可见，方便确认命中位置。
		DrawDebugSphere(World, Location, 25.0f, 12, FColor::Red, false, 0.5f);
	}
}

// ============================================================================
// Sound
// ============================================================================

void UHitFeedbackComponent::PlayHitSoundAt(const FVector& Location)
{
	if (HitSound == nullptr)
	{
		return;
	}
	UGameplayStatics::PlaySoundAtLocation(this, HitSound, Location);
}
