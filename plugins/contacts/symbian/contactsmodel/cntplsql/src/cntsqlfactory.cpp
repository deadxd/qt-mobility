/*
* Copyright (c) 2007-2009 Nokia Corporation and/or its subsidiary(-ies).
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of "Eclipse Public License v1.0"
* which accompanies this distribution, and is available
* at the URL "http://www.eclipse.org/legal/epl-v10.html".
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors:
*
* Description: 
*
*/


/**
 @file
 @internalComponent
 @released
*/
#include "cntsqlfactory.h"
#include "cntsqlsearchinterface.h"
#include "cntsqlkoreanitut.h"

#include <QString>
#include <QLocale>
/**
CCntPplViewSession first phase constructor.
*/
CntSqlFactory::CntSqlFactory()
    {
    
    }

    
/**
CCntPplViewSession destructor.
*/
CntSqlFactory::~CntSqlFactory()
    {
    
    }

CntSqlSearchInterface* CntSqlFactory::getSqlIinstance(const QString &pattern, QLocale& language)
    {
    
    int sysLang = language.language();

        switch (sysLang) {
        case 66:
        return new CntSqlKoreanItuT();
        break;
        
        default:
        return NULL;
        }
    }
   
