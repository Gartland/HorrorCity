// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LootManager.generated.h"

USTRUCT(BlueprintType)
struct FLootPool
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	int32 SpawnWeight;

	UPROPERTY(EditAnywhere)
	TArray<TSubclassOf<AActor>> ItemBPs;

};

UCLASS()
class HORRORCITY_API ALootManager : public AActor
{
	GENERATED_BODY()
	
	public:	
		// Sets default values for this actor's properties
		ALootManager();

		UPROPERTY(EditAnywhere)
		TSubclassOf<AActor> ItemBaseClass;

		UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TArray<FLootPool> LootPools;

		UFUNCTION(BlueprintCallable, Category = "Dungeon Generation")
		void SpawnLoot();

	private:
		FRandomStream RandStream;
		TSubclassOf<AActor> GetRandomItem();

};
