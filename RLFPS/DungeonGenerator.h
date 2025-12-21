// DungeonGenerator.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "DungeonGenerator.generated.h"

UENUM(BlueprintType)
enum class ERoomDirection : uint8
{
  NORTH UMETA(DisplayName = "North"),
  EAST UMETA(DisplayName = "East"),
  SOUTH UMETA(DisplayName = "South"),
  WEST UMETA(DisplayName = "West")
};

UCLASS()
class RLFPS_API ADungeonGenerator : public AActor
{
  GENERATED_BODY()

public:
  ADungeonGenerator();

  // Static instance reference (like Unity's singleton)
  static ADungeonGenerator* Main;

  // Public properties (like Unity's [SerializeField])
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation")
  TSubclassOf<AActor> RoomClass;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation")
  float CellSize = 1000.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation", meta = (ClampMin = "5", ClampMax = "100"))
  int32 CellCount = 15;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
  float ExtraDoorChance = 0.3f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation")
  TSubclassOf<AActor> EnemyPrefabClass;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation")
  TSubclassOf<AActor> TreasurePrefabClass;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation")
  TSubclassOf<AActor> KeyPrefabClass;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation")
  TSubclassOf<AActor> LockedDoorPrefabClass;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation")
  int32 EnemyCount = 3;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation")
  int32 TreasureCount = 2;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation", meta = (ClampMin = "0.2", ClampMax = "0.5"))
  float LockedAreaSizePercent = 0.3f;

  // Public functions
  UFUNCTION(BlueprintCallable, Category = "Dungeon Generation")
  void GenerateDungeon();

  UFUNCTION(BlueprintCallable, Category = "Dungeon Generation")
  void ClearDungeon();

  UFUNCTION(BlueprintCallable, Category = "Dungeon Generation")
  void NextLevel();

protected:
  virtual void BeginPlay() override;

private:
  // Data structures
  TSet<FIntPoint> OccupiedCells;
  TArray<FIntPoint> AvailablePositions;
  TArray<AActor*> ActiveDungeonRooms;
  TMap<FIntPoint, AActor*> RoomMap;
  TSet<FString> ConnectedDoors;
  TArray<AActor*> SpawnedObjects;
  TSet<FIntPoint> LockedArea;
  TSet<FIntPoint> AccessibleArea;
  FIntPoint LockedDoorPos1;
  FIntPoint LockedDoorPos2;
  ERoomDirection LockedDoorDirection;

  // Helper functions
  void SpawnRoom(FIntPoint GridPos);
  void AddAdjacentPositions(FIntPoint Pos);
  void CreateMinimalConnections();
  void AddExtraDoors();
  void CreateLockedArea();
  void RemoveLockedAreaConnections();
  void CreateSingleLockedConnection();
  void CalculateAccessibleArea();
  void SetupDoorsAndWindows();
  void SpawnLockedDoor();
  void SpawnKey();
  void SpawnObjectsInFarRooms();
  void CheckAndSetDoorOrWindow(AActor* CurrentRoom, FIntPoint Pos, FIntPoint Direction,
    ERoomDirection DoorDirection, bool bIsNearPerimeter, int32 MinX, int32 MaxX, int32 MinZ, int32 MaxZ);

  FString GetConnectionKey(FIntPoint Pos1, FIntPoint Pos2) const;
  bool HasDoorConnection(FIntPoint Pos1, FIntPoint Pos2) const;
  void RebuildNavigation();
};
