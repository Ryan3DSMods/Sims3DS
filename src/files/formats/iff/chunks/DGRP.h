/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/DGRP.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"
#include "files/utils/ITextureProvider.h"
#include <memory>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

class DGRP;

enum class DGRPSpriteFlags : uint32_t
{
    Flip = 0x1,
    AllowCache = 0x4
};
FSO_FLAGS_ENUM(DGRPSpriteFlags)

/// <summary>
/// Makes up a DGRPImage.
/// </summary>
class DGRPSprite : public Utils::ITextureProvider, public Utils::IWorldTextureProvider
{
private:
    DGRP* Parent;
public:
    uint32_t SpriteID = 0;
    uint32_t SpriteFrameIndex = 0;
    DGRPSpriteFlags Flags = (DGRPSpriteFlags)0;

    Vector2 SpriteOffset;
    Vector3 ObjectOffset;

    bool Flip() const { return (Flags & DGRPSpriteFlags::Flip) > 0; }
    void Flip(bool value) {
        Flags = (DGRPSpriteFlags)(Flags & (~DGRPSpriteFlags::Flip));
        if (value) Flags |= DGRPSpriteFlags::Flip;
    }

    explicit DGRPSprite(DGRP* parent)
    {
        this->Parent = parent;
    }

    /// <summary>
    /// Reads a DGRPSprite from a stream.
    /// </summary>
    void Read(uint32_t version, st::Reader& io);

    void Write(st::Writer& io);

    /// <summary>
    /// Gets position of this sprite.
    /// </summary>
    /// <returns>A Vector2 instance holding position of this sprite.</returns>
    Vector2 GetPosition();

    Utils::Texture2D* GetTexture(Utils::GraphicsDevice* device) override;

    Utils::WorldTexture* GetWorldTexture(Utils::GraphicsDevice* device) override;
    Utils::WorldTexture WorldTextureCache; //the object returned by GetWorldTexture (SPR# path)
};

/// <summary>
/// A DGRP is made up of multiple DGRPImages,
/// which are made up of multiple DGRPSprites.
/// </summary>
class DGRPImage
{
private:
    DGRP* Parent;
public:
    uint32_t Direction = 0;
    uint32_t Zoom = 0;
    std::vector<DGRPSprite> Sprites;

    explicit DGRPImage(DGRP* parent)
    {
        this->Parent = parent;
    }

    /// <summary>
    /// Reads a DGRPImage from a stream.
    /// </summary>
    void Read(uint32_t version, st::Reader& io);

    void Write(st::Writer& io);
};

/// <summary>
/// This chunk type collects SPR# and SPR2 resources into a "drawing group" which
/// can be used to display one tile of an object from all directions and zoom levels.
/// Objects which span across multiple tiles have a separate DGRP chunk for each tile.
/// A DGRP chunk always consists of 12 images (one for every direction/zoom level combination),
/// which in turn contain info about one or more sprites.
/// </summary>
class DGRP : public IffChunk
{
public:
    IFF_CHUNK_TYPE("DGRP")

    std::vector<DGRPImage> Images;

    /// <summary>
    /// Gets a DGRPImage instance from this DGRP instance.
    /// </summary>
    /// <param name="direction">The direction the DGRP is facing.</param>
    /// <param name="zoom">Zoom level DGRP is drawn at.</param>
    /// <param name="worldRotation">Current rotation of world.</param>
    /// <returns>A DGRPImage instance.</returns>
    DGRPImage* GetImage(uint32_t direction, uint32_t zoom, uint32_t worldRotation);

    /// <summary>
    /// Reads a DGRP from a stream instance.
    /// </summary>
    /// <param name="iff">An Iff instance.</param>
    /// <param name="stream">A Stream instance holding a DGRP chunk.</param>
    void Read(IffFile* iff, st::Reader& stream) override;

    bool Write(IffFile* iff, st::Writer& stream) override;
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
