#include "svg_path.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace figo {

namespace {

constexpr float kPi = 3.14159265358979323846f;

struct Cursor {
    const char* p;

    void skipSep() {
        while (*p && (std::isspace(static_cast<unsigned char>(*p)) || *p == ',')) ++p;
    }

    bool number(float& out) {
        skipSep();
        char* end = nullptr;
        float v = std::strtof(p, &end);
        if (end == p) return false;
        p = end;
        out = v;
        return true;
    }

    // SVG arc flags may be written without separators ("11" = two flags).
    bool flag(bool& out) {
        skipSep();
        if (*p == '0') { out = false; ++p; return true; }
        if (*p == '1') { out = true; ++p; return true; }
        return false;
    }
};

// Endpoint-parameterized elliptical arc → cubic Béziers.
// See SVG 1.1 spec, appendix B.2.4 (conversion to center parameterization).
void arcTo(tvg::Shape& shape, float x1, float y1, float rx, float ry, float rotDeg,
           bool largeArc, bool sweep, float x2, float y2) {
    if (x1 == x2 && y1 == y2) return;
    rx = std::fabs(rx);
    ry = std::fabs(ry);
    if (rx < 1e-6f || ry < 1e-6f) {
        shape.lineTo(x2, y2);
        return;
    }

    const float phi = rotDeg * kPi / 180.0f;
    const float cosPhi = std::cos(phi), sinPhi = std::sin(phi);

    // (x1', y1'): midpoint vector in the rotated frame
    const float dx = (x1 - x2) * 0.5f, dy = (y1 - y2) * 0.5f;
    const float x1p = cosPhi * dx + sinPhi * dy;
    const float y1p = -sinPhi * dx + cosPhi * dy;

    // Scale radii up if they cannot span the endpoints.
    const float lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
    if (lambda > 1.0f) {
        const float s = std::sqrt(lambda);
        rx *= s;
        ry *= s;
    }

    // Center in the rotated frame.
    float num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
    if (num < 0) num = 0;
    const float den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
    float coef = den > 0 ? std::sqrt(num / den) : 0;
    if (largeArc == sweep) coef = -coef;
    const float cxp = coef * (rx * y1p / ry);
    const float cyp = coef * (-ry * x1p / rx);

    const float cx = cosPhi * cxp - sinPhi * cyp + (x1 + x2) * 0.5f;
    const float cy = sinPhi * cxp + cosPhi * cyp + (y1 + y2) * 0.5f;

    auto angle = [](float ux, float uy, float vx, float vy) {
        const float dot = ux * vx + uy * vy;
        const float len = std::sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
        float a = std::acos(std::fmax(-1.0f, std::fmin(1.0f, dot / len)));
        if (ux * vy - uy * vx < 0) a = -a;
        return a;
    };

    const float theta1 = angle(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry);
    float dTheta = angle((x1p - cxp) / rx, (y1p - cyp) / ry,
                         (-x1p - cxp) / rx, (-y1p - cyp) / ry);
    if (!sweep && dTheta > 0) dTheta -= 2 * kPi;
    if (sweep && dTheta < 0) dTheta += 2 * kPi;

    // Split into segments of at most 90° and emit one cubic per segment.
    const int segments = static_cast<int>(std::ceil(std::fabs(dTheta) / (kPi * 0.5f)));
    const float delta = dTheta / segments;
    const float t = 4.0f / 3.0f * std::tan(delta * 0.25f);

    float cosT1 = std::cos(theta1), sinT1 = std::sin(theta1);
    float theta = theta1;
    for (int i = 0; i < segments; ++i) {
        const float theta2 = theta + delta;
        const float cosT2 = std::cos(theta2), sinT2 = std::sin(theta2);

        auto point = [&](float cosT, float sinT, float& px, float& py) {
            const float ex = rx * cosT, ey = ry * sinT;
            px = cosPhi * ex - sinPhi * ey + cx;
            py = sinPhi * ex + cosPhi * ey + cy;
        };
        auto deriv = [&](float cosT, float sinT, float& vx, float& vy) {
            const float ex = -rx * sinT, ey = ry * cosT;
            vx = cosPhi * ex - sinPhi * ey;
            vy = sinPhi * ex + cosPhi * ey;
        };

        float p1x, p1y, p2x, p2y, d1x, d1y, d2x, d2y;
        point(cosT1, sinT1, p1x, p1y);
        point(cosT2, sinT2, p2x, p2y);
        deriv(cosT1, sinT1, d1x, d1y);
        deriv(cosT2, sinT2, d2x, d2y);

        shape.cubicTo(p1x + t * d1x, p1y + t * d1y, p2x - t * d2x, p2y - t * d2y, p2x, p2y);

        theta = theta2;
        cosT1 = cosT2;
        sinT1 = sinT2;
    }
}

}  // namespace

bool appendSvgPath(tvg::Shape& shape, const char* d, float sx, float sy) {
    if (!d) return false;
    Cursor c{d};

    float curX = 0, curY = 0;       // current point
    float startX = 0, startY = 0;   // subpath start (for Z)
    float ctrlX = 0, ctrlY = 0;     // last control point (for S/T reflection)
    char lastCmd = 0;
    bool any = false;

    c.skipSep();
    while (*c.p) {
        char cmd = *c.p;
        if (std::isalpha(static_cast<unsigned char>(cmd))) {
            ++c.p;
        } else {
            // Implicit command repetition; an implicit M continues as L.
            if (!lastCmd) return false;
            cmd = lastCmd;
            if (cmd == 'M') cmd = 'L';
            if (cmd == 'm') cmd = 'l';
        }
        const bool rel = std::islower(static_cast<unsigned char>(cmd));
        const char op = static_cast<char>(std::toupper(static_cast<unsigned char>(cmd)));

        float x, y, x1, y1, x2, y2;
        switch (op) {
        case 'M':
            if (!c.number(x) || !c.number(y)) return false;
            x *= sx; y *= sy;
            if (rel) { x += curX; y += curY; }
            shape.moveTo(x, y);
            curX = startX = x;
            curY = startY = y;
            any = true;
            break;
        case 'L':
            if (!c.number(x) || !c.number(y)) return false;
            x *= sx; y *= sy;
            if (rel) { x += curX; y += curY; }
            shape.lineTo(x, y);
            curX = x; curY = y;
            break;
        case 'H':
            if (!c.number(x)) return false;
            x *= sx;
            if (rel) x += curX;
            shape.lineTo(x, curY);
            curX = x;
            break;
        case 'V':
            if (!c.number(y)) return false;
            y *= sy;
            if (rel) y += curY;
            shape.lineTo(curX, y);
            curY = y;
            break;
        case 'C':
            if (!c.number(x1) || !c.number(y1) || !c.number(x2) || !c.number(y2) ||
                !c.number(x) || !c.number(y)) return false;
            x1 *= sx; y1 *= sy; x2 *= sx; y2 *= sy; x *= sx; y *= sy;
            if (rel) { x1 += curX; y1 += curY; x2 += curX; y2 += curY; x += curX; y += curY; }
            shape.cubicTo(x1, y1, x2, y2, x, y);
            ctrlX = x2; ctrlY = y2;
            curX = x; curY = y;
            break;
        case 'S': {
            if (!c.number(x2) || !c.number(y2) || !c.number(x) || !c.number(y)) return false;
            x2 *= sx; y2 *= sy; x *= sx; y *= sy;
            if (rel) { x2 += curX; y2 += curY; x += curX; y += curY; }
            const bool reflect = lastCmd && std::strchr("CcSs", lastCmd);
            x1 = reflect ? 2 * curX - ctrlX : curX;
            y1 = reflect ? 2 * curY - ctrlY : curY;
            shape.cubicTo(x1, y1, x2, y2, x, y);
            ctrlX = x2; ctrlY = y2;
            curX = x; curY = y;
            break;
        }
        case 'Q':
            if (!c.number(x1) || !c.number(y1) || !c.number(x) || !c.number(y)) return false;
            x1 *= sx; y1 *= sy; x *= sx; y *= sy;
            if (rel) { x1 += curX; y1 += curY; x += curX; y += curY; }
            // Quadratic → cubic degree elevation.
            shape.cubicTo(curX + 2.0f / 3.0f * (x1 - curX), curY + 2.0f / 3.0f * (y1 - curY),
                          x + 2.0f / 3.0f * (x1 - x), y + 2.0f / 3.0f * (y1 - y), x, y);
            ctrlX = x1; ctrlY = y1;
            curX = x; curY = y;
            break;
        case 'T': {
            if (!c.number(x) || !c.number(y)) return false;
            x *= sx; y *= sy;
            if (rel) { x += curX; y += curY; }
            const bool reflect = lastCmd && std::strchr("QqTt", lastCmd);
            x1 = reflect ? 2 * curX - ctrlX : curX;
            y1 = reflect ? 2 * curY - ctrlY : curY;
            shape.cubicTo(curX + 2.0f / 3.0f * (x1 - curX), curY + 2.0f / 3.0f * (y1 - curY),
                          x + 2.0f / 3.0f * (x1 - x), y + 2.0f / 3.0f * (y1 - y), x, y);
            ctrlX = x1; ctrlY = y1;
            curX = x; curY = y;
            break;
        }
        case 'A': {
            float rx, ry, rot;
            bool large, sweep;
            if (!c.number(rx) || !c.number(ry) || !c.number(rot) || !c.flag(large) ||
                !c.flag(sweep) || !c.number(x) || !c.number(y)) return false;
            // Approximate for rotated arcs under non-uniform scale.
            rx *= std::fabs(sx); ry *= std::fabs(sy);
            x *= sx; y *= sy;
            if (rel) { x += curX; y += curY; }
            arcTo(shape, curX, curY, rx, ry, rot, large, sweep, x, y);
            curX = x; curY = y;
            break;
        }
        case 'Z':
            shape.close();
            curX = startX;
            curY = startY;
            break;
        default:
            return false;  // unknown command
        }
        lastCmd = cmd;
        c.skipSep();
    }
    return any;
}

}  // namespace figo
