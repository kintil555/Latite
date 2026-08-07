#include "pch.h"
#include "DrawUtil3D.h"
#include "mc/common/client/renderer/MeshUtils.h"
#include <mc/common/client/renderer/Tessellator.h>

MCDrawUtil3D::MCDrawUtil3D(SDK::LevelRenderer* renderer, SDK::ScreenContext* ctx, SDK::MaterialPtr* material)
    : levelRenderer(renderer)
    , screenContext(ctx)
    , material(material) {
    if (!material) {
        this->material = SDK::MaterialPtr::getSelectionBoxMaterial();
    }
}

void MCDrawUtil3D::drawLine(Vec3 const& p1, Vec3 const& p2, d2d::Color const& color, bool immediate) {
    auto scn = screenContext;
    auto tess = scn->tess;
    *scn->shaderColor = { 1.f, 1.f, 1.f, 1.f };
    tess->begin(SDK::Primitive::LineList, 1);
    tess->color(color);
    auto& origin = levelRenderer->getLevelRendererPlayer()->getOrigin();
    Vec3 a = p1;
    Vec3 b = p2;

    a.x -= origin.x;
    a.y -= origin.y;
    a.z -= origin.z;

    b.x -= origin.x;
    b.y -= origin.y;
    b.z -= origin.z;
    tess->vertex(a.x, a.y, a.z);
    tess->vertex(b.x, b.y, b.z);
    if (immediate) flush();
}

void MCDrawUtil3D::drawQuad(Vec3 a, Vec3 b, Vec3 c, Vec3 d, d2d::Color const& col) {
    auto scn = screenContext;
    auto tess = scn->tess;
    *scn->shaderColor = { 1.f, 1.f, 1.f, 1.f };
    tess->begin(SDK::Primitive::LineList, 1);
    tess->color(col);
    auto origin = levelRenderer->getLevelRendererPlayer()->getOrigin();
    a = a - (origin);
    b = b - (origin);
    c = c - (origin);
    d = d - (origin);

    tess->vertex(a.x, a.y, a.z);
    tess->vertex(b.x, b.y, b.z);

    tess->begin(SDK::Primitive::LineList, 1);
    tess->color(col);
    tess->vertex(b.x, b.y, b.z);
    tess->vertex(c.x, c.y, c.z);

    tess->begin(SDK::Primitive::LineList, 1);
    tess->color(col);
    tess->vertex(c.x, c.y, c.z);
    tess->vertex(d.x, d.y, d.z);

    tess->begin(SDK::Primitive::LineList, 1);
    tess->color(col);
    tess->vertex(d.x, d.y, d.z);
    tess->vertex(a.x, a.y, a.z);
}

void MCDrawUtil3D::fillQuad(Vec3 p1, Vec3 p2, Vec3 p3, Vec3 p4, d2d::Color const& color) {
    auto scn = screenContext;

    auto tess = scn->tess;
    *scn->shaderColor = Color(1.f, 1.f, 1.f, 1.f);
    tess->begin(SDK::Primitive::Quad, 4);
    tess->color(color);
    auto& origin = levelRenderer->getLevelRendererPlayer()->getOrigin();
    Vec3 a = p1;
    Vec3 b = p2;
    Vec3 c = p3;
    Vec3 d = p4;

    a.x -= origin.x;
    a.y -= origin.y;
    a.z -= origin.z;

    b.x -= origin.x;
    b.y -= origin.y;
    b.z -= origin.z;

    c.x -= origin.x;
    c.y -= origin.y;
    c.z -= origin.z;

    d.x -= origin.x;
    d.y -= origin.y;
    d.z -= origin.z;

    tess->vertex(a.x, a.y, a.z);
    tess->vertex(b.x, b.y, b.z);
    tess->vertex(c.x, c.y, c.z);
    tess->vertex(d.x, d.y, d.z);

    // render other side
    tess->vertex(d.x, d.y, d.z);
    tess->vertex(c.x, c.y, c.z);
    tess->vertex(b.x, b.y, b.z);
    tess->vertex(a.x, a.y, a.z);
}

void MCDrawUtil3D::drawBox(AABB const& bb, d2d::Color const& color) {
    auto scn = screenContext;
    auto tess = scn->tess;
    std::vector<std::pair<Vec3, Vec3>> verticies = {
        { bb.lower, Vec3(bb.lower.x, bb.higher.y, bb.lower.z) },
        { Vec3(bb.higher.x, bb.lower.y, bb.lower.z), Vec3(bb.higher.x, bb.higher.y, bb.lower.z) },
        { Vec3(bb.lower.x, bb.lower.y, bb.higher.z), Vec3(bb.lower.x, bb.higher.y, bb.higher.z) },
        { Vec3(bb.higher.x, bb.lower.y, bb.higher.z), Vec3(bb.higher.x, bb.higher.y, bb.higher.z) },
    };
    for (auto& vert : verticies) {
        drawLine(vert.first, vert.second, color);
    }

    drawQuad(Vec3(bb.lower.x, bb.lower.y, bb.lower.z), Vec3(bb.higher.x, bb.lower.y, bb.lower.z),
             Vec3(bb.higher.x, bb.lower.y, bb.higher.z), Vec3(bb.lower.x, bb.lower.y, bb.higher.z), color);

    drawQuad(Vec3(bb.lower.x, bb.higher.y, bb.lower.z), Vec3(bb.higher.x, bb.higher.y, bb.lower.z),
             Vec3(bb.higher.x, bb.higher.y, bb.higher.z), Vec3(bb.lower.x, bb.higher.y, bb.higher.z), color);

    drawQuad(Vec3(bb.lower.x, bb.higher.y, bb.lower.z), Vec3(bb.higher.x, bb.higher.y, bb.lower.z),
             Vec3(bb.higher.x, bb.higher.y, bb.higher.z), Vec3(bb.lower.x, bb.higher.y, bb.higher.z), color);
}

void MCDrawUtil3D::drawFullBox(AABB const& bb, d2d::Color const& color) {
    Vec3 const& lo = bb.lower;
    Vec3 const& hi = bb.higher;

    Vec3 corners[8] = {
        { lo.x, lo.y, lo.z }, { hi.x, lo.y, lo.z }, { hi.x, lo.y, hi.z }, { lo.x, lo.y, hi.z },
        { lo.x, hi.y, lo.z }, { hi.x, hi.y, lo.z }, { hi.x, hi.y, hi.z }, { lo.x, hi.y, hi.z },
    };

    fillQuad(corners[0], corners[1], corners[2], corners[3], color); // bottom
    fillQuad(corners[4], corners[5], corners[6], corners[7], color); // top
    fillQuad(corners[0], corners[1], corners[5], corners[4], color); // -Z side
    fillQuad(corners[1], corners[2], corners[6], corners[5], color); // +X side
    fillQuad(corners[2], corners[3], corners[7], corners[6], color); // +Z side
    fillQuad(corners[3], corners[0], corners[4], corners[7], color); // -X side
}

void MCDrawUtil3D::flush() {
    SDK::MeshHelpers::renderMeshImmediately(screenContext, screenContext->tess, material);
}
