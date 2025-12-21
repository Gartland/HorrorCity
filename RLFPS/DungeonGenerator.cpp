// DungeonGenerator.cpp
#include "DungeonGenerator.h"
//#include "RoomActor.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"

// Initialize static member
ADungeonGenerator* ADungeonGenerator::Main = nullptr;

ADungeonGenerator::ADungeonGenerator()
{
  PrimaryActorTick.bCanEverTick = false;
  RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
}

void ADungeonGenerator::BeginPlay()
{
  Super::BeginPlay();
  Main = this;
  GenerateDungeon();
}

void ADungeonGenerator::NextLevel()
{
  CellCount += 3;
  EnemyCount = CellCount / 10;
  GenerateDungeon();

  // Reset player position (you'll need to implement this based on your player setup)
  APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
  if (PlayerPawn)
  {
    PlayerPawn->SetActorLocation(FVector(100, 100, 100));
  }
}

void ADungeonGenerator::GenerateDungeon()
{
  // Clear previous dungeon
  ClearDungeon();

  // Reset data structures
  OccupiedCells.Empty();
  AvailablePositions.Empty();
  RoomMap.Empty();
  ConnectedDoors.Empty();
  SpawnedObjects.Empty();
  LockedArea.Empty();
  AccessibleArea.Empty();

  // Start with the first room at origin
  FIntPoint StartPos(0, 0);
  SpawnRoom(StartPos);
  OccupiedCells.Add(StartPos);
  AddAdjacentPositions(StartPos);

  // Generate remaining rooms
  for (int32 i = 1; i < CellCount; i++)
  {
    if (AvailablePositions.Num() == 0)
    {
      UE_LOG(LogTemp, Warning, TEXT("No more available positions for rooms!"));
      break;
    }

    // Pick a random available position
    int32 RandomIndex = FMath::RandRange(0, AvailablePositions.Num() - 1);
    FIntPoint NewPos = AvailablePositions[RandomIndex];

    // Spawn the room
    SpawnRoom(NewPos);
    OccupiedCells.Add(NewPos);
    AvailablePositions.RemoveAt(RandomIndex);

    // Add new adjacent positions
    AddAdjacentPositions(NewPos);
  }

  // First create minimal connections to ensure all rooms are reachable
  CreateMinimalConnections();

  // Create locked area and restrict its connections
  CreateLockedArea();

  // Calculate accessible area
  CalculateAccessibleArea();

  // Set up all doors and windows
  SetupDoorsAndWindows();

  // Spawn the locked door
  SpawnLockedDoor();

  // Spawn the key
  SpawnKey();

  // Spawn enemies and treasure
  SpawnObjectsInFarRooms();

  // Rebuild navigation mesh
  RebuildNavigation();
}

void ADungeonGenerator::ClearDungeon()
{
  // Destroy all active rooms
  for (AActor* Room : ActiveDungeonRooms)
  {
    if (Room && IsValid(Room))
    {
      Room->Destroy();
    }
  }
  ActiveDungeonRooms.Empty();

  // Destroy all spawned objects
  for (AActor* Obj : SpawnedObjects)
  {
    if (Obj && IsValid(Obj))
    {
      Obj->Destroy();
    }
  }
  SpawnedObjects.Empty();

  // Reset data structures
  OccupiedCells.Empty();
  AvailablePositions.Empty();
  RoomMap.Empty();
  ConnectedDoors.Empty();
  LockedArea.Empty();
  AccessibleArea.Empty();
}

void ADungeonGenerator::SpawnRoom(FIntPoint GridPos)
{
  if (!RoomClass)
  {
    UE_LOG(LogTemp, Error, TEXT("RoomClass is not set!"));
    return;
  }

  FVector SpawnLocation(GridPos.X * CellSize, GridPos.Y * CellSize, 0.0f);
  FRotator SpawnRotation(0.0f, 0.0f, 0.0f);
  FActorSpawnParameters SpawnParams;
  SpawnParams.Owner = this;

  AActor* RoomInstance = GetWorld()->SpawnActor<AActor>(RoomClass, SpawnLocation, SpawnRotation, SpawnParams);

  if (RoomInstance)
  {
    ActiveDungeonRooms.Add(RoomInstance);
    RoomMap.Add(GridPos, RoomInstance);
  }
}

void ADungeonGenerator::CreateLockedArea()
{
  if (OccupiedCells.Num() < 5) return;

  // Find the farthest room from origin
  FIntPoint FarthestRoom(0, 0);
  int32 MaxDistance = 0;

  for (const FIntPoint& Pos : OccupiedCells)
  {
    int32 Distance = FMath::Abs(Pos.X) + FMath::Abs(Pos.Y);
    if (Distance > MaxDistance)
    {
      MaxDistance = Distance;
      FarthestRoom = Pos;
    }
  }

  // Calculate target locked rooms
  int32 TargetLockedRooms = FMath::Max(2, FMath::CeilToInt(OccupiedCells.Num() * LockedAreaSizePercent));

  // Flood fill from farthest room
  TQueue<FIntPoint> Queue;
  LockedArea.Add(FarthestRoom);
  Queue.Enqueue(FarthestRoom);

  while (!Queue.IsEmpty() && LockedArea.Num() < TargetLockedRooms)
  {
    FIntPoint Current;
    Queue.Dequeue(Current);

    TArray<FIntPoint> Neighbors = {
        FIntPoint(Current.X, Current.Y + 1),
        FIntPoint(Current.X + 1, Current.Y),
        FIntPoint(Current.X, Current.Y - 1),
        FIntPoint(Current.X - 1, Current.Y)
    };

    // Shuffle neighbors
    for (int32 i = Neighbors.Num() - 1; i > 0; i--)
    {
      int32 j = FMath::RandRange(0, i);
      Neighbors.Swap(i, j);
    }

    for (const FIntPoint& Neighbor : Neighbors)
    {
      if (OccupiedCells.Contains(Neighbor) && !LockedArea.Contains(Neighbor))
      {
        LockedArea.Add(Neighbor);
        Queue.Enqueue(Neighbor);

        if (LockedArea.Num() >= TargetLockedRooms)
          break;
      }
    }
  }

  RemoveLockedAreaConnections();
  CreateSingleLockedConnection();
}

void ADungeonGenerator::RemoveLockedAreaConnections()
{
  TArray<FString> ConnectionsToRemove;

  for (const FIntPoint& LockedRoom : LockedArea)
  {
    TArray<FIntPoint> Neighbors = {
        FIntPoint(LockedRoom.X, LockedRoom.Y + 1),
        FIntPoint(LockedRoom.X + 1, LockedRoom.Y),
        FIntPoint(LockedRoom.X, LockedRoom.Y - 1),
        FIntPoint(LockedRoom.X - 1, LockedRoom.Y)
    };

    for (const FIntPoint& Neighbor : Neighbors)
    {
      if (OccupiedCells.Contains(Neighbor) && !LockedArea.Contains(Neighbor))
      {
        FString Key = GetConnectionKey(LockedRoom, Neighbor);
        ConnectionsToRemove.Add(Key);
      }
    }
  }

  for (const FString& Key : ConnectionsToRemove)
  {
    ConnectedDoors.Remove(Key);
  }
}

void ADungeonGenerator::CreateSingleLockedConnection()
{
  struct FConnection
  {
    FIntPoint Locked;
    FIntPoint Unlocked;
    ERoomDirection Dir;
  };

  TArray<FConnection> PossibleConnections;

  for (const FIntPoint& LockedRoom : LockedArea)
  {
    TArray<FIntPoint> Neighbors = {
        FIntPoint(LockedRoom.X, LockedRoom.Y + 1),
        FIntPoint(LockedRoom.X + 1, LockedRoom.Y),
        FIntPoint(LockedRoom.X, LockedRoom.Y - 1),
        FIntPoint(LockedRoom.X - 1, LockedRoom.Y)
    };

    TArray<ERoomDirection> Directions = {
        ERoomDirection::NORTH,
        ERoomDirection::EAST,
        ERoomDirection::SOUTH,
        ERoomDirection::WEST
    };

    for (int32 i = 0; i < Neighbors.Num(); i++)
    {
      if (OccupiedCells.Contains(Neighbors[i]) && !LockedArea.Contains(Neighbors[i]))
      {
        FConnection Connection;
        Connection.Locked = LockedRoom;
        Connection.Unlocked = Neighbors[i];
        Connection.Dir = Directions[i];
        PossibleConnections.Add(Connection);
      }
    }
  }

  if (PossibleConnections.Num() > 0)
  {
    FConnection ChosenConnection = PossibleConnections[FMath::RandRange(0, PossibleConnections.Num() - 1)];
    LockedDoorPos1 = ChosenConnection.Locked;
    LockedDoorPos2 = ChosenConnection.Unlocked;
    LockedDoorDirection = ChosenConnection.Dir;

    FString ChosenKey = GetConnectionKey(LockedDoorPos1, LockedDoorPos2);
    ConnectedDoors.Add(ChosenKey);
  }
}

void ADungeonGenerator::CalculateAccessibleArea()
{
  AccessibleArea.Empty();
  TQueue<FIntPoint> Queue;

  FIntPoint Start(0, 0);
  AccessibleArea.Add(Start);
  Queue.Enqueue(Start);

  while (!Queue.IsEmpty())
  {
    FIntPoint Current;
    Queue.Dequeue(Current);

    TArray<FIntPoint> Neighbors = {
        FIntPoint(Current.X, Current.Y + 1),
        FIntPoint(Current.X + 1, Current.Y),
        FIntPoint(Current.X, Current.Y - 1),
        FIntPoint(Current.X - 1, Current.Y)
    };

    for (const FIntPoint& Neighbor : Neighbors)
    {
      if (OccupiedCells.Contains(Neighbor) && !AccessibleArea.Contains(Neighbor))
      {
        FString ConnectionKey = GetConnectionKey(Current, Neighbor);

        // Skip the locked door
        if (ConnectionKey == GetConnectionKey(LockedDoorPos1, LockedDoorPos2))
          continue;

        if (ConnectedDoors.Contains(ConnectionKey))
        {
          AccessibleArea.Add(Neighbor);
          Queue.Enqueue(Neighbor);
        }
      }
    }
  }
}

void ADungeonGenerator::SpawnLockedDoor()
{
  if (!LockedDoorPrefabClass || !RoomMap.Contains(LockedDoorPos1)) return;

  FVector LockedRoomWorld(LockedDoorPos1.X * CellSize, LockedDoorPos1.Y * CellSize, 0.0f);
  FVector UnlockedRoomWorld(LockedDoorPos2.X * CellSize, LockedDoorPos2.Y * CellSize, 0.0f);
  FVector DoorPosition = (LockedRoomWorld + UnlockedRoomWorld) / 2.0f;

  FRotator DoorRotation(0.0f, 0.0f, 0.0f);
  switch (LockedDoorDirection)
  {
  case ERoomDirection::NORTH:
    DoorRotation = FRotator(0, 0, 0);
    break;
  case ERoomDirection::EAST:
    DoorRotation = FRotator(0, 90, 0);
    break;
  case ERoomDirection::SOUTH:
    DoorRotation = FRotator(0, 180, 0);
    break;
  case ERoomDirection::WEST:
    DoorRotation = FRotator(0, 270, 0);
    break;
  }

  AActor* LockedDoor = GetWorld()->SpawnActor<AActor>(LockedDoorPrefabClass, DoorPosition, DoorRotation);
  if (LockedDoor)
  {
    SpawnedObjects.Add(LockedDoor);
  }
}

void ADungeonGenerator::SpawnKey()
{
  if (!KeyPrefabClass || AccessibleArea.Num() == 0) return;

  FVector DoorWorldPos(
    (LockedDoorPos1.X + LockedDoorPos2.X) * CellSize / 2.0f,
    (LockedDoorPos1.Y + LockedDoorPos2.Y) * CellSize / 2.0f,
    0.0f
  );

  // Find farthest accessible room from door
  FIntPoint KeyRoom(0, 0);
  float MaxDistance = 0.0f;

  for (const FIntPoint& Pos : AccessibleArea)
  {
    FVector RoomWorldPos(Pos.X * CellSize, Pos.Y * CellSize, 0.0f);
    float Distance = FVector::Dist(RoomWorldPos, DoorWorldPos);
    if (Distance > MaxDistance)
    {
      MaxDistance = Distance;
      KeyRoom = Pos;
    }
  }

  FVector KeyPosition(KeyRoom.X * CellSize, KeyRoom.Y * CellSize, 0.0f);
  AActor* Key = GetWorld()->SpawnActor<AActor>(KeyPrefabClass, KeyPosition, FRotator::ZeroRotator);
  if (Key)
  {
    SpawnedObjects.Add(Key);
  }

  UE_LOG(LogTemp, Log, TEXT("Key spawned at (%d, %d), distance from door: %f"), KeyRoom.X, KeyRoom.Y, MaxDistance);
}

void ADungeonGenerator::SpawnObjectsInFarRooms()
{
  if (OccupiedCells.Num() == 0) return;

  // Sort rooms by distance from origin
  TArray<FIntPoint> RoomsByDistance = OccupiedCells.Array();
  RoomsByDistance.Sort([](const FIntPoint& A, const FIntPoint& B) {
    return (FMath::Abs(A.X) + FMath::Abs(A.Y)) > (FMath::Abs(B.X) + FMath::Abs(B.Y));
    });

  int32 FarRoomCount = FMath::Max(1, FMath::CeilToInt(RoomsByDistance.Num() * 0.3f));
  TArray<FIntPoint> FarRooms;
  for (int32 i = 0; i < FarRoomCount && i < RoomsByDistance.Num(); i++)
  {
    FarRooms.Add(RoomsByDistance[i]);
  }

  // Spawn enemies
  if (EnemyPrefabClass)
  {
    for (int32 i = 0; i < EnemyCount && i < FarRooms.Num(); i++)
    {
      FVector WorldPos(FarRooms[i].X * CellSize, FarRooms[i].Y * CellSize, 0.0f);
      AActor* Enemy = GetWorld()->SpawnActor<AActor>(EnemyPrefabClass, WorldPos, FRotator::ZeroRotator);
      if (Enemy)
      {
        SpawnedObjects.Add(Enemy);
      }
    }
  }

  // Spawn treasure
  if (TreasurePrefabClass)
  {
    TArray<FIntPoint> TreasureRooms = FarRooms;
    for (int32 i = TreasureRooms.Num() - 1; i > 0; i--)
    {
      int32 j = FMath::RandRange(0, i);
      TreasureRooms.Swap(i, j);
    }

    for (int32 i = 0; i < TreasureCount && i < TreasureRooms.Num(); i++)
    {
      FVector WorldPos(TreasureRooms[i].X * CellSize, TreasureRooms[i].Y * CellSize, 0.0f);
      AActor* Treasure = GetWorld()->SpawnActor<AActor>(TreasurePrefabClass, WorldPos, FRotator::ZeroRotator);
      if (Treasure)
      {
        SpawnedObjects.Add(Treasure);
      }
    }
  }
}

void ADungeonGenerator::SetupDoorsAndWindows()
{
  int32 MinX = INT32_MAX, MaxX = INT32_MIN;
  int32 MinZ = INT32_MAX, MaxZ = INT32_MIN;

  for (const FIntPoint& Pos : OccupiedCells)
  {
    if (Pos.X < MinX) MinX = Pos.X;
    if (Pos.X > MaxX) MaxX = Pos.X;
    if (Pos.Y < MinZ) MinZ = Pos.Y;
    if (Pos.Y > MaxZ) MaxZ = Pos.Y;
  }

  AddExtraDoors();

  for (const auto& Pair : RoomMap)
  {
    FIntPoint Pos = Pair.Key;
    AActor* CurrentRoom = Pair.Value;

    CheckAndSetDoorOrWindow(CurrentRoom, Pos, FIntPoint(0, 1), ERoomDirection::NORTH, Pos.Y >= MaxZ - 1, MinX, MaxX, MinZ, MaxZ);
    CheckAndSetDoorOrWindow(CurrentRoom, Pos, FIntPoint(1, 0), ERoomDirection::EAST, Pos.X >= MaxX - 1, MinX, MaxX, MinZ, MaxZ);
    CheckAndSetDoorOrWindow(CurrentRoom, Pos, FIntPoint(0, -1), ERoomDirection::SOUTH, Pos.Y <= MinZ + 1, MinX, MaxX, MinZ, MaxZ);
    CheckAndSetDoorOrWindow(CurrentRoom, Pos, FIntPoint(-1, 0), ERoomDirection::WEST, Pos.X <= MinX + 1, MinX, MaxX, MinZ, MaxZ);
  }
}

void ADungeonGenerator::CreateMinimalConnections()
{
  TSet<FIntPoint> Visited;
  TQueue<FIntPoint> Queue;

  FIntPoint Start(0, 0);
  Visited.Add(Start);
  Queue.Enqueue(Start);

  while (!Queue.IsEmpty())
  {
    FIntPoint Current;
    Queue.Dequeue(Current);

    TArray<FIntPoint> Neighbors = {
        FIntPoint(Current.X, Current.Y + 1),
        FIntPoint(Current.X + 1, Current.Y),
        FIntPoint(Current.X, Current.Y - 1),
        FIntPoint(Current.X - 1, Current.Y)
    };

    for (const FIntPoint& Neighbor : Neighbors)
    {
      if (OccupiedCells.Contains(Neighbor) && !Visited.Contains(Neighbor))
      {
        Visited.Add(Neighbor);
        Queue.Enqueue(Neighbor);

        FString ConnectionKey = GetConnectionKey(Current, Neighbor);
        ConnectedDoors.Add(ConnectionKey);
      }
    }
  }
}

void ADungeonGenerator::AddExtraDoors()
{
  for (const FIntPoint& Pos : OccupiedCells)
  {
    TArray<FIntPoint> Neighbors = {
        FIntPoint(Pos.X, Pos.Y + 1),
        FIntPoint(Pos.X + 1, Pos.Y),
        FIntPoint(Pos.X, Pos.Y - 1),
        FIntPoint(Pos.X - 1, Pos.Y)
    };

    for (const FIntPoint& Neighbor : Neighbors)
    {
      if (OccupiedCells.Contains(Neighbor))
      {
        FString ConnectionKey = GetConnectionKey(Pos, Neighbor);

        bool bIsLockedBoundary = (LockedArea.Contains(Pos) && !LockedArea.Contains(Neighbor)) ||
          (!LockedArea.Contains(Pos) && LockedArea.Contains(Neighbor));

        if (!bIsLockedBoundary && !ConnectedDoors.Contains(ConnectionKey) && FMath::FRand() < ExtraDoorChance)
        {
          ConnectedDoors.Add(ConnectionKey);
        }
      }
    }
  }
}

FString ADungeonGenerator::GetConnectionKey(FIntPoint Pos1, FIntPoint Pos2) const
{
  if (Pos1.X < Pos2.X || (Pos1.X == Pos2.X && Pos1.Y < Pos2.Y))
  {
    return FString::Printf(TEXT("%d,%d-%d,%d"), Pos1.X, Pos1.Y, Pos2.X, Pos2.Y);
  }
  else
  {
    return FString::Printf(TEXT("%d,%d-%d,%d"), Pos2.X, Pos2.Y, Pos1.X, Pos1.Y);
  }
}

bool ADungeonGenerator::HasDoorConnection(FIntPoint Pos1, FIntPoint Pos2) const
{
  FString ConnectionKey = GetConnectionKey(Pos1, Pos2);
  return ConnectedDoors.Contains(ConnectionKey);
}

void ADungeonGenerator::CheckAndSetDoorOrWindow(AActor* CurrentRoom, FIntPoint Pos, FIntPoint Direction,
  ERoomDirection DoorDirection, bool bIsNearPerimeter, int32 MinX, int32 MaxX, int32 MinZ, int32 MaxZ)
{
  if (!CurrentRoom) return;

  FIntPoint AdjacentPos = Pos + Direction;

  // You'll need to implement this based on your Room actor's interface
  // This would call a function on your room blueprint/actor to set doors/windows
  // Example (assuming you have a ARoomActor class):
  // ARoomActor* RoomActor = Cast<ARoomActor>(CurrentRoom);
  // if (RoomActor)
  // {
  //     if (OccupiedCells.Contains(AdjacentPos))
  //     {
  //         bool bHasDoor = HasDoorConnection(Pos, AdjacentPos);
  //         RoomActor->SetDoor(DoorDirection, bHasDoor);
  //     }
  //     else
  //     {
  //         RoomActor->SetDoor(DoorDirection, false);
  //         if (bIsNearPerimeter)
  //         {
  //             RoomActor->SetWindow(DoorDirection, true);
  //         }
  //     }
  // }
}

void ADungeonGenerator::AddAdjacentPositions(FIntPoint Pos)
{
  TArray<FIntPoint> Directions = {
      FIntPoint(0, 1),   // North
      FIntPoint(0, -1),  // South
      FIntPoint(-1, 0),  // West
      FIntPoint(1, 0)    // East
  };

  for (const FIntPoint& Dir : Directions)
  {
    FIntPoint AdjacentPos = Pos + Dir;

    if (AdjacentPos.Y >= 0 && !OccupiedCells.Contains(AdjacentPos) && !AvailablePositions.Contains(AdjacentPos))
    {
      AvailablePositions.Add(AdjacentPos);
    }
  }
}

void ADungeonGenerator::RebuildNavigation()
{
  UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
  if (NavSys)
  {
    NavSys->Build();
  }
}