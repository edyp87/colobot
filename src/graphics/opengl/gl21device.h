/*
 * This file is part of the Colobot: Gold Edition source code
 * Copyright (C) 2001-2015, Daniel Roux, EPSITEC SA & TerranovaTeam
 * http://epsitec.ch; http://colobot.info; http://github.com/colobot
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see http://gnu.org/licenses
 */

/**
 * \file graphics/opengl/gldevice.h
 * \brief OpenGL implementation - CGL21Device class
 */

#pragma once

#include "graphics/core/device.h"

#include "graphics/opengl/glutil.h"

#include <string>
#include <vector>
#include <set>
#include <map>


// Graphics module namespace
namespace Gfx
{

struct GLDevicePrivate;

/**
  \class CGL21Device
  \brief Implementation of CDevice interface in OpenGL

  Provides the concrete implementation of 3D device in OpenGL.

  This class should be initialized (by calling Initialize() ) only after
  setting the video mode by CApplication, once the OpenGL context is defined.
  Because of that, CGL21DeviceConfig is outside the CDevice class and must be set
  in CApplication.
*/
class CGL21Device : public CDevice
{
public:
    CGL21Device(const DeviceConfig &config);
    virtual ~CGL21Device();

    void DebugHook() override;
    void DebugLights() override;

    bool Create() override;
    void Destroy() override;

    void ConfigChanged(const DeviceConfig &newConfig) override;

    void BeginScene() override;
    void EndScene() override;

    void Clear() override;

    void SetTransform(TransformType type, const Math::Matrix &matrix) override;

    void SetMaterial(const Material &material) override;

    int GetMaxLightCount() override;
    void SetLight(int index, const Light &light) override;
    void SetLightEnabled(int index, bool enabled) override;

    Texture CreateTexture(CImage *image, const TextureCreateParams &params) override;
    Texture CreateTexture(ImageData *data, const TextureCreateParams &params) override;
    Texture CreateDepthTexture(int width, int height, int depth) override;
    void DestroyTexture(const Texture &texture) override;
    void DestroyAllTextures() override;

    int GetMaxTextureStageCount() override;
    void SetTexture(int index, const Texture &texture) override;
    void SetTexture(int index, unsigned int textureId) override;
    void SetTextureEnabled(int index, bool enabled) override;

    void SetTextureStageParams(int index, const TextureStageParams &params) override;

    void SetTextureStageWrap(int index, Gfx::TexWrapMode wrapS, Gfx::TexWrapMode wrapT) override;
    void SetTextureCoordGeneration(int index, TextureGenerationParams &params) override;

    virtual void DrawPrimitive(PrimitiveType type, const Vertex *vertices    , int vertexCount,
                               Color color = Color(1.0f, 1.0f, 1.0f, 1.0f)) override;
    virtual void DrawPrimitive(PrimitiveType type, const VertexTex2 *vertices, int vertexCount,
                               Color color = Color(1.0f, 1.0f, 1.0f, 1.0f)) override;
    void DrawPrimitive(PrimitiveType type, const VertexCol *vertices , int vertexCount) override;

    unsigned int CreateStaticBuffer(PrimitiveType primitiveType, const Vertex* vertices, int vertexCount) override;
    unsigned int CreateStaticBuffer(PrimitiveType primitiveType, const VertexTex2* vertices, int vertexCount) override;
    unsigned int CreateStaticBuffer(PrimitiveType primitiveType, const VertexCol* vertices, int vertexCount) override;
    void UpdateStaticBuffer(unsigned int bufferId, PrimitiveType primitiveType, const Vertex* vertices, int vertexCount) override;
    void UpdateStaticBuffer(unsigned int bufferId, PrimitiveType primitiveType, const VertexTex2* vertices, int vertexCount) override;
    void UpdateStaticBuffer(unsigned int bufferId, PrimitiveType primitiveType, const VertexCol* vertices, int vertexCount) override;
    void DrawStaticBuffer(unsigned int bufferId) override;
    void DestroyStaticBuffer(unsigned int bufferId) override;

    int ComputeSphereVisibility(const Math::Vector &center, float radius) override;

    void SetViewport(int x, int y, int width, int height) override;

    void SetRenderState(RenderState state, bool enabled) override;

    void SetColorMask(bool red, bool green, bool blue, bool alpha) override;

    void SetDepthTestFunc(CompFunc func) override;

    void SetDepthBias(float factor, float units) override;

    void SetAlphaTestFunc(CompFunc func, float refValue) override;

    void SetBlendFunc(BlendFunc srcBlend, BlendFunc dstBlend) override;

    void SetClearColor(const Color &color) override;

    void SetGlobalAmbient(const Color &color) override;

    void SetFogParams(FogMode mode, const Color &color, float start, float end, float density) override;

    void SetCullMode(CullMode mode) override;

    void SetShadeModel(ShadeModel model) override;

    void SetShadowColor(float value) override;

    void SetFillMode(FillMode mode) override;

    void CopyFramebufferToTexture(Texture& texture, int xOffset, int yOffset, int x, int y, int width, int height) override;

    std::unique_ptr<CFrameBufferPixels> GetFrameBufferPixels() const override;

    CFramebuffer* GetFramebuffer(std::string name) override;

    CFramebuffer* CreateFramebuffer(std::string name, const FramebufferParams& params) override;

    void DeleteFramebuffer(std::string name) override;

    bool IsAnisotropySupported() override;
    int GetMaxAnisotropyLevel() override;

    int GetMaxSamples() override;

    bool IsShadowMappingSupported() override;

    int GetMaxTextureSize() override;

    bool IsFramebufferSupported() override;

private:
    //! Updates position for given light based on transformation matrices
    void UpdateLightPosition(int index);
    //! Updates the texture params for given texture stage
    void UpdateTextureParams(int index);
    //! Updates texture status
    void UpdateTextureStatus();
    //! Binds VBO
    inline void BindVBO(GLuint vbo);

private:
    //! Current config
    DeviceConfig m_config;

    //! Current world matrix
    Math::Matrix m_worldMat;
    //! Current view matrix
    Math::Matrix m_viewMat;
    //! OpenGL modelview matrix = world matrix * view matrix
    Math::Matrix m_modelviewMat;
    //! Current projection matrix
    Math::Matrix m_projectionMat;

    //! The current material
    Material m_material;

    //! Whether lighting is enabled
    bool m_lighting = false;
    //! Current lights
    std::vector<Light> m_lights;
    //! Current lights enable status
    std::vector<bool> m_lightsEnabled;

    //! Current textures; \c nullptr value means unassigned
    std::vector<Texture> m_currentTextures;
    //! Current texture stages enable status
    std::vector<bool> m_texturesEnabled;
    //! Current texture params
    std::vector<TextureStageParams> m_textureStageParams;

    //! Set of all created textures
    std::set<Texture> m_allTextures;

    //! Map of framebuffers
    std::map<std::string, std::unique_ptr<CFramebuffer>> m_framebuffers;

    //! Type of vertex structure
    enum VertexType
    {
        VERTEX_TYPE_NORMAL,
        VERTEX_TYPE_TEX2,
        VERTEX_TYPE_COL,
    };

    //! Info about static VBO buffers
    struct VboObjectInfo
    {
        PrimitiveType primitiveType = {};
        unsigned int bufferId = 0;
        VertexType vertexType = {};
        int vertexCount = 0;
        int size = 0;
    };

    //! Detected capabilities
    //! OpenGL version
    int m_glMajor = 1, m_glMinor = 1;
    //! Whether anisotropic filtering is available
    bool m_anisotropyAvailable = false;
    //! Maximum anisotropy level
    int m_maxAnisotropy = 1;
    //! Maximum samples
    int m_maxSamples = 1;
    //! Framebuffer support
    FramebufferSupport m_framebufferSupport = FBS_NONE;
    //! Map of saved VBO objects
    std::map<unsigned int, VboObjectInfo> m_vboObjects;
    //! Last ID of VBO object
    unsigned int m_lastVboId = 0;
    //! Currently bound VBO
    GLuint m_currentVBO = 0;

    //! true enables per-pixel lighting
    bool m_perPixelLighting = false;

    //! Shader program
    GLuint m_program = 0;

    // Uniforms
    //! Projection matrix
    GLint uni_ProjectionMatrix = 0;
    //! View matrix
    GLint uni_ViewMatrix = 0;
    //! Model matrix
    GLint uni_ModelMatrix = 0;
    //! Shadow matrix
    GLint uni_ShadowMatrix = 0;
    //! Normal matrix
    GLint uni_NormalMatrix = 0;

    //! Primary texture sampler
    GLint uni_PrimaryTexture = 0;
    //! Secondary texture sampler
    GLint uni_SecondaryTexture = 0;
    //! Shadow texture sampler
    GLint uni_ShadowTexture = 0;

    //! true enables texture
    GLint uni_TextureEnabled[3] = {};

    // Alpha test parameters
    //! true enables alpha test
    GLint uni_AlphaTestEnabled = 0;
    //! Alpha test reference value
    GLint uni_AlphaReference = 0;

    //! true enables fog
    GLint uni_FogEnabled = 0;
    //! Fog range
    GLint uni_FogRange = 0;
    //! Fog color
    GLint uni_FogColor = 0;

    //! Shadow color
    GLint uni_ShadowColor = 0;

    //! true enables lighting
    GLint uni_LightingEnabled = 0;
    //! true enables light source
    GLint uni_LightEnabled[8] = {};
};


} // namespace Gfx
