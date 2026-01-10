// DungeonGenerator.cpp
#include "DungeonGenerator.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"

ADungeonGenerator::ADungeonGenerator()
{
  PrimaryActorTick.bCanEverTick = false;
  RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
}

void ADungeonGenerator::BeginPlay()
{
  Super::BeginPlay();
  GenerateDungeon();
}

void ADungeonGenerator::NextLevel()
{
  CellCount += 3;
  EnemyCount = CellCount / 10;
  GenerateDungeon();

  APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
  if (PlayerPawn)
  {
    PlayerPawn->SetActorLocation(FVector(100, 100, 100));
  }
}

void ADungeonGenerator::GenerateDungeon()
{
  ClearDungeon();

  OccupiedCells.Empty();
  AvailablePositions.Empty();
  RoomMap.Empty();
  ConnectedDoors.Empty();
  SpawnedObjects.Empty();
  LockedArea.Empty();
  AccessibleArea.Empty();

  // Generate room positions (but don't spawn yet)
  FIntPoint StartPos(0, 0);
  OccupiedCells.Add(StartPos);
  AddAdjacentPositions(StartPos);

  // Force room 1 to be a dead end by only adding one neighbor initially
  if (AvailablePositions.Num() > 0)
  {
    int32 RandomIndex = FMath::RandRange(0, AvailablePositions.Num() - 1);
    FIntPoint FirstRoom = AvailablePositions[RandomIndex];
    OccupiedCells.Add(FirstRoom);
    AvailablePositions.RemoveAt(RandomIndex);
    AddAdjacentPositions(FirstRoom);
  }

  for (int32 i = 2; i < CellCount; i++)
  {
    if (AvailablePositions.Num() == 0)
    {
      UE_LOG(LogTemp, Warning, TEXT("No more available positions for rooms!"));
      break;
    }

    int32 RandomIndex = FMath::RandRange(0, AvailablePositions.Num() - 1);
    FIntPoint NewPos = AvailablePositions[RandomIndex];

    OccupiedCells.Add(NewPos);
    AvailablePositions.RemoveAt(RandomIndex);
    AddAdjacentPositions(NewPos);
  }

  // Create all connections first
  CreateMinimalConnections();
  AddExtraDoors();  // Move this BEFORE CreateLockedArea
  CreateLockedArea();
  CalculateAccessibleArea();

  // NOW spawn rooms based on connectivity
  SpawnAllRooms();

  // Setup doors and spawn objects
  SetupDoorsAndWindows();
  SpawnLockedDoor();
  SpawnKey();
  SpawnObjectsInFarRooms();
  RebuildNavigation();
}

void ADungeonGenerator::SpawnAllRooms()
{
  // Spawn origin room first to ensure it's Room 0
  FIntPoint Origin(0, 0);
  if (OccupiedCells.Contains(Origin))
  {
    SpawnRoom(Origin);
  }

  // Spawn remaining rooms
  for (const FIntPoint& GridPos : OccupiedCells)
  {
    if (GridPos != Origin && !RoomMap.Contains(GridPos))
    {
      SpawnRoom(GridPos);
    }
  }
}

void ADungeonGenerator::SpawnRoom(FIntPoint GridPos)
{
  // Determine which directions have connections
  TArray<ERoomDirection> OpenDirections;

  if (HasDoorConnection(GridPos, GridPos + FIntPoint(0, 1)))
    OpenDirections.Add(ERoomDirection::SOUTH);
  if (HasDoorConnection(GridPos, GridPos + FIntPoint(1, 0)))
    OpenDirections.Add(ERoomDirection::EAST);
  if (HasDoorConnection(GridPos, GridPos + FIntPoint(0, -1)))
    OpenDirections.Add(ERoomDirection::NORTH);
  if (HasDoorConnection(GridPos, GridPos + FIntPoint(-1, 0)))
    OpenDirections.Add(ERoomDirection::WEST);

  // Select appropriate room class and rotation
  TSubclassOf<AActor> SelectedClass = nullptr;
  FRotator SpawnRotation = FRotator::ZeroRotator;

  int32 ConnectionCount = OpenDirections.Num();

  if (ConnectionCount == 1)
  {
    SelectedClass = GetRandomClass(DeadendRooms);
    SpawnRotation = GetDeadendRotation(OpenDirections[0]);
  }
  else if (ConnectionCount == 2)
  {
    if (IsOpposite(OpenDirections[0], OpenDirections[1]))
    {
      SelectedClass = GetRandomClass(StraightRooms);
      SpawnRotation = GetStraightRotation(OpenDirections[0]);
    }
    else
    {
      SelectedClass = GetRandomClass(TurnRooms);
      SpawnRotation = GetTurnRotation(OpenDirections[0], OpenDirections[1]);
    }
  }
  else if (ConnectionCount == 3)
  {
    SelectedClass = GetRandomClass(TJunctionRooms);
    SpawnRotation = GetTJunctionRotation(OpenDirections);
  }
  else if (ConnectionCount == 4)
  {
    SelectedClass = GetRandomClass(CrossroadRooms);
  }

  if (!SelectedClass)
  {
    UE_LOG(LogTemp, Error, TEXT("No room class available for position (%d, %d) with %d connections"),
      GridPos.X, GridPos.Y, ConnectionCount);
    return;
  }

  // Offset to center the room (pivot is at northwest corner)
  FVector SpawnLocation(GridPos.X * CellSize + CellSize / 2.0f, GridPos.Y * CellSize + CellSize / 2.0f, 0.0f);
  FActorSpawnParameters SpawnParams;
  SpawnParams.Owner = this;

  AActor* RoomInstance = GetWorld()->SpawnActor<AActor>(SelectedClass, SpawnLocation, SpawnRotation, SpawnParams);

  if (RoomInstance)
  {
    ActiveDungeonRooms.Add(RoomInstance);
    RoomMap.Add(GridPos, RoomInstance);
  }
}

TSubclassOf<AActor> ADungeonGenerator::GetRandomClass(const TArray<TSubclassOf<AActor>>& ClassArray)
{
  if (ClassArray.Num() == 0) return nullptr;
  return ClassArray[FMath::RandRange(0, ClassArray.Num() - 1)];
}

FRotator ADungeonGenerator::GetDeadendRotation(ERoomDirection OpenDir)
{
  switch (OpenDir)
  {
  case ERoomDirection::NORTH: return FRotator(0, 0, 0);
  case ERoomDirection::EAST:  return FRotator(0, 90, 0);
  case ERoomDirection::SOUTH: return FRotator(0, 180, 0);
  case ERoomDirection::WEST:  return FRotator(0, 270, 0);
  }
  return FRotator::ZeroRotator;
}

FRotator ADungeonGenerator::GetStraightRotation(ERoomDirection FirstDir)
{
  if (FirstDir == ERoomDirection::NORTH || FirstDir == ERoomDirection::SOUTH)
    return FRotator(0, 0, 0);  // North-South
  else
    return FRotator(0, 90, 0); // East-West
}

FRotator ADungeonGenerator::GetTurnRotation(ERoomDirection Dir1, ERoomDirection Dir2)
{
  // Assuming turn rooms are designed with openings at North and East by default
  bool bHasNorth = (Dir1 == ERoomDirection::NORTH || Dir2 == ERoomDirection::NORTH);
  bool bHasEast = (Dir1 == ERoomDirection::EAST || Dir2 == ERoomDirection::EAST);
  bool bHasSouth = (Dir1 == ERoomDirection::SOUTH || Dir2 == ERoomDirection::SOUTH);
  bool bHasWest = (Dir1 == ERoomDirection::WEST || Dir2 == ERoomDirection::WEST);

  if (bHasNorth && bHasEast)  return FRotator(0, 0, 0);
  if (bHasEast && bHasSouth)  return FRotator(0, 90, 0);
  if (bHasSouth && bHasWest)  return FRotator(0, 180, 0);
  if (bHasWest && bHasNorth)  return FRotator(0, 270, 0);

  return FRotator::ZeroRotator;
}

FRotator ADungeonGenerator::GetTJunctionRotation(const TArray<ERoomDirection>& OpenDirs)
{
  // Assuming T-junction rooms are designed with opening at North, East, and West by default (missing South)
  bool bHasNorth = OpenDirs.Contains(ERoomDirection::NORTH);
  bool bHasEast = OpenDirs.Contains(ERoomDirection::EAST);
  bool bHasSouth = OpenDirs.Contains(ERoomDirection::SOUTH);
  bool bHasWest = OpenDirs.Contains(ERoomDirection::WEST);

  if (!bHasSouth) return FRotator(0, 0, 0);    // Missing South
  if (!bHasWest)  return FRotator(0, 90, 0);   // Missing West
  if (!bHasNorth) return FRotator(0, 180, 0);  // Missing North
  if (!bHasEast)  return FRotator(0, 270, 0);  // Missing East

  return FRotator::ZeroRotator;
}

bool ADungeonGenerator::IsOpposite(ERoomDirection Dir1, ERoomDirection Dir2)
{
  return (Dir1 == ERoomDirection::NORTH && Dir2 == ERoomDirection::SOUTH) ||
    (Dir1 == ERoomDirection::SOUTH && Dir2 == ERoomDirection::NORTH) ||
    (Dir1 == ERoomDirection::EAST && Dir2 == ERoomDirection::WEST) ||
    (Dir1 == ERoomDirection::WEST && Dir2 == ERoomDirection::EAST);
}

FVector ADungeonGenerator::GetRotationOffset(float YawRotation)
{
  // Pivot is at northwest corner, so we need to offset based on rotation
  // 0° (North): No offset needed
  // 90° (East): Offset by +X (CellSize)
  // 180° (South): Offset by +X and +Y (CellSize, CellSize)
  // 270° (West): Offset by +Y (CellSize)

  if (FMath::IsNearlyEqual(YawRotation, 0.0f, 0.1f))
  {
    return FVector(0, 0, 0);  // 0° - No offset
  }
  else if (FMath::IsNearlyEqual(YawRotation, 90.0f, 0.1f))
  {
    return FVector(CellSize, 0, 0);  // 90° - Offset X
  }
  else if (FMath::IsNearlyEqual(YawRotation, 180.0f, 0.1f))
  {
    return FVector(CellSize, CellSize, 0);  // 180° - Offset X and Y
  }
  else if (FMath::IsNearlyEqual(YawRotation, 270.0f, 0.1f))
  {
    return FVector(0, CellSize, 0);  // 270° - Offset Y
  }

  return FVector::ZeroVector;
}

void ADungeonGenerator::ClearDungeon()
{
  for (AActor* Room : ActiveDungeonRooms)
  {
    if (Room && IsValid(Room))
    {
      Room->Destroy();
    }
  }
  ActiveDungeonRooms.Empty();

  for (AActor* Obj : SpawnedObjects)
  {
    if (Obj && IsValid(Obj))
    {
      Obj->Destroy();
    }
  }
  SpawnedObjects.Empty();

  OccupiedCells.Empty();
  AvailablePositions.Empty();
  RoomMap.Empty();
  ConnectedDoors.Empty();
  LockedArea.Empty();
  AccessibleArea.Empty();
}

void ADungeonGenerator::CreateLockedArea()
{
  if (OccupiedCells.Num() < 5) return;

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

  int32 TargetLockedRooms = FMath::Max(2, FMath::CeilToInt(OccupiedCells.Num() * LockedAreaSizePercent));

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

  float offset = CellSize / 2;
  FVector LockedRoomWorld(LockedDoorPos1.X * CellSize + offset, LockedDoorPos1.Y * CellSize + offset, 0.0f);
  FVector UnlockedRoomWorld(LockedDoorPos2.X * CellSize + offset, LockedDoorPos2.Y * CellSize + offset, 0.0f);
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

  FIntPoint KeyRoom(0, 0);
  float MaxDistance = 0.0f;

  for (const FIntPoint& Pos : AccessibleArea)
  {
    //find farthest room
    FVector RoomWorldPos(Pos.X * CellSize, Pos.Y * CellSize, 0.0f);
    float Distance = FVector::Dist(RoomWorldPos, DoorWorldPos);
    if (Distance > MaxDistance)
    {
      MaxDistance = Distance;
      KeyRoom = Pos;
    }
  }

  float offset = CellSize / 2;
  FVector KeyPosition(KeyRoom.X * CellSize + offset, KeyRoom.Y * CellSize + offset, 0.0f);
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

  if (EnemyPrefabClass)
  {
    for (int32 i = 0; i < EnemyCount && i < FarRooms.Num(); i++)
    {
      float offset = CellSize / 2;
      FVector WorldPos(FarRooms[i].X * CellSize + offset, FarRooms[i].Y * CellSize + offset, 0.0f);
      AActor* Enemy = GetWorld()->SpawnActor<AActor>(EnemyPrefabClass, WorldPos, FRotator::ZeroRotator);
      if (Enemy)
      {
        SpawnedObjects.Add(Enemy);
      }
    }
  }

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
      float offset = CellSize / 2;
      FVector WorldPos(TreasureRooms[i].X * CellSize + offset, TreasureRooms[i].Y * CellSize + offset, 0.0f);
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
  // This function is now mostly empty since rooms handle their own doors
  // Keep it for compatibility or remove if not needed
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
  // No longer needed - rooms are spawned with correct configuration
}

void ADungeonGenerator::AddAdjacentPositions(FIntPoint Pos)
{
  TArray<FIntPoint> Directions = {
      FIntPoint(0, 1),
      FIntPoint(0, -1),
      FIntPoint(-1, 0),
      FIntPoint(1, 0)
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