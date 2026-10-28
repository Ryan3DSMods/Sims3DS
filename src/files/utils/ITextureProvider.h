/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/utils/ITextureProvider.cs, IWorldTextureProvider.cs, WorldTexture.cs
#pragma once

namespace FSO { namespace Files { namespace Utils {

// STUB(3ds): Microsoft.Xna.Framework.Graphics.Texture2D / GraphicsDevice. The 3DS port
// has no XNA; these are opaque placeholder types so the ITextureProvider interfaces keep
// their shape. Frame classes keep their decoded pixel arrays (PixelData/ZBufferData)
// instead of uploading to a GPU texture.
struct Texture2D { };
struct GraphicsDevice { };

class ITextureProvider
{
public:
    virtual ~ITextureProvider() {}
    virtual Texture2D* GetTexture(GraphicsDevice* device) = 0;
};

/// A texture used in the game world.
class WorldTexture
{
public:
    Texture2D* Pixel = nullptr;
    Texture2D* ZBuffer = nullptr;
};

class IWorldTextureProvider
{
public:
    virtual ~IWorldTextureProvider() {}
    virtual WorldTexture* GetWorldTexture(GraphicsDevice* device) = 0;
};

}}} // namespace FSO::Files::Utils
