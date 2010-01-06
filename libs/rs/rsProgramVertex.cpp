/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "rsContext.h"
#include "rsProgramVertex.h"

#include <GLES/gl.h>
#include <GLES/glext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

using namespace android;
using namespace android::renderscript;


ProgramVertex::ProgramVertex(Context *rsc, bool texMat) :
    Program(rsc)
{
    mAllocFile = __FILE__;
    mAllocLine = __LINE__;
    mTextureMatrixEnable = texMat;
    mLightCount = 0;
    init(rsc);
}

ProgramVertex::ProgramVertex(Context *rsc, const char * shaderText,
                             uint32_t shaderLength, const uint32_t * params,
                             uint32_t paramLength) :
    Program(rsc, shaderText, shaderLength, params, paramLength)
{
    mAllocFile = __FILE__;
    mAllocLine = __LINE__;
    mTextureMatrixEnable = false;
    mLightCount = 0;

    init(rsc);
}

ProgramVertex::~ProgramVertex()
{
}

static void logMatrix(const char *txt, const float *f)
{
    LOGV("Matrix %s, %p", txt, f);
    LOGV("%6.2f, %6.2f, %6.2f, %6.2f", f[0], f[4], f[8], f[12]);
    LOGV("%6.2f, %6.2f, %6.2f, %6.2f", f[1], f[5], f[9], f[13]);
    LOGV("%6.2f, %6.2f, %6.2f, %6.2f", f[2], f[6], f[10], f[14]);
    LOGV("%6.2f, %6.2f, %6.2f, %6.2f", f[3], f[7], f[11], f[15]);
}

void ProgramVertex::setupGL(const Context *rsc, ProgramVertexState *state)
{
    if ((state->mLast.get() == this) && !mDirty) {
        return;
    }
    state->mLast.set(this);

    const float *f = static_cast<const float *>(mConstants->getPtr());

    glMatrixMode(GL_TEXTURE);
    if (mTextureMatrixEnable) {
        glLoadMatrixf(&f[RS_PROGRAM_VERTEX_TEXTURE_OFFSET]);
    } else {
        glLoadIdentity();
    }

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    if (mLightCount) {
        int v = 0;
        glEnable(GL_LIGHTING);
        glLightModelxv(GL_LIGHT_MODEL_TWO_SIDE, &v);
        for (uint32_t ct = 0; ct < mLightCount; ct++) {
            const Light *l = mLights[ct].get();
            glEnable(GL_LIGHT0 + ct);
            l->setupGL(ct);
        }
        for (uint32_t ct = mLightCount; ct < MAX_LIGHTS; ct++) {
            glDisable(GL_LIGHT0 + ct);
        }
    } else {
        glDisable(GL_LIGHTING);
    }

    if (!f) {
        LOGE("Must bind constants to vertex program");
    }

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(&f[RS_PROGRAM_VERTEX_PROJECTION_OFFSET]);
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(&f[RS_PROGRAM_VERTEX_MODELVIEW_OFFSET]);

    mDirty = false;
}

void ProgramVertex::loadShader(Context *rsc) {
    Program::loadShader(rsc, GL_VERTEX_SHADER);
}

void ProgramVertex::createShader()
{
    mShader.setTo("");

    for (uint32_t ct=0; ct < mUniformCount; ct++) {
        mShader.append("uniform mat4 ");
        mShader.append(mUniformNames[ct]);
        mShader.append(";\n");
    }

    mShader.append("varying vec4 varColor;\n");
    mShader.append("varying vec4 varTex0;\n");

    if (mUserShader.length() > 1) {
        for (uint32_t ct=0; ct < mInputCount; ct++) {
            const Element *e = mInputElements[ct].get();
            for (uint32_t field=0; field < e->getFieldCount(); field++) {
                const Element *f = e->getField(field);

                // Cannot be complex
                rsAssert(!f->getFieldCount());
                switch(f->getComponent().getVectorSize()) {
                case 1: mShader.append("attribute float ATTRIB_"); break;
                case 2: mShader.append("attribute vec2 ATTRIB_"); break;
                case 3: mShader.append("attribute vec3 ATTRIB_"); break;
                case 4: mShader.append("attribute vec4 ATTRIB_"); break;
                default:
                    rsAssert(0);
                }

                mShader.append(e->getFieldName(field));
                mShader.append(";\n");
            }
        }
        mShader.append(mUserShader);
    } else {
        for (uint32_t ct=VertexArray::POSITION; ct < mAttribCount; ct++) {
            mShader.append("attribute vec4 ");
            mShader.append(mAttribNames[ct]);
            mShader.append(";\n");
        }

        mShader.append("void main() {\n");
        mShader.append("  gl_Position = uni_MVP * ATTRIB_Position;\n");
        mShader.append("  gl_PointSize = ATTRIB_PointSize.x;\n");

        mShader.append("  varColor = ATTRIB_Color;\n");
        if (mTextureMatrixEnable) {
            mShader.append("  varTex0 = uni_TexMatrix * ATTRIB_Texture;\n");
        } else {
            mShader.append("  varTex0 = ATTRIB_Texture;\n");
        }
        //mShader.append("  pos.x = pos.x / 480.0;\n");
        //mShader.append("  pos.y = pos.y / 800.0;\n");
        //mShader.append("  gl_Position = pos;\n");
        mShader.append("}\n");
    }
}

void ProgramVertex::setupGL2(const Context *rsc, ProgramVertexState *state, ShaderCache *sc)
{
    //LOGE("sgl2 vtx1 %x", glGetError());
    if ((state->mLast.get() == this) && !mDirty) {
        //return;
    }

    glVertexAttrib4f(1, state->color[0], state->color[1], state->color[2], state->color[3]);

    const float *f = static_cast<const float *>(mConstants->getPtr());

    Matrix mvp;
    mvp.load(&f[RS_PROGRAM_VERTEX_PROJECTION_OFFSET]);
    Matrix t;
    t.load(&f[RS_PROGRAM_VERTEX_MODELVIEW_OFFSET]);
    mvp.multiply(&t);

    glUniformMatrix4fv(sc->vtxUniformSlot(0), 1, GL_FALSE, mvp.m);
    if (mTextureMatrixEnable) {
        glUniformMatrix4fv(sc->vtxUniformSlot(1), 1, GL_FALSE,
                           &f[RS_PROGRAM_VERTEX_TEXTURE_OFFSET]);
    }

    state->mLast.set(this);
    rsc->checkError("ProgramVertex::setupGL2");
}

void ProgramVertex::addLight(const Light *l)
{
    if (mLightCount < MAX_LIGHTS) {
        mLights[mLightCount].set(l);
        mLightCount++;
    }
}

void ProgramVertex::setProjectionMatrix(const rsc_Matrix *m) const
{
    float *f = static_cast<float *>(mConstants->getPtr());
    memcpy(&f[RS_PROGRAM_VERTEX_PROJECTION_OFFSET], m, sizeof(rsc_Matrix));
    mDirty = true;
}

void ProgramVertex::setModelviewMatrix(const rsc_Matrix *m) const
{
    float *f = static_cast<float *>(mConstants->getPtr());
    memcpy(&f[RS_PROGRAM_VERTEX_MODELVIEW_OFFSET], m, sizeof(rsc_Matrix));
    mDirty = true;
}

void ProgramVertex::setTextureMatrix(const rsc_Matrix *m) const
{
    float *f = static_cast<float *>(mConstants->getPtr());
    memcpy(&f[RS_PROGRAM_VERTEX_TEXTURE_OFFSET], m, sizeof(rsc_Matrix));
    mDirty = true;
}

void ProgramVertex::transformToScreen(const Context *rsc, float *v4out, const float *v3in) const
{
    float *f = static_cast<float *>(mConstants->getPtr());
    Matrix mvp;
    mvp.loadMultiply((Matrix *)&f[RS_PROGRAM_VERTEX_MODELVIEW_OFFSET],
                     (Matrix *)&f[RS_PROGRAM_VERTEX_PROJECTION_OFFSET]);
    mvp.vectorMultiply(v4out, v3in);
}

void ProgramVertex::initAddUserAttrib(const Element *e)
{
    rsAssert(e->getFieldCount());
    for (uint32_t ct=0; ct < e->getFieldCount(); ct++) {
        const Element *ce = e->getField(ct);
        if (ce->getFieldCount()) {
            initAddUserAttrib(ce);
        } else {
            String8 tmp("ATTRIB_");
            tmp.append(e->getFieldName(ct));
            mAttribNames[mAttribCount].setTo(tmp.string());
            mAttribCount++;
        }
    }
}

void ProgramVertex::init(Context *rsc)
{
    if (mUserShader.size() > 0) {
        mAttribCount = 0;
        for (uint32_t ct=0; ct < mInputCount; ct++) {
            initAddUserAttrib(mInputElements[ct].get());
        }
    } else {
        mAttribCount = 5;
        mAttribNames[0].setTo("ATTRIB_Position");
        mAttribNames[1].setTo("ATTRIB_Color");
        mAttribNames[2].setTo("ATTRIB_Normal");
        mAttribNames[3].setTo("ATTRIB_PointSize");
        mAttribNames[4].setTo("ATTRIB_Texture");
    }

    mUniformCount = 2;
    mUniformNames[0].setTo("uni_MVP");
    mUniformNames[1].setTo("uni_TexMatrix");

    createShader();
}


///////////////////////////////////////////////////////////////////////

ProgramVertexState::ProgramVertexState()
{
}

ProgramVertexState::~ProgramVertexState()
{
}

void ProgramVertexState::init(Context *rsc, int32_t w, int32_t h)
{
    RsElement e = Element::create(rsc, RS_TYPE_FLOAT_32, RS_KIND_USER, false, 1);

    rsi_TypeBegin(rsc, e);
    rsi_TypeAdd(rsc, RS_DIMENSION_X, 48);
    mAllocType.set((Type *)rsi_TypeCreate(rsc));

    ProgramVertex *pv = new ProgramVertex(rsc, false);
    Allocation *alloc = (Allocation *)rsi_AllocationCreateTyped(rsc, mAllocType.get());
    mDefaultAlloc.set(alloc);
    mDefault.set(pv);
    pv->init(rsc);
    pv->bindAllocation(alloc);

    color[0] = 1.f;
    color[1] = 1.f;
    color[2] = 1.f;
    color[3] = 1.f;

    updateSize(rsc, w, h);
}

void ProgramVertexState::updateSize(Context *rsc, int32_t w, int32_t h)
{
    Matrix m;
    m.loadOrtho(0,w, h,0, -1,1);
    mDefaultAlloc->subData(RS_PROGRAM_VERTEX_PROJECTION_OFFSET, 16, &m.m[0], 16*4);

    m.loadIdentity();
    mDefaultAlloc->subData(RS_PROGRAM_VERTEX_MODELVIEW_OFFSET, 16, &m.m[0], 16*4);
}

void ProgramVertexState::deinit(Context *rsc)
{
    mDefaultAlloc.clear();
    mDefault.clear();
    mAllocType.clear();
    mLast.clear();
}


namespace android {
namespace renderscript {


RsProgramVertex rsi_ProgramVertexCreate(Context *rsc, bool texMat)
{
    ProgramVertex *pv = new ProgramVertex(rsc, texMat);
    pv->incUserRef();
    return pv;
}

RsProgramVertex rsi_ProgramVertexCreate2(Context *rsc, const char * shaderText,
                             uint32_t shaderLength, const uint32_t * params,
                             uint32_t paramLength)
{
    ProgramVertex *pv = new ProgramVertex(rsc, shaderText, shaderLength, params, paramLength);
    pv->incUserRef();
    return pv;
}


}
}