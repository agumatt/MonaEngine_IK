#include "Splines.hpp"
#include "../Core/Log.hpp"
#include <math.h>

namespace Mona{

    // Solucion extraida de https://stackoverflow.com/questions/55421835/c-binomial-coefficient-is-too-slow
    // autor: BiagioF
    int BinomialCoefficient(const int n, const int k) {
        MONA_ASSERT(0<=k && k<=n, "BinomialCoefficient: it must be true that 0<=k<=n.");
        std::vector<int> aSolutions(k);
        aSolutions[0] = n - k + 1;
        for (int i = 1; i < k; ++i) {
            aSolutions[i] = aSolutions[i - 1] * (n - k + 1 + i) / (i + 1);
        }
        return aSolutions[k - 1];
    }

    BezierCurve::BezierCurve(int order, std::vector<glm::vec3> controlPoints,float minT, float maxT):
        m_order(order), m_controlPoints(controlPoints), m_minT(minT), m_maxT(maxT){
        MONA_ASSERT(order >= 1,
            "BezierCurve: Order must be at least 1.");
        MONA_ASSERT(controlPoints.size() == order + 1,
            "BezierCurve: Number of points provided does not fit the order. Points must be order plus 1.");
        MONA_ASSERT(m_minT < m_maxT, "maxT must be greater than minT.");
    }

    float BezierCurve::bernsteinBP(int i, int n, float t) {
        return BinomialCoefficient(n, i) * std::pow(t, i) * std::pow((1 - t), (n - i));
    }
    float BezierCurve::normalizeT(float t) {
        if (t < m_minT || m_maxT < t) {
            MONA_LOG_ERROR("BezierCurve: t must be a value between {0} and {1}.", m_minT, m_maxT);
        }
        return (t - m_minT) / (m_maxT - m_minT);
    }
    glm::vec3 BezierCurve::evalCurve(float t) {
        glm::vec3 result = { 0,0,0 };
        int n = m_order;
        t = normalizeT(t);
        for (int i = 0; i <= n; i++) {
            result += bernsteinBP(i, n, t)*m_controlPoints[i];
        }
        return result;
    }
    glm::vec3 BezierCurve::getVelocity(float t) {
        glm::vec3 result = { 0,0,0 };
        int n = m_order;
        t = normalizeT(t);
        for (int i = 0; i <= n-1; i++) {
            result += bernsteinBP(i, n-1, t) * (m_controlPoints[i+1] - m_controlPoints[i]);
        }
        result *= n;
        return result;
    }



    CubicBezierSpline::CubicBezierSpline(std::vector<glm::vec3> splinePoints) {
        // con los puntos recibidos P0 y P4 por segmento, generamos los puntos de control faltantes P1 y P2 para cada segmento(sub curva de bezier)
        // P(1,i-1) + 4P(1,i) + P(1, i+1) = 4K(i) + 2K(i+1)  i pertenece a [1, n-2]
        // 2P(1, 0) + P(1,1) = K(O) + 2K(1)
        // 2P(1,n-2) + 7P(1,n-1) = 8K(n-1) + K(n)
        // primero se obtiene P1 para cada segmento


    };


    
}