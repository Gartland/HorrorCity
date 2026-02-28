// Fill out your copyright notice in the Description page of Project Settings.


#include "LootManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

// Sets default values
ALootManager::ALootManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

void ALootManager::SpawnLoot()
{
	TArray<AActor*> PlacedItems;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ItemBaseClass, PlacedItems);
	
	for (AActor* Actor : PlacedItems)
	{
		if (Actor->GetName().Contains("Key"))
		{
			continue; // skip this one
		}
		FTransform SpawnTransform = Actor->GetActorTransform();

		TSubclassOf<AActor> RandomItem = GetRandomItem();
		if (RandomItem)
		{
			GetWorld()->SpawnActor<AActor>(RandomItem, SpawnTransform);
			Actor->Destroy();
		}
	}
}

// Called when the game starts or when spawned
void ALootManager::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ALootManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

TSubclassOf<AActor> ALootManager::GetRandomItem()
{
	TSubclassOf<AActor> item;
	TArray<TSubclassOf<AActor>> itemPool;
	//build weighted item pool
	for (FLootPool& pool : LootPools) 
	{
		for (int32 i = 0; i < pool.SpawnWeight; i++)
		{
			for (TSubclassOf<AActor> itemRef : pool.ItemBPs)
			{
				itemPool.Add(itemRef);
			}
		}
	}

	//TODO: seed as parameter
	if (itemPool.Num() > 0)
	{
		item = itemPool[FMath::RandRange(0, itemPool.Num() - 1)];
	}
	return item;
}

