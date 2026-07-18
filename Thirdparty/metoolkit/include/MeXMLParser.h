#ifndef _MEXMLPARSER_H
#define _MEXMLPARSER_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:40 $ - Revision: $Revision: 1.9.2.2 $

   This software and its accompanying manuals have been developed
   by Mathengine PLC ("MathEngine") and the copyright and all other
   intellectual property rights in them belong to MathEngine. All
   rights conferred by law (including rights under international
   copyright conventions) are reserved to MathEngine. This software
   may also incorporate information which is confidential to
   MathEngine.

   Save to the extent permitted by law, or as otherwise expressly
   permitted by MathEngine, this software and the manuals must not
   be copied (in whole or in part), re-arranged, altered or adapted
   in any way without the prior written consent of the Company. In
   addition, the information contained in the software may not be
   disseminated without the prior written consent of MathEngine.

 */

/** @file
 *  XML parser.
 */

#include <stddef.h>
#include <MeCall.h>
#include <MeStream.h>
#include <MePrecision.h>

#define ME_XML_TAG_BUFFER_SIZE 256
#define ME_XML_DATA_BUFFER_SIZE 1024

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _MeXMLInput {
  char buffer[4096];
  int bufptr;
  int bufmax;
  char stack[8];
  int top;
  MeStream stream;
  int line;
  int posn;
  int eof;
  void *userdata;
  char error[ME_XML_DATA_BUFFER_SIZE+256];
} MeXMLInput;

typedef enum _MeXMLError
{
    MeXMLErrorNone,
    MeXMLErrorMalformed,        /* XML is malformed */
    MeXMLErrorInvalidValue,     /* invalid data value  */
    MeXMLErrorEOF,              /* unexpected EOF */
    MeXMLErrorParseFail,        /* XML may be OK, but parse failed with e.g. buffer overflow */
    MeXMLErrorSemantic,         /* XML ok but Mathengine semantics wrong, e.g. missing id attribute */
    MeXMLErrors
} MeXMLError;


typedef enum _ActionType {
  MeXMLActionCallback,
  MeXMLActionParse,
  MeXMLActionEnd
} MeXMLActionType;

typedef struct _MeXMLElement {
  char name[ME_XML_DATA_BUFFER_SIZE];   /* whatever's inside the <> */
  MeXMLInput * fi;  /* the input */
  int level;        /* how deep */
  char *attr;       /* ptr to attributes if any, else 0 */
} MeXMLElement;

typedef struct _MeXMLHandler {
  char *name;             /* corresponding tag */
  MeXMLActionType type;   /* type */
  void *fn;               /* function to call */
  unsigned int offset;    /* struct offset to parse into */
  unsigned int max;       /* expected number of elts for array */
  unsigned int maxstr;    /* max length of string. Used by string array handler */
  MeBool called;          /* 1 if handler was called, ie if tag was in XML
                             file, 0 otherwise */

/* 
   Optional callback that gets called after all handlers except an element handler
   has been called. The second arg is a ptr to the filled structure.
   The third arg is the fourth argument to MeXMLElementProcess().
   This callback allows handling of multiple child elements with the same name.
*/

  MeXMLError (MEAPI *cb)(MeXMLElement *, void *, void *);
} MeXMLHandler;


typedef MeXMLError (MEAPI *MeXMLCallback)(const MeXMLElement *, void *parent);
typedef MeXMLError (MEAPI *MeXMLParseFn)(MeXMLInput *, const MeXMLHandler *, void *);


/*
  Parsing functions
*/
MEPUBLIC int MEAPI
MeXMLParseComma(char *in, char **out);

MeXMLError MEAPI
MeXMLParseUInt(MeXMLInput *, const MeXMLHandler *, void *);

MEPUBLIC MeXMLError MEAPI
MeXMLParseUIntArray(MeXMLInput *, const MeXMLHandler *, void *);

MEPUBLIC MeXMLError MEAPI
MeXMLParseInt(MeXMLInput *, const MeXMLHandler *, void *);

MEPUBLIC MeXMLError MEAPI
MeXMLParseIntArray(MeXMLInput *, const MeXMLHandler *, void *);

MEPUBLIC MeXMLError MEAPI
MeXMLParseFloat(MeXMLInput *, const MeXMLHandler *, void *);

MEPUBLIC MeXMLError MEAPI
MeXMLParseFloatArray(MeXMLInput *, const MeXMLHandler *, void *);

MEPUBLIC MeXMLError MEAPI
MeXMLParseMeReal(MeXMLInput *, const MeXMLHandler *, void *);

MEPUBLIC MeXMLError MEAPI
MeXMLParseMeRealArray(MeXMLInput *, const MeXMLHandler *, void *);

MEPUBLIC MeXMLError MEAPI
MeXMLParseDouble(MeXMLInput *, const MeXMLHandler *, void *);

MEPUBLIC MeXMLError MEAPI
MeXMLParseDoubleArray(MeXMLInput *, const MeXMLHandler *, void *);

MEPUBLIC MeXMLError MEAPI
MeXMLParseString(MeXMLInput *, const MeXMLHandler *, void *);

MEPUBLIC MeXMLError MEAPI
MeXMLParseStringArray(MeXMLInput *, const MeXMLHandler *, void *);

#define ME_XML_MEMBER_OFFSET(object, member)   offsetof(object, member)

#define ME_XML_ELEMENT_HANDLER(name, proc) \
    { name, MeXMLActionCallback, (void*)proc, 0, 0, 0, 0, 0 }

#define ME_XML_INT_HANDLER(name, object, member, cb) \
    { name, MeXMLActionParse, (void*)MeXMLParseInt, ME_XML_MEMBER_OFFSET(object, member), 0, 0, 0, cb}

#define ME_XML_INT_ARRAY_HANDLER(name, object, member, number, cb) \
    { name, MeXMLActionParse, (void*)MeXMLParseIntArray, ME_XML_MEMBER_OFFSET(object, member), number, 0, 0, cb}

#define ME_XML_UINT_HANDLER(name, object, member, cb) \
    { name, MeXMLActionParse, (void*)MeXMLParseUInt, ME_XML_MEMBER_OFFSET(object, member), 0, 0, 0, cb}

#define ME_XML_UINT_ARRAY_HANDLER(name, object, member, number, cb) \
    { name, MeXMLActionParse, (void*)MeXMLParseUIntArray, ME_XML_MEMBER_OFFSET(object, member), number, 0, 0, cb}

#define ME_XML_FLOAT_HANDLER(name, object, member, cb) \
    { name, MeXMLActionParse, (void*)MeXMLParseFloat, ME_XML_MEMBER_OFFSET(object, member), 0, 0, 0, cb}

#define ME_XML_FLOAT_ARRAY_HANDLER(name, object, member, number, cb) \
    { name, MeXMLActionParse, (void*)MeXMLParseFloatArray, ME_XML_MEMBER_OFFSET(object, member), number, 0, 0, cb}

#define ME_XML_MEREAL_HANDLER(name, object, member, cb) \
    { name, MeXMLActionParse, (void*)MeXMLParseMeReal, ME_XML_MEMBER_OFFSET(object, member), 0, 0, 0, cb}

#define ME_XML_MEREAL_ARRAY_HANDLER(name, object, member, number, cb) \
    { name, MeXMLActionParse, (void*)MeXMLParseMeRealArray, ME_XML_MEMBER_OFFSET(object, member), number, 0, 0, cb}

#define ME_XML_DOUBLE_HANDLER(name, object, member, cb) \
    { name, MeXMLActionParse, (void*)MeXMLParseDouble, ME_XML_MEMBER_OFFSET(object, member), 0, 0, 0, cb}

#define ME_XML_DOUBLE_ARRAY_HANDLER(name, object, member, number, cb) \
    { name, MeXMLActionParse, (void*)MeXMLParseDoubleArray, ME_XML_MEMBER_OFFSET(object, member), number, 0, 0, cb}

#define ME_XML_STRING_HANDLER(name, object, member, maxLen, cb) \
    { name, MeXMLActionParse, (void*)MeXMLParseString, ME_XML_MEMBER_OFFSET(object, member), maxLen, 0, 0, cb}

#define ME_XML_STRING_ARRAY_HANDLER(name, object, member, maxLen, number, cb) \
    { name, MeXMLActionParse, (void*)MeXMLParseStringArray, ME_XML_MEMBER_OFFSET(object, member), maxLen, number, 0, cb}

#define ME_XML_HANDLER_END         { (char *)NULL, MeXMLActionEnd }



MEPUBLIC
MeXMLInput *     MEAPI   MeXMLInputCreate(MeStream file);
MEPUBLIC
void             MEAPI   MeXMLInputDestroy(MeXMLInput *input);
MEPUBLIC
void             MEAPI   MeXMLInputSetUserData(MeXMLInput * input, void *userData);
MEPUBLIC
void *           MEAPI   MeXMLInputGetUserData(const MeXMLInput * input);
MEPUBLIC
MeXMLError       MEAPI   MeXMLInputProcess(MeXMLInput * input, MeXMLHandler handlers[], void *userdata);
MEPUBLIC
const char *     MEAPI   MeXMLInputGetErrorString(const MeXMLInput *input);
MEPUBLIC
void             MEAPI   MeXMLInputSetErrorString(MeXMLInput *input, char *error, ...);
MEPUBLIC
MeXMLError       MEAPI   MeXMLElementProcess(MeXMLElement *elem, MeXMLHandler handlers[], void *data, void *userdata);
MEPUBLIC
MeXMLInput *     MEAPI   MeXMLElementGetInput(MeXMLElement *elem);
MEPUBLIC
MeBool           MEAPI   MeXMLHandlerWasCalled(MeXMLHandler handlers[],char *name);
MEPUBLIC
void             MEAPI   MeXMLElementHandlerCreate(MeXMLHandler *handler,char *name,void *fn);
MEPUBLIC
void             MEAPI   MeXMLElementHandlerDestroy(MeXMLHandler *handler);

#ifdef __cplusplus
}
#endif

#endif /* _MEXMLPARSER_H */
