#ifdef WITH_KARMA
#ifndef KARMAEDITORSUPPORT_H
#define KARMAEDITORSUPPORT_H


typedef enum
{
    kMesh2GeometrySphere,
        kMesh2GeometryBox,
        kMesh2GeometryCylinder,
        kMesh2GeometryConvex,
        kMesh2GeometryUnknown        
} Mesh2GeometryType;

// Internal
static Mesh2GeometryType Kname2Type(const char* name);
static void Ktype2Name(Mesh2GeometryType type, char name[256]);


#endif
#endif
