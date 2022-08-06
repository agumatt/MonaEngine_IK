#pragma once
#ifndef PARAMETRICCURVES_HPP
#define PARAMETRICCURVES_HPP

#include "glm/glm.hpp"
#include <glm/gtx/quaternion.hpp>
#include "glm/gtx/vector_angle.hpp"
#include <vector>
#include "../Core/Log.hpp"
#include "../Core/FuncUtils.hpp"
#include "../Core/GlmUtils.hpp"
#include <math.h>

namespace Mona{

    template <int D>
    // linearly interpolated curve
    class LIC {
    private:
        // puntos de la curva
        std::vector<glm::vec<D, float>> m_curvePoints;
        // valores de t
        std::vector<float> m_tValues = { 1,0 };
        // dimension de los puntos
        int m_dimension = 0;
        float m_tEpsilon = 0.000001;
		glm::vec<D, float> getRightHandVelocity(float t) {
			MONA_ASSERT(inTRange(t), "LIC: t must be a value between {0} and {1}.", m_tValues[0], m_tValues.back());
			if (t == m_tValues.back()) { return glm::vec<D, float>(0); }
			for (int i = 0; i < m_tValues.size(); i++) {
				if (m_tValues[i] == t) {
					return (m_curvePoints[i + 1] - m_curvePoints[i]) / (m_tValues[i + 1] - m_tValues[i]);
				}
				else if (m_tValues[i] < t && t < m_tValues[i + 1]) {
					return (m_curvePoints[i + 1] - m_curvePoints[i]) / (m_tValues[i + 1] - m_tValues[i]);
				}
			}
			return glm::vec<D, float>(0);;
		}
		bool correctTValues() {
			bool needCorrection = false;
			for (int i = 1; i < m_tValues.size(); i++) {
				if (m_tValues[i] - m_tValues[i-1] < m_tEpsilon) {
					needCorrection = true;
                    break;
				}
			}
			if (!needCorrection) {
				return true;
			}

			// correccion de valores
            // de izquierda a derecha
			std::vector<int> targetIndexesLeftToRight;
			for (int i = 1; i < getNumberOfPoints()-1; i++) {
				if (m_tValues[i] - m_tValues[i - 1] < m_tEpsilon) {
					targetIndexesLeftToRight.push_back(i);
				}
			}
			for (int i = targetIndexesLeftToRight.size() - 1; 0 <= i; i--) {
				int tIndex = targetIndexesLeftToRight[i];
				m_tValues[tIndex] = (m_tValues[tIndex] + m_tValues[tIndex + 1]) / 2.0f;
				m_curvePoints[tIndex] = (m_curvePoints[tIndex] + m_curvePoints[tIndex + 1]) / 2.0f;
			}
            // de derecha a izquierda
			std::vector<int> targetIndexesRightToLeft;
			for (int i = getNumberOfPoints() - 2; 0 <= i; i--) {
				if (m_tValues[i + 1] - m_tValues[i] <= m_tEpsilon) {
					targetIndexesLeftToRight.push_back(i);
				}
			}
			for (int i = targetIndexesRightToLeft.size() - 1; 0 <= i; i--) {
				int tIndex = targetIndexesRightToLeft[i];
				m_tValues[tIndex] = (m_tValues[tIndex] + m_tValues[tIndex - 1]) / 2.0f;
				m_curvePoints[tIndex] = (m_curvePoints[tIndex] + m_curvePoints[tIndex - 1]) / 2.0f;
			}

			// chequeo final
			for (int i = 1; i < m_tValues.size(); i++) {
				if (m_tValues[i] - m_tValues[i - 1] < m_tEpsilon) {
					return false;
				}
			}
			return true;
		}
    public:
        glm::vec2 getTRange() const { return glm::vec2({ m_tValues[0], m_tValues.back() }); }
        bool inTRange(float t) const { return m_tValues[0] <= t && t <= m_tValues.back(); }
        int getNumberOfPoints() const { return m_curvePoints.size(); }
        int getDimension() const { return m_dimension; }
        glm::vec<D, float> getStart() { return m_curvePoints[0]; }
        glm::vec<D, float> getEnd() { return m_curvePoints.back(); }
        LIC() = default;
        LIC(std::vector<glm::vec<D, float>> curvePoints, std::vector<float> tValues, float tEpsilon = 0.000001) {
            MONA_ASSERT(1 < curvePoints.size(), "LIC: must provide at least two points.");
            MONA_ASSERT(curvePoints.size() == tValues.size(), "LIC: there must be exactly one tValue per spline point.");
            MONA_ASSERT(0 < tEpsilon, "LIC: tEpsilon must be greater than 0.");
            // chequeamos que los tValues vengan correctamente ordenados
            for (int i = 1; i < tValues.size(); i++) {
                MONA_ASSERT(m_tEpsilon <= tValues[i] - tValues[i-1], 
                    "LIC: tValues must come in a strictly ascending order and differ in more than tEpsilon.");
            }
            m_curvePoints = curvePoints;
            m_tValues = tValues;
            m_dimension = D;
            m_tEpsilon = tEpsilon;

            // se ajustan los extremos con un epsilon
            funcUtils::epsilonAdjustment_add(m_tValues.back(), m_tEpsilon);
            funcUtils::epsilonAdjustment_subtract(m_tValues[0], m_tEpsilon);
        }

        glm::vec<D, float> getVelocity(float t, bool rightHandVelocity = false) {
            if (rightHandVelocity) {
                return getRightHandVelocity(t);
            }
            MONA_ASSERT(inTRange(t), "LIC: t must be a value between {0} and {1}.", m_tValues[0], m_tValues.back());
            if (t == m_tValues[0]) { return glm::vec<D, float>(0); }
            for (int i = 0; i < m_tValues.size(); i++) {
                if (m_tValues[i] == t) {
                    return (m_curvePoints[i] - m_curvePoints[i - 1]) / (m_tValues[i] - m_tValues[i - 1]);
                }
                else if (m_tValues[i] < t && t < m_tValues[i + 1]) {
                    return (m_curvePoints[i + 1] - m_curvePoints[i]) / (m_tValues[i + 1] - m_tValues[i]);
                }
            }
            return glm::vec<D, float>(0);
        }

		

        glm::vec<D, float> getAcceleration(int pointIndex) {
            MONA_ASSERT(0 < pointIndex && pointIndex < m_tValues.size() - 1, "LIC: pointIndex must be an inner point.");
            return (getVelocity(getTValue(pointIndex + 1)) - getVelocity(getTValue(pointIndex))) / (getTValue(pointIndex + 1) - getTValue(pointIndex));
        }

        glm::vec<D, float> evalCurve(float t) {
            MONA_ASSERT(inTRange(t), "LIC: t must be a value between {0} and {1}.", m_tValues[0], m_tValues.back());
            for (int i = 0; i < m_tValues.size() - 1; i++) {
                if (m_tValues[i] <= t && t <= m_tValues[i + 1]) {
                    float fraction = funcUtils::getFraction(m_tValues[i], m_tValues[i + 1], t);
                    return funcUtils::lerp(m_curvePoints[i], m_curvePoints[i + 1], fraction);
                }
            }
            return glm::vec<D, float>(0);
        }

        

        void displacePointT(int pointIndex, int lowIndex, int highIndex, float newT, 
            bool scalePoints = true, float pointScalingRatio = 1, int maxCorrectionSteps = 3) {
            MONA_ASSERT(lowIndex < highIndex && 0 <= lowIndex && highIndex < m_tValues.size(), "LIC: low and high index must be within bounds.");
            MONA_ASSERT(lowIndex <= pointIndex && pointIndex <= highIndex, "LIC: input point index must be within input bounds");
            float oldT = m_tValues[pointIndex];
            if (oldT == newT) { return; }
            if (pointIndex != lowIndex) {
                MONA_ASSERT(getTValue(lowIndex) < newT,
                    "LIC: If not the lower end, newT cannot subceed or match original low t bound.");
                float fractionBelow = funcUtils::getFraction(m_tValues[lowIndex], oldT, newT);
                for (int i = lowIndex + 1; i <= pointIndex; i++) {
                    m_tValues[i] = funcUtils::lerp(m_tValues[lowIndex], m_tValues[i], fractionBelow);
                    if (scalePoints) {
                        m_curvePoints[i] = funcUtils::lerp(m_curvePoints[lowIndex], m_curvePoints[i], fractionBelow * pointScalingRatio);
                    }
                }
            }

            if (pointIndex != highIndex) {
                MONA_ASSERT(newT < getTValue(highIndex),
                    "LIC: If not the higher end, newT cannot exceed or match original high t bound.");
                float fractionAbove = funcUtils::getFraction(m_tValues[highIndex], oldT, newT);
                for (int i = pointIndex; i < highIndex; i++) {
                    if (pointIndex < i || pointIndex == lowIndex) {
                        m_tValues[i] = funcUtils::lerp(m_tValues[highIndex], m_tValues[i], fractionAbove);
                        if (scalePoints) {
                            m_curvePoints[i] = funcUtils::lerp(m_curvePoints[highIndex], m_curvePoints[i], fractionAbove * pointScalingRatio);
                        }
                    }
                }
            }

            // correccion de valores
            bool corrected = false;
            int correctionSteps = 0;
            while (!corrected && correctionSteps < maxCorrectionSteps) {
                corrected = correctTValues();
                correctionSteps += 1;
            }
            MONA_ASSERT(corrected, "LIC: tValues were pushed too close together.");                       
        }

        void setCurvePoint(int pointIndex, glm::vec<D, float> newValue) {
            MONA_ASSERT(0 <= pointIndex && pointIndex < m_curvePoints.size(), "LIC: input index must be within bounds");
            m_curvePoints[pointIndex] = newValue;
        }

        LIC<D> sample(float minT, float maxT) {
            MONA_ASSERT(m_tEpsilon <= maxT - minT, "LIC: maxT must be greater than minT by at least tEpsilon.");
            MONA_ASSERT(inTRange(minT) && inTRange(maxT), "LIC: Both minT and maxT must be in t range.");
            std::vector<float> sampleTValues;
            sampleTValues.reserve(m_tValues.size());
            std::vector<glm::vec<D, float>> samplePoints;
            samplePoints.reserve(m_tValues.size());
            int startInd = 0;
            for (int i = 0; i < m_tValues.size() - 1; i++) {
                if (m_tValues[i] <= minT  && minT < m_tValues[i + 1]) {
                    sampleTValues.push_back(minT);
                    samplePoints.push_back(evalCurve(minT));
                    startInd = i + 1;
                    break;
                }
            }
            for (int i = startInd; i < m_tValues.size() - 1; i++) {
                if (m_tEpsilon <= maxT - m_tValues[i]) {
					sampleTValues.push_back(m_tValues[i]);
					samplePoints.push_back(m_curvePoints[i]);
                }
                else {
					sampleTValues.push_back(maxT);
					samplePoints.push_back(evalCurve(maxT));
					break;
                }
            }
            if (samplePoints.size() == 0 || samplePoints.size() == 1) {
                samplePoints = { evalCurve(minT), evalCurve(maxT) };
                sampleTValues = { minT, maxT };
            }
            return LIC(samplePoints, sampleTValues);
        }

        void scale(glm::vec<D, float> scaling) {
            for (int i = 0; i < m_curvePoints.size(); i++) {
                m_curvePoints[i] *= scaling;
            }
        }

        void translate(glm::vec<D, float> translation) {
            for (int i = 0; i < m_curvePoints.size(); i++) {
                m_curvePoints[i] += translation;
            }
        }

        void rotate(glm::fquat rotation) {
            MONA_ASSERT(D == 3, "LIC: Rotation is only valid for dimension 3 LICs.");
            for (int i = 0; i < m_curvePoints.size(); i++) {
                m_curvePoints[i] = glm::rotate(rotation, glm::vec4(m_curvePoints[i], 1));
            }
        }

        void offsetTValues(float offset) {
            for (int i = 0; i < m_tValues.size(); i++) {
                m_tValues[i] += offset;
            }
        }

        static LIC<D> join(const LIC& curve1, const LIC& curve2) {
            std::vector<float> jointTValues;
            jointTValues.reserve(curve1.m_curvePoints.size() + curve2.m_curvePoints.size());
            std::vector<glm::vec<D, float>> jointCurvePoints;
            jointCurvePoints.reserve(curve1.m_curvePoints.size() + curve2.m_curvePoints.size());
			jointTValues = curve1.m_tValues;
			jointCurvePoints = curve1.m_curvePoints;
			for (int i = 0; i < curve2.m_tValues.size(); i++) {
				if (curve1.m_tEpsilon <= curve2.m_tValues[i] - curve1.m_tValues.back()) {
					jointTValues.insert(jointTValues.end(), curve2.m_tValues.begin() + i, curve2.m_tValues.end());
					jointCurvePoints.insert(jointCurvePoints.end(), curve2.m_curvePoints.begin() + i, curve2.m_curvePoints.end());
					break;
				}
			}
			return LIC(jointCurvePoints, jointTValues, curve1.m_tEpsilon);
            
        }

        static LIC<D> transition(const LIC& curve1, const LIC& curve2, float transitionT) {
            MONA_ASSERT(curve1.inTRange(transitionT) && curve2.inTRange(transitionT), "LIC: transitionT must be within t ranges of both curves.");
            std::vector<float> transitionTValues;
            transitionTValues.reserve(curve1.m_curvePoints.size() + curve2.m_curvePoints.size());
            std::vector<glm::vec<D, float>> transitionCurvePoints;
            transitionCurvePoints.reserve(curve1.m_curvePoints.size() + curve2.m_curvePoints.size());
            for (int i = 0; i < curve1.m_tValues.size(); i++) {
                if (curve1.m_tValues[i] < transitionT) {
                    transitionTValues.push_back(curve1.m_tValues[i]);
                    transitionCurvePoints.push_back(curve1.m_curvePoints[i]);
                }
                else { break; }
            }
            for (int i = 0; i < curve2.m_tValues.size(); i++) {
                if (transitionT <= curve2.m_tValues[i]) {
                    transitionTValues.push_back(curve2.m_tValues[i]);
                    transitionCurvePoints.push_back(curve2.m_curvePoints[i]);
                }
            }
            if (transitionTValues.size() == 1) {
                float extraTValue = transitionTValues[0];
                funcUtils::epsilonAdjustment_add(extraTValue, curve1.m_tEpsilon);
                transitionTValues.push_back(extraTValue);
                transitionCurvePoints.push_back(transitionCurvePoints[0]);
            }           
            return LIC(transitionCurvePoints, transitionTValues);
        }

        float getTValue(int pointIndex) const {
            MONA_ASSERT(0 <= pointIndex && pointIndex < m_tValues.size(), "LIC: input index must be within bounds.");
            return m_tValues[pointIndex];
        };

        glm::vec<D, float> getCurvePoint(int pointIndex) const {
            MONA_ASSERT(0 <= pointIndex && pointIndex < m_curvePoints.size(), "LIC: input index must be within bounds.");
            return m_curvePoints[pointIndex];
        };


        int getClosestPointIndex(float tValue) const {
            if (tValue < m_tValues[0]) {
                return 0;
            }
            for (int i = 1; i < m_tValues.size() - 1; i++) {
                if (m_tValues[i] <= tValue && tValue <= m_tValues[i + 1]) {
                    if ((m_tValues[i + 1] - tValue) <= (tValue - m_tValues[i])) { return i + 1; }
                    else { return i; }
                }
            }
            return m_tValues.size() - 1;
        }

        int getPointIndex(float tValue, bool getNext=false) const {
            if (tValue < m_tValues[0]) {
                MONA_ASSERT(getNext, "LIC: There is no point at t={0} or before", tValue);
                return 0;
            }
            for (int i = 1; i < m_tValues.size() - 1; i++) {
                if (m_tValues[i] <= tValue && tValue <= m_tValues[i + 1]) {
                    if (tValue == m_tValues[i]) { return i; }
                    if (tValue == m_tValues[i + 1]) { return i + 1; }
                    if (getNext) { return i + 1; }
                    return i;
                }
            }
            MONA_ASSERT(!getNext, "LIC: There is no point at t={0} or after", tValue);
            return m_tValues.size() - 1;
        }

        static LIC<D> connect(LIC<D> curve1, LIC<D> curve2, float curve2TOffset) {
            glm::vec<D, float> velEndC1 = curve1.getVelocity(curve1.getTRange()[1]);
            glm::vec<D, float> velStartC2 = curve2.getVelocity(curve2.getTRange()[0]);
            glm::vec<D, float> transitionVel = (velEndC1 + velStartC2) / 2.0f;
            float tDiff = (curve2.getTRange()[0] + curve2TOffset) - curve1.getTRange()[1];
            glm::vec<D, float> transitionPoint = curve1.m_curvePoints.back() + transitionVel * tDiff;
            // desplazamos las posiciones de la parte 2 al punto de transicion
            curve2.translate(- curve2.getStart() + transitionPoint);
            // luego hacemos el desplazamiento temporal
            curve2.offsetTValues(curve2TOffset);
            return LIC<D>::join(curve1, curve2);
        }

		static LIC<D> connectPoint(LIC<D> curve1, glm::vec<D,float> extraPoint, float extraPointTValue, float extraPointTOffset) {
            glm::vec<D, float> transitionVel = curve1.getVelocity(curve1.getTRange()[1]);
            float extraPointFinalTValue = extraPointTValue + extraPointTOffset;
            funcUtils::epsilonAdjustment_add(extraPointFinalTValue, curve1.m_tEpsilon);
            if (curve1.getTRange()[1] < extraPointFinalTValue) {
				float tDiff = extraPointFinalTValue - curve1.getTRange()[1];
                glm::vec<D, float> modifiedPoint = curve1.m_curvePoints.back() + transitionVel * tDiff;
                curve1.m_curvePoints.push_back(modifiedPoint);
                curve1.m_tValues.push_back(extraPointFinalTValue);
            }
            return curve1;	
		}

		void fitStartAndDir(glm::vec3 newStart, glm::vec3 targetDirection) {
            MONA_ASSERT(m_dimension == 3, "LIC: LIC must have a dimension equal to 3.");
            translate(-getStart());
			// rotarla para que quede en linea con las pos inicial y final
			glm::fquat targetRotation = glm::identity<glm::fquat>();
			glm::vec3 originalDirection = glm::normalize(getEnd() - getStart());
			if (originalDirection != targetDirection) {
                glm::vec3 crossVec = glm::cross(originalDirection, targetDirection);
				glm::vec3 rotAxis = glm::normalize(crossVec);
                float rotAngle = glm::asin(glm::length(crossVec));
				targetRotation = glm::fquat(rotAngle, rotAxis);
			}
			rotate(targetRotation);
			translate(newStart);
		}

		void fitEnds(glm::vec3 newStart, glm::vec3 newEnd) {
			MONA_ASSERT(m_dimension == 3, "LIC: LIC must have a dimension equal to 3.");

			glm::vec3 targetDirection = glm::normalize(newEnd - newStart);
            fitStartAndDir(newStart, targetDirection);

			// escalarla para que llegue a newEnd
			float origLength = glm::distance(getStart(), getEnd());
			float targetLength = glm::distance(newStart, newEnd);
			scale(glm::vec3(targetLength / origLength));
			translate(-getStart() + newStart);
		}

		void debugPrintTValues() {
            std::cout << "tValues: " << std::endl;
            std::cout << funcUtils::vecToString(m_tValues) << std::endl;
		}


		void debugPrintCurvePoints() {
            MONA_ASSERT(m_dimension == 3, "LIC: only available for dim 3 LICs.");
			glmUtils::printColoredStdVector(m_curvePoints);

		}
    };
    
}





#endif