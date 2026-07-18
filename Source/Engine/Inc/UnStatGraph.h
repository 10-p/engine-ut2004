/*=============================================================================
	UnStatGraph.h: Stats Graphing Utility
	Copyright 2002 Epic MegaGames, Inc. This software is a trade secret.
	Revision history:
		* Created by James Golding
=============================================================================*/

class ENGINE_API FStatGraphLine
{
public:
	UBOOL							bHideLine;

	TArray<FLOAT>					DataHistory;
	INT								DataPos;

	FColor							LineColor;
	FString							LineName;
	FLOAT							YRange[2]; // [Min,Max]
	FLOAT							XSpacing;
	UBOOL							bAutoScale;

	FStatGraphLine();

	UBOOL operator==( const FStatGraphLine& Other ) const
	{
		return this == &Other;
	}
};

class ENGINE_API FStatGraph
{
public:
	UBOOL							bHideGraph;
	UBOOL							bLockScale;

	/* *** Members *** */
	TMap<FString,INT>				LineNameMap;
	TArray<FStatGraphLine>			Lines;

	// Index of stat to grab from GStat and graph.
	TArray<INT>						GetStat;

	// Screen [x.y] size of graph
	FLOAT							GraphSize[2];

	// Screen [x,y] origin (bottom left) of graph
	FLOAT							GraphOrigin[2];

	INT								XRange;
	UBOOL							bHideKey;
	FLOAT							ZeroYRatio;
	BYTE							BackgroundAlpha;
	FString							FilterString;

	/* *** Interface *** */
	FStatGraph();
	~FStatGraph();

	void AddDataPoint(FString  LineName, FLOAT Data, UBOOL bAutoAddLine=0);

	void AddLineAutoRange(FString LineName, FColor Color);
	void AddLine(FString LineName, FColor Color, FLOAT YRangeMin, FLOAT YRangeMax);
	void DestroyLine(FString LineName);

	void Reset(); 
	UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar);
	void Render(UViewport* viewport, FRenderInterface* RI);
};
