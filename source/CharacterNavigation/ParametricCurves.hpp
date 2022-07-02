#pragma once
#ifndef PARAMETRICCURVES_HPP
#define PARAMETRICCURVES_HPP
#include "glm/glm.hpp"
#include <glm/gtx/quaternion.hpp>
#include <vector>

namespace Mona{

    // linearly interpolated curve
    template <int D>
    class LIC {
    private:
        // puntos de la curva
        std::vector<glm::vec<D, float>> m_curvePoints;
        // valores de t
        std::vector<float> m_tValues = { 1,0 };
        // dimension de los puntos
        int m_dimension = 0;
    public:
        LIC() = default;
        LIC(std::vector<glm::vec<D, float>> curvePoints, std::vector<float> tValues);
        glm::vec2 getTRange() { return glm::vec2({ m_tValues[0], m_tValues.back() }); }
        bool inTRange(float t) { return m_tValues[0] <= t && t <= m_tValues.back(); }
        const std::vector<float>& getTValues() const { return m_tValues; };
        int getNumberOfPoints() const { return m_curvePoints.size(); }
        glm::vec<D, float> evalCurve(float t);
        glm::vec<D, float> getLeftHandVelocity(float t);
        glm::vec<D, float> getRightHandVelocity(float t);
        void displacePointT(int pointIndex, float newT, float pointScalingRatio = 1);
        void setCurvePoint(int pointIndex, glm::vec<D, float> newValue);
        LIC getSubCurve(int minPointIndex, int maxPointIndex);
        void scale(glm::vec<D, float> scaling);
        void translate(glm::vec<D, float> translation);
        void rotate(glm::fquat rotation);
        int getDimension() { return m_dimension; }
    };   


    
}





#endif