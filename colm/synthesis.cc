/*
 *  Copyright 2007-2012 Adrian Thurston <thurston@complang.org>
 */

/*  This file is part of Colm.
 *
 *  Colm is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 *  Colm is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with Colm; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

#include "bytecode.h"
#include "parsedata.h"
#include "fsmrun.h"
#include "pdarun.h"
#include "input.h"
#include <iostream>
#include <assert.h>

using std::cout;
using std::cerr;
using std::endl;

void Compiler::initUniqueTypes( )
{
	uniqueTypeNil = new UniqueType( TYPE_NIL );
	uniqueTypePtr = new UniqueType( TYPE_TREE, ptrLangEl );
	uniqueTypeBool = new UniqueType( TYPE_TREE, boolLangEl );
	uniqueTypeInt = new UniqueType( TYPE_TREE, intLangEl );
	uniqueTypeStr = new UniqueType( TYPE_TREE, strLangEl );
	uniqueTypeStream = new UniqueType( TYPE_TREE, streamLangEl );
	uniqueTypeInput = new UniqueType( TYPE_TREE, inputLangEl );
	uniqueTypeIgnore = new UniqueType( TYPE_TREE, ignoreLangEl );
	uniqueTypeAny = new UniqueType( TYPE_TREE, anyLangEl );

	uniqeTypeMap.insert( uniqueTypeNil );
	uniqeTypeMap.insert( uniqueTypePtr );
	uniqeTypeMap.insert( uniqueTypeBool );
	uniqeTypeMap.insert( uniqueTypeInt );
	uniqeTypeMap.insert( uniqueTypeStr );
	uniqeTypeMap.insert( uniqueTypeStream );
	uniqeTypeMap.insert( uniqueTypeInput );
	uniqeTypeMap.insert( uniqueTypeIgnore );
	uniqeTypeMap.insert( uniqueTypeAny );
}

IterDef::IterDef( Type type ) : 
	type(type), 
	func(0),
	useFuncId(false),
	useSearchUT(false)
{
	switch ( type ) {
	case Tree:
		inCreateWV = IN_TRITER_FROM_REF;
		inCreateWC = IN_TRITER_FROM_REF;
		inDestroy = IN_TRITER_DESTROY;
		inAdvance = IN_TRITER_ADVANCE;

		inGetCurR = IN_TRITER_GET_CUR_R;
		inGetCurWC = IN_TRITER_GET_CUR_WC;
		inSetCurWC = IN_TRITER_SET_CUR_WC;
		inRefFromCur = IN_TRITER_REF_FROM_CUR;
		useSearchUT = true;
		break;
	case Child:
		inCreateWV = IN_TRITER_FROM_REF;
		inCreateWC = IN_TRITER_FROM_REF;
		inDestroy = IN_TRITER_DESTROY;
		inAdvance = IN_TRITER_NEXT_CHILD;

		inGetCurR = IN_TRITER_GET_CUR_R;
		inGetCurWC = IN_TRITER_GET_CUR_WC;
		inSetCurWC = IN_TRITER_SET_CUR_WC;
		inRefFromCur = IN_TRITER_REF_FROM_CUR;
		useSearchUT = true;
		break;
	case RevChild:
		inCreateWV = IN_REV_TRITER_FROM_REF;
		inCreateWC = IN_REV_TRITER_FROM_REF;
		inDestroy = IN_REV_TRITER_DESTROY;
		inAdvance = IN_REV_TRITER_PREV_CHILD;

		inGetCurR = IN_TRITER_GET_CUR_R;
		inGetCurWC = IN_TRITER_GET_CUR_WC;
		inSetCurWC = IN_TRITER_SET_CUR_WC;
		inRefFromCur = IN_TRITER_REF_FROM_CUR;
		useSearchUT = true;
		break;
	
	case Repeat:
		inCreateWV = IN_TRITER_FROM_REF;
		inCreateWC = IN_TRITER_FROM_REF;
		inDestroy = IN_TRITER_DESTROY;
		inAdvance = IN_TRITER_NEXT_REPEAT;

		inGetCurR = IN_TRITER_GET_CUR_R;
		inGetCurWC = IN_TRITER_GET_CUR_WC;
		inSetCurWC = IN_TRITER_SET_CUR_WC;
		inRefFromCur = IN_TRITER_REF_FROM_CUR;
		useSearchUT = true;
		break;

	case RevRepeat:
		inCreateWV = IN_TRITER_FROM_REF;
		inCreateWC = IN_TRITER_FROM_REF;
		inDestroy = IN_TRITER_DESTROY;
		inAdvance = IN_TRITER_PREV_REPEAT;

		inGetCurR = IN_TRITER_GET_CUR_R;
		inGetCurWC = IN_TRITER_GET_CUR_WC;
		inSetCurWC = IN_TRITER_SET_CUR_WC;
		inRefFromCur = IN_TRITER_REF_FROM_CUR;
		useSearchUT = true;
		break;

	case User:
		assert(false);
	}
}

IterDef::IterDef( Type type, Function *func ) : 
	type(type),
	func(func),
	useFuncId(true),
	useSearchUT(true),
	inCreateWV(IN_UITER_CREATE_WV),
	inCreateWC(IN_UITER_CREATE_WC),
	inDestroy(IN_UITER_DESTROY),
	inAdvance(IN_UITER_ADVANCE),
	inGetCurR(IN_UITER_GET_CUR_R),
	inGetCurWC(IN_UITER_GET_CUR_WC),
	inSetCurWC(IN_UITER_SET_CUR_WC),
	inRefFromCur(IN_UITER_REF_FROM_CUR)
{}

ObjMethod *initFunction( UniqueType *retType, ObjectDef *obj, 
		const String &name, int methIdWV, int methIdWC, bool isConst )
{
	ObjMethod *objMethod = new ObjMethod( retType, name, 
			methIdWV, methIdWC, 0, 0, 0, isConst );
	obj->objMethodMap->insert( name, objMethod );
	return objMethod;
}

ObjMethod *initFunction( UniqueType *retType, ObjectDef *obj, 
		const String &name, int methIdWV, int methIdWC, UniqueType *arg1, bool isConst )
{
	UniqueType *args[] = { arg1 };
	ObjMethod *objMethod = new ObjMethod( retType, name, 
			methIdWV, methIdWC, 1, args, 0, isConst );
	obj->objMethodMap->insert( name, objMethod );
	return objMethod;
}

ObjMethod *initFunction( UniqueType *retType, ObjectDef *obj, 
		const String &name, int methIdWV, int methIdWC, 
		UniqueType *arg1, UniqueType *arg2, bool isConst )
{
	UniqueType *args[] = { arg1, arg2 };
	ObjMethod *objMethod = new ObjMethod( retType, name, 
			methIdWV, methIdWC, 2, args, 0, isConst );
	obj->objMethodMap->insert( name, objMethod );
	return objMethod;
}

IterDef *Compiler::findIterDef( IterDef::Type type, Function *func )
{
	IterDefSetEl *el = iterDefSet.find( IterDef( type, func ) );
	if ( el == 0 )
		el = iterDefSet.insert( IterDef( type, func ) );
	return &el->key;
}

IterDef *Compiler::findIterDef( IterDef::Type type )
{
	IterDefSetEl *el = iterDefSet.find( IterDef( type ) );
	if ( el == 0 )
		el = iterDefSet.insert( IterDef( type ) );
	return &el->key;
}

UniqueType *Compiler::findUniqueType( int typeId )
{
	UniqueType searchKey( typeId );
	UniqueType *uniqueType = uniqeTypeMap.find( &searchKey );
	if ( uniqueType == 0 ) {
		uniqueType = new UniqueType( typeId );
		uniqeTypeMap.insert( uniqueType );
	}
	return uniqueType;
}

UniqueType *Compiler::findUniqueType( int typeId, LangEl *langEl )
{
	UniqueType searchKey( typeId, langEl );
	UniqueType *uniqueType = uniqeTypeMap.find( &searchKey );
	if ( uniqueType == 0 ) {
		uniqueType = new UniqueType( typeId, langEl );
		uniqeTypeMap.insert( uniqueType );
	}
	return uniqueType;
}

UniqueType *Compiler::findUniqueType( int typeId, IterDef *iterDef )
{
	UniqueType searchKey( typeId, iterDef );
	UniqueType *uniqueType = uniqeTypeMap.find( &searchKey );
	if ( uniqueType == 0 ) {
		uniqueType = new UniqueType( typeId, iterDef );
		uniqeTypeMap.insert( uniqueType );
	}
	return uniqueType;
}

void ObjectDef::iterPushScope()
{
	//cout << "iter push scope ";
	if ( scope->childIter == 0 ) {
		scope->childIter = scope->children.head;
	}
	else {
		scope->childIter = scope->childIter->next;
		/* Resetting. */
		if ( scope->childIter == 0 )
			scope ->childIter = scope->children.head;
	}

	scope = scope->childIter;
}

void ObjectDef::iterPopScope()
{
	//cout << "iter pop scope" << endl;
	scope = scope->parentScope;
}

void ObjectDef::pushScope()
{
	ObjNameScope *newScope = new ObjNameScope;
	newScope->objFieldMap = new ObjFieldMap;

	newScope->parentScope = scope;
	scope->children.append( newScope );

	scope = newScope;
}

void ObjectDef::popScope()
{
	scope = scope->parentScope;
}

void ObjectDef::insertField( const String &name, ObjField *value )
{
	scope->objFieldMap->insert( name, value );
	objFieldList->append( value );
}

/* Recurisve find through a single object def's scope. */
ObjField *ObjectDef::findFieldInScope( const String &name, ObjNameScope *inScope )
{
	ObjFieldMapEl *objDefMapEl = inScope->objFieldMap->find( name );
	if ( objDefMapEl != 0 )
		return objDefMapEl->value;
	if ( inScope->parentScope != 0 )
		return findFieldInScope( name, inScope->parentScope );
	return 0;
}

ObjField *ObjectDef::checkRedecl( const String &name )
{
	//cout << "looking for " << name << endl;
	ObjFieldMapEl *objDefMapEl = scope->objFieldMap->find( name );
	if ( objDefMapEl != 0 )
		return objDefMapEl->value;
	return 0;

}

/* 0-based. */
ObjField *ObjectDef::findFieldNum( long offset )
{
	int fn = 0;
	ObjFieldList::Iter field = *objFieldList; 
	while ( fn < offset ) {
		fn++;
		field++;
	}
	return field->value;
}

ObjField *ObjectDef::findField( const String &name )
{
	//cout << "looking for " << name << endl;
	ObjField *objField = findFieldInScope( name, scope );
	if ( objField != 0 )
		return objField;
	return 0;
}
 
ObjMethod *ObjectDef::findMethod( const String &name )
{
	ObjMethodMapEl *objMethodMapEl = objMethodMap->find( name );
	if ( objMethodMapEl != 0 )
		return objMethodMapEl->value;
	return 0;
}

long sizeOfField( UniqueType *fieldUT )
{
	long size = 0;
	if ( fieldUT->typeId == TYPE_ITER ) {
		/* Select on the iterator type. */
		switch ( fieldUT->iterDef->type ) {
			case IterDef::Tree:
			case IterDef::Child:
			case IterDef::Repeat:
			case IterDef::RevRepeat:
				size = sizeof(TreeIter) / sizeof(Word);
				break;
			case IterDef::RevChild:
				size = sizeof(RevTreeIter) / sizeof(Word);
				break;

			case IterDef::User:
				/* User iterators are just a pointer to the UserIter struct. The
				 * struct needs to go right beneath the call to the user iterator
				 * so it can be found by a yield. It is therefore allocated on the
				 * stack right before the call. */
				size = 1;
				break;
		}
	}
	else if ( fieldUT->typeId == TYPE_REF )
		size = 2;
	else
		size = 1;

	return size;
}

void ObjectDef::referenceField( Compiler *pd, ObjField *field )
{
	field->beenReferenced = true;
	initField( pd, field );
}

void ObjectDef::initField( Compiler *pd, ObjField *field )
{
	if ( !field->beenInitialized ) {
		field->beenInitialized = true;
		UniqueType *fieldUT = field->typeRef->uniqueType;

		if ( type == FrameType ) {
			nextOffset += sizeOfField( fieldUT );
			field->offset = -nextOffset;

			pd->initLocalInstructions( field );
		}
		else if ( field->isRhsGet ) {
			field->useOffset = false;
			field->inGetR =   IN_GET_RHS_VAL_R;
			field->inGetWC =  IN_GET_RHS_VAL_WC;
			field->inGetWV =  IN_GET_RHS_VAL_WV;
			field->inSetWC =  IN_SET_RHS_VAL_WC;
			field->inSetWV =  IN_SET_RHS_VAL_WC;
		}
		else {
			field->offset = nextOffset;
			nextOffset += sizeOfField( fieldUT );

			/* Initialize the instructions. */
			pd->initFieldInstructions( field );
		}
	}
}

UniqueType *LangVarRef::loadFieldInstr( Compiler *pd, CodeVect &code, 
		ObjectDef *inObject, ObjField *el, bool forWriting, bool revert ) const
{
	/* Ensure that the field is referenced. */
	inObject->referenceField( pd, el );

	UniqueType *elUT = el->typeRef->uniqueType;

	/* If it's a reference then we load it read always. */
	if ( forWriting ) {
		/* The instruction, depends on whether or not we are reverting. */
		if ( elUT->typeId == TYPE_ITER )
			code.append( elUT->iterDef->inGetCurWC );
		else if ( pd->revertOn && revert )
			code.append( el->inGetWV );
		else
			code.append( el->inGetWC );
	}
	else {
		/* Loading something for writing */
		if ( elUT->typeId == TYPE_ITER )
			code.append( elUT->iterDef->inGetCurR );
		else
			code.append( el->inGetR );
	}

	if ( el->useOffset ) {
		/* Gets of locals and fields require offsets. Fake vars like token
		 * data and lhs don't require it. */
		code.appendHalf( el->offset );
	}
	else if ( el->isRhsGet ) {
		/* Need to place the array computing the val. */
		code.append( el->rhsVal.length() );
		for ( Vector<RhsVal>::Iter rg = el->rhsVal; rg.lte(); rg++ ) {
			code.append( rg->prodEl->production->prodNum );
			code.append( rg->prodEl->pos );
		}
	}

	/* If we are dealing with an iterator then dereference it. */
	if ( elUT->typeId == TYPE_ITER )
		elUT = el->typeRef->searchUniqueType;

	return elUT;
}

ObjectDef *objDefFromUT( Compiler *pd, UniqueType *ut )
{
	ObjectDef *objDef = 0;
	if ( ut->typeId == TYPE_TREE || ut->typeId == TYPE_REF )
		objDef = ut->langEl->objectDef;
	else {
		/* This should have generated a compiler error. */
		assert(false);
	}
	return objDef;
}

/* The qualification must start at a local frame. There cannot be any pointer. */
long LangVarRef::loadQualificationRefs( Compiler *pd, CodeVect &code ) const
{
	long count = 0;
	ObjectDef *rootObj = pd->curLocalFrame;

	/* Start the search from the root object. */
	ObjectDef *searchObjDef = rootObj;

	for ( QualItemVect::Iter qi = *qual; qi.lte(); qi++ ) {
		/* Lookup the field in the current qualification. */
		ObjField *el = searchObjDef->findField( qi->data );
		if ( el == 0 )
			error(qi->loc) << "cannot resolve qualification " << qi->data << endp;

		if ( qi.pos() > 0 ) {
			code.append( IN_REF_FROM_QUAL_REF );
			code.appendHalf( 0 );
			code.appendHalf( el->offset );
		}
		else if ( el->typeRef->iterDef != 0 ) {
			code.append( el->typeRef->iterDef->inRefFromCur );
			code.appendHalf( el->offset );
		}
		else if ( el->typeRef->type == TypeRef::Ref ) {
			code.append( IN_REF_FROM_REF );
			code.appendHalf( el->offset );
		}
		else {
			code.append( IN_REF_FROM_LOCAL );
			code.appendHalf( el->offset );
		}

		UniqueType *elUT = el->typeRef->uniqueType;
		if ( elUT->typeId == TYPE_ITER )
			elUT = el->typeRef->searchUniqueType;
		
		assert( qi->type == QualItem::Dot );

		searchObjDef = objDefFromUT( pd, elUT );
		count += 1;
	}
	return count;
}

void LangVarRef::loadQualification( Compiler *pd, CodeVect &code, 
		ObjectDef *rootObj, int lastPtrInQual, bool forWriting, bool revert ) const
{
	/* Start the search from the root object. */
	ObjectDef *searchObjDef = rootObj;

	for ( QualItemVect::Iter qi = *qual; qi.lte(); qi++ ) {
		/* Lookup the field int the current qualification. */
		ObjField *el = searchObjDef->findField( qi->data );
		if ( el == 0 )
			error(qi->loc) << "cannot resolve qualification " << qi->data << endp;

		if ( forWriting && el->refActive )
			error(qi->loc) << "reference active, cannot write to object" << endp;

		bool lfForWriting = forWriting;
		bool lfRevert = revert;

		/* If there is a pointer in the qualification, we need to compute
		 * forWriting and revert. */
		if ( lastPtrInQual >= 0 ) {
			if ( qi.pos() <= lastPtrInQual ) {
				/* If we are before or at the pointer we are strictly read
				 * only, regardless of the origin. */
				lfForWriting = false;
				lfRevert = false;
			}
			else {
				/* If we are past the pointer then we are always reverting
				 * because the object is global. Forwriting is as passed in.
				 * */
				lfRevert = true;
			}
		}

		UniqueType *qualUT = loadFieldInstr( pd, code, searchObjDef, 
				el, lfForWriting, lfRevert );
		
		if ( qi->type == QualItem::Dot ) {
			/* Cannot a reference. Iterator yes (access of the iterator not
			 * hte current) */
			if ( qualUT->typeId == TYPE_PTR )
				error(loc) << "dot cannot be used to access a pointer" << endp;
		}
		else if ( qi->type == QualItem::Arrow ) {
			if ( qualUT->typeId == TYPE_PTR ) {
				/* Always dereference references when used for qualification. If
				 * this is the last one then we must start with the reverse
				 * execution business. */
				if ( pd->revertOn && qi.pos() == lastPtrInQual && forWriting ) {
					/* This is like a global load. */
					code.append( IN_PTR_DEREF_WV );
				}
				else {
					/* If reading or not yet the last in ref then we only need a
					 * reading deref. */
					code.append( IN_PTR_DEREF_R );
				}

				qualUT = pd->findUniqueType( TYPE_TREE, qualUT->langEl );
			}
			else {
				error(loc) << "arrow operator cannot be used to access this type" << endp;
			}
		}

		searchObjDef = objDefFromUT( pd, qualUT );
	}
}

void LangVarRef::loadContextObj( Compiler *pd, CodeVect &code, 
		int lastPtrInQual, bool forWriting ) const
{
	/* Start the search in the global object. */
	ObjectDef *rootObj = pd->context->contextObjDef;

	if ( forWriting && lastPtrInQual < 0 ) {
		/* If we are writing an no reference was found in the qualification
		 * then load the gloabl with a revert. */
		if ( pd->revertOn )
			code.append( IN_LOAD_CONTEXT_WV );
		else
			code.append( IN_LOAD_CONTEXT_WC );
	}
	else {
		/* Either we are reading or we are loading a pointer that will be
		 * dereferenced. */
		code.append( IN_LOAD_CONTEXT_R );
	}

	loadQualification( pd, code, rootObj, lastPtrInQual, forWriting, true );
}

void LangVarRef::loadGlobalObj( Compiler *pd, CodeVect &code, 
		int lastPtrInQual, bool forWriting ) const
{
	/* Start the search in the global object. */
	ObjectDef *rootObj = pd->globalObjectDef;

	if ( forWriting && lastPtrInQual < 0 ) {
		/* If we are writing an no reference was found in the qualification
		 * then load the gloabl with a revert. */
		if ( pd->revertOn )
			code.append( IN_LOAD_GLOBAL_WV );
		else
			code.append( IN_LOAD_GLOBAL_WC );
	}
	else {
		/* Either we are reading or we are loading a pointer that will be
		 * dereferenced. */
		code.append( IN_LOAD_GLOBAL_R );
	}

	loadQualification( pd, code, rootObj, lastPtrInQual, forWriting, true );
}

void LangVarRef::loadCustom( Compiler *pd, CodeVect &code, 
		int lastPtrInQual, bool forWriting ) const
{
	/* Start the search in the local frame. */
	loadQualification( pd, code, pd->curLocalFrame, lastPtrInQual, forWriting, true );
}

void LangVarRef::loadLocalObj( Compiler *pd, CodeVect &code, 
		int lastPtrInQual, bool forWriting ) const
{
	/* Start the search in the local frame. */
	loadQualification( pd, code, pd->curLocalFrame, lastPtrInQual, forWriting, false );
}

bool LangVarRef::isLocalRef( Compiler *pd ) const
{
	if ( qual->length() > 0 ) {
		if ( pd->curLocalFrame->findField( qual->data[0].data ) != 0 )
			return true;
	}
	else if ( pd->curLocalFrame->findField( name ) != 0 )
		return true;
	else if ( pd->curLocalFrame->findMethod( name ) != 0 )
		return true;

	return false;
}

bool LangVarRef::isContextRef( Compiler *pd ) const
{
	if ( pd->context != 0 ) {
		if ( qual->length() > 0 ) {
			if ( pd->context->contextObjDef->findField( qual->data[0].data ) != 0 )
				return true;
		}
		else if ( pd->context->contextObjDef->findField( name ) != 0 )
			return true;
		else if ( pd->context->contextObjDef->findMethod( name ) != 0 )
			return true;
	}

	return false;
}

bool LangVarRef::isCustom( Compiler *pd ) const
{
	if ( qual->length() > 0 ) {
		ObjField *field = pd->curLocalFrame->findField( qual->data[0].data );
		if ( field != 0 && field->isCustom )
			return true;
	}
	else {
		ObjField *field = pd->curLocalFrame->findField( name );
		if ( field != 0 ) {
			if ( field->isCustom )
				return true;
		}
		else {
			ObjMethod *method = pd->curLocalFrame->findMethod( name );
			if ( method != 0 && method->isCustom )
				return true;
		}
	
	}
	return false;
}

void LangVarRef::loadObj( Compiler *pd, CodeVect &code, 
		int lastPtrInQual, bool forWriting ) const
{
	if ( isCustom( pd ) )
		loadCustom( pd, code, lastPtrInQual, forWriting );
	else if ( isLocalRef( pd ) )
		loadLocalObj( pd, code, lastPtrInQual, forWriting );
	else if ( isContextRef( pd ) )
		loadContextObj( pd, code, lastPtrInQual, forWriting );
	else
		loadGlobalObj( pd, code, lastPtrInQual, forWriting );
}

VarRefLookup LangVarRef::lookupQualification( Compiler *pd, ObjectDef *rootDef ) const
{
	int lastPtrInQual = -1;
	ObjectDef *searchObjDef = rootDef;
	int firstConstPart = -1;

	for ( QualItemVect::Iter qi = *qual; qi.lte(); qi++ ) {
		/* Lookup the field int the current qualification. */
		ObjField *el = searchObjDef->findField( qi->data );
		if ( el == 0 )
			error(qi->loc) << "cannot resolve qualification " << qi->data << endp;

		/* Lookup the type of the field. */
		UniqueType *qualUT = el->typeRef->uniqueType;

		/* If we are dealing with an iterator then dereference it. */
		if ( qualUT->typeId == TYPE_ITER )
			qualUT = el->typeRef->searchUniqueType;

		/* Is it const? */
		if ( firstConstPart < 0 && el->isConst )
			firstConstPart = qi.pos();

		/* Check for references. When loop is done we will have the last one
		 * present, if any. */
		if ( qualUT->typeId == TYPE_PTR )
			lastPtrInQual = qi.pos();

		if ( qi->type == QualItem::Dot ) {
			/* Cannot dot a reference. Iterator yes (access of the iterator
			 * not the current) */
			if ( qualUT->typeId == TYPE_PTR )
				error(loc) << "dot cannot be used to access a pointer" << endp;
		}
		else if ( qi->type == QualItem::Arrow ) {
			if ( qualUT->typeId == TYPE_ITER )
				qualUT = el->typeRef->searchUniqueType;
			else if ( qualUT->typeId == TYPE_PTR )
				qualUT = pd->findUniqueType( TYPE_TREE, qualUT->langEl );
		}

		searchObjDef = objDefFromUT( pd, qualUT );
	}

	return VarRefLookup( lastPtrInQual, firstConstPart, searchObjDef );
}

VarRefLookup LangVarRef::lookupObj( Compiler *pd ) const
{
	ObjectDef *rootDef;
	if ( isLocalRef( pd ) )
		rootDef = pd->curLocalFrame;
	else if ( isContextRef( pd ) )
		rootDef = pd->context->contextObjDef;
	else
		rootDef = pd->globalObjectDef;

	return lookupQualification( pd, rootDef );
}

VarRefLookup LangVarRef::lookupField( Compiler *pd ) const
{
	/* Lookup the object that the field is in. */
	VarRefLookup lookup = lookupObj( pd );

	/* Lookup the field. */
	ObjField *field = lookup.inObject->findField( name );
	if ( field == 0 )
		error(loc) << "cannot find name " << name << " in object" << endp;

	lookup.objField = field;
	lookup.uniqueType = field->typeRef->uniqueType;

	if ( field->typeRef->searchUniqueType != 0 )
		lookup.iterSearchUT = field->typeRef->searchUniqueType;

	return lookup;
}


VarRefLookup LangVarRef::lookupMethod( Compiler *pd ) 
{
	/* Lookup the object that the field is in. */
	VarRefLookup lookup = lookupObj( pd );

	/* Find the method. */
	assert( lookup.inObject->objMethodMap != 0 );
	ObjMethod *method = lookup.inObject->findMethod( name );
	if ( method == 0 ) {
		/* Not found as a method, try it as an object on which we will call a
		 * default function. */
		qual->append( QualItem( InputLoc(), name, QualItem::Dot ) );
		name = "finish";

		/* Lookup the object that the field is in. */
		VarRefLookup lookup = lookupObj( pd );

		/* Find the method. */
		assert( lookup.inObject->objMethodMap != 0 );
		method = lookup.inObject->findMethod( name );
		if ( method == 0 )
			error(loc) << "cannot find " << name << "(...) in object" << endp;
	}
	
	lookup.objMethod = method;
	lookup.uniqueType = method->returnUT;
	
	return lookup;
}

void LangVarRef::setFieldInstr( Compiler *pd, CodeVect &code, 
		ObjectDef *inObject, ObjField *el, UniqueType *exprUT, bool revert ) const
{
	/* Ensure that the field is referenced. */
	inObject->referenceField( pd, el );

	if ( pd->revertOn && revert )
		code.append( el->inSetWV );
	else
		code.append( el->inSetWC );

	/* Maybe write out an offset. */
	if ( el->useOffset )
		code.appendHalf( el->offset );
}

bool castAssignment( Compiler *pd, CodeVect &code, UniqueType *destUT, 
		UniqueType *destSearchUT, UniqueType *srcUT )
{
	if ( destUT == srcUT )
		return true;

	/* Casting trees to any. */
	if ( destUT->typeId == TYPE_TREE && destUT->langEl == pd->anyLangEl &&
			srcUT->typeId == TYPE_TREE )
		return true;
	
	/* Setting a reference from a tree. */
	if ( destUT->typeId == TYPE_REF && srcUT->typeId == TYPE_TREE &&
			destUT->langEl == srcUT->langEl )
		return true;

	/* Setting a tree from a reference. */
	if ( destUT->typeId == TYPE_TREE && srcUT->typeId == TYPE_REF &&
			destUT->langEl == srcUT->langEl )
		return true;
	
	/* Setting an iterator from a tree. */
	if ( destUT->typeId == TYPE_ITER && srcUT->typeId == TYPE_TREE && 
			destSearchUT->langEl == srcUT->langEl )
		return true;
	
	/* Assigning nil to a tree. */
	if ( destUT->typeId == TYPE_TREE && srcUT->typeId == TYPE_NIL )
		return true;

	/* Assigning nil to a pointer. */
	if ( destUT->typeId == TYPE_PTR && srcUT->typeId == TYPE_NIL )
		return true;

	return false;
}

void LangVarRef::setField( Compiler *pd, CodeVect &code, 
		ObjectDef *inObject, UniqueType *exprUT, bool revert ) const
{
	ObjField *el = inObject->findField( name );
	if ( el == 0 )
		error(loc) << "cannot find name " << name << " in object" << endp;

	setFieldInstr( pd, code, inObject, el, exprUT, revert );
}

void LangVarRef::setFieldIter( Compiler *pd, CodeVect &code, 
		ObjectDef *inObject, UniqueType *objUT, UniqueType *exprType, bool revert ) const
{
	ObjField *el = inObject->findField( name );
	if ( el == 0 )
		error(loc) << "cannot find name " << name << " in object" << endp;

	code.append( objUT->iterDef->inSetCurWC );
	code.appendHalf( el->offset );
}

UniqueType *LangVarRef::evaluate( Compiler *pd, CodeVect &code, bool forWriting ) const
{
	/* Lookup the loadObj. */
	VarRefLookup lookup = lookupField( pd );

	/* Load the object, if any. */
	loadObj( pd, code, lookup.lastPtrInQual, forWriting );

	/* Load the field. */
	UniqueType *ut = loadFieldInstr( pd, code, lookup.inObject, 
			lookup.objField, forWriting, false );

	return ut;
}

void LangVarRef::canTakeRef( Compiler *pd, VarRefLookup &lookup ) const
{
	bool canTake = false;

	/* If the var is not a local, it must be an attribute accessed
	 * via a local and attributes. */
	if ( lookup.inObject->type == ObjectDef::FrameType )
		canTake = true;
	else if ( isLocalRef(pd) && lookup.lastPtrInQual < 0 && lookup.uniqueType->typeId != TYPE_PTR ) 
		canTake = true;

	if ( !canTake ) {
		error(loc) << "can only take references of locals or "
				"attributes accessed via a local" << endp;
	}

	if ( lookup.objField->refActive )
		error(loc) << "reference currently active, cannot take another" << endp;
}

/* Return the field referenced. */
ObjField *LangVarRef::preEvaluateRef( Compiler *pd, CodeVect &code ) const
{
	VarRefLookup lookup = lookupField( pd );

	canTakeRef( pd, lookup );

	loadQualificationRefs( pd, code );

	return lookup.objField;
}

/* Return the field referenced. */
ObjField *LangVarRef::evaluateRef( Compiler *pd, CodeVect &code, long pushCount ) const
{
	VarRefLookup lookup = lookupField( pd );

	canTakeRef( pd, lookup );

	/* Ensure that the field is referenced. */
	lookup.inObject->referenceField( pd, lookup.objField );

	/* Note that we could have modified children. */
	if ( qual->length() == 0 )
		lookup.objField->refActive = true;

	/* Whenever we take a reference we have to assume writing and that the
	 * tree is dirty. */
	lookup.objField->dirtyTree = true;

	if ( qual->length() > 0 ) {
		code.append( IN_REF_FROM_QUAL_REF );
		code.appendHalf( pushCount );
		code.appendHalf( lookup.objField->offset );
	}
	else if ( lookup.objField->typeRef->iterDef != 0 ) {
		code.append( lookup.objField->typeRef->iterDef->inRefFromCur );
		code.appendHalf( lookup.objField->offset );
	}
	else if ( lookup.objField->typeRef->type == TypeRef::Ref ) {
		code.append( IN_REF_FROM_REF );
		code.appendHalf( lookup.objField->offset );
	}
	else {
		code.append( IN_REF_FROM_LOCAL );
		code.appendHalf( lookup.objField->offset );
	}

	return lookup.objField;
}

ObjField **LangVarRef::evaluateArgs( Compiler *pd, CodeVect &code, 
		VarRefLookup &lookup, ExprVect *args ) const
{
	/* Parameter list is given only for user defined methods. Otherwise it
	 * will be null. */
	ParameterList *paramList = lookup.objMethod->paramList;

	/* Match the number of arguments. */
	int numArgs = args != 0 ? args->length() : 0;
	if ( numArgs != lookup.objMethod->numParams )
		error(loc) << "wrong number of arguments" << endp;

	/* This is for storing the object fields used by references. */
	ObjField **paramRefs = new ObjField*[numArgs];
	memset( paramRefs, 0, sizeof(ObjField*) * numArgs );

	/* Evaluate and push the args. */
	if ( args != 0 ) {
		/* We use this only if there is a paramter list. */
		ParameterList::Iter p;
		long pushCount = 0;

		/* First pass we need to push object loads for reference parameters. */
		paramList != 0 && ( p = *paramList );
		for ( ExprVect::Iter pe = *args; pe.lte(); pe++ ) {
			/* Get the expression and the UT for the arg. */
			LangExpr *expression = *pe;
			UniqueType *paramUT = lookup.objMethod->paramUTs[pe.pos()];

			if ( paramUT->typeId == TYPE_REF ) {
				/* Make sure we are dealing with a variable reference. */
				if ( expression->type != LangExpr::TermType )
					error(loc) << "not a term: argument must be a local variable" << endp;
				if ( expression->term->type != LangTerm::VarRefType )
					error(loc) << "not a variable: argument must be a local variable" << endp;

				/* Lookup the field. */
				LangVarRef *varRef = expression->term->varRef;

				ObjField *refOf = varRef->preEvaluateRef( pd, code );
				paramRefs[pe.pos()] = refOf;

				pushCount += varRef->qual->length() * 2;
			}

			/* Advance the parameter list iterator if we have it. */
			paramList != 0 && p.increment();
		}

		paramList != 0 && ( p = *paramList );
		for ( ExprVect::Iter pe = *args; pe.lte(); pe++ ) {
			/* Get the expression and the UT for the arg. */
			LangExpr *expression = *pe;
			UniqueType *paramUT = lookup.objMethod->paramUTs[pe.pos()];

			if ( paramUT->typeId == TYPE_REF ) {
				
				/* Make sure we are dealing with a variable reference. */
				if ( expression->type != LangExpr::TermType )
					error(loc) << "not a term: argument must be a local variable" << endp;
				if ( expression->term->type != LangTerm::VarRefType )
					error(loc) << "not a variable: argument must be a local variable" << endp;

				/* Lookup the field. */
				LangVarRef *varRef = expression->term->varRef;

				pushCount -= varRef->qual->length() * 2;

				ObjField *refOf = varRef->evaluateRef( pd, code, pushCount );
				paramRefs[pe.pos()] = refOf;

				pushCount += 2;
			}
			else {
				UniqueType *exprUT = expression->evaluate( pd, code );

				if ( !castAssignment( pd, code, paramUT, 0, exprUT ) )
					error(loc) << "arg " << pe.pos()+1 << " is of the wrong type" << endp;

				pushCount += 1;
			}

			/* Advance the parameter list iterator if we have it. */
			paramList != 0 && p.increment();
		}
	}

	return paramRefs;
}

void LangVarRef::resetActiveRefs( Compiler *pd, VarRefLookup &lookup, ObjField **paramRefs ) const
{
	/* Parameter list is given only for user defined methods. Otherwise it
	 * will be null. */
	for ( long p = 0; p < lookup.objMethod->numParams; p++ ) {
		if ( paramRefs[p] != 0 )
			paramRefs[p]->refActive = false;
	}
}


void LangVarRef::callOperation( Compiler *pd, CodeVect &code, VarRefLookup &lookup ) const
{
	/* This is for writing if it is a non-const builtin. */
	bool forWriting = lookup.objMethod->func == 0 && 
			!lookup.objMethod->isConst;

	if ( lookup.objMethod->useCallObj ) {
		/* Load the object, if any. */
		loadObj( pd, code, lookup.lastPtrInQual, forWriting );
	}

	/* Check if we need to revert the function. If it operates on a reference
	 * or if it is not local then we need to revert it. */
	bool revert = lookup.lastPtrInQual >= 0 || !isLocalRef(pd);
	
	/* The call instruction. */
	if ( pd->revertOn && revert )  {
		if ( lookup.objMethod->opcodeWV == IN_PARSE_FINISH_WV ) {
			code.append( IN_PARSE_SAVE_STEPS );
			code.append( IN_PARSE_FINISH_WV );
			code.appendHalf( 0 );
			code.append( IN_PCR_CALL );
			code.append( IN_PARSE_FINISH_EXIT_WV );
		}
		else {
			code.append( lookup.objMethod->opcodeWV );
		}
	}
	else {
		if ( lookup.objMethod->opcodeWC == IN_PARSE_FINISH_WC ) {
			code.append( IN_PARSE_SAVE_STEPS );
			code.append( IN_PARSE_FINISH_WC );
			code.appendHalf( 0 );
			code.append( IN_PCR_CALL );
			code.append( IN_PARSE_FINISH_EXIT_WC );
		}
		else {
			code.append( lookup.objMethod->opcodeWC );
		}
	}
	
	if ( lookup.objMethod->useFuncId )
		code.appendHalf( lookup.objMethod->funcId );
}

void LangVarRef::popRefQuals( Compiler *pd, CodeVect &code, 
		VarRefLookup &lookup, ExprVect *args ) const
{
	long popCount = 0;

	/* Evaluate and push the args. */
	if ( args != 0 ) {
		/* We use this only if there is a paramter list. */
		for ( ExprVect::Iter pe = *args; pe.lte(); pe++ ) {
			/* Get the expression and the UT for the arg. */
			LangExpr *expression = *pe;
			UniqueType *paramUT = lookup.objMethod->paramUTs[pe.pos()];

			if ( paramUT->typeId == TYPE_REF ) {
				/* Lookup the field. */
				LangVarRef *varRef = expression->term->varRef;
				popCount += varRef->qual->length() * 2;
			}
		}
		if ( popCount > 0 ) {
			code.append( IN_POP_N_WORDS );
			code.appendHalf( (short)popCount );
		}
	}
}

UniqueType *LangVarRef::evaluateCall( Compiler *pd, CodeVect &code, ExprVect *args ) 
{
	code.append( IN_CONTIGUOUS );
	code.appendHalf( 512 );

	/* Evaluate the object. */
	VarRefLookup lookup = lookupMethod( pd );

	/* Evaluate and push the arguments. */
	ObjField **paramRefs = evaluateArgs( pd, code, lookup, args );

	/* Write the call opcode. */
	callOperation( pd, code, lookup );

	popRefQuals( pd, code, lookup, args );

	resetActiveRefs( pd, lookup, paramRefs);
	delete[] paramRefs;

	/* Return the type to the expression. */
	return lookup.uniqueType;
}

UniqueType *LangTerm::evaluateMatch( Compiler *pd, CodeVect &code ) const
{
	/* Add the vars bound by the pattern into the local scope. */
	for ( PatternItemList::Iter item = *pattern->list; item.lte(); item++ ) {
		if ( item->varRef != 0 )
			item->bindId = pattern->nextBindId++;
	}

	UniqueType *ut = varRef->evaluate( pd, code );
	if ( ut->typeId != TYPE_TREE )
		error(varRef->loc) << "expected match against a tree type" << endp;

	/* Store the language element type in the pattern. This is needed by
	 * the pattern parser. */
	pattern->langEl = ut->langEl;

	code.append( IN_MATCH );
	code.appendHalf( pattern->patRepId );

	for ( PatternItemList::Iter item = pattern->list->last(); item.gtb(); item-- ) {
		if ( item->varRef != 0 ) {
			/* Compute the unique type. */
			UniqueType *exprType = pd->findUniqueType( TYPE_TREE, item->factor->langEl );

			/* Get the type of the variable being assigned to. */
			VarRefLookup lookup = item->varRef->lookupField( pd );

			item->varRef->loadObj( pd, code, lookup.lastPtrInQual, false );
			item->varRef->setField( pd, code, lookup.inObject, exprType, false );
		}
	}

	return ut;
}

UniqueType *LangTerm::evaluateNew( Compiler *pd, CodeVect &code ) const
{
	/* Evaluate the expression. */
	UniqueType *ut = expr->evaluate( pd, code );
	if ( ut->typeId != TYPE_TREE )
		error() << "new can only be applied to tree types" << endp;

	code.append( IN_TREE_NEW );
	return pd->findUniqueType( TYPE_PTR, ut->langEl );
}

void LangTerm::assignFieldArgs( Compiler *pd, CodeVect &code, UniqueType *replUT ) const
{
	/* Now assign the field initializations. Note that we need to do this in
	 * reverse because the last expression evaluated is at the top of the
	 * stack. */
	if ( fieldInitArgs != 0 && fieldInitArgs->length() > 0 ) {
		ObjectDef *objDef = objDefFromUT( pd, replUT );
		/* Note the reverse traversal. */
		for ( FieldInitVect::Iter pi = fieldInitArgs->last(); pi.gtb(); pi-- ) {
			FieldInit *fieldInit = *pi;
			ObjField *field = objDef->findFieldNum( pi.pos() );
			if ( field == 0 ) {
				error(fieldInit->loc) << "failed to find init pos " << 
					pi.pos() << " in object" << endp;
			}

			/* Lookup the type of the field and compare it to the type of the
			 * expression. */
			UniqueType *fieldUT = field->typeRef->uniqueType;
			if ( !castAssignment( pd, code, fieldUT, 0, fieldInit->exprUT ) )
				error(fieldInit->loc) << "type mismatch in initialization" << endp;

			/* The set field instruction must leave the object on the top of
			 * the stack. */
			code.append( IN_SET_FIELD_LEAVE_WC );
			code.appendHalf( field->offset );
		}
	}
}

UniqueType *LangTerm::evaluateConstruct( Compiler *pd, CodeVect &code ) const
{
	/* Evaluate the initialization expressions. */
	if ( fieldInitArgs != 0 && fieldInitArgs->length() > 0 ) {
		for ( FieldInitVect::Iter pi = *fieldInitArgs; pi.lte(); pi++ ) {
			FieldInit *fieldInit = *pi;
			fieldInit->exprUT = fieldInit->expr->evaluate( pd, code );
		}
	}

	/* Assign bind ids to the variables in the replacement. */
	for ( ConsItemList::Iter item = *constructor->list; item.lte(); item++ ) {
		if ( item->expr != 0 )
			item->bindId = constructor->nextBindId++;
	}

	/* Evaluate variable references. */
	for ( ConsItemList::Iter item = constructor->list->last(); item.gtb(); item-- ) {
		if ( item->type == ConsItem::ExprType ) {
			UniqueType *ut = item->expr->evaluate( pd, code );
		
			if ( ut->typeId != TYPE_TREE )
				error() << "variables used in replacements must be trees" << endp;

			item->langEl = ut->langEl;
		}
	}

	/* Construct the tree using the tree information stored in the compiled
	 * code. */
	code.append( IN_CONSTRUCT );
	code.appendHalf( constructor->patRepId );

	/* Lookup the type of the replacement and store it in the replacement
	 * object so that replacement parsing has a target. */
	UniqueType *replUT = typeRef->uniqueType;
	if ( replUT->typeId != TYPE_TREE )
		error(loc) << "don't know how to construct this type" << endp;
	
	if ( replUT->langEl->generic != 0 && replUT->langEl->generic->typeId == GEN_PARSER ) {
		code.append( IN_DUP_TOP );
		code.append( IN_CONSTRUCT_INPUT );
		code.append( IN_TOP_SWAP );
		code.append( IN_SET_INPUT );
	}
	
	constructor->langEl = replUT->langEl;
	assignFieldArgs( pd, code, replUT );

	if ( varRef != 0 ) {
		code.append( IN_DUP_TOP );

		/* Get the type of the variable being assigned to. */
		VarRefLookup lookup = varRef->lookupField( pd );

		varRef->loadObj( pd, code, lookup.lastPtrInQual, false );
		varRef->setField( pd, code, lookup.inObject, replUT, false );
	}

	return replUT;
}

void LangTerm::parseFrag( Compiler *pd, CodeVect &code, int stopId ) const
{
	/* Parse instruction, dependent on whether or not we are producing
	 * revert or commit code. */
	if ( pd->revertOn ) {
		code.append( IN_PARSE_SAVE_STEPS );
		code.append( IN_PARSE_FRAG_WV );
		code.appendHalf( stopId );
		code.append( IN_PCR_CALL );
		code.append( IN_PARSE_FRAG_EXIT_WV );
	}
	else {
		code.append( IN_PARSE_SAVE_STEPS );
		code.append( IN_PARSE_FRAG_WC );
		code.appendHalf( stopId );
		code.append( IN_PCR_CALL );
		code.append( IN_PARSE_FRAG_EXIT_WC );
	}
}

UniqueType *LangTerm::evaluateParse( Compiler *pd, CodeVect &code, bool stop, bool orig ) const
{
	UniqueType *ut = typeRef->uniqueType->langEl->generic->utArg;

	/* If this is a parse stop then we need to verify that the type is
	 * compatible with parse stop. */
	if ( stop )
		ut->langEl->parseStop = true;
	int stopId = stop ? ut->langEl->id : 0;

	bool context = false;
	if ( ut->langEl->contextIn != 0 ) {
		if ( fieldInitArgs == 0 || fieldInitArgs->length() != 1 )
			error(loc) << "parse command requires just input" << endp;
		context = true;
	}

	/* Assign bind ids to the variables in the replacement. */
	for ( ConsItemList::Iter item = *constructor->list; item.lte(); item++ ) {
		if ( item->expr != 0 )
			item->bindId = constructor->nextBindId++;
	}

	/* Evaluate variable references. */
	for ( ConsItemList::Iter item = constructor->list->last(); item.gtb(); item-- ) {
		if ( item->type == ConsItem::ExprType ) {
			UniqueType *ut = item->expr->evaluate( pd, code );
		
			if ( ut->typeId != TYPE_TREE )
				error() << "variables used in replacements must be trees" << endp;

			item->langEl = ut->langEl;
		}
	}

	/* Construct the tree using the tree information stored in the compiled
	 * code. */
	code.append( IN_CONSTRUCT );
	code.appendHalf( constructor->patRepId );

	/* Dup for the finish operation. */
	code.append( IN_DUP_TOP );

	/*
	 * First load the context into the parser.
	 */
	if ( context ) {
		/* Dup the parser. */
		code.append( IN_DUP_TOP );

		/* Eval the context. */
		UniqueType *argUT = fieldInitArgs->data[0]->expr->evaluate( pd, code );

		if ( argUT != pd->uniqueTypeStream && argUT->typeId != TYPE_TREE )
			error(loc) << "context argument must be a stream or a tree" << endp;

		/* Store the context. */
		code.append( IN_TOP_SWAP );
		code.append( IN_SET_PARSER_CTX_WC );
	}

	/* Lookup the type of the replacement and store it in the replacement
	 * object so that replacement parsing has a target. */
	UniqueType *replUT = typeRef->uniqueType;
	if ( replUT->typeId != TYPE_TREE )
		error(loc) << "don't know how to construct this type" << endp;
	
	/*
	 * Construct and set the input stream.
	 */
	if ( replUT->langEl->generic != 0 && replUT->langEl->generic->typeId == GEN_PARSER ) {
	}
	
	constructor->langEl = replUT->langEl;

	/*****************************/

	if ( parserText->list->length() == 1 && 
			parserText->list->head->type == ConsItem::ExprType )
	{
		/* Only one argument and it is an expression type. Evaluate it (but
		 * ignore the code) to determine if it is an input type. */
		CodeVect unused;
		UniqueType *ut = parserText->list->head->expr->evaluate( pd, unused );

		if ( ut == pd->uniqueTypeInput ) {
			/* Only one argument and it is an input object. Set the input into
			 * the parser and parse using it. */
			code.append( IN_DUP_TOP );

			parserText->list->head->expr->evaluate( pd, code );

			code.append( IN_TOP_SWAP );
			code.append( IN_SET_INPUT );

			code.append( IN_DUP_TOP );

			parseFrag( pd, code, stopId );

			goto finish;
		}
	}

	code.append( IN_DUP_TOP );
	code.append( IN_CONSTRUCT_INPUT );
	code.append( IN_TOP_SWAP );
	code.append( IN_SET_INPUT );

	for ( ConsItemList::Iter item = *parserText->list; item.lte(); item++ )
		code.append( IN_DUP_TOP );
	
	/* Assign bind ids to the variables in the replacement. */
	for ( ConsItemList::Iter item = *parserText->list; item.lte(); item++ ) {
		switch ( item->type ) {
		case ConsItem::FactorType: {
			String result;
			bool unusedCI;
			prepareLitString( result, unusedCI, 
					item->factor->typeRef->pdaLiteral->data,
					item->factor->typeRef->pdaLiteral->loc );

			/* Make sure we have this string. */
			StringMapEl *mapEl = 0;
			if ( pd->literalStrings.insert( result, &mapEl ) )
				mapEl->value = pd->literalStrings.length()-1;

			code.append( IN_LOAD_STR );
			code.appendWord( mapEl->value );
			break;
		}
		case ConsItem::InputText: {
			/* Make sure we have this string. */
			StringMapEl *mapEl = 0;
			if ( pd->literalStrings.insert( item->data, &mapEl ) )
				mapEl->value = pd->literalStrings.length()-1;

			code.append( IN_LOAD_STR );
			code.appendWord( mapEl->value );
			break;
		}
		case ConsItem::ExprType:
			item->expr->evaluate( pd, code );
			break;
		}

		code.append( IN_TOP_SWAP );

		/* Not a stream. Get the input first. */
		code.append( IN_GET_INPUT );
		if ( pd->revertOn )
			code.append( IN_INPUT_APPEND_WV );
		else
			code.append( IN_INPUT_APPEND_WC );
		code.append( IN_POP );

		code.append( IN_DUP_TOP );

		/* Parse instruction, dependent on whether or not we are producing
		 * revert or commit code. */
		parseFrag( pd, code, stopId );
	}

	/*
	 * Finish operation
	 */
finish:

	/* Parse instruction, dependent on whether or not we are producing revert
	 * or commit code. */
	if ( pd->revertOn ) {
		/* Finish immediately. */
		code.append( IN_PARSE_SAVE_STEPS );
		code.append( IN_PARSE_FINISH_WV );
		code.appendHalf( stopId );
		code.append( IN_PCR_CALL );
		code.append( IN_PARSE_FINISH_EXIT_WV );
	}
	else {
		/* Finish immediately. */
		code.append( IN_PARSE_SAVE_STEPS );
		code.append( IN_PARSE_FINISH_WC );
		code.appendHalf( stopId );
		code.append( IN_PCR_CALL );
		code.append( IN_PARSE_FINISH_EXIT_WC );
	}

	code.append( IN_POP );

	/*
	 * Capture to the local var.
	 */
	if ( varRef != 0 ) {
		code.append( IN_DUP_TOP );

		if ( orig ) {
			code.append( IN_GET_PARSER_MEM_R );
			code.appendHalf( 0 );
		}

		/* Get the type of the variable being assigned to. */
		VarRefLookup lookup = varRef->lookupField( pd );

		varRef->loadObj( pd, code, lookup.lastPtrInQual, false );
		varRef->setField( pd, code, lookup.inObject, replUT, false );
	}


	return replUT;
}


UniqueType *LangTerm::evaluateSend( Compiler *pd, CodeVect &code ) const
{
	UniqueType *varUt = varRef->evaluate( pd, code );

	/* Dup for every send. */
	for ( ConsItemList::Iter item = *parserText->list; item.lte(); item++ )
		code.append( IN_DUP_TOP );

	/* Assign bind ids to the variables in the replacement. */
	for ( ConsItemList::Iter item = *parserText->list; item.lte(); item++ ) {
		switch ( item->type ) {
		case ConsItem::FactorType: {
			String result;
			bool unusedCI;
			prepareLitString( result, unusedCI, 
					item->factor->typeRef->pdaLiteral->data,
					item->factor->typeRef->pdaLiteral->loc );

			/* Make sure we have this string. */
			StringMapEl *mapEl = 0;
			if ( pd->literalStrings.insert( result, &mapEl ) )
				mapEl->value = pd->literalStrings.length()-1;

			code.append( IN_LOAD_STR );
			code.appendWord( mapEl->value );
			break;
		}
		case ConsItem::InputText: {
			/* Make sure we have this string. */
			StringMapEl *mapEl = 0;
			if ( pd->literalStrings.insert( item->data, &mapEl ) )
				mapEl->value = pd->literalStrings.length()-1;

			code.append( IN_LOAD_STR );
			code.appendWord( mapEl->value );
			break;
		}
		case ConsItem::ExprType:
			item->expr->evaluate( pd, code );
			break;
		}

		code.append( IN_TOP_SWAP );

		/* Not a stream. Get the input first. */
		code.append( IN_GET_INPUT );
		if ( pd->revertOn )
			code.append( IN_INPUT_APPEND_WV );
		else
			code.append( IN_INPUT_APPEND_WC );
		code.append( IN_POP );

		code.append( IN_DUP_TOP );

		parseFrag( pd, code, 0 );
	}

	return varUt;
}


UniqueType *LangTerm::evaluateEmbedString( Compiler *pd, CodeVect &code ) const
{
	/* Assign bind ids to the variables in the replacement. */
	for ( ConsItemList::Iter item = *consItemList; item.lte(); item++ ) {
		switch ( item->type ) {
		case ConsItem::FactorType: {
			String result;
			bool unusedCI;
			prepareLitString( result, unusedCI, 
					item->factor->typeRef->pdaLiteral->data,
					item->factor->typeRef->pdaLiteral->loc );

			/* Make sure we have this string. */
			StringMapEl *mapEl = 0;
			if ( pd->literalStrings.insert( result, &mapEl ) )
				mapEl->value = pd->literalStrings.length()-1;

			code.append( IN_LOAD_STR );
			code.appendWord( mapEl->value );
			break;
		}
		case ConsItem::InputText: {
			/* Make sure we have this string. */
			StringMapEl *mapEl = 0;
			if ( pd->literalStrings.insert( item->data, &mapEl ) )
				mapEl->value = pd->literalStrings.length()-1;

			code.append( IN_LOAD_STR );
			code.appendWord( mapEl->value );
			break;
		}
		case ConsItem::ExprType:
			item->expr->evaluate( pd, code );
			break;
		}

	}

	long items = consItemList->length();
	for ( long i = 0; i < items-1; i++ )
		code.append( IN_CONCAT_STR );

	return pd->uniqueTypeStr;
}

UniqueType *LangTerm::evaluate( Compiler *pd, CodeVect &code ) const
{
	switch ( type ) {
		case VarRefType:
			return varRef->evaluate( pd, code );
		case MethodCallType:
			return varRef->evaluateCall( pd, code, args );
		case NilType:
			code.append( IN_LOAD_NIL );
			return pd->uniqueTypeNil;
		case TrueType:
			code.append( IN_LOAD_TRUE );
			return pd->uniqueTypeBool;
		case FalseType:
			code.append( IN_LOAD_FALSE );
			return pd->uniqueTypeBool;
		case MakeTokenType:
			return evaluateMakeToken( pd, code );
		case MakeTreeType:
			return evaluateMakeTree( pd, code );
		case NumberType: {
			unsigned int n = atoi( data );
			code.append( IN_LOAD_INT );
			code.appendWord( n );
			return pd->uniqueTypeInt;
		}
		case StringType: {
			String interp;
			bool unused;
			prepareLitString( interp, unused, data, InputLoc() );

			/* Make sure we have this string. */
			StringMapEl *mapEl = 0;
			if ( pd->literalStrings.insert( interp, &mapEl ) )
				mapEl->value = pd->literalStrings.length()-1;

			code.append( IN_LOAD_STR );
			code.appendWord( mapEl->value );
			return pd->uniqueTypeStr;
		}
		case MatchType:
			return evaluateMatch( pd, code );
		case ParseType:
			return evaluateParse( pd, code, false, false );
		case ParseStopType:
			return evaluateParse( pd, code, true, false );
		case ConstructType:
			return evaluateConstruct( pd, code );
		case SendType:
			return evaluateSend( pd, code );
		case NewType:
			return evaluateNew( pd, code );
		case TypeIdType: {
			/* Evaluate the expression. */
			UniqueType *ut = typeRef->uniqueType;
			if ( ut->typeId != TYPE_TREE )
				error() << "typeid can only be applied to tree types" << endp;

			code.append( IN_LOAD_INT );
			code.appendWord( ut->langEl->id );
			return pd->uniqueTypeInt;
		}
		case SearchType: {
			/* Evaluate the expression. */
			UniqueType *ut = typeRef->uniqueType;
			if ( ut->typeId != TYPE_TREE )
				error(loc) << "can only search for tree types" << endp;

			UniqueType *treeUT = varRef->evaluate( pd, code );
			if ( treeUT->typeId != TYPE_TREE )
				error(loc) << "search can be applied only to tree types" << endl;

			code.append( IN_TREE_SEARCH );
			code.appendWord( ut->langEl->id );
			return ut;
		};
		case EmbedStringType: {
			return evaluateEmbedString( pd, code );
		}
	}
	return 0;
}

UniqueType *LangExpr::evaluate( Compiler *pd, CodeVect &code ) const
{
	switch ( type ) {
		case BinaryType: {
			switch ( op ) {
				case '+': {
					UniqueType *lt = left->evaluate( pd, code );
					UniqueType *rt = right->evaluate( pd, code );

					if ( lt == pd->uniqueTypeInt && rt == pd->uniqueTypeInt ) {
						code.append( IN_ADD_INT );
						return pd->uniqueTypeInt;
					}

					if ( lt == pd->uniqueTypeStr && rt == pd->uniqueTypeStr ) {
						code.append( IN_CONCAT_STR );
						return pd->uniqueTypeStr;
					}

					error(loc) << "do not have an addition operator for these types" << endp;
					break;
				}
				case '-': {
					UniqueType *lt = left->evaluate( pd, code );
					UniqueType *rt = right->evaluate( pd, code );

					if ( lt == pd->uniqueTypeInt && rt == pd->uniqueTypeInt ) {
						code.append( IN_SUB_INT );
						return pd->uniqueTypeInt;
					}

					error(loc) << "do not have an addition operator for these types" << endp;
					break;
				}
				case '*': {
					UniqueType *lt = left->evaluate( pd, code );
					UniqueType *rt = right->evaluate( pd, code );

					if ( lt == pd->uniqueTypeInt && rt == pd->uniqueTypeInt ) {
						code.append( IN_MULT_INT );
						return pd->uniqueTypeInt;
					}

					error(loc) << "do not have an multiplication "
							"operator for these types" << endp;
					break;
				}
				case '/': {
					UniqueType *lt = left->evaluate( pd, code );
					UniqueType *rt = right->evaluate( pd, code );

					if ( lt == pd->uniqueTypeInt && rt == pd->uniqueTypeInt ) {
						code.append( IN_DIV_INT );
						return pd->uniqueTypeInt;
					}

					error(loc) << "do not have an division"
							"operator for these types" << endp;
					break;
				}
				case OP_DoubleEql: {
					UniqueType *lt = left->evaluate( pd, code );
					UniqueType *rt = right->evaluate( pd, code );

					if ( lt != rt )
						error(loc) << "comparison of different types" << endp;
						
					code.append( IN_TST_EQL );
					return pd->uniqueTypeBool;
				}
				case OP_NotEql: {
					UniqueType *lt = left->evaluate( pd, code );
					UniqueType *rt = right->evaluate( pd, code );

					if ( lt != rt )
						error(loc) << "comparison of different types" << endp;

					code.append( IN_TST_NOT_EQL );
					return pd->uniqueTypeBool;
				}
				case '<': {
					left->evaluate( pd, code );
					right->evaluate( pd, code );

					code.append( IN_TST_LESS );
					return pd->uniqueTypeBool;
				}
				case '>': {
					left->evaluate( pd, code );
					right->evaluate( pd, code );

					code.append( IN_TST_GRTR );
					return pd->uniqueTypeBool;
				}
				case OP_LessEql: {
					left->evaluate( pd, code );
					right->evaluate( pd, code );

					code.append( IN_TST_LESS_EQL );
					return pd->uniqueTypeBool;
				}
				case OP_GrtrEql: {
					left->evaluate( pd, code );
					right->evaluate( pd, code );

					code.append( IN_TST_GRTR_EQL );
					return pd->uniqueTypeBool;
				}
				case OP_LogicalAnd: {
					/* Evaluate the left and duplicate it. */
					left->evaluate( pd, code );
					code.append( IN_DUP_TOP );

					/* Jump over the right if false, leaving the original left
					 * result on the top of the stack. We don't know the
					 * distance yet so record the position of the jump. */
					long jump = code.length();
					code.append( IN_JMP_FALSE );
					code.appendHalf( 0 );

					/* Evauluate the right, add the test. Store it separately. */
					right->evaluate( pd, code );
					code.append( IN_TST_LOGICAL_AND );

					/* Set the distance of the jump. */
					long distance = code.length() - jump - 3;
					code.setHalf( jump+1, distance );

					return pd->uniqueTypeInt;
				}
				case OP_LogicalOr: {
					/* Evaluate the left and duplicate it. */
					left->evaluate( pd, code );
					code.append( IN_DUP_TOP );

					/* Jump over the right if true, leaving the original left
					 * result on the top of the stack. We don't know the
					 * distance yet so record the position of the jump. */
					long jump = code.length();
					code.append( IN_JMP_TRUE );
					code.appendHalf( 0 );

					/* Evauluate the right, add the test. */
					right->evaluate( pd, code );
					code.append( IN_TST_LOGICAL_OR );

					/* Set the distance of the jump. */
					long distance = code.length() - jump - 3;
					code.setHalf( jump+1, distance );

					return pd->uniqueTypeInt;
				}
			}

			assert(false);
			return 0;
		}
		case UnaryType: {
			switch ( op ) {
				case '!': {
					/* Evaluate the left and duplicate it. */
					right->evaluate( pd, code );
					code.append( IN_NOT );
					return pd->uniqueTypeBool;
				}
				case '$': {
					right->evaluate( pd, code );
					code.append( IN_TREE_TO_STR );
					return pd->uniqueTypeStr;
					
				}
				case '%': {
					right->evaluate( pd, code );
					code.append( IN_TREE_TO_STR_NOTRIM );
					return pd->uniqueTypeStr;
				}
				case '^': {
					UniqueType *rt = right->evaluate( pd, code );
					code.append( IN_TREE_TRIM );
					return rt;
				}
				case OP_Deref: {
					UniqueType *ut = right->evaluate( pd, code );
					if ( ut->typeId != TYPE_PTR )
						error(loc) << "can only dereference pointers" << endl;

					code.append( IN_PTR_DEREF_R );
					ut = pd->findUniqueType( TYPE_TREE, ut->langEl );
					return ut;
				}
				default: 
					assert(false);
			}
			return 0;
		}
		case TermType: {
			return term->evaluate( pd, code );
		}
	}
	return 0;
}

void LangVarRef::assignValue( Compiler *pd, CodeVect &code, 
		UniqueType *exprUT ) const
{
	/* Lookup the left hand side of the assignment. */
	VarRefLookup lookup = lookupField( pd );

	if ( lookup.objField->refActive )
		error(loc) << "reference active, cannot write to object" << endp;

	if ( lookup.firstConstPart >= 0 ) {
		error(loc) << "left hand side qualification \"" <<
			qual->data[lookup.firstConstPart].data << "\" is const" << endp;
	}

	if ( lookup.objField->isConst )
		error(loc) << "field \"" << name << "\" is const" << endp;

	/* Writing guarantees the field is dirty. tree is dirty. */
	lookup.objField->dirtyTree = true;

	/* Check the types of the assignment and possibly cast. */
	UniqueType *objUT = lookup.objField->typeRef->uniqueType;
	assert( lookup.uniqueType == lookup.objField->typeRef->uniqueType );
	if ( !castAssignment( pd, code, objUT, lookup.iterSearchUT, exprUT ) )
		error(loc) << "type mismatch in assignment" << endp;
	
	/* Decide if we need to revert the assignment. */
	bool revert = lookup.lastPtrInQual >= 0 || !isLocalRef(pd);

	/* Load the object and generate the field setting code. */
	loadObj( pd, code, lookup.lastPtrInQual, true );

	if ( lookup.uniqueType->typeId == TYPE_ITER )
		setFieldIter( pd, code, lookup.inObject, lookup.uniqueType, exprUT, false );
	else
		setField( pd, code, lookup.inObject, exprUT, revert );
}

UniqueType *LangTerm::evaluateMakeToken( Compiler *pd, CodeVect &code ) const
{
//	if ( pd->compileContext != Compiler::CompileTranslation )
//		error(loc) << "make_token can be used only in a translation block" << endp;

	/* Match the number of arguments. */
	int numArgs = args != 0 ? args->length() : 0;
	if ( numArgs < 2 )
		error(loc) << "need at least two arguments" << endp;

	for ( ExprVect::Iter pe = *args; pe.lte(); pe++ ) {
		/* Evaluate. */
		UniqueType *exprUT = (*pe)->evaluate( pd, code );

		if ( pe.pos() == 0 && exprUT != pd->uniqueTypeInt )
			error(loc) << "first arg, id, must be an int" << endp;

		if ( pe.pos() == 1 && exprUT != pd->uniqueTypeStr )
			error(loc) << "second arg, length, must be a string" << endp;
	}

	/* The token is now created, send it. */
	code.append( IN_MAKE_TOKEN );
	code.append( args->length() );

	return pd->uniqueTypeAny;
}

UniqueType *LangTerm::evaluateMakeTree( Compiler *pd, CodeVect &code ) const
{
	if ( pd->compileContext != Compiler::CompileTranslation )
		error(loc) << "make_tree can be used only in a translation block" << endp;

	/* Match the number of arguments. */
	int numArgs = args != 0 ? args->length() : 0;
	if ( numArgs < 1 )
		error(loc) << "need at least one argument" << endp;

	for ( ExprVect::Iter pe = *args; pe.lte(); pe++ ) {
		/* Evaluate. */
		UniqueType *exprUT = (*pe)->evaluate( pd, code );

		if ( pe.pos() == 0 && exprUT != pd->uniqueTypeInt )
			error(loc) << "first arg, nonterm id, must be an int" << endp;
	}

	/* The token is now created, send it. */
	code.append( IN_MAKE_TREE );
	code.append( args->length() );

	return pd->uniqueTypeAny;
}

void LangStmt::compileForIterBody( Compiler *pd, 
		CodeVect &code, UniqueType *iterUT ) const
{
	/* Remember the top of the loop. */
	long top = code.length();

	/* Advance */
	code.append( iterUT->iterDef->inAdvance );
	code.appendHalf( objField->offset );

	/* Test: jump past the while block if false. Note that we don't have the
	 * distance yet. */
	long jumpFalse = code.length();
	code.append( IN_JMP_FALSE );
	code.appendHalf( 0 );

	/*
	 * Set up the loop cleanup code. 
	 */

	/* Set up the current loop cleanup. */
	CodeVect loopCleanup;
	if ( pd->loopCleanup != 0 )
		loopCleanup.setAs( *pd->loopCleanup );

	/* Add the cleanup for the current loop. */
	loopCleanup.append( iterUT->iterDef->inDestroy );
	loopCleanup.appendHalf( objField->offset );

	/* Push the loop cleanup. */
	CodeVect *oldLoopCleanup = pd->loopCleanup;
	pd->loopCleanup = &loopCleanup;

	/* Compile the contents. */
	for ( StmtList::Iter stmt = *stmtList; stmt.lte(); stmt++ )
		stmt->compile( pd, code );

	pd->loopCleanup = oldLoopCleanup;

	/* Jump back to the top to retest. */
	long retestDist = code.length() - top + 3;
	code.append( IN_JMP );
	code.appendHalf( -retestDist );

	/* Set the jump false distance. */
	long falseDist = code.length() - jumpFalse - 3;
	code.setHalf( jumpFalse+1, falseDist );

	/* Compute the jump distance for the break jumps. */
	for ( LongVect::Iter brk = pd->breakJumps; brk.lte(); brk++ ) {
		long distance = code.length() - *brk - 3;
		code.setHalf( *brk+1, distance );
	}
	pd->breakJumps.empty();

	/* Destroy the iterator. */
	code.append( iterUT->iterDef->inDestroy );
	code.appendHalf( objField->offset );

	/* Clean up any prepush args. */
}

LangTerm *LangStmt::chooseDefaultIter( Compiler *pd, LangTerm *fromVarRef ) const
{
	/* Lookup the lang term and decide what iterator to use based
	 * on its type. */
	VarRefLookup lookup = fromVarRef->varRef->lookupField( pd );
	
	if ( lookup.inObject->type != ObjectDef::FrameType )
		error(loc) << "root of iteration must be a local" << endp;
	
	LangVarRef *callVarRef = 0;
	if ( lookup.uniqueType->typeId == TYPE_TREE || 
			lookup.uniqueType->typeId == TYPE_REF ||
			lookup.uniqueType->typeId == TYPE_ITER ||
			lookup.uniqueType->typeId == TYPE_PTR )
	{
		/* The iterator name. */
		callVarRef = LangVarRef::cons( loc, "triter" );
	}
	else {
		error(loc) << "there is no default iterator for a "
				"root of that type" << endp;
	}

	/* The parameters. */
	ExprVect *callExprVect = new ExprVect;
	LangExpr *callExpr = LangExpr::cons( LangTerm::cons( 
			InputLoc(), LangTerm::VarRefType, fromVarRef->varRef ) );
	callExprVect->append( callExpr );

	LangTerm *callLangTerm = LangTerm::cons( InputLoc(), callVarRef, callExprVect );

	return callLangTerm;
}

void LangStmt::compileForIter( Compiler *pd, CodeVect &code ) const
{
	code.append( IN_CONTIGUOUS );
	code.appendHalf( 512 );

	pd->curLocalFrame->iterPushScope();

	LangTerm *iterCallTerm = langTerm;
	if ( iterCallTerm->type != LangTerm::MethodCallType )
		iterCallTerm = chooseDefaultIter( pd, langTerm );

	/* The type we are searching for. */
	UniqueType *searchUT = typeRef->uniqueType;

	/* 
	 * Declare the iterator variable.
	 */
	VarRefLookup lookup = iterCallTerm->varRef->lookupMethod( pd );
	if ( lookup.objMethod->iterDef == 0 ) {
		error(loc) << "attempt to iterate using something "
				"that is not an iterator" << endp;
	}

	/* Now that we have done the iterator call lookup we can make the type
	 * reference for the object field. */
	UniqueType *iterUniqueType = pd->findUniqueType( TYPE_ITER, lookup.objMethod->iterDef );
	objField->typeRef = TypeRef::cons( loc, lookup.objMethod->iterDef, iterUniqueType, searchUT );

	/* Also force the field to be initialized. */
	pd->curLocalFrame->initField( pd, objField );

	/* 
	 * Create the iterator from the local var.
	 */

	UniqueType *iterUT = objField->typeRef->uniqueType;

	/* Evaluate and push the arguments. */
	ObjField **paramRefs = iterCallTerm->varRef->evaluateArgs( 
			pd, code, lookup, iterCallTerm->args );

	if ( pd->revertOn )
		code.append( iterUT->iterDef->inCreateWV );
	else
		code.append( iterUT->iterDef->inCreateWC );

	code.appendHalf( objField->offset );
	if ( lookup.objMethod->func != 0 )
		code.appendHalf( lookup.objMethod->func->funcId );

	if ( iterUT->iterDef->useSearchUT ) {
		if ( searchUT->typeId == TYPE_PTR )
			code.appendHalf( pd->uniqueTypePtr->langEl->id );
		else
			code.appendHalf( searchUT->langEl->id );
	}

	compileForIterBody( pd, code, iterUT );

	iterCallTerm->varRef->popRefQuals( pd, code, lookup, iterCallTerm->args );

	iterCallTerm->varRef->resetActiveRefs( pd, lookup, paramRefs );
	delete[] paramRefs;

	pd->curLocalFrame->iterPopScope();
}

void LangStmt::compileWhile( Compiler *pd, CodeVect &code ) const
{
	pd->curLocalFrame->iterPushScope();

	/* Generate code for the while test. Remember the top. */
	long top = code.length();
	expr->evaluate( pd, code );

	/* Jump past the while block if false. Note that we don't have the
	 * distance yet. */
	long jumpFalse = code.length();
	code.append( IN_JMP_FALSE );
	code.appendHalf( 0 );

	/* Compute the while block. */
	for ( StmtList::Iter stmt = *stmtList; stmt.lte(); stmt++ )
		stmt->compile( pd, code );

	/* Jump back to the top to retest. */
	long retestDist = code.length() - top + 3;
	code.append( IN_JMP );
	code.appendHalf( -retestDist );

	/* Set the jump false distance. */
	long falseDist = code.length() - jumpFalse - 3;
	code.setHalf( jumpFalse+1, falseDist );

	/* Compute the jump distance for the break jumps. */
	for ( LongVect::Iter brk = pd->breakJumps; brk.lte(); brk++ ) {
		long distance = code.length() - *brk - 3;
		code.setHalf( *brk+1, distance );
	}
	pd->breakJumps.empty();

	pd->curLocalFrame->iterPopScope();
}

void LangStmt::compile( Compiler *pd, CodeVect &code ) const
{
	switch ( type ) {
		case PrintType: 
		case PrintXMLACType:
		case PrintXMLType:
		case PrintStreamType: {
			UniqueType **types = new UniqueType*[exprPtrVect->length()];
			
			/* Push the args backwards. */
			for ( ExprVect::Iter pex = exprPtrVect->last(); pex.gtb(); pex-- )
				types[pex.pos()] = (*pex)->evaluate( pd, code );

			/* Run the printing forwards. */
			if ( type == PrintType ) {
				code.append( IN_PRINT );
				code.append( exprPtrVect->length() );
			}
			else if ( type == PrintXMLACType ) {
				code.append( IN_PRINT_XML_AC );
				code.append( exprPtrVect->length() );
			}
			else if ( type == PrintXMLType ) {
				code.append( IN_PRINT_XML );
				code.append( exprPtrVect->length() );
			}
			else if ( type == PrintStreamType ) {
				/* Minus one because the first arg is the stream. */
				code.append( IN_PRINT_STREAM );
				code.append( exprPtrVect->length() - 1 );
			}

			delete[] types;

			break;
		}
		case ExprType: {
			/* Evaluate the exrepssion, then pop it immediately. */
			expr->evaluate( pd, code );
			code.append( IN_POP );
			break;
		}
		case IfType: {
			pd->curLocalFrame->iterPushScope();

			long jumpFalse = 0, jumpPastElse = 0, distance = 0;

			/* Evaluate the test. */
			expr->evaluate( pd, code );

			/* Jump past the if block if false. We don't know the distance
			 * yet so store the location of the jump. */
			jumpFalse = code.length();
			code.append( IN_JMP_FALSE );
			code.appendHalf( 0 );

			/* Compile the if true branch. */
			for ( StmtList::Iter stmt = *stmtList; stmt.lte(); stmt++ )
				stmt->compile( pd, code );

			if ( elsePart != 0 ) {
				/* Jump past the else code for the if true branch. */
				jumpPastElse = code.length();
				code.append( IN_JMP );
				code.appendHalf( 0 );
			}

			/* Set the distance for the jump false case. */
			distance = code.length() - jumpFalse - 3;
			code.setHalf( jumpFalse+1, distance );

			pd->curLocalFrame->iterPopScope();

			if ( elsePart != 0 ) {
				/* Compile the else branch. */
				elsePart->compile( pd, code );

				/* Set the distance for jump over the else part. */
				distance = code.length() - jumpPastElse - 3;
				code.setHalf( jumpPastElse+1, distance );
			}

			break;
		}
		case ElseType: {
			pd->curLocalFrame->iterPushScope();

			/* Compile the else branch. */
			for ( StmtList::Iter stmt = *stmtList; stmt.lte(); stmt++ )
				stmt->compile( pd, code );

			pd->curLocalFrame->iterPopScope();
			break;
		}
		case RejectType: {
			code.append( IN_REJECT );
			break;
		}
		case WhileType: {
			compileWhile( pd, code );
			break;
		}
		case AssignType: {
			/* Evaluate the exrepssion. */
			UniqueType *exprUT = expr->evaluate( pd, code );

			/* Do the assignment. */
			varRef->assignValue( pd, code, exprUT );
			break;
		}
		case ForIterType: {
			compileForIter( pd, code );
			break;
		}
		case ReturnType: {
			/* Evaluate the exrepssion. */
			UniqueType *exprUT = expr->evaluate( pd, code );

			if ( pd->curFunction == 0 ) {
				/* In the main function */
				pd->mainReturnUT = exprUT;
			}
			else {
				UniqueType *resUT = pd->curFunction->typeRef->uniqueType;
				if ( !castAssignment( pd, code, resUT, 0, exprUT ) )
					error(loc) << "return value wrong type" << endp;
			}

			code.append( IN_SAVE_RET );

			/* The loop cleanup code. */
			if ( pd->loopCleanup != 0 )
				code.append( *pd->loopCleanup );

			/* Jump to the return label. The distnacnce will be filled in
			 * later. */
			pd->returnJumps.append( code.length() );
			code.append( IN_JMP );
			code.appendHalf( 0 );
			break;
		}
		case BreakType: {
			pd->breakJumps.append( code.length() );
			code.append( IN_JMP );
			code.appendHalf( 0 );
			break;
		}
		case YieldType: {
			/* take a reference and yield it. Immediately reset the referece. */
			varRef->preEvaluateRef( pd, code );
			ObjField *objField = varRef->evaluateRef( pd, code, 0 );
			code.append( IN_YIELD );

			if ( varRef->qual->length() > 0 ) {
				code.append( IN_POP_N_WORDS );
				code.appendHalf( (short)(varRef->qual->length()*2) );
			}

			objField->refActive = false;
			break;
		}
	}
}

void CodeBlock::compile( Compiler *pd, CodeVect &code ) const
{
	for ( StmtList::Iter stmt = *stmtList; stmt.lte(); stmt++ )
		stmt->compile( pd, code );
}

void Compiler::addMatchLength( ObjectDef *frame, LangEl *lel )
{
	/* Make the type ref. */
	TypeRef *typeRef = TypeRef::cons( InputLoc(), uniqueTypeInt );

	/* Create the field and insert it into the map. */
	ObjField *el = new ObjField( InputLoc(), typeRef, "match_length" );
	el->beenReferenced = true;
	el->beenInitialized = true;
	el->isConst = true;
	el->useOffset = false;
	el->inGetR    = IN_GET_MATCH_LENGTH_R;
	frame->insertField( el->name, el );
}

void Compiler::addMatchText( ObjectDef *frame, LangEl *lel )
{
	/* Make the type ref. */
	TypeRef *typeRef = TypeRef::cons( InputLoc(), uniqueTypeStr );

	/* Create the field and insert it into the map. */
	ObjField *el = new ObjField( InputLoc(), typeRef, "match_text" );
	el->beenReferenced = true;
	el->beenInitialized = true;
	el->isConst = true;
	el->useOffset = false;
	el->inGetR    = IN_GET_MATCH_TEXT_R;
	frame->insertField( el->name, el );
}

void Compiler::addInput( ObjectDef *frame )
{
	/* Make the type ref. */
	TypeRef *typeRef = TypeRef::cons( InputLoc(), uniqueTypeInput );

	/* Create the field and insert it into the map. */
	ObjField *el = new ObjField( InputLoc(), typeRef, "input" );
	el->beenReferenced = true;
	el->beenInitialized = true;
	el->isConst   = false;
	el->useOffset = false;
	el->isCustom  = true;
	el->inGetR    = IN_LOAD_INPUT_R;
	el->inGetWV   = IN_LOAD_INPUT_WV;
	el->inGetWC   = IN_LOAD_INPUT_WC;
	frame->insertField( el->name, el );
}

void Compiler::addCtx( ObjectDef *frame )
{
	/* Make the type ref. */
	TypeRef *typeRef = TypeRef::cons( InputLoc(), uniqueTypeStream );

	/* Create the field and insert it into the map. */
	ObjField *el = new ObjField( InputLoc(), typeRef, "ctx" );
	el->beenReferenced = true;
	el->beenInitialized = true;
	el->isConst   = false;
	el->useOffset = false;
	el->isCustom  = true;
	el->inGetR    = IN_LOAD_CTX_R;
	el->inGetWV   = IN_LOAD_CTX_WV;
	el->inGetWC   = IN_LOAD_CTX_WC;
	frame->insertField( el->name, el );
}

void Compiler::initFieldInstructions( ObjField *el )
{
	el->inGetR =   IN_GET_FIELD_R;
	el->inGetWC =  IN_GET_FIELD_WC;
	el->inGetWV =  IN_GET_FIELD_WV;
	el->inSetWC =  IN_SET_FIELD_WC;
	el->inSetWV =  IN_SET_FIELD_WV;
}

void Compiler::initLocalInstructions( ObjField *el )
{
	el->inGetR =   IN_GET_LOCAL_R;
	el->inGetWC =  IN_GET_LOCAL_WC;
	el->inSetWC =  IN_SET_LOCAL_WC;
}

void Compiler::initLocalRefInstructions( ObjField *el )
{
	el->inGetR =   IN_GET_LOCAL_REF_R;
	el->inGetWC =  IN_GET_LOCAL_REF_WC;
	el->inSetWC =  IN_SET_LOCAL_REF_WC;
}

void Compiler::initIntObject( )
{
	intObj = new ObjectDef( ObjectDef::BuiltinType, "int", nextObjectId++ );
	intLangEl->objectDef = intObj;

	initFunction( uniqueTypeStr, intObj, "to_string", IN_INT_TO_STR, IN_INT_TO_STR, true );
}

/* Add a constant length field to the object. 
 * Opcode supplied by the caller. */
void Compiler::addLengthField( ObjectDef *objDef, Code getLength )
{
	/* Create the "length" field. */
	TypeRef *typeRef = TypeRef::cons( InputLoc(), uniqueTypeInt );
	ObjField *el = new ObjField( InputLoc(), typeRef, "length" );
	el->beenReferenced = true;
	el->beenInitialized = true;
	el->isConst = true;
	el->useOffset = false;
	el->inGetR = getLength;

	objDef->insertField( el->name, el );
}

void Compiler::initStrObject( )
{
	strObj = new ObjectDef( ObjectDef::BuiltinType, "str", nextObjectId++ );
	strLangEl->objectDef = strObj;

	initFunction( uniqueTypeInt, strObj, "atoi",   IN_STR_ATOI, IN_STR_ATOI, true );
	initFunction( uniqueTypeInt, strObj, "uord8",  IN_STR_UORD8,  IN_STR_UORD8, true );
	initFunction( uniqueTypeInt, strObj, "sord8",  IN_STR_SORD8,  IN_STR_SORD8, true );
	initFunction( uniqueTypeInt, strObj, "uord16", IN_STR_UORD16, IN_STR_UORD16, true );
	initFunction( uniqueTypeInt, strObj, "sord16", IN_STR_SORD16, IN_STR_SORD16, true );
	initFunction( uniqueTypeInt, strObj, "uord32", IN_STR_UORD32, IN_STR_UORD32, true );
	initFunction( uniqueTypeInt, strObj, "sord32", IN_STR_SORD32, IN_STR_SORD32, true );
	addLengthField( strObj, IN_STR_LENGTH );

	initFunction( uniqueTypeStr, globalObjectDef, "sprintf", 
			IN_SPRINTF, IN_SPRINTF, uniqueTypeStr, uniqueTypeInt, true );
}

void Compiler::initStreamObject( )
{
	streamObj = new ObjectDef( ObjectDef::BuiltinType,
			"stream", nextObjectId++ );
	streamLangEl->objectDef = streamObj;
}

void Compiler::initInputObject( )
{
	inputObj = new ObjectDef( ObjectDef::BuiltinType,
			"accum_stream", nextObjectId++ );
	inputLangEl->objectDef = inputObj;

	initFunction( uniqueTypeStr, inputObj, "pull",  
			IN_INPUT_PULL_WV, IN_INPUT_PULL_WV, uniqueTypeInt, false );
	initFunction( uniqueTypeStr, inputObj, "push",  
			IN_INPUT_PUSH_WV, IN_INPUT_PUSH_WV, uniqueTypeAny, false );
	initFunction( uniqueTypeStr, inputObj, "push_ignore",  
			IN_INPUT_PUSH_IGNORE_WV, IN_INPUT_PUSH_IGNORE_WV, uniqueTypeAny, false );
}

ObjField *Compiler::makeDataEl()
{
	/* Create the "data" field. */
	TypeRef *typeRef = TypeRef::cons( InputLoc(), uniqueTypeStr );
	ObjField *el = new ObjField( InputLoc(), typeRef, "data" );

	/* Setting beenReferenced to true prevents us from assigning instructions
	 * and an offset to the field. */

	el->beenReferenced = true;
	el->beenInitialized = true;
	el->useOffset = false;
	el->inGetR = IN_GET_TOKEN_DATA_R;
	el->inSetWC = IN_SET_TOKEN_DATA_WC;
	el->inSetWV = IN_SET_TOKEN_DATA_WV;
	return el;
}

ObjField *Compiler::makePosEl()
{
	/* Create the "data" field. */
	TypeRef *typeRef = TypeRef::cons( InputLoc(), uniqueTypeInt );
	ObjField *el = new ObjField( InputLoc(), typeRef, "pos" );

	/* Setting beenReferenced to true prevents us from assigning instructions
	 * and an offset to the field. */

	el->isConst = true;
	el->beenReferenced = true;
	el->beenInitialized = true;
	el->useOffset = false;
	el->inGetR = IN_GET_TOKEN_POS_R;
	return el;
}

ObjField *Compiler::makeLineEl()
{
	/* Create the "data" field. */
	TypeRef *typeRef = TypeRef::cons( InputLoc(), uniqueTypeInt );
	ObjField *el = new ObjField( InputLoc(), typeRef, "line" );

	/* Setting beenReferenced to true prevents us from assigning instructions
	 * and an offset to the field. */

	el->isConst = true;
	el->beenReferenced = true;
	el->beenInitialized = true;
	el->useOffset = false;
	el->inGetR = IN_GET_TOKEN_LINE_R;
	return el;
}

void Compiler::initTokenObjects( )
{
	/* Make a default object Production. */
	tokenObj = new ObjectDef( ObjectDef::BuiltinType, "token", nextObjectId++ );

	ObjField *dataEl = makeDataEl();
	tokenObj->insertField( dataEl->name, dataEl );

	ObjField *posEl = makePosEl();
	tokenObj->insertField( posEl->name, posEl );

	ObjField *lineEl = makeLineEl();
	tokenObj->insertField( lineEl->name, lineEl );

	/* Give all user terminals the token object type. */
	for ( LelList::Iter lel = langEls; lel.lte(); lel++ ) {
		if ( lel->isUserTerm ) {
			if ( lel->objectDef == 0 )
				lel->objectDef = tokenObj;
			else {
				/* Create the "data" field. */
				ObjField *dataEl = makeDataEl();
				lel->objectDef->insertField( dataEl->name, dataEl );

				/* Create the "pos" field. */
				ObjField *posEl = makePosEl();
				lel->objectDef->insertField( posEl->name, posEl );

				/* Create the "line" field. */
				ObjField *lineEl = makeLineEl();
				lel->objectDef->insertField( lineEl->name, lineEl );
			}
		}
	}
}

void Compiler::findLocalTrees( CharSet &trees )
{
	/* We exlcude "lhs" from being downrefed because we need to use if after
	 * the frame is is cleaned and so it must survive. */
	for ( ObjFieldList::Iter ol = *curLocalFrame->objFieldList; ol.lte(); ol++ ) {
		ObjField *el = ol->value;
		/* FIXME: This test needs to be improved. Match_text was getting
		 * through before useOffset was tested. What will? */
		if ( el->useOffset && !el->isLhsEl && ( el->beenReferenced || el->isParam ) ) {
			UniqueType *ut = el->typeRef->uniqueType;
			if ( ut->typeId == TYPE_TREE || ut->typeId == TYPE_PTR )
				trees.insert( el->offset );
		}
	}
}

void Compiler::makeProdCopies( Production *prod )
{
	int pos = 0;
	for ( ProdElList::Iter pel = *prod->prodElList; pel.lte(); pel++, pos++) {
		if ( pel->captureField != 0 ) {
			prod->copy.append( pel->captureField->offset );
			prod->copy.append( pos );
		}
	}
}

void Compiler::compileReductionCode( Production *prod )
{
	CodeBlock *block = prod->redBlock;

	/* Init the compilation context. */
	compileContext = CompileReduction;
	curLocalFrame = block->localFrame;
	revertOn = true;
	block->frameId = nextFrameId++;

	CodeVect &code = block->codeWV;

	/* Add the alloc frame opcode. We don't have the right 
	 * frame size yet. We will fill it in later. */
	code.append( IN_INIT_LOCALS );
	code.appendHalf( 0 );
	long afterInit = code.length();

	/* Compile the reduce block. */
	block->compile( this, code );

	/* We have the frame size now. Set in the alloc frame instruction. */
	long frameSize = curLocalFrame->size();
	code.setHalf( 1, frameSize );

	/* Might need to load right hand side values. */
	addProdRHSLoads( prod, code, afterInit );

	addProdLHSLoad( prod, code, afterInit );
	addPushBackLHS( prod, code, afterInit );

	code.append( IN_PCR_RET );

	/* Now that compilation is done variables are referenced. Make the local
	 * trees descriptor. */
	findLocalTrees( block->trees );
}

void Compiler::compileTranslateBlock( LangEl *langEl )
{
	CodeBlock *block = langEl->transBlock;

	/* Set up compilation context. */
	compileContext = CompileTranslation;
	curLocalFrame = block->localFrame;
	revertOn = true;
	block->frameId = nextFrameId++;

	/* References to the reduce item. */
	addMatchLength( curLocalFrame, langEl );
	addMatchText( curLocalFrame, langEl );
	addInput( curLocalFrame );
	addCtx( curLocalFrame );

	CodeVect &code = block->codeWV;

	/* Add the alloc frame opcode. We don't have the right 
	 * frame size yet. We will fill it in later. */
	code.append( IN_INIT_LOCALS );
	code.appendHalf( 0 );

	if ( langEl->tokenDef->reCaptureVect.length() > 0 ) {
		code.append( IN_INIT_CAPTURES );
		code.append( langEl->tokenDef->reCaptureVect.length() );

		ObjFieldList::Iter f = *curLocalFrame->objFieldList;
		for ( int i = 0; i < langEl->tokenDef->reCaptureVect.length(); i++, f++ )
			curLocalFrame->referenceField( this, f->value );
	}

	/* Set the local frame and compile the reduce block. */
	block->compile( this, code );

	/* We have the frame size now. Set in the alloc frame instruction. */
	long frameSize = curLocalFrame->size();
	code.setHalf( 1, frameSize );

	code.append( IN_PCR_RET );

	/* Now that compilation is done variables are referenced. Make the local
	 * trees descriptor. */
	findLocalTrees( block->trees );
}

void Compiler::compilePreEof( TokenRegion *region )
{
	CodeBlock *block = region->preEofBlock;

	/* Set up compilation context. */
	compileContext = CompileTranslation;
	curLocalFrame = region->preEofBlock->localFrame;
	revertOn = true;
	block->frameId = nextFrameId++;

	addInput( curLocalFrame );
	addCtx( curLocalFrame );

	CodeVect &code = block->codeWV;

	/* Add the alloc frame opcode. We don't have the right 
	 * frame size yet. We will fill it in later. */
	code.append( IN_INIT_LOCALS );
	code.appendHalf( 0 );

	/* Set the local frame and compile the reduce block. */
	block->compile( this, code );

	/* We have the frame size now. Set in the alloc frame instruction. */
	long frameSize = curLocalFrame->size();
	code.setHalf( 1, frameSize );

	code.append( IN_PCR_RET );

	/* Now that compilation is done variables are referenced. Make the local
	 * trees descriptor. */
	findLocalTrees( block->trees );
}

void Compiler::compileRootBlock( )
{
	CodeBlock *block = rootCodeBlock;

	/* The root block never needs to be reverted. */

	/* Set up the compile context. No locals are needed for the root code
	 * block, but we need an empty local frame for the compile. */
	compileContext = CompileRoot;
	curLocalFrame = rootLocalFrame;
	revertOn = false;

	/* The block needs a frame id. */
	block->frameId = nextFrameId++;

	/* The root block is not reverted. */
	CodeVect &code = block->codeWC;

	/* Add the alloc frame opcode. We don't have the right 
	 * frame size yet. We will fill it in later. */
	code.append( IN_INIT_LOCALS );
	code.appendHalf( 0 );

	code.append( IN_LOAD_ARGV );
	code.appendHalf( argvOffset() );

	block->compile( this, code );

	/* We have the frame size now. Store it in frame init. */
	long frameSize = curLocalFrame->size();
	code.setHalf( 1, frameSize );

	code.append( IN_STOP );

	/* Make the local trees descriptor. */
	findLocalTrees( block->trees );
}

void Compiler::initAllLanguageObjects()
{
	/* Init all user object fields (need consistent size). */
	for ( LelList::Iter lel = langEls; lel.lte(); lel++ ) {
		ObjectDef *objDef = lel->objectDef;
		if ( objDef != 0 ) {
			/* Init all fields of the object. */
			for ( ObjFieldList::Iter f = *objDef->objFieldList; f.lte(); f++ )
				objDef->initField( this, f->value );
		}
	}

	/* Init all fields of the global object. */
	for ( ObjFieldList::Iter f = *globalObjectDef->objFieldList; f.lte(); f++ )
		globalObjectDef->initField( this, f->value );
}

void Compiler::initMapFunctions( GenericType *gen )
{
	addLengthField( gen->objDef, IN_MAP_LENGTH );
	initFunction( gen->utArg, gen->objDef, "find", 
			IN_MAP_FIND,      IN_MAP_FIND, gen->keyUT, true );
	initFunction( uniqueTypeInt, gen->objDef, "insert", 
			IN_MAP_INSERT_WV, IN_MAP_INSERT_WC, gen->keyUT, gen->utArg, false );
	initFunction( uniqueTypeInt, gen->objDef, "store", 
			IN_MAP_STORE_WV,  IN_MAP_STORE_WC, gen->keyUT, gen->utArg, false );
	initFunction( gen->utArg, gen->objDef, "remove", 
			IN_MAP_REMOVE_WV, IN_MAP_REMOVE_WC, gen->keyUT, false );
}

void Compiler::initListFunctions( GenericType *gen )
{
	addLengthField( gen->objDef, IN_LIST_LENGTH );

	initFunction( uniqueTypeInt, gen->objDef, "append", 
			IN_LIST_APPEND_WV, IN_LIST_APPEND_WC, gen->utArg, false );
	initFunction( uniqueTypeInt, gen->objDef, "push", 
			IN_LIST_APPEND_WV, IN_LIST_APPEND_WC, gen->utArg, false );

	initFunction( gen->utArg, gen->objDef, "remove_end", 
			IN_LIST_REMOVE_END_WV, IN_LIST_REMOVE_END_WC, false );
	initFunction( gen->utArg, gen->objDef, "pop", 
			IN_LIST_REMOVE_END_WV, IN_LIST_REMOVE_END_WC, false );
}

void Compiler::initListField( GenericType *gen, const char *name, int offset )
{
	/* Make the type ref and create the field. */
	TypeRef *typeRef = TypeRef::cons( InputLoc(), gen->utArg );
	ObjField *el = new ObjField( InputLoc(), typeRef, name );

	el->inGetR =  IN_GET_LIST_MEM_R;
	el->inGetWC = IN_GET_LIST_MEM_WC;
	el->inGetWV = IN_GET_LIST_MEM_WV;
	el->inSetWC = IN_SET_LIST_MEM_WC;
	el->inSetWV = IN_SET_LIST_MEM_WV;

	gen->objDef->insertField( el->name, el );

	el->useOffset = true;
	el->beenReferenced = true;
	el->beenInitialized = true;

	/* Zero for head, One for tail. */
	el->offset = offset;
}

void Compiler::initListFields( GenericType *gen )
{
	initListField( gen, "head", 0 );
	initListField( gen, "tail", 1 );
	initListField( gen, "top", 1 );
}

void Compiler::initVectorFunctions( GenericType *gen )
{
	addLengthField( gen->objDef, IN_VECTOR_LENGTH );
	initFunction( uniqueTypeInt, gen->objDef, "append", 
			IN_VECTOR_APPEND_WV, IN_VECTOR_APPEND_WC, gen->utArg, false );
	initFunction( uniqueTypeInt, gen->objDef, "insert", 
			IN_VECTOR_INSERT_WV, IN_VECTOR_INSERT_WC, uniqueTypeInt, gen->utArg, false );
}

void Compiler::initParserFunctions( GenericType *gen )
{
	initFunction( gen->utArg, gen->objDef, "finish",
			IN_PARSE_FINISH_WV, IN_PARSE_FINISH_WC, true );

	initFunction( gen->utArg, gen->objDef, "eof",
			IN_PARSE_FINISH_WV, IN_PARSE_FINISH_WC, true );
}

void Compiler::initParserField( GenericType *gen, const char *name, int offset )
{
	/* Make the type ref and create the field. */
	TypeRef *typeRef = TypeRef::cons( InputLoc(), gen->utArg );
	ObjField *el = new ObjField( InputLoc(), typeRef, name );

	el->inGetR =  IN_GET_PARSER_MEM_R;
	el->inGetWC = IN_GET_PARSER_MEM_WC;
	el->inGetWV = IN_GET_PARSER_MEM_WV;
	el->inSetWC = IN_SET_PARSER_MEM_WC;
	el->inSetWV = IN_SET_PARSER_MEM_WV;

	gen->objDef->insertField( el->name, el );

	el->useOffset = true;
	el->beenReferenced = true;
	el->beenInitialized = true;

	/* Zero for head, One for tail. */
	el->offset = offset;
}

void Compiler::initCtxField( GenericType *gen )
{
	LangEl *langEl = gen->utArg->langEl;
	Context *context = langEl->contextIn;

	/* Make the type ref and create the field. */
	UniqueType *ctxUT = findUniqueType( TYPE_TREE, context->lel );
	TypeRef *typeRef = TypeRef::cons( InputLoc(), ctxUT );
	ObjField *el = new ObjField( InputLoc(), typeRef, "ctx" );

	el->inGetR =  IN_GET_PARSER_CTX_R;
	el->inGetWC = IN_GET_PARSER_CTX_WC;
	el->inGetWV = IN_GET_PARSER_CTX_WV;
	el->inSetWC = IN_SET_PARSER_CTX_WC;
	el->inSetWV = IN_SET_PARSER_CTX_WV;

	gen->objDef->insertField( el->name, el );

	el->useOffset = false;
	el->beenReferenced = true;
	el->beenInitialized = true;
}

void Compiler::initParserFields( GenericType *gen )
{
	LangEl *langEl = gen->utArg->langEl;
	if ( langEl->contextIn != 0 )
		initCtxField( gen );
	
	initParserField( gen, "tree", 0 );
}

void Compiler::initGenericTypes()
{
	for ( NamespaceList::Iter ns = namespaceList; ns.lte(); ns++ ) {
		for ( GenericList::Iter gen = ns->genericList; gen.lte(); gen++ ) {
			gen->utArg = gen->typeArg->uniqueType;
 
			if ( gen->typeId == GEN_MAP )
				gen->keyUT = gen->keyTypeArg->uniqueType; 

			gen->objDef = new ObjectDef( ObjectDef::BuiltinType, 
					gen->name, nextObjectId++ );

			switch ( gen->typeId ) {
				case GEN_MAP: 
					initMapFunctions( gen );
					break;
				case GEN_LIST:
					initListFunctions( gen );
					initListFields( gen );
					break;
				case GEN_VECTOR:
					initVectorFunctions( gen );
					break;
				case GEN_PARSER:
					/* Need to generate a parser for the type. */
					gen->utArg->langEl->parserId = nextParserId++;
					initParserFunctions( gen );
					initParserFields( gen );
					break;
			}

			gen->langEl->objectDef = gen->objDef;
		}
	}
}

void Compiler::makeFuncVisible( Function *func, bool isUserIter )
{
	func->localFrame = func->codeBlock->localFrame;

	/* Set up the parameters. */
	long paramPos = 0, paramListSize = 0;
	UniqueType **paramUTs = new UniqueType*[func->paramList->length()];
	for ( ParameterList::Iter param = *func->paramList; param.lte(); param++ ) {
		paramUTs[paramPos] = param->typeRef->uniqueType;

		if ( func->localFrame->findField( param->name ) != 0 )
			error(param->loc) << "parameter " << param->name << " redeclared" << endp;

		func->localFrame->insertField( param->name, param );
		param->beenInitialized = true;
		param->pos = paramPos;

		/* Initialize the object field as a local variable. We also want trees
		 * downreffed. */
		if ( paramUTs[paramPos]->typeId == TYPE_REF )
			initLocalRefInstructions( param );
		else
			initLocalInstructions( param );

		paramListSize += sizeOfField( paramUTs[paramPos] );
		paramPos += 1;
	}

	/* Param offset is relative to one past the last item in the array of
	 * words containing the args. */
	long paramOffset = 0;
	for ( ParameterList::Iter param = *func->paramList; param.lte(); param++ ) {
		/* Moving downward, and need the offset to point to the lower half of
		 * the argument. */
		paramOffset -= sizeOfField( paramUTs[param->pos] );

		/* How much space do we need to make for call overhead. */
		long frameAfterArgs = isUserIter ? IFR_AA : FR_AA;

		/* Going up first we have the frame data, then maybe
		 * the user iterator, then the args from high to low. */
		param->offset = frameAfterArgs + 
				( isUserIter ? ( sizeof(UserIter) / sizeof(Word) ) : 0 ) +
				paramListSize + paramOffset;
	}

	func->paramListSize = paramListSize;
	func->paramUTs = paramUTs;

	/* Insert the function into the global function map. */
	UniqueType *returnUT = func->typeRef != 0 ? 
			func->typeRef->uniqueType : uniqueTypeInt;
	ObjMethod *objMethod = new ObjMethod( returnUT, func->name, 
			IN_CALL_WV, IN_CALL_WC, 
			func->paramList->length(), paramUTs, func->paramList, false );
	objMethod->funcId = func->funcId;
	objMethod->useFuncId = true;
	objMethod->useCallObj = false;
	objMethod->func = func;

	if ( isUserIter ) {
		IterDef *uiter = findIterDef( IterDef::User, func );
		objMethod->iterDef = uiter;
	}

	globalObjectDef->objMethodMap->insert( func->name, objMethod );
}

void Compiler::compileUserIter( Function *func, CodeVect &code )
{
	CodeBlock *block = func->codeBlock;

	/* Add the alloc frame opcode. We don't have the right 
	 * frame size yet. We will fill it in later. */
	code.append( IN_INIT_LOCALS );
	code.appendHalf( 0 );

	/* Compile the block. */
	block->compile( this, code );

	/* We have the frame size now. Set in the alloc frame instruction. */
	int frameSize = func->localFrame->size();
	code.setHalf( 1, frameSize );

	/* Check for a return statement. */
	if ( block->stmtList->length() == 0 ||
			block->stmtList->tail->type != LangStmt::YieldType )
	{
		/* Push the return value. */
		code.append( IN_LOAD_NIL );
		code.append( IN_YIELD );
	}
}

void Compiler::compileUserIter( Function *func )
{
	CodeBlock *block = func->codeBlock;

	/* Set up the context. */
	compileContext = CompileFunction;
	curFunction = func;
	block->frameId = nextFrameId++;

	/* Need an object for the local frame. */
	curLocalFrame = func->codeBlock->localFrame;

	/* Compile for revert and commit. */
	revertOn = true;
	compileUserIter( func, block->codeWV );

	revertOn = false;
	compileUserIter( func, block->codeWC );

	/* Now that compilation is done variables are referenced. Make the local
	 * trees descriptor. */
	findLocalTrees( block->trees );

	/* FIXME: Need to deal with the freeing of local trees. */
}

/* Called for each type of function compile: revert and commit. */
void Compiler::compileFunction( Function *func, CodeVect &code )
{
	CodeBlock *block = func->codeBlock;

	/* Add the alloc frame opcode. We don't have the right 
	 * frame size yet. We will fill it in later. */
	code.append( IN_INIT_LOCALS );
	code.appendHalf( 0 );

	/* Compile the block. */
	block->compile( this, code );

	/* We have the frame size now. Set in the alloc frame instruction. */
	int frameSize = func->localFrame->size();
	code.setHalf( 1, frameSize );

	/* Check for a return statement. */
	if ( block->stmtList->length() == 0 ||
			block->stmtList->tail->type != LangStmt::ReturnType )
	{
		/* Push the return value. */
		code.append( IN_LOAD_NIL );
		code.append( IN_SAVE_RET );
	}

	/* Compute the jump distance for the return jumps. */
	for ( LongVect::Iter rj = returnJumps; rj.lte(); rj++ ) {
		long distance = code.length() - *rj - 3;
		code.setHalf( *rj+1, distance );
	}

	/* Reset the vector of return jumps. */
	returnJumps.empty();

	/* Return cleans up the stack (including the args) and leaves the return
	 * value on the top. */
	code.append( IN_RET );
}

void Compiler::compileFunction( Function *func )
{
	CodeBlock *block = func->codeBlock;

	/* Set up the compilation context. */
	compileContext = CompileFunction;
	curFunction = func;

	/* Assign a frame Id. */
	block->frameId = nextFrameId++;

	/* Need an object for the local frame. */
	curLocalFrame = func->codeBlock->localFrame;

	/* Compile once for revert. */
	revertOn = true;
	compileFunction( func, block->codeWV );

	/* Compile once for commit. */
	revertOn = false;
	compileFunction( func, block->codeWC );

	/* Now that compilation is done variables are referenced. Make the local
	 * trees descriptor. */
	findLocalTrees( block->trees );
}

void Compiler::makeDefaultIterators()
{
	/* Tree iterator. */
	{
		UniqueType *anyRefUT = findUniqueType( TYPE_REF, anyLangEl );
		ObjMethod *objMethod = initFunction( uniqueTypeAny, globalObjectDef, 
				"triter", IN_HALT, IN_HALT, anyRefUT, true );

		IterDef *triter = findIterDef( IterDef::Tree );
		objMethod->iterDef = triter;
	}

	/* Child iterator. */
	{
		UniqueType *anyRefUT = findUniqueType( TYPE_REF, anyLangEl );
		ObjMethod *objMethod = initFunction( uniqueTypeAny, globalObjectDef, 
				"child", IN_HALT, IN_HALT, anyRefUT, true );

		IterDef *triter = findIterDef( IterDef::Child );
		objMethod->iterDef = triter;
	}

	/* Reverse iterator. */
	{
		UniqueType *anyRefUT = findUniqueType( TYPE_REF, anyLangEl );
		ObjMethod *objMethod = initFunction( uniqueTypeAny, globalObjectDef, 
				"rev_child", IN_HALT, IN_HALT, anyRefUT, true );

		IterDef *triter = findIterDef( IterDef::RevChild );
		objMethod->iterDef = triter;
	}

	/* Repeat iterator. */
	{
		UniqueType *anyRefUT = findUniqueType( TYPE_REF, anyLangEl );
		ObjMethod *objMethod = initFunction( uniqueTypeAny, globalObjectDef, 
				"repeat", IN_HALT, IN_HALT, anyRefUT, true );

		IterDef *triter = findIterDef( IterDef::Repeat );
		objMethod->iterDef = triter;
	}

	/* Reverse repeat iterator. */
	{
		UniqueType *anyRefUT = findUniqueType( TYPE_REF, anyLangEl );
		ObjMethod *objMethod = initFunction( uniqueTypeAny, globalObjectDef, 
				"rev_repeat", IN_HALT, IN_HALT, anyRefUT, true );

		IterDef *triter = findIterDef( IterDef::RevRepeat );
		objMethod->iterDef = triter;
	}
}

void Compiler::addStdin()
{
	/* Make the type ref. */
	TypeRef *typeRef = TypeRef::cons( InputLoc(), uniqueTypeStream );

	/* Create the field and insert it into the map. */
	ObjField *el = new ObjField( InputLoc(), typeRef, "stdin" );
	el->beenReferenced = true;
	el->beenInitialized = true;
	el->isConst = true;
	el->useOffset = false;
	el->inGetR    = IN_GET_STDIN;
	globalObjectDef->insertField( el->name, el );
}

void Compiler::addStdout()
{
	/* Make the type ref. */
	TypeRef *typeRef = TypeRef::cons( InputLoc(), uniqueTypeStr );

	/* Create the field and insert it into the map. */
	ObjField *el = new ObjField( InputLoc(), typeRef, "stout" );
	el->beenReferenced = true;
	el->beenInitialized = true;
	el->isConst = true;
	el->useOffset = false;
	el->inGetR    = IN_GET_STDOUT;
	globalObjectDef->insertField( el->name, el );
}

void Compiler::addStderr()
{
	/* Make the type ref. */
	TypeRef *typeRef = TypeRef::cons( InputLoc(), uniqueTypeStr );

	/* Create the field and insert it into the map. */
	ObjField *el = new ObjField( InputLoc(), typeRef, "stderr" );
	el->beenReferenced = true;
	el->beenInitialized = true;
	el->isConst = true;
	el->useOffset = false;
	el->inGetR    = IN_GET_STDERR;
	globalObjectDef->insertField( el->name, el );
}

void Compiler::addArgv()
{
	/* Create the field and insert it into the map. */
	ObjField *el = new ObjField( InputLoc(), argvTypeRef, "argv" );
	el->isArgv = true;
	el->isConst = true;
	globalObjectDef->insertField( el->name, el );
}

int Compiler::argvOffset()
{
	for ( ObjFieldList::Iter field = *globalObjectDef->objFieldList;
			field.lte(); field++ )
	{
		if ( field->value->isArgv ) {
			globalObjectDef->referenceField( this, field->value );
			return field->value->offset;
		}
	}
	assert(false);
}

void Compiler::initGlobalFunctions()
{
	ObjMethod *method;

	method = initFunction( uniqueTypeStream, globalObjectDef, "open",
		IN_OPEN_FILE, IN_OPEN_FILE, uniqueTypeStr, uniqueTypeStr, true );
	method->useCallObj = false;

	method = initFunction( uniqueTypeStr, globalObjectDef, "tolower",
		IN_TO_LOWER, IN_TO_LOWER, uniqueTypeStr, true );
	method->useCallObj = false;

	method = initFunction( uniqueTypeStr, globalObjectDef, "toupper",
		IN_TO_UPPER, IN_TO_UPPER, uniqueTypeStr, true );
	method->useCallObj = false;

	method = initFunction( uniqueTypeInt, globalObjectDef, "exit",
		IN_EXIT, IN_EXIT, uniqueTypeInt, true );

	method = initFunction( uniqueTypeStr, globalObjectDef, "error",
		IN_ERROR, IN_ERROR, true );

	addStdin();
	addStdout();
	addStderr();
	addArgv();
}

void Compiler::removeNonUnparsableRepls()
{
	for ( ConsList::Iter repl = replList; repl.lte(); ) {
		Constructor *maybeDel = repl++;
		if ( !maybeDel->parse )
			replList.detach( maybeDel );
	}
}

void Compiler::compileByteCode()
{
//	initUniqueTypes();
	initIntObject();
	initStrObject();
	initStreamObject();
	initInputObject();
	initTokenObjects();
	makeDefaultIterators();
	initAllLanguageObjects();
	initGenericTypes();

	initGlobalFunctions();

	for ( FunctionList::Iter f = functionList; f.lte(); f++ )
		makeFuncVisible( f, f->isUserIter );

	/* This may be comment rot: The function info structure relies on functions
	 * being compiled first, then iterators. */

	/* Compile functions. */
	for ( FunctionList::Iter f = functionList; f.lte(); f++ ) {
		if ( f->inContext != 0 )
			context = f->inContext;
		if ( f->isUserIter )
			compileUserIter( f );
		else
			compileFunction( f );
		context = 0;
	}

	/* Compile the reduction code. */
	for ( DefList::Iter prod = prodList; prod.lte(); prod++ ) {
		makeProdCopies( prod );
		if ( prod->redBlock != 0 ) {
			if ( prod->redBlock->context != 0 )
				context = prod->redBlock->context;
			compileReductionCode( prod );
			context = 0;
		}
	}

	/* Compile the token translation code. */
	for ( LelList::Iter lel = langEls; lel.lte(); lel++ ) {
		if ( lel->transBlock != 0 ) {
			if ( lel->transBlock->context != 0 )
				context = lel->transBlock->context;
			compileTranslateBlock( lel );
			context = 0;
		}
	}

	/* Compile preeof blocks. */
	for ( RegionList::Iter r = regionList; r.lte(); r++ ) {
		if ( r->preEofBlock != 0 ) {
			if ( r->preEofBlock->context != 0 )
				context = r->preEofBlock->context;
			compilePreEof( r );
			context = 0;
		}
	}

	/* Compile the init code */
	compileRootBlock( );
	removeNonUnparsableRepls();
}
