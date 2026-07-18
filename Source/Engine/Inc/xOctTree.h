//=============================================================================
// xOctTree
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================

#ifndef xOctTree_H
#define xOctTree_H

#include "UnForcePacking_begin.h"

const int INVALID = -1;

bool ContainsBox( const FBox& a, const FBox& b )
{
	if (!FPointBoxIntersection(b.Min,a))
		return false;
	if (!FPointBoxIntersection(b.Max,a))
		return false;
	return true;
}

template<class T> class OctNode
{
public:
	typedef TArray<OctNode<T>*> NodePtrList;

	OctNode()
	{
		for ( int i=0; i<8; i++ )
		{
			pChildren[i] = NULL;
		}
		empty = true;
        contents.Empty();
	}

	~OctNode()
	{
		for ( int i=0; i<8; i++ )
		{
			delete pChildren[i];
		}
        contents.Empty();
	}

    void AddItem( T& item, FBox box )
    {
        int curDepth = 0;
        int maxDepth = 16;
        OctNode<T>* pNode = GetContainingNode( box, curDepth, maxDepth );
        pNode->contents.AddItem(item);
    }

	bool CheckFit( FBox& box )
	{
		return ContainsBox( bounds, box );
	}

	OctNode<T>* GetContainingNode( FBox& box, int& curDepth, int& maxDepth )
	{
		curDepth++;
		check( CheckFit( box ) );
		if ( curDepth == maxDepth )
		{
			return this;
		}
		if ( pChildren[0] )
		{
			for( int i=0; i<8; i++ )
			{
				if ( pChildren[i]->CheckFit( box ) )
				{
					return pChildren[i]->GetContainingNode( box, curDepth, maxDepth );
				}
			}
			return this;
		}
		else // evaluate whether it would fit within a subdivision
		{
			int fitIdx = -1;
			for ( int i=0; i<8; i++ )
			{
				if ( ContainsBox( CalcChildBound(i), box ))
				{
					fitIdx = i;
					break;
				}
			}
			if ( fitIdx == INVALID )
			{
				return this;
			}
			CreateChildren();
			return pChildren[fitIdx]->GetContainingNode( box, curDepth, maxDepth );
		}
	}

	void CreateChildren( void )
	{
		for ( int i=0; i<8; i++ )
		{
			check( pChildren[i]==NULL );
			pChildren[i] = new OctNode<T>;
			pChildren[i]->bounds = CalcChildBound(i);
		}
		empty = false;
	}

	void Draw( FSceneNode* SceneNode )
	{
        FLineBatcher lines(SceneNode->Viewport->RI,0);
		if ( contents.Num() > 0 )
		{
            lines.DrawBox( bounds, FColor(255,0,0,255) );
		}

		if ( pChildren[0] )
		{
			for ( int i=0; i<8; i++ )
			{
				pChildren[i]->Draw( SceneNode );
			}
		}
	}

	inline FBox CalcChildBound( int idx )
	{
		FBox b;

		// halve each dimension
		b.Min = bounds.Min;
		b.Max = bounds.Min + 0.5f * (bounds.Max - bounds.Min);

		FVector xaxis( b.Max.X - b.Min.X, 0, 0 );
		FVector yaxis( 0, b.Max.Y - b.Min.Y, 0 );
		FVector zaxis( 0, 0, b.Max.Z - b.Min.Z );

		switch( idx )
		{
			case 0: // lower left front			
				break;
			case 1: // lower right front
				b.Min += xaxis;
				b.Max += xaxis;
				break;
			case 2: // upper left front
				b.Min += yaxis;
				b.Max += yaxis;
				break;
			case 3: // upper right front
				b.Min += xaxis + yaxis;
				b.Max += xaxis + yaxis;
				break;
			case 4: // lower left back
				b.Min += zaxis;
				b.Max += zaxis;
				break;
			case 5: // lower right back
				b.Min += zaxis + xaxis;
				b.Max += zaxis + xaxis;
				break;
			case 6: // upper left back
				b.Min += zaxis + yaxis;
				b.Max += zaxis + yaxis;
				break;
			case 7: // upper right back
				b.Min += zaxis + xaxis + yaxis;
				b.Max += zaxis + xaxis + yaxis;
				break;
			default:
				check(0);
		}
		return b;
	}

    void ExtentMark( FVector& center, FVector& extent, NodePtrList& nodeList ) // mark everything that has items that overlaps extent box
    {
        if ( contents.Num()==0 && pChildren[0]==NULL )
			return;

		FBox box = bounds;
		box.Min -= extent;//ExtentsExpand(extent);
        box.Max += extent;
		if (!FPointBoxIntersection(center,box))//box.ContainsPoint( center ) )
			return;

		if ( contents.Num() )
			nodeList.AddItem( this );

		if ( pChildren[0] )
		{
			for( int i=0; i<8; i++ )
			{
				pChildren[i]->ExtentMark( center, extent, nodeList );
			}
		}
    }

	bool		empty;
	bool		skipped;
	FBox		bounds;
	OctNode<T>*	pChildren[8];
	TArray<T>	contents;
	float		t_marked;
};

#include "UnForcePacking_end.h"

#endif//xOctTree_H
