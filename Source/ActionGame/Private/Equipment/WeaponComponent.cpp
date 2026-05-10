#include "Equipment/WeaponComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeapon, Log, All);

UWeaponComponent::UWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	// 默认装备：SkeletalMesh 优先（更高规格的武器），否则用 StaticMesh。
	if (DefaultRightHandSkeletalMesh != nullptr)
	{
		EquipSkeletalMesh(EWeaponHand::Right, DefaultRightHandSkeletalMesh);
	}
	else if (DefaultRightHandStaticMesh != nullptr)
	{
		EquipStaticMesh(EWeaponHand::Right, DefaultRightHandStaticMesh);
	}

	if (DefaultLeftHandSkeletalMesh != nullptr)
	{
		EquipSkeletalMesh(EWeaponHand::Left, DefaultLeftHandSkeletalMesh);
	}
	else if (DefaultLeftHandStaticMesh != nullptr)
	{
		EquipStaticMesh(EWeaponHand::Left, DefaultLeftHandStaticMesh);
	}
}

ACharacter* UWeaponComponent::GetCharacterOwner() const
{
	return Cast<ACharacter>(GetOwner());
}

FName UWeaponComponent::GetSocketNameForHand(EWeaponHand Hand) const
{
	return Hand == EWeaponHand::Right ? RightHandSocketName : LeftHandSocketName;
}

void UWeaponComponent::EquipStaticMesh(EWeaponHand Hand, UStaticMesh* WeaponMesh)
{
	if (WeaponMesh == nullptr)
	{
		UE_LOG(LogWeapon, Warning, TEXT("WeaponComponent: EquipStaticMesh called with null mesh."));
		return;
	}
	AttachStaticMeshToSocket(Hand, WeaponMesh);
}

void UWeaponComponent::EquipSkeletalMesh(EWeaponHand Hand, USkeletalMesh* WeaponMesh)
{
	if (WeaponMesh == nullptr)
	{
		UE_LOG(LogWeapon, Warning, TEXT("WeaponComponent: EquipSkeletalMesh called with null mesh."));
		return;
	}
	AttachSkeletalMeshToSocket(Hand, WeaponMesh);
}

void UWeaponComponent::Unequip(EWeaponHand Hand)
{
	if (Hand == EWeaponHand::Right)
	{
		if (RightHandWeapon != nullptr)
		{
			RightHandWeapon->DestroyComponent();
			RightHandWeapon = nullptr;
		}
	}
	else
	{
		if (LeftHandWeapon != nullptr)
		{
			LeftHandWeapon->DestroyComponent();
			LeftHandWeapon = nullptr;
		}
	}
}

void UWeaponComponent::UnequipAll()
{
	Unequip(EWeaponHand::Right);
	Unequip(EWeaponHand::Left);
}

bool UWeaponComponent::HasWeapon(EWeaponHand Hand) const
{
	return Hand == EWeaponHand::Right ? RightHandWeapon != nullptr : LeftHandWeapon != nullptr;
}

UMeshComponent* UWeaponComponent::GetWeaponMeshComponent(EWeaponHand Hand) const
{
	return Hand == EWeaponHand::Right ? RightHandWeapon.Get() : LeftHandWeapon.Get();
}

void UWeaponComponent::AttachStaticMeshToSocket(EWeaponHand Hand, UStaticMesh* WeaponMesh)
{
	ACharacter* Owner = GetCharacterOwner();
	if (Owner == nullptr || Owner->GetMesh() == nullptr)
	{
		UE_LOG(LogWeapon, Warning, TEXT("WeaponComponent: Cannot attach weapon, owner or mesh is null."));
		return;
	}

	// 销毁旧的（若有）。
	Unequip(Hand);

	UStaticMeshComponent* NewMesh = NewObject<UStaticMeshComponent>(Owner);
	NewMesh->RegisterComponent();
	NewMesh->SetStaticMesh(WeaponMesh);
	NewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NewMesh->AttachToComponent(
		Owner->GetMesh(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		GetSocketNameForHand(Hand));

	if (Hand == EWeaponHand::Right)
	{
		RightHandWeapon = NewMesh;
	}
	else
	{
		LeftHandWeapon = NewMesh;
	}

	UE_LOG(LogWeapon, Log, TEXT("WeaponComponent: Equipped static mesh %s to %s socket %s."),
		*GetNameSafe(WeaponMesh),
		Hand == EWeaponHand::Right ? TEXT("Right") : TEXT("Left"),
		*GetSocketNameForHand(Hand).ToString());
}

void UWeaponComponent::AttachSkeletalMeshToSocket(EWeaponHand Hand, USkeletalMesh* WeaponMesh)
{
	ACharacter* Owner = GetCharacterOwner();
	if (Owner == nullptr || Owner->GetMesh() == nullptr)
	{
		UE_LOG(LogWeapon, Warning, TEXT("WeaponComponent: Cannot attach weapon, owner or mesh is null."));
		return;
	}

	Unequip(Hand);

	USkeletalMeshComponent* NewMesh = NewObject<USkeletalMeshComponent>(Owner);
	NewMesh->RegisterComponent();
	NewMesh->SetSkeletalMesh(WeaponMesh);
	NewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NewMesh->AttachToComponent(
		Owner->GetMesh(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		GetSocketNameForHand(Hand));

	if (Hand == EWeaponHand::Right)
	{
		RightHandWeapon = NewMesh;
	}
	else
	{
		LeftHandWeapon = NewMesh;
	}

	UE_LOG(LogWeapon, Log, TEXT("WeaponComponent: Equipped skeletal mesh %s to %s socket %s."),
		*GetNameSafe(WeaponMesh),
		Hand == EWeaponHand::Right ? TEXT("Right") : TEXT("Left"),
		*GetSocketNameForHand(Hand).ToString());
}
