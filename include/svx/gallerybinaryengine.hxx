/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#pragma once

#include <svx/galtheme.hxx>
#include <svx/galmisc.hxx>
#include <svx/svxdllapi.h>
#include <svx/fmmodel.hxx>
#include <svx/gallerystoragelocations.hxx>
#include <tools/urlobj.hxx>
#include <sot/storage.hxx>

#include <memory>

class GalleryStorageLocations;
class GalleryObjectCollection;
class SgaObjectSvDraw;
class SgaObjectBmp;
class SgaObject;
class SotStorage;
struct GalleryObject;
class FmFormModel;
class GalleryTheme;

class SVXCORE_DLLPUBLIC GalleryBinaryEngine
{
private:
    tools::SvRef<SotStorage> m_aSvDrawStorageRef;
    const GalleryStorageLocations& maGalleryStorageLocations;
    GalleryObjectCollection& mrGalleryObjectCollection;
    bool mbReadOnly;

public:
    GalleryBinaryEngine(const GalleryStorageLocations& rGalleryStorageLocations,
                        GalleryObjectCollection& rGalleryObjectCollection, bool bReadOnly);
    SAL_DLLPRIVATE ~GalleryBinaryEngine();

    void clearSotStorage();

    SAL_DLLPRIVATE void ImplCreateSvDrawStorage();
    SAL_DLLPRIVATE const tools::SvRef<SotStorage>& GetSvDrawStorage() const;

    const INetURLObject& GetThmURL() const { return maGalleryStorageLocations.GetThmURL(); }
    const INetURLObject& GetSdgURL() const { return maGalleryStorageLocations.GetSdgURL(); }
    const INetURLObject& GetSdvURL() const { return maGalleryStorageLocations.GetSdvURL(); }
    const INetURLObject& GetStrURL() const { return maGalleryStorageLocations.GetStrURL(); }

    SAL_DLLPRIVATE bool implWrite(const GalleryTheme& rTheme);

    void insertObject(const SgaObject& rObj, GalleryObject* pFoundEntry, OUString& rDestDir,
                      sal_uInt32& rInsertPos);

    std::unique_ptr<SgaObject> implReadSgaObject(GalleryObject const* pEntry);
    bool implWriteSgaObject(const SgaObject& rObj, sal_uInt32 nPos, GalleryObject* pExistentEntry,
                            OUString& aDestDir);

    bool readModel(const GalleryObject* pObject, SdrModel& rModel);
    SgaObjectSvDraw insertModel(const FmFormModel& rModel, const INetURLObject& rUserURL);

    bool readModelStream(const GalleryObject* pObject,
                         tools::SvRef<SotStorageStream> const& rxModelStream);
    SgaObjectSvDraw insertModelStream(const tools::SvRef<SotStorageStream>& rxModelStream,
                                      const INetURLObject& rUserURL);

    INetURLObject implCreateUniqueURL(SgaObjKind eObjKind, const INetURLObject& rUserURL,
                                      ConvertDataFormat nFormat = ConvertDataFormat::Unknown);

    SgaObjectBmp insertGraphic(const Graphic& rGraphic, const GfxLink& aGfxLink,
                               ConvertDataFormat& nExportFormat, const INetURLObject& rUserURL);

    SgaObjectSvDraw updateSvDrawObject(GalleryObject* pEntry);

    void updateTheme();
    static void insertFileOrDirURL(const INetURLObject& rFileOrDirURL,
                                   std::vector<INetURLObject>& rURLVector);
};

SvStream& WriteGalleryTheme(SvStream& rOut, const GalleryTheme& rTheme);

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
