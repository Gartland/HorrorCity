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

  // Public properties (like Unity's [SerializeField])
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation")
  float CellSize = 1000.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation", meta = (ClampMin = "5", ClampMax = "100"))
  int32 CellCount = 15;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
  float EnemiesPerRoom = 0.3f;

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

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation")
  TSubclassOf<AActor> SafeRoomClass;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation")
  TSubclassOf<AActor> KeyRoomClass;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Generation")
  TSubclassOf<AActor> LadderRoomClass;

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
  TMap<FIntPoint, int32> RoomDepthMap;
  FIntPoint KeyRoomPos;
  FIntPoint LadderRoomPos;

  // Helper functions
  void SpawnAllRooms();
  void SpawnRoom(FIntPoint GridPos);
  void AddAdjacentPositions(FIntPoint Pos);
  void CreateMinimalConnections();
  void AddExtraDoors();
  void CreateLockedArea();
  void RemoveLockedAreaConnections();
  void CreateSingleLockedConnection();
  void CalculateAccessibleArea();
  void SpawnLockedDoor();
  void SpawnSafeRoom(FIntPoint Pos);
  void SpawnObjectsInFarRooms();
  FString GetConnectionKey(FIntPoint Pos1, FIntPoint Pos2) const;
  bool HasDoorConnection(FIntPoint Pos1, FIntPoint Pos2) const;
  void RebuildNavigation();
  // Advanced gen functions
  void CalculateRoomDepths();
  void FindKeyAndLadderRooms();
  void SpawnKeyRoom(FIntPoint GridPos);
  void SpawnLadderRoom(FIntPoint GridPos);
  void SpawnEnemiesWithPacing();
  void SpawnEnemyInRoom(FIntPoint RoomPos, int32 Count);
  void SpawnLootWithPacing();
  bool IsAdjacentToSafeRoom(FIntPoint Pos);

  // Room selection helpers
  TSubclassOf<AActor> GetRandomClass(const TArray<TSubclassOf<AActor>>& ClassArray);
  FRotator GetDeadendRotation(ERoomDirection OpenDir);
  FRotator GetStraightRotation(ERoomDirection FirstDir);
  FRotator GetTurnRotation(ERoomDirection Dir1, ERoomDirection Dir2);
  FRotator GetTJunctionRotation(const TArray<ERoomDirection>& OpenDirs);
  bool IsOpposite(ERoomDirection Dir1, ERoomDirection Dir2);
  FVector GetRotationOffset(float YawRotation);
};