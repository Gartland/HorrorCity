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
class HORRORCITY_API ADungeonGenerator : public AActor
{
  GENERATED_BODY()

public:
  ADungeonGenerator();

  // Room type arrays
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation|Room Types")
  TArray<TSubclassOf<AActor>> DeadendRooms;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation|Room Types")
  TArray<TSubclassOf<AActor>> StraightRooms;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation|Room Types")
  TArray<TSubclassOf<AActor>> TurnRooms;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation|Room Types")
  TArray<TSubclassOf<AActor>> TJunctionRooms;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation|Room Types")
  TArray<TSubclassOf<AActor>> CrossroadRooms;

  UPROPERTY(EditAnywhere, Category = "Dungeon Generation")
  TSubclassOf<AActor> SafeRoom;

  UPROPERTY(EditAnywhere, Category = "Dungeon Generation")
  TSubclassOf<AActor> EndRoomClass;

  UPROPERTY(EditAnywhere, Category = "Dungeon Generation")
  TSubclassOf<AActor> KeyRoomClass;

  UPROPERTY(EditAnywhere, Category = "Dungeon Generation")
  TSubclassOf<AActor> BossFloorClass;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation")
  float CellSize = 1000.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation")
  int32 Floor = 1;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation", meta = (ClampMin = "5", ClampMax = "100"))
  int32 CellCount = 15;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation")
  int32 FloorsPerBoss = 5;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
  float EnemiesPerRoom = 0.3f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
  float ExtraDoorChance = 0.3f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation")
  TSubclassOf<AActor> EnemyPrefabClass;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation")
  TSubclassOf<AActor> LockedDoorPrefabClass;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation")
  int32 EnemyCount = 3;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation", meta = (ClampMin = "0.2", ClampMax = "0.5"))
  float LockedAreaSizePercent = 0.3f;

  // Public functions
  UFUNCTION(BlueprintCallable, Category = "Dungeon Generation")
  void GenerateDungeon();

  UFUNCTION(BlueprintCallable, Category = "Dungeon Generation")
  void ClearDungeon();

  UFUNCTION(BlueprintCallable, Category = "Dungeon Generation")
  void NextLevel();

  void SpawnBossFloor();

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
  FIntPoint SafeRoomGridPos;
  FIntPoint EndRoomGridPos;
  FIntPoint KeyRoomGridPos;
  FIntPoint LockedDoorPos1;
  FIntPoint LockedDoorPos2;
  ERoomDirection LockedDoorDirection;

  // Helper functions
  void SpawnAllRooms();
  void SpawnRoom(FIntPoint GridPos);
  void SpawnSpecialRoom(TSubclassOf<AActor> RoomClass, FIntPoint GridPos);
  void AddAdjacentPositions(FIntPoint Pos);
  void CreateMinimalConnections();
  void AddExtraDoors();
  void CreateLockedArea();
  void RemoveLockedAreaConnections();
  void CreateSingleLockedConnection();
  void CalculateAccessibleArea();
  void SpawnLockedDoor();
  void PlaceKeyRoom();
  void SpawnObjectsInFarRooms();
  FString GetConnectionKey(FIntPoint Pos1, FIntPoint Pos2) const;
  bool HasDoorConnection(FIntPoint Pos1, FIntPoint Pos2) const;
  void RebuildNavigation();

  // Room selection helpers
  TSubclassOf<AActor> GetRandomClass(const TArray<TSubclassOf<AActor>>& ClassArray);
  FRotator GetDeadendRotation(ERoomDirection OpenDir);
  FRotator GetStraightRotation(ERoomDirection FirstDir);
  FRotator GetTurnRotation(ERoomDirection Dir1, ERoomDirection Dir2);
  FRotator GetTJunctionRotation(const TArray<ERoomDirection>& OpenDirs);
  bool IsOpposite(ERoomDirection Dir1, ERoomDirection Dir2);
  FVector GetRotationOffset(float YawRotation);
};