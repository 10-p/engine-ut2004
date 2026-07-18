//=============================================================================
// xObjExporters - Wavefront OBJ format exporters.
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================

#ifndef X_OBJ_EXPORTERS_H
#define X_OBJ_EXPORTERS_H

class EDITOR_API ULevelExporterOBJ : public UExporter
{
	DECLARE_CLASS(ULevelExporterOBJ,UExporter,0,Editor)
	void StaticConstructor();
	UBOOL ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn );
};
class EDITOR_API UPolysExporterOBJ : public UExporter
{
	DECLARE_CLASS(UPolysExporterOBJ,UExporter,0,Editor)
	void StaticConstructor();
	UBOOL ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn );
};

#endif //X_OBJ_EXPORTERS_H

