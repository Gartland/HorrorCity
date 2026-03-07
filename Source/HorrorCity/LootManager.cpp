// Fill out your copyright notice in the Description page of Project Settings.


#include "LootManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "DungeonGenerator.h"

// Sets default values
ALootManager::ALootManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

void ALootManager::SpawnLoot()
{
	//Get seed from Dungeon Gen
	ADungeonGenerator* DungeonGen = Cast<ADungeonGenerator>(UGameplayStatics::GetActorOfClass(GetWorld(), ADungeonGenerator::StaticClass()));
	RandStream.Initialize(DungeonGen->Seed + DungeonGen->Floor);

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

	if (itemPool.Num() > 0)
	{
		item = itemPool[RandStream.RandRange(0, itemPool.Num() - 1)];
	}
	return item;
}

