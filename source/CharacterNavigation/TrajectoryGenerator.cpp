#include "TrajectoryGenerator.hpp"
#include "../Core/GlmUtils.hpp"
#include "../Core/FuncUtils.hpp"
#include "glm/gtx/rotate_vector.hpp"
#include "glm/gtx/vector_angle.hpp"
#include "IKRig.hpp"
#include "../World/ComponentManager.hpp"
#include "../Animation/AnimationClip.hpp"
#include <algorithm>

namespace Mona{

    // funciones para descenso de gradiente

    // primer termino: acercar los modulos de las velocidades
    std::function<float(const std::vector<float>&, TGData*)> term1Function =
        [](const std::vector<float>& varPCoord, TGData* dataPtr)->float {
        float result = 0;
        for (int i = 0; i < dataPtr->pointIndexes.size(); i++) {
            int pIndex = dataPtr->pointIndexes[i];
            result += glm::distance2(dataPtr->varCurve->getPointVelocity(pIndex),  dataPtr->baseCurve.getPointVelocity(pIndex));
            result += glm::distance2(dataPtr->varCurve->getPointVelocity(pIndex, true), dataPtr->baseCurve.getPointVelocity(pIndex, true));
        }
        return result;
    };

    std::function<float(const std::vector<float>&, int, TGData*)> term1PartialDerivativeFunction =
        [](const std::vector<float>& varPCoord, int varIndex, TGData* dataPtr)->float {
        int D = 3;
        int pIndex = dataPtr->pointIndexes[varIndex / D];
        int coordIndex = varIndex % D;
		float t_kPrev = dataPtr->varCurve->getTValue(pIndex - 1);
		float t_kCurr = dataPtr->varCurve->getTValue(pIndex);
		float t_kNext = dataPtr->varCurve->getTValue(pIndex + 1);
		glm::vec3 lVel = dataPtr->varCurve->getPointVelocity(pIndex);
		glm::vec3 rVel = dataPtr->varCurve->getPointVelocity(pIndex, true);
        glm::vec3 baseLVel = dataPtr->baseCurve.getPointVelocity(pIndex);
		glm::vec3 baseRVel = dataPtr->baseCurve.getPointVelocity(pIndex, true);
        float result = 0;
        result += 2 * (lVel[coordIndex] - baseLVel[coordIndex]) * (1 / (t_kCurr - t_kPrev));
        result += 2 * (rVel[coordIndex] - baseRVel[coordIndex]) * (-1 / (t_kNext - t_kCurr));
        return result;
    };

    std::function<void(std::vector<float>&, TGData*, std::vector<float>&, int)>  postDescentStepCustomBehaviour =
        [](std::vector<float>& varPCoord, TGData* dataPtr, std::vector<float>& argsRawDelta, int varIndex_progressive)->void {
        glm::vec3 newPos;
        int D = 3;
        for (int i = 0; i < dataPtr->pointIndexes.size(); i++) {
            for (int j = 0; j < D; j++) {
                if (varPCoord[i * D + j] <= dataPtr->minValues[i * D + j]) {
                    varPCoord[i * D + j] = dataPtr->minValues[i * D + j];
                    argsRawDelta[i * D + j] *= 0.5;
                }
                newPos[j] = varPCoord[i * D + j];
            }
            int pIndex = dataPtr->pointIndexes[i];
            dataPtr->varCurve->setCurvePoint(pIndex, newPos);
        }
    };


    TrajectoryGenerator::TrajectoryGenerator(IKRig* ikRig) {
        m_ikRig = ikRig;
    }

    void TrajectoryGenerator::init() {
        FunctionTerm<TGData> term1(term1Function, term1PartialDerivativeFunction);
        //FunctionTerm<TGData> term2(term2Function, term2PartialDerivativeFunction);
        m_gradientDescent = GradientDescent<TGData>({ term1 }, 0, &m_tgData, postDescentStepCustomBehaviour);
        m_tgData.descentRate = 1 / pow(10, 5);
        m_tgData.maxIterations = 600;
        m_tgData.alphaValue = 0.8;
        m_tgData.betaValue = 0.2;
        m_tgData.targetPosDelta = 1 /pow(10, 6);
    }

	void TrajectoryGenerator::generateNewTrajectories(AnimationIndex animIndex,
		ComponentManager<TransformComponent>& transformManager,
		ComponentManager<StaticMeshComponent>& staticMeshManager) {
		// las trayectorias anteriores siempre deben llegar hasta el currentFrame

        IKRigConfig* config = m_ikRig->getAnimationConfig(animIndex);

        for (ChainIndex i = 0; i < m_ikRig->getChainNum(); i++) {
			JointIndex eeIndex = m_ikRig->getIKChain(i)->getEndEffector();
			generateEETrajectory(i, config, transformManager, staticMeshManager);
		}

		// queda setear la trayectoria de la cadera
		generateHipTrajectory(config, transformManager, staticMeshManager);
	}

    void TrajectoryGenerator::generateFixedTrajectory(glm::vec2 basePos,
        glm::vec2 timeRange, float supportHeight,
        ChainIndex ikChain, IKRigConfig* config,
        ComponentManager<TransformComponent>& transformManager,
        ComponentManager<StaticMeshComponent>& staticMeshManager) {
        EEGlobalTrajectoryData* trData = config->getEETrajectoryData(ikChain);
        float calcHeight = m_environmentData.getTerrainHeight(glm::vec2(basePos), transformManager, staticMeshManager);
        glm::vec3 fixedPos(basePos, calcHeight + supportHeight);
        LIC<3> fixedCurve({ fixedPos, fixedPos }, { timeRange[0], timeRange[1] });
        trData->setTargetTrajectory(fixedCurve, TrajectoryType::STATIC, -2);
    }

	void TrajectoryGenerator::generateEETrajectory(ChainIndex ikChain, IKRigConfig* config,
		ComponentManager<TransformComponent>& transformManager,
		ComponentManager<StaticMeshComponent>& staticMeshManager) {
		EEGlobalTrajectoryData* trData = config->getEETrajectoryData(ikChain);
		FrameIndex currentFrame = config->getCurrentFrameIndex();
        FrameIndex nextFrame = config->getNextFrameIndex();
		float currentAnimTime = config->getAnimationTime(currentFrame);
        float nextAnimTime = config->getAnimationTime(nextFrame);
		float currentRepTime = config->getReproductionTime(currentFrame);
        float currSupportHeight = trData->getSupportHeight(currentFrame);
        glm::vec3 currentPos = trData->getSavedPosition(currentFrame);
		if (config->getAnimationType() == AnimationType::IDLE) {
            generateFixedTrajectory(glm::vec2(currentPos), { currentRepTime, currentRepTime + config->getAnimationDuration() },
                currSupportHeight ,ikChain, config, transformManager, staticMeshManager);
			return;
		}
		EETrajectory originalTrajectory = trData->getSubTrajectory(currentAnimTime);
        TrajectoryType trType;
		if (originalTrajectory.isDynamic()) {
            trType = TrajectoryType::DYNAMIC;
		}
		else {
			trType = TrajectoryType::STATIC;
		}
        LIC<3>& baseCurve = originalTrajectory.getEECurve();

        FrameIndex initialFrame = config->getFrame(baseCurve.getTRange()[0]);
        FrameIndex finalFrame = config->getFrame(baseCurve.getTRange()[1]);

        // llevar a reproduction time
        baseCurve.offsetTValues(-currentAnimTime);
        baseCurve.offsetTValues(currentRepTime);

		if (glm::length(baseCurve.getEnd() - baseCurve.getStart()) == 0) {
			generateFixedTrajectory(glm::vec2(currentPos), { baseCurve.getTRange()[0], baseCurve.getTRange()[1] },
				currSupportHeight, ikChain, config, transformManager, staticMeshManager);
			return;
		}

        glm::vec3 initialPos = trData->getSavedPosition(initialFrame);
        float initialRepTime = baseCurve.getTValue(0);
		
		glm::vec3 originalDirection = glm::normalize(baseCurve.getEnd() - baseCurve.getStart());
		glm::vec2 targetXYDirection = glm::normalize(glm::rotate(glm::vec2(originalDirection), m_ikRig->getRotationAngle()));
		
        // chequear si hay info de posicion valida previa
        if (!(trData->isSavedDataValid(initialFrame) && trData->getTargetTrajectory().getEECurve().inTRange(initialRepTime))) {
            float referenceTime = config->getReproductionTime(currentFrame);
            glm::vec3 sampledCurveReferencePoint = baseCurve.evalCurve(referenceTime);
            float xyDistanceToStart = glm::distance(glm::vec2(baseCurve.getStart()), glm::vec2(sampledCurveReferencePoint));
            glm::vec2 currEEXYPoint = glm::vec2(currentPos);
            float supportHeight = trData->getSupportHeight(initialFrame);
            initialPos = calcStrideStartingPoint(supportHeight, currEEXYPoint, xyDistanceToStart,
                targetXYDirection, 4, transformManager, staticMeshManager);
        }
        
        float targetDistance = glm::distance(baseCurve.getStart(), baseCurve.getEnd());
		float supportHeightStart = initialPos[2];
		float supportHeightEnd = trData->getSupportHeight(finalFrame);
        std::vector<glm::vec3> strideData = calcStrideData(supportHeightStart, supportHeightEnd,
            initialPos, targetDistance, targetXYDirection, 8, transformManager, staticMeshManager);
        if (strideData.size() == 0) { // si no es posible avanzar por la elevacion del terreno
			generateFixedTrajectory(glm::vec2(currentPos), { baseCurve.getTRange()[0], baseCurve.getTRange()[1] },
				currSupportHeight, ikChain, config, transformManager, staticMeshManager);
            return;
        }
        glm::vec3 finalPos = strideData.back();
        baseCurve.fitEnds(initialPos, finalPos);

        // DEBUG
        float repCountOffset_d = config->getNextFrameIndex() == 0 ? 1 : 0;
        float transitionTime_d = config->getReproductionTime(config->getNextFrameIndex(), repCountOffset_d);
        int currSubTrID_d = trData->getTargetTrajectory().getSubTrajectoryID();
        int newSubTrID_d = originalTrajectory.getSubTrajectoryID();
        if (currSubTrID_d == newSubTrID_d) {
            trData->setTargetTrajectory(LIC<3>::transition(trData->getTargetTrajectory().getEECurve(),
                baseCurve, transitionTime_d), trType, newSubTrID_d, baseCurve);
        }
        else {
            trData->setTargetTrajectory(baseCurve, trType, newSubTrID_d, baseCurve);
        }
        return;
        // DEBUG

        // setear los minimos de altura y aplicar el descenso de gradiente
        m_tgData.pointIndexes.clear();
        m_tgData.pointIndexes.reserve(baseCurve.getNumberOfPoints());
        m_tgData.minValues.clear();
        m_tgData.minValues.reserve(baseCurve.getNumberOfPoints() * 3);
        for (int i = 1; i < baseCurve.getNumberOfPoints() - 1; i++) {
            m_tgData.pointIndexes.push_back(i);
        }
        std::vector<std::pair<int, int>> curvePointIndex_stridePointIndex;
        std::vector<int> baseCurveIndices;
        baseCurveIndices.reserve(baseCurve.getNumberOfPoints());
        std::vector<int> strideDataIndices;
        strideDataIndices.reserve(strideData.size());
        for (int i = 0; i < m_tgData.pointIndexes.size(); i++) { baseCurveIndices.push_back(m_tgData.pointIndexes[i]); }
        for (int i = 0; i < strideData.size(); i++) { strideDataIndices.push_back(i); }
        while (0 < strideDataIndices.size() && 0 < baseCurveIndices.size()) {
            float minDistance = std::numeric_limits<float>::max();
            int minBaseCurveIndex = -1;
            int savedI = -1;
            int strideDataIndex = strideDataIndices[0];
            for (int i = 0; i < baseCurveIndices.size(); i++) {
                float baseCurveIndex = baseCurveIndices[i];
                float currDistance = glm::distance2(glm::vec2(strideData[strideDataIndex]), glm::vec2(baseCurve.getCurvePoint(baseCurveIndex)));
                if (currDistance < minDistance) {
                    minDistance = currDistance;
                    minBaseCurveIndex = baseCurveIndex;
                    savedI = i;
                }
            }
            curvePointIndex_stridePointIndex.push_back({ minBaseCurveIndex, strideDataIndex });
            baseCurveIndices.erase(baseCurveIndices.begin() + savedI);
            strideDataIndices.erase(strideDataIndices.begin());
        }

        m_tgData.minValues = std::vector<float>(m_tgData.pointIndexes.size() * 3, std::numeric_limits<float>::lowest());
        std::vector<int> assignedTGDataPointIndexes;
        for (int i = 0; i < curvePointIndex_stridePointIndex.size(); i++) {
            int curvePointIndex = curvePointIndex_stridePointIndex[i].first;
            int tgDataIndex = -1;
            for (int j = 0; j < m_tgData.pointIndexes.size(); j++) {
                if (curvePointIndex == m_tgData.pointIndexes[j]) {
                    tgDataIndex = j;
                    break;
                }
            }
            if (tgDataIndex != -1) {
                int stridePointIndex = curvePointIndex_stridePointIndex[i].second;
                m_tgData.minValues[tgDataIndex * 3 + 2] = strideData[stridePointIndex][2];
                assignedTGDataPointIndexes.push_back(tgDataIndex);
            }                  
        }
        if (assignedTGDataPointIndexes[0] != 0) {
            m_tgData.minValues[2] = strideData[0][2];
            assignedTGDataPointIndexes.insert(assignedTGDataPointIndexes.begin(), 0);
        }
        if (assignedTGDataPointIndexes.back() != m_tgData.pointIndexes.size()-1) {
            m_tgData.minValues.back() = strideData.back()[2];
            assignedTGDataPointIndexes.push_back(m_tgData.pointIndexes.size() - 1);
        }

        
        for (int i = 0; i < m_tgData.pointIndexes.size(); i++) {
            int minLerpIndex = -1;
            int maxLerpIndex = -1;
            float minVal;
            float maxVal;
            int currIndex = i;
            for (int j = 0; j < assignedTGDataPointIndexes.size()-1; j++) {
                if (assignedTGDataPointIndexes[j] < currIndex && currIndex < assignedTGDataPointIndexes[j + 1]) {
                    minLerpIndex = assignedTGDataPointIndexes[j];
                    maxLerpIndex = assignedTGDataPointIndexes[j + 1];
                    minVal = m_tgData.minValues[minLerpIndex * 3 + 2];
                    maxVal = m_tgData.minValues[maxLerpIndex * 3 + 2];
                    break;
                }
            }
            if (minLerpIndex != -1) {
                m_tgData.minValues[currIndex * 3 + 2] = funcUtils::lerp(minVal, maxVal, funcUtils::getFraction(minLerpIndex, maxLerpIndex, currIndex));
            }

        }

        // valores iniciales y curva base
        std::vector<float> initialArgs(m_tgData.pointIndexes.size() * 3);
        for (int i = 0; i < m_tgData.pointIndexes.size(); i++) {
            int pIndex = m_tgData.pointIndexes[i];
            for (int j = 0; j < 3; j++) {
                initialArgs[i * 3 + j] = baseCurve.getCurvePoint(pIndex)[j];
            }
        }
        m_tgData.baseCurve = baseCurve;
        m_tgData.varCurve = &baseCurve;

        m_gradientDescent.setArgNum(initialArgs.size());
        m_gradientDescent.computeArgsMin(m_tgData.descentRate, m_tgData.maxIterations, m_tgData.targetPosDelta, initialArgs);

        float repCountOffset = config->getNextFrameIndex() == 0 ? 1 : 0;
        float transitionTime = config->getReproductionTime(config->getNextFrameIndex(), repCountOffset);
        int currSubTrID = trData->getTargetTrajectory().getSubTrajectoryID();
        int newSubTrID = originalTrajectory.getSubTrajectoryID();
        if (currSubTrID == newSubTrID) {
            trData->setTargetTrajectory(LIC<3>::transition(trData->getTargetTrajectory().getEECurve(),
                baseCurve, transitionTime), trType, newSubTrID, baseCurve);
        }
        else {
            trData->setTargetTrajectory(baseCurve, trType, newSubTrID, baseCurve);
        }

	}
	

    void TrajectoryGenerator::generateHipTrajectory(IKRigConfig* config,
        ComponentManager<TransformComponent>& transformManager,
        ComponentManager<StaticMeshComponent>& staticMeshManager) {

        bool fixedTrajectories = false;
        FrameIndex currentFrame = config->getCurrentFrameIndex();
        for (ChainIndex i = 0; i < m_ikRig->getChainNum(); i++) {
            if (config->getEETrajectoryData(i)->getTargetTrajectory().getSubTrajectoryID() == -2) { 
                fixedTrajectories = true;
                break;
            }
        }

        HipGlobalTrajectoryData* hipTrData = config->getHipTrajectoryData();
        
        if (fixedTrajectories) {
            float initialTime_rep = config->getReproductionTime(currentFrame);
            float currFrameTime_extendedAnim = config->getAnimationTime(currentFrame);
            float nextFrameTime_extendedAnim = config->getAnimationTime(config->getNextFrameIndex());
            if (nextFrameTime_extendedAnim < currFrameTime_extendedAnim) {
                nextFrameTime_extendedAnim += config->getAnimationDuration();
            }
			LIC<1> hipRotAnglCurve = hipTrData->sampleOriginalRotationAngles(currFrameTime_extendedAnim, nextFrameTime_extendedAnim);
			LIC<3> hipRotAxCurve = hipTrData->sampleOriginalRotationAxes(currFrameTime_extendedAnim, nextFrameTime_extendedAnim);
            float initialRotAngle = hipRotAnglCurve.getStart()[0];
            glm::vec3 initialRotAxis = hipRotAxCurve.getStart();
            glm::vec3 initialTrans = hipTrData->getSavedTranslation(currentFrame);
            // chequear si hay info de posicion valida previa
            if (!(hipTrData->isSavedDataValid(currentFrame) && hipTrData->getTargetTranslations().inTRange(initialTime_rep))) {
                glm::vec2 basePoint(initialTrans);
				/*initialTrans = glm::vec3(basePoint, calcHipAdjustedHeight(basePoint, trDataPair,
                    initialTime_rep, config->getAnimationTime(currentFrame), config,
					transformManager, staticMeshManager));*/
            }
            LIC<1> newHipRotAngles({ glm::vec1(initialRotAngle), glm::vec1(initialRotAngle) }, { initialTime_rep, initialTime_rep + config->getAnimationDuration() });
            LIC<3> newHipRotAxes({ initialRotAxis, initialRotAxis }, { initialTime_rep, initialTime_rep + config->getAnimationDuration() });
            LIC<3> newHipTrans({ initialTrans, initialTrans }, { initialTime_rep, initialTime_rep + config->getAnimationDuration() });
            hipTrData->setTargetRotationAngles(newHipRotAngles);
            hipTrData->setTargetRotationAxes(newHipRotAxes);
            hipTrData->setTargetTranslations(newHipTrans);
        }
        else {
            HipGlobalTrajectoryData* hipTrData = config->getHipTrajectoryData();
            // buscamos la curva dinamica a la que le quede mas tiempo
            float tInfLimitRep = std::numeric_limits<float>::lowest();
            float tSupLimitRep = std::numeric_limits<float>::lowest();
            EETrajectory baseEETargetTr;
            EEGlobalTrajectoryData* baseEETrData;
            ChainIndex baseEEChain = -1;
            for (ChainIndex i = 0; i < m_ikRig->getChainNum(); i++) {
                EEGlobalTrajectoryData* trData = config->getEETrajectoryData(i);
                EETrajectory& currTr = trData->getTargetTrajectory();
                float tCurrentSupLimit = currTr.getEECurve().getTRange()[1];
                if (currTr.isDynamic() && tSupLimitRep < tCurrentSupLimit) {
                    tSupLimitRep = tCurrentSupLimit;
                    tInfLimitRep = currTr.getEECurve().getTRange()[0];
                    baseEETargetTr = currTr;
                    baseEETrData = trData;
                    baseEEChain = i;
                }
            }

            EETrajectory baseEEOriginalTr = baseEETrData->getSubTrajectoryByID(baseEETargetTr.getSubTrajectoryID());
            LIC<3>& baseEEOriginalCurve = baseEEOriginalTr.getEECurve();
            LIC<3>& baseEETargetCurve = baseEETargetTr.getAltCurve();

			ChainIndex opposite = m_ikRig->getIKChain(baseEEChain)->getOpposite();
			EEGlobalTrajectoryData* oppositeTrData = config->getEETrajectoryData(opposite);
			EETrajectory oppositeTargetTr = oppositeTrData->getTargetTrajectory();
			LIC<3> oppositeEETargetCurve = oppositeTargetTr.getAltCurve();
			LIC<3> oppositeEEOriginalCurve = oppositeTrData->getSubTrajectoryByID(oppositeTargetTr.getSubTrajectoryID()).getEECurve();

            std::pair<EEGlobalTrajectoryData*, EEGlobalTrajectoryData*> trDataPair(baseEETrData, oppositeTrData);

			float tInfLimitExtendedAnim = baseEEOriginalCurve.getTRange()[0];
			float tSupLimitExtendedAnim = baseEEOriginalCurve.getTRange()[1];
            float tCurrExtendedAnim = config->getAnimationTime(currentFrame);
            while (tCurrExtendedAnim < tInfLimitExtendedAnim) { tCurrExtendedAnim += config->getAnimationDuration(); }
            while (tSupLimitExtendedAnim < tCurrExtendedAnim) { tCurrExtendedAnim -= config->getAnimationDuration(); }


            LIC<3> hipTrCurve = hipTrData->sampleOriginalTranslations(tInfLimitExtendedAnim, tSupLimitExtendedAnim);
            LIC<1> hipRotAnglCurve = hipTrData->sampleOriginalRotationAngles(tInfLimitExtendedAnim, tSupLimitExtendedAnim);
            LIC<3> hipRotAxCurve = hipTrData->sampleOriginalRotationAxes(tInfLimitExtendedAnim, tSupLimitExtendedAnim);
            
            glm::vec3 hipTrOriginalDirection = glm::normalize(hipTrCurve.getEnd() - hipTrCurve.getStart());

            float hipOriginalXYDistance = glm::length(glm::vec2(hipTrCurve.getEnd() - hipTrCurve.getStart()));


            FrameIndex initialFrame = config->getFrame(tInfLimitExtendedAnim);
            // primer paso: calculo del punto inicial de la trayectoria
			float initialRotAngle = hipRotAnglCurve.getStart()[0];
			glm::vec3 initialRotAxis = hipRotAxCurve.getStart();
            glm::vec3 initialTrans;
            // chequear si hay info de posicion valida previa
            if (hipTrData->isSavedDataValid(initialFrame) && hipTrData->getTargetTranslations().inTRange(tInfLimitRep)) {
				initialTrans = hipTrData->getSavedTranslation(initialFrame);
            }
            else {
                glm::vec2 hipXYReferencePoint = hipTrCurve.evalCurve(tCurrExtendedAnim);
                float targetXYDistance = glm::distance(glm::vec2(hipTrCurve.getStart()), hipXYReferencePoint);
                glm::vec2 currXYHipPos = hipTrData->getSavedTranslation(currentFrame);
                glm::vec2 targetDirection = glm::rotate(glm::vec2(hipTrOriginalDirection), m_ikRig->getRotationAngle());
                glm::vec2 targetXYPos = currXYHipPos - targetDirection * targetXYDistance;
				initialTrans = glm::vec3(glm::vec2(targetXYPos),
					calcHipAdjustedHeight(trDataPair, tInfLimitRep, tInfLimitExtendedAnim, config, transformManager, staticMeshManager));
            }
            
            // segundo paso: ajustar punto de maxima altura      
            float hipHighOriginalTime_extendedAnim = baseEEOriginalTr.getHipMaxAltitudeTime();
            float hipHighOriginalTime_rep = hipHighOriginalTime_extendedAnim - hipTrCurve.getTRange()[0] + tInfLimitRep;
            glm::vec3 hipHighOriginalPoint = baseEEOriginalCurve.evalCurve(hipHighOriginalTime_extendedAnim);
            glm::vec3 hipHighOriginalPoint_opposite = oppositeEEOriginalCurve.evalCurve(hipHighOriginalTime_extendedAnim);
            float hipHighOriginalOppositesXYDist = glm::distance(glm::vec2(hipHighOriginalPoint), glm::vec2(hipHighOriginalPoint_opposite));
            
            // buscamos el par de puntos entre curvas opuestas que tengan una distancia xy lo mas cercana a la original
            float hipHighNewTime_rep = -1;
            float minDistDiff = std::numeric_limits<float>::max();
            float totalTime = baseEETargetCurve.getTRange()[1] - baseEETargetCurve.getTRange()[0];
            std::vector<float> distDiffsWPenalty;
            std::vector<float> penalties;
            std::vector<float> penaltyMults;
            std::vector<float> finalPenalties;
            for (int i = 1; i < baseEETargetCurve.getNumberOfPoints()-1; i++) {
                float currDist = glm::distance(glm::vec2(baseEETargetCurve.getCurvePoint(i)), 
                    glm::vec2(oppositeEETargetCurve.getCurvePoint(i)));
                // se considera la diferencia con el tiempo original
                float currDistDiff = abs(hipHighOriginalOppositesXYDist - currDist);
                float currTimeDiff = abs(baseEETargetCurve.getTValue(i) - hipHighOriginalTime_rep);
                float penalty = currDistDiff*(currTimeDiff/totalTime);
                float penaltyMult = 0 < currTimeDiff ? totalTime / currTimeDiff : 0;
                penaltyMult = std::clamp(penaltyMult, 0.0f, 5.0f);
                currDistDiff += penalty*penaltyMult;
                distDiffsWPenalty.push_back(currDistDiff);
                penalties.push_back(penalty);
                penaltyMults.push_back(penaltyMult);
                finalPenalties.push_back(penalty * penaltyMult);
                if (currDistDiff < minDistDiff) {
                    hipHighNewTime_rep = baseEETargetCurve.getTValue(i);
                    minDistDiff = currDistDiff;
                }
            }
            int hipHighPointIndex_hip = hipTrCurve.getClosestPointIndex(hipHighOriginalTime_extendedAnim);

            

			// ajuste a tiempo de reproduccion
			hipTrCurve.offsetTValues(-tInfLimitExtendedAnim + tInfLimitRep);
			hipRotAnglCurve.offsetTValues(-tInfLimitExtendedAnim + tInfLimitRep);
			hipRotAxCurve.offsetTValues(-tInfLimitExtendedAnim + tInfLimitRep);
            
			hipTrCurve.displacePointT(hipHighPointIndex_hip, 0, hipTrCurve.getNumberOfPoints() - 1, hipHighNewTime_rep, true);
			hipRotAnglCurve.displacePointT(hipHighPointIndex_hip, 0, hipTrCurve.getNumberOfPoints() - 1, hipHighNewTime_rep, true);
			hipRotAxCurve.displacePointT(hipHighPointIndex_hip, 0, hipTrCurve.getNumberOfPoints() - 1, hipHighNewTime_rep, false);


            // correccion de posicion y orientacion
            hipTrCurve.translate(-hipTrCurve.getStart());
            glm::fquat xyRot = glm::angleAxis(m_ikRig->getRotationAngle(), m_ikRig->getUpVector());
            hipTrCurve.rotate(xyRot);
            hipTrCurve.translate(initialTrans);

            LIC<3> hipTrCorrectedCurve = hipTrCurve;
            // ajustar el valor el z del punto de maxima altura
           /* glm::vec3 hipHighBasePoint = hipTrCurve.getCurvePoint(hipHighPointIndex_hip);
            float hipHightPointAdjustedZ = calcHipAdjustedHeight(trDataPair,
                hipHighNewTime_rep, hipHighOriginalTime_extendedAnim, config, transformManager, staticMeshManager);
            hipTrCorrectedCurve.setCurvePoint(hipHighPointIndex_hip, glm::vec3(glm::vec2(hipHighBasePoint), hipHightPointAdjustedZ));*/


            // DEBUG
            for (int i = 1; i < hipTrCorrectedCurve.getNumberOfPoints(); i++) {
                float tVal_rep = hipTrCorrectedCurve.getTValue(i);
                float adjustedZ = calcHipAdjustedHeight(trDataPair, tVal_rep, tVal_rep - tInfLimitRep + tInfLimitExtendedAnim, 
                    config, transformManager, staticMeshManager);
                glm::vec3 adjustedPoint(glm::vec2(hipTrCorrectedCurve.evalCurve(tVal_rep)), adjustedZ);
                hipTrCorrectedCurve.setCurvePoint(i, adjustedPoint);
            }

             // DEBUG
            
            // aplicamos descenso de gradiente tomando en cuenta los ajustes
            m_tgData.pointIndexes.clear();
            m_tgData.pointIndexes.reserve(hipTrCorrectedCurve.getNumberOfPoints());
            m_tgData.minValues.clear();
            m_tgData.minValues.reserve(m_tgData.pointIndexes.size() * 3);
            for (int i = 1; i < hipTrCorrectedCurve.getNumberOfPoints() - 1; i++) {
                // Los extremos de la curva y el punto de max altura de la cadera se mantendran fijos.
                if (i != hipHighPointIndex_hip) {
                    m_tgData.pointIndexes.push_back(i);
                    for (int j = 0; j < 3; j++) {
                        m_tgData.minValues.push_back(std::numeric_limits<float>::lowest());
                    }
                }
            }
            // valores iniciales y curva base
            std::vector<float> initialArgs(m_tgData.pointIndexes.size() * 3);
            for (int i = 0; i < m_tgData.pointIndexes.size(); i++) {
                int pIndex = m_tgData.pointIndexes[i];
                for (int j = 0; j < 3; j++) {
                    initialArgs[i * 3 + j] = hipTrCorrectedCurve.getCurvePoint(pIndex)[j];
                }
            }
            m_tgData.baseCurve = hipTrCurve;
            m_tgData.varCurve = &hipTrCorrectedCurve;
            m_gradientDescent.setArgNum(initialArgs.size());
            m_gradientDescent.computeArgsMin(m_tgData.descentRate, m_tgData.maxIterations, m_tgData.targetPosDelta, initialArgs);
           

            // tercer paso: encontrar el punto final
            // creamos una curva que represente la trayectoria de caida
   //         glm::vec3 fall1 = hipTrCorrectedCurve.getCurvePoint(hipHighPointIndex_hip);
   //         float fall1_time = hipTrCorrectedCurve.getTValue(hipHighPointIndex_hip);
   //         glm::vec3 fall2(0);
   //         float fall2_time = 0;
   //         int pointNum = 0;
   //         for (int i = hipHighPointIndex_hip; i < hipTrCorrectedCurve.getNumberOfPoints(); i++) {
   //             fall2 += hipTrCorrectedCurve.getCurvePoint(i);
   //             fall2_time += hipTrCorrectedCurve.getTValue(i);
   //             pointNum += 1;
   //         }
   //         fall2 /= pointNum;
   //         fall2_time /= pointNum;

   //         glm::vec3 fall3 = hipTrCorrectedCurve.getEnd();
   //         float fall3_time = hipTrCorrectedCurve.getTRange()[1];

   //         LIC<3> fallCurve({ fall1, fall2, fall3 }, { fall1_time, fall2_time, fall3_time });
   //         glm::vec3 fallAcc = fallCurve.getPointAcceleration(1);

   //         // calculamos el t aproximado del final de la caida
   //         float originalFallDuration = fallCurve.getTRange()[1] - fallCurve.getTRange()[0];
   //         float baseEEOriginalXYFallDist = glm::length(glm::vec2(baseEEOriginalCurve.getEnd() - 
   //             baseEEOriginalCurve.evalCurve(hipHighOriginalTime_extendedAnim)));
   //         float baseEETargetXYFallDist = glm::length(glm::vec2(baseEETargetCurve.getEnd() - baseEETargetCurve.evalCurve(hipHighNewTime_rep)));
   //         float xyFallDistRatio = baseEETargetXYFallDist / baseEEOriginalXYFallDist;
   //         float newFallDuration = originalFallDuration * xyFallDistRatio;

   //        

   //         // con el t encontrado y la aceleracion calculamos el punto final
   //         glm::vec3 finalTrans;
   //         // si el t cae antes de que termine la curva original
   //         float calcT = fall1_time + newFallDuration;
   //         if (calcT <= hipTrCorrectedCurve.getTRange()[1]) {
   //             finalTrans = hipTrCorrectedCurve.evalCurve(calcT);
   //         }
   //         else {// si no
   //             finalTrans = hipTrCorrectedCurve.getEnd();
   //             float fallRemainingTime = calcT - hipTrCorrectedCurve.getTRange()[1];
   //             glm::vec3 calcVel = hipTrCorrectedCurve.getVelocity(hipTrCorrectedCurve.getTRange()[1]);
   //             int accSteps = 5;
   //             for (int i = 0; i < accSteps; i++) {
   //                 float deltaT = fallRemainingTime / accSteps;
   //                 finalTrans += calcVel * deltaT;
   //                 calcVel += fallAcc * deltaT;
   //             }
   //         }
   //         std::vector<glm::vec3> newPoints;
   //         std::vector<float> newTimes;
   //         for (int i = 0; i <= hipHighPointIndex_hip; i++) {
			//	newPoints.push_back(hipTrCorrectedCurve.getCurvePoint(i));
			//	newTimes.push_back(hipTrCorrectedCurve.getTValue(i));
   //         }
   //         for (int i = hipHighPointIndex_hip + 1; i < hipTrCorrectedCurve.getNumberOfPoints(); i++) {
   //             if (hipTrCorrectedCurve.getTValue(i) < calcT && hipTrCorrectedCurve.getTEpsilon()*3 <(calcT- hipTrCorrectedCurve.getTValue(i))) {
   //                 newPoints.push_back(hipTrCorrectedCurve.getCurvePoint(i));
   //                 newTimes.push_back(hipTrCorrectedCurve.getTValue(i));
   //             }
   //             else {
   //                 newPoints.push_back(finalTrans);
   //                 newTimes.push_back(calcT);
   //                 break;
   //             }
   //         }

   //         LIC<3> hipTrFinalCurve(newPoints, newTimes);
			//// ajustamos los valores de t de la caida
   //         int finalIndex = hipTrFinalCurve.getNumberOfPoints() - 1;
   //         hipTrFinalCurve.displacePointT(finalIndex, hipHighPointIndex_hip, finalIndex, hipTrCorrectedCurve.getTRange()[1], false);

            // DEBUG
            LIC<3> hipTrFinalCurve = hipTrCorrectedCurve;
            // DEBUG

            // escalamos la curva de acuerdo a las trayectorias de los ee
            float baseEEOriginalXYDist = glm::length(glm::vec2(baseEEOriginalCurve.getEnd() - baseEEOriginalCurve.getStart()));
            float baseEETargetXYDist = glm::length(glm::vec2(baseEETargetCurve.getEnd() - baseEETargetCurve.getStart()));
            float eeXYDistRatio = baseEETargetXYDist / baseEEOriginalXYDist;
            float hipNewXYDist = glm::length(glm::vec2(hipTrFinalCurve.getEnd() - hipTrFinalCurve.getStart()));
            float hipXYDistRatio = hipNewXYDist / hipOriginalXYDistance;
            float eeHipXYDistRatio = eeXYDistRatio / hipXYDistRatio ;
            glm::vec3 scalingVec(eeHipXYDistRatio, eeHipXYDistRatio, 1);
            //hipTrFinalCurve.scale(scalingVec);

			float repCountOffset = config->getNextFrameIndex() == 0 ? 1 : 0;
			float transitionTime = config->getReproductionTime(config->getNextFrameIndex(), repCountOffset);
            if (hipTrData->getTargetTranslations().inTRange(transitionTime)) {
                hipTrData->setTargetTranslations(LIC<3>::transition(hipTrData->getTargetTranslations(), hipTrFinalCurve, transitionTime));
                hipTrData->setTargetRotationAngles(LIC<1>::transition(hipTrData->getTargetRotationAngles(), hipRotAnglCurve, transitionTime));
                hipTrData->setTargetRotationAxes(LIC<3>::transition(hipTrData->getTargetRotationAxes(), hipRotAxCurve, transitionTime));
            }
            else {
                hipTrData->setTargetRotationAngles(hipRotAnglCurve);
                hipTrData->setTargetRotationAxes(hipRotAxCurve);
                hipTrData->setTargetTranslations(hipTrFinalCurve);
            }
        }
    }


	glm::vec3 TrajectoryGenerator::calcStrideStartingPoint(float supportHeight,
		glm::vec2 xyReferencePoint, float targetDistance,
		glm::vec2 targetDirection, int stepNum,
		ComponentManager<TransformComponent>& transformManager,
		ComponentManager<StaticMeshComponent>& staticMeshManager) {
		std::vector<glm::vec3> collectedPoints;
		collectedPoints.reserve(stepNum);
		for (int i = 1; i <= stepNum; i++) {
			glm::vec2 testPoint = xyReferencePoint - targetDirection * targetDistance * ((float)i / stepNum);
			float calcHeight = m_environmentData.getTerrainHeight(testPoint,
				transformManager, staticMeshManager);
			collectedPoints.push_back(glm::vec3(testPoint, calcHeight));
		}
		float minDistanceDiff = std::numeric_limits<float>::max();
		glm::vec3 floorReferencePoint = glm::vec3(xyReferencePoint, m_environmentData.getTerrainHeight(xyReferencePoint,
			transformManager, staticMeshManager));
		glm::vec3 closest = collectedPoints[0];
		for (int i = 0; i < collectedPoints.size(); i++) {
			float distDiff = std::abs(glm::distance(floorReferencePoint, collectedPoints[i]) - targetDistance);
			if (distDiff < minDistanceDiff) {
				minDistanceDiff = distDiff;
				closest = collectedPoints[i];
			}
		}
        closest[2] += supportHeight;
		return closest;
	}
    

	std::vector<glm::vec3> TrajectoryGenerator::calcStrideData(float supportHeightStart, float supportHeightEnd,
        glm::vec3 startingPoint, float targetDistance,
		glm::vec2 targetDirection, int stepNum,
		ComponentManager<TransformComponent>& transformManager,
		ComponentManager<StaticMeshComponent>& staticMeshManager) {
		std::vector<glm::vec3> collectedPoints;
		collectedPoints.reserve(stepNum);
		for (int i = 1; i <= stepNum; i++) {
			glm::vec2 testPoint = glm::vec2(startingPoint) + targetDirection * targetDistance * ((float)i / stepNum);
            float supportHeight = funcUtils::lerp(supportHeightStart, supportHeightEnd, (float) i / stepNum);
			float calcHeight = supportHeight + m_environmentData.getTerrainHeight(testPoint, transformManager, staticMeshManager);
			collectedPoints.push_back(glm::vec3(testPoint, calcHeight));
		}
        // validacion de los puntos
		std::vector<glm::vec3> strideDataPoints;
		strideDataPoints.reserve(stepNum);
		float previousDistance = 0;
        float epsilon = targetDistance * 0.005;
        float minDistDiff = std::numeric_limits<float>::max();
        int finalPointIndex = -1;
		for (int i = 0; i < collectedPoints.size(); i++) {
			float distance = glm::distance(startingPoint, collectedPoints[i]);
			if (abs(targetDistance - distance) < minDistDiff) {
                minDistDiff = abs(targetDistance - distance);
                finalPointIndex = i;
			}
		}
        for (int i = 0; i <= finalPointIndex; i++) {
            strideDataPoints.push_back(collectedPoints[i]);
        }

		return strideDataPoints;
	}


	float TrajectoryGenerator::calcHipAdjustedHeight(std::pair<EEGlobalTrajectoryData*, EEGlobalTrajectoryData*> oppositeTrajectories,
        float targetCurvesTime_rep,
		float originalCurvesTime_extendedAnim, IKRigConfig* config,
		ComponentManager<TransformComponent>& transformManager,
		ComponentManager<StaticMeshComponent>& staticMeshManager) {

        
		HipGlobalTrajectoryData* hipTrData = config->getHipTrajectoryData();
		LIC<3> hipTrCurve = hipTrData->sampleOriginalTranslations(originalCurvesTime_extendedAnim,	originalCurvesTime_extendedAnim + config->getAnimationDuration());
        float hipOriginalZ = hipTrCurve.evalCurve(originalCurvesTime_extendedAnim)[2];

        // distancias originales de ee's con cadera
        std::vector<float> origZDiffs(2);
        int baseID1 = oppositeTrajectories.first->getTargetTrajectory().getSubTrajectoryID();
        float originalZ1 = oppositeTrajectories.first->getSubTrajectoryByID(baseID1).getEECurve().evalCurve(originalCurvesTime_extendedAnim)[2];
        int baseID2 = oppositeTrajectories.second->getTargetTrajectory().getSubTrajectoryID();
        float originalZ2 = oppositeTrajectories.second->getSubTrajectoryByID(baseID2).getEECurve().evalCurve(originalCurvesTime_extendedAnim)[2];
        origZDiffs[0] = hipOriginalZ - originalZ1;
        origZDiffs[1] = hipOriginalZ - originalZ2;

		std::vector<float> targetPointsZ(2);
        targetPointsZ[0] = oppositeTrajectories.first->getTargetTrajectory().getEECurve().evalCurve(targetCurvesTime_rep)[2];
        targetPointsZ[1] = oppositeTrajectories.second->getTargetTrajectory().getEECurve().evalCurve(targetCurvesTime_rep)[2];
		float zValue = std::min(targetPointsZ[0], targetPointsZ[1]);

		// valor mas bajo de z para las tr actuales de los ee
		float zDelta = m_ikRig->getRigHeight() * m_ikRig->getRigScale() / 500;
		std::vector<bool> conditions(2, true);
		int maxSteps = 1000;
		int steps = 0;
		while (funcUtils::conditionVector_AND(conditions) && steps < maxSteps) {
			zValue += zDelta;
            std::vector<float> currZDiffs(2);
			for (int i = 0; i < 2; i++) {
				currZDiffs[i] = zValue - targetPointsZ[i];
				conditions[i] = currZDiffs[i] < origZDiffs[i];
			}

			steps += 1;
		}
		return zValue;
	}


    void TrajectoryGenerator::buildHipTrajectory(IKRigConfig* config, std::vector<glm::mat4> const& hipGlobalTransforms, float minDistance, float floorZ) {
        int frameNum = config->getFrameNum();
        std::shared_ptr<AnimationClip> anim = config->m_animationClip;
		std::vector<float> hipTimeStamps;
		hipTimeStamps.reserve(frameNum);
		std::vector<glm::vec3> hipRotAxes;
		hipRotAxes.reserve(frameNum);
		std::vector<glm::vec1> hipRotAngles;
		hipRotAngles.reserve(frameNum);
		std::vector<glm::vec3> hipTranslations;
		hipTranslations.reserve(frameNum);
		glm::vec3 previousHipPosition(std::numeric_limits<float>::lowest());
		glm::vec3 hipScale; glm::quat hipRotation; glm::vec3 hipTranslation; glm::vec3 hipSkew; glm::vec4 hipPerspective;
		for (int i = 0; i < frameNum; i++) {
			float timeStamp = config->getAnimationTime(i);
            glm::mat4 hipTransform = hipGlobalTransforms[i];
			glm::vec3 hipPosition = hipTransform * glm::vec4(0, 0, 0, 1);
			if (minDistance <= glm::distance(hipPosition, previousHipPosition) || i == (frameNum - 1)) { // el valor del ultimo frame se guarda si o si
				glm::decompose(hipTransform, hipScale, hipRotation, hipTranslation, hipSkew, hipPerspective);
				hipRotAngles.push_back(glm::vec1(glm::angle(hipRotation)));
				hipRotAxes.push_back(glm::axis(hipRotation));
				hipTranslation[2] -= floorZ;
				hipTranslations.push_back(hipTranslation);
				hipTimeStamps.push_back(timeStamp);
			}
			previousHipPosition = hipPosition;
		}

		config->m_hipTrajectoryData.init(config);
		config->m_hipTrajectoryData.m_originalRotationAngles = LIC<1>(hipRotAngles, hipTimeStamps);
		config->m_hipTrajectoryData.m_originalRotationAxes = LIC<3>(hipRotAxes, hipTimeStamps);
		config->m_hipTrajectoryData.m_originalTranslations = LIC<3>(hipTranslations, hipTimeStamps);
    }

    void TrajectoryGenerator::buildEETrajectories(IKRigConfig* config, 
        std::vector<std::vector<bool>> supportFramesPerChain, 
        std::vector<std::vector<glm::vec3>> globalPositionsPerChain) {
        int chainNum = supportFramesPerChain.size();
        int frameNum = config->getFrameNum();
		std::vector<int> connectedCurveIndexes(chainNum, -1);
		std::vector<bool> continueTrajectory(chainNum, false);
		for (int i = 0; i < chainNum; i++) {
			std::vector<EETrajectory> subTrajectories;
			bool allStatic = funcUtils::conditionVector_AND(supportFramesPerChain[i]);
			if (allStatic) {
				glm::vec3 staticPos = globalPositionsPerChain[i][0];
				LIC<3> staticTr({ staticPos, staticPos }, { config->getAnimationTime(0), config->getAnimationTime(frameNum - 1) });
				subTrajectories.push_back(EETrajectory(staticTr, TrajectoryType::STATIC, 0));
				for (int j = 0; j < frameNum; j++) { config->m_eeTrajectoryData[i].m_supportHeights[j] = globalPositionsPerChain[i][j][2]; }
			}
			else {
				// encontrar primer punto de interes (dinamica luego de uno estatico)
				FrameIndex curveStartFrame = -1;
				for (int j = 1; j < frameNum; j++) {
					if (supportFramesPerChain[i][j - 1] && !supportFramesPerChain[i][j]) {
						curveStartFrame = j;
						break;
					}
				}
				MONA_ASSERT(curveStartFrame != -1, "IKRigController: There must be at least one support frame per ee trajectory.");
				std::vector<std::pair<int, int>> shInterpolationLimits;
				float supportHeight;
				int j = curveStartFrame;
				FrameIndex currFrame;
				while (j < frameNum + curveStartFrame) {
					currFrame = j % frameNum;
					FrameIndex initialFrame = 0 < currFrame ? currFrame - 1 : frameNum - 1;
					bool baseFrameType = supportFramesPerChain[i][currFrame];
					TrajectoryType trType = baseFrameType ? TrajectoryType::STATIC : TrajectoryType::DYNAMIC;
					supportHeight = globalPositionsPerChain[i][initialFrame][2];
					config->m_eeTrajectoryData[i].m_supportHeights[initialFrame] = supportHeight;
					if (trType == TrajectoryType::DYNAMIC) {
						shInterpolationLimits.push_back({ j - 1, j });
					}
					std::vector<glm::vec3> curvePoints_1 = { globalPositionsPerChain[i][initialFrame] };
					std::vector<float> tValues_1 = { config->getAnimationTime(initialFrame) };
					std::vector<glm::vec3> curvePoints_2;
					std::vector<float> tValues_2;
					std::vector<glm::vec3>* selectedCPArr = &curvePoints_1;
					std::vector<float>* selectedTVArr = &tValues_1;
				GATHER_POINTS:
					while (baseFrameType == supportFramesPerChain[i][currFrame]) {
						config->m_eeTrajectoryData[i].m_supportHeights[currFrame] = baseFrameType ?
							globalPositionsPerChain[i][currFrame][2] : supportHeight;
						(*selectedCPArr).push_back(globalPositionsPerChain[i][currFrame]);
						(*selectedTVArr).push_back(config->getAnimationTime(currFrame));
						j++;
						currFrame = j % frameNum;
						if (j == frameNum) {
							continueTrajectory[i] = true;
							break;
						}
						if (currFrame == curveStartFrame) { break; }

					}
					if (continueTrajectory[i]) {
						selectedCPArr = &curvePoints_2;
						selectedTVArr = &tValues_2;
						continueTrajectory[i] = false;
						goto GATHER_POINTS;
					}
					LIC<3> fullCurve;
					if (curvePoints_2.size() == 0) {
						fullCurve = LIC<3>(curvePoints_1, tValues_1);
					}
					else if (curvePoints_2.size() == 1) {
						connectedCurveIndexes[i] = subTrajectories.size();
						LIC<3> curve(curvePoints_1, tValues_1);
						float tDiff = tValues_2[0] + config->getAnimationDuration() - curve.getTRange()[1];
						if (0 < tDiff) {
							fullCurve = LIC<3>::connectPoint(curve, curvePoints_2[0], tDiff);
						}
						else {
							fullCurve = curve;
						}
					}
					else if (1 < curvePoints_2.size()) {
						connectedCurveIndexes[i] = subTrajectories.size();
						LIC<3> part1(curvePoints_1, tValues_1);
						LIC<3> part2(curvePoints_2, tValues_2);
						fullCurve = LIC<3>::connect(part1, part2);
					}
					subTrajectories.push_back(EETrajectory(fullCurve, trType));
					if (trType == TrajectoryType::DYNAMIC) {
						shInterpolationLimits.back().second = j;
					}
				}
				if (connectedCurveIndexes[i] != -1) {
					// falta agregar la misma curva pero al comienzo del arreglo (con otro desplazamiento temporal)
					LIC<3> connectedCurve = subTrajectories[connectedCurveIndexes[i]].getEECurve();
					TrajectoryType connectedTrType = subTrajectories[connectedCurveIndexes[i]].isDynamic() ? TrajectoryType::DYNAMIC : TrajectoryType::STATIC;
					connectedCurve.offsetTValues(-config->getAnimationDuration());
					subTrajectories.push_back(EETrajectory(connectedCurve, connectedTrType));
				}
				// nos aseguramos de que las curvas esten bien ordenadas
				std::vector<EETrajectory> tempTr = subTrajectories;
				subTrajectories = {};
				while (0 < tempTr.size()) {
					float minT = std::numeric_limits<float>::max();
					int minIndex = -1;
					for (int t = 0; t < tempTr.size(); t++) {
						float currT = tempTr[t].getEECurve().getTRange()[0];
						if (currT < minT) {
							minT = currT;
							minIndex = t;
						}
					}
					subTrajectories.push_back(tempTr[minIndex]);
					tempTr.erase(tempTr.begin() + minIndex);
				}

				// corregimos las posiciones de la trayectoria insertada al principio (si es que la hubo)
				if (connectedCurveIndexes[i] != -1) {
					glm::vec3 targetEnd = subTrajectories[1].getEECurve().getStart();
					LIC<3>& curveToCorrect = subTrajectories[0].getEECurve();
					curveToCorrect.translate(-curveToCorrect.getEnd());
					curveToCorrect.translate(targetEnd);
				}

				// interpolamos los valores de support height para las curvas dinamicas
				for (int k = 0; k < shInterpolationLimits.size(); k++) {
					float sh1 = config->m_eeTrajectoryData[i].m_supportHeights[shInterpolationLimits[k].first % frameNum];
					float sh2 = config->m_eeTrajectoryData[i].m_supportHeights[shInterpolationLimits[k].second % frameNum];
					int minIndex = shInterpolationLimits[k].first;
					int maxIndex = shInterpolationLimits[k].second;
					if (sh1 != sh2) {
						for (int l = minIndex; l <= maxIndex; l++) {
							FrameIndex shFrame = l % frameNum;
							float shFraction = funcUtils::getFraction(minIndex, maxIndex, l);
							config->m_eeTrajectoryData[i].m_supportHeights[shFrame] = funcUtils::lerp(sh1, sh2, shFraction);
						}
					}
				}
			}

			// asignamos las sub trayectorias a la cadena correspondiente
			config->m_eeTrajectoryData[i].m_originalSubTrajectories = subTrajectories;
			for (int j = 0; j < subTrajectories.size(); j++) {
				config->m_eeTrajectoryData[i].m_originalSubTrajectories[j].m_subTrajectoryID = j;
			}
		}


		// guardamos los tiempos de maxima altitud de la cadera
		LIC<3>& hipTr = config->getHipTrajectoryData()->m_originalTranslations;
		for (int i = 0; i <chainNum; i++) {
			EEGlobalTrajectoryData& trData = config->m_eeTrajectoryData[i];
			for (int j = 0; j < trData.m_originalSubTrajectories.size(); j++) {
				float maxZ = std::numeric_limits<float>::lowest();
				int savedIndex = -1;
				LIC<3>& currentCurve = trData.m_originalSubTrajectories[j].getEECurve();
				for (int k = 0; k < currentCurve.getNumberOfPoints(); k++) {
					float tValue = currentCurve.getTValue(k);
					if (connectedCurveIndexes[i] != -1) {
						if (j == 0) {
							if (!hipTr.inTRange(tValue)) {
								tValue += config->getAnimationDuration();
							}
						}
						else if (j == trData.m_originalSubTrajectories.size() - 1) {
							if (!hipTr.inTRange(tValue)) {
								tValue -= config->getAnimationDuration();
							}
						}
					}
					float hipZ = hipTr.evalCurve(tValue)[2];
					if (maxZ < hipZ) {
						maxZ = hipZ;
						savedIndex = k;
					}
				}
				config->m_eeTrajectoryData[i].m_originalSubTrajectories[j].m_hipMaxAltitudeTime =
					currentCurve.getTValue(savedIndex);
			}
		}
    }

    
}