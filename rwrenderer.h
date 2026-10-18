#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "renderer.h"

struct Renderer;
struct CTextureDictionary;
struct RwGeometry;
struct CKAnyGeometry;

struct ProTexDict {
	Renderer *_gfx;
	ProTexDict *_next = nullptr;
	std::unordered_map<std::string, texture_t> dict;
	static uint32_t globalVersion;

	ProTexDict(Renderer *gfx) : _gfx(gfx) {}
	ProTexDict(Renderer *gfx, CTextureDictionary *ctd);
	ProTexDict(ProTexDict &&ptd) = default;
	~ProTexDict();

	std::pair<bool, texture_t> find(const std::string &name);
	void reset(CTextureDictionary *ctd);
};

struct ProGeometry {
	Renderer *_gfx;
	ProTexDict *_proTexDict;
	RwGeometry *geo = nullptr;
	std::unique_ptr<RVertexBuffer> vbuf;
	std::unique_ptr<RIndexBuffer> ibuf;
	size_t numTris;
	std::string textureName;
	// The material texture this instance was looked up with, the cache key beside geo. It can differ
	// from textureName, which is the texture actually used for drawing.
	std::string overrideKey;

	texture_t ptdTexId = nullptr;
	uint32_t ptdVersion = 0;

	//ProGeometry(Renderer *gfx, RVertexBuffer *vbuf, RIndexBuffer *ibuf, size_t numTris, const std::string &textureName, ProTexDict *proTexDict) :
	//	gfx(gfx), vbuf(vbuf), ibuf(ibuf), numTris(numTris), textureName(textureName), proTexDict(proTexDict);

	ProGeometry(Renderer *gfx, RwGeometry *geo, ProTexDict *proTexDict, const std::string &textureOverride);

	void draw(bool showTextures = true);
};

struct ProGeoCache {
	Renderer *_gfx;
	std::unordered_map<RwGeometry*, std::unique_ptr<ProGeometry>> dict;
	// Geometries drawn with a material texture of their own geometry object, by geometry. One geometry
	// can be drawn with more than one material texture, so it keeps the variants it was drawn with.
	std::unordered_map<RwGeometry*, std::vector<std::unique_ptr<ProGeometry>>> namedDict;

	ProGeoCache(Renderer *gfx);

	// textureOverride is the diffuse texture of the geometry object's material, used when the
	// geometry itself carries no texture (Spyro DotD keeps the mesh texture in its material).
	ProGeometry* getPro(RwGeometry* geo, ProTexDict* proTexDict, const std::string& textureOverride = std::string());
	ProGeometry* getPro(const std::shared_ptr<RwGeometry>& geo, ProTexDict* proTexDict, const std::string& textureOverride = std::string()) { return getPro(geo.get(), proTexDict, textureOverride); }
	void clear() { dict.clear(); namedDict.clear(); }
};