#include "ArcadeMinigameCatalog.h"
TArray<UArcadeMinigameDefinition*> UArcadeMinigamePool::GetUniqueGames() const
{
    TArray<UArcadeMinigameDefinition*> Result;
    TSet<FSoftObjectPath> Maps;
    for (UArcadeMinigameDefinition* Game : Games)
        if (IsValid(Game) && !Game->Map.IsNull() && !Maps.Contains(Game->Map.ToSoftObjectPath()))
        { Maps.Add(Game->Map.ToSoftObjectPath()); Result.Add(Game); }
    return Result;
}
TArray<int32> UArcadeMinigamePool::Allocate(int32 GameCount, int32 CellCount, FRandomStream& Random)
{
    TArray<int32> Result;
    if (GameCount <= 0 || CellCount <= 0) return Result;
    TArray<int32> Bag;
    for (int32 I = 0; I < GameCount; ++I) Bag.Add(I);
    for (int32 I = GameCount - 1; I > 0; --I) Bag.Swap(I, Random.RandRange(0, I));
    for (int32 I = 0; I < CellCount; ++I) Result.Add(I < GameCount ? Bag[I] : Random.RandRange(0, GameCount - 1));
    for (int32 I = CellCount - 1; I > 0; --I) Result.Swap(I, Random.RandRange(0, I));
    return Result;
}

